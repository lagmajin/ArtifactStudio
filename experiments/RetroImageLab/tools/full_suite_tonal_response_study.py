
from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from retro_lab import BAYER_4, PRESETS, apply_retro, ordered_quantize, srgb_to_linear  # noqa: E402

CAPTURE = ROOT / "captures" / "full-suite-tonal-response"
SEED = 31041
BACKGROUNDS = {"black": (0.0, 0.0, 0.0), "mid-gray": (0.5, 0.5, 0.5), "white": (1.0, 1.0, 1.0)}
ASSETS = (
    ("eval-corpus/cc0-white-clover/8052d08ef1901b36.png", "eval-corpus/cc0-white-clover/manifest.json", "8052d08ef1901b36.png", "clover_close"),
    ("eval-corpus/cc0-white-clover/214259e22c9db14f.png", "eval-corpus/cc0-white-clover/manifest.json", "214259e22c9db14f.png", "clover_close2"),
    ("eval-corpus/cc0-white-clover/2e325727cf8369e7.png", "eval-corpus/cc0-white-clover/manifest.json", "2e325727cf8369e7.png", "clover_full"),
    ("eval-corpus/cc0-white-clover/f347ea575b04c894.png", "eval-corpus/cc0-white-clover/manifest.json", "f347ea575b04c894.png", "trifolium_macro"),
    ("eval-corpus/cc0-white-clover/3548803a33d8936f.png", "eval-corpus/cc0-white-clover/manifest.json", "3548803a33d8936f.png", "vegetation_clover"),
    ("eval-inputs/cc0-feathers/feather_simple_sprite.png", "eval-inputs/cc0-feathers/provenance.json", "feather_simple_sprite.png", "feather_simple"),
    ("eval-inputs/cc0-feathers/feather_silver.png", "eval-inputs/cc0-feathers/provenance.json", "feather_silver.png", "feather_silver"),
    ("eval-inputs/cc0-feathers/feather_pixel_26.png", "eval-inputs/cc0-feathers/provenance.json", "feather_pixel_26.png", "feather_pixel"),
    ("eval-inputs/cc0-hair-fur/hair_fur_texture.png", "eval-inputs/cc0-hair-fur/provenance.json", "hair_fur_texture.png", "hair_fur_opaque"),
    ("eval-inputs/cc0-hair-fur/hair_0_dugi.jpg", "eval-inputs/cc0-hair-fur/provenance.json", "hair_0_dugi.jpg", "hair_texture_jpeg"),
)


def load_asset(spec: tuple[str, str, str, str]) -> tuple[Image.Image, dict[str, object], str]:
    rel_path, rel_manifest, filename, name = spec
    path = ROOT / rel_path
    manifest = json.loads((ROOT / rel_manifest).read_text(encoding="utf-8"))
    entry = next(row for row in manifest if row["file"] == filename) if isinstance(manifest, list) else manifest[filename]
    expected = entry["sha256"] if "sha256" in entry else entry.get("source_sha256", entry.get("download_sha256"))
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != expected:
        raise ValueError(f"provenance SHA-256 mismatch for {path}: {digest} != {expected}")
    with Image.open(path) as opened:
        image = opened.convert("RGBA")
    return image, {"name": name, "source file": rel_path, "manifest": rel_manifest,
                   "sha256": digest, "provenance": entry}, digest


def make_ramp() -> tuple[Image.Image, dict[str, object]]:
    width, height = 768, 384
    x = np.linspace(0, 1, width, dtype=np.float32)[None, :]
    ramp = np.broadcast_to(x, (height, width))
    rgb = np.stack((ramp, ramp, ramp), axis=2)
    image = Image.fromarray(np.uint8(np.clip(rgb * 255 + .5, 0, 255)), "RGB").convert("RGBA")
    draw = ImageDraw.Draw(image)
    for row, level in enumerate((0.08, 0.18, 0.32, 0.5, 0.72, 0.92)):
        y0 = 144 + row * 36
        draw.rectangle((0, y0, width - 1, y0 + 24), fill=tuple(round(c * 255) for c in (level, level * .7, 1 - level * .5)))
        draw.line((0, y0 + 28, width - 1, y0 + 28), fill=(255, 255, 255, 255), width=1)
    digest = hashlib.sha256(image.tobytes()).hexdigest()
    return image, {"name": "synthetic-ramp-control", "kind": "generated synthetic input; not a target image",
                   "dimensions": [width, height], "sha256 decoded RGBA bytes": digest,
                   "description": "8-bit neutral horizontal gray ramp with six broad colored patches and thin white boundaries"}


