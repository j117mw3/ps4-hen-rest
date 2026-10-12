# HEN Rest — PS4 Shutdown-to-Rest Mode Plugin

v0.3.1 Experimental Preview: one-shot GoldHEN LaunchPad payload, ShellCore keeper, and
ShellUI hook. No game needs to be opened and no game registration is installed.
The current target is **PS4 firmware 13.520.001** (`0x13520001`).

## Installation and activation

1. Upload `dist/hen_rest_start.bin` to `/data/payloads/hen_rest_start_031r3.bin`.
2. Upload `dist/hen_rest_keeper.prx` as
   `/data/GoldHEN/plugins/hen_rest_keeper_031r3.prx` and `dist/hen_rest_hook.prx` as
   `/data/GoldHEN/plugins/hen_rest_hook_031r3.prx`.
3. Before upgrading, remove the previous HEN Rest entry from AutoRun Queue with
   Square. Upload all three matching files. Remove active HEN Rest game starters
   from `plugins.ini` while preserving other plugins.
4. When replacing an older resident build, use **Restart PS4**, reload GoldHEN,
   and stay on the home screen. Do not force-unload a live hook.
5. Run `hen_rest_start_031r3.bin` from GoldHEN LaunchPad. Wait for the ready notification
   and a current `ready` status before trying Turn Off PS4.
6. After the manual and duplicate-run checks pass, highlight `hen_rest_start_031r3.bin`
   in LaunchPad and press **Square: Add to AutoRun queue**. Open AutoRun Queue
   and confirm exactly one entry for `/data/payloads/hen_rest_start_031r3.bin`, checked
   enabled. These controller steps were observed on GoldHEN 2.4b18.12.
   Confirm startup after a fresh GoldHEN load.

`python keeper/deploy.py PS4_IP` performs steps 1–3 with backups and readback
verification. It stages files only. It does not run a payload or edit AutoRun.
`--inspect` reads the inventory without changing console files.
An existing staged file is never overwritten automatically.

## How it works

The payload checks firmware, the GoldHEN process-info command, and ShellCore's
identity. It uses SDK command 6 to load the keeper, then exits. The keeper lives
in ShellCore, stages the hook inside each new ShellUI sandbox, and loads it there.
It watches the ShellUI PID, process start seconds/microseconds and opaque
kernel process cookie (redacted on this tested firmware), including when
sandbox folder names repeat. A build-specific record must match that process
before the keeper reports readiness. A module handle alone means **loading**.

The hook locates the native five-argument `PowerManager.RequestStateChange`
method through existing module metadata, without calling Mono or loading a system
dependency. It scans module handles 0–65535, matches the exact image fingerprint,
and verifies a 256-byte instruction digest plus the original 14-byte prologue.
Shutdown state 2 becomes suspend state 3 with EAP boot flag 1; other requests pass
through. Rejected suspend requests never fall back to shutdown.

ShellUI lifetime is its start interval relative to ShellCore from the same process
snapshot. This cancels shared wall-clock changes across sleep. The keeper refreshes
Core's current identity, tracks replacement processes even when sandbox paths repeat,
and installs one replacement hook per lifetime. No PC service is required.

## Status and duplicate protection

`/data/GoldHEN/hen_rest_status.txt` distinguishes `starting`, `waiting`, `loading`,
`ready`, and `failed`. The hook writes a process/build marker under
`/Temp/hen_rest_031r3/status.txt` inside its own sandbox. Klog includes
`KEEPER_STARTED`, `NEW_PROCESS`, `HOOK_LOAD`, `HOOK_READY`, `HOOK_CONFIRMED`, and
explicit failures. The hook refreshes a sequence every two seconds; the keeper
requires advancement before reporting ready and monitors freshness afterward.
A stale marker cannot confirm success. Enable GoldHEN TTY Redirect when collecting plugin output.

Descriptor-owned locks serialize starter calls and allow one keeper worker.
Lock files are not deleted: the kernel releases their locks on process exit.
A receipt prevents repeating an ambiguous load in the same ShellCore lifetime.
After a hook load returns a valid handle, the keeper waits up to 60 seconds for
readiness and never reinjects that live process. Failure is reported explicitly.

## Build and checks

These steps are optional for installing the release. Use the prebuilt ZIP for
installation. From the repository root, obtain the dependencies at these exact
revisions (Git and WSL Ubuntu must be installed):

```powershell
git clone https://github.com/GoldHEN/GoldHEN_Plugins_SDK.git GoldHEN_Plugins_SDK
git -C GoldHEN_Plugins_SDK checkout bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7
New-Item -ItemType Directory -Force .toolchains
git clone https://github.com/Scene-Collective/ps4-payload-sdk.git .toolchains/ps4-payload-sdk
git -C .toolchains/ps4-payload-sdk checkout b7326416c23ce14639e9180a24093bdc5eedb579
```

Download [OpenOrbis PS4 Toolchain v0.5.4](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/releases/tag/v0.5.4)
and extract its `PS4Toolchain` directory to `.toolchains/OpenOrbis/PS4Toolchain`.
Check that `bin/linux/create-fself`, `include`, `lib`, and `link.x` exist there.
In Ubuntu, install `clang-18 lld-18 gcc binutils make python3` using its package
manager. The distribution must provide the version-18 LLVM executables. Run the
PowerShell build below from the repository root. A rebuilt file is a new artifact
and is not the console-tested release binary.

Windows uses WSL Ubuntu. Dependencies: clang-18, lld-18, GCC, binutils, make,
Python 3; OpenOrbis PS4Toolchain v0.5.4 at `.toolchains/OpenOrbis/PS4Toolchain`;
GoldHEN Plugin SDK at `GoldHEN_Plugins_SDK`, commit
`bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7`; PS4 Payload SDK at
`.toolchains/ps4-payload-sdk`, commit
`b7326416c23ce14639e9180a24093bdc5eedb579`.

```powershell
./keeper/build.ps1
python keeper/inspect_artifacts.py
python -m unittest discover -s keeper/tests -v
wsl -d Ubuntu -- gcc -std=c11 -Wall -Wextra -Werror keeper/tests/process_records_test.c -o keeper/build/process_records_test
wsl -d Ubuntu -- keeper/build/process_records_test
```

All C files, including linked SDK code, require `-mstackrealign`. The flat
payload uses the pinned SDK's startup assembly and a linker script with explicit
BSS bounds. Every execution resets SDK pointers before resolving them.
The manifest records SHA-256 hashes. Keep the two packaged `.oelf` files for
FTP readback: GoldHEN may return expanded ELF instead of the uploaded SELF;
the deployer compares all program headers and segment bytes.

## Removal and limitations

Remove the payload from AutoRun first. Use a fresh session without HEN Rest
before removing its files. `deploy.py PS4_IP --restore BACKUP --fresh-session`
restores a matching staging backup and refuses files changed since deployment.
It does not force-unload resident code. Console backups and raw logs stay local.

This experimental preview targets one firmware and the reported GoldHEN build.
Captured checks confirmed activation, shutdown redirection, wake restoration, and
a subsequent redirect by the restored hook. The owner reported further repeated
cycles without issues. Fresh-load AutoRun on this build, long-term stability,
broader firmware support and physical-button/remote shutdown coverage are unverified.
See [observed results](../docs/VERIFICATION.md). Host checks and FTP verification
alone do not establish runtime compatibility.

Project source is GPL-3.0. Dependency notices must accompany distributed builds.
