# Observed results

## v0.3.0 Experimental Preview

Build: 030-launchpad-4.
Firmware guard: 13.520.001 / 0x13520001.
GoldHEN build reported by the console user: **2.4b18.12**.
LaunchPad activation and duplicate execution passed on this build. AutoRun,
fresh-load startup without a PC connection and Rest Mode restoration remain unverified.
The AutoRun Queue entry was visually confirmed enabled in a user-supplied photo.
That configuration does not establish execution after a fresh GoldHEN load.

Host build succeeded. Both PRX module exports and INIT/FINI entry points were
inspected. The flat payload enters at zero, contains its BSS, and has no dynamic
linker, unresolved symbols or relocation records. Process-record bounds and
ambiguity checks, process/build-specific readiness, registration preservation,
artifact-manifest checks and expanded-ELF readback tests passed.

The three console uploads were read back and verified. The binary matched
byte for byte. PRX uploads matched every packaged ELF program header and
segment (GoldHEN FTP expands SELF to ELF). The old game starter's active
registration was removed; other configuration and old modules were preserved.

Console observations for this release:

- **Passed:** Manual LaunchPad startup from the home screen, without opening a game.
- **Passed:** Duplicate execution leaves one worker and one patch; no second module load,
  worker creation or patch write was recorded, and matched ready records were unchanged.
- **Unverified:** AutoRun starts HEN Rest after a fresh GoldHEN load, without a PC dependency.
- **Unverified:** Rest Mode/wake replaces ShellUI and the new process receives a ready hook.
- **Unverified:** A subsequent shutdown request is redirected by this build.

The owner requested publication as an experimental prerelease with these remaining
checks explicitly unverified, and requested no further PS4 checks. Successful
payload-context SDK loading was recorded during the verified LaunchPad run.
The tested binaries were packaged unchanged, without rebuilding for publication.

The successful run used process start timestamps from the measured 1096-byte
process record rather than the unavailable kernel boot-time query. Firmware
and exact native instruction guards passed. The starter received a module
handle, then waited for a matched keeper ready record. The hook wrote a
separate build- and process-lifetime-specific ready marker.

## Earlier builds

A v0.1.0 manual shutdown request entered Rest Mode, and GoldHEN remained active
on controller wake. v0.2.0 initially installed its keeper and hook successfully.

Later v0.2.0 Klog observations recorded a shutdown redirect accepted at
2026-10-10 00:02:06 UTC, a replacement hook ready at 00:02:20, and keeper
confirmation at 00:02:22. These are observations of one successful later
restoration. Earlier readiness gaps remain unresolved.

An intermittent wake error was reported after hours of inactivity, without an
established faulting module or error-code capture. One subsequent wake had no
visible recurrence. A separate saved Cannot install error is not evidence of
the cause of the wake report. No long-term stability conclusion follows.

Console backups, operational paths, addresses, account data and raw logs are
kept local and excluded from this repository.
