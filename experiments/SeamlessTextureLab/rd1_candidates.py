"""RD1: seamless-texture candidates (mirror / quilt / periodic) versus the
naive-tile baseline.

Each candidate synthesises a tile from a source image and is measured with the
same seam metrics as the baseline. The alpha fixture is additionally run
through the straight and premultiplied descriptors to check the alpha contract.

Gates stay unset; no pass/fail judgment and no quality ranking is performed.
"""

from __future__ import annotations

import argparse
import json
import platform
import time
import tracemalloc
from pathlib import Path

import cv2
import numpy as np

import generate_fixtures as gf
from seamless_reference import (absolute_probe, descriptor_delta, repetition_peak,
                                seam_metrics, synthesize, tile)

REPEATS = 3
BLIND_SEED = gf.SEED + 700
CANDIDATES = {
    "st_mirror": {"model": "A"},
    "st_quilt": {"model": "B"},
    "st_periodic": {"model": "C"},
}
GATE_ITEMS = (
    "5.1 効果の正しさ", "5.2 画質と細部", "5.3 色と alpha", "5.4 時間安定性",
    "5.5 経路と解像度", "5.6 性能", "5.7 効果固有項目",
)


def _fmt(value, spec: str = ".4g") -> str:
    """Format a metric that may legitimately be None."""
    if value is None:
        return "n/a"
    try:
        return format(value, spec)
    except (TypeError, ValueError):
        return str(value)


def sha256(data: bytes) -> str:
    import hashlib
    return hashlib.sha256(data).hexdigest()


