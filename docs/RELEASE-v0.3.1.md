# HEN Rest v0.3.1 — Rest Mode Restoration Fix

Experimental prerelease · Build **031-native-3**

This update addresses hook restoration after Rest Mode. Install the prebuilt
files from **HEN-Rest-v0.3.1.zip**; compiling is optional.

## Why this update was needed

During testing, ShellUI's main thread crashed with a fatal mutex assertion inside
the Mono runtime while a replacement hook was being installed. The console later
displayed **CE-36329-3**. Concurrent external Mono lookup during ShellUI startup
was a suspected contributor; the precise mutex ownership cause was not proven.

Hook installation now makes **no Mono calls**. It reads existing module metadata,
matches the exact system-image fingerprint, resolves the native power method
relative to that image, and checks an instruction digest and the original prologue
before patching. No Sony system module is included in this release.

Additional recovery fixes:

- Normalize ShellUI process lifetime against ShellCore to survive clock changes
  across sleep and recognize new processes even when sandbox names repeat.
- Require an advancing live readiness sequence for the current process and build;
  a returned module handle or stale marker alone is not success.
- Search module handles through 65535. A diagnostic found the correct image at
  handle 8381 after wake, beyond the preceding candidate's limit of 4095.
- Preserve firmware/instruction guards, stack realignment, singleton keeper
  protection, explicit failed states, and refusal to force-unload patched code.

## Testing and compatibility

Target: **PS4 firmware 13.520.001**, user-reported **GoldHEN 2.4b18.12**.
Manual LaunchPad activation on the home screen was captured without opening a
game. Captured cycles confirmed shutdown-to-Rest Mode redirection, replacement
hook installation with advancing readiness after wake, direct Rest Mode/wake
restoration, and another shutdown redirected by the restored hook.

No new fatal crash was captured in those current-build cycles. The owner then
reported several additional cycles without issues. The earlier duplicate-run
check passed on the preceding candidate; the singleton code is unchanged, but
that check was not repeated on the final build.

These results support the tested behavior on this setup. **Fresh-load AutoRun on
this build, long-term stability, other firmware, and every shutdown path remain
unverified.** No game or ongoing PC service is required by the implementation.

## Upgrade

1. Remove the previous HEN Rest entry from GoldHEN AutoRun Queue with Square.
2. Back up existing files and upload all three matching files from this ZIP:

| ZIP file | Exact console destination |
| --- | --- |
| `keeper/dist/hen_rest_start.bin` | `/data/payloads/hen_rest_start_031r3.bin` |
| `keeper/dist/hen_rest_keeper.prx` | `/data/GoldHEN/plugins/hen_rest_keeper_031r3.prx` |
| `keeper/dist/hen_rest_hook.prx` | `/data/GoldHEN/plugins/hen_rest_hook_031r3.prx` |

3. Restart PS4, reload GoldHEN, and remain on the home screen. Never force-unload
   a resident hook or mix release files.
4. Run **hen_rest_start_031r3.bin** once from LaunchPad and wait for HEN Rest's
   hook-ready / keeper-running notifications.
5. Highlight it and press **Square → Add to AutoRun queue**. Confirm exactly one
   checked entry for the new payload and no older HEN Rest entry.

The two `.oelf` files stay on the PC as deployment-verification references.
The ZIP preserves the directory layout required by `keeper/deploy.py`.
Verify the download using **SHA256SUMS.txt**.

The installation binaries are the tested artifacts, packaged without rebuilding.
GPL-3.0 project licensing and dependency notices are retained. Console backups,
raw logs, diagnostics, private system binaries, and personal screenshots stay local.
The v0.3.0 tag and release assets are preserved.
