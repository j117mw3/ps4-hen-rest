"""Linux/WSL: verify the real flat starter at multiple bases with SDK calls stubbed."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
exe = root / "build/payload_rebase_test"
subprocess.run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                str(root / "tests/payload_rebase_test.c"), "-o", str(exe)], check=True)
symbols = {}
for line in subprocess.check_output(["nm", "-n", str(root / "build/hen_rest_start.elf")], text=True).splitlines():
    fields = line.split()
    if len(fields) == 3:
        symbols[fields[2]] = fields[0]
subprocess.run([str(exe), str(root / "dist/hen_rest_start.bin"),
                *(symbols[key] for key in ("initKernel", "initLibc", "__payload_bss_start", "__payload_bss_end"))], check=True)
