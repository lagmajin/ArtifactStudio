"""Compare per-stage sRGB and linear-light domains across the retro suite."""

from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from retro_lab import (
    BAYER_4,
    PRESETS,
    apply_retro,
    chromatic_shift,
    linear_to_srgb,
    ordered_quantize_domain,
    srgb_to_linear,
)

ROOT = Path(__file__).resolve().parents[1]
CORPUS = ROOT / "eval-corpus"
CAPTURE = ROOT / "captures" / "full-suite-color-domain"
SEED = 79
DOMAINS = ("resize", "quantize", "chromatic", "scanline", "grain")
ASSET_SPECS = (
    ("cc0-white-clover", "2e325727cf8369e7.png"),
    ("cc0-white-clover", "f347ea575b04c894.png"),
    ("cc0-hair-fur", "deb0bc3a37ee22f5.png"),
)
CONFIGS = ("sRGB baseline", *(f"linear {name}" for name in DOMAINS), "all linear")


def edge_masks(alpha: np.ndarray) -> dict[str, np.ndarray]:
    visible = alpha > 0
    partial = (alpha > 0) & (alpha < 255)
    h, w = alpha.shape
    padded = np.pad(visible, 1, constant_values=False)
    edge = partial.copy()
    for dy in range(3):
        for dx in range(3):
            if (dy, dx) != (1, 1):
                edge |= visible != padded[dy:dy + h, dx:dx + w]
    if not np.any(edge):
        edge = np.ones_like(visible)
    return {"whole": np.ones_like(visible), "alpha edge": edge, "partial alpha": partial if np.any(partial) else visible}


def render_variant(source: Image.Image, preset_name: str, linear_stages: frozenset[str]) -> Image.Image:
    preset = PRESETS[preset_name]
    rgba = source.convert("RGBA")
    source_rgb = np.asarray(rgba.convert("RGB"), dtype=np.float32) / 255.0
    alpha = np.asarray(rgba.getchannel("A"), dtype=np.uint8)
    rgb = source_rgb
    width, height = rgba.size

    if preset["scale"] < 1.0:
        domain = "linear" if "resize" in linear_stages else "srgb"
        # The preset scale is retained as authored; the helper's exact target
        # size below handles every current suite scale.
        if domain == "srgb":
            small_size = (max(1, round(width * preset["scale"])), max(1, round(height * preset["scale"])))
            resized = Image.fromarray(np.uint8(np.clip(rgb * 255.0 + 0.5, 0, 255)), "RGB")
            rgb = np.asarray(resized.resize(small_size, Image.Resampling.BOX).resize((width, height), Image.Resampling.NEAREST), dtype=np.float32) / 255.0
        else:
            linear = srgb_to_linear(rgb)
            small_size = (max(1, round(width * preset["scale"])), max(1, round(height * preset["scale"])))
            planes = []
            for channel in range(3):
                plane = Image.fromarray(linear[:, :, channel].astype(np.float32), "F")
                plane = plane.resize(small_size, Image.Resampling.BOX).resize((width, height), Image.Resampling.NEAREST)
                planes.append(np.asarray(plane, dtype=np.float32))
            rgb = linear_to_srgb(np.stack(planes, axis=2))

    quant_domain = "linear" if "quantize" in linear_stages else "srgb"
    rgb = ordered_quantize_domain(rgb, max(2, preset["colors"]), BAYER_4,
                                  min(max(preset["dither"], 0.0), 1.0), quant_domain)
    chromatic_domain = "linear" if "chromatic" in linear_stages else "srgb"
    rgb = chromatic_shift(rgb, preset["aberration"], "bilinear", "clamp", chromatic_domain)

    if preset["scanlines"] > 0:
        gain = np.where(np.arange(height)[:, None, None] % 2 == 1,
                        1.0 - min(preset["scanlines"], 0.9), 1.0)
        if "scanline" in linear_stages:
            rgb = linear_to_srgb(srgb_to_linear(rgb) * gain)
        else:
            rgb *= gain
    if preset["grain"] > 0:
        noise = np.random.default_rng(SEED).normal(0.0, preset["grain"], size=(height, width, 1)).astype(np.float32)
        if "grain" in linear_stages:
            rgb = linear_to_srgb(srgb_to_linear(rgb) + noise)
        else:
            rgb += noise
    out = Image.fromarray(np.uint8(np.clip(rgb, 0.0, 1.0) * 255.0), "RGB").convert("RGBA")
    out.putalpha(rgba.getchannel("A"))
    return out


