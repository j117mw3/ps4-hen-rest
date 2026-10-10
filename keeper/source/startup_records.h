#pragma once
/* Receipts apply to one Core process lifetime, not just a reusable PID. */
static inline int startup_receipt_matches(const char *text, const char *expected_version,
    const char *expected_build, struct process_identity core)
{
    char version[16], build[32]; int attempted = 0;
    struct process_identity record = {0};
    return process_identity_valid(core) &&
        sscanf(text, "HEN_REST_START %15s build=%31s core_pid=%d core_cookie=%llx core_start_s=%lld core_start_us=%lld attempted=%d",
               version, build, &record.pid, &record.cookie, &record.start_seconds, &record.start_microseconds, &attempted) == 7 &&
        !strcmp(version, expected_version) && !strcmp(build, expected_build) &&
        attempted == 1 && same_process_identity(record, core);
}
static inline int parse_keeper_record(const char *text, const char *version_expected,
    const char *build_expected, struct process_identity core, char *state,
    struct process_identity *target)
{
    char version[16], expected_tail[48];
    struct process_identity recorded_core = {0};
    memset(target, 0, sizeof(*target));
    if (sscanf(text, "HEN_REST %15s state=%15s core_pid=%d core_cookie=%llx core_start_s=%lld core_start_us=%lld shell_pid=%d cookie=%llx shell_start_s=%lld shell_start_us=%lld",
               version, state, &recorded_core.pid, &recorded_core.cookie,
               &recorded_core.start_seconds, &recorded_core.start_microseconds,
               &target->pid, &target->cookie, &target->start_seconds, &target->start_microseconds) != 10 ||
        strcmp(version, version_expected) || !same_process_identity(recorded_core, core)) return 0;
    int n = snprintf(expected_tail, sizeof(expected_tail), " build=%s\n", build_expected);
    if (n < 0 || (size_t)n >= sizeof(expected_tail)) return 0;
    size_t length = strlen(text);
    if (length < (size_t)n || memcmp(text + length - (size_t)n, expected_tail, (size_t)n)) return 0;
    return !strcmp(state, "starting") || !strcmp(state, "waiting") || !strcmp(state, "loading") ||
           !strcmp(state, "ready") || !strcmp(state, "failed");
}
