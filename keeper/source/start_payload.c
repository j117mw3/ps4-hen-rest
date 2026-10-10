/* One-shot GoldHEN LaunchPad / AutoRun starter. No game registration. */
#include <kernel.h>
#include <libc.h>
#include <syscall.h>
#include <proc.h>
#include "process_records.h"
#include "startup_records.h"

#define VERSION "0.3.0"
#define BUILD "030-launchpad-4"
#define KEEPER_PATH "/data/GoldHEN/plugins/hen_rest_keeper_030r4.prx"
#define STATUS_PATH "/data/GoldHEN/hen_rest_status.txt"
#define LOCK_PATH "/data/GoldHEN/hen_rest.lock"
#define RECEIPT_PATH "/data/GoldHEN/hen_rest_start_receipt.txt"
/* SDK commands use syscall 500. Copy the packed ABI, not platform libc types. */
struct sdk_proc_info {
    int pid; char name[40], path[64], titleid[16], contentid[64], version[6];
    unsigned long long base_address;
} __attribute__((packed));
struct sdk_prx_load {
    char process_name[32], prx_path[100]; unsigned long long res;
} __attribute__((packed));
_Static_assert(__builtin_offsetof(struct kinfo_proc, pid) == 72, "PID layout");
_Static_assert(__builtin_offsetof(struct kinfo_proc, name) == 447, "name layout");
_Static_assert(__builtin_offsetof(struct kinfo_proc, paddr) == 16, "cookie layout");

static void report(const char *event, const char *message)
{
    char text[512];
    snprintf(text, sizeof(text), "[HEN_REST_START] %s version=%s %s\n", event, VERSION, message);
    syscall(601, 7, text, 0);
    SceNotificationRequest req;
    memset(&req, 0, sizeof(req)); req.target_id = -1;
    snprintf(req.message, sizeof(req.message), "HEN Rest: %s", message);
    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}

static int find_named_process(const char *name, struct process_identity *identity, int verbose)
{
    int mib[] = {1, 14, 0}; size_t bytes = 0;
    errno = 0; long query = (long) syscall(202, mib, 3, 0, &bytes, 0, 0);
    char log[256];
    snprintf(log, sizeof(log), "[HEN_REST_START] PROCESS_QUERY status=%ld errno=%d bytes=%llu\n", query, errno, (unsigned long long)bytes);
    if (verbose) syscall(601, 7, log, 0);
    if (query != 0 || bytes < 448 || bytes > 4194304) return 0;
    size_t capacity = bytes + 16384;
    if (capacity > 4194304) capacity = 4194304;
    unsigned char *dump = malloc(capacity);
    if (!dump) return 0;
    bytes = capacity;
    errno = 0; long result = (long) syscall(202, mib, 3, dump, &bytes, 0, 0);
    int ok = result == 0 && bytes <= capacity && parse_process_records(dump, bytes, name, identity);
    snprintf(log, sizeof(log), "[HEN_REST_START] PROCESS_READ status=%ld errno=%d bytes=%llu parsed=%d\n", result, errno, (unsigned long long)bytes, ok);
    if (verbose) syscall(601, 7, log, 0);
    if (verbose && result == 0 && bytes <= capacity && !ok) {
        int n = 0;
        for (size_t offset = 0; offset + 448 <= bytes && n < 32; ++n) {
            int length, pid; memcpy(&length, dump + offset, 4);
            if (length <= 447 || (size_t)length > bytes - offset) break;
            memcpy(&pid, dump + offset + 72, 4);
            snprintf(log, sizeof(log), "[HEN_REST_START] PROCESS_RECORD offset=%llu bytes=%d pid=%d name=%.31s\n",
                     (unsigned long long)offset, length, pid, (char *)dump + offset + 447);
            syscall(601, 7, log, 0); offset += (size_t)length;
        }
    }
    free(dump); return ok;
}

/* -2 means definitely busy; -1 means unsupported/denied, never assume active. */
static int acquire(const char *path)
{
    int fd = sceKernelOpen(path, 0x202, 0666);
    if (fd < 0) return -1;
    errno = 0;
    long result = (long) syscall(131, fd, 6);
    int error = errno;
    if (result == 0) return fd;
    sceKernelClose(fd); return error == 35 ? -2 : -1;
}

static int read_text(const char *path, char *buffer, size_t capacity)
{
    memset(buffer, 0, capacity);
    int fd = sceKernelOpen(path, 0, 0);
    if (fd < 0) return 0;
    int n = sceKernelRead(fd, buffer, capacity - 1); sceKernelClose(fd);
    return n > 0 && (size_t) n < capacity;
}

static int current_state(struct process_identity core, char *state)
{
    char text[512]; struct process_identity recorded_target = {0}, live = {0};
    if (!read_text(STATUS_PATH, text, sizeof(text)) ||
        !parse_keeper_record(text, VERSION, BUILD, core, state, &recorded_target)) return 0;
    /* A ready file from the last UI lifetime must not imply current readiness. */
    if (!strcmp(state, "ready") && (!find_named_process("SceShellUI", &live, 0) ||
                                   !same_process_identity(recorded_target, live))) {
        snprintf(state, 16, "waiting");
    }
    return 1;
}

static int attempted(struct process_identity core)
{
    char text[240];
    return read_text(RECEIPT_PATH, text, sizeof(text)) &&
        startup_receipt_matches(text, VERSION, BUILD, core);
}

