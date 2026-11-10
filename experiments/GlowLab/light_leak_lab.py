"""Deterministic still-image R&D harness for photographic light leaks."""

from __future__ import annotations

import argparse
import math
from pathlib import Path

import cv2
import numpy as np

from glow_lab import add_label, linear_to_srgb, srgb_to_linear, to_u8


def make_fixture(width: int = 960, height: int = 540) -> np.ndarray:
    """Create a muted dusk landscape with smooth gradients and fine detail."""
    y, x = np.mgrid[0:height, 0:width]
    vertical = y.astype(np.float32) / float(height - 1)
    top = np.asarray((0.10, 0.13, 0.23), dtype=np.float32)
    horizon = np.asarray((0.68, 0.39, 0.30), dtype=np.float32)
    lower = np.asarray((0.10, 0.17, 0.24), dtype=np.float32)
    sky_mix = np.clip(vertical / 0.64, 0.0, 1.0)[..., None]
    sky = top[None, None, :] * (1.0 - sky_mix) + horizon[None, None, :] * sky_mix
    ground_mix = np.clip((vertical - 0.64) / 0.36, 0.0, 1.0)[..., None]
    image = sky * (1.0 - ground_mix) + lower[None, None, :] * ground_mix
    image = np.broadcast_to(image, (height, width, 3)).copy()

    sun = np.zeros((height, width), dtype=np.float32)
    cv2.circle(sun, (735, 285), 42, 1.0, -1, cv2.LINE_AA)
    sun = cv2.GaussianBlur(sun, (0, 0), 1.4)
    image += sun[..., None] * np.asarray((0.92, 0.72, 0.48), dtype=np.float32)[None, None, :] * 0.75

    mountains = np.asarray([(0, 360), (115, 300), (230, 365), (375, 295), (520, 360),
                            (680, 318), (825, 370), (960, 305), (960, 540), (0, 540)],
                           dtype=np.int32)
    cv2.fillPoly(image, [mountains], (0.10, 0.12, 0.17), lineType=cv2.LINE_AA)
    cv2.line(image, (0, 400), (960, 400), (0.37, 0.26, 0.22), 2, cv2.LINE_AA)

    # Quiet foreground silhouettes and lettering make contrast changes legible.
    cv2.ellipse(image, (174, 434), (40, 13), 0, 0, 360, (0.035, 0.060, 0.078), -1, cv2.LINE_AA)
    cv2.circle(image, (170, 382), 8, (0.035, 0.060, 0.078), -1, cv2.LINE_AA)
    cv2.line(image, (170, 391), (176, 425), (0.035, 0.060, 0.078), 8, cv2.LINE_AA)
    cv2.line(image, (174, 401), (158, 414), (0.035, 0.060, 0.078), 5, cv2.LINE_AA)
    cv2.line(image, (174, 401), (190, 412), (0.035, 0.060, 0.078), 5, cv2.LINE_AA)
    cv2.putText(image, "LAST LIGHT", (370, 480), cv2.FONT_HERSHEY_SIMPLEX, 0.72,
                (0.84, 0.82, 0.78), 1, cv2.LINE_AA)
    cv2.putText(image, "35MM  /  FRAME 018", (370, 501), cv2.FONT_HERSHEY_SIMPLEX, 0.38,
                (0.64, 0.67, 0.72), 1, cv2.LINE_AA)
    grain = np.random.default_rng(17).normal(0.0, 0.0025, image.shape[:2]).astype(np.float32)
    image += grain[..., None]
    return np.dstack((np.clip(image, 0.0, 1.0), np.ones((height, width), dtype=np.float32)))


def apply_screen(rgba: np.ndarray, leak_linear: np.ndarray, amount: float = 0.72) -> np.ndarray:
    source = srgb_to_linear(np.maximum(rgba[..., :3], 0.0))
    leaked = 1.0 - (1.0 - source) * np.exp(-np.maximum(leak_linear, 0.0) * amount)
    return np.dstack((np.clip(linear_to_srgb(leaked), 0.0, 1.0), rgba[..., 3]))


