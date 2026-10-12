"""Stage LaunchPad artifacts, preserving unrelated configuration and old modules."""
import argparse
from datetime import datetime, timezone
from ftplib import FTP
import hashlib
import io
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parent
CONFIG = "/data/GoldHEN/plugins.ini"
ARTIFACT_PATHS = {
    "hen_rest_start.bin": "/data/payloads/hen_rest_start_031r3.bin",
    "hen_rest_keeper.prx": "/data/GoldHEN/plugins/hen_rest_keeper_031r3.prx",
    "hen_rest_hook.prx": "/data/GoldHEN/plugins/hen_rest_hook_031r3.prx",
}
OUR_STARTERS = {"/data/GoldHEN/plugins/hen_rest_start_loader.prx",
                "/data/GoldHEN/plugins/hen_rest_loader.prx"}


def elf_contents(blob):
    if len(blob) < 64 or blob[:6] != b"\x7fELF\x02\x01":
        raise ValueError("Not a little-endian ELF64 module")
    phoff = struct.unpack_from("<Q", blob, 32)[0]
    entsize, count = struct.unpack_from("<HH", blob, 54)
    if entsize != 56 or count > 64 or phoff + count * entsize > len(blob):
        raise ValueError("Unexpected program header table")
    headers = [struct.unpack_from("<IIQQQQQQ", blob, phoff + i * entsize) for i in range(count)]
    if any(h[2] + h[5] > len(blob) for h in headers):
        raise ValueError("Truncated ELF segment")
    return blob[:32], headers, [blob[h[2]:h[2] + h[5]] for h in headers]


def verify_uploaded(path, expected, downloaded):
    if downloaded == expected:
        return "raw_bytes"
    if path.endswith(".prx") and downloaded is not None:
        name = next((n for n, remote in ARTIFACT_PATHS.items() if remote == path), None)
        if name:
            oelf = ROOT / "build" / (Path(name).stem + ".oelf")
            if elf_contents(downloaded) == elf_contents(oelf.read_bytes()):
                return "expanded_elf_all_program_headers_and_segments"
    raise ValueError(f"Read-back contents differ: {path}")


def listing(ftp, parent):
    return {item.rsplit("/", 1)[-1] for item in ftp.nlst(parent)}


def retrieve(ftp, path):
    parent, name = path.rsplit("/", 1)
    if name not in listing(ftp, parent):
        return None
    buffer = io.BytesIO()
    ftp.retrbinary("RETR " + path, buffer.write)
    return buffer.getvalue()


def remove_our_registrations(original):
    if original is None:
        return None
    bom = b"\xef\xbb\xbf" if original.startswith(b"\xef\xbb\xbf") else b""
    lines = original.decode("utf-8-sig").splitlines(keepends=True)
    kept = [line for line in lines if line.strip().split("=", 1)[0].strip() not in OUR_STARTERS]
    return bom + "".join(kept).encode("utf-8")