static int write_receipt(struct process_identity core)
{
    char text[240];
    int n = snprintf(text, sizeof(text), "HEN_REST_START %s build=%s core_pid=%d core_cookie=%llx core_start_s=%lld core_start_us=%lld attempted=1\n",
                     VERSION, BUILD, core.pid, core.cookie, core.start_seconds, core.start_microseconds);
    int fd = sceKernelOpen(RECEIPT_PATH, 0x601, 0666);
    if (fd < 0) return 0;
    int written = (int) syscall(4, fd, text, n), closed = sceKernelClose(fd);
    return written == n && closed == 0;
}

int _main(void)
{
    extern unsigned char __payload_bss_start[] __attribute__((visibility("hidden")));
    extern unsigned char __payload_bss_end[] __attribute__((visibility("hidden")));
    for (volatile unsigned char *p = __payload_bss_start; p < __payload_bss_end; ++p) *p = 0;
    initKernel(); initLibc();
    if (!__error || !memset || !snprintf || !sscanf || !strcmp || !strstr || !strlen || !memcpy || !memcmp ||
        !malloc || !free || !sceKernelOpen || !sceKernelRead || !sceKernelClose ||
        !sceKernelUsleep || !sceKernelGetSystemSwVersion || !sceKernelSendNotificationRequest) return -1;
    SceFwInfo fw; memset(&fw, 0, sizeof(fw)); fw.unk1 = sizeof(fw);
    if (sceKernelGetSystemSwVersion(&fw) != 0 || fw.version != 0x13520001) {
        report("FAILED", "unsupported firmware; nothing loaded."); return -1;
    }
    struct sdk_proc_info own; memset(&own, 0, sizeof(own));
    errno = 0;
    long sdk = (long) syscall(500, 4, &own);
    char detail[256];
    snprintf(detail, sizeof(detail), "SDK_CHECK result=%ld errno=%d pid=%d name=%.40s", sdk, errno, own.pid, own.name);
    char line[320]; snprintf(line, sizeof(line), "[HEN_REST_START] %s\n", detail); syscall(601, 7, line, 0);
    if (sdk != 0 || own.pid <= 0 || !getpid || own.pid != getpid()) {
        report("FAILED", "GoldHEN SDK unavailable in payload context; nothing loaded."); return -1;
    }
    struct process_identity core = {0};
    if (!find_named_process("SceShellCore", &core, 1)) { report("FAILED", "ShellCore identity unavailable; nothing loaded."); return -1; }
    snprintf(line, sizeof(line), "[HEN_REST_START] CORE_IDENTITY pid=%d cookie=%llx start_s=%lld start_us=%lld\n",
             core.pid, core.cookie, core.start_seconds, core.start_microseconds);
    syscall(601, 7, line, 0);
    int start_lock = acquire("/data/GoldHEN/hen_rest_start.lock");
    if (start_lock < 0) { report("FAILED", "starter lock busy or unavailable; no duplicate load."); return -1; }
    int keeper_lock = acquire(LOCK_PATH);
    char state[16] = "";
    if (keeper_lock == -2) {
        int known = current_state(core, state);
        snprintf(detail, sizeof(detail), "keeper already running; state=%s; no new worker.", known ? state : "unknown");
        report(known && strcmp(state, "failed") != 0 ? "ALREADY_RUNNING" : "FAILED", detail);
        sceKernelClose(start_lock); return known && strcmp(state, "failed") != 0 ? 0 : -1;
    }
    if (keeper_lock < 0 || attempted(core)) {
        if (keeper_lock >= 0) sceKernelClose(keeper_lock);
        report("FAILED", "lock unavailable or earlier load needs a fresh session; no retry.");
        sceKernelClose(start_lock); return -1;
    }
    sceKernelClose(keeper_lock);
    /* Persist attempt before SDK entry, preventing a second load after an
     * ambiguous SDK result. A fresh ShellCore lifetime permits another try. */
    if (!write_receipt(core)) { report("FAILED", "cannot save load receipt; nothing loaded."); sceKernelClose(start_lock); return -1; }
    struct sdk_prx_load request; memset(&request, 0, sizeof(request));
    snprintf(request.process_name, sizeof(request.process_name), "SceShellCore");
    snprintf(request.prx_path, sizeof(request.prx_path), "%s", KEEPER_PATH);
    request.res = ~0ULL; errno = 0;
    long result = (long) syscall(500, 6, &request);
    snprintf(line, sizeof(line), "[HEN_REST_START] KEEPER_LOAD status=%ld errno=%d handle=%llx core_pid=%d core_cookie=%llx\n",
             result, errno, request.res, core.pid, core.cookie); syscall(601, 7, line, 0);
    if (result < 0 || request.res == 0 || request.res > 0x7fffffff) {
        report("FAILED", "keeper SDK load failed; check Klog. No automatic retry."); sceKernelClose(start_lock); return -1;
    }
    /* A module handle does not prove a running worker or installed hook. */
    int observed = 0;
    for (int i = 0; i < 120; ++i) {
        keeper_lock = acquire(LOCK_PATH);
        int busy = keeper_lock == -2;
        if (keeper_lock >= 0) sceKernelClose(keeper_lock);
        if (busy && current_state(core, state)) {
            if (strcmp(state, "ready") == 0) { observed = 1; report("READY", "hook confirmed ready; keeper running."); break; }
            if (strcmp(state, "failed") == 0) { report("FAILED", "keeper reports failed installation; check Klog."); break; }
        }
        sceKernelUsleep(250000);
    }
    if (!observed && strcmp(state, "failed") != 0) report("LOADING", "load returned a handle; readiness unconfirmed. Check status/Klog.");
    sceKernelClose(start_lock); return observed ? 0 : -1;
}