def edge_wash(rgba: np.ndarray, strength: float = 1.0) -> np.ndarray:
    h, w = rgba.shape[:2]
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    xn, yn = x / (w - 1), y / (h - 1)
    top_left = np.exp(-((xn / 0.34) ** 2 + (yn / 0.82) ** 2) * 1.7)
    top_edge = np.exp(-((yn / 0.19) ** 2 + ((xn - 0.24) / 0.56) ** 2) * 1.5)
    side = np.exp(-(((1.0 - xn) / 0.16) ** 2 + ((yn - 0.23) / 0.58) ** 2) * 1.4)
    warm = top_left * 0.85 + top_edge * 0.46
    leak = np.stack((warm * 0.80 + side * 0.12,
                     warm * 0.36 + side * 0.12,
                     warm * 0.10 + side * 0.32), axis=-1)
    return apply_screen(rgba, leak, 0.8 * max(0.0, strength))


def anamorphic_streak(rgba: np.ndarray, strength: float = 1.0,
                       source_position: tuple[float, float] = (0.01, 0.105),
                       angle_degrees: float = math.degrees(0.045)) -> np.ndarray:
    h, w = rgba.shape[:2]
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    xn, yn = x / w, y / h
    theta = math.radians(float(np.clip(angle_degrees, -45.0, 45.0)))
    source_x, source_y = np.clip(np.asarray(source_position, dtype=np.float32), 0.0, 1.0)
    dx, dy = xn - source_x + 0.03, yn - source_y
    along = np.cos(theta) * dx + np.sin(theta) * dy
    across = -np.sin(theta) * dx + np.cos(theta) * dy
    beam = np.exp(-0.5 * ((along / 0.78) ** 2 + (across / 0.018) ** 2))
    source = np.exp(-0.5 * (((xn - source_x) / 0.11) ** 2 +
                            ((yn - source_y) / 0.15) ** 2))
    red_streak = np.exp(-0.5 * ((along / 0.78) ** 2 + ((across + 0.008) / 0.022) ** 2))
    blue_streak = np.exp(-0.5 * ((along / 0.78) ** 2 + ((across - 0.010) / 0.026) ** 2))
    leak = np.stack((beam * 0.86 + red_streak * 0.30 + source * 0.86,
                     beam * 0.68 + source * 0.58,
                     beam * 0.50 + blue_streak * 0.38 + source * 0.38), axis=-1)
    return apply_screen(rgba, leak, 0.8 * max(0.0, strength))


def organic_gate_leak(rgba: np.ndarray, strength: float = 1.0) -> np.ndarray:
    h, w = rgba.shape[:2]
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    xn, yn = x / (w - 1), y / (h - 1)

    def lobe(cx: float, cy: float, rx: float, ry: float, weight: float) -> np.ndarray:
        return weight * np.exp(-0.5 * (((xn - cx) / rx) ** 2 + ((yn - cy) / ry) ** 2))

    edge = np.exp(-((1.0 - yn) / 0.30) ** 2)
    pooling = 0.30 + 0.70 * np.sin(2.0 * np.pi * (xn * 0.72 + 0.08))
    amber = (edge * pooling * 0.50 + lobe(0.91, 0.78, 0.23, 0.30, 0.65) +
             lobe(0.60, 0.98, 0.33, 0.12, 0.40) +
             lobe(0.06, 0.66, 0.12, 0.36, 0.42))
    magenta = lobe(0.06, 0.48, 0.20, 0.34, 0.70) + lobe(0.28, 0.92, 0.25, 0.13, 0.18)
    blue = lobe(0.98, 0.38, 0.08, 0.27, 0.35)
    leak = np.stack((amber * 0.92 + magenta * 0.46,
                     amber * 0.48 + magenta * 0.16 + blue * 0.10,
                     amber * 0.20 + magenta * 0.58 + blue * 0.48), axis=-1)
    return apply_screen(rgba, leak, 0.34 * max(0.0, strength))


def resize_fixture(rgba: np.ndarray, width: int, height: int) -> np.ndarray:
    """Resize the same scene while retaining deterministic pixel sampling."""
    return cv2.resize(rgba, (width, height), interpolation=cv2.INTER_AREA)


