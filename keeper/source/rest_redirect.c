#include "probe_common.h"
#include "native_method.h"
#include <stdbool.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <orbis/libkernel.h>

enum { PAGE_SIZE = 0x4000, JUMP_SIZE = 14, SHUTDOWN = 2, SUSPEND = 3, BOOT_EAP = 1 };
/* All 14 bytes are complete push/mov instructions; none are PC-relative.
 * This was read from this console's resolved five-argument native method. */
static const unsigned char expected[JUMP_SIZE] = {
    0x55, 0x48, 0x89, 0xe5, 0x41, 0x57, 0x41,
    0x56, 0x41, 0x55, 0x41, 0x54, 0x53, 0x50
};
typedef bool (*RequestStateChange)(int, void *, int, int, int);
static RequestStateChange original_request;
static OrbisPthread worker_thread;
static atomic_int started;
static atomic_int must_remain_resident;
static atomic_int stopping;
static int worker_created;
static unsigned long long status_sequence;

static int write_status(const char *state)
{
    struct process_identity identity = {0};
    if (!probe_find_process("SceShellUI", &identity)) {
        probe_log("SHELL", "STATUS_IDENTITY_FAILED state=%s", state);
        return 0;
    }
    struct proc_info own;
    memset(&own, 0, sizeof(own));
    if (sys_sdk_proc_info(&own) != 0 || own.pid != identity.pid) return 0;
    char text[240];
    snprintf(text, sizeof(text), "HEN_REST_HOOK %s build=%s pid=%d cookie=%llx start_s=%lld start_us=%lld state=%s retained=%d seq=%llu\n",
             PROBE_VERSION, PROBE_BUILD, identity.pid, (unsigned long long) identity.cookie,
             identity.start_seconds, identity.start_microseconds, state,
             atomic_load(&must_remain_resident), ++status_sequence);
    int written = probe_write_text("/Temp/" PROBE_STAGE "/status.txt", text);
    if (status_sequence == 1 || !written) probe_log("SHELL", "STATUS_FILE state=%s written=%d", state, written);
    return written;
}

static bool redirect_request(int state, void *reason, int boot, int user, int app)
{
    if (state != SHUTDOWN) return original_request(state, reason, boot, user, app);
    probe_log("SHELL", "REDIRECT from_state=%d to_state=%d from_boot=%d to_boot=%d reason=%p user=%d app=%d",
              state, SUSPEND, boot, BOOT_EAP, reason, user, app);
    bool accepted = original_request(SUSPEND, reason, BOOT_EAP, user, app);
    probe_log("SHELL", "REDIRECT_RESULT accepted=%d shutdown_fallback=0", accepted);
    return accepted;
}

static void absolute_jump(unsigned char *buffer, const void *destination)
{
    static const unsigned char opcode[6] = { 0xff, 0x25, 0, 0, 0, 0 };
    memcpy(buffer, opcode, sizeof(opcode));
    memcpy(buffer + sizeof(opcode), &destination, sizeof(destination));
}

static int write_code(void *target, const unsigned char *bytes)
{
    struct proc_rw request;
    memset(&request, 0, sizeof(request));
    request.address = (uintptr_t) target;
    request.data = (void *) bytes;
    request.length = JUMP_SIZE;
    request.write_flags = 1;
    int result = sys_sdk_proc_rw(&request);
    atomic_thread_fence(memory_order_seq_cst);
    int matches = memcmp(target, bytes, JUMP_SIZE) == 0;
    probe_log("SHELL", "CODE_WRITE address=%p status=%d readback_match=%d", target, result, matches);
    return result == 0 && matches;
}

