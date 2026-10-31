"""Synthetic tests for the local diagnostic bundle tool; no Unreal, controller, or network access is used."""
import contextlib
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile


SCRIPT = Path(__file__).resolve().parents[1] / "Scripts" / "make-diagnostic-bundle.py"
SPEC = importlib.util.spec_from_file_location("diagnostic_bundle", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class DiagnosticBundleTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="diagnostic-bundle-test-")
        self.root = Path(self.temporary.name) / "Saved"
        self.root.mkdir()
        self.output_directory = Path(self.temporary.name) / "output"
        self.output_directory.mkdir()

    def tearDown(self):
        self.temporary.cleanup()

    def write(self, relative_path, content):
        target = self.root / relative_path
        target.parent.mkdir(parents=True, exist_ok=True)
        if isinstance(content, bytes):
            target.write_bytes(content)
        else:
            target.write_text(content, encoding="utf-8")
        return target

    def test_default_dry_run_lists_only_supported_records_without_writing(self):
        self.write("FlightLogs/flight.jsonl", '{"event":"stop"}\n')
        self.write("Reports/state-fleet.csv", "id,speed_mps\nIW-01,0\n")
        self.write("TrainingResults/AI/result.json", '{"schema":2}\n')
        self.write("Diagnostics/radio-20261031.json", '{"connected":false}\n')
        self.write("Other/readme.txt", "not an explicit diagnostic payload\n")
        stream = io.StringIO()
        with contextlib.redirect_stdout(stream):
            result = MODULE.main(["--saved-root", str(self.root)])
        self.assertEqual(result, 0)
        rendered = stream.getvalue()
        self.assertIn("INCLUDE FlightLogs/flight.jsonl", rendered)
        self.assertIn("INCLUDE Reports/state-fleet.csv", rendered)
        self.assertIn("INCLUDE TrainingResults/AI/result.json", rendered)
        self.assertIn("EXCLUDE Other/readme.txt", rendered)
        self.assertIn("no ZIP is created", rendered)
        self.assertFalse((self.root / "diagnostic-bundle.zip").exists())

    def test_policy_excludes_profiles_credentials_binary_and_oversize_files(self):
        self.write("FlightLogs/normal.jsonl", '{"event":"sample"}\n')
        self.write("Config/RadioProfile.json", '{"private":"calibration"}\n')
        self.write("Config/PilotControls.json", "{}\n")
        self.write("Config/.env", "ACCESS_TOKEN=not-for-export\n")
        self.write("Diagnostics/session-token.json", '{"token":"not-for-export"}\n')
        self.write("FlightLogs/binary.jsonl", b"\x00not text")
        self.write("FlightLogs/large.jsonl", b"x" * (MODULE.MAX_FILE_BYTES + 1))
        plan = MODULE.plan_bundle(self.root, environ={"USERNAME": "SyntheticUser"})
        included = {item.relative_path for item in plan.included}
        excluded = {item.relative_path: item.reason for item in plan.excluded}
        self.assertEqual(included, {"FlightLogs/normal.jsonl"})
        self.assertIn("controller calibration/profile", excluded["Config/RadioProfile.json"])
        self.assertIn("controller calibration/profile", excluded["Config/PilotControls.json"])
        self.assertIn("credential", excluded["Config/.env"])
        self.assertIn("token", excluded["Diagnostics/session-token.json"])
        self.assertIn("binary", excluded["FlightLogs/binary.jsonl"])
        self.assertIn("per-file size", excluded["FlightLogs/large.jsonl"])

    def test_text_redaction_removes_paths_usernames_and_identifiers(self):
        text = (
            "path=C:\\Users\\Alice\\Saved\\FlightLogs\\x.jsonl\n"
            "user=Alice email=pilot@example.test\n"
            "guid={01234567-89ab-cdef-0123-456789abcdef}\n"
            "hid=HID\\VID_1209&PID_4F54\\SERIAL-778\n"
            "ip=192.168.10.44\n"
        )
        result = MODULE.redact_text(text, {"USERNAME": "Alice", "USER": "Alice"})
        self.assertNotIn("C:\\Users\\Alice", result.text)
        self.assertNotIn("pilot@example.test", result.text)
        self.assertNotIn("01234567-89ab-cdef-0123-456789abcdef", result.text)
        self.assertNotIn("SERIAL-778", result.text)
        self.assertNotIn("192.168.10.44", result.text)
        self.assertIn("%USERPROFILE%\\", result.text)
        self.assertEqual(result.counts["windows_user_profile_path"], 1)
        self.assertEqual(result.counts["environment_username"], 1)
        self.assertEqual(result.counts["email_address"], 1)
        self.assertEqual(result.counts["guid_like_device_id"], 1)
        self.assertEqual(result.counts["hid_device_instance"], 1)
        self.assertEqual(result.counts["ipv4_address"], 1)

    def test_confirm_gate_requires_explicit_flag_before_zip_write(self):
        self.write("FlightLogs/flight.jsonl", '{"event":"stop"}\n')
        output = self.output_directory / "bundle.zip"
        dry_stream = io.StringIO()
        with contextlib.redirect_stdout(dry_stream):
            dry_result = MODULE.main(["--saved-root", str(self.root), "--output", str(output)])
        self.assertEqual(dry_result, 0)
        self.assertFalse(output.exists())
        confirm_stream = io.StringIO()
        with contextlib.redirect_stdout(confirm_stream):
            confirm_result = MODULE.main([
                "--saved-root", str(self.root), "--output", str(output), "--confirm",
            ])
        self.assertEqual(confirm_result, 0)
        self.assertTrue(output.is_file())
        self.assertIn("CREATED", confirm_stream.getvalue())
        with zipfile.ZipFile(output) as archive:
            self.assertEqual(archive.namelist(), ["manifest.json", "FlightLogs/flight.jsonl"])

    def test_manifest_hashes_post_redaction_memo_and_build_fingerprint(self):
        self.write("FlightLogs/flight.jsonl", "tester@example.test\n")
        self.write(
            "Diagnostics/build-fingerprint.json",
            '{"version":"0.7.15-test","source_sha256":"abc123","ignored":{"path":"C:\\\\Users\\\\Alice"}}\n',
        )
        plan = MODULE.plan_bundle(self.root, memo="Contact pilot@example.test", environ={"USERNAME": "Alice"})
        output = self.output_directory / "manifest.zip"
        MODULE.write_bundle(plan, output)
        with zipfile.ZipFile(output) as archive:
            manifest = json.loads(archive.read("manifest.json").decode("utf-8"))
            flight_data = archive.read("FlightLogs/flight.jsonl")
            flight_entry = next(item for item in manifest["files"] if item["path"] == "FlightLogs/flight.jsonl")
            self.assertEqual(flight_entry["sha256"], hashlib.sha256(flight_data).hexdigest())
            self.assertIn("[REDACTED_EMAIL]", flight_data.decode("utf-8"))
            self.assertEqual(flight_entry["redaction_counts"]["email_address"], 1)
            self.assertEqual(manifest["memo"], "Contact [REDACTED_EMAIL]")
            self.assertEqual(manifest["memo_redaction_counts"]["email_address"], 1)
            self.assertEqual(manifest["build_fingerprint"]["version"], "0.7.15-test")
            self.assertEqual(manifest["build_fingerprint"]["source_sha256"], "abc123")
            self.assertEqual(manifest["network_access"], "not performed")

    def test_symlink_unicode_and_path_traversal_safety(self):
        self.write("FlightLogs/한국어-비행.jsonl", '{"event":"stop"}\n')
        outside = Path(self.temporary.name) / "outside.jsonl"
        outside.write_text('{"secret":"outside"}\n', encoding="utf-8")
        link = self.root / "FlightLogs" / "escape.jsonl"
        try:
            link.symlink_to(outside)
        except OSError as error:
            self.fail("sandbox must permit synthetic symlink safety fixture: " + str(error))
        plan = MODULE.plan_bundle(self.root)
        included = {item.relative_path for item in plan.included}
        excluded = {item.relative_path: item.reason for item in plan.excluded}
        self.assertIn("FlightLogs/한국어-비행.jsonl", included)
        self.assertIn("symbolic link", excluded["FlightLogs/escape.jsonl"])
        with self.assertRaises(MODULE.BundleError):
            MODULE.write_bundle(plan, self.root / "unsafe-inside-saved.zip")

    def test_repeated_bundle_has_deterministic_order_timestamps_and_bytes(self):
        self.write("Reports/z-fleet.csv", "id\nIW-01\n")
        self.write("Diagnostics/a.json", '{"diagnostic":true}\n')
        self.write("FlightLogs/k.jsonl", '{"event":"start"}\n')
        first = self.output_directory / "first.zip"
        second = self.output_directory / "second.zip"
        MODULE.write_bundle(MODULE.plan_bundle(self.root), first)
        MODULE.write_bundle(MODULE.plan_bundle(self.root), second)
        self.assertEqual(first.read_bytes(), second.read_bytes())
        with zipfile.ZipFile(first) as archive:
            names = archive.namelist()
            self.assertEqual(names, [
                "manifest.json", "Diagnostics/a.json", "FlightLogs/k.jsonl", "Reports/z-fleet.csv",
            ])
            self.assertTrue(all(info.date_time == MODULE.ZIP_TIMESTAMP for info in archive.infolist()))

    def test_output_inside_saved_root_is_rejected_without_creating_file(self):
        self.write("FlightLogs/flight.jsonl", '{"event":"stop"}\n')
        unsafe_output = self.root / "Diagnostics" / "bundle.zip"
        stream = io.StringIO()
        error_stream = io.StringIO()
        with contextlib.redirect_stdout(stream), contextlib.redirect_stderr(error_stream):
            result = MODULE.main([
                "--saved-root", str(self.root), "--output", str(unsafe_output), "--confirm",
            ])
        self.assertEqual(result, 2)
        self.assertFalse(unsafe_output.exists())
        self.assertIn("outside the Saved root", error_stream.getvalue())


if __name__ == "__main__":
    print("Scope: synthetic local diagnostic-bundle tests only; no Unreal build, runtime, controller, network, or personal Saved data.")
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(DiagnosticBundleTests)
    result = unittest.TextTestRunner(verbosity=1).run(suite)
    failures = len(result.failures) + len(result.errors)
    print(str(result.testsRun) + " checks, " + str(failures) + " failures")
    raise SystemExit(1 if failures else 0)
