#!/usr/bin/env python3
"""Launch ArtifactStudio for local UI automation without a desktop window.

The app stays in a hidden Windows window while its localhost MCP control
endpoint accepts virtual Qt input. No desktop capture or OS input injection is
used. Pair this launcher with ``ui_control_client.py``.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time

from ui_control_client import UiControlClient


def _free_loopback_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind(("127.0.0.1", 0))
        return int(probe.getsockname()[1])


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", required=True, type=Path, help="Artifact.exe path")
    parser.add_argument(
        "--port", type=int, default=0,
        help="localhost UI-control port (default: choose a free port)",
    )
    parser.add_argument(
        "--run-dir", type=Path,
        help="directory for isolated app data and logs (default: create a temp run directory)",
    )
    parser.add_argument(
        "--project", type=Path,
        help="optional project to open explicitly",
    )
    parser.add_argument(
        "app_args", nargs=argparse.REMAINDER,
        help="additional Artifact arguments after --, such as --lang en",
    )
    return parser.parse_args()


def _wait_for_endpoint(port: int, process: subprocess.Popen[bytes], timeout: float) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if process.poll() is not None:
            return False
        try:
            client = UiControlClient(port, timeout=0.5)
            try:
                identity = client.handshake().get("serverInfo", {})
                if identity.get("name") == "ArtifactUiControl":
                    return True
            finally:
                client.close()
        except (OSError, RuntimeError, ValueError):
            time.sleep(0.1)
    return False


def main() -> int:
    args = _parse_args()
    exe = args.exe.expanduser().resolve()
    if os.name != "nt":
        print("run_ui_offline: this launcher requires Windows", file=sys.stderr)
        return 2
    if not exe.is_file():
        print(f"run_ui_offline: executable not found: {exe}", file=sys.stderr)
        return 2
    if args.port < 0 or args.port > 65535:
        print("run_ui_offline: port must be between 0 and 65535", file=sys.stderr)
        return 2
    if args.project is not None and not args.project.expanduser().is_file():
        print(f"run_ui_offline: project not found: {args.project}", file=sys.stderr)
        return 2

    run_dir = (
        args.run_dir.expanduser().resolve()
        if args.run_dir else
        Path(tempfile.mkdtemp(prefix="artifact-ui-offline-")).resolve()
    )
    run_dir.mkdir(parents=True, exist_ok=True)
    app_data = run_dir / "appdata"
    app_data.mkdir(parents=True, exist_ok=True)
    port = args.port or _free_loopback_port()
    app_args = list(args.app_args)
    if app_args and app_args[0] == "--":
        app_args.pop(0)
    if "--ui-headless" in app_args or "--ui-mcp-port" in app_args:
        print("run_ui_offline: UI headless/control flags are supplied by this launcher", file=sys.stderr)
        return 2

    command = [str(exe), "--no-splash", "--ui-headless", "--ui-mcp-port", str(port)]
    command.extend(app_args)
    if args.project is not None:
        command.append(str(args.project.expanduser().resolve()))

    environment = os.environ.copy()
    environment["APPDATA"] = str(app_data)
    environment["LOCALAPPDATA"] = str(app_data)
    environment["QT_QPA_PLATFORM"] = "windows"
    environment["QT_QPA_PLATFORM_PLUGIN_PATH"] = str(exe.parent / "platforms")
    # Artifact enables startup capture by default when no user setting exists.
    # Headless sessions capture only when the caller explicitly requests it.
    environment["ARTIFACT_STARTUP_SCREENSHOT"] = "0"
    for key in (
        "ARTIFACT_STARTUP_SCREENSHOT_PATH",
        "ARTIFACT_STARTUP_SCREENSHOT_WIDGET",
        "ARTIFACT_STARTUP_SCREENSHOT_NATIVE_WINDOW",
    ):
        environment.pop(key, None)
    log_path = run_dir / "artifact-ui.log"
    log_file = log_path.open("wb")
    creation_flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    process = subprocess.Popen(
        command,
        cwd=exe.parent,
        env=environment,
        stdin=subprocess.DEVNULL,
        stdout=log_file,
        stderr=subprocess.STDOUT,
        creationflags=creation_flags,
    )

    print(f"Artifact UI process: {process.pid}")
    print(f"Control endpoint: 127.0.0.1:{port}")
    print(f"Run directory: {run_dir}")
    print(f"Application log: {log_path}")
    print("Use tools/ui_control_client.py to inspect, operate, and capture widgets.")
    print("Press Ctrl+C here to request a clean application shutdown.")

    try:
        if not _wait_for_endpoint(port, process, 90.0):
            print("run_ui_offline: UI endpoint did not become ready", file=sys.stderr)
            if process.poll() is not None:
                print(f"Artifact exited with code {process.returncode}", file=sys.stderr)
            return 1
        while process.poll() is None:
            time.sleep(0.25)
        return int(process.returncode or 0)
    except KeyboardInterrupt:
        if process.poll() is None:
            try:
                with UiControlClient(port, timeout=2.0) as client:
                    client.call("ui.quit")
                process.wait(timeout=12.0)
            except (OSError, RuntimeError, subprocess.TimeoutExpired):
                process.terminate()
                try:
                    process.wait(timeout=5.0)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
        return int(process.returncode or 0)
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5.0)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        log_file.close()


if __name__ == "__main__":
    raise SystemExit(main())
