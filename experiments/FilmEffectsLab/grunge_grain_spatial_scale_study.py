#!/usr/bin/env python3
"""Source-equation model of ArtifactFilmGrungeEffect's deterministic grain field."""
from __future__ import annotations

import hashlib
import json
import platform
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
PLAN = ROOT / "experiments/FilmEffectsLab/plans/grunge-grain-spatial-scale-r1.json"
PHOTO = ROOT / "experiments/GlowLab/assets/cc0-neon-streets-1536.jpg"
SOURCE = ROOT / "Artifact/src/Effect/ArtifactCreativeEffects.cpp"
OUT = ROOT / "experiments/FilmEffectsLab/captures/grunge-grain-spatial-scale-r1"
SEED, FRAME, AMOUNT = 1977, 0, np.float32(.075)
REQUESTS = (1.0, 1.25, 1.49, 1.5, 1.51, 2.49, 2.5, 2.51, 4.0, 8.0)
EXPECTED_PHOTO_SHA = "8791bd593b699be76dcfb181d72dd8655caaae2e88c235b89ed5d9097aa319d4"
U32 = np.uint32


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def make_synthetic(size: int = 1024) -> np.ndarray:
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    gray = np.full((size, size), .32, np.float32)
    gray[:, size//10:size//4] = .08
    gray[:, size//4:size*2//5] = .5
    gray[:, size*2//5:size*3//5] = .9
    gray[:, size*3//5:size*4//5] = .24 + .48 * xx[:, size*3//5:size*4//5] / size
    checker = ((xx.astype(np.int32)//8 + yy.astype(np.int32)//8) % 2) == 0
    gray[checker] = .68
    for k in range(16, size, 64):
        gray[max(0, k-1):k+1, :] = .96
        gray[:, max(0, k-1):k+1] = .03
    return np.stack((gray, np.clip(gray*.88+.04, 0, 1), np.clip(gray*.72+.09, 0, 1)), axis=-1).astype(np.float32)


def make_photo(size: int = 1024) -> np.ndarray:
    image = Image.open(PHOTO).convert("RGB")
    w, h = image.size
    side = min(w, h)
    left, top = (w-side)//2, (h-side)//2
    crop = image.crop((left, top, left+side, top+side)).resize((size, size), Image.Resampling.LANCZOS)
    return np.asarray(crop, dtype=np.float32) / np.float32(255.0)


def round_positive_like_std(value: float) -> int:
    # Inputs are positive. C++ std::round uses halfway-away-from-zero.
    return max(1, int(np.floor(np.float32(value) + np.float32(.5))))


def grain_field(width: int, height: int, grain_size: float, amount: float = float(AMOUNT)) -> tuple[np.ndarray, int, int]:
    grain_block = round_positive_like_std(grain_size)
    frame_seed = (SEED * 1664525 + FRAME * 1013904223) & 0xFFFFFFFF
    yy, xx = np.mgrid[0:height, 0:width]
    # Mirror explicit uint32 wrapping in the source hash.
    xb = (xx // grain_block).astype(U32)
    yb = (yy // grain_block).astype(U32)
    block_hash = U32(frame_seed) ^ (xb * U32(73856093)) ^ (yb * U32(19349663))
    low = (block_hash & U32(0xFFFF)).astype(np.float32)
    scalar = ((low / np.float32(32767.5) - np.float32(1.0)) * np.float32(amount)).astype(np.float32)
    field = np.empty((height, width, 3), np.float32)
    field[..., 0] = scalar * np.float32(.92)
    field[..., 1] = scalar
    field[..., 2] = scalar * np.float32(1.05)
    return field, grain_block, frame_seed


def correlation(a: np.ndarray, b: np.ndarray) -> float:
    av, bv = a.ravel().astype(np.float64), b.ravel().astype(np.float64)
    av -= av.mean(); bv -= bv.mean()
    denom = np.sqrt(np.mean(av*av) * np.mean(bv*bv))
    return float(np.mean(av*bv) / denom) if denom else 0.0


def display_png(rgb: np.ndarray, path: Path) -> None:
    # Diagnostic view: direct numeric values interpreted as display RGB, clipped and quantized.
    pixels = np.uint8(np.clip(rgb, 0, 1) * 255.0 + .5)
    Image.fromarray(pixels, "RGB").save(path)


def stats(src: np.ndarray, residual: np.ndarray, out: np.ndarray, grain_block: int) -> dict:
    h, w, _ = residual.shape
    ch = []
    for c in range(3):
        plane = residual[..., c]
        ch.append({
            "mean": float(np.mean(plane)), "stddev": float(np.std(plane)),
            "rms": float(np.sqrt(np.mean(plane*plane))), "min": float(np.min(plane)),
            "max": float(np.max(plane)), "unique_levels": int(np.unique(plane).size),
            "lag1_x_correlation": correlation(plane[:, :-1], plane[:, 1:]),
            "lag1_y_correlation": correlation(plane[:-1, :], plane[1:, :]),
        })
    # Re-expand each block's top-left value; also covers incomplete edge blocks
    # when the image dimensions are not divisible by the rounded grain size.
    block_reference = np.repeat(np.repeat(residual[::grain_block, ::grain_block], grain_block, axis=0), grain_block, axis=1)[:h, :w]
    block_error = residual - block_reference
    rerun, _, _ = grain_field(w, h, grain_block)
    zero_field, _, _ = grain_field(w, h, grain_block, amount=0.0)
    x_product = ((w-1)//grain_block) * 73856093
    y_product = ((h-1)//grain_block) * 19349663
    return {
        "channel_residual_stats": ch,
        "hash_level_count": int(np.unique(residual[..., 1]).size),
        "changed_pixel_fraction": float(np.mean(np.any(residual != 0, axis=2))),
        "within_block_max_abs_deviation": float(np.max(np.abs(block_error))),
        "within_block_exactly_constant": bool(np.max(np.abs(block_error)) == 0),
        "cpp_signed_int_multiply_overflow_possible": bool(x_product > 0x7FFFFFFF or y_product > 0x7FFFFFFF),
        "max_signed_product_x_assuming_int32": int(x_product),
        "max_signed_product_y_assuming_int32": int(y_product),
        "deterministic_field_repeat": bool(np.array_equal(residual, rerun)),
        "zero_amount_exact_identity": bool(np.array_equal(src + zero_field, src) and np.count_nonzero(zero_field) == 0),
        "output_min": float(out.min()), "output_max": float(out.max()),
        "below_zero_channel_values": int(np.count_nonzero(out < 0)),
        "above_one_channel_values": int(np.count_nonzero(out > 1)),
        "alpha_identity": True,
        "input_grain_residual_identity_max_error": float(np.max(np.abs((out-src)-residual))),
        "addition_roundtrip_within_one_float32_ulp": bool(np.max(np.abs((out-src)-residual)) <= np.finfo(np.float32).eps * max(1.0, float(np.max(np.abs(src))))),
    }


def main() -> None:
    plan = json.loads(PLAN.read_text(encoding="utf-8"))
    source_text = SOURCE.read_text(encoding="utf-8")
    markers = {
        "rounded_block": "std::round(impl_->grainSize)" in source_text,
        "hash_x": "(x / grainBlock) * 73856093" in source_text,
        "hash_y": "(y / grainBlock) * 19349663" in source_text,
        "hash_mask": "blockHash & 0xffffu" in source_text,
        "grain_amount": "* impl_->grainAmount" in source_text,
        "channel_weights": "channel == 2 ? 1.05f : channel == 0 ? 0.92f : 1.0f" in source_text,
    }
    if not all(markers.values()):
        raise RuntimeError(f"Source formula markers not all found: {markers}")
    photo_hash = sha(PHOTO)
    if photo_hash != EXPECTED_PHOTO_SHA:
        raise RuntimeError(f"Unexpected photo asset hash: {photo_hash}")
    OUT.mkdir(parents=True, exist_ok=True)
    report = {
        "plan_id": plan["plan_id"], "plan_sha256": sha(PLAN),
        "script_sha256": sha(Path(__file__)), "source_sha256": sha(SOURCE),
        "photo_sha256": photo_hash, "source_markers_verified": markers,
        "python": platform.python_version(), "numpy": np.__version__, "pillow": Image.__version__,
        "fixed_conditions": {"seed": SEED, "frame": FRAME, "grainAmount": float(AMOUNT), "alpha": 1.0,
                             "numeric_domain": "float32 normalized direct RGB; no transfer conversion"},
        "limits": "Independent intended modulo-2^32 equation model, not compiled Artifact output. C++ source performs signed int products before uint32 casts; these may overflow and are undefined by C++. PNG is clipped/quantized diagnostic only. No perceptual acceptance, transparent input, temporal sequence, product display transform, or runtime performance claim.",
        "conditions": [], "contact_sheets": [],
    }
    for material in ("synthetic", "photo"):
        full = make_synthetic(1024) if material == "synthetic" else make_photo(1024)
        plates = {1024: full, 512: np.asarray(Image.fromarray(np.uint8(np.clip(full, 0, 1)*255+.5), "RGB").resize((512, 512), Image.Resampling.LANCZOS), dtype=np.float32)/np.float32(255.0)}
        for size, src in plates.items():
            base_name = f"{material}-{size}"
            baseline = src.copy()
            display_png(baseline, OUT / f"{base_name}-baseline.png")
            previews: list[tuple[float, np.ndarray]] = []
            for requested in REQUESTS:
                residual, block, frame_seed = grain_field(size, size, requested)
                candidate = src + residual
                metric = stats(src, residual, candidate, block)
                tag = f"{base_name}-size-{requested:.2f}-block-{block}"
                display_png(candidate, OUT / f"{tag}.png")
                if requested in (1.0, 1.5, 2.5, 4.0, 8.0):
                    mid = candidate[size//2-128:size//2+128, size//2-128:size//2+128]
                    display_png(mid, OUT / f"{tag}-center256.png")
                previews.append((requested, candidate))
                report["conditions"].append({"material": material, "resolution": size,
                    "requested_grainSize": requested, "rounded_grainBlock_px": block,
                    "frameSeed_u32": frame_seed, "source_pixel_hash": hashlib.sha256(src.tobytes()).hexdigest(), **metric})
            cell, label_h = 128, 18
            sheet = Image.new("RGB", (cell*5, (cell+label_h)*2), (28, 29, 32))
            draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
            for i, (requested, candidate) in enumerate(previews):
                x, y = (i%5)*cell, (i//5)*(cell+label_h)
                block = round_positive_like_std(requested)
                draw.text((x+2, y+2), f"size {requested:.2f} -> {block}px", fill=(245,245,245), font=font)
                thumb = Image.fromarray(np.uint8(np.clip(candidate, 0, 1)*255+.5), "RGB").resize((cell, cell), Image.Resampling.NEAREST)
                sheet.paste(thumb, (x, y+label_h))
            sheet_path = OUT / f"{base_name}-grain-size-contact.png"
            sheet.save(sheet_path)
            report["contact_sheets"].append(str(sheet_path.relative_to(ROOT)))
    metrics_path = OUT / "metrics.json"
    metrics_path.write_text(json.dumps(report, indent=2, ensure_ascii=False)+"\n", encoding="utf-8", newline="\n")
    checksum = hashlib.sha256(metrics_path.read_bytes()).hexdigest()
    (OUT / "metrics.sha256").write_text(f"{checksum}  metrics.json\n", encoding="ascii", newline="\n")
    print(f"Wrote {len(report['conditions'])} source-equation conditions to {OUT}")


if __name__ == "__main__":
    main()
