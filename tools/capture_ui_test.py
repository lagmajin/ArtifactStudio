#!/usr/bin/env python3
"""Launch ArtifactUiTest.exe and capture a startup, Timeline, or Render Manager fixture."""

from __future__ import annotations

import argparse
import ctypes
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", required=True, type=Path, help="ArtifactUiTest.exe path")
    parser.add_argument("--output", required=True, type=Path, help="PNG output path")
    parser.add_argument(
        "--language",
        choices=("ja", "en", "zh", "zh-TW", "ko", "fr", "de", "es", "pt", "ru", "ar"),
        default="en",
        help="UI language passed to ArtifactUiTest.exe via --lang (default: en)",
    )
    parser.add_argument(
        "--timeout-seconds",
        type=float,
        default=60.0,
        help="maximum time to wait for the app screenshot (default: 60)",
    )
    parser.add_argument(
        "--timeline",
        action="store_true",
        help="show the isolated Timeline fixture instead of the default startup view",
    )
    parser.add_argument(
        "--curve-editor",
        action="store_true",
        help="show the isolated animated Curve Editor fixture",
    )
    parser.add_argument(
        "--render-manager",
        action="store_true",
        help="show a fixed Render Manager queue fixture",
    )
    parser.add_argument(
        "--app-arg",
        action="append",
        default=[],
        help="argument passed to ArtifactUiTest.exe; may be repeated",
    )
    return parser.parse_args()


def stop_process(process: subprocess.Popen[bytes]) -> None:
    if process.poll() is not None:
        return
    request_close_window(process.pid)
    try:
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


def request_close_window(process_id: int) -> bool:
    if os.name != "nt":
        return False

    from ctypes import wintypes

    user32 = ctypes.WinDLL("user32", use_last_error=True)
    enum_windows = user32.EnumWindows
    enum_windows_callback_type = ctypes.WINFUNCTYPE(
        wintypes.BOOL,
        wintypes.HWND,
        wintypes.LPARAM,
    )
    enum_windows.argtypes = (enum_windows_callback_type, wintypes.LPARAM)
    enum_windows.restype = wintypes.BOOL
    get_window_thread_process_id = user32.GetWindowThreadProcessId
    get_window_thread_process_id.argtypes = (
        wintypes.HWND,
        ctypes.POINTER(wintypes.DWORD),
    )
    get_window_thread_process_id.restype = wintypes.DWORD
    is_window_visible = user32.IsWindowVisible
    is_window_visible.argtypes = (wintypes.HWND,)
    is_window_visible.restype = wintypes.BOOL
    post_message = user32.PostMessageW
    post_message.argtypes = (
        wintypes.HWND,
        wintypes.UINT,
        wintypes.WPARAM,
        wintypes.LPARAM,
    )
    post_message.restype = wintypes.BOOL

    found_window = False

    @enum_windows_callback_type
    def close_process_window(hwnd: int, _parameter: int) -> bool:
        nonlocal found_window
        window_process_id = wintypes.DWORD()
        get_window_thread_process_id(hwnd, ctypes.byref(window_process_id))
        if window_process_id.value == process_id and is_window_visible(hwnd):
            found_window = bool(post_message(hwnd, 0x0010, 0, 0)) or found_window
        return True

    enum_windows(close_process_window, 0)
    return found_window


def main() -> int:
    args = parse_args()
    if args.render_manager and (args.timeline or args.curve_editor):
        print("capture_ui_test: --render-manager cannot be combined with Timeline fixtures", file=sys.stderr)
        return 2
    if os.name != "nt":
        print("capture_ui_test: this helper requires Windows", file=sys.stderr)
        return 2

    exe_path = args.exe.expanduser().resolve()
    output_path = args.output.expanduser().resolve()

    if not exe_path.is_file():
        print(f"capture_ui_test: executable not found: {exe_path}", file=sys.stderr)
        return 2
    if args.timeout_seconds <= 0:
        print("capture_ui_test: timeout must be positive", file=sys.stderr)
        return 2

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.unlink(missing_ok=True)

    environment = os.environ.copy()
    for fixture_flag in (
        "ARTIFACT_UI_TEST_TIMELINE",
        "ARTIFACT_UI_TEST_CURVE_EDITOR",
        "ARTIFACT_UI_TEST_RENDER_MANAGER",
        "ARTIFACT_UI_TEST_PROJECT_ROOT",
        "ARTIFACT_UI_TEST_FORCE_SHUTDOWN",
        "ARTIFACT_STARTUP_SCREENSHOT_WIDGET",
    ):
        environment.pop(fixture_flag, None)
    environment["ARTIFACT_STARTUP_SCREENSHOT"] = "1"
    environment["ARTIFACT_UI_TEST_FORCE_SHUTDOWN"] = "1"
    environment["ARTIFACT_STARTUP_SCREENSHOT_PATH"] = str(output_path)
    fixture_root: Path | None = None
    if args.timeline or args.curve_editor or args.render_manager:
        fixture_root = Path(tempfile.mkdtemp(
            prefix="artifact_ui_test_", dir=output_path.parent
        )).resolve()
        fixture_project_root = fixture_root / "project"
        fixture_appdata_root = fixture_root / "appdata"
        fixture_project_root.mkdir()
        fixture_appdata_root.mkdir()
        environment["ARTIFACT_UI_TEST_TIMELINE"] = "1"
        environment["ARTIFACT_UI_TEST_PROJECT_ROOT"] = str(fixture_project_root)
        environment["ARTIFACT_STARTUP_SCREENSHOT_WIDGET"] = "timelineUiFixtureWidget"
        environment["APPDATA"] = str(fixture_appdata_root)
        environment["LOCALAPPDATA"] = str(fixture_appdata_root)
    if args.render_manager:
        environment.pop("ARTIFACT_UI_TEST_TIMELINE", None)
        environment["ARTIFACT_UI_TEST_RENDER_MANAGER"] = "1"
        environment.pop("ARTIFACT_STARTUP_SCREENSHOT_WIDGET", None)
        environment["ARTIFACT_STARTUP_SCREENSHOT_WIDGET"] = "renderManagerUiFixtureWidget"
    if args.curve_editor:
        environment["ARTIFACT_UI_TEST_CURVE_EDITOR"] = "1"

    try:
        process = subprocess.Popen(
            [str(exe_path), "--lang", args.language, *args.app_arg],
            cwd=exe_path.parent,
            env=environment,
        )
    except OSError as error:
        print(f"capture_ui_test: failed to launch {exe_path}: {error}", file=sys.stderr)
        if fixture_root is not None:
            shutil.rmtree(fixture_root, ignore_errors=True)
        return 2

    deadline = time.monotonic() + args.timeout_seconds
    previous_size = -1
    stable_observations = 0
    try:
        while time.monotonic() < deadline:
            if output_path.is_file():
                size = output_path.stat().st_size
                if size > 0 and size == previous_size:
                    stable_observations += 1
                    if stable_observations >= 2:
                        print(output_path)
                        return 0
                else:
                    stable_observations = 0
                previous_size = size

            exit_code = process.poll()
            if exit_code is not None:
                print(
                    "capture_ui_test: app exited before producing a screenshot "
                    f"(exit code {exit_code})",
                    file=sys.stderr,
                )
                return 1
            time.sleep(0.1)

        print(
            f"capture_ui_test: timed out waiting for screenshot: {output_path}",
            file=sys.stderr,
        )
        return 1
    finally:
        stop_process(process)
        if fixture_root is not None:
            shutil.rmtree(fixture_root, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())
