#include "probe_common.h"
#include <limits.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include "hook_record.h"
#include "startup_records.h"
#include <orbis/libkernel.h>

/* ShellCore owns the existing power-management lifecycle. This module only
 * watches for a new ShellUI sandbox; it patches no ShellCore code. */
static OrbisPthread worker;
static int worker_created;
static atomic_int started, stopping;
static int singleton_fd = -1;
static struct process_identity core_identity;
static unsigned char module_bytes[1024 * 1024];
static size_t module_length;
extern int *__error(void);

static int read_module(void)
{
    int fd = sceKernelOpen(PROBE_HOOK_SOURCE, 0, 0);
    if (fd < 0) { probe_log("KEEPER", "SOURCE_OPEN status=%d", fd); return 0; }
    while (module_length < sizeof(module_bytes)) {
        size_t count = sceKernelRead(fd, module_bytes + module_length, sizeof(module_bytes) - module_length);
        if (count > sizeof(module_bytes) - module_length) { module_length = 0; break; }
        if (!count) break;
        module_length += count;
    }
    int close_result = sceKernelClose(fd);
    static const unsigned char self_magic[4] = { 0x4f, 0x15, 0x3d, 0x1d };
    probe_log("KEEPER", "SOURCE_READ bytes=%zu close_status=%d", module_length, close_result);
    return close_result == 0 && module_length > 32 && module_length < sizeof(module_bytes) &&
           memcmp(module_bytes, self_magic, sizeof(self_magic)) == 0;
}

static int find_sandbox(char *output, size_t capacity)
{
    int fd = sceKernelOpen("/mnt/sandbox", 0, 0);
    if (fd < 0) return 0;
    char buffer[4096];
    int found = 0, ambiguous = 0, count;
    while ((count = sceKernelGetdents(fd, buffer, sizeof(buffer))) > 0) {
        for (int offset = 0; offset + 8 <= count;) {
            /* Orbis/FreeBSD dirent: uint32 inode, uint16 record length,
             * uint8 type, uint8 name length, then the NUL-terminated name. */
            uint16_t record_length;
            memcpy(&record_length, buffer + offset + 4, sizeof(record_length));
            unsigned int name_length = (unsigned char) buffer[offset + 7];
            if (record_length < 9 || offset + record_length > count || name_length >= record_length - 8u) {
                ambiguous = 1; break;
            }
            const char *name = buffer + offset + 8;
            if (name_length == 13 && memcmp(name, "NPXS20001_", 10) == 0 &&
                name[10] >= '0' && name[10] <= '9' && name[11] >= '0' && name[11] <= '9' &&
                name[12] >= '0' && name[12] <= '9' && name[13] == '\0') {
                if (found) ambiguous = 1;
                snprintf(output, capacity, "/mnt/sandbox/%s", name);
                found = 1;
            }
            offset += record_length;
        }
        if (ambiguous) break;
    }
    sceKernelClose(fd);
    return found && !ambiguous && count >= 0;
}

static int stage_module(const char *sandbox)
{
    char folder[100], path[140];
    snprintf(folder, sizeof(folder), "%s/Temp/%s", sandbox, PROBE_STAGE);
    OrbisKernelStat directory;
    if (sceKernelStat(folder, &directory) != 0 && sceKernelMkdir(folder, 0777) != 0) return 0;
    snprintf(path, sizeof(path), "%s/hen_rest_hook.prx", folder);
    /* This unique directory is owned by this build. Do not modify the source
     * module or any existing system file. Called only before first load here. */
    int fd = sceKernelOpen(path, 0x601, 0777); /* WRITE | CREATE | TRUNCATE */
    if (fd < 0) return 0;
    size_t written = 0;
    while (written < module_length) {
        size_t count = sceKernelWrite(fd, module_bytes + written, module_length - written);
        if (!count || count > module_length - written) break;
        written += count;
    }
    int closed = sceKernelClose(fd);
    if (written != module_length || closed != 0) return 0;
    fd = sceKernelOpen(path, 0, 0);
    if (fd < 0) return 0;
    unsigned char buffer[4096];
    size_t checked = 0;
    while (checked < module_length) {
        size_t wanted = module_length - checked;
        if (wanted > sizeof(buffer)) wanted = sizeof(buffer);
        size_t count = sceKernelRead(fd, buffer, wanted);
        if (!count || count > wanted || memcmp(buffer, module_bytes + checked, count) != 0) break;
        checked += count;
    }
    closed = sceKernelClose(fd);
    probe_log("KEEPER", "STAGE sandbox=%s bytes=%zu verified=%zu close_status=%d", sandbox, written, checked, closed);
    return checked == module_length && closed == 0;
}

