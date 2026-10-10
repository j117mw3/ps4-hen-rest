# HEN Rest v0.3.0 — Experimental Preview

Build `030-launchpad-4`. Install the prebuilt files from **HEN-Rest-v0.3.0.zip**;
compiling is optional. The ZIP preserves the source, tools and `.oelf` references
needed by the deployment verifier.

- Start on the PS4 home screen through GoldHEN LaunchPad.
- Add the starter to GoldHEN AutoRun Queue after its manual check passes.
- Remove the Elden Ring dependency and game-specific registration.
- Match hook readiness to the current ShellUI process and build.
- Use descriptor-owned locks to prevent duplicate keeper workers.
- Report loading and installation failures explicitly.
- Restore after ShellUI process replacement, including reused sandbox names.
- Preserve firmware, instruction-byte and stack-alignment guards.

Target firmware: 13.520.001 only.
GoldHEN build reported: 2.4b18.12.
At initial publication, observed v0.3.0 console results were: manual home-screen LaunchPad startup passed,
with confirmed keeper and hook readiness. A repeated launch reported already
running and produced no second module load, worker creation or patch write.
AutoRun Queue configuration was visually confirmed enabled. Fresh-load AutoRun,
startup without a PC connection, wake restoration and a subsequent shutdown
redirect were **unverified**. No additional console checks were performed for
initial publication. See the [initial evidence snapshot](https://github.com/j117mw3/ps4-hen-rest/blob/v0.3.0/docs/VERIFICATION.md)
and the post-publication update below.

Assets: **HEN-Rest-v0.3.0.zip** and **SHA256SUMS.txt**. The ZIP also includes
checksums for its contents. The tested artifacts are packaged without rebuilding.

This is an experimental preview with no claim of long-term stability or
universal firmware support. See keeper/README.md for installation/removal and
NOTICE.md for dependency attribution.

## Post-publication verification update

**Rest Mode wake readiness: verified by user report.** After waking from Rest Mode,
the console user confirmed that the hook notification reappeared indicating it was
enabled and listening. No new process-lifetime records or Klog capture accompanied
this report. Full-restart/fresh-load AutoRun and a subsequent shutdown redirect
remain unverified; this report does not establish long-term stability.

See the [updated verification record](https://github.com/j117mw3/ps4-hen-rest/blob/main/docs/VERIFICATION.md).
The installation ZIP, tested binaries and original release tag remain unchanged.
