# HEN Rest — PS4 Shutdown-to-Rest Mode Plugin

v0.3.0 Experimental Preview: one-shot GoldHEN LaunchPad payload, ShellCore keeper, and
ShellUI hook. No game needs to be opened and no game registration is installed.
The current target is **PS4 firmware 13.520.001** (`0x13520001`).

## Installation and activation

1. Upload `dist/hen_rest_start.bin` to `/data/payloads/hen_rest_start.bin`.
2. Upload `dist/hen_rest_keeper.prx` as
   `/data/GoldHEN/plugins/hen_rest_keeper_030r4.prx` and `dist/hen_rest_hook.prx` as
   `/data/GoldHEN/plugins/hen_rest_hook_030r4.prx`.
3. Remove active HEN Rest game starters from `plugins.ini`. Preserve other plugins.
4. When replacing an older resident build, use **Restart PS4**, reload GoldHEN,
   and stay on the home screen. Do not force-unload a live hook.
5. Run `hen_rest_start.bin` from GoldHEN LaunchPad. Wait for the ready notification
   and a current `ready` status before trying Turn Off PS4.
6. After the manual and duplicate-run checks pass, highlight `hen_rest_start.bin`
   in LaunchPad and press **Square: Add to AutoRun queue**. Open AutoRun Queue
   and confirm exactly one entry for `/data/payloads/hen_rest_start.bin`, checked
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

The hook resolves the native five-argument `PowerManager.RequestStateChange`
method through Mono. After firmware and exact 14-byte instruction guards pass,
shutdown state 2 becomes suspend state 3 with EAP boot flag 1. Other state
requests pass through. A rejected suspend request does not fall back to shutdown.
The keeper observes normal process replacement after Rest Mode and installs a
replacement hook. No PC service is needed by the payload, keeper, or hook.

## Status and duplicate protection

`/data/GoldHEN/hen_rest_status.txt` distinguishes `starting`, `waiting`, `loading`,
`ready`, and `failed`. The hook writes a process/build marker under
`/Temp/hen_rest_030r4/status.txt` inside its own sandbox. Klog includes
`KEEPER_STARTED`, `NEW_PROCESS`, `HOOK_LOAD`, `HOOK_READY`, `HOOK_CONFIRMED`, and
explicit failures. Enable GoldHEN TTY Redirect when collecting plugin output.

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

This is an experimental preview for one firmware and the exact tested GoldHEN
build. The owner confirmed the hook's enabled/listening notification returned
after Rest Mode wake. This is recorded as verified by user report. Fresh-load
AutoRun and a subsequent redirect remain unverified for v0.3.0; see
[observed results](../docs/VERIFICATION.md). Host checks and
FTP verification do not establish runtime compatibility or long-term stability.
Physical-button/remote shutdown coverage and every shutdown path are unverified.
The earlier intermittent wake dialog has no established faulting module or cause.

Project source is GPL-3.0. Dependency notices must accompany distributed builds.