enum hook_state { HOOK_WAITING, HOOK_LOADING, HOOK_READY, HOOK_FAILED };

static int hook_record(const char *sandbox, struct process_identity target)
{
    char path[140];
    snprintf(path, sizeof(path), "%s/Temp/%s/status.txt", sandbox, PROBE_STAGE);
    int fd = sceKernelOpen(path, 0, 0);
    if (fd < 0) return 0;
    char value[240] = {0};
    size_t count = sceKernelRead(fd, value, sizeof(value) - 1);
    sceKernelClose(fd);
    if (!count || count >= sizeof(value)) return HOOK_WAITING;
    return parse_hook_record(value, PROBE_VERSION, PROBE_BUILD, target);
}

static void keeper_status(const char *state, int core_pid, struct process_identity target,
                          const char *sandbox, int attempts, unsigned long long handle)
{
    char text[512];
    snprintf(text, sizeof(text), "HEN_REST %s state=%s core_pid=%d core_cookie=%llx core_start_s=%lld core_start_us=%lld shell_pid=%d cookie=%llx shell_start_s=%lld shell_start_us=%lld sandbox=%s attempts=%d handle=%llx build=%s\n",
             PROBE_VERSION, state, core_pid, core_identity.cookie, core_identity.start_seconds, core_identity.start_microseconds,
             target.pid, (unsigned long long) target.cookie, target.start_seconds, target.start_microseconds,
             *sandbox ? sandbox : "none", attempts, handle, PROBE_BUILD);
    if (!probe_write_text(PROBE_STATUS, text)) probe_log("KEEPER", "STATUS_WRITE_FAILED state=%s", state);
}

static void *run_keeper(void *unused)
{
    (void) unused;
    struct proc_info info;
    if (probe_process_report("KEEPER", &info) != 0 ||
        strncmp(info.name, "SceShellCore", sizeof(info.name)) != 0 ||
        strncmp(info.titleid, "NPXS20000", sizeof(info.titleid)) != 0) goto failed;
    OrbisKernelSwVersion firmware;
    memset(&firmware, 0, sizeof(firmware));
    firmware.Size = sizeof(firmware);
    if (sceKernelGetSystemSwVersion(&firmware) != 0 || firmware.Version != 0x13520001 || !read_module()) goto failed;
    char current[80] = "";
    struct process_identity active = {0};
    int settled = 0, attempts = 0, load_ticks = 0, unavailable_reported = 0;
    enum hook_state state = HOOK_WAITING;
    unsigned long long handle = 0;
    keeper_status("waiting", info.pid, active, current, attempts, handle);
    probe_log("KEEPER", "KEEPER_STARTED pid=%d version=%s", info.pid, PROBE_VERSION);
    while (!atomic_load(&stopping)) {
        char sandbox[80] = "";
        struct process_identity target = {0};
        if (!probe_find_process("SceShellUI", &target) || !find_sandbox(sandbox, sizeof(sandbox))) {
            settled = 0;
            if (!unavailable_reported) {
                probe_log("KEEPER", "TARGET_UNAVAILABLE process_or_sandbox=1");
                keeper_status("waiting", info.pid, active, current, attempts, handle);
                unavailable_reported = 1;
            }
            goto wait;
        }
        int returned = unavailable_reported;
        unavailable_reported = 0;
        /* One installation per process lifetime, even when the sandbox path
         * is reused. Path changes also settle again before any action. */
        if (!probe_same_process(active, target)) {
            active = target;
            snprintf(current, sizeof(current), "%s", sandbox);
            settled = attempts = load_ticks = 0;
            handle = 0;
            state = HOOK_WAITING;
            probe_log("KEEPER", "NEW_PROCESS pid=%d cookie=%llx start_s=%lld start_us=%lld sandbox=%s", active.pid,
                      (unsigned long long) active.cookie, active.start_seconds, active.start_microseconds, current);
            keeper_status("waiting", info.pid, active, current, attempts, handle);
        } else if (strcmp(current, sandbox) != 0) {
            snprintf(current, sizeof(current), "%s", sandbox);
            settled = 0;
        }
        if (state == HOOK_READY || state == HOOK_FAILED) {
            if (returned) keeper_status(state == HOOK_READY ? "ready" : "failed", info.pid, active, current, attempts, handle);
            goto wait;
        }
        int record = hook_record(current, active);
        if (record == HOOK_READY) {
            state = HOOK_READY;
            probe_log("KEEPER", "HOOK_CONFIRMED pid=%d cookie=%llx sandbox=%s", active.pid,
                      (unsigned long long) active.cookie, current);
            keeper_status("ready", info.pid, active, current, attempts, handle);
            goto wait;
        }
        if (state == HOOK_LOADING) {
            if (record == HOOK_FAILED || ++load_ticks >= 30) {
                state = HOOK_FAILED;
                probe_log("KEEPER", "INSTALL_FAILED pid=%d reason=%s handle=%llx no_reinject=1", active.pid,
                          record == HOOK_FAILED ? "worker_failed" : "ready_timeout", handle);
                keeper_status("failed", info.pid, active, current, attempts, handle);
                probe_notify("HEN Rest: hook is not ready. Check logs; use Enter Rest Mode.");
            }
            goto wait;
        }
        /* Let the standard process initialization finish before injection. */
        if (++settled < 3 || attempts >= 20) goto wait;
        ++attempts;
        if (!stage_module(current)) {
            probe_log("KEEPER", "STAGE_FAILED sandbox=%s attempt=%d", current, attempts);
            if (attempts >= 20) {
                state = HOOK_FAILED;
                keeper_status("failed", info.pid, active, current, attempts, handle);
                probe_notify("HEN Rest: module staging failed. Check logs.");
            }
            goto wait;
        }
        struct process_identity checked = {0};
        if (!probe_find_process("SceShellUI", &checked) || !probe_same_process(active, checked)) goto wait;
        struct proc_prx_load request;
        memset(&request, 0, sizeof(request));
        snprintf(request.process_name, sizeof(request.process_name), "SceShellUI");
        snprintf(request.prx_path, sizeof(request.prx_path), "%s", PROBE_PATH);
        request.res = UINT64_MAX;
        *__error() = 0;
        int result = sys_sdk_cmd(GOLDHEN_SDK_CMD_PROCESS_PRX_LOAD, &request);
        int error = *__error();
        probe_log("KEEPER", "HOOK_LOAD sandbox=%s attempt=%d status=%d errno=%d handle=0x%llx",
                  current, attempts, result, error, (unsigned long long) request.res);
        if (result >= 0 && request.res > 0 && request.res <= INT_MAX) {
            handle = (unsigned long long) request.res;
            state = HOOK_LOADING;
            load_ticks = 0;
            keeper_status("loading", info.pid, active, current, attempts, handle);
        } else if (attempts >= 20) {
            state = HOOK_FAILED;
            keeper_status("failed", info.pid, active, current, attempts, handle);
            probe_notify("HEN Rest: hook load failed. Check logs.");
        }
wait:
        sceKernelUsleep(2000000);
    }
    return NULL;
failed:
    probe_log("KEEPER", "KEEPER_FAILED identity_or_source_guard=1");
    probe_notify("HEN Rest: keeper could not start. Check Klog.");
    keeper_status("failed", core_identity.pid, (struct process_identity){0}, "", 0, 0);
    return NULL;
}

