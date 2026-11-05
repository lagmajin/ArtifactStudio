#!/usr/bin/env python3
"""Export an animated complex-script title through Artifact's render queue.

Requires an OpenCV Python package with video decoding enabled. The app renders
and encodes the video; OpenCV then checks decoded frames for visible text,
clipping, and time-varying glyph imagery.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time


class VideoAnalysisError(RuntimeError):
    def __init__(self, message: str, diagnostics: dict[str, object]):
        super().__init__(message)
        self.diagnostics = diagnostics


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", required=True, type=Path,
                        help="ArtifactUiTest.exe path")
    parser.add_argument("--output-dir", required=True, type=Path,
                        help="directory for the MP4 and visual-analysis artifacts")
    parser.add_argument("--timeout-seconds", type=float, default=300.0,
                        help="maximum time for render queue export (default: 300)")
    parser.add_argument("--backend", choices=("gpu", "cpu"), default="gpu",
                        help="render queue backend (default: gpu)")
    parser.add_argument("--encoder", choices=("auto", "native", "pipe"),
                        default="auto", help="video encoder backend")
    return parser.parse_args()


def analyze_video(video_path: Path, artifact_dir: Path) -> dict[str, object]:
    try:
        import cv2
        import numpy as np
    except ImportError as error:
        raise RuntimeError(
            "image analysis requires Python packages opencv-python and numpy"
        ) from error

    capture = cv2.VideoCapture(str(video_path))
    if not capture.isOpened():
        raise RuntimeError(f"OpenCV could not decode the MP4: {video_path}")
    width = int(capture.get(cv2.CAP_PROP_FRAME_WIDTH))
    height = int(capture.get(cv2.CAP_PROP_FRAME_HEIGHT))
    fps = float(capture.get(cv2.CAP_PROP_FPS))
    frame_count = int(capture.get(cv2.CAP_PROP_FRAME_COUNT))
    fourcc = int(capture.get(cv2.CAP_PROP_FOURCC))
    codec = "".join(chr((fourcc >> (8 * index)) & 0xFF) for index in range(4))
    if width != 640 or height != 360:
        capture.release()
        raise RuntimeError(f"unexpected video dimensions: {width}x{height}")
    if not np.isfinite(fps) or abs(fps - 12.0) > 0.05 or frame_count != 24:
        capture.release()
        raise RuntimeError(
            f"unexpected video timing: fps={fps:.3f}, frames={frame_count}"
        )
    if codec.lower() not in {"avc1", "h264", "x264"}:
        capture.release()
        raise RuntimeError(f"expected H.264 MP4 video, decoded fourcc={codec!r}")

    sample_indices = sorted({
        round(index * (frame_count - 1) / 6) for index in range(7)
    })
    frames: list[tuple[int, object]] = []
    for target in sample_indices:
        capture.set(cv2.CAP_PROP_POS_FRAMES, target)
        ok, frame = capture.read()
        if not ok or frame is None:
            capture.release()
            raise RuntimeError(f"could not decode sampled frame {target}")
        frames.append((target, frame))
    capture.release()

    artifact_dir.mkdir(parents=True, exist_ok=True)
    observations: list[dict[str, object]] = []
    foreground_masks = []
    for ordinal, (frame_index, frame) in enumerate(frames):
        corner_pixels = np.concatenate((
            frame[:12, :12].reshape(-1, 3),
            frame[:12, -12:].reshape(-1, 3),
            frame[-12:, :12].reshape(-1, 3),
            frame[-12:, -12:].reshape(-1, 3),
        ), axis=0)
        background = np.median(corner_pixels, axis=0)
        color_distance = np.linalg.norm(frame.astype(np.float32) - background, axis=2)
        mask = (color_distance > 42).astype(np.uint8)
        active_rows = np.flatnonzero(mask.sum(axis=1) >= 3)
        line_bands: list[list[int]] = []
        for row in active_rows:
            row_index = int(row)
            if line_bands and row_index - line_bands[-1][1] <= 6:
                line_bands[-1][1] = row_index
            else:
                line_bands.append([row_index, row_index])
        if len(line_bands) != 3:
            raise RuntimeError(
                f"frame {frame_index} should show three separate text lines; "
                f"vision found {len(line_bands)} bands: {line_bands}"
            )
        ys, xs = np.where(mask != 0)
        if xs.size < 180:
            raise RuntimeError(
                f"frame {frame_index} contains too little detected text "
                f"({xs.size} foreground pixels)"
            )
        x0, x1 = int(xs.min()), int(xs.max())
        y0, y1 = int(ys.min()), int(ys.max())
        if x0 < 2 or y0 < 2 or x1 >= width - 2 or y1 >= height - 2:
            raise RuntimeError(
                f"detected text touches the output edge in frame {frame_index}: "
                f"bbox=({x0},{y0},{x1},{y1})"
            )
        frame_path = artifact_dir / f"sample_{ordinal:02d}_frame_{frame_index:04d}.png"
        if not cv2.imwrite(str(frame_path), frame):
            raise RuntimeError(f"could not write diagnostic frame {frame_path}")
        foreground_masks.append(mask)
        observations.append({
            "frame": frame_index,
            "foreground_pixels": int(xs.size),
            "bbox": [x0, y0, x1, y1],
            "text_line_bands": line_bands,
            "sample_image": frame_path.name,
        })

    changes: list[dict[str, object]] = []
    visible_pairs_by_line = [0, 0, 0]
    motion_kernel = np.ones((11, 11), dtype=np.uint8)
    for index, ((left_index, left), (right_index, right)) in enumerate(
            zip(frames, frames[1:])):
        delta = cv2.absdiff(left, right)
        gray_delta = cv2.cvtColor(delta, cv2.COLOR_BGR2GRAY)
        text_roi = cv2.dilate(
            np.maximum(foreground_masks[index], foreground_masks[index + 1]),
            motion_kernel,
        )
        changed_mask = (gray_delta > 24) & (text_roi != 0)
        changed_pixels = int(np.count_nonzero(changed_mask))
        changed_pixels_by_line = []
        line_bands = observations[index]["text_line_bands"]
        for line_index, (y0, y1) in enumerate(line_bands):
            region_top = max(0, int(y0) - 12)
            region_bottom = min(height, int(y1) + 13)
            line_changed_pixels = int(np.count_nonzero(
                changed_mask[region_top:region_bottom, :]
            ))
            changed_pixels_by_line.append(line_changed_pixels)
            if line_changed_pixels >= 30:
                visible_pairs_by_line[line_index] += 1
        changes.append({
            "from_frame": left_index,
            "to_frame": right_index,
            "changed_pixels": changed_pixels,
            "changed_pixels_by_line": changed_pixels_by_line,
        })
    changed_pairs = sum(item["changed_pixels"] >= 50 for item in changes)
    motion_diagnostics = {
        "sampled_frames": observations,
        "frame_changes": changes,
        "visible_animation_pairs": changed_pairs,
        "visible_animation_pairs_by_line": visible_pairs_by_line,
    }
    if changed_pairs < 4:
        raise VideoAnalysisError(
            "sampled video frames do not show sustained animation "
            f"(only {changed_pairs}/6 frame pairs changed visibly)",
            motion_diagnostics,
        )
    if any(pair_count < 4 for pair_count in visible_pairs_by_line):
        raise VideoAnalysisError(
            "each complex-script line should animate visibly; "
            f"visible frame pairs by line={visible_pairs_by_line}",
            motion_diagnostics,
        )

    return {
        "status": "passed",
        "video": str(video_path),
        "codec_fourcc": codec,
        "width": width,
        "height": height,
        "fps": fps,
        "decoded_frames": frame_count,
        "sampled_frames": observations,
        "frame_changes": changes,
        "visible_animation_pairs": changed_pairs,
        "visible_animation_pairs_by_line": visible_pairs_by_line,
        "checks": [
            "MP4 decodes with the expected dimensions and frame rate",
            "all three complex-script text lines are visually present",
            "detected text stays inside the video frame",
            "image differences inside the text region show sustained animation",
        ],
    }


def main() -> int:
    args = parse_args()
    try:
        import cv2  # noqa: F401
        import numpy  # noqa: F401
    except ImportError as error:
        print("test_text_animation_video: image analysis requires "
              "opencv-python and numpy", file=sys.stderr)
        return 2
    exe_path = args.exe.expanduser().resolve()
    if not exe_path.is_file():
        print(f"test_text_animation_video: executable not found: {exe_path}",
              file=sys.stderr)
        return 2

    run_name = f"text-animation-video-{time.strftime('%Y%m%d-%H%M%S')}-{os.getpid()}"
    artifact_dir = args.output_dir.expanduser().resolve() / run_name
    artifact_dir.mkdir(parents=True, exist_ok=False)
    video_path = artifact_dir / "complex_script_animation.mp4"
    result_path = artifact_dir / "app_result.json"
    range_report_path = artifact_dir / "resolved_render_range.json"
    encoded_frame_log_path = artifact_dir / "encoded_frame_indices.txt"
    encoder_report_path = artifact_dir / "encoder_report.json"
    app_log_path = artifact_dir / "app.log"
    report_path = artifact_dir / "visual_analysis.json"
    project_root = artifact_dir / "project"
    appdata_root = artifact_dir / "appdata"
    project_root.mkdir()
    appdata_root.mkdir()

    environment = os.environ.copy()
    for fixture_flag in (
        "ARTIFACT_UI_TEST_TIMELINE",
        "ARTIFACT_UI_TEST_CURVE_EDITOR",
        "ARTIFACT_UI_TEST_RENDER_MANAGER",
        "ARTIFACT_UI_TEST_CONTENTS_VIEWER",
        "ARTIFACT_UI_TEST_CONTENTS_VIEWER_PATH",
        "ARTIFACT_UI_TEST_CLONER",
        "ARTIFACT_UI_TEST_COMPONENTS",
        "ARTIFACT_UI_TEST_COMPONENT",
        "ARTIFACT_UI_TEST_COMPONENTS_RESULT_PATH",
        "ARTIFACT_UI_TEST_COMPONENTS_NO_WINDOW",
        "ARTIFACT_UI_TEST_TRACKER",
    ):
        environment.pop(fixture_flag, None)
    environment["ARTIFACT_UI_TEST_TEXT_ANIMATION_VIDEO"] = "1"
    environment["ARTIFACT_UI_TEST_TEXT_ANIMATION_VIDEO_RANGE_REPORT"] = str(
        range_report_path)
    environment["ARTIFACT_UI_TEST_TEXT_ANIMATION_VIDEO_FRAME_LOG"] = str(
        encoded_frame_log_path)
    environment["ARTIFACT_UI_TEST_TEXT_ANIMATION_VIDEO_ENCODER_REPORT"] = str(
        encoder_report_path)
    environment["ARTIFACT_UI_TEST_TEXT_ANIMATION_VIDEO_BACKEND"] = args.backend
    environment["ARTIFACT_UI_TEST_TEXT_ANIMATION_VIDEO_ENCODER"] = args.encoder
    environment["ARTIFACT_UI_TEST_TEXT_ANIMATION_VIDEO_OUTPUT"] = str(video_path)
    environment["ARTIFACT_UI_TEST_TEXT_ANIMATION_VIDEO_RESULT"] = str(result_path)
    environment["ARTIFACT_UI_TEST_FORCE_SHUTDOWN"] = "1"
    environment["ARTIFACT_UI_TEST_PROJECT_ROOT"] = str(project_root)
    environment["APPDATA"] = str(appdata_root)
    environment["LOCALAPPDATA"] = str(appdata_root)
    try:
        completed = subprocess.run(
            [str(exe_path), "--lang", "en"],
            cwd=exe_path.parent,
            env=environment,
            capture_output=True,
            text=True,
            timeout=args.timeout_seconds,
            check=False,
        )
    except subprocess.TimeoutExpired as error:
        app_log_path.write_text(
            (error.stdout or "") + "\n--- stderr ---\n" + (error.stderr or ""),
            encoding="utf-8", errors="replace")
        print(f"test_text_animation_video: app timed out after "
              f"{args.timeout_seconds:.0f}s; artifacts: {artifact_dir}",
              file=sys.stderr)
        if error.stdout:
            print(error.stdout[-6000:], file=sys.stderr)
        if error.stderr:
            print(error.stderr[-6000:], file=sys.stderr)
        return 1

    app_log_path.write_text(
        completed.stdout + "\n--- stderr ---\n" + completed.stderr,
        encoding="utf-8", errors="replace")

    if completed.returncode != 0:
        print(f"test_text_animation_video: app exited {completed.returncode}; "
              f"artifacts: {artifact_dir}", file=sys.stderr)
        print(completed.stdout[-6000:], file=sys.stderr)
        print(completed.stderr[-6000:], file=sys.stderr)
        return 1
    if not result_path.is_file():
        print("test_text_animation_video: app did not write its result marker; "
              f"artifacts: {artifact_dir}", file=sys.stderr)
        print(completed.stdout[-6000:], file=sys.stderr)
        print(completed.stderr[-6000:], file=sys.stderr)
        return 1

    app_result = json.loads(result_path.read_text(encoding="utf-8"))
    if range_report_path.is_file():
        app_result["resolved_render_range"] = json.loads(
            range_report_path.read_text(encoding="utf-8"))
    if encoded_frame_log_path.is_file():
        app_result["encoded_frame_indices"] = [
            int(line) for line in encoded_frame_log_path.read_text(
                encoding="utf-8").splitlines() if line.strip()]
    if encoder_report_path.is_file():
        app_result["encoder_report"] = json.loads(
            encoder_report_path.read_text(encoding="utf-8"))
    if app_result.get("status") != "passed" or not video_path.is_file():
        print(f"test_text_animation_video: render queue failed: {app_result}; "
              f"artifacts: {artifact_dir}", file=sys.stderr)
        print(completed.stderr[-6000:], file=sys.stderr)
        return 1

    try:
        expected_stack = [
            "position", "rotation", "opacity", "scale", "tracking", "skew"
        ]
        if (app_result.get("animator_count") != len(expected_stack) or
                app_result.get("animation_stack") != expected_stack):
            raise RuntimeError(
                "fixture did not report the expected six-animator stack: "
                f"count={app_result.get('animator_count')}, "
                f"stack={app_result.get('animation_stack')}"
            )
        selector_overview = app_result.get("selector_overview", "")
        required_selector_states = (
            "unit=cluster", "lineAware=true", "regex=on", "expr=on",
            "tag=mixed",
        )
        missing_selector_states = [
            state for state in required_selector_states
            if state not in selector_overview
        ]
        if missing_selector_states:
            raise RuntimeError(
                "fixture selector stack is incomplete; missing summary states: "
                f"{missing_selector_states}; overview={selector_overview!r}"
            )
        report = analyze_video(video_path, artifact_dir)
    except RuntimeError as error:
        report = {"status": "failed", "reason": str(error),
                  "video": str(video_path), "app_result": app_result}
        if isinstance(error, VideoAnalysisError):
            report["diagnostics"] = error.diagnostics
        report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(f"test_text_animation_video: {error}; artifacts: {artifact_dir}",
              file=sys.stderr)
        return 1
    report["app_result"] = app_result
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"Text animation video export passed. Report: {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
