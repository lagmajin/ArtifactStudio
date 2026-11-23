"""RD0: baseline for the seamless-texture lab.

The baseline is the naive "repeat the source image as a tile" result. Every
fixture is built so that the border does not match, so the baseline carries a
visible seam and gives the numbers that later runs are compared against.

Records seam metrics, repetition peak, absolute probes, and a review sheet.
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
from seamless_reference import absolute_probe, repetition_peak, seam_metrics, tile

REPEATS = 3
GATE_ITEMS = (
    "5.1 効果の正しさ", "5.2 画質と細部", "5.3 色と alpha", "5.4 時間安定性",
    "5.5 経路と解像度", "5.6 性能", "5.7 効果固有項目",
)


def _fmt(value, spec: str = ".4g") -> str:
    """Format a metric that may legitimately be None (e.g. no near-transparent pixels)."""
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


def run(output_rd0: Path) -> dict:
    fm = json.loads((output_rd0 / "fixtures_manifest.json").read_text(encoding="utf-8"))
    base_dir = output_rd0 / "baselines"
    base_dir.mkdir(parents=True, exist_ok=True)
    review_dir = output_rd0 / "review"
    review_dir.mkdir(parents=True, exist_ok=True)

    metrics = {
        "lab": "SeamlessTextureLab", "rd": "rd0", "stage": "naive_tile_baseline",
        "gates": "unset; no pass/fail judgment or quality ranking performed",
        "definitions": {
            "baseline": "the source image repeated as a tile; no synthesis",
            "seam_energy_ratio": "gradient energy on the tile border lines / gradient energy inside the tiles",
            "luminance_step": "mean |luma| difference across the tile border",
            "boundary_mode_energy": "share of the spectrum on the tile border frequencies",
            "repetition_peak_ratio": "self-similarity peak at 8%-50% lag / zero-lag autocorrelation",
        },
        "repeats": REPEATS,
        "fixtures": {},
    }
    outputs = {}
    sheet_items = []
    for key, entry in fm["fixtures"].items():
        rgba = np.load(output_rd0 / "fixtures" / entry["npy"])
        tiled = tile(rgba, REPEATS)
        stem = f"baseline_{key}"
        # The float tile is the measurement artifact; the repeated image is
        # written as an 8-bit review PNG only.
        np.save(base_dir / (stem + ".npy"), rgba.astype(np.float32))
        png = base_dir / (stem + ".png")
        cv2.imwrite(str(png), cv2.cvtColor(gf.linear_to_srgb_u8(tiled[..., :3]), cv2.COLOR_RGB2BGR))
        outputs[stem] = {"npy": stem + ".npy", "npy_sha256": sha256((base_dir / (stem + ".npy")).read_bytes()),
                         "png": stem + ".png", "png_sha256": sha256(png.read_bytes())}
        seam = seam_metrics(rgba, REPEATS)
        rep = repetition_peak(rgba)
        metrics["fixtures"][key] = {
            "seam": seam,
            "repetition": rep,
            "absolute_probe": absolute_probe(rgba),
        }
        sheet_items.append((key, png))

    sheet = review_dir / "rd0_baseline_sheet.png"
    contact_sheet(sheet_items, 3, 320, 200).save(sheet)

    lines = [
        "# RD0 naive-tile baseline tables (blank conclusion)", "",
        "No gate is set; no pass/fail or quality ranking is recorded.", "",
        "## Seam metrics of the naive tile", "",
        "| fixture | seam energy ratio | luminance step | rgb step | boundary mode energy | repetition peak ratio |",
        "|---|---:|---:|---:|---:|---:|",
    ]
    for key, d in metrics["fixtures"].items():
        s = d["seam"]
        lines.append(f"| {key} | {s['seam_energy_ratio']:.4f} | {s['luminance_step']:.6f} | "
                     f"{s['rgb_step']:.6f} | {s['boundary_mode_energy']:.3g} | "
                     f"{d['repetition']['repetition_peak_ratio']:.4f} |")
    lines.extend(["", "## Absolute probe", "",
                  "| fixture | visible max | near max | near >1 | nonfinite |",
                  "|---|---:|---:|---:|---:|"])
    for key, d in metrics["fixtures"].items():
        p = d["absolute_probe"]
        lines.append(f"| {key} | {_fmt(p['visible_max'])} | {_fmt(p['near_transparent_max'])} | "
                     f"{p['near_transparent_channels_gt_1']} | {p['nonfinite_count']} |")
    table = review_dir / "RD0_TABLES.md"
    table.write_text("\n".join(lines) + "\n", encoding="utf-8")

    form = review_dir / "GATE_FORM.md"
    form.write_text(
        "# RD0 gate form (thresholds intentionally blank)\n\n"
        "A reviewer independent of the implementer fills tolerances after viewing the RD0 baseline, "
        "before candidate comparison. Unfilled items remain unevaluated.\n\n"
        "| item | region | evidence | tolerance (BLANK) | judge | fix date | rationale |\n"
        "|---|---|---|---|---|---|---|\n"
        + "".join(f"| {item} | | | | | | |\n" for item in GATE_ITEMS),
        encoding="utf-8")

    metrics_path = output_rd0 / "metrics_rd0.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    manifest = {
        "lab": "SeamlessTextureLab", "rd": "rd0", "stage": "naive_tile_baseline", "date": "2026-11-18",
        "environment": {"python": platform.python_version(), "numpy": np.__version__,
                        "opencv": cv2.__version__, "platform": platform.platform()},
        "outputs": outputs,
        "evidence": {
            "metrics": {"file": metrics_path.name, "sha256": sha256(metrics_path.read_bytes())},
            "tables": {"file": f"review/{table.name}", "sha256": sha256(table.read_bytes())},
            "sheet": {"file": f"review/{sheet.name}", "sha256": sha256(sheet.read_bytes())},
            "form": {"file": f"review/{form.name}", "sha256": sha256(form.read_bytes())},
        },
        "gates": "unset; no judgment or quality ranking performed",
        "unverified": ["human visual review and quality gate", "real photo textures and their licences",
                       "product effect / UI / undo / save-reload", "preview and final render paths",
                       "CPU/GPU parity, D3D12/Vulkan", "production performance at 1080p / 4K"],
    }
    (output_rd0 / "manifest_rd0.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default="captures/rd0")
    args = parser.parse_args()
    output = Path(args.output)
    if not (output / "fixtures_manifest.json").exists():
        raise SystemExit(f"missing fixtures: {output}")
    manifest = run(output)
    print(f"RD0 baseline complete: {len(manifest['outputs'])} outputs -> {output}")


if __name__ == "__main__":
    main()