def compare_outputs(source: Image.Image, baseline: Image.Image, candidate: Image.Image) -> dict[str, object]:
    base = np.asarray(baseline, dtype=np.float32) / 255.0
    current = np.asarray(candidate, dtype=np.float32) / 255.0
    alpha = base[:, :, 3]
    if not np.array_equal(np.asarray(baseline)[:, :, 3], np.asarray(candidate)[:, :, 3]):
        raise ValueError("suite candidate modified alpha")
    masks = edge_masks(np.asarray(source.convert("RGBA"))[:, :, 3])
    delta = np.abs(srgb_to_linear(base[:, :, :3]) - srgb_to_linear(current[:, :, :3]))
    composited = delta * alpha[:, :, None]
    per_pixel = np.max(composited, axis=2)
    result: dict[str, object] = {
        "alpha changed ratio": 0.0,
        "linear RGB RMSE vs current sRGB suite": float(np.mean((srgb_to_linear(base[:, :, :3]) - srgb_to_linear(current[:, :, :3])) ** 2) ** 0.5),
        "composited RGB changed ratio >1/255": float(np.mean(per_pixel > (1.0 / 255.0))),
        "unique RGB colors": int(np.unique(np.asarray(candidate.convert("RGB")).reshape(-1, 3), axis=0).shape[0]),
    }
    for name, mask in masks.items():
        values = composited[mask]
        result[f"{name} composited linear RGB MAE"] = float(np.mean(values)) if values.size else 0.0
        result[f"{name} composited linear RGB P95"] = float(np.percentile(values, 95)) if values.size else 0.0
        result[f"{name} changed pixel ratio >1/255"] = float(np.mean(np.max(values, axis=1) > (1.0 / 255.0))) if values.size else 0.0
    return result


def load_verified_asset(corpus_name: str, filename: str) -> tuple[Image.Image, dict[str, object]]:
    corpus = CORPUS / corpus_name
    manifest = json.loads((corpus / "manifest.json").read_text(encoding="utf-8"))
    entry = next(item for item in manifest if item["file"] == filename)
    path = corpus / filename
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != entry["sha256"]:
        raise ValueError(f"corpus SHA-256 mismatch: {path}")
    return Image.open(path).convert("RGBA"), {**entry, "sha256 verified": True, "corpus": corpus_name}


def main() -> dict[str, object]:
    assets: list[dict[str, object]] = []
    for corpus_name, filename in ASSET_SPECS:
        source, provenance = load_verified_asset(corpus_name, filename)
        source_array = np.asarray(source, dtype=np.uint8)
        preset_rows: dict[str, object] = {}
        for preset_name in PRESETS:
            baseline = apply_retro(source, **PRESETS[preset_name], seed=SEED)
            baseline_check = render_variant(source, preset_name, frozenset())
            if not np.array_equal(np.asarray(baseline), np.asarray(baseline_check)):
                raise ValueError(f"sRGB model does not match current preset: {filename}/{preset_name}")
            output_by_config = {"sRGB baseline": baseline}
            for stage in DOMAINS:
                output_by_config[f"linear {stage}"] = render_variant(source, preset_name, frozenset((stage,)))
            output_by_config["all linear"] = render_variant(source, preset_name, frozenset(DOMAINS))
            preset_rows[preset_name] = {
                "source alpha coverage": {
                    "alpha-zero ratio": float(np.mean(source_array[:, :, 3] == 0)),
                    "partial-alpha ratio": float(np.mean((source_array[:, :, 3] > 0) & (source_array[:, :, 3] < 255))),
                },
                "variants vs sRGB baseline": {
                    config: compare_outputs(source, baseline, image)
                    for config, image in output_by_config.items()
                },
                "outputs": output_by_config,
            }
        assets.append({"provenance": provenance, "dimensions": list(source.size), "presets": preset_rows})

    CAPTURE.mkdir(parents=True, exist_ok=True)
    for asset in assets:
        filename = str(asset["provenance"]["file"])
        for preset_name, preset_data in asset["presets"].items():
            outputs = preset_data["outputs"]
            tile = 192
            label_h = 22
            sheet = Image.new("RGB", (len(CONFIGS) * tile, tile + label_h), (28, 29, 33))
            draw = ImageDraw.Draw(sheet)
            for column, config in enumerate(CONFIGS):
                image = outputs[config].convert("RGB").resize((tile, tile), Image.Resampling.LANCZOS)
                draw.text((column * tile + 4, 4), config, fill=(245, 245, 245), font=ImageFont.load_default())
                sheet.paste(image, (column * tile, label_h))
            (CAPTURE / preset_name).mkdir(parents=True, exist_ok=True)
            sheet.save(CAPTURE / preset_name / f"{Path(filename).stem}-domains.png")
            del preset_data["outputs"]

    report = {
        "question": "How do independently switching resize, quantization, chromatic interpolation, scanline attenuation, and grain between sRGB code values and linear light change each existing retro preset?",
        "method": "Five one-stage-at-a-time linear counterfactuals plus all-linear, compared to the exact current apply_retro output. Same deterministic grain seed across configurations. Current sRGB renderer equality is checked pixel-for-pixel before the sweep.",
        "baseline validation": "The custom sRGB-domain runner matched apply_retro pixel-for-pixel for all 3 assets and 4 presets.",
        "assets": assets,
        "limitations": "Three SHA-verified CC0 assets: two clover transparent cutouts from one pack and one opaque hair/fur texture. Results quantify output difference from the current recipe, not which look is better. Domain variants retain numeric coefficients, so their grain magnitude and scanline response are intentionally not perceptually matched. No human quality threshold, GPU/Core parity, or performance claim.",
    }
    (CAPTURE / "full-suite-color-domain-metrics.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    report = main()
    print(f"Measured {len(report['assets'])} provenance-checked assets across {len(PRESETS)} presets and {len(CONFIGS)} color-domain variants.")