def regional_metrics(source: np.ndarray, result: np.ndarray) -> dict[str, float]:
    """Report leak response in broad spatial regions, not whole-frame mean."""
    source_linear = srgb_to_linear(np.maximum(source[..., :3], 0.0))
    result_linear = srgb_to_linear(np.maximum(result[..., :3], 0.0))
    delta = result_linear - source_linear
    h, w = source.shape[:2]
    y, x = np.mgrid[0:h, 0:w]
    xn, yn = x / max(w - 1, 1), y / max(h - 1, 1)
    masks = {
        "top_left": (xn < 0.25) & (yn < 0.35),
        "center": (xn > 0.35) & (xn < 0.65) & (yn > 0.30) & (yn < 0.70),
        "bottom_right": (xn > 0.75) & (yn > 0.70),
    }
    return {name: float(delta[mask].mean()) for name, mask in masks.items()}


def load_input(path: Path, frame_index: int) -> np.ndarray:
    """Read a still or one deterministic frame from a video as float RGBA."""
    suffix = path.suffix.lower()
    if suffix in {".mp4", ".mov", ".mkv", ".avi", ".webm"}:
        capture = cv2.VideoCapture(str(path))
        if not capture.isOpened():
            raise RuntimeError(f"Could not open video: {path}")
        capture.set(cv2.CAP_PROP_POS_FRAMES, max(0, frame_index))
        ok, frame = capture.read()
        capture.release()
        if not ok:
            raise RuntimeError(f"Could not read frame {frame_index} from {path}")
        rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
        return np.dstack((rgb, np.ones(rgb.shape[:2], dtype=np.float32)))

    frame = cv2.imread(str(path), cv2.IMREAD_UNCHANGED)
    if frame is None:
        raise RuntimeError(f"Could not read image: {path}")
    scale = (255.0 if frame.dtype == np.uint8 else
             65535.0 if frame.dtype == np.uint16 else 1.0)
    if frame.ndim == 2:
        rgb = cv2.cvtColor(frame, cv2.COLOR_GRAY2RGB)
        alpha = np.ones(frame.shape, dtype=np.float32)
    elif frame.shape[2] == 4:
        rgba = cv2.cvtColor(frame, cv2.COLOR_BGRA2RGBA).astype(np.float32)
        rgba /= scale
        return rgba
    else:
        rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        alpha = np.ones(frame.shape[:2], dtype=np.float32)
    rgb = rgb.astype(np.float32) / scale
    return np.dstack((rgb, alpha))


