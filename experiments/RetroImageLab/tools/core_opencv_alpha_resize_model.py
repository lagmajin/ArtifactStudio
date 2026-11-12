"""Model Core's default OpenCV linear resize under different alpha associations."""

from __future__ import annotations

import json
import sys
from pathlib import Path

import cv2
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / "fixtures" / "alpha-resize-edge-chart.png"
CAPTURE = ROOT / "captures" / "core-opencv-alpha-resize"
SCALES = (0.25, 0.375, 0.5, 0.75, 1.0)
HIDDEN_COLORS = {"green": (9, 240, 17), "magenta": (240, 9, 230)}
BACKGROUNDS = {"black": (0.0, 0.0, 0.0), "mid-gray": (0.5, 0.5, 0.5), "white": (1.0, 1.0, 1.0)}
ASSOCIATIONS = ("straight", "premultiplied")
DOMAINS = ("encoded values", "linear light")


def decode_srgb(x: np.ndarray) -> np.ndarray:
    return np.where(x <= 0.04045, x / 12.92, ((x + 0.055) / 1.055) ** 2.4)


def encode_srgb(x: np.ndarray) -> np.ndarray:
    x = np.clip(x, 0.0, 1.0)
    return np.where(x <= 0.0031308, x * 12.92, 1.055 * np.power(x, 1.0 / 2.4) - 0.055)


def make_variant(base: np.ndarray, hidden: tuple[int, int, int]) -> np.ndarray:
    rgba = base.astype(np.float32) / 255.0
    invisible = rgba[:, :, 3] == 0
    rgba[invisible, :3] = np.asarray(hidden, dtype=np.float32) / 255.0
    return rgba


def resize_rgba(source: np.ndarray, output_size: tuple[int, int], association: str, domain: str) -> np.ndarray:
    rgb, alpha = source[:, :, :3].copy(), source[:, :, 3:4].copy()
    if domain == "linear light":
        rgb = decode_srgb(rgb)
    if association == "premultiplied":
        rgb *= alpha
    work = np.concatenate((rgb, alpha), axis=2).astype(np.float32)
    resized = cv2.resize(work, output_size, interpolation=cv2.INTER_LINEAR)
    out_rgb, out_alpha = resized[:, :, :3], resized[:, :, 3:4]
    if association == "premultiplied":
        out_rgb = np.divide(out_rgb, out_alpha, out=np.zeros_like(out_rgb), where=out_alpha > 1e-8)
    if domain == "linear light":
        out_rgb = encode_srgb(out_rgb)
    return np.concatenate((np.clip(out_rgb, 0, 1), np.clip(out_alpha, 0, 1)), axis=2)


def composite(rgba: np.ndarray, background: tuple[float, float, float]) -> np.ndarray:
    rgb_linear = decode_srgb(rgba[:, :, :3])
    alpha = rgba[:, :, 3:4]
    bg = decode_srgb(np.asarray(background, dtype=np.float32))[None, None, :]
    return encode_srgb(rgb_linear * alpha + bg * (1 - alpha))


def summarize(a: np.ndarray, b: np.ndarray) -> dict[str, object]:
    delta = a[:, :, :3] - b[:, :, :3]
    output = {
        "alpha max absolute difference": float(np.max(np.abs(a[:, :, 3] - b[:, :, 3]))),
        "alpha changed pixel ratio >1e-6": float(np.mean(np.max(np.abs(a[:, :, 3:4] - b[:, :, 3:4]), axis=2) > 1e-6)),
        "RGB max absolute difference": float(np.max(np.abs(delta))),
    }
    for name, background in BACKGROUNDS.items():
        ca, cb = composite(a, background), composite(b, background)
        output[name + " composite sRGB RMSE"] = float(np.mean((ca - cb) ** 2) ** .5)
        output[name + " composite max sRGB difference"] = float(np.max(np.abs(ca - cb)))
    return output


def make_sheet(results: dict[str, np.ndarray], scale: str) -> Path:
    tile = 192
    keys = list(results)
    sheet = Image.new("RGB", (tile * 3, (tile + 20) * len(keys)), (28, 29, 33))
    from PIL import ImageDraw, ImageFont
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    for row, key in enumerate(keys):
        image = results[key]
        for col, bg in enumerate(((0, 0, 0, 255), (128, 128, 128, 255), (255, 255, 255, 255))):
            x, y = col * tile, row * (tile + 20)
            if col == 0:
                draw.text((x + 2, y + 3), key, fill=(245, 245, 245), font=font)
            panel = Image.fromarray(np.uint8(np.clip(image * 255 + .5, 0, 255)), "RGBA").resize((tile, tile), Image.Resampling.NEAREST)
            panel = Image.alpha_composite(Image.new("RGBA", panel.size, bg), panel)
            sheet.paste(panel.convert("RGB"), (x, y + 20))
    CAPTURE.mkdir(parents=True, exist_ok=True)
    path = CAPTURE / f"opencv-alpha-{scale}.png"
    sheet.save(path)
    return path


def main() -> dict[str, object]:
    with Image.open(FIXTURE) as image:
        base = np.asarray(image.convert("RGBA"), dtype=np.uint8)
    h, w = base.shape[:2]
    report: dict[str, object] = {
        "question": "How does the Core ImageF32x4_RGBA default OpenCV linear interpolation respond to transparent hidden RGB, alpha association and stored color domain?",
        "source basis": "ImageF32x4_RGBA::resize calls cv::resize(mat, resized, Size) without interpolation argument; this model uses cv2.INTER_LINEAR on CV_32FC4-like float arrays.",
        "method": "For each hidden RGB variant, resize the same alpha image to five output scales. Straight mode resizes RGBA channels directly. Premultiplied mode multiplies RGB by alpha before OpenCV resize, unassociates after. Compare encoded-sRGB-value interpolation with linear-light interpolation. Composite outputs in linear light on black/gray/white and measure sensitivity between inputs identical except RGB where alpha=0.",
        "limits": "Standalone Python OpenCV model, not execution of ArtifactCore or proof every caller uses these conventions. No descriptor transfer, true caller provenance, GPU parity, performance or human quality score. Input alpha is resampled in all conditions, matching the generic all-channel API shape.",
        "OpenCV version": cv2.__version__,
        "scales": {},
    }
    retained: dict[str, dict[str, np.ndarray]] = {}
    for scale in SCALES:
        target = (max(1, round(w * scale)), max(1, round(h * scale)))
        outputs: dict[str, np.ndarray] = {}
        condition_data = {}
        for domain in DOMAINS:
            for assoc in ASSOCIATIONS:
                a = resize_rgba(make_variant(base, HIDDEN_COLORS["green"]), target, assoc, domain)
                b = resize_rgba(make_variant(base, HIDDEN_COLORS["magenta"]), target, assoc, domain)
                key = f"{domain}/{assoc}"
                condition_data[key] = summarize(a, b)
                outputs[key] = a
        retained[f"{scale:.3f}"] = outputs
        report["scales"][f"{scale:.3f}"] = {
            "output dimensions": list(target),
            "hidden RGB sensitivity by domain and association": condition_data,
        }
    sheet = make_sheet(retained["0.500"], "0.500")
    report["scale 0.5 contact sheet"] = str(sheet.relative_to(ROOT))
    CAPTURE.mkdir(parents=True, exist_ok=True)
    (CAPTURE / "opencv-alpha-resize-metrics.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    data = main()
    print(f"Modeled OpenCV {data['OpenCV version']} RGBA resize across {len(data['scales'])} scales.")
