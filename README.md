# HEN Rest — PS4 Shutdown-to-Rest Mode Plugin

GoldHEN payload and plugin for PS4 firmware 13.520.001 that redirects shutdown
to Rest Mode, with automatic startup and hook restoration.

**v0.3.0 — Experimental Preview** · Build `030-launchpad-4`

HEN Rest starts from GoldHEN LaunchPad and, after setup, its AutoRun Queue.
No game needs to be opened. The starter loads a keeper in ShellCore; the keeper
installs a guarded hook in each new ShellUI process.

## Download and requirements

[Download HEN-Rest-v0.3.0.zip](https://github.com/j117mw3/ps4-hen-rest/releases/download/v0.3.0/HEN-Rest-v0.3.0.zip)
and [SHA256SUMS.txt](https://github.com/j117mw3/ps4-hen-rest/releases/download/v0.3.0/SHA256SUMS.txt).
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
| `keeper/dist/hen_rest_start.bin` | `/data/payloads/hen_rest_start.bin` |
| `keeper/dist/hen_rest_keeper.prx` | `/data/GoldHEN/plugins/hen_rest_keeper_030r4.prx` |
| `keeper/dist/hen_rest_hook.prx` | `/data/GoldHEN/plugins/hen_rest_hook_030r4.prx` |

**Rename the two PRX files exactly as shown.** Upload all three from this release.
Keep the two `keeper/build/*.oelf` files on your PC; they are upload-verification
references, not console installation files.

If upgrading from the game starter, remove only HEN Rest's old registration
lines from `/data/GoldHEN/plugins.ini` (the paths ending in
`hen_rest_start_loader.prx` or `hen_rest_loader.prx`). Preserve every other entry.
This release needs no game registration. For an already resident older build,
use **Restart PS4**, reload GoldHEN, and remain on the home screen before activation.
Do not force-unload a resident hook.

An optional Python deployer makes backups and verifies FTP readback:
`python keeper/deploy.py PS4_IP`. See [advanced installation](keeper/README.md).

## Activate and configure AutoRun

1. Open GoldHEN → LaunchPad. Select `hen_rest_start.bin` and press **X** once.
2. Wait for HEN Rest's **hook ready** / **keeper running** notifications.
   GoldHEN's generic “payload launched successfully” message alone does not confirm readiness.
3. Highlight the payload in LaunchPad and press **Square → Add to AutoRun queue**.
4. Open AutoRun Queue and confirm exactly one checked entry for
   `/data/payloads/hen_rest_start.bin`.

AutoRun is designed to start HEN Rest whenever GoldHEN loads. Its queue configuration
was visually confirmed; fresh-load AutoRun execution has **not been verified** for
this release. Manual LaunchPad activation is verified. HEN Rest must be active in
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
HEN Rest fault. An earlier intermittent wake dialog has no established cause.

## Verified scope

| Check on build `030-launchpad-4` | Result |
| --- | --- |
| Home-screen LaunchPad activation without opening a game | Verified |
| Duplicate execution: one keeper worker and one patch | Verified |
| AutoRun Queue entry enabled | Visually confirmed |
| Automatic startup after a fresh GoldHEN load without a PC | Unverified |
| Hook readiness notification after Rest Mode wake | Verified (user-reported notification) |
| A subsequent shutdown request redirected by this build | Unverified |

The console user subsequently confirmed that, after waking from Rest Mode,
the hook notification reappeared indicating it was enabled and listening.
This verifies the visible readiness notification for that wake cycle. Process
lifetime records and a subsequent shutdown redirect were not captured in this
follow-up. A full PS4 restart with a fresh GoldHEN load is a separate check.

See [observed results](docs/VERIFICATION.md). This preview makes no long-term stability or broader
firmware compatibility claims. Physical-button and other shutdown paths are unverified.

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
