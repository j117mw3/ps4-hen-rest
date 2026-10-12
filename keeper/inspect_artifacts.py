"""Inspect Orbis module symbols, entry points, and SELF headers after packaging."""
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent


def inspect(name):
    elf = (ROOT / "build" / (name + ".oelf")).read_bytes()
    prx = (ROOT / "dist" / (name + ".prx")).read_bytes()
    if elf[:4] != b"\x7fELF" or elf[4:6] != b"\x02\x01":
        raise ValueError("Expected little-endian ELF64")
    if struct.unpack_from("<HH", elf, 16) != (0xFE18, 62):
        raise ValueError("Expected Orbis x86-64 shared module")
    if prx[:4] != bytes.fromhex("4f153d1d"):
        raise ValueError("Expected packaged SELF header")
    phoff = struct.unpack_from("<Q", elf, 32)[0]
    phentsize, phnum = struct.unpack_from("<HH", elf, 54)
    segments = [struct.unpack_from("<IIQQQQQQ", elf, phoff + i * phentsize) for i in range(phnum)]
    dynamic = next(p for p in segments if p[0] == 2)
    dynlib = next(p for p in segments if p[0] == 0x61000000)
    tags = {}
    for offset in range(dynamic[2], dynamic[2] + dynamic[5], 16):
        tag, value = struct.unpack_from("<QQ", elf, offset)
        tags[tag] = value
        if tag == 0:
            break
    strings = dynlib[2] + tags[0x61000035]
    symbols = dynlib[2] + tags[0x61000039]
    exports, imports = [], []
    for offset in range(symbols, symbols + tags[0x6100003F], 24):
        st_name, st_info, st_other, st_shndx, value, size = struct.unpack_from("<IBBHQQ", elf, offset)
        if not st_name:
            continue
        start = strings + st_name
        symbol = elf[start:elf.index(b"\0", start)].decode("ascii")
        (exports if st_shndx else imports).append({"name": symbol, "value": hex(value), "size": size})
    # Orbis encodes exports as NIDs. Match their addresses/sizes to the original
    # linker symbol table so the report can show the names used by sceKernelDlsym.
    original = (ROOT / "build" / (name + ".elf")).read_bytes()
    shoff = struct.unpack_from("<Q", original, 40)[0]
    shentsize, shnum = struct.unpack_from("<HH", original, 58)
    sections = [struct.unpack_from("<IIQQQQIIQQ", original, shoff + i * shentsize) for i in range(shnum)]
    symtab = next(section for section in sections if section[1] == 2)
    strtab = sections[symtab[6]][4]
    names = {}
    for offset in range(symtab[4], symtab[4] + symtab[5], 24):
        n, info, other, section, value, size = struct.unpack_from("<IBBHQQ", original, offset)
        if not n or not section or info >> 4 != 1:
            continue
        start = strtab + n
        symbol = original[start:original.index(b"\0", start)].decode("ascii")
        names.setdefault((hex(value), size), []).append(symbol)
    for entry in exports:
        entry["linker_names"] = names.get((entry["value"], entry["size"]), [])
    required = {"plugin_load", "plugin_unload", "g_pluginName", "g_pluginVersion"} if name.endswith("loader") else {"module_start", "module_stop"}
    available = {name for entry in exports for name in entry["linker_names"]}
    if not required <= available:
        raise ValueError(f"Missing plugin exports: {required - available}; exports={exports}")
    if 12 not in tags or 13 not in tags:
        raise ValueError("Missing INIT/FINI entry points")
    return {"name": name, "self_bytes": len(prx), "type": "Orbis PRX", "init": hex(tags[12]),
            "fini": hex(tags[13]), "exports": exports, "imports": imports}


if __name__ == "__main__":
    report = [inspect(name) for name in ("hen_rest_hook", "hen_rest_keeper")]
    payload = (ROOT / "build" / "hen_rest_start.elf").read_bytes()
    binary = (ROOT / "dist" / "hen_rest_start.bin").read_bytes()
    if payload[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<HH", payload, 16) != (3, 62):
        raise ValueError("Payload must be linked as x86-64 PIE; static EXEC can relax PIC into absolute addresses")
    shoff = struct.unpack_from("<Q", payload, 40)[0]
    shsize, shcount = struct.unpack_from("<HH", payload, 58)
    sections = [struct.unpack_from("<IIQQQQIIQQ", payload, shoff + i * shsize) for i in range(shcount)]
    if any(s[1] in (4, 9) and s[5] for s in sections):
        raise ValueError("Flat payload cannot require runtime relocations")
    for section in sections:
        if section[1] in (2, 11):
            for offset in range(section[4], section[4] + section[5], 24):
                name, info, other, index, value, size = struct.unpack_from("<IBBHQQ", payload, offset)
                if name and index == 0 and info >> 4 != 2:
                    raise ValueError("Payload contains an unresolved symbol")
        if section[1] == 6:
            if any(struct.unpack_from("<Q", payload, off)[0] == 1 for off in range(section[4], section[4] + section[5], 16)):
                raise ValueError("Flat payload has a dynamic dependency")
    entry = struct.unpack_from("<Q", payload, 24)[0]
    if entry != 0 or binary[:1] != b"\xe9":
        raise ValueError("Flat payload must enter at offset zero with SDK crt0 jump")
    phoff = struct.unpack_from("<Q", payload, 32)[0]
    entsize, count = struct.unpack_from("<HH", payload, 54)
    headers = [struct.unpack_from("<IIQQQQQQ", payload, phoff + i * entsize) for i in range(count)]
    if any(h[0] in (2, 3) for h in headers):
        raise ValueError("Payload must have no dynamic linker or dynamic segment")
    for h in headers:
        if h[0] == 1 and h[5] and binary[h[3]:h[3] + h[5]] != payload[h[2]:h[2] + h[5]]:
            raise ValueError("Flat payload differs from linked load segments")
    diagnostic = sys.argv[1:] == ["--identity-diagnostic"]
    if sys.argv[1:] and not diagnostic:
        raise ValueError("Unknown inspection option")
    required = (b"identity diagnostic recorded; nothing loaded", b"IDENTITY_RECORD", b"unsupported firmware") if diagnostic else (b"hen_rest_keeper_031r3.prx", b"unsupported firmware", b"no new worker")
    for value in required:
        if value not in binary:
            raise ValueError("Payload missing required paths/guard messages")
    if diagnostic and (b"KEEPER_LOAD" in binary or b"hen_rest_keeper_031r3.prx" in binary):
        raise ValueError("Identity diagnostic must contain no keeper loading path")
    report.append({"name": "hen_rest_start", "type": "flat payload", "bytes": len(binary),
                   "entry": entry, "memory_extent": max(h[3] + h[6] for h in headers if h[0] == 1),
                   "dynamic_dependencies": False, "identity_diagnostic_only": diagnostic})
    (ROOT / "dist" / "artifact-inspection.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    for item in report:
        print(f"{item['name']}: {item['type']} validated")