def run(output_dir: Path, input_path: Path | None = None, frame_index: int = 20,
        strength: float = 1.0,
        streak_position: tuple[float, float] = (0.01, 0.105),
        streak_angle_degrees: float = math.degrees(0.045),
        streak_angle_sweep: tuple[float, ...] = ()) -> None:
    if not np.isfinite(strength) or strength <= 0.0:
        raise ValueError("strength must be a finite value greater than zero")
    output_dir.mkdir(parents=True, exist_ok=True)
    fixture = load_input(input_path, frame_index) if input_path else make_fixture()
    fixture = np.nan_to_num(fixture, nan=0.0, posinf=1.0, neginf=0.0)
    fixture = np.clip(fixture, 0.0, 1.0).astype(np.float32, copy=False)
    looks = [
        ("edge-wash", "EDGE WASH", edge_wash(fixture, strength)),
        ("anamorphic-streak", "STREAK",
         anamorphic_streak(fixture, strength, streak_position, streak_angle_degrees)),
        ("organic-gate-leak", "ORGANIC", organic_gate_leak(fixture, strength)),
    ]
    assert np.isfinite(fixture).all()
    for name, _label, result in looks:
        assert np.isfinite(result).all(), f"{name} produced non-finite pixels"
        assert np.array_equal(result[..., 3], fixture[..., 3]), f"{name} changed alpha"
        assert float(result[..., :3].mean()) > float(fixture[..., :3].mean()), f"{name} added no light"
        cv2.imwrite(str(output_dir / f"{name}.png"),
                    cv2.cvtColor(to_u8(result), cv2.COLOR_RGBA2BGRA))

    if streak_angle_sweep:
        angle_panels = [add_label(to_u8(fixture), "SOURCE")]
        for angle in streak_angle_sweep:
            result = anamorphic_streak(fixture, strength, streak_position, angle)
            assert np.isfinite(result).all()
            assert np.array_equal(result[..., 3], fixture[..., 3])
            safe_angle = f"{angle:+.1f}".replace("+", "p").replace("-", "m").replace(".", "_")
            cv2.imwrite(str(output_dir / f"streak-angle-{safe_angle}.png"),
                        cv2.cvtColor(to_u8(result), cv2.COLOR_RGBA2BGRA))
            angle_panels.append(add_label(to_u8(result), f"STREAK {angle:+.1f} DEG"))
        angle_panels = [cv2.resize(panel, None, fx=0.5, fy=0.5,
                                   interpolation=cv2.INTER_AREA)
                        for panel in angle_panels]
        cv2.imwrite(str(output_dir / "streak-angle-sweep.png"),
                    cv2.cvtColor(np.vstack(angle_panels), cv2.COLOR_RGBA2BGRA))

    # Scale-normalized evidence: shape controls use normalized coordinates,
    # while a fixed seed and radius-free fields must remain comparable as the
    # same plate is rendered at odd and even dimensions.
    scales = ((960, 540), (641, 359), (320, 180))
    scaled_metrics: dict[str, dict[str, dict[str, float]]] = {}
    for width, height in scales:
        scaled_source = resize_fixture(fixture, width, height)
        scaled_metrics[f"{width}x{height}"] = {}
        for name, render in (("edge-wash", edge_wash),
                             ("anamorphic-streak", lambda image, amount: anamorphic_streak(
                                 image, amount, streak_position, streak_angle_degrees)),
                             ("organic-gate-leak", organic_gate_leak)):
            scaled_result = render(scaled_source, strength)
            assert np.isfinite(scaled_result).all()
            assert np.array_equal(scaled_result[..., 3], scaled_source[..., 3])
            scaled_metrics[f"{width}x{height}"][name] = regional_metrics(
                scaled_source, scaled_result)

    cv2.imwrite(str(output_dir / "light-leak-fixture.png"),
                cv2.cvtColor(to_u8(fixture), cv2.COLOR_RGBA2BGRA))
    panels = [add_label(to_u8(fixture), "SOURCE")] + [
        add_label(to_u8(result), label) for _name, label, result in looks
    ]
    panels = [cv2.resize(panel, None, fx=0.5, fy=0.5, interpolation=cv2.INTER_AREA)
              for panel in panels]
    sheet = np.vstack(panels)
    cv2.imwrite(str(output_dir / "light-leak-comparison.png"),
                cv2.cvtColor(sheet, cv2.COLOR_RGBA2BGRA))
    print(f"Saved deterministic light-leak captures to {output_dir.resolve()}")
    if input_path:
        print(f"Input: {input_path.resolve()} frame={frame_index}")
    print(f"Strength: {strength:.3f}")
    print(f"Streak source position: ({streak_position[0]:.3f}, {streak_position[1]:.3f})")
    print(f"Streak angle: {streak_angle_degrees:.3f} degrees")
    for name, _label, result in looks:
        print(f"mean RGB: {name}={result[..., :3].mean():.5f}")
    for resolution, model_metrics in scaled_metrics.items():
        for name, regions in model_metrics.items():
            print(f"linear leak Δ {resolution} {name}: " + ", ".join(
                f"{region}={value:.5f}" for region, value in regions.items()))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path,
                        help="Optional still image or video for a real-scene capture")
    parser.add_argument("--frame-index", type=int, default=20,
                        help="Video frame used when --input is a video")
    parser.add_argument("--strength", type=float, default=1.0,
                        help="Shared leak intensity multiplier")
    parser.add_argument("--streak-position", type=float, nargs=2, default=(0.01, 0.105),
                        metavar=("X", "Y"),
                        help="Normalized anamorphic streak source position (0..1)")
    parser.add_argument("--streak-angle-sweep", type=float, nargs="+", default=(),
                        metavar="DEGREES",
                        help="Also capture anamorphic streak at each angle (clamped to -45..45 degrees)")
    parser.add_argument("--streak-angle", type=float, default=math.degrees(0.045),
                        metavar="DEGREES",
                        help="Anamorphic streak axis angle (clamped to -45..45 degrees)")
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures")
    args = parser.parse_args()
    run(args.output, args.input, args.frame_index, args.strength,
        tuple(args.streak_position), args.streak_angle, tuple(args.streak_angle_sweep))
