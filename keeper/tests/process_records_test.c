#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../source/process_records.h"
#include "../source/hook_record.h"
#include "../source/startup_records.h"
static void record(unsigned char *p, int pid, unsigned long long cookie, const char *name)
{
    int size = 1096; memset(p, 0, 1096); memcpy(p, &size, 4);
    memcpy(p + 72, &pid, 4); memcpy(p + 16, &cookie, 8);
    long long seconds = 1791620000, microseconds = 12345;
    memcpy(p + 336, &seconds, 8); memcpy(p + 344, &microseconds, 8);
    strcpy((char *) p + 447, name);
}
int main(void)
{
    unsigned char dump[2192]; struct process_identity out;
    record(dump, 21, 123, "SceShellCore"); record(dump + 1096, 22, 124, "SceShellUI");
    assert(parse_process_records(dump, sizeof(dump), "SceShellUI", &out) && out.pid == 22 && out.cookie == 124);
    assert(!parse_process_records(dump, sizeof(dump), "Missing", &out));
    assert(!parse_process_records(dump, 1000, "SceShellCore", &out));
    record(dump + 1096, 23, 125, "SceShellUI"); record(dump, 22, 124, "SceShellUI");
    assert(!parse_process_records(dump, sizeof(dump), "SceShellUI", &out));
    record(dump + 1096, 22, 124, "SceShellUI");
    assert(parse_process_records(dump, sizeof(dump), "SceShellUI", &out) && out.pid == 22 && out.cookie == 124);
    record(dump + 1096, 22, 125, "SceShellUI");
    assert(!parse_process_records(dump, sizeof(dump), "SceShellUI", &out));
    int bad_size = -1; memcpy(dump, &bad_size, 4);
    assert(!parse_process_records(dump, sizeof(dump), "SceShellUI", &out));
    record(dump, 22, 124, "SceShellUI"); memset(dump + 447, 'x', 649);
    assert(!parse_process_records(dump, 1096, "SceShellUI", &out));
    const char *ready = "HEN_REST_HOOK 0.3.0 build=030-launchpad-1 pid=22 cookie=7c start_s=1791620000 start_us=12345 state=ready retained=1";
    struct process_identity target = {22, 124, 1791620000, 12345};
    assert(parse_hook_record(ready, "0.3.0", "030-launchpad-1", target) == 2);
    target.pid = 23; /* Reused sandbox cannot reuse the old ready marker. */
    assert(parse_hook_record(ready, "0.3.0", "030-launchpad-1", target) == 0);
    target.pid = 22; target.cookie = 125;
    assert(parse_hook_record(ready, "0.3.0", "030-launchpad-1", target) == 0);
    target.cookie = 124;
    assert(parse_hook_record(ready, "0.3.0", "other-build", target) == 0);
    assert(parse_hook_record(ready, "0.2.0", "030-launchpad-1", target) == 0);
    assert(parse_hook_record("HEN_REST_HOOK 0.3.0 build=030-launchpad-1 pid=22 cookie=7c start_s=1791620000 start_us=12345 state=loading", "0.3.0", "030-launchpad-1", target) == 0);
    assert(parse_hook_record("HEN_REST_HOOK 0.3.0 build=030-launchpad-1 pid=22 cookie=7c start_s=1791620000 start_us=12345 state=failed", "0.3.0", "030-launchpad-1", target) == 3);
    ++target.start_microseconds;
    assert(parse_hook_record(ready, "0.3.0", "030-launchpad-1", target) == 0);
    --target.start_microseconds;
    const char *receipt = "HEN_REST_START 0.3.0 build=030-launchpad-4 core_pid=22 core_cookie=7c core_start_s=1791620000 core_start_us=12345 attempted=1";
    assert(startup_receipt_matches(receipt, "0.3.0", "030-launchpad-4", target));
    ++target.start_seconds;
    assert(!startup_receipt_matches(receipt, "0.3.0", "030-launchpad-4", target));
    --target.start_seconds; ++target.start_microseconds;
    assert(!startup_receipt_matches(receipt, "0.3.0", "030-launchpad-4", target));
    --target.start_microseconds;
    assert(!startup_receipt_matches("HEN_REST_START 0.3.0 build=030-launchpad-4 core_pid=22 core_cookie=7c core_start_s=1791620000 core_start_us=12345 attempted=0", "0.3.0", "030-launchpad-4", target));
    /* Duplicate PID/cookie but different start timeval is a conflicting record. */
    record(dump, 22, 0, "SceShellUI"); record(dump + 1096, 22, 0, "SceShellUI");
    assert(parse_process_records(dump, sizeof(dump), "SceShellUI", &out));
    long long changed_start = 12346;
    memcpy(dump + 1096 + 344, &changed_start, 8);
    assert(!parse_process_records(dump, sizeof(dump), "SceShellUI", &out));
    changed_start = 0; memcpy(dump + 336, &changed_start, 8);
    assert(!parse_process_records(dump, 1096, "SceShellUI", &out));
    record(dump, 22, 0, "SceShellUI");
    changed_start = 1000000; memcpy(dump + 344, &changed_start, 8);
    assert(!parse_process_records(dump, 1096, "SceShellUI", &out));
    changed_start = -1; memcpy(dump + 344, &changed_start, 8);
    assert(!parse_process_records(dump, 1096, "SceShellUI", &out));
    record(dump, 22, 0, "SceShellUI"); int wrong_size = 512;
    memcpy(dump, &wrong_size, 4);
    assert(!parse_process_records(dump, 512, "SceShellUI", &out));
    /* Actual timestamp bytes from the hardware diagnostic. */
    record(dump, 42, 0, "SceShellCore");
    long long captured_s = 0x6aca4c7c, captured_us = 0x223c7;
    memcpy(dump + 336, &captured_s, 8); memcpy(dump + 344, &captured_us, 8);
    assert(parse_process_records(dump, 1096, "SceShellCore", &out));
    assert(out.pid == 42 && out.cookie == 0 && out.start_seconds == 1791642748 && out.start_microseconds == 140231);
    char state[16]; struct process_identity current = {42, 0, 1791642748, 140231}, target_record;
    const char *status = "HEN_REST 0.3.0 state=ready core_pid=42 core_cookie=0 core_start_s=1791642748 core_start_us=140231 shell_pid=43 cookie=0 shell_start_s=1791642751 shell_start_us=555635 sandbox=test attempts=1 handle=1 build=030-launchpad-4\n";
    assert(parse_keeper_record(status, "0.3.0", "030-launchpad-4", current, state, &target_record));
    assert(!strcmp(state, "ready") && target_record.pid == 43 && target_record.start_microseconds == 555635);
    ++current.start_microseconds;
    assert(!parse_keeper_record(status, "0.3.0", "030-launchpad-4", current, state, &target_record));
    --current.start_microseconds;
    assert(!parse_keeper_record(status, "0.3.0", "other-build", current, state, &target_record));
    assert(!parse_keeper_record(status, "0.2.0", "030-launchpad-4", current, state, &target_record));
    puts("PASS: process bounds, thread deduplication, lifetime and stale receipt/readiness guards");
}
