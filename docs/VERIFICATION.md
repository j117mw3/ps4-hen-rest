# Observed results

## v0.3.0 Experimental Preview

Build: 030-launchpad-4.
Firmware guard: 13.520.001 / 0x13520001.
GoldHEN build reported by the console user: **2.4b18.12**.
LaunchPad activation and duplicate execution passed on this build. After publication,
the console user confirmed the hook readiness notification returned after Rest Mode
wake. Fresh-load AutoRun without a PC connection remains unverified.
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
- **Verified by user report:** After Rest Mode wake, the hook notification reappeared
  indicating that it was enabled and listening. This confirms the visible readiness
  notification for that cycle; no new process-lifetime record or Klog capture was obtained.
- **Unverified:** A subsequent shutdown request is redirected by this build.

The owner requested publication as an experimental prerelease with these remaining
checks explicitly unverified, and requested no further PS4 checks. Successful
payload-context SDK loading was recorded during the verified LaunchPad run.
The tested binaries were packaged unchanged, without rebuilding for publication.

### Post-publication Rest Mode report

The owner subsequently reported a normal Rest Mode/wake cycle followed by the
hook's enabled/listening notification. The observed wake behavior is recorded as
verified by that report. It does not establish a full PS4 restart, fresh GoldHEN
AutoRun execution, a subsequent redirected shutdown, or long-term stability.
No additional console connections or scheduled checks were started to obtain
this report. The original v0.3.0 installation ZIP and tag retain the evidence
snapshot from initial publication; this follow-up is documented on the main branch
and the release page.

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
