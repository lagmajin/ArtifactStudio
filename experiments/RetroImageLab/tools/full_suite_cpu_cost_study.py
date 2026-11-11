"""Benchmark the standalone Python suite and estimate traced peak allocations."""

from __future__ import annotations

import gc
import json
import platform
import statistics
import sys
import time
import tracemalloc
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tools"))
from retro_lab import PRESETS, apply_retro  # noqa: E402

CAPTURE = ROOT / "captures" / "full-suite-cpu-cost"
SIZES = (128, 256, 512, 1024)
SEED = 31039
WARMUPS = 2
REPEATS = 9
ABLATION_SIZES = (256, 1024)
ABLATIONS = ("grain", "aberration", "scanlines", "scale")


def make_source(size: int) -> Image.Image:
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    r = 0.22 + 0.52 * xx / max(size - 1, 1)
    g = 0.24 + 0.50 * yy / max(size - 1, 1)
    b = 0.28 + 0.38 * (xx + yy) / max(2 * (size - 1), 1)
    highlight = np.exp(-((xx - size * .68) ** 2 + (yy - size * .34) ** 2) / (2 * (size * .11) ** 2))
    rgb = np.stack((r + .12 * highlight, g + .08 * highlight, b + .04 * highlight), axis=2)
    rgba = np.concatenate((np.uint8(np.clip(rgb * 255 + 0.5, 0, 255)),
                           np.full((size, size, 1), 255, dtype=np.uint8)), axis=2)
    return Image.fromarray(rgba, "RGBA")


def timed_samples(source: Image.Image, settings: dict[str, object]) -> list[float]:
    for _ in range(WARMUPS):
        apply_retro(source, **settings, seed=SEED)
    values = []
    for _ in range(REPEATS):
        begin = time.perf_counter_ns()
        apply_retro(source, **settings, seed=SEED)
        values.append((time.perf_counter_ns() - begin) / 1e6)
    return values


def traced_peak(source: Image.Image, settings: dict[str, object]) -> dict[str, int]:
    gc.collect()
    tracemalloc.start()
    baseline, _ = tracemalloc.get_traced_memory()
    tracemalloc.reset_peak()
    apply_retro(source, **settings, seed=SEED)
    current, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return {"peak traced bytes above baseline": max(0, peak - baseline),
            "retained traced bytes above baseline": max(0, current - baseline)}


def stats(samples: list[float]) -> dict[str, float]:
    return {"median ms": float(statistics.median(samples)),
            "p95 ms": float(np.percentile(samples, 95)),
            "minimum ms": float(min(samples)),
            "maximum ms": float(max(samples)),
            "samples": len(samples)}


def make_chart(rows: list[dict[str, object]]) -> Path:
    width, row_h, label_w = 900, 70, 185
    height = 60 + len(rows) * row_h
    image = Image.new("RGB", (width, height), (35, 36, 40))
    draw, font = ImageDraw.Draw(image), ImageFont.load_default()
    colors = {128: (90, 157, 198), 256: (105, 185, 139), 512: (218, 177, 87), 1024: (207, 112, 104)}
    draw.text((label_w, 12), "Median wall time (ms); bar length is linear within row", fill=(240, 240, 240), font=font)
    max_time = max(r["full suite"]["median ms"] for r in rows)
    for index, row in enumerate(rows):
        top = 48 + index * row_h
        draw.text((5, top + 22), f"{row['preset']}", fill=(245, 245, 245), font=font)
        draw.text((5, top + 36), f"{row['size']}px", fill=(183, 187, 194), font=font)
        x = label_w
        maxbar = width - label_w - 110
        value = row["full suite"]["median ms"]
        bar = int(maxbar * value / max_time)
        draw.rectangle((x, top + 21, x + bar, top + 43), fill=colors[row["size"][0]])
        draw.text((x + bar + 6, top + 27), f"{value:.2f} ms", fill=(245, 245, 245), font=font)
    CAPTURE.mkdir(parents=True, exist_ok=True)
    path = CAPTURE / "full-suite-cpu-cost-chart.png"
    image.save(path)
    return path


