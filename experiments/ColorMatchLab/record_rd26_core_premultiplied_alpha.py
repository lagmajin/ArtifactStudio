"""Record the predeclared Core premultiplied-alpha numeric probe."""
from __future__ import annotations

import csv
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent.parent
LOG = REPO / "temp" / "rd26_core_premultiplied.log"
OUT = ROOT / "captures" / "rd26_core_premultiplied"
TEST = REPO / "tests" / "ArtifactCore" / "AutoColorMatcherContractTest.cpp"
CORE = REPO / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    rows = []
    for line in LOG.read_text(encoding="utf-8", errors="replace").splitlines():
        marker = "RD26_CORE_PREMULT,"
        if marker not in line:
            continue
        values = line[line.index(marker) + len(marker):].strip().split(",")
        if len(values) != 9:
            raise RuntimeError(f"Unexpected RD26 log row: {line}")
        method, intensity, pixels, below, above, excess, transparent, finite, alpha = values
        rows.append({
            "method": method,
            "intensity": float(intensity),
            "pixels": int(pixels),
            "rgb_channels_below_zero": int(below),
            "rgb_channels_above_alpha": int(above),
            "max_excess_over_alpha": float(excess),
            "transparent_nonzero_rgb_channels": int(transparent),
            "finite": finite == "1",
            "alpha_preserved": alpha == "1",
        })
    expected = {(method, intensity) for method in ("Reinhard", "MeanStddev", "Histogram")
                for intensity in (0.5, 1.0)}
    if {(row["method"], row["intensity"]) for row in rows} != expected:
        raise RuntimeError(f"Expected six RD26 conditions, got {len(rows)}")

    OUT.mkdir(parents=True, exist_ok=True)
    metrics_path = OUT / "metrics.csv"
    with metrics_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    manifest = {
        "rd": "rd26-actual-core-premultiplied-alpha-numeric-association",
        "status": "synthetic Core alpha mechanism diagnostic; no quality gate",
        "fixture": {
            "construction": "64x64 deterministic RGB gradient, same spatial alpha ramp 0..1, source/reference stored as premultiplied BGRA float32",
            "color_descriptor": "canonicalLinearPremultipliedBgra (linear sRGB/Rec.709 primaries)",
            "width": 64,
            "height": 64,
            "alpha_zero_pixels": 64,
            "fractional_alpha_pixels": 3968,
        },
        "measurement": {
            "route": "ArtifactCore AutoColorMatcher::match(ImageF32x4_RGBA&, ...)",
            "methods": ["Reinhard", "MeanStddev", "Histogram"],
            "intensities": [0.5, 1.0],
            "statistics": "finite status, exact alpha preservation, count of RGB channels below zero/above alpha, max RGB excess over alpha, nonzero RGB channels at alpha zero",
            "acceptance": False,
        },
        "provenance": {
            "test_source_sha256": sha(TEST),
            "core_source_sha256": sha(CORE),
            "test_log": "temp/rd26_core_premultiplied.log",
            "test_log_sha256": sha(LOG),
            "metrics_csv": "metrics.csv",
            "metrics_sha256": sha(metrics_path),
        },
        "rows": rows,
        "limits": [
            "one generated deterministic synthetic gradient, not production imagery",
            "no display composites, visual review, or acceptance threshold",
            "does not establish expected alpha behavior for every caller",
            "does not replace the rights-cleared user-provided WB pair",
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
