# HEN Rest — PS4 Shutdown-to-Rest Mode Plugin

GoldHEN payload and plugin for PS4 firmware 13.520.001 that redirects shutdown
to Rest Mode, with automatic startup and hook restoration.

**v0.3.1 — Experimental Preview** · Build `031-native-3`

HEN Rest starts from GoldHEN LaunchPad and, after setup, its AutoRun Queue.
No game needs to be opened. The starter loads a keeper in ShellCore; the keeper
installs a guarded hook in each new ShellUI process.

## Download and requirements

[Download HEN-Rest-v0.3.1.zip](https://github.com/j117mw3/ps4-hen-rest/releases/download/v0.3.1/HEN-Rest-v0.3.1.zip)
and [SHA256SUMS.txt](https://github.com/j117mw3/ps4-hen-rest/releases/download/v0.3.1/SHA256SUMS.txt).
Extract the ZIP. Installation uses the included prebuilt files; compiling is optional.

- PS4 firmware **13.520.001**. The firmware guard rejects other versions.
- GoldHEN installed and loaded. The console user reported **2.4b18.12** during verification.
- GoldHEN LaunchPad and AutoRun Queue, plus FTP access for the initial upload.
- Normal Rest Mode must work on your console. No game or ongoing PC service is
  required by this implementation. A PC is useful for the initial file transfer.

## Install the three files

Enable FTP in GoldHEN → Server Settings. Use an FTP client on the same network:
the PS4's IP address, port **2121**, anonymous login. Create the destination
folders if missing. Back up existing HEN Rest files and `plugins.ini` before an upgrade.

| File in the extracted ZIP | Exact destination on the PS4 |
| --- | --- |
| `keeper/dist/hen_rest_start.bin` | `/data/payloads/hen_rest_start_031r3.bin` |
| `keeper/dist/hen_rest_keeper.prx` | `/data/GoldHEN/plugins/hen_rest_keeper_031r3.prx` |
| `keeper/dist/hen_rest_hook.prx` | `/data/GoldHEN/plugins/hen_rest_hook_031r3.prx` |

**Rename all three files exactly as shown.** Upload all three from this release.
Keep the two `keeper/build/*.oelf` files on your PC; they are upload-verification
references, not console installation files.

Before upgrading, remove the previous HEN Rest entry from AutoRun Queue with Square.
Back up existing files and upload all three matching files from this release.

If upgrading from the game starter, remove only HEN Rest's old registration
lines from `/data/GoldHEN/plugins.ini` (the paths ending in
`hen_rest_start_loader.prx` or `hen_rest_loader.prx`). Preserve every other entry.
This release needs no game registration. For an already resident older build,
use **Restart PS4**, reload GoldHEN, and remain on the home screen before activation.
Do not force-unload a resident hook.

An optional Python deployer makes backups and verifies FTP readback:
`python keeper/deploy.py PS4_IP`. See [advanced installation](keeper/README.md).

## Activate and configure AutoRun

1. Open GoldHEN → LaunchPad. Select `hen_rest_start_031r3.bin` and press **X** once.
2. Wait for HEN Rest's **hook ready** / **keeper running** notifications.
   GoldHEN's generic “payload launched successfully” message alone does not confirm readiness.
3. Highlight the payload in LaunchPad and press **Square → Add to AutoRun queue**.
4. Open AutoRun Queue and confirm exactly one checked entry for
   `/data/payloads/hen_rest_start_031r3.bin`.

AutoRun is designed to start HEN Rest whenever GoldHEN loads. The earlier v0.3.0
queue configuration was visually confirmed; fresh-load AutoRun execution has
**not been verified** for this release. Manual LaunchPad activation is verified. HEN Rest must be active in
the current session before its shutdown redirect can apply.

## Status and troubleshooting

| Status/message | Meaning and action |
| --- | --- |
| Hook confirmed ready; keeper running | A matching hook reported readiness in the current ShellUI process. |
| Keeper already running; state=ready; no new worker | A repeated launch reused the existing keeper. |
| `starting`, `waiting`, or `loading` | Initialization is in progress; a returned module handle is not readiness. |
| `failed`, identity unavailable, or nothing loaded | Activation failed. Keep the exact message; check firmware, filenames and matching release files. |

The detailed status is `/data/GoldHEN/hen_rest_status.txt`. On a valid module
handle with no ready marker, installation times out after 60 seconds and reports
failure; repeated launches do not repeatedly inject that same live process.
For a failed session, use Restart PS4 and reload GoldHEN before retrying.
Report the release/build, firmware, GoldHEN build and exact error text in an issue.
Remove account details, network addresses and other personal information from logs.

GoldHEN browser/exploit failures before HEN Rest activation are not evidence of a
HEN Rest fault. The captured restoration failure and changes are described in the release notes.

## Why v0.3.1 changed

A ShellUI mutex crash was captured inside Mono during restoration. Concurrent
external Mono lookup was suspected; the exact mutex cause was not proven. Hook
installation now uses guarded native-module metadata and makes no Mono calls.
Process identity now survives clock changes across sleep, readiness requires an
advancing live sequence, and module lookup covers the larger handles seen after wake.

See the [release notes](docs/RELEASE-v0.3.1.md) for the investigation and upgrade steps.

## Verified scope

| Check on build `031-native-3` | Result |
| --- | --- |
| Home-screen LaunchPad activation without opening a game | Captured and verified |
| Shutdown-to-Rest Mode redirect | Captured and verified |
| Replacement hook readiness with advancing status after wake | Captured and verified |
| Subsequent shutdown redirected by the restored hook | Captured and verified |
| Additional repeated cycles without issues | User reported |
| Duplicate execution | Passed on preceding candidate; unchanged protection, not rerun on final build |
| Fresh-load AutoRun without a PC connection | Unverified on this build |
| Long-term stability, other firmware, every shutdown path | Unverified |

No new fatal crash was captured in the current-build test cycles. The owner also
reported several additional cycles without issues. See [observed results](docs/VERIFICATION.md).
No game or ongoing PC service is required by the implementation. These observations
do not establish long-term stability or broader firmware compatibility.

## Remove HEN Rest

1. In AutoRun Queue, highlight the payload and press **Square → Remove**.
2. Use **Restart PS4** to end the resident session. Do not run HEN Rest afterward.
3. After loading GoldHEN for FTP, delete the three exact destination files in the
   installation table. Removing files alone does not unload already resident code.
4. If the optional deployer was used, its matching backup can be restored with
   `python keeper/deploy.py PS4_IP --restore BACKUP_PATH --fresh-session`.

For [optional compiling and existing checks](keeper/README.md#build-and-checks),
see the developer guide. See [dependency notices](NOTICE.md).

Project code: GPL-3.0. Third-party components retain their own notices.
