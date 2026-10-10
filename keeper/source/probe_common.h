#pragma once
#include <stdint.h>
#include <GoldHEN.h>
#include <stddef.h>
#include <string.h>
#include "process_records.h"
#define PROBE_VERSION "0.3.0"
#define PROBE_BUILD "030-launchpad-4"
#define PROBE_PATH "/Temp/hen_rest_030r4/hen_rest_hook.prx"
#define PROBE_STAGE "hen_rest_030r4"
#define PROBE_LOCK "/data/GoldHEN/hen_rest.lock"
#define PROBE_STATUS "/data/GoldHEN/hen_rest_status.txt"
#define PROBE_HOOK_SOURCE "/data/GoldHEN/plugins/hen_rest_hook_030r4.prx"
#define PROBE_EXPORT __attribute__((visibility("default")))
void probe_log(const char *, const char *, ...) __attribute__((format(printf, 2, 3)));
void probe_notify(const char *);
int probe_process_report(const char *, struct proc_info *);
int probe_find_process(const char *, struct process_identity *);
int probe_same_process(struct process_identity, struct process_identity);
int probe_lock_file(const char *);
int probe_write_text(const char *, const char *);