def contact_sheet(items: list[tuple[str, Path]], cols: int, cell_w: int, cell_h: int):
    from PIL import Image, ImageDraw
    rows = (len(items) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * cell_w, rows * (cell_h + 16)), (32, 32, 32))
    draw = ImageDraw.Draw(sheet)
    for i, (label, path) in enumerate(items):
        img = Image.open(path).convert("RGB")
        img.thumbnail((cell_w - 4, cell_h - 4))
        x, y = (i % cols) * cell_w, (i // cols) * (cell_h + 16)
        sheet.paste(img, (x + 2, y + 2))
        draw.text((x + 3, y + cell_h), label, fill=(230, 230, 230))
    return sheet


def run(output_rd0: Path, output_rd1: Path) -> dict:
    fm = json.loads((output_rd0 / "fixtures_manifest.json").read_text(encoding="utf-8"))
    output_rd1.mkdir(parents=True, exist_ok=True)
    cand_dir = output_rd1 / "candidates"
    cand_dir.mkdir(parents=True, exist_ok=True)
    review_dir = output_rd1 / "review"
    review_dir.mkdir(parents=True, exist_ok=True)

    metrics = {
        "lab": "SeamlessTextureLab", "rd": "rd1", "stage": "seamless_candidates",
        "gates": "unset; no pass/fail judgment or quality ranking performed",
        "candidates": CANDIDATES,
        "repeats": REPEATS,
        "definitions": {
            "st_mirror": "reflect the tile across both borders; seamless by construction",
            "st_quilt": "half-period offset re-joined along a dynamic-programming minimum-cost seam",
            "st_periodic": "torus (circular FFT) low-pass; the field is periodic by construction",
            "seam_energy_ratio": "gradient energy on the tile border lines / gradient energy inside the tiles",
        },
        "fixtures": {},
        "descriptor_check": {},
        "performance": {},
    }

    outputs = {}
    for key, entry in fm["fixtures"].items():
        source = np.load(output_rd0 / "fixtures" / entry["npy"])
        baseline_seam = seam_metrics(source, REPEATS)
        metrics["fixtures"][key] = {"baseline": baseline_seam,
                                    "baseline_repetition": repetition_peak(source),
                                    "candidates": {}}
        for name, spec in CANDIDATES.items():
            synthesized = synthesize(source, spec, descriptor="straight")
            tiled = tile(synthesized, REPEATS)
            stem = f"{key}_{name}"
            # Keep the float tile as the measurement artifact and the repeated
            # image as an 8-bit review PNG; storing the repeated float buffer
            # would multiply the capture size by repeats^2.
            np.save(cand_dir / (stem + ".npy"), synthesized.astype(np.float32))
            png = cand_dir / (stem + ".png")
            cv2.imwrite(str(png), cv2.cvtColor(gf.linear_to_srgb_u8(tiled[..., :3]), cv2.COLOR_RGB2BGR))
            outputs[stem] = {"npy": stem + ".npy", "npy_sha256": sha256((cand_dir / (stem + ".npy")).read_bytes()),
                             "png": stem + ".png", "png_sha256": sha256(png.read_bytes())}
            seam = seam_metrics(synthesized, REPEATS)
            metrics["fixtures"][key]["candidates"][name] = {
                "seam": seam,
                "seam_ratio_vs_baseline": (seam["seam_energy_ratio"] / baseline_seam["seam_energy_ratio"]
                                           if baseline_seam["seam_energy_ratio"] else float("nan")),
                "repetition": repetition_peak(synthesized),
                "absolute_probe": absolute_probe(synthesized),
                "tile_shape": list(synthesized.shape),
            }

    alpha_key = "alpha_patch_small"
    if alpha_key in fm["fixtures"]:
        straight = np.load(output_rd0 / "fixtures" / fm["fixtures"][alpha_key]["npy"])
        premul = np.load(output_rd0 / "fixtures" / fm["fixtures"][alpha_key]["premultiplied"]["npy"])
        for name, spec in CANDIDATES.items():
            s = synthesize(straight, spec, descriptor="straight")
            p = synthesize(premul, spec, descriptor="premultiplied")
            metrics["descriptor_check"][name] = {
                "descriptor_delta": descriptor_delta(s, p),
                "alpha_delta": float(np.max(np.abs(s[..., 3] - p[..., 3]))),
                "absolute_probe_straight": absolute_probe(s),
            }

    for name, spec in CANDIDATES.items():
        source = np.load(output_rd0 / "fixtures" / fm["fixtures"]["clouds_small"]["npy"])
        synthesize(source, spec, descriptor="straight")
        times = []
        for _ in range(3):
            tracemalloc.start()
            t0 = time.perf_counter()
            synthesize(source, spec, descriptor="straight")
            times.append(time.perf_counter() - t0)
            _, peak = tracemalloc.get_traced_memory()
            tracemalloc.stop()
        metrics["performance"][name] = {
            "seconds_median": float(np.median(times)), "tracemalloc_peak_bytes": int(peak),
            "fixture": "clouds_small",
        }

    lines = [
        "# RD1 seamless candidate tables (blank conclusion)", "",
        "No gate is set; no pass/fail or quality ranking is recorded.", "",
        "## Seam energy ratio (1.0 means the border is indistinguishable from the interior)", "",
        "| fixture | baseline | st_mirror | st_quilt | st_periodic |", "|---|---:|---:|---:|---:|",
    ]
    for key, d in metrics["fixtures"].items():
        c = d["candidates"]
        lines.append(f"| {key} | {d['baseline']['seam_energy_ratio']:.4f} | "
                     + " | ".join(f"{c[n]['seam']['seam_energy_ratio']:.4f}" for n in CANDIDATES) + " |")
    lines.extend(["", "## Luminance step across the tile border", "",
                  "| fixture | baseline | st_mirror | st_quilt | st_periodic |", "|---|---:|---:|---:|---:|"])
    for key, d in metrics["fixtures"].items():
        c = d["candidates"]
        lines.append(f"| {key} | {d['baseline']['luminance_step']:.6f} | "
                     + " | ".join(f"{c[n]['seam']['luminance_step']:.6f}" for n in CANDIDATES) + " |")
    lines.extend(["", "## Boundary mode energy share", "",
                  "| fixture | baseline | st_mirror | st_quilt | st_periodic |", "|---|---:|---:|---:|---:|"])
    for key, d in metrics["fixtures"].items():
        c = d["candidates"]
        lines.append(f"| {key} | {d['baseline']['boundary_mode_energy']:.3g} | "
                     + " | ".join(f"{c[n]['seam']['boundary_mode_energy']:.3g}" for n in CANDIDATES) + " |")
    lines.extend(["", "## Repetition peak ratio (duplicated motif)", "",
                  "| fixture | baseline | st_mirror | st_quilt | st_periodic |", "|---|---:|---:|---:|---:|"])
    for key, d in metrics["fixtures"].items():
        c = d["candidates"]
        lines.append(f"| {key} | {d['baseline_repetition']['repetition_peak_ratio']:.4f} | "
                     + " | ".join(f"{c[n]['repetition']['repetition_peak_ratio']:.4f}" for n in CANDIDATES) + " |")
    lines.extend(["", "## Descriptor / absolute probe", "",
                  "| candidate | descriptor delta | alpha delta | near >1 | nonfinite |",
                  "|---|---:|---:|---:|---:|"])
    for name, d in metrics["descriptor_check"].items():
        p = d["absolute_probe_straight"]
        lines.append(f"| {name} | {d['descriptor_delta']:.3g} | {d['alpha_delta']:.3g} | "
                     f"{p['near_transparent_channels_gt_1']} | {p['nonfinite_count']} |")
    lines.extend(["", "## Performance (clouds_small)", "",
                  "| candidate | median s | tracemalloc peak |", "|---|---:|---:|"])
    for name, p in metrics["performance"].items():
        lines.append(f"| {name} | {p['seconds_median']:.4f} | {p['tracemalloc_peak_bytes']} |")
    table = review_dir / "RD1_TABLES.md"
    table.write_text("\n".join(lines) + "\n", encoding="utf-8")

    form = review_dir / "GATE_FORM.md"
    form.write_text(
        "# RD1 gate form (thresholds intentionally blank)\n\n"
        "A reviewer independent of the implementer fills tolerances after viewing the RD0 baseline, "
        "before candidate comparison. Unfilled items remain unevaluated.\n\n"
        "Judge the blind sheet for: is the tile border visible when repeated, does the texture still "
        "read as a continuous surface, is the motif obviously duplicated, and is the blend band of the "
        "quilt candidate acceptable. The numeric tables are descriptive; they do not decide quality.\n\n"
        "| item | region | evidence | tolerance (BLANK) | judge | fix date | rationale |\n"
        "|---|---|---|---|---|---|---|\n"
        + "".join(f"| {item} | | | | | | |\n" for item in GATE_ITEMS),
        encoding="utf-8")

    records = []
    for key in fm["fixtures"]:
        for name in CANDIDATES:
            records.append({"fixture": key, "candidate": name})
    rng = np.random.default_rng(BLIND_SEED)
    order = rng.permutation(len(records))
    blind_key = {}
    items = []
    for i, idx in enumerate(order):
        bid = f"S{i + 1:02d}"
        rec = records[int(idx)]
        blind_key[bid] = rec
        path = cand_dir / f"{rec['fixture']}_{rec['candidate']}.png"
        items.append((bid, path))
    sheet = review_dir / "rd1_candidate_blind_sheet.png"
    contact_sheet(items, 4, 300, 190).save(sheet)
    key_path = output_rd1 / "blind_key.json"
    key_path.write_text(json.dumps(blind_key, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    blind_form = review_dir / "CANDIDATE_BLIND_FORM.md"
    blind_form.write_text(
        "# RD1 candidate blind review form (blank)\n\n"
        "Do not score until GATE_FORM.md thresholds are fixed. Judge the visible seam when the tile is "
        "repeated, the continuity of the texture, and the duplication of the motif.\n\n"
        "| blind ID | visible seam | continuity | duplicated motif | reviewer | date |\n"
        "|---|---|---|---|---|---|\n" + "".join(f"| {b} | | | | | |\n" for b in blind_key),
        encoding="utf-8")

    metrics_path = output_rd1 / "metrics_rd1.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    manifest = {
        "lab": "SeamlessTextureLab", "rd": "rd1", "stage": "seamless_candidates", "date": "2026-11-18",
        "environment": {"python": platform.python_version(), "numpy": np.__version__,
                        "opencv": cv2.__version__, "platform": platform.platform()},
        "blind_seed": BLIND_SEED, "outputs": outputs,
        "evidence": {
            "metrics": {"file": metrics_path.name, "sha256": sha256(metrics_path.read_bytes())},
            "tables": {"file": f"review/{table.name}", "sha256": sha256(table.read_bytes())},
            "blind_sheet": {"file": f"review/{sheet.name}", "sha256": sha256(sheet.read_bytes())},
            "blind_key": {"file": key_path.name, "sha256": sha256(key_path.read_bytes())},
            "form": {"file": f"review/{form.name}", "sha256": sha256(form.read_bytes())},
            "gate_form": {"file": f"review/{form.name}", "sha256": sha256(form.read_bytes())},
        },
        "gates": "unset; no judgment or quality ranking performed",
        "unverified": ["human visual review and quality gate", "real photo textures and their licences",
                       "perceived duplication and blend band quality", "product effect / UI / undo / save-reload",
                       "preview and final render paths", "CPU/GPU parity, D3D12/Vulkan",
                       "production performance at 1080p / 4K"],
    }
    (output_rd1 / "manifest_rd1.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rd0", default="captures/rd0")
    parser.add_argument("--output", default="captures/rd1")
    args = parser.parse_args()
    rd0 = Path(args.rd0)
    if not (rd0 / "fixtures_manifest.json").exists():
        raise SystemExit(f"missing RD0 fixtures: {rd0}")
    manifest = run(rd0, Path(args.output))
    print(f"RD1 seamless candidates complete: {len(manifest['outputs'])} outputs -> {args.output}")


if __name__ == "__main__":
    main()