def load_artifacts():
    manifest = json.loads((ROOT / "dist" / "manifest.json").read_text(encoding="utf-8-sig"))
    if manifest["probe_version"] != "0.3.1" or {i["name"] for i in manifest["artifacts"]} != ARTIFACT_PATHS.keys():
        raise ValueError("Expected complete v0.3.1 manifest")
    uploads = {}
    for item in manifest["artifacts"]:
        data = (ROOT / "dist" / item["name"]).read_bytes()
        if len(data) != item["bytes"] or hashlib.sha256(data).hexdigest() != item["sha256"]:
            raise ValueError(f"Artifact does not match manifest: {item['name']}")
        if item["name"].endswith(".prx"):
            elf_contents((ROOT / "build" / (Path(item["name"]).stem + ".oelf")).read_bytes())
        uploads[ARTIFACT_PATHS[item["name"]]] = data
    return uploads


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("host")
    parser.add_argument("--port", type=int, default=2121)
    parser.add_argument("--inspect", action="store_true", help="Read-only inventory")
    parser.add_argument("--restore", type=Path, help="Restore this staging operation")
    parser.add_argument("--fresh-session", action="store_true", help="Confirm no v0.3.1 modules are resident")
    args = parser.parse_args()
    if args.restore and not args.fresh_session:
        parser.error("Restore requires a fresh session; never remove resident backing files")
    uploads = load_artifacts() if not args.restore and not args.inspect else {}
    with FTP() as ftp:
        ftp.connect(args.host, args.port, timeout=10)
        ftp.login()
        if args.inspect:
            config = retrieve(ftp, CONFIG)
            print("PLUGINS_INI:", config.decode("utf-8-sig") if config else "absent/empty")
            for folder in ("/data", "/data/GoldHEN/plugins"):
                print(folder, sorted(listing(ftp, folder)))
            if "payloads" in listing(ftp, "/data"):
                print("/data/payloads", sorted(listing(ftp, "/data/payloads")))
            return
        if args.restore:
            record_path = args.restore / "deployment.json"
            record = json.loads(record_path.read_text(encoding="utf-8"))
            if record["host"] != args.host or record.get("version") != "0.3.1":
                raise ValueError("Wrong console or backup version")
            for path in reversed(record["changed"]):
                if path in record.get("restored", []):
                    continue
                current = retrieve(ftp, path)
                expected = record["readback_sha256"].get(path)
                if expected is None or current is None or hashlib.sha256(current).hexdigest() != expected:
                    raise ValueError(f"{path} changed or incomplete; inspect and merge manually")
                name = record["originals"][path]
                if name:
                    data = (args.restore / name).read_bytes()
                    ftp.storbinary("STOR " + path, io.BytesIO(data))
                    if retrieve(ftp, path) != data:
                        raise ValueError("Configuration restore readback failed")
                else:
                    ftp.delete(path)
                record.setdefault("restored", []).append(path)
                record_path.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
            print("Staging restored. Manage AutoRun settings in GoldHEN.")
            return
        if "payloads" not in listing(ftp, "/data"):
            ftp.mkd("/data/payloads")
        originals = {path: retrieve(ftp, path) for path in [*uploads, CONFIG]}
        for path in uploads:
            if originals[path] is not None:
                raise ValueError(f"Refusing to overwrite existing staged file: {path}")
        clean_config = remove_our_registrations(originals[CONFIG])
        if clean_config != originals[CONFIG]:
            uploads[CONFIG] = clean_config
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        backup = ROOT / "deployment" / stamp
        backup.mkdir(parents=True)
        record = {"host": args.host, "version": "0.3.1", "created_utc": stamp, "originals": {},
                  "installed_sha256": {}, "readback_sha256": {}, "changed": []}
        for index, (path, data) in enumerate(originals.items()):
            name = f"original-{index}.bin" if data is not None else None
            if name:
                (backup / name).write_bytes(data)
            record["originals"][path] = name
        record_path = backup / "deployment.json"

        def save():
            record_path.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")

        save()
        try:
            for path, data in uploads.items():
                record["changed"].append(path); save()
                ftp.storbinary("STOR " + path, io.BytesIO(data))
                downloaded = retrieve(ftp, path)
                mode = verify_uploaded(path, data, downloaded)
                record["installed_sha256"][path] = hashlib.sha256(data).hexdigest()
                record["readback_sha256"][path] = hashlib.sha256(downloaded).hexdigest()
                save()
                print(f"UPLOADED_AND_VERIFIED {path} bytes={len(data)} mode={mode}", flush=True)
        except Exception:
            for path in reversed(record["changed"]):
                original = originals[path]
                if original is None:
                    if retrieve(ftp, path) is not None:
                        ftp.delete(path)
                else:
                    ftp.storbinary("STOR " + path, io.BytesIO(original))
                    if retrieve(ftp, path) != original:
                        raise RuntimeError(f"Rollback verification failed: {path}")
            record["rolled_back"] = True; save()
            raise
        print(f"Backup: {backup}")
        print("Staged only. Use a fresh GoldHEN session, then LaunchPad; no game required.")


if __name__ == "__main__":
    main()
