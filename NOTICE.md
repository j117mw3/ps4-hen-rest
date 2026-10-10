# Dependency notices

HEN Rest's original project code is licensed under GPL-3.0.

## GoldHEN Plugin SDK

https://github.com/GoldHEN/GoldHEN_Plugins_SDK
Pinned commit: bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7.
MIT License, copyright (c) 2022 GoldHEN. Its full notice is in
licenses/GoldHEN-Plugin-SDK-MIT.txt. PRX builds link its SDK wrappers and runtime.
Credits from the SDK: SiSTRo, ctn123, OSM, jocover, bucanero, OpenOrbis Team,
nik. GoldHEN itself is installed separately and is not included.

## PS4 Payload SDK

https://github.com/Scene-Collective/ps4-payload-sdk
Pinned commit: b7326416c23ce14639e9180a24093bdc5eedb579.
The starter uses this SDK's libPS4 runtime and crt0 startup assembly.
Development chain credited by upstream:
CTurt → IDC → xvortex → stooged → Scene-Collective.
The pinned repository supplies no top-level LICENSE/COPYING file. This notice
records that fact and does not assign a new license to upstream code.
Upstream source is obtained separately at its pinned revision. The project's
payload.ld is adapted from its libPS4/linker.x with BSS initialization bounds.

## OpenOrbis PS4 Toolchain

https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain
Build dependency: v0.5.4. The upstream repository's GPL-3.0 license text is
preserved in licenses/OpenOrbis-GPL-3.0.txt. It supplies PRX packaging,
headers and link libraries; the toolchain distribution is installed separately.
Its own bundled components may have additional licenses. HEN Rest does not
include Sony firmware files or proprietary console modules.

## Host build tools

Clang/LLVM and lld 18: Apache-2.0 with LLVM exceptions.
GCC, GNU binutils and make: their upstream GNU licensing.
These build tools are not bundled in the installation ZIP.
Python is used for checks and deployment and is not bundled.
