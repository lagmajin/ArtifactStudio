#!/usr/bin/env python3
"""Record compiler tool identities without compiling shader sources."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
from typing import Any


TOOLS = {
    "glslangValidator": ("glslangValidator", "--version"),
    "dxc": ("dxc", "--version"),
    "spirv-val": ("spirv-val", "--version"),
    "spirv-opt": ("spirv-opt", "--version"),
    "spirv-dis": ("spirv-dis", "--version"),
}
DXC_RUNTIME_FILES = ("dxcompiler.dll", "dxil.dll", "libdxcompiler.so", "libdxil.so")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def portable_location(path: Path) -> str:
    resolved = path.resolve()
    sdk = os.environ.get("VULKAN_SDK")
    if sdk:
        try:
            return "<VULKAN_SDK>/" + resolved.relative_to(Path(sdk).resolve()).as_posix()
        except ValueError:
            pass
    return "<PATH>/" + resolved.name


def inspect_binary(path: Path, role: str) -> dict[str, Any]:
    stat = path.stat()
    return {
        "role": role,
        "location": portable_location(path),
        "fileName": path.name,
        "byteLength": stat.st_size,
        "sha256": sha256(path),
    }


def inspect_tool(name: str, command: str, version_argument: str) -> dict[str, Any]:
    located = shutil.which(command)
    if located is None:
        return {"status": "not-found", "command": command}
    executable = Path(located)
    result = subprocess.run([str(executable), version_argument], capture_output=True, text=True)
    record: dict[str, Any] = {
        "status": "captured" if result.returncode == 0 else "version-query-failed",
        "command": command,
        "versionArguments": [version_argument],
        "versionExitCode": result.returncode,
        "versionStdout": result.stdout.replace("\r\n", "\n").strip(),
        "versionStderr": result.stderr.replace("\r\n", "\n").strip(),
        "executable": inspect_binary(executable, "executable"),
    }
    if name == "dxc":
        record["runtimeLibraries"] = [
            inspect_binary(executable.parent / filename, "runtime-library")
            for filename in DXC_RUNTIME_FILES
            if (executable.parent / filename).is_file()
        ]
    return record


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path,
        default=Path("experiments/ShaderGraphLab/shader_compiler_toolchain_snapshot.json"))
    args = parser.parse_args()

    tools = {name: inspect_tool(name, command, version_arg)
             for name, (command, version_arg) in TOOLS.items()}
    complete = all(item.get("status") == "captured" for item in tools.values())
    report = {
        "schemaId": "artifact.shadergraph.compiler-toolchain-snapshot",
        "schemaVersion": 1,
        "status": ("captured-current-toolchain-not-historical-provenance" if complete
                   else "incomplete-current-toolchain-not-historical-provenance"),
        "capturedAtUtc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "host": {"system": platform.system(), "release": platform.release(),
                 "machine": platform.machine()},
        "targetEnvironment": "vulkan1.2",
        "tools": tools,
        "evidenceBoundary": "This is a current host snapshot. It does not establish which binaries produced legacy_core_projection_compiler_evidence.json; that older compilation report omitted tool identities and must be rerun to bind results to this snapshot.",
        "shaderCompilationPerformed": False,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                           encoding="utf-8", newline="\n")
    print(f"status={report['status']} output={args.output}")
    for name, item in tools.items():
        executable = item.get("executable", {})
        print(f"{name}: {item['status']} {executable.get('sha256', '')}")
    return 0 if complete else 1


if __name__ == "__main__":
    raise SystemExit(main())