static int install_redirect(void *target)
{
    if (memcmp(target, expected, JUMP_SIZE) != 0) {
        probe_log("SHELL", "ABORT reason=unexpected_prologue target=%p", target);
        return 0;
    }
    unsigned char *trampoline = NULL;
    int result = sceKernelMmap(NULL, PAGE_SIZE, 3, 0x1002, -1, 0, (void **) &trampoline);
    probe_log("SHELL", "TRAMPOLINE_ALLOC status=%d address=%p", result, (void *) trampoline);
    if (result != 0 || !trampoline || trampoline == (void *) -1) return 0;
    memcpy(trampoline, expected, JUMP_SIZE);
    absolute_jump(trampoline + JUMP_SIZE, (unsigned char *) target + JUMP_SIZE);
    result = sceKernelMprotect(trampoline, PAGE_SIZE, 5);
    probe_log("SHELL", "TRAMPOLINE_RX status=%d", result);
    if (result != 0) {
        probe_log("SHELL", "TRAMPOLINE_FREE status=%d", sceKernelMunmap(trampoline, PAGE_SIZE));
        return 0;
    }
    memcpy(&original_request, &trampoline, sizeof(original_request));
    unsigned char jump[JUMP_SIZE];
    RequestStateChange replacement = redirect_request;
    void *replacement_address = NULL;
    memcpy(&replacement_address, &replacement, sizeof(replacement_address));
    absolute_jump(jump, replacement_address);
    /* Mark resident before writing, including partial-write failure cases.
     * The caller must keep the power menu idle during this 14-byte write. */
    atomic_store(&must_remain_resident, 1);
    if (!write_code(target, jump)) {
        int restored = write_code(target, expected);
        probe_log("SHELL", "ROLLBACK restored=%d", restored);
        /* Keep module and trampoline resident even after rollback: another
         * thread could already have entered the redirect during installation. */
        return 0;
    }
    probe_log("SHELL", "HOOK_READY target=%p trampoline=%p shutdown=2 suspend=3 boot_eap=1 scope=ShellUI_RequestStateChange_5",
              target, (void *) trampoline);
    return 1;
}

static void *run_redirect(void *unused)
{
    (void) unused;
    struct proc_info info;
    if (probe_process_report("SHELL", &info) != 0 ||
        strncmp(info.name, "SceShellUI", sizeof(info.name)) != 0 ||
        strncmp(info.titleid, "NPXS20001", sizeof(info.titleid)) != 0) goto fail;
    OrbisKernelSwVersion firmware;
    memset(&firmware, 0, sizeof(firmware));
    firmware.Size = sizeof(firmware);
    int result = sceKernelGetSystemSwVersion(&firmware);
    if (result != 0 || firmware.Version != 0x13520001) {
        probe_log("SHELL", "ABORT reason=firmware_guard status=%d raw=0x%08x", result, firmware.Version);
        goto fail;
    }
    void *native = NULL;
    for (int i = 0; i < 10 && !native && !atomic_load(&stopping); ++i) {
        native = probe_find_native_power_method();
        if (!native) sceKernelUsleep(1000000);
    }
    probe_log("SHELL", "METHOD name=RequestStateChange params=5 native=%p lookup=metadata_only", native);
    int installed = native && install_redirect(native);
    if (installed) {
        int announced = 0;
        while (!atomic_load(&stopping)) {
            int recorded = write_status("ready");
            if (recorded && !announced) {
                probe_notify("HEN Rest: shutdown-to-Rest Mode hook ready. Keeper enabled.");
                announced = 1;
            }
            sceKernelUsleep(2000000);
        }
        return NULL;
    }
fail:
    write_status("failed");
    probe_log("SHELL", "INSTALL_FAILED retained=%d", atomic_load(&must_remain_resident));
    probe_notify("HEN Rest: hook installation failed. Do not test Turn Off yet.");
    return NULL;
}

PROBE_EXPORT int module_start(size_t args, const void *argp)
{
    (void) args; (void) argp;
    if (atomic_exchange(&started, 1)) return 0;
    probe_log("SHELL", "MODULE_START version=%s", PROBE_VERSION);
    int result = scePthreadCreate(&worker_thread, NULL, run_redirect, NULL, "hen_rest_hook");
    worker_created = result == 0;
    probe_log("SHELL", "WORKER_CREATE status=%d", result);
    return result;
}

PROBE_EXPORT int module_stop(size_t args, const void *argp)
{
    (void) args; (void) argp;
    if (atomic_load(&must_remain_resident)) return -1;
    atomic_store(&stopping, 1);
    if (worker_created) {
        int result = scePthreadJoin(worker_thread, NULL);
        if (result != 0) return result;
        worker_created = 0;
    }
    /* No live unload: removing code still referenced by a detour can crash
     * ShellUI. Its normal process exit removes both module and hook together. */
    if (atomic_load(&must_remain_resident)) {
        probe_log("SHELL", "UNLOAD_REFUSED reason=live_hook_or_trampoline");
        return -1;
    }
    return 0;
}
