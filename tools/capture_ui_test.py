#!/usr/bin/env python3
"""Launch ArtifactUiTest.exe and capture an isolated application UI fixture."""

from __future__ import annotations

import argparse
import ctypes
import math
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
        "--contents-viewer",
        type=Path,
        metavar="MEDIA",
        help="show only Contents Viewer with the given image, SVG, video, audio, or 3D model",
    )
    parser.add_argument(
        "--tracker",
        action="store_true",
        help="show the Tracker dock beside a temporary composition",
    )
    parser.add_argument(
        "--tracker-footage",
        type=Path,
        help="decode a video into a numbered image sequence for Tracker acceptance",
    )
    parser.add_argument(
        "--tracker-mode",
        choices=("point", "planar"),
        default="point",
        help="Tracker mode used by the fixture (default: point)",
    )
    parser.add_argument(
        "--tracker-direction",
        choices=("all", "forward", "backward"),
        default="all",
        help="range direction used by the fixture (default: all)",
    )
    parser.add_argument(
        "--tracker-camera",
        action="store_true",
        help="run the existing Camera Tracker menu operation on the same footage",
    )
    parser.add_argument(
        "--cloner",
        action="store_true",
        help="show a raster source with a Cloner in the Composition View",
    )
    parser.add_argument(
        "--cloner-backend",
        choices=("cpu", "gpu"),
        default="cpu",
        help="Cloner placement path to validate when --cloner is used (default: cpu)",
    )
    parser.add_argument(
        "--cloner-expect-cpu-fallback",
        action="store_true",
        help="with random distribution and the GPU backend, assert CPU fallback",
    )
    parser.add_argument(
        "--cloner-distribution",
        choices=("grid", "matrix", "linear", "radial", "random"),
        default="grid",
        help="Cloner distribution used by the --cloner fixture (default: grid)",
    )
    parser.add_argument(
        "--cloner-rotation-step",
        type=float,
        default=0.0,
        help="rotation step in degrees for the Linear or Radial Cloner fixture",
    )
    parser.add_argument(
        "--cloner-opacity-decay",
        type=float,
        default=0.0,
        help="per-clone opacity decay for the Cloner fixture",
    )
    parser.add_argument(
        "--cloner-transform-stack",
        action="store_true",
        help="exercise translation and Z-rotation transform-stack placement",
    )
    parser.add_argument(
        "--cloner-plain-translation",
        action="store_true",
        help="exercise Grid placement with a translated Plain clone modifier",
    )
    parser.add_argument(
        "--cloner-plain-rotation",
        action="store_true",
        help="exercise Grid placement with a rotated Plain clone modifier",
    )
    parser.add_argument(
        "--cloner-plain-scale",
        action="store_true",
        help="exercise Grid placement with a scaled Plain clone modifier",
    )
    parser.add_argument(
        "--cloner-plain-chain",
        action="store_true",
        help="exercise a shear-free two-Plain modifier chain on Grid placement",
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
    isolated_fixtures = sum((
        bool(args.timeline or args.curve_editor), args.render_manager,
        args.cloner, args.tracker, args.contents_viewer is not None,
    ))
    if isolated_fixtures > 1:
        print("capture_ui_test: choose only one isolated fixture", file=sys.stderr)
        return 2
    if args.contents_viewer is not None:
        args.contents_viewer = args.contents_viewer.expanduser().resolve()
        if not args.contents_viewer.is_file():
            print(f"capture_ui_test: media file not found: {args.contents_viewer}", file=sys.stderr)
            return 2
        if args.contents_viewer.suffix.lower() not in {
            ".png", ".jpg", ".jpeg", ".svg", ".mp4", ".avi", ".mov",
            ".mkv", ".wav", ".mp3", ".flac", ".obj", ".fbx", ".gltf", ".glb",
        }:
            print(f"capture_ui_test: unsupported Contents Viewer fixture: {args.contents_viewer.suffix}", file=sys.stderr)
            return 2
    if args.render_manager and (args.timeline or args.curve_editor or args.cloner or args.tracker):
        print("capture_ui_test: --render-manager cannot be combined with other fixtures", file=sys.stderr)
        return 2
    if args.cloner and (args.timeline or args.curve_editor):
        print("capture_ui_test: --cloner cannot be combined with Timeline fixtures", file=sys.stderr)
        return 2
    plain_modifier_fixture = (
        args.cloner_plain_translation or args.cloner_plain_rotation or
        args.cloner_plain_scale or args.cloner_plain_chain
    )
    if args.cloner_plain_chain and (
            args.cloner_plain_translation or args.cloner_plain_rotation or
            args.cloner_plain_scale):
        print(
            "capture_ui_test: --cloner-plain-chain cannot be combined with "
            "other Plain modifier fixtures",
            file=sys.stderr,
        )
        return 2
    if plain_modifier_fixture and not args.cloner:
        print(
            "capture_ui_test: Plain modifier fixtures require --cloner",
            file=sys.stderr,
        )
        return 2
    if plain_modifier_fixture and args.cloner_distribution not in ("grid", "matrix"):
        print(
            "capture_ui_test: Plain modifier fixtures require Grid or Matrix",
            file=sys.stderr,
        )
        return 2
    if args.cloner_backend != "cpu" and not args.cloner:
        print("capture_ui_test: --cloner-backend requires --cloner", file=sys.stderr)
        return 2
    if args.cloner_expect_cpu_fallback and (
            not args.cloner or args.cloner_backend != "gpu"):
        print(
            "capture_ui_test: --cloner-expect-cpu-fallback requires "
            "--cloner --cloner-backend gpu",
            file=sys.stderr,
        )
        return 2
    if args.cloner_expect_cpu_fallback and args.cloner_distribution != "random":
        print(
            "capture_ui_test: --cloner-expect-cpu-fallback requires "
            "--cloner-distribution random",
            file=sys.stderr,
        )
        return 2
    if (args.cloner and args.cloner_backend == "gpu" and
            args.cloner_distribution == "random" and
            not args.cloner_expect_cpu_fallback):
        print(
            "capture_ui_test: GPU random distribution requires "
            "--cloner-expect-cpu-fallback",
            file=sys.stderr,
        )
        return 2
    if args.cloner_distribution != "grid" and not args.cloner:
        print("capture_ui_test: --cloner-distribution requires --cloner", file=sys.stderr)
        return 2
    if args.cloner_transform_stack and (
            not args.cloner or args.cloner_distribution not in
            ("grid", "linear", "radial")):
        print(
            "capture_ui_test: --cloner-transform-stack requires "
            "--cloner with Grid, Linear, or Radial distribution",
            file=sys.stderr,
        )
        return 2
    if (args.cloner_plain_translation or args.cloner_plain_rotation) and (
            not args.cloner or args.cloner_distribution != "grid"):
        print(
            "capture_ui_test: Plain Cloner modifier fixtures require "
            "--cloner and Grid distribution",
            file=sys.stderr,
        )
        return 2
    if args.cloner_rotation_step != 0.0 and args.cloner_distribution not in (
            "linear", "radial"):
        print(
            "capture_ui_test: --cloner-rotation-step requires "
            "--cloner-distribution linear or radial",
            file=sys.stderr,
        )
        return 2
    if args.tracker and (args.timeline or args.curve_editor or args.cloner):
        print("capture_ui_test: --tracker cannot be combined with other fixtures", file=sys.stderr)
        return 2
    if (args.tracker_footage or args.tracker_mode != "point" or
            args.tracker_direction != "all" or args.tracker_camera) and not args.tracker:
        print("capture_ui_test: Tracker options require --tracker", file=sys.stderr)
        return 2
    if args.tracker_camera and not args.tracker_footage:
        print("capture_ui_test: --tracker-camera requires --tracker-footage", file=sys.stderr)
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
    renderer_output_path = (
        output_path.with_name(f"{output_path.stem}-renderer.png")
        if args.cloner else None
    )
    cloner_route_path = (
        Path(f"{renderer_output_path}.route.txt")
        if renderer_output_path is not None else None
    )
    cloner_stack_fixture_path = (
        Path(f"{cloner_route_path}.stackfixture.txt")
        if cloner_route_path is not None and args.cloner_transform_stack
        else None
    )
    cloner_fixture_path = (
        Path(f"{cloner_route_path}.fixture.txt")
        if cloner_route_path is not None else None
    )
    if renderer_output_path is not None:
        renderer_output_path.unlink(missing_ok=True)
    if cloner_route_path is not None:
        cloner_route_path.unlink(missing_ok=True)
    if cloner_stack_fixture_path is not None:
        cloner_stack_fixture_path.unlink(missing_ok=True)
    if cloner_fixture_path is not None:
        cloner_fixture_path.unlink(missing_ok=True)
    tracker_result_path = output_path.with_suffix(".result.txt")
    if args.tracker:
        tracker_result_path.unlink(missing_ok=True)

    environment = os.environ.copy()
    environment.pop("ARTIFACT_ENABLE_GPU_CLONER", None)
    for fixture_flag in (
        "ARTIFACT_UI_TEST_TIMELINE",
        "ARTIFACT_UI_TEST_CURVE_EDITOR",
        "ARTIFACT_UI_TEST_RENDER_MANAGER",
        "ARTIFACT_UI_TEST_CONTENTS_VIEWER",
        "ARTIFACT_UI_TEST_CONTENTS_VIEWER_PATH",
        "ARTIFACT_UI_TEST_CLONER",
        "ARTIFACT_UI_TEST_TRACKER",
        "ARTIFACT_UI_TEST_PROJECT_ROOT",
        "ARTIFACT_UI_TEST_CLONER_RENDER_OUTPUT",
        "ARTIFACT_UI_TEST_CLONER_DISTRIBUTION",
        "ARTIFACT_UI_TEST_CLONER_ROTATION_STEP",
        "ARTIFACT_UI_TEST_CLONER_OPACITY_DECAY",
        "ARTIFACT_UI_TEST_CLONER_TRANSFORM_STACK",
        "ARTIFACT_UI_TEST_CLONER_PLAIN_TRANSLATION",
        "ARTIFACT_UI_TEST_CLONER_PLAIN_ROTATION",
        "ARTIFACT_UI_TEST_CLONER_PLAIN_SCALE",
        "ARTIFACT_UI_TEST_CLONER_PLAIN_CHAIN",
        "ARTIFACT_UI_TEST_FORCE_SHUTDOWN",
        "ARTIFACT_STARTUP_SCREENSHOT_WIDGET",
        "ARTIFACT_STARTUP_SCREENSHOT_NATIVE_WINDOW",
    ):
        environment.pop(fixture_flag, None)
    environment["ARTIFACT_STARTUP_SCREENSHOT"] = "1"
    environment["ARTIFACT_UI_TEST_FORCE_SHUTDOWN"] = "1"
    environment["ARTIFACT_STARTUP_SCREENSHOT_PATH"] = str(output_path)
    fixture_root: Path | None = None
    console_log_file = None
    console_log_path = (
        Path(f"{output_path}.console.log")
        if args.contents_viewer is not None else None
    )
    if console_log_path is not None:
        console_log_path.parent.mkdir(parents=True, exist_ok=True)
        console_log_file = console_log_path.open("wb")
    if args.timeline or args.curve_editor or args.render_manager or args.cloner or args.tracker or args.contents_viewer:
        fixture_root = Path(tempfile.mkdtemp(
            prefix="artifact_ui_test_", dir=output_path.parent
        )).resolve()
        fixture_project_root = fixture_root / "project"
        fixture_appdata_root = fixture_root / "appdata"
        fixture_project_root.mkdir()
        fixture_appdata_root.mkdir()
        environment["ARTIFACT_UI_TEST_PROJECT_ROOT"] = str(fixture_project_root)
        environment["APPDATA"] = str(fixture_appdata_root)
        environment["LOCALAPPDATA"] = str(fixture_appdata_root)
    if args.timeline or args.curve_editor:
        environment["ARTIFACT_UI_TEST_TIMELINE"] = "1"
        environment["ARTIFACT_STARTUP_SCREENSHOT_WIDGET"] = "timelineUiFixtureWidget"
    if args.render_manager:
        environment.pop("ARTIFACT_UI_TEST_TIMELINE", None)
        environment["ARTIFACT_UI_TEST_RENDER_MANAGER"] = "1"
        environment.pop("ARTIFACT_STARTUP_SCREENSHOT_WIDGET", None)
        environment["ARTIFACT_STARTUP_SCREENSHOT_WIDGET"] = "renderManagerUiFixtureWidget"
    if args.contents_viewer:
        environment["ARTIFACT_UI_TEST_CONTENTS_VIEWER"] = "1"
        environment["ARTIFACT_UI_TEST_CONTENTS_VIEWER_PATH"] = str(args.contents_viewer)
        environment["ARTIFACT_STARTUP_SCREENSHOT_WIDGET"] = "contentsViewerUiFixtureWidget"
        environment["ARTIFACT_STARTUP_SCREENSHOT_NATIVE_WINDOW"] = "1"
    if args.curve_editor:
        environment["ARTIFACT_UI_TEST_CURVE_EDITOR"] = "1"
    if args.cloner:
        if args.cloner_backend == "gpu":
            environment["ARTIFACT_ENABLE_GPU_CLONER"] = "1"
        environment["ARTIFACT_UI_TEST_CLONER"] = "1"
        environment["ARTIFACT_UI_TEST_CLONER_RENDER_OUTPUT"] = str(
            renderer_output_path
        )
        environment["ARTIFACT_UI_TEST_CLONER_DISTRIBUTION"] = (
            args.cloner_distribution
        )
        environment["ARTIFACT_UI_TEST_CLONER_ROTATION_STEP"] = str(
            args.cloner_rotation_step
        )
        environment["ARTIFACT_UI_TEST_CLONER_OPACITY_DECAY"] = str(
            args.cloner_opacity_decay
        )
        if args.cloner_transform_stack:
            environment["ARTIFACT_UI_TEST_CLONER_TRANSFORM_STACK"] = "1"
        if args.cloner_plain_translation:
            environment["ARTIFACT_UI_TEST_CLONER_PLAIN_TRANSLATION"] = "1"
        if args.cloner_plain_rotation:
            environment["ARTIFACT_UI_TEST_CLONER_PLAIN_ROTATION"] = "1"
        if args.cloner_plain_scale:
            environment["ARTIFACT_UI_TEST_CLONER_PLAIN_SCALE"] = "1"
        if args.cloner_plain_chain:
            environment["ARTIFACT_UI_TEST_CLONER_PLAIN_CHAIN"] = "1"
        environment["ARTIFACT_STARTUP_SCREENSHOT_NATIVE_WINDOW"] = "1"
    if args.tracker:
        environment["ARTIFACT_UI_TEST_TRACKER"] = "1"
        environment["ARTIFACT_UI_TEST_TRACKER_RESULT_PATH"] = str(
            tracker_result_path
        )
        environment["ARTIFACT_UI_TEST_TRACKER_MODE"] = args.tracker_mode
        environment["ARTIFACT_UI_TEST_TRACKER_DIRECTION"] = args.tracker_direction
        if args.tracker_footage:
            footage_path = args.tracker_footage.expanduser().resolve()
            if not footage_path.is_file():
                print(f"capture_ui_test: tracker footage not found: {footage_path}", file=sys.stderr)
                return 2
            ffmpeg = shutil.which("ffmpeg")
            if not ffmpeg:
                print("capture_ui_test: ffmpeg is required for --tracker-footage", file=sys.stderr)
                return 2
            sequence_dir = fixture_root / "tracker-footage-sequence"
            sequence_dir.mkdir()
            extraction = subprocess.run(
                [ffmpeg, "-hide_banner", "-loglevel", "error", "-i",
                 str(footage_path), "-fps_mode", "passthrough",
                 "-frames:v", "600",
                 str(sequence_dir / "frame-%06d.png")],
                check=False,
                capture_output=True,
                text=True,
            )
            if extraction.returncode != 0:
                print(f"capture_ui_test: failed to decode tracker footage: {extraction.stderr}", file=sys.stderr)
                return 2
            environment["ARTIFACT_UI_TEST_TRACKER_SEQUENCE_DIR"] = str(sequence_dir)
            environment["ARTIFACT_UI_TEST_TRACKER_SEQUENCE_FPS"] = "15"
            if args.tracker_camera:
                environment["ARTIFACT_UI_TEST_TRACKER_CAMERA_PATH"] = str(footage_path)

    try:
        process = subprocess.Popen(
            [str(exe_path), "--lang", args.language, *args.app_arg],
            cwd=exe_path.parent,
            env=environment,
            stdout=console_log_file,
            stderr=subprocess.STDOUT if console_log_file is not None else None,
        )
    except OSError as error:
        print(f"capture_ui_test: failed to launch {exe_path}: {error}", file=sys.stderr)
        if console_log_file is not None:
            console_log_file.close()
        if fixture_root is not None:
            shutil.rmtree(fixture_root, ignore_errors=True)
        return 2

    deadline = time.monotonic() + args.timeout_seconds
    previous_size = -1
    stable_observations = 0
    renderer_previous_size = -1
    renderer_stable_observations = 0
    try:
        while time.monotonic() < deadline:
            if output_path.is_file():
                size = output_path.stat().st_size
                if size > 0 and size == previous_size:
                    stable_observations += 1
                    if stable_observations >= 2:
                        if renderer_output_path is not None:
                            if not renderer_output_path.is_file():
                                pass
                            else:
                                renderer_size = renderer_output_path.stat().st_size
                                if (renderer_size > 0 and
                                        renderer_size == renderer_previous_size):
                                    renderer_stable_observations += 1
                                else:
                                    renderer_stable_observations = 0
                                renderer_previous_size = renderer_size
                            if renderer_stable_observations < 2:
                                time.sleep(0.1)
                                continue
                        if cloner_route_path is not None:
                            if not cloner_route_path.is_file():
                                time.sleep(0.1)
                                continue
                            if (cloner_fixture_path is None or
                                    not cloner_fixture_path.is_file()):
                                print(
                                    "capture_ui_test: Cloner fixture descriptor "
                                    "marker missing; executable may not support "
                                    "the requested fixture",
                                    file=sys.stderr,
                                )
                                return 1
                            fixture_text = cloner_fixture_path.read_text(
                                encoding="utf-8"
                            )
                            fixture_fields = dict(
                                field.split("=", 1)
                                for field in fixture_text.split()
                                if "=" in field
                            )
                            expected_generator = {
                                "grid": "artifact.generator.cloner.grid",
                                "matrix": "artifact.generator.cloner.matrix",
                                "linear": "artifact.generator.cloner.linear",
                                "radial": "artifact.generator.cloner.radial",
                                "random": "artifact.generator.cloner.random",
                            }[args.cloner_distribution]
                            if (fixture_fields.get("generator") !=
                                    expected_generator or
                                    fixture_fields.get("generatorCount") != "1"):
                                print(
                                    "capture_ui_test: Cloner fixture descriptor "
                                    "mismatch: "
                                    f"{fixture_text.strip()} "
                                    f"expected generator={expected_generator} "
                                    "generatorCount=1",
                                    file=sys.stderr,
                                )
                                return 1
                            if args.cloner_plain_chain:
                                try:
                                    chain_values = tuple(
                                        float(fixture_fields[key])
                                        for key in (
                                            "plain0RotationZ", "plain0ScaleX",
                                            "plain0ScaleY", "plain1TranslationX",
                                            "plain1TranslationY", "plain1TranslationZ",
                                            "plain1RotationZ", "plain1ScaleX",
                                            "plain1ScaleY",
                                        )
                                    )
                                except (KeyError, ValueError):
                                    chain_values = ()
                                if (
                                    fixture_fields.get("modifier") != "plain-chain" or
                                    fixture_fields.get("plainCount") != "2" or
                                    chain_values !=
                                    (15.0, 1.2, 1.2, 9.0, -6.0, 0.0, 0.0,
                                     1.5, 0.75)
                                ):
                                    print(
                                        "capture_ui_test: Cloner Plain chain fixture "
                                        "mismatch: "
                                        f"{fixture_text.strip()}",
                                        file=sys.stderr,
                                    )
                                    return 1
                            elif (args.cloner_plain_translation or
                                  args.cloner_plain_rotation or
                                  args.cloner_plain_scale):
                                try:
                                    modifier_translation = tuple(
                                        float(fixture_fields[key])
                                        for key in (
                                            "plainTranslationX",
                                            "plainTranslationY",
                                            "plainTranslationZ",
                                        )
                                    )
                                    modifier_rotation = float(
                                        fixture_fields["plainRotationZ"]
                                    )
                                    modifier_scale = tuple(
                                        float(fixture_fields[key])
                                        for key in ("plainScaleX", "plainScaleY")
                                    )
                                except (KeyError, ValueError):
                                    modifier_translation = ()
                                    modifier_rotation = float("nan")
                                    modifier_scale = ()
                                expected_translation = (
                                    (9.0, -6.0, 0.0)
                                    if args.cloner_plain_translation else
                                    (0.0, 0.0, 0.0)
                                )
                                expected_rotation = (
                                    15.0 if args.cloner_plain_rotation else 0.0
                                )
                                expected_scale = (
                                    (1.5, 0.75) if args.cloner_plain_scale else
                                    (1.0, 1.0)
                                )
                                if (
                                    fixture_fields.get("modifier") != "plain" or
                                    modifier_translation != expected_translation or
                                    abs(modifier_rotation - expected_rotation) > 1.0e-5 or
                                    modifier_scale != expected_scale
                                ):
                                    print(
                                        "capture_ui_test: Cloner Plain modifier "
                                        "fixture mismatch: "
                                        f"{fixture_text.strip()}",
                                        file=sys.stderr,
                                    )
                                    return 1
                            if cloner_stack_fixture_path is not None:
                                if not cloner_stack_fixture_path.is_file():
                                    time.sleep(0.1)
                                    continue
                                stack_fixture = dict(
                                    field.split("=", 1)
                                    for field in cloner_stack_fixture_path
                                    .read_text(encoding="utf-8")
                                    .split()
                                    if "=" in field
                                )
                                if stack_fixture != {
                                        "generator": args.cloner_distribution,
                                        "entries": "2",
                                        "translationA": "13,-5,2",
                                        "translationB": "-4,9,1",
                                        "scaleB": "1.5,0.75",
                                        "rotationTotal": "15",
                                }:
                                    print(
                                        "capture_ui_test: Cloner transform-stack "
                                        "fixture marker mismatch: "
                                        f"{stack_fixture}",
                                        file=sys.stderr,
                                    )
                                    return 1
                            route = cloner_route_path.read_text(encoding="utf-8")
                            fields = dict(
                                field.split("=", 1)
                                for field in route.split()
                                if "=" in field
                            )
                            if args.cloner_backend == "gpu":
                                if args.cloner_expect_cpu_fallback:
                                    valid_route = (
                                        fields.get("gpuReady") == "1"
                                        and fields.get("placementHandled") == "0"
                                        and fields.get("gpuDraws") == "0"
                                        and int(fields.get("cpuFallbackDraws", "0")) > 0
                                    )
                                else:
                                    expected_draws = (
                                        {"grid": "9", "matrix": "9", "linear": "4", "radial": "5"}[
                                            args.cloner_distribution
                                        ]
                                    )
                                    valid_route = (
                                        fields.get("gpuReady") == "1"
                                        and fields.get("placementHandled") == "1"
                                        and fields.get("gpuDraws") == expected_draws
                                        and fields.get("cpuFallbackDraws") == "0"
                                    )
                            else:
                                valid_route = fields.get("gpuReady") == "0"
                            if not valid_route:
                                print(
                                    "capture_ui_test: Cloner route validation failed: "
                                    f"{route.strip()}",
                                    file=sys.stderr,
                                )
                                return 1
                            if args.cloner_distribution == "linear":
                                weight_path = Path(
                                    f"{cloner_route_path}.weights.txt"
                                )
                                if not weight_path.is_file():
                                    time.sleep(0.1)
                                    continue
                                try:
                                    actual_weights = [
                                        float(value)
                                        for value in weight_path.read_text(
                                            encoding="utf-8"
                                        ).split(",")
                                        if value.strip()
                                    ]
                                except ValueError:
                                    actual_weights = []
                                decay = args.cloner_opacity_decay
                                if not math.isfinite(decay):
                                    decay = 0.0
                                decay = min(max(decay, 0.0), 1.0)
                                expected_weights = [1.0] + [
                                    min(max(1.0 - decay * index, 0.0), 1.0)
                                    for index in range(3)
                                ]
                                valid_weights = (
                                    len(actual_weights) == len(expected_weights)
                                    and all(
                                        abs(actual - expected) <= 0.001
                                        for actual, expected in zip(
                                            actual_weights, expected_weights
                                        )
                                    )
                                )
                                if not valid_weights:
                                    print(
                                        "capture_ui_test: Linear Cloner weight "
                                        f"validation failed: {actual_weights}",
                                        file=sys.stderr,
                                    )
                                    return 1
                        if not args.tracker:
                            print(output_path)
                            if renderer_output_path is not None:
                                print(renderer_output_path)
                            if cloner_route_path is not None:
                                print(cloner_route_path)
                            return 0
                        if tracker_result_path.is_file():
                            result = tracker_result_path.read_text(
                                encoding="utf-8"
                            )
                            if not result.startswith("success "):
                                print(
                                    "capture_ui_test: Tracker runtime failed: "
                                    f"{result}",
                                    file=sys.stderr,
                                )
                                return 1
                            print(f"{output_path} ({result})")
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
        if console_log_file is not None:
            console_log_file.flush()
            console_log_file.close()
        if fixture_root is not None:
            if args.contents_viewer is not None:
                diagnostics_path = Path(f"{output_path}.diagnostics.txt")
                appdata_root = fixture_root / "appdata"
                log_paths = sorted(appdata_root.rglob("*.log"))
                with diagnostics_path.open("w", encoding="utf-8", newline="\n") as report:
                    if not log_paths:
                        report.write("No application log files were written under fixture APPDATA.\n")
                    for log_path in log_paths:
                        report.write(f"--- {log_path.relative_to(appdata_root)} ---\n")
                        report.write(log_path.read_text(encoding="utf-8", errors="replace"))
                        report.write("\n")
                    if console_log_path is not None and console_log_path.is_file():
                        report.write("--- process stdout/stderr ---\n")
                        report.write(console_log_path.read_text(
                            encoding="utf-8", errors="replace"
                        ))
            shutil.rmtree(fixture_root, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())