def masks(rgb: np.ndarray, alpha: np.ndarray) -> dict[str, np.ndarray]:
    visible = alpha > 0
    partial = (alpha > 0) & (alpha < 255)
    padded = np.pad(visible, 1, constant_values=False)
    edge = np.zeros_like(visible)
    h, w = visible.shape
    for dy in range(3):
        for dx in range(3):
            edge |= visible != padded[dy:dy + h, dx:dx + w]
    luma = srgb_to_linear(rgb) @ np.array([.2126, .7152, .0722], dtype=np.float32)
    gradx = np.zeros_like(luma)
    grady = np.zeros_like(luma)
    gradx[:, 1:] = np.abs(luma[:, 1:] - luma[:, :-1])
    grady[1:, :] = np.abs(luma[1:, :] - luma[:-1, :])
    gradient = (gradx + grady) > .025
    smooth = (gradx + grady) < .004
    return {"visible": visible, "alpha edge": edge, "partial alpha": partial,
            "smooth low-gradient": smooth & visible, "gradient/detail": gradient & visible}


def quantized_unique(rgb: np.ndarray, mask: np.ndarray) -> int:
    values = np.uint8(np.clip(rgb[mask] * 255 + .5, 0, 255))
    return int(np.unique(values, axis=0).shape[0]) if values.size else 0


def distribution(rgb: np.ndarray, region: np.ndarray, alpha: np.ndarray) -> dict[str, object]:
    selected = rgb[region]
    if not selected.size:
        return {"pixel count": 0}
    luma = srgb_to_linear(selected) @ np.array([.2126, .7152, .0722], dtype=np.float32)
    weights = alpha[region].astype(np.float32) / 255.0
    unique = quantized_unique(rgb, region)
    hist = np.histogram(luma, bins=32, range=(0, 1), weights=weights)[0]
    hist = hist / max(float(hist.sum()), 1e-12)
    entropy = float(-np.sum(hist[hist > 0] * np.log2(hist[hist > 0])))
    return {"pixel count": int(selected.shape[0]), "unique RGB8 colors": unique,
            "linear Rec709 luma min/median/p95/max": [float(v) for v in np.percentile(luma, [0, 50, 95, 100])],
            "weighted luma histogram entropy bits (32 bins, descriptive)": entropy,
            "channel clipped-low ratio": float(np.mean(selected <= 0.0)),
            "channel clipped-high ratio": float(np.mean(selected >= 1.0))}


def error_stats(source: np.ndarray, result: np.ndarray, mask: np.ndarray) -> dict[str, float]:
    delta = srgb_to_linear(result[mask]) - srgb_to_linear(source[mask])
    if not delta.size:
        return {"linear RGB MAE": 0.0, "linear RGB RMSE": 0.0,
                "linear RGB absolute P95": 0.0,
                "pixel ratio max channel delta >1/255 linear": 0.0}
    per_pixel = np.max(np.abs(delta), axis=1)
    return {"linear RGB MAE": float(np.mean(np.abs(delta))),
            "linear RGB RMSE": float(np.sqrt(np.mean(delta * delta))),
            "linear RGB absolute P95": float(np.percentile(np.abs(delta), 95)),
            "pixel ratio max channel delta >1/255 linear": float(np.mean(per_pixel > 1 / 255))}