def main() -> dict[str, object]:
    results: list[dict[str, object]] = []
    ablations: list[dict[str, object]] = []
    for preset_name, preset in PRESETS.items():
        for size in SIZES:
            source = make_source(size)
            settings = dict(preset)
            samples = timed_samples(source, settings)
            memory = traced_peak(source, settings)
            results.append({
                "preset": preset_name, "size": [size, size],
                "input bytes RGBA8": size * size * 4,
                "output bytes RGBA8": size * size * 4,
                "full suite": stats(samples),
                "memory": memory,
                "full-frame float32 RGB bytes": size * size * 3 * 4,
            })
            if size in ABLATION_SIZES:
                full_ms = statistics.median(samples)
                for stage in ABLATIONS:
                    if stage == "scale" and float(preset["scale"]) >= 1.0:
                        continue
                    disabled = dict(preset)
                    if stage == "scale":
                        disabled[stage] = 1.0
                    else:
                        disabled[stage] = 0.0
                    ablation_samples = timed_samples(source, disabled)
                    ablation_ms = statistics.median(ablation_samples)
                    ablations.append({
                        "preset": preset_name, "size": size, "disabled stage": stage,
                        "ablated pipeline median ms": ablation_ms,
                        "full pipeline median ms": full_ms,
                        "median time difference ms": full_ms - ablation_ms,
                        "median time change percent": ((full_ms - ablation_ms) / full_ms * 100.0) if full_ms else 0.0,
                        "interpretation": "counterfactual pipeline difference; not an additive isolated stage cost",
                    })
    chart = make_chart(results)
    report = {
        "question": "How does the standalone Python suite's CPU wall time and traced allocation peak scale with output resolution, and which stage ablations change total runtime?",
        "evaluation plan": {
            "purpose": "Characterize research-runner cost only, with exact current apply_retro recipes; do not infer Core/GPU performance.",
            "input": "Deterministic opaque RGBA synthetic smooth-gradient image created at each tested size; no disk decode or output file write inside timed region.",
            "conditions": "Current four PRESETS, 128/256/512/1024 square, seed 31039, 2 warm-up calls and 9 timed repetitions per case using perf_counter_ns. Timed end-to-end calls invoke apply_retro and discard the returned image.",
            "stage attribution": "At 256/1024, compare full preset to one-stage-disabled counterfactuals (grain, aberration, scanline, and pixelation scale where active), each warmed and repeated. Time differences include changed array work/allocation/cache effects and are not additive stage timings.",
            "memory": "tracemalloc peak and retained bytes above call baseline, plus explicit RGBA8 input/output and one float32 RGB-frame size. NumPy buffers are traced by this environment; Pillow/native allocator peak outside Python tracing may be missed.",
            "acceptance": "No performance budget or target hardware acceptance threshold exists; descriptive benchmark only.",
            "execution environment": {"python": platform.python_version(), "numpy": np.__version__, "pillow": Image.__version__,
                                     "platform": platform.platform(), "processor": platform.processor(),
                                     "logical CPU count": __import__("os").cpu_count()},
            "unverified": "ArtifactStudio C++/Core/GPU runtime, preview/export, production decoder, multithread behavior, device GPU time, VRAM, end-to-end IO and user latency.",
        },
        "warmups": WARMUPS, "repetitions": REPEATS, "seed": SEED,
        "results": results, "ablation timings": ablations,
        "chart": str(chart.relative_to(ROOT)),
        "limits": "Standalone Python benchmark on one machine. Python heap tracing is not a complete process RSS/GPU memory measurement. Wall-time medians/p95 are sensitive to OS scheduling and background load; the 9 samples are repeats on one process, not independent machine runs.",
    }
    CAPTURE.mkdir(parents=True, exist_ok=True)
    (CAPTURE / "full-suite-cpu-cost-metrics.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    main()
    print("Benchmarked 4 suite presets at 4 resolutions with warmed repeats.")
