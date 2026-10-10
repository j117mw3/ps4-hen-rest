#pragma once
/* Firmware 13.520.001 exports 1096-byte kinfo_proc records. PID and name
 * offsets match the pinned payload SDK. ki_start's timeval at 336/344
 * matches the FreeBSD amd64 layout and the recorded Core/UI launch times
 * (2026-10-10T14:54:41Z diagnostic). Pointers are redacted on this console;
 * never use a zero paddr alone to identify a process lifetime. */
struct process_identity {
    int pid; unsigned long long cookie;
    long long start_seconds, start_microseconds;
};
static inline int process_identity_valid(struct process_identity p)
{ return p.pid > 0 && p.start_seconds > 0 && p.start_microseconds >= 0 && p.start_microseconds < 1000000; }
static inline int same_process_identity(struct process_identity a, struct process_identity b)
{
    return process_identity_valid(a) && process_identity_valid(b) &&
           a.pid == b.pid && a.cookie == b.cookie &&
           a.start_seconds == b.start_seconds && a.start_microseconds == b.start_microseconds;
}
static inline int parse_process_records(const unsigned char *dump, size_t bytes,
                                        const char *name, struct process_identity *out)
{
    size_t wanted = strlen(name);
    int matches = 0;
    memset(out, 0, sizeof(*out));
    for (size_t offset = 0; offset < bytes;) {
        int length, pid;
        if (bytes - offset < 4) return 0;
        memcpy(&length, dump + offset, 4);
        if (length <= 447 || (size_t) length > bytes - offset) return 0;
        size_t available = (size_t) length - 447, n = 0;
        const char *record_name = (const char *) dump + offset + 447;
        while (n < available && record_name[n]) ++n;
        if (n == available) return 0;
        if (n == wanted && memcmp(record_name, name, wanted) == 0) {
            if (length != 1096) return 0; /* Do not guess another firmware ABI. */
            struct process_identity p = {0};
            memcpy(&pid, dump + offset + 72, 4); p.pid = pid;
            memcpy(&p.cookie, dump + offset + 16, 8);
            memcpy(&p.start_seconds, dump + offset + 336, 8);
            memcpy(&p.start_microseconds, dump + offset + 344, 8);
            if (!process_identity_valid(p)) return 0;
            /* KERN_PROC_ALL includes one record per thread on this firmware.
             * Collapse repeated identities, but never choose between processes. */
            if (matches && !same_process_identity(*out, p)) return 0;
            matches = 1;
            *out = p;
        }
        offset += (size_t) length;
    }
    return matches == 1;
}
