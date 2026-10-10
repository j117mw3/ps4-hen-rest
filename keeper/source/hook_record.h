#pragma once
/* Caller supplies the C library declarations and process_identity. */
static inline int parse_hook_record(const char *text, const char *version_expected,
                                    const char *build_expected, struct process_identity target)
{
    char version[16], build[32], state[16];
    struct process_identity record = {0};
    if (sscanf(text, "HEN_REST_HOOK %15s build=%31s pid=%d cookie=%llx start_s=%lld start_us=%lld state=%15s",
               version, build, &record.pid, &record.cookie, &record.start_seconds, &record.start_microseconds, state) != 7 ||
        strcmp(version, version_expected) || strcmp(build, build_expected) ||
        !same_process_identity(record, target)) return 0;
    if (!strcmp(state, "ready")) return 2;
    if (!strcmp(state, "failed")) return 3;
    return 0;
}
