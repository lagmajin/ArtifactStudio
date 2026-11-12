#!/usr/bin/env python3
"""Run the existing transition contract executable and verify its captures."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def git_metadata(repository: Path) -> dict[str, Any]:
    def git(*arguments: str) -> str | None:
        try:
            completed = subprocess.run(
                ["git", "-C", str(repository), *arguments],
                check=False, capture_output=True, text=True, timeout=5,
            )
        except (OSError, subprocess.TimeoutExpired):
            return None
        return completed.stdout.strip() if completed.returncode == 0 else None

    revision = git("rev-parse", "HEAD")
    status = git("status", "--porcelain", "--untracked-files=all")
    return {
        "revision": revision,
        "working_tree_dirty": bool(status) if status is not None else None,
        "status_sha256": hashlib.sha256(status.encode("utf-8")).hexdigest()
        if status is not None else None,
        "status_available": status is not None,
    }


def infer_build_config(executable: Path) -> str | None:
    known = {"debug", "release", "relwithdebinfo", "minsizerel"}
    for part in (executable.parent.name, executable.parent.parent.name):
        if part.lower() in known:
            return part
    return None


def write_json(path: Path, value: dict[str, Any]) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--test-executable", required=True, type=Path,
                        help="existing ArtifactCoreTransitionRenderContractTest executable")
    parser.add_argument("--capture-directory", required=True, type=Path,
                        help="new or empty directory for PNG and JSON artifacts")
    parser.add_argument("--build-config", help="build configuration, e.g. Debug or Release")
    parser.add_argument("--runtime-path", action="append", type=Path, default=[],
                        help="additional DLL/shared-library search directory; may be repeated")
    parser.add_argument("--baseline", type=Path,
                        help="previous capture directory or verification_report.json to compare")
    parser.add_argument("--repo-root", type=Path,
                        default=Path(__file__).resolve().parents[1],
                        help="repository root used to record Git revision and dirty state")
    args = parser.parse_args()

    executable = args.test_executable.expanduser().resolve()
    directory = args.capture_directory.expanduser().resolve()
    repository = args.repo_root.expanduser().resolve()
    if not executable.is_file():
        parser.error(f"test executable does not exist: {executable}")
    if directory.exists() and not directory.is_dir():
        parser.error(f"capture path is not a directory: {directory}")
    if directory.exists() and any(directory.iterdir()):
        parser.error(f"capture directory must be empty to avoid overwriting: {directory}")
    directory.mkdir(parents=True, exist_ok=True)

    build_config = args.build_config or infer_build_config(executable)
    manifest: dict[str, Any] = {
        "schema_version": 1,
        "suite": "Transition Effects R&D",
        "started_at_utc": datetime.now(timezone.utc).isoformat(),
        "source": git_metadata(repository),
        "binary": {"name": executable.name, "sha256": sha256(executable)},
        "build_config": build_config or "unknown",
        "host": {
            "platform": platform.platform(),
            "system": platform.system(),
            "release": platform.release(),
            "machine": platform.machine(),
            "processor": platform.processor() or "unknown",
            "logical_cpu_count": os.cpu_count(),
        },
        "runner": {"python": sys.version.split()[0],
                   "sha256": sha256(Path(__file__).resolve())},
        "capture_directory": directory.name,
        "test_process": {"return_code": None},
    }

    environment = os.environ.copy()
    environment["ARTIFACT_TRANSITION_CAPTURE_DIR"] = str(directory)
    environment["ARTIFACT_BUILD_CONFIG"] = build_config or "unknown"
    environment["ARTIFACT_SOURCE_REVISION"] = manifest["source"]["revision"] or "unknown"
    runtime_paths = [str(executable.parent)] + [
        str(path.expanduser().resolve()) for path in args.runtime_path
    ]
    if environment.get("PATH"):
        runtime_paths.append(environment["PATH"])
    environment["PATH"] = os.pathsep.join(runtime_paths)
    try:
        process = subprocess.run([str(executable)], cwd=repository, env=environment, check=False)
        manifest["test_process"]["return_code"] = process.returncode
    except OSError as error:
        manifest["test_process"]["launch_error"] = str(error)
        manifest["test_process"]["return_code"] = 127
    manifest["finished_at_utc"] = datetime.now(timezone.utc).isoformat()
    write_json(directory / "run_manifest.json", manifest)

    analyzer = Path(__file__).resolve().with_name("analyze_transition_capture.py")
    analysis_command = [sys.executable, str(analyzer), str(directory)]
    if args.baseline:
        analysis_command.extend(["--baseline", str(args.baseline.expanduser().resolve())])
    analysis = subprocess.run(analysis_command, cwd=repository, check=False)
    if manifest["test_process"]["return_code"] != 0:
        return int(manifest["test_process"]["return_code"])
    return analysis.returncode


if __name__ == "__main__":
    raise SystemExit(main())
