"""Write an evidence index for the selected image R&D priority runs."""

from __future__ import annotations

import hashlib
import json
import platform
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "captures" / "priority-rd-initial-runs.json"


def file_entry(relative: str) -> dict[str, str]:
    path = ROOT / relative
    return {
        "path": relative.replace("\\", "/"),
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
    }


def main() -> None:
    runs = [
        {
            "priority": 4,
            "theme": "chromatic aberration",
            "command": "python experiments/RetroImageLab/tools/core_chromatic_angle_study.py",
            "status": "completed",
            "summary": "12 amount levels, 360 angles per amount, 48 preview cases; Core source-equation mirror only.",
            "artifacts": [
                "captures/core-chromatic-angle/core-chromatic-angle-metrics.json",
                "captures/core-chromatic-angle/core-chromatic-angle-sheet.png",
                "fixtures/core-chromatic-angle-sweep.png",
            ],
        },
        {
            "priority": 11,
            "theme": "palette extraction",
            "command": "python experiments/RetroImageLab/tools/adaptive_palette_weighting_study.py",
            "status": "completed",
            "summary": "3 provenance-verified CC0 assets × 3 palette sizes × 2 weightings × 2 domains; offline median-cut, not Core SmartPaletteAnalyzer.",
            "artifacts": ["captures/adaptive-palette-weighting/adaptive-palette-weighting-metrics.json"],
        },
        {
            "priority": 13,
            "theme": "dithering",
            "command": "python experiments/RetroImageLab/retro_lab.py fixtures/color-space-fixture.png --dither-pattern-study",
            "status": "completed",
            "summary": "Bayer 4×4/8×8 and Floyd–Steinberg at two domains; one synthetic opaque landscape fixture; no gate.",
            "artifacts": ["captures/priority-rd/dither-pattern-study.png", "captures/priority-rd/dither-pattern-detail-crop.png"],
        },
        {
            "priority": 13,
            "theme": "temporal dither phase",
            "command": "python experiments/RetroImageLab/retro_lab.py fixtures/color-space-fixture.png --temporal-dither-study",
            "status": "completed_after_repair",
            "summary": "Translation fixture: screen-locked Bayer aligned MAE about 0.0285–0.0290; content-locked Bayer 0; per-frame Floyd–Steinberg about 0.0345. Not a look-quality verdict.",
            "artifacts": ["captures/priority-rd/temporal-dither-study.png", "captures/priority-rd/temporal-dither-detail-strip.png"],
            "repair": "Pass domain= by keyword in ordered_quantize_with_origin; previous positional call bound the string to origin_y and failed NumPy integer arithmetic.",
        },
        {
            "priority": 14,
            "theme": "pixel-art alpha/crop",
            "command": "python experiments/RetroImageLab/retro_lab.py fixtures/color-space-fixture.png --alpha-pixel-art-crop-study",
            "status": "completed",
            "summary": "Synthetic 515×257 RGBA chart; global, tile-local, and tile-origin resize strategies at 0.50/0.75 with alpha/edge/hidden-RGB measurements over three backgrounds.",
            "artifacts": ["fixtures/alpha-pixel-art-crop-fixture.png", "captures/priority-rd/alpha-pixel-art-crop-study.png"],
        },
    ]
    for run in runs:
        run["artifacts"] = [file_entry(path) for path in run["artifacts"]]
    record = {
        "purpose": "initial candidate evidence index; not product acceptance or a quality ranking",
        "python": sys.version,
        "platform": platform.platform(),
        "runs": runs,
        "limitations": [
            "No independent human review or pre-registered product quality gates.",
            "Studies are offline reference models; Core/Artifact runtime and GPU parity are not implied.",
            "Numerical response metrics do not establish aesthetic preference.",
        ],
    }
    OUT.write_text(json.dumps(record, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    print(f"Saved {OUT.relative_to(ROOT)} with {len(runs)} run records")


if __name__ == "__main__":
    main()
