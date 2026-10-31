#!/usr/bin/env python3
"""Create a reviewable, local-only Imperial Wings diagnostic ZIP.

The default mode is a dry run: it never creates a ZIP and prints every file that
would be included or excluded.  The utility deliberately has no network code.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import math
import os
from pathlib import Path, PurePosixPath
import re
import sys
import tempfile
from typing import Dict, Iterable, List, Mapping, Optional, Sequence, Tuple
import zipfile


TOOL_VERSION = "1.0.0"
MAX_FILE_BYTES = 5 * 1024 * 1024
MAX_TOTAL_BYTES = 32 * 1024 * 1024
ZIP_TIMESTAMP = (1980, 1, 1, 0, 0, 0)
TEXT_EXTENSIONS = frozenset({".csv", ".json", ".jsonl", ".log", ".txt"})
BUILD_FINGERPRINT_PATHS = frozenset({
    "build-fingerprint.json",
    "build-info.json",
    "diagnostics/build-fingerprint.json",
    "diagnostics/build-info.json",
    "diagnostics/build.json",
})
BUILD_FINGERPRINT_KEYS = (
    "version",
    "build_version",
    "source_sha256",
    "source_hash",
    "input_sha256",
    "build_utc",
    "build_utc_timestamp",
    "commit",
    "git_commit",
    "build_fingerprint",
)
SENSITIVE_FILE_RE = re.compile(
    r"(?:^|[_\-.])(credential|credentials|secret|secrets|token|tokens|"
    r"api[_-]?key|apikey|private[_-]?key|key)(?:[_\-.]|$)",
    re.IGNORECASE,
)
CONTROLLER_PROFILE_RE = re.compile(
    r"^(?:radioprofile.*|pilotcontrols.*|pilotresponse.*|.*(?:calibration|"
    r"controller[-_ ]?profile|joystick[-_ ]?profile).*)\.(?:json|ini|cfg|txt)$",
    re.IGNORECASE,
)
WINDOWS_PROFILE_PATH_RE = re.compile(
    r"C:(?P<separator>\\+)Users(?P=separator)(?P<username>[^\\/\r\n]+)(?P=separator)",
    re.IGNORECASE,
)
EMAIL_RE = re.compile(r"\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}\b", re.IGNORECASE)
GUID_RE = re.compile(
    r"\b\{?[0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12}\}?\b",
    re.IGNORECASE,
)
HID_DEVICE_RE = re.compile(
    r"\bHID\\+VID_[0-9A-F]{4}&PID_[0-9A-F]{4}(?:&[^\\\s\"']*)?\\+[^\\\s\"'\r\n]+",
    re.IGNORECASE,
)
IPV4_RE = re.compile(
    r"(?<![\d.])(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)"
    r"(?:\.(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}(?![\d.])"
)


class BundleError(RuntimeError):
    """A safe diagnostic-bundle request could not be fulfilled."""


@dataclass(frozen=True)
class RedactionResult:
    text: str
    counts: Mapping[str, int]

    @property
    def total(self) -> int:
        return sum(self.counts.values())


@dataclass(frozen=True)
class IncludedFile:
    relative_path: str
    source_size_bytes: int
    data: bytes
    reason: str
    redaction_counts: Mapping[str, int]

    @property
    def bundle_size_bytes(self) -> int:
        return len(self.data)

    @property
    def sha256(self) -> str:
        return hashlib.sha256(self.data).hexdigest()


@dataclass(frozen=True)
class ExcludedFile:
    relative_path: str
    size_bytes: Optional[int]
    reason: str


@dataclass(frozen=True)
class BundlePlan:
    saved_root: Path
    included: Tuple[IncludedFile, ...]
    excluded: Tuple[ExcludedFile, ...]
    memo: Optional[RedactionResult]
    build_fingerprint: Optional[Mapping[str, object]]


@dataclass(frozen=True)
class _WalkEntry:
    path: Path
    relative_path: str
    is_symlink: bool
    is_directory: bool


def default_saved_root(environ: Optional[Mapping[str, str]] = None) -> Path:
    """Return the normal packaged-game Saved root without touching the filesystem."""
    values = os.environ if environ is None else environ
    local_app_data = values.get("LOCALAPPDATA")
    if local_app_data:
        return Path(local_app_data) / "ImperialWingsSimulator" / "Saved"
    return Path.home() / "AppData" / "Local" / "ImperialWingsSimulator" / "Saved"


def _is_relative_to(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
        return True
    except ValueError:
        return False


def _safe_relative(path: Path, root: Path) -> str:
    try:
        relative = path.relative_to(root)
    except ValueError as error:
        raise BundleError("A scanned path is outside the Saved root.") from error
    text = relative.as_posix()
    if not text or text.startswith("/") or "\\" in text:
        raise BundleError("An unsafe ZIP path was encountered.")
    parts = PurePosixPath(text).parts
    if any(part in ("", ".", "..") for part in parts):
        raise BundleError("A traversal-like ZIP path was encountered.")
    return text


def _walk_entries(root: Path) -> Tuple[_WalkEntry, ...]:
    """List direct filesystem entries without following links outside Saved."""
    entries: List[_WalkEntry] = []
    for directory_text, directory_names, file_names in os.walk(str(root), followlinks=False):
        directory = Path(directory_text)
        directory_names.sort(key=str.casefold)
        file_names.sort(key=str.casefold)
        for name in list(directory_names):
            candidate = directory / name
            if candidate.is_symlink():
                entries.append(_WalkEntry(candidate, _safe_relative(candidate, root), True, True))
                directory_names.remove(name)
        for name in file_names:
            candidate = directory / name
            entries.append(_WalkEntry(
                candidate,
                _safe_relative(candidate, root),
                candidate.is_symlink(),
                False,
            ))
    return tuple(sorted(entries, key=lambda entry: entry.relative_path.casefold()))


def _controller_or_secret_reason(relative_path: str) -> Optional[str]:
    name = PurePosixPath(relative_path).name
    stem = Path(name).stem
    suffix = Path(name).suffix.casefold()
    lowered_path = relative_path.casefold()
    if CONTROLLER_PROFILE_RE.fullmatch(name):
        return "controller calibration/profile file"
    if name.casefold() in {".env", ".netrc", "id_rsa", "id_ed25519"} or name.casefold().startswith(".env."):
        return "credential or environment file"
    if suffix in {".pem", ".p12", ".pfx", ".key"}:
        return "private key or credential file"
    if SENSITIVE_FILE_RE.search(stem) is not None or "/secrets/" in "/" + lowered_path + "/":
        return "credential, token, or key-like file"
    return None


def _inclusion_reason(relative_path: str) -> Optional[str]:
    parts = PurePosixPath(relative_path).parts
    suffix = Path(relative_path).suffix.casefold()
    lowered = tuple(part.casefold() for part in parts)
    if len(lowered) >= 2 and lowered[0] == "flightlogs" and suffix == ".jsonl":
        return "F6 flight recording JSONL"
    if len(lowered) >= 2 and lowered[0] == "reports" and suffix == ".csv":
        return "F5 telemetry CSV"
    if len(lowered) >= 3 and lowered[0] == "trainingresults" and lowered[1] == "ai" and suffix == ".json":
        return "AI training result JSON"
    if len(lowered) >= 2 and lowered[0] == "diagnostics" and suffix in TEXT_EXTENSIONS:
        return "diagnostic text record"
    return None


def _environment_usernames(environ: Mapping[str, str]) -> Tuple[str, ...]:
    values = []
    for key in ("USERNAME", "USER", "LOGNAME"):
        value = environ.get(key, "").strip()
        if len(value) >= 2 and value not in values:
            values.append(value)
    return tuple(sorted(values, key=lambda value: (-len(value), value.casefold())))


def _replace_count(pattern: re.Pattern[str], replacement: str, text: str) -> Tuple[str, int]:
    return pattern.subn(replacement, text)


def redact_text(text: str, environ: Optional[Mapping[str, str]] = None) -> RedactionResult:
    """Redact paths and common personal/device identifiers from UTF-8 text."""
    values = os.environ if environ is None else environ
    counts: Dict[str, int] = {}

    def replace_profile(match: re.Match[str]) -> str:
        counts["windows_user_profile_path"] = counts.get("windows_user_profile_path", 0) + 1
        return "%USERPROFILE%" + match.group("separator")

    text = WINDOWS_PROFILE_PATH_RE.sub(replace_profile, text)
    for username in _environment_usernames(values):
        pattern = re.compile(r"(?<![A-Z0-9_.-])" + re.escape(username) + r"(?![A-Z0-9_.-])", re.IGNORECASE)
        text, count = _replace_count(pattern, "[REDACTED_USERNAME]", text)
        if count:
            counts["environment_username"] = counts.get("environment_username", 0) + count
    replacements = (
        ("email_address", EMAIL_RE, "[REDACTED_EMAIL]"),
        ("guid_like_device_id", GUID_RE, "[REDACTED_GUID]"),
        ("hid_device_instance", HID_DEVICE_RE, "[REDACTED_HID_DEVICE]"),
        ("ipv4_address", IPV4_RE, "[REDACTED_IPV4]"),
    )
    for label, pattern, replacement in replacements:
        text, count = _replace_count(pattern, replacement, text)
        if count:
            counts[label] = counts.get(label, 0) + count
    return RedactionResult(text=text, counts=dict(sorted(counts.items())))


def _inspect_file(entry: _WalkEntry, root: Path, environ: Mapping[str, str]) -> IncludedFile | ExcludedFile:
    if entry.is_symlink:
        return ExcludedFile(entry.relative_path, None, "symbolic link is never followed")
    secret_reason = _controller_or_secret_reason(entry.relative_path)
    if secret_reason is not None:
        return ExcludedFile(entry.relative_path, None, secret_reason)
    inclusion_reason = _inclusion_reason(entry.relative_path)
    if inclusion_reason is None:
        return ExcludedFile(entry.relative_path, None, "not an allowed diagnostic location or text format")
    try:
        resolved_root = root.resolve(strict=True)
        resolved_file = entry.path.resolve(strict=True)
        if not _is_relative_to(resolved_file, resolved_root):
            return ExcludedFile(entry.relative_path, None, "resolved path escapes the Saved root")
        stat = entry.path.stat()
    except OSError as error:
        return ExcludedFile(entry.relative_path, None, "cannot inspect file: " + error.__class__.__name__)
    size = stat.st_size
    if size > MAX_FILE_BYTES:
        return ExcludedFile(entry.relative_path, size, "file exceeds per-file size limit")
    try:
        data = entry.path.read_bytes()
    except OSError as error:
        return ExcludedFile(entry.relative_path, size, "cannot read file: " + error.__class__.__name__)
    if b"\x00" in data:
        return ExcludedFile(entry.relative_path, size, "binary content is excluded")
    try:
        decoded = data.decode("utf-8-sig")
    except UnicodeDecodeError:
        return ExcludedFile(entry.relative_path, size, "non-UTF-8 or binary content is excluded")
    redacted = redact_text(decoded, environ)
    return IncludedFile(
        relative_path=entry.relative_path,
        source_size_bytes=size,
        data=redacted.text.encode("utf-8"),
        reason=inclusion_reason,
        redaction_counts=redacted.counts,
    )


def _build_fingerprint(included: Iterable[IncludedFile]) -> Optional[Mapping[str, object]]:
    for item in included:
        if item.relative_path.casefold() not in BUILD_FINGERPRINT_PATHS:
            continue
        try:
            candidate = json.loads(item.data.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            continue
        if not isinstance(candidate, dict):
            continue
        fingerprint: Dict[str, object] = {}
        for key in BUILD_FINGERPRINT_KEYS:
            value = candidate.get(key)
            if isinstance(value, str) and 0 < len(value) <= 128:
                fingerprint[key] = value
            elif isinstance(value, int) and not isinstance(value, bool):
                fingerprint[key] = value
            elif isinstance(value, float) and math.isfinite(value):
                fingerprint[key] = value
        if fingerprint:
            return dict(sorted(fingerprint.items()))
    return None


def plan_bundle(saved_root: Path | str, memo: Optional[str] = None,
                environ: Optional[Mapping[str, str]] = None) -> BundlePlan:
    """Build a read-only deterministic inclusion/exclusion plan for a Saved tree."""
    root = Path(saved_root)
    if not root.exists() or not root.is_dir():
        raise BundleError("Saved root does not exist or is not a directory: " + str(root))
    try:
        root = root.resolve(strict=True)
    except OSError as error:
        raise BundleError("Saved root cannot be resolved safely.") from error
    values = os.environ if environ is None else environ
    included: List[IncludedFile] = []
    excluded: List[ExcludedFile] = []
    total_bytes = 0
    for entry in _walk_entries(root):
        if entry.is_directory:
            excluded.append(ExcludedFile(entry.relative_path, None, "symbolic link directory is never followed"))
            continue
        inspected = _inspect_file(entry, root, values)
        if isinstance(inspected, ExcludedFile):
            excluded.append(inspected)
            continue
        if total_bytes + inspected.bundle_size_bytes > MAX_TOTAL_BYTES:
            excluded.append(ExcludedFile(
                inspected.relative_path,
                inspected.source_size_bytes,
                "bundle total would exceed size limit",
            ))
            continue
        included.append(inspected)
        total_bytes += inspected.bundle_size_bytes
    included.sort(key=lambda item: item.relative_path.casefold())
    excluded.sort(key=lambda item: item.relative_path.casefold())
    memo_result = redact_text(memo, values) if memo is not None else None
    return BundlePlan(
        saved_root=root,
        included=tuple(included),
        excluded=tuple(excluded),
        memo=memo_result,
        build_fingerprint=_build_fingerprint(included),
    )


def _manifest(plan: BundlePlan) -> Dict[str, object]:
    files = []
    for item in plan.included:
        files.append({
            "path": item.relative_path,
            "reason": item.reason,
            "source_size_bytes": item.source_size_bytes,
            "bundle_size_bytes": item.bundle_size_bytes,
            "sha256": item.sha256,
            "redaction_counts": dict(item.redaction_counts),
        })
    manifest: Dict[str, object] = {
        "schema": "imperial-wings-diagnostic-bundle",
        "schema_version": 1,
        "tool_version": TOOL_VERSION,
        "network_access": "not performed",
        "files": files,
        "excluded_file_count": len(plan.excluded),
    }
    if plan.memo is not None:
        manifest["memo"] = plan.memo.text
        manifest["memo_redaction_counts"] = dict(plan.memo.counts)
    if plan.build_fingerprint is not None:
        manifest["build_fingerprint"] = dict(plan.build_fingerprint)
    return manifest


def _zip_info(name: str) -> zipfile.ZipInfo:
    if name.startswith("/") or "\\" in name or ".." in PurePosixPath(name).parts:
        raise BundleError("Refusing an unsafe archive member path.")
    info = zipfile.ZipInfo(filename=name, date_time=ZIP_TIMESTAMP)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.create_system = 3
    info.external_attr = 0o100644 << 16
    return info


def _validate_output_path(output_path: Path, saved_root: Path) -> Path:
    output = output_path.expanduser()
    if output.suffix.casefold() != ".zip":
        raise BundleError("Output file must end with .zip.")
    try:
        resolved_output = output.resolve(strict=False)
    except OSError as error:
        raise BundleError("Output path cannot be resolved safely.") from error
    if _is_relative_to(resolved_output, saved_root):
        raise BundleError("Output ZIP must be outside the Saved root to avoid self-inclusion.")
    return resolved_output


def write_bundle(plan: BundlePlan, output_path: Path | str) -> Path:
    """Write the already-reviewed plan atomically, without making network requests."""
    output = _validate_output_path(Path(output_path), plan.saved_root)
    output.parent.mkdir(parents=True, exist_ok=True)
    manifest_data = (json.dumps(
        _manifest(plan), ensure_ascii=False, sort_keys=True, indent=2
    ) + "\n").encode("utf-8")
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=".diagnostic-bundle-", suffix=".zip", dir=str(output.parent)
    )
    os.close(descriptor)
    temporary = Path(temporary_name)
    try:
        with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED,
                             compresslevel=9, strict_timestamps=True) as archive:
            archive.writestr(_zip_info("manifest.json"), manifest_data, compress_type=zipfile.ZIP_DEFLATED,
                             compresslevel=9)
            for item in plan.included:
                archive.writestr(_zip_info(item.relative_path), item.data, compress_type=zipfile.ZIP_DEFLATED,
                                 compresslevel=9)
        os.replace(temporary, output)
    except (OSError, zipfile.BadZipFile) as error:
        try:
            temporary.unlink()
        except OSError:
            pass
        raise BundleError("Could not create diagnostic ZIP: " + str(error)) from error
    return output


def _format_size(size: Optional[int]) -> str:
    return "unknown" if size is None else str(size) + " bytes"


def print_plan(plan: BundlePlan, stream: Optional[object] = None) -> None:
    """Print the complete review list.  This function never writes a ZIP."""
    if stream is None:
        stream = sys.stdout
    print("DRY RUN REVIEW — no ZIP is created unless --confirm is supplied.", file=stream)
    for item in plan.included:
        print(
            "INCLUDE {path} | source={source} | bundled={bundled} | {reason} | redactions={redactions}".format(
                path=item.relative_path,
                source=_format_size(item.source_size_bytes),
                bundled=_format_size(item.bundle_size_bytes),
                reason=item.reason,
                redactions=sum(item.redaction_counts.values()),
            ),
            file=stream,
        )
    for item in plan.excluded:
        print(
            "EXCLUDE {path} | size={size} | {reason}".format(
                path=item.relative_path,
                size=_format_size(item.size_bytes),
                reason=item.reason,
            ),
            file=stream,
        )
    print(
        "SUMMARY: {included} file(s) would be included; {excluded} item(s) excluded; no network access.".format(
            included=len(plan.included), excluded=len(plan.excluded)
        ),
        file=stream,
    )


def _argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Review and explicitly create a redacted local diagnostic ZIP. No network access is performed."
    )
    parser.add_argument(
        "--saved-root", type=Path, default=default_saved_root(),
        help="ImperialWingsSimulator Saved directory (default: %%LOCALAPPDATA%%\\ImperialWingsSimulator\\Saved).",
    )
    parser.add_argument(
        "--output", type=Path, default=Path("diagnostic-bundle.zip"),
        help="ZIP destination outside --saved-root; used only with --confirm.",
    )
    parser.add_argument("--memo", help="Optional user note placed in manifest.json after redaction.")
    parser.add_argument(
        "--confirm", action="store_true",
        help="Create the ZIP after printing the complete review list. Without this flag the command is dry-run only.",
    )
    return parser


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = _argument_parser()
    args = parser.parse_args(arguments)
    try:
        plan = plan_bundle(args.saved_root, args.memo)
        print_plan(plan)
        if not args.confirm:
            return 0
        output = write_bundle(plan, args.output)
        print("CREATED " + str(output) + " after explicit --confirm; no network access.")
        return 0
    except BundleError as error:
        print("error: " + str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
