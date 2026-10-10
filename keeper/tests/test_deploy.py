import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import deploy

class DeploymentTests(unittest.TestCase):
    def test_preserves_other_entries_and_comments(self):
        original = (b"\xef\xbb\xbf[default]\r\n/other.prx\r\n[CUSA18723]\r\n"
                    b"; /data/GoldHEN/plugins/hen_rest_loader.prx\r\n"
                    b"/data/GoldHEN/plugins/hen_rest_start_loader.prx=true\r\n"
                    b"/data/GoldHEN/plugins/button_swap.prx=false\r\n[OTHER]\r\nlast")
        expected = original.replace(b"/data/GoldHEN/plugins/hen_rest_start_loader.prx=true\r\n", b"")
        self.assertEqual(deploy.remove_our_registrations(original), expected)
        self.assertEqual(deploy.remove_our_registrations(expected), expected)

    def test_no_config_no_new_registration(self):
        self.assertIsNone(deploy.remove_our_registrations(None))
        self.assertEqual(deploy.remove_our_registrations(b"[default]\nabc"), b"[default]\nabc")

    def test_only_exact_owned_names(self):
        original = b"[CUSA18723]\n/data/GoldHEN/plugins/hen_rest_loader.prx\n/other/hen_rest_loader.prx\n"
        self.assertEqual(deploy.remove_our_registrations(original), b"[CUSA18723]\n/other/hen_rest_loader.prx\n")

    def test_manifest_and_elf_readback(self):
        artifacts = deploy.load_artifacts()
        for name, path in deploy.ARTIFACT_PATHS.items():
            raw = artifacts[path]
            self.assertEqual(deploy.verify_uploaded(path, raw, raw), "raw_bytes")
            if name.endswith(".prx"):
                expanded = (deploy.ROOT / "build" / (Path(name).stem + ".oelf")).read_bytes()
                self.assertEqual(deploy.verify_uploaded(path, raw, expanded), "expanded_elf_all_program_headers_and_segments")
                changed = bytearray(expanded)
                changed[24] ^= 1
                with self.assertRaises(ValueError):
                    deploy.verify_uploaded(path, raw, changed)
        with self.assertRaises(ValueError):
            deploy.verify_uploaded("/data/payloads/hen_rest_start.bin", b"x", b"y")

if __name__ == "__main__":
    unittest.main()