PROBE_EXPORT int module_start(size_t args, const void *argp)
{
    (void) args; (void) argp;
    if (atomic_exchange(&started, 1)) return 0;
    singleton_fd = probe_lock_file(PROBE_LOCK);
    if (singleton_fd < 0) {
        probe_log("KEEPER", "SINGLETON_NOT_ACQUIRED no_worker_started=1");
        atomic_store(&started, 0);
        return -1;
    }
    if (!probe_find_process("SceShellCore", &core_identity)) {
        probe_log("KEEPER", "CORE_IDENTITY_FAILED no_worker_started=1");
        sceKernelClose(singleton_fd); singleton_fd = -1;
        atomic_store(&started, 0); return -1;
    }
    keeper_status("starting", core_identity.pid, (struct process_identity){0}, "", 0, 0);
    int result = scePthreadCreate(&worker, NULL, run_keeper, NULL, "hen_rest_keeper");
    worker_created = result == 0;
    if (result != 0) {
        keeper_status("failed", core_identity.pid, (struct process_identity){0}, "", 0, 0);
        sceKernelClose(singleton_fd); singleton_fd = -1; atomic_store(&started, 0);
    }
    probe_log("KEEPER", "WORKER_CREATE status=%d", result);
    return result;
}
PROBE_EXPORT int module_stop(size_t args, const void *argp)
{
    (void) args; (void) argp;
    atomic_store(&stopping, 1);
    if (worker_created) {
        int result = scePthreadJoin(worker, NULL);
        if (result != 0) return result;
        worker_created = 0;
    }
    probe_log("KEEPER", "KEEPER_STOPPED ShellUI_hook_lifetime=independent");
    if (singleton_fd >= 0) { sceKernelClose(singleton_fd); singleton_fd = -1; }
    return 0;
}
