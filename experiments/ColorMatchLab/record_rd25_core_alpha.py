"""Record actual Core alpha/hidden-RGB sensitivity from the authorized run."""
from __future__ import annotations

import csv
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent.parent
LOG = REPO / "temp" / "rd25_core_alpha.log"
RD13 = ROOT / "captures" / "rd13_transparent_hidden_rgb"
OUT = ROOT / "captures" / "rd25_core_alpha"
TEST = REPO / "tests" / "ArtifactCore" / "AutoColorMatcherContractTest.cpp"
CORE = REPO / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    rows = []
    for line in LOG.read_text(encoding="utf-8", errors="replace").splitlines():
        marker = "RD25_CORE_ALPHA,"
        if marker not in line:
            continue
        values = line[line.index(marker) + len(marker):].strip().split(",")
        if len(values) != 11:
            raise RuntimeError(f"Unexpected RD25 log row: {line}")
        method, intensity, pixels = values[:3]
        rows.append({
            "method": method,
            "intensity": float(intensity),
            "visible_pixels": int(pixels),
            "paired_rgb_mae": float(values[3]),
            "paired_rgb_p95": float(values[4]),
            "paired_rgb_max": float(values[5]),
            "paired_delta_e76_mean": float(values[6]),
            "paired_delta_e76_p95": float(values[7]),
            "paired_delta_e76_max": float(values[8]),
            "finite": values[9] == "1",
            "alpha_preserved": values[10] == "1",
        })
    expected = {(m, i) for m in ("Reinhard", "MeanStddev", "Histogram")
                for i in (0.5, 1.0)}
    if {(row["method"], row["intensity"]) for row in rows} != expected:
        raise RuntimeError(f"Expected six RD25 conditions, got {len(rows)}")

    OUT.mkdir(parents=True, exist_ok=True)
    metrics_path = OUT / "metrics.csv"
    fields = list(rows[0])
    with metrics_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)

    rd13_manifest = RD13 / "manifest.json"
    provenance = json.loads(rd13_manifest.read_text(encoding="utf-8"))
    manifest = {
        "rd": "rd25-actual-core-transparent-hidden-rgb-influence",
        "status": "synthetic alpha mechanism diagnostic; not a quality gate",
        "fixture": {
            "source_manifest_sha256": sha(rd13_manifest),
            "source_black_hidden_sha256": sha(RD13 / "source_black_hidden_rgba.npy"),
            "source_magenta_hidden_sha256": sha(RD13 / "source_magenta_hidden_rgba.npy"),
            "constructed_target_sha256": sha(RD13 / "constructed_target_rgb.npy"),
            "alpha_model": provenance["alpha"]["model"],
            "alpha_mask": provenance["alpha"]["mask"],
            "transparent_pixels": provenance["alpha"]["transparent_pixel_count"],
            "visible_pixels": provenance["alpha"]["visible_pixel_count"],
            "visible_rgb_and_alpha_identical": True,
            "hidden_rgb": provenance["hidden_rgb_variants"],
        },
        "measurement": {
            "route": "ArtifactCore AutoColorMatcher::match(ImageF32x4_RGBA&, ...)",
            "methods": ["Reinhard", "MeanStddev", "Histogram"],
            "intensities": [0.5, 1.0],
            "roi": "all fixed alpha>0 pixels",
            "statistics": "paired black-hidden vs magenta-hidden output RGB absolute differences per channel and Lab D65/2-degree DeltaE76; mean, nearest-rank p95, max",
            "color_interpretation": "encoded RGB treated as sRGB for descriptive Lab calculation",
            "acceptance": False,
        },
        "provenance": {
            "test_source_sha256": sha(TEST),
            "core_source_sha256": sha(CORE),
            "test_log": "temp/rd25_core_alpha.log",
            "test_log_sha256": sha(LOG),
            "metrics_csv": "metrics.csv",
            "metrics_sha256": sha(metrics_path),
        },
        "rows": rows,
        "limits": [
            "one CC0 portrait with a constructed reference and a synthetic binary-alpha strip",
            "no independent visual reviewer or quality acceptance threshold",
            "does not evaluate premultiplied inputs, semitransparent edges, or typical project prevalence",
            "does not replace a user-provided rights-cleared natural WB pair",
        ],
    }
    (OUT / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"capture": str(OUT), "rows": len(rows),
                      "metrics_sha256": sha(metrics_path),
                      "test_source_sha256": manifest["provenance"]["test_source_sha256"]},
                     indent=2))


if __name__ == "__main__":
    main()