def make_contact_sheet(cases: list[dict[str, object]]) -> Path:
    tile, label = 128, 18
    cols = 5
    rows = (len(cases) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * tile, rows * (tile + label)), (28, 29, 33))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    for i, case in enumerate(cases):
        x, y = (i % cols) * tile, (i // cols) * (tile + label)
        draw.text((x + 2, y + 3), f"{case['asset']} / {case['preset']}", fill=(245, 245, 245), font=font)
        image = Image.fromarray(case["preview"], "RGB").resize((tile, tile), Image.Resampling.NEAREST)
        sheet.paste(image, (x, y + label))
    CAPTURE.mkdir(parents=True, exist_ok=True)
    path = CAPTURE / "full-suite-tonal-response-sheet.png"
    sheet.save(path)
    return path


def make_ramp_sheet() -> Path:
    source, _ = make_ramp()
    w, h = source.size
    rows = []
    labels = []
    for name, preset in PRESETS.items():
        rgb = np.asarray(source.convert("RGB"), dtype=np.float32) / 255.0
        out = ordered_quantize(rgb, int(preset["colors"]), BAYER_4, float(preset["dither"]))
        rows.append(Image.fromarray(np.uint8(np.clip(out * 255 + .5, 0, 255)), "RGB"))
        labels.append(f"{name} / quantizer only")
        out_full = apply_retro(source, **preset, seed=SEED).convert("RGB")
        rows.append(out_full)
        labels.append(f"{name} / full suite")
    label_h = 22
    sheet = Image.new("RGB", (w, len(rows) * (h + label_h)), (27, 28, 31))
    draw = ImageDraw.Draw(sheet)
    for i, (image, name) in enumerate(zip(rows, labels)):
        y = i * (h + label_h)
        draw.text((5, y + 4), f"{name} / display scale 1:1", fill=(245, 245, 245), font=ImageFont.load_default())
        sheet.paste(image, (0, y + label_h))
    path = CAPTURE / "full-suite-tonal-response-ramp-quantizer-vs-suite-1to1.png"
    sheet.save(path)
    return path


def main() -> dict[str, object]:
    ramp, ramp_provenance = make_ramp()
    sources = [(ramp, ramp_provenance)]
    for spec in ASSETS:
        image, provenance, _ = load_asset(spec)
        sources.append((image, provenance))
    records: list[dict[str, object]] = []
    sheet_cases: list[dict[str, object]] = []
    for image, provenance in sources:
        src_rgba = np.asarray(image, dtype=np.uint8)
        src_rgb = src_rgba[:, :, :3].astype(np.float32) / 255.0
        alpha = src_rgba[:, :, 3]
        regions = masks(src_rgb, alpha)
        base_stats = {name: distribution(src_rgb, region, alpha) for name, region in regions.items()}
        for preset_name, preset in PRESETS.items():
            full = apply_retro(image, **preset, seed=SEED)
            full_arr = np.asarray(full, dtype=np.uint8)
            full_rgb = full_arr[:, :, :3].astype(np.float32) / 255.0
            # Isolate the suite's quantizer before channel offset, scanline, and grain.
            preprocessed = src_rgb
            if preset["scale"] < 1.0:
                w, h = image.size
                small = (max(1, round(w * preset["scale"])), max(1, round(h * preset["scale"])))
                resized = image.convert("RGB").resize(small, Image.Resampling.BOX).resize(image.size, Image.Resampling.NEAREST)
                preprocessed = np.asarray(resized, dtype=np.float32) / 255.0
            quant = ordered_quantize(preprocessed, int(preset["colors"]), BAYER_4, float(preset["dither"]))
            quant_rgb = np.clip(quant, 0, 1)
            record = {"asset": provenance["name"], "preset": preset_name,
                      "source SHA-256": provenance.get("sha256", provenance.get("sha256 decoded RGBA bytes")),
                      "source provenance": provenance,
                      "source dimensions": [image.width, image.height],
                      "alpha nonopaque pixel ratio": float(np.mean(alpha < 255)),
                      "source regions": base_stats,
                      "full suite alpha exact match": bool(np.array_equal(alpha, full_arr[:, :, 3])),
                      "quantizer-only": {}, "full suite": {}, "composited full suite": {}}
            for region_name, mask in regions.items():
                record["quantizer-only"][region_name] = {
                    "distribution": distribution(quant_rgb, mask, alpha),
                    "delta from input": error_stats(preprocessed, quant_rgb, mask),
                }
                record["full suite"][region_name] = {
                    "distribution": distribution(full_rgb, mask, alpha),
                    "delta from input": error_stats(src_rgb, full_rgb, mask),
                }
            for bg_name, bg in BACKGROUNDS.items():
                bg_arr = np.asarray(bg, dtype=np.float32)[None, None, :]
                src_comp = src_rgb * (alpha[:, :, None] / 255.0) + bg_arr * (1 - alpha[:, :, None] / 255.0)
                out_comp = full_rgb * (full_arr[:, :, 3:4] / 255.0) + bg_arr * (1 - full_arr[:, :, 3:4] / 255.0)
                record["composited full suite"][bg_name] = error_stats(src_comp, out_comp, regions["visible"])
            preview = full.convert("RGBA")
            preview = Image.alpha_composite(Image.new("RGBA", preview.size, (128, 128, 128, 255)), preview).convert("RGB")
            thumb = preview.resize((128, 128), Image.Resampling.LANCZOS)
            sheet_cases.append({"asset": provenance["name"], "preset": preset_name,
                                "preview": np.asarray(thumb, dtype=np.uint8)})
            records.append(record)
    sheet = make_contact_sheet(sheet_cases)
    ramp_sheet = make_ramp_sheet()
    report = {
        "question": "How do palette quantization and the complete current suite affect tone distributions, available colors, and local gradients across a varied provenance-checked still corpus?",
        "evaluation plan": {
            "target": "Describe current suite response, not rank presets or prove visual quality.",
            "baseline": "Same decoded RGBA input. Input differences are response measurements only; there is no approved target image or ground truth.",
            "conditions": f"Four current PRESETS, fixed seed {SEED}. Quantizer-only uses the preset's color count and Bayer strength before later suite stages; scale presets use the same BOX downsample + NEAREST upsample first. Full suite calls apply_retro unchanged.",
            "assets": "10 CC0 assets from existing hashed corpus: five white-clover cutouts, three feather assets, and two hair/fur textures; plus a generated opaque gray ramp/color-patch control. Manifest SHA-256 is verified before decode.",
            "image condition": "Native asset dimensions, decoded to RGBA8; no external resize. RGB statistics use decoded sRGB code values converted to linear sRGB for Rec.709 luma/errors. Alpha is preserved by suite and separately composited onto black/mid-gray/white using normalized code-value compositing for descriptive comparison, not a product compositing assumption.",
            "regions": "visible, 3x3 alpha boundary band, partial-alpha, smooth low-gradient (linear luma neighbor sum <.004), and gradient/detail (> .025). Masks and pixel counts are computed per source and reused for candidate output.",
            "metrics": "unique RGB8 colors; linear Rec.709 luma min/median/p95/max; 32-bin alpha-coverage-weighted luma entropy; per-channel 0/1 clip ratios; linear RGB MAE/RMSE/P95 and >1/255 changed pixel ratio; output alpha exact match; RGB error after three backgrounds.",
            "time stability": "Not applicable to this still-image suite study; no time-varying effect or frame sequence is under evaluation.",
            "performance and product paths": "Not measured here. Python runner only; no Artifact.exe, Core, GPU, preview/export or runtime path.",
            "acceptance": "No approved target, human raters, or image-quality threshold exists. All figures are descriptive; no pass/fail or preset quality ranking is claimed.",
            "unverified": "Natural photographs, high-bit-depth/HDR input, color-managed decoding, controlled display transform, real application paths, and human-approved visual targets.",
        },
        "environment": {"python": sys.version, "numpy": np.__version__, "pillow": Image.__version__, "platform": sys.platform},
        "records": records, "contact sheet": str(sheet.relative_to(ROOT)),
        "ramp sheet at 1:1 source pixels": str(ramp_sheet.relative_to(ROOT)),
        "limits": "Pixel statistics do not establish perceived quality. The crop region masks are source-derived and do not isolate semantic objects; alpha compositing is an explicit display model. Color conversion assumes decoded RGB is sRGB; embedded profiles are not applied.",
    }
    CAPTURE.mkdir(parents=True, exist_ok=True)
    (CAPTURE / "full-suite-tonal-response-metrics.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    report = main()
    print(f"Analyzed {len(report['records'])} asset/preset cases with provenance checks.")
