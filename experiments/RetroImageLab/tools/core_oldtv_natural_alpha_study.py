"""Run the OldTV hidden-RGB probe on provenance-checked CC0 cutouts."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
CORPUS = ROOT / "eval-corpus" / "cc0-white-clover"
MANIFEST_PATH = CORPUS / "manifest.json"
ASSETS = ("2e325727cf8369e7.png", "f347ea575b04c894.png")
FRINGES = (0.0, 0.3, 1.0)
BACKGROUNDS = {"black": (0, 0, 0), "mid-gray": (128, 128, 128), "white": (255, 255, 255)}
PATTERNS = ("uniform", "checker")
REVIEW_CROP = 96


def hidden_rgb_pair(source: np.ndarray, pattern: str) -> tuple[np.ndarray, np.ndarray]:
    a, b = source.copy(), source.copy()
    transparent = source[:, :, 3] == 0
    h, w = transparent.shape
    if pattern == "uniform":
        a[transparent, :3] = (255, 0, 255)
        b[transparent, :3] = (0, 255, 32)
    else:
        yy, xx = np.mgrid[0:h, 0:w]
        tile = ((xx // 9 + yy // 11) & 1).astype(bool)
        pat_a = np.where(tile[:, :, None], (255, 0, 255), (0, 220, 255))
        pat_b = np.where(tile[:, :, None], (0, 255, 32), (255, 220, 0))
        a[:, :, :3] = np.where(transparent[:, :, None], pat_a, a[:, :, :3])
        b[:, :, :3] = np.where(transparent[:, :, None], pat_b, b[:, :, :3])
    return a, b


def sample_indices(width: int, height: int, fringe: float) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    y, x = np.mgrid[0:height, 0:width]
    cx = width * 0.5
    split = fringe * 5.0 * (np.abs(x - cx) / cx)
    red_x = np.clip(np.trunc(x - split).astype(np.int64), 0, width - 1)
    blue_x = np.clip(np.trunc(x + split).astype(np.int64), 0, width - 1)
    return y, red_x, blue_x


def apply_fringe(rgba: np.ndarray, fringe: float) -> tuple[np.ndarray, dict[str, float]]:
    h, w = rgba.shape[:2]
    y, red_x, blue_x = sample_indices(w, h, fringe)
    out = rgba.copy()
    out[:, :, 0] = rgba[y, red_x, 0]
    out[:, :, 2] = rgba[y, blue_x, 2]
    alpha = rgba[:, :, 3]
    visible_dst = alpha > 0
    red_hidden = (rgba[y, red_x, 3] == 0) & visible_dst
    blue_hidden = (rgba[y, blue_x, 3] == 0) & visible_dst
    any_hidden = red_hidden | blue_hidden
    visible_count = max(1, int(np.count_nonzero(visible_dst)))
    return out, {
        "visible destinations sampling red from alpha-zero count": int(np.count_nonzero(red_hidden)),
        "visible destinations sampling blue from alpha-zero count": int(np.count_nonzero(blue_hidden)),
        "visible destinations sampling either shifted channel from alpha-zero count": int(np.count_nonzero(any_hidden)),
        "visible destinations sampling either shifted channel from alpha-zero ratio": float(np.count_nonzero(any_hidden) / visible_count),
    }


def composite(rgba: np.ndarray, bg_rgb: tuple[int, int, int]) -> np.ndarray:
    rgb = rgba[:, :, :3].astype(np.float32) / 255.0
    alpha = rgba[:, :, 3:4].astype(np.float32) / 255.0
    bg = np.asarray(bg_rgb, dtype=np.float32).reshape(1, 1, 3) / 255.0
    return rgb * alpha + bg * (1.0 - alpha)


def region_masks(alpha: np.ndarray) -> dict[str, np.ndarray]:
    visible = alpha > 0
    partial = (alpha > 0) & (alpha < 255)
    padded = np.pad(visible, 1, constant_values=False)
    edge = np.zeros_like(visible)
    h, w = visible.shape
    for dy in range(3):
        for dx in range(3):
            if (dx, dy) != (1, 1):
                edge |= visible != padded[dy:dy + h, dx:dx + w]
    edge |= partial
    return {"whole": np.ones_like(visible), "visible": visible, "edge": edge, "partial-alpha": partial}


def summarize(a: np.ndarray, b: np.ndarray, mask: np.ndarray) -> dict[str, object]:
    if not np.any(mask):
        return {"pixel count": 0, "MAE normalized code": 0.0,
                "P95 abs normalized code": 0.0, "max abs normalized code": 0.0,
                "changed pixel ratio >1/255": 0.0}
    delta = np.abs(a[mask] - b[mask])
    return {
        "pixel count": int(np.count_nonzero(mask)),
        "MAE normalized code": float(np.mean(delta)),
        "P95 abs normalized code": float(np.percentile(delta, 95)),
        "max abs normalized code": float(np.max(delta)),
        "changed pixel ratio >1/255": float(np.mean(np.max(delta, axis=1) > 1 / 255)),
    }


def uint_hash(values: np.ndarray) -> np.ndarray:
    """Vector form of the Artifact HLSL uint hash."""
    with np.errstate(over="ignore"):
        v = values.astype(np.uint32, copy=True)
        v ^= v >> np.uint32(16)
        v *= np.uint32(0x7FEB352D)
        v ^= v >> np.uint32(15)
        v *= np.uint32(0x846CA68B)
        v ^= v >> np.uint32(16)
    return v


def gpu_random01(x: np.ndarray, y: np.ndarray, salt: int) -> np.ndarray:
    seed = (x.astype(np.uint32) * np.uint32(73856093) ^
            y.astype(np.uint32) * np.uint32(19349663) ^ np.uint32(salt))
    return (uint_hash(seed) & np.uint32(0x00FFFFFF)).astype(np.float32) / np.float32(16777215.0)


def artifact_gpu_model(rgba: np.ndarray) -> tuple[np.ndarray, dict[str, object]]:
    """Faithful CPU translation of the current Artifact GPU OldTV HLSL equation."""
    h, w = rgba.shape[:2]
    y, x = np.mgrid[0:h, 0:w]
    chance = gpu_random01(np.zeros_like(x), y, 17)
    amount = gpu_random01(np.zeros_like(x), y, 18)
    jitter_f = np.where(chance < 0.08, amount * 10.0 - 5.0, 0.0)
    # HLSL uses an integer cast (truncate toward zero) before clamping.
    source_x = np.clip(x + np.trunc(jitter_f).astype(np.int64), 0, w - 1)
    sampled = rgba[y, source_x]
    scanline = np.where((y % 4) == 0, 0.7, 1.0).astype(np.float32)
    noise = (gpu_random01(x, y, 19) * 2.0 - 1.0) * 0.005
    out = np.empty_like(rgba, dtype=np.float32)
    out[:, :, :3] = np.clip(sampled[:, :, :3] * scanline[:, :, None] + noise[:, :, None], 0.0, 1.0)
    out[:, :, 3] = sampled[:, :, 3]
    meta = {
        "nonzero jitter row ratio": float(np.mean(np.any(source_x != x, axis=1))),
        "jitter min/max pixels": [int(np.min(np.trunc(jitter_f))), int(np.max(np.trunc(jitter_f)))],
        "alpha changed destination pixel ratio": float(np.mean(out[:, :, 3] != rgba[:, :, 3])),
        "alpha changed visible destination ratio": float(np.mean((out[:, :, 3] != rgba[:, :, 3]) & (rgba[:, :, 3] > 0)) / max(1e-12, np.mean(rgba[:, :, 3] > 0))),
        "RGB input values below zero": int(np.count_nonzero(rgba[:, :, :3] < 0)),
        "RGB input values above one": int(np.count_nonzero(rgba[:, :, :3] > 1)),
        "RGB output values clipped low": int(np.count_nonzero(sampled[:, :, :3] * scanline[:, :, None] + noise[:, :, None] < 0)),
        "RGB output values clipped high": int(np.count_nonzero(sampled[:, :, :3] * scanline[:, :, None] + noise[:, :, None] > 1)),
    }
    return out, meta


def core_cpu_model(rgba: np.ndarray, fringe: float = 0.3) -> np.ndarray:
    """Core OldTV at time=0, with curve/flicker disabled and scanline=0."""
    h, w = rgba.shape[:2]
    y, red_x, blue_x = sample_indices(w, h, fringe)
    out = rgba.astype(np.float32, copy=True)
    out[:, :, 0] = rgba[y, red_x, 0]
    out[:, :, 2] = rgba[y, blue_x, 2]
    return out


def summarize_float(a: np.ndarray, b: np.ndarray, mask: np.ndarray) -> dict[str, object]:
    if not np.any(mask):
        return {"pixel count": 0, "MAE": 0.0, "P95 abs": 0.0, "max abs": 0.0,
                "changed pixel ratio >1e-6": 0.0}
    delta = np.abs(a[mask].astype(np.float64) - b[mask].astype(np.float64))
    return {"pixel count": int(np.count_nonzero(mask)), "MAE": float(np.mean(delta)),
            "P95 abs": float(np.percentile(delta, 95)), "max abs": float(np.max(delta)),
            "changed pixel ratio >1e-6": float(np.mean(np.max(delta, axis=1) > 1e-6))}


def linear_composite(rgba: np.ndarray, bg: tuple[float, float, float]) -> np.ndarray:
    alpha = rgba[:, :, 3:4].astype(np.float32)
    background = np.asarray(bg, dtype=np.float32).reshape(1, 1, 3)
    return rgba[:, :, :3] * alpha + background * (1.0 - alpha)


def float_domain_probe(source: np.ndarray, scale: float, offset: float) -> dict[str, object]:
    """Map visible RGB into HDR/negative ranges; retain the original alpha coverage."""
    rgba = source.astype(np.float32) / 255.0
    rgba[:, :, :3] = rgba[:, :, :3] * np.float32(scale) + np.float32(offset)
    alpha = rgba[:, :, 3]
    masks = region_masks(source[:, :, 3])
    cpu = core_cpu_model(rgba)
    gpu, gpu_meta = artifact_gpu_model(rgba)
    pair_a_u8, pair_b_u8 = hidden_rgb_pair(source, "uniform")
    pair_a = pair_a_u8.astype(np.float32) / 255.0
    pair_b = pair_b_u8.astype(np.float32) / 255.0
    pair_a[:, :, :3] = pair_a[:, :, :3] * np.float32(scale) + np.float32(offset)
    pair_b[:, :, :3] = pair_b[:, :, :3] * np.float32(scale) + np.float32(offset)
    gpu_pair_a, _ = artifact_gpu_model(pair_a)
    gpu_pair_b, _ = artifact_gpu_model(pair_b)
    cpu_pair_a, cpu_pair_b = core_cpu_model(pair_a), core_cpu_model(pair_b)
    backgrounds = {"black": (0.0, 0.0, 0.0), "mid-gray": (0.5, 0.5, 0.5), "white": (1.0, 1.0, 1.0)}
    composite_deltas = {}
    for bg_name, bg in backgrounds.items():
        composite_deltas[bg_name] = {
            "CPU/GPU edge": summarize_float(linear_composite(cpu, bg), linear_composite(gpu, bg), masks["edge"]),
            "CPU/GPU partial-alpha": summarize_float(linear_composite(cpu, bg), linear_composite(gpu, bg), masks["partial-alpha"]),
            "GPU hidden-RGB pair edge": summarize_float(linear_composite(gpu_pair_a, bg), linear_composite(gpu_pair_b, bg), masks["edge"]),
            "CPU hidden-RGB pair edge": summarize_float(linear_composite(cpu_pair_a, bg), linear_composite(cpu_pair_b, bg), masks["edge"]),
        }
    return {
        "scale": scale, "offset": offset,
        "input RGB min/max": [float(np.min(rgba[:, :, :3])), float(np.max(rgba[:, :, :3]))],
        "GPU shader stats": gpu_meta,
        "CPU/GPU alpha changed pixel ratio": float(np.mean(cpu[:, :, 3] != gpu[:, :, 3])),
        "CPU/GPU RGB normalized-code-domain edge comparison": summarize_float(cpu[:, :, :3], gpu[:, :, :3], masks["edge"]),
        "background composite comparisons": composite_deltas,
        "interpretation": "Synthetic domain transform on real alpha coverage. Straight linear-value compositing is a measurement assumption, not the declared product color/alpha contract.",
    }


def alpha_composite_probe(source: np.ndarray, scale: float, offset: float) -> dict[str, object]:
    """Measure how GPU alpha sampling changes a straight-alpha foreground composite."""
    rgba = source.astype(np.float32) / 255.0
    rgba[:, :, :3] = rgba[:, :, :3] * np.float32(scale) + np.float32(offset)
    gpu, _ = artifact_gpu_model(rgba)
    alpha_held = gpu.copy()
    alpha_held[:, :, 3] = rgba[:, :, 3]
    masks = region_masks(source[:, :, 3])
    backgrounds = {"black": (0.0, 0.0, 0.0), "mid-gray": (0.5, 0.5, 0.5), "white": (1.0, 1.0, 1.0)}
    by_background = {}
    for name, bg in backgrounds.items():
        actual = linear_composite(gpu, bg)
        counterfactual = linear_composite(alpha_held, bg)
        by_background[name] = {
            "GPU alpha sample vs destination-alpha hold, edge": summarize_float(actual, counterfactual, masks["edge"]),
            "GPU alpha sample vs destination-alpha hold, partial-alpha": summarize_float(actual, counterfactual, masks["partial-alpha"]),
            "fully composited output max outside display range": [float(np.min(actual)), float(np.max(actual))],
        }
    # Select a review crop on edge pixels where sampled alpha has the largest effect.
    mid_delta = np.max(np.abs(linear_composite(gpu, backgrounds["mid-gray"]) -
                              linear_composite(alpha_held, backgrounds["mid-gray"])), axis=2)
    mid_delta[~masks["edge"]] = 0.0
    cy, cx = np.unravel_index(np.argmax(mid_delta), mid_delta.shape)
    h, w = source.shape[:2]
    x0 = max(0, min(w - REVIEW_CROP, int(cx) - REVIEW_CROP // 2))
    y0 = max(0, min(h - REVIEW_CROP, int(cy) - REVIEW_CROP // 2))
    crop = (x0, y0, min(w, x0 + REVIEW_CROP), min(h, y0 + REVIEW_CROP))
    def display_over_mid(array: np.ndarray) -> np.ndarray:
        rgb = np.clip(linear_composite(array, (0.5, 0.5, 0.5)), 0.0, 1.0)
        return np.uint8(np.clip(rgb * 255.0 + 0.5, 0, 255))
    original = rgba.copy()
    cpu = core_cpu_model(rgba)
    return {
        "RGB domain": {"scale": scale, "offset": offset},
        "alpha changed visible destination ratio": float(np.mean((gpu[:, :, 3] != rgba[:, :, 3]) & (rgba[:, :, 3] > 0)) / max(1e-12, np.mean(rgba[:, :, 3] > 0))),
        "largest edge alpha-composite delta, mid-gray": float(mid_delta[cy, cx]),
        "review crop xyxy": list(crop),
        "composite metrics": by_background,
        "display variants over mid-gray": [
            ("source", display_over_mid(original)),
            ("Core fringe-only", display_over_mid(cpu)),
            ("GPU HLSL", display_over_mid(gpu)),
            ("GPU alpha held at destination", display_over_mid(alpha_held)),
        ],
        "difference crop": np.uint8(np.clip(mid_delta[crop[1]:crop[3], crop[0]:crop[2]] * 4.0 * 255 + 0.5, 0, 255)),
    }

def premultiplied_composite(rgba: np.ndarray, bg: tuple[float, float, float]) -> np.ndarray:
    alpha = rgba[:, :, 3:4].astype(np.float32)
    background = np.asarray(bg, dtype=np.float32).reshape(1, 1, 3)
    return rgba[:, :, :3] + background * (1.0 - alpha)


def alpha_association_probe(source: np.ndarray) -> dict[str, object]:
    """Run the same GPU equation on straight and valid premultiplied encodings."""
    straight = source.astype(np.float32) / 255.0
    premult = straight.copy()
    premult[:, :, :3] *= premult[:, :, 3:4]
    masks = region_masks(source[:, :, 3])
    backgrounds = {"black": (0.0, 0.0, 0.0), "mid-gray": (0.5, 0.5, 0.5), "white": (1.0, 1.0, 1.0)}
    output = {}
    for name, encoded in (("straight", straight), ("premultiplied", premult)):
        gpu, meta = artifact_gpu_model(encoded)
        rgb, alpha = gpu[:, :, :3], gpu[:, :, 3:4]
        violation = np.any(rgb > alpha + 1e-6, axis=2) | np.any(rgb < -1e-6, axis=2)
        transparent_rgb = (encoded[:, :, 3] <= 1e-6) & np.any(np.abs(rgb) > 1e-6, axis=2)
        composite_fn = linear_composite if name == "straight" else premultiplied_composite
        by_bg = {}
        for bg_name, bg in backgrounds.items():
            by_bg[bg_name] = {
                "Core fringe-only vs GPU full, edge": summarize_float(
                    composite_fn(core_cpu_model(encoded), bg), composite_fn(gpu, bg), masks["edge"]),
                "GPU output vs input, edge": summarize_float(
                    composite_fn(encoded, bg), composite_fn(gpu, bg), masks["edge"]),
            }
        # Changing alpha-zero RGB is meaningful in straight encoding, but a valid
        # premultiplication maps all such RGB values to exactly zero.
        pair_a_u8, pair_b_u8 = hidden_rgb_pair(source, "uniform")
        pair_a = pair_a_u8.astype(np.float32) / 255.0
        pair_b = pair_b_u8.astype(np.float32) / 255.0
        if name == "premultiplied":
            pair_a[:, :, :3] *= pair_a[:, :, 3:4]
            pair_b[:, :, :3] *= pair_b[:, :, 3:4]
        gpu_a, _ = artifact_gpu_model(pair_a)
        gpu_b, _ = artifact_gpu_model(pair_b)
        pair_bg = "mid-gray"
        pair_delta = summarize_float(composite_fn(gpu_a, backgrounds[pair_bg]),
                                     composite_fn(gpu_b, backgrounds[pair_bg]), masks["edge"])
        output[name] = {
            "GPU shader stats": meta,
            "input valid premult invariant violations RGB>alpha": int(np.count_nonzero(np.any(encoded[:, :, :3] > encoded[:, :, 3:4] + 1e-6, axis=2))) if name == "premultiplied" else None,
            "GPU output premult invariant violation pixel count RGB>alpha": int(np.count_nonzero(violation)) if name == "premultiplied" else None,
            "GPU output nonzero RGB at alpha-zero pixel count": int(np.count_nonzero(transparent_rgb)),
            "GPU hidden-RGB pair delta after matching composite": pair_delta,
            "edge composites": by_bg,
            "interpretation": "Parameterization under this alpha representation; source boundary contract is not inferred.",
        }
    return {"conditions": "same 8-bit cutout converted to straight RGB or RGB*alpha; GPU HLSL equation held fixed",
            "straight input": output["straight"], "premultiplied input": output["premultiplied"]}



def save_alpha_composite_sheet(records: list[dict[str, object]], capture_dir: Path) -> None:
    tile = REVIEW_CROP * 4
    label_h = 22
    columns = 5
    sheet = Image.new("RGB", (columns * tile, len(records) * (tile + label_h)), (28, 29, 33))
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default()
    for row_index, record in enumerate(records):
        crop = tuple(record["review crop xyxy"])
        variants = record["display variants over mid-gray"]
        for col, (label, rgb) in enumerate(variants):
            panel = Image.fromarray(rgb, "RGB").crop(crop).resize((tile, tile), Image.Resampling.NEAREST)
            x0, y0 = col * tile, row_index * (tile + label_h)
            draw.text((x0 + 3, y0 + 3), f"{record['asset']} | {label}", fill=(245, 245, 245), font=font)
            sheet.paste(panel, (x0, y0 + label_h))
        diff = Image.fromarray(record["difference crop"], "L").convert("RGB").resize((tile, tile), Image.Resampling.NEAREST)
        x0, y0 = 4 * tile, row_index * (tile + label_h)
        draw.text((x0 + 3, y0 + 3), f"{record['asset']} | alpha-policy composite diff ×4", fill=(245, 245, 245), font=font)
        sheet.paste(diff, (x0, y0 + label_h))
    sheet.save(capture_dir / "core-oldtv-gpu-alpha-composite-sheet.png")


def main() -> dict[str, object]:
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    manifest_by_file = {item["file"]: item for item in manifest}
    results, visuals, provenance, policy_probes, composite_records, association_probes = [], [], [], [], [], []
    for asset in ASSETS:
        if asset not in manifest_by_file:
            raise ValueError(f"asset missing from provenance manifest: {asset}")
        entry = manifest_by_file[asset]
        path = CORPUS / asset
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if digest != entry["sha256"]:
            raise ValueError(f"provenance hash mismatch for {asset}: {digest}")
        source = np.asarray(Image.open(path).convert("RGBA"), dtype=np.uint8)
        h, w = source.shape[:2]
        alpha = source[:, :, 3]
        masks = region_masks(alpha)
        transparent = alpha == 0
        partial = (alpha > 0) & (alpha < 255)
        visible_count = int(np.count_nonzero(alpha > 0))
        provenance.append({**entry, "manifest sha256 verified": True,
            "alpha-zero pixels": int(np.count_nonzero(transparent)),
            "partial-alpha pixels": int(np.count_nonzero(partial)), "visible pixels": visible_count})
        policy_probes.append({
            "asset": asset,
            "8-bit normalized domain": float_domain_probe(source, 1.0, 0.0),
            "HDR domain x2": float_domain_probe(source, 2.0, 0.0),
            "negative/HDR domain x2 minus .5": float_domain_probe(source, 2.0, -0.5),
        })
        composite_result = alpha_composite_probe(source, 1.0, 0.0)
        composite_records.append({"asset": asset, **composite_result})
        association_probes.append({"asset": asset, **alpha_association_probe(source)})
        asset_visuals = {"asset": asset, "variants": []}
        for pattern in PATTERNS:
            pair_a, pair_b = hidden_rgb_pair(source, pattern)
            same_alpha = bool(np.array_equal(pair_a[:, :, 3], pair_b[:, :, 3]))
            visible_rgb_same = bool(np.array_equal(pair_a[alpha > 0, :3], pair_b[alpha > 0, :3]))
            if not same_alpha or not visible_rgb_same:
                raise ValueError(f"visible-identical pair invariant failed: {asset}/{pattern}")
            for fringe in FRINGES:
                out_a, sample_meta = apply_fringe(pair_a, fringe)
                out_b, _ = apply_fringe(pair_b, fringe)
                alpha_delta = np.abs(out_a[:, :, 3].astype(np.int16) - alpha.astype(np.int16))
                row: dict[str, object] = {"asset": asset, "width": w, "height": h,
                    "hidden RGB pattern": pattern, "fringe": fringe,
                    "alpha-zero pixels": int(np.count_nonzero(transparent)),
                    "partial-alpha pixels": int(np.count_nonzero(partial)),
                    "visible source pair identical": visible_rgb_same, "source alpha pair identical": same_alpha,
                    "output alpha max code difference": int(np.max(alpha_delta)),
                    "output alpha changed pixel ratio": float(np.mean(alpha_delta != 0)), **sample_meta}
                for bg_name, bg_rgb in BACKGROUNDS.items():
                    comp_a, comp_b = composite(out_a, bg_rgb), composite(out_b, bg_rgb)
                    for region, mask in masks.items():
                        row[f"{bg_name} {region} pair delta"] = summarize(comp_a, comp_b, mask)
                results.append(row)
        a, b = hidden_rgb_pair(source, "uniform")
        out_a, _ = apply_fringe(a, 1.0); out_b, _ = apply_fringe(b, 1.0)
        pair_delta = np.max(np.abs(composite(out_a, BACKGROUNDS["mid-gray"]) - composite(out_b, BACKGROUNDS["mid-gray"])), axis=2)
        pair_delta[alpha == 0] = 0.0
        max_y, max_x = np.unravel_index(np.argmax(pair_delta), pair_delta.shape)
        half = REVIEW_CROP // 2
        x0 = max(0, min(w - REVIEW_CROP, max_x - half)); y0 = max(0, min(h - REVIEW_CROP, max_y - half))
        crop = (int(x0), int(y0), int(min(w, x0 + REVIEW_CROP)), int(min(h, y0 + REVIEW_CROP)))
        asset_visuals["review crop xyxy"] = list(crop)
        asset_visuals["max pair delta xy"] = [int(max_x), int(max_y)]
        asset_visuals["max pair delta"] = float(pair_delta[max_y, max_x])
        asset_visuals["variants"] = [("source A", a), ("source B", b),
            ("F=.3 A", apply_fringe(a, .3)[0]), ("F=.3 B", apply_fringe(b, .3)[0]),
            ("F=1 A", out_a), ("F=1 B", out_b)]
        visuals.append(asset_visuals)

    panels_per_variant, scale, tile, label_h = 7, 4, REVIEW_CROP * 4, 20
    sheet = Image.new("RGB", (panels_per_variant * tile, len(visuals) * (tile + label_h)), (28, 29, 33))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    for row_index, visual in enumerate(visuals):
        crop, variants = tuple(visual["review crop xyxy"]), visual["variants"]
        for col, (label, image_array) in enumerate(variants):
            displayed = Image.fromarray(image_array, "RGBA").crop(crop)
            bg = Image.new("RGBA", displayed.size, (128, 128, 128, 255))
            shown = Image.alpha_composite(bg, displayed).convert("RGB").resize((tile, tile), Image.Resampling.NEAREST)
            x0, y0 = col * tile, row_index * (tile + label_h)
            draw.text((x0 + 3, y0 + 3), f"{visual['asset']} | {label}", fill=(245, 245, 245), font=font)
            sheet.paste(shown, (x0, y0 + label_h))
        delta = np.uint8(np.clip(np.max(np.abs(composite(variants[4][1], BACKGROUNDS["mid-gray"])[crop[1]:crop[3], crop[0]:crop[2]] - composite(variants[5][1], BACKGROUNDS["mid-gray"])[crop[1]:crop[3], crop[0]:crop[2]]), axis=2) * 4 * 255 + .5, 0, 255))
        diff = Image.fromarray(np.repeat(delta[:, :, None], 3, axis=2), "RGB").resize((tile, tile), Image.Resampling.NEAREST)
        x0, y0 = 6 * tile, row_index * (tile + label_h)
        draw.text((x0 + 3, y0 + 3), f"{visual['asset']} | F=1 pair diff ×4", fill=(245, 245, 245), font=font)
        sheet.paste(diff, (x0, y0 + label_h))

    capture_dir = ROOT / "captures" / "core-oldtv-natural-alpha"
    capture_dir.mkdir(parents=True, exist_ok=True)
    sheet.save(capture_dir / "core-oldtv-natural-alpha-sheet.png")
    save_alpha_composite_sheet(composite_records, capture_dir)
    report = {
        "source model": "ArtifactCore OldTV fringe only; C++ int truncation toward zero and edge clamp modeled.",
        "provenance": provenance,
        "hidden RGB pair construction": "Only alpha-zero RGB is changed; visible RGBA and alpha are asserted identical.",
        "compositing": "Straight source-over in normalized sRGB code-value space over black, mid-gray, white; alpha association is unverified and this is a measurement assumption.",
        "implementation source-policy probes": policy_probes,
        "straight-alpha composite sensitivity probes": [{k: v for k, v in record.items() if k not in ("display variants over mid-gray", "difference crop")} for record in composite_records],
        "alpha association parameterization probes": association_probes,
        "review crops": [{k: v for k, v in visual.items() if k != "variants"} for visual in visuals],
        "limits": "Source-derived Python models only. GPU HLSL integer hash, jitter, scanline, noise, alpha sample, and RGB saturate are translated to NumPy; no GPU execution or parity evidence. HDR/negative inputs and straight/premultiplied encodings are parameterized models on real alpha coverage. Actual effect-boundary alpha mode is path-dependent/unknown. No product caller, runtime, performance, or visual acceptance threshold.",
        "conditions": len(results), "results": results,
    }
    (capture_dir / "core-oldtv-natural-alpha-metrics.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    result = main()
    print(f"Measured {result['conditions']} natural OldTV alpha/fringe cases")
