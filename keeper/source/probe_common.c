#include "probe_common.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <orbis/libkernel.h>
#include <stdlib.h>
#include <Syscall.h>
void probe_log(const char *component, const char *format, ...)
{
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    printf("[HEN_REST_%s] %s\n", component, message);
}
void probe_notify(const char *message)
{
    OrbisNotificationRequest request;
    memset(&request, 0, sizeof(request));
    request.type = NotificationRequest;
    request.targetId = -1;
    snprintf(request.message, sizeof(request.message), "%s", message);
    sceKernelSendNotificationRequest(0, &request, sizeof(request), 0);
}
int probe_process_report(const char *component, struct proc_info *info)
{
    memset(info, 0, sizeof(*info));
    int result = sys_sdk_proc_info(info);
    probe_log(component, "PROCESS status=%d pid=%d name=%.40s title=%.16s base=0x%llx",
              result, info->pid, info->name, info->titleid, (unsigned long long) info->base_address);
    OrbisKernelSwVersion firmware;
    memset(&firmware, 0, sizeof(firmware));
    firmware.Size = sizeof(firmware);
    int fw_result = sceKernelGetSystemSwVersion(&firmware);
    probe_log(component, "FIRMWARE status=%d version=%.28s raw=0x%08x SDK_API=0x%08x probe=%s",
              fw_result, firmware.VersionString, firmware.Version, sys_sdk_version(), PROBE_VERSION);
    return result;
}

/* Read-only kern.proc.all, using the pinned payload SDK's Orbis kinfo_proc
 * layout. Validate each variable-size record before accessing it. paddr is an
 * opaque process cookie, never dereferenced. The process start timeval also
 * distinguishes lifetime when the cookie is redacted or PID is reused. */
int probe_find_process(const char *name, struct process_identity *identity)
{
    enum { NAME_OFFSET = 447, MAX_DUMP = 4 * 1024 * 1024 };
    int mib[3] = { 1, 14, 0 };
    size_t bytes = 0;
    memset(identity, 0, sizeof(*identity));
    if (orbis_syscall(202, mib, 3, NULL, &bytes, NULL, 0) != 0 ||
        bytes < NAME_OFFSET + 1 || bytes > MAX_DUMP) return 0;
    size_t capacity = bytes + 16384;
    if (capacity > MAX_DUMP) capacity = MAX_DUMP;
    unsigned char *dump = malloc(capacity);
    if (!dump) return 0;
    bytes = capacity;
    int ok = 0;
    if (orbis_syscall(202, mib, 3, dump, &bytes, NULL, 0) != 0 || bytes > capacity) goto done;
    ok = parse_process_records(dump, bytes, name, identity);
done:
    free(dump);
    if (!ok) memset(identity, 0, sizeof(*identity));
    return ok;
}

int probe_same_process(struct process_identity a, struct process_identity b)
{ return same_process_identity(a, b); }

int probe_lock_file(const char *path)
{
    int fd = sceKernelOpen(path, 0x202, 0666); /* RDWR | CREATE, no truncation */
    if (fd < 0) return -1;
    /* BSD flock LOCK_EX | LOCK_NB. The open descriptor owns the lock; the
     * kernel releases it on process exit, so a stale file cannot block boot. */
    if (orbis_syscall(131, fd, 6) != 0) { sceKernelClose(fd); return -1; }
    return fd;
}

int probe_write_text(const char *path, const char *text)
{
    char temporary[200];
    if (snprintf(temporary, sizeof(temporary), "%s.tmp", path) >= (int) sizeof(temporary)) return 0;
    int fd = sceKernelOpen(temporary, 0x601, 0666);
    if (fd < 0) return 0;
    size_t length = strlen(text), written = 0;
    while (written < length) {
        size_t amount = sceKernelWrite(fd, text + written, length - written);
        if (!amount || amount > length - written) break;
        written += amount;
    }
    int closed = sceKernelClose(fd);
    return written == length && closed == 0 && orbis_syscall(128, temporary, path) == 0;
}
