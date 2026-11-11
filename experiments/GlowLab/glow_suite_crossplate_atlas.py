"""Build a cross-plate visual atlas for the five ordered Glow suite recipes."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2
import numpy as np

from glow_lab import add_label


PLATES = {
    "CITY / central intersection": (
        "glow-suite-ordered-stacks-cc0-neon-cpu-rd3",
        "central_intersection",
    ),
    "PORTRAIT / streetlamp context": (
        "glow-suite-ordered-stacks-portrait-cpu-rd1",
        "streetlamp_context",
    ),
    "STREETLIGHT / both bulbs": (
        "glow-suite-ordered-stacks-cc0-streetlight-cpu-rd1",
        "both_bulbs",
    ),
}
RECIPE_ORDER = (
    "contour-streak",
    "reactive-spectrum",
    "film-optical",
    "refractive-ridge",
    "point-rays",
)
ROOT = Path(__file__).resolve().parents[2]


def crop_normalized(image: np.ndarray, bounds: list[float]) -> np.ndarray:
    height, width = image.shape[:2]
    x0, y0, x1, y1 = bounds
    left, top = int(x0 * width), int(y0 * height)
    right, bottom = int(x1 * width), int(y1 * height)
    return image[top:bottom, left:right]


def fit(image: np.ndarray, size: tuple[int, int]) -> np.ndarray:
    width, height = size
    h, w = image.shape[:2]
    scale = min(width / w, height / h)
    resized = cv2.resize(image, (max(1, round(w * scale)), max(1, round(h * scale))),
                         interpolation=cv2.INTER_AREA if scale < 1 else cv2.INTER_CUBIC)
    canvas = np.zeros((height, width, 3), dtype=np.uint8)
    y = (height - resized.shape[0]) // 2
    x = (width - resized.shape[1]) // 2
    canvas[y:y + resized.shape[0], x:x + resized.shape[1]] = resized
    return canvas


def run(output: Path) -> None:
    columns: list[np.ndarray] = []
    evidence: dict[str, object] = {}
    for plate_name, (capture_name, roi_name) in PLATES.items():
        capture_dir = ROOT / "experiments/GlowLab/captures" / capture_name
        metrics_path = capture_dir / "review/ordered-stack-suite-metrics.json"
        metrics = json.loads(metrics_path.read_text(encoding="utf-8"))
        manifest = json.loads((capture_dir / "ordered-stack-manifest.json").read_text(encoding="utf-8"))
        source_path = ROOT / Path(manifest["input"].replace("\\", "/"))
        foundation_path = capture_dir / "shared-foundation-glow-cpu.png"
        foundation = cv2.imread(str(foundation_path), cv2.IMREAD_COLOR)
        if foundation is None:
            raise RuntimeError(f"Could not read source plate: {source_path}")
        bounds = metrics["roi_bounds_normalized"][roi_name]
        foundation_crop = crop_normalized(foundation, bounds)
        recipe_index = {row["recipe"]: row for row in metrics["recipe_results"]}
        tiles: list[np.ndarray] = [add_label(np.zeros((246, 450, 3), dtype=np.uint8), plate_name)]
        recipe_evidence: dict[str, object] = {}
        for recipe in RECIPE_ORDER:
            row = recipe_index[recipe]
            final_path = capture_dir / row["final_output"]
            final = cv2.imread(str(final_path), cv2.IMREAD_COLOR)
            if final is None:
                raise RuntimeError(f"Could not read {recipe} output: {final_path}")
            final_crop = crop_normalized(final, bounds)
            delta_x5 = np.clip(np.abs(final_crop.astype(np.float32) -
                                      foundation_crop.astype(np.float32)) * 5.0,
                               0, 255).astype(np.uint8)
            foundation_view = fit(foundation_crop, (142, 205))
            final_view = fit(final_crop, (142, 205))
            delta_view = fit(delta_x5, (142, 205))
            tile = np.zeros((246, 450, 3), dtype=np.uint8)
            tile[28:233, 4:146] = foundation_view
            tile[28:233, 154:296] = final_view
            tile[28:233, 304:446] = delta_view
            cv2.putText(tile, "FOUNDATION", (5, 22), cv2.FONT_HERSHEY_SIMPLEX,
                        0.40, (210, 210, 210), 1, cv2.LINE_AA)
            cv2.putText(tile, recipe, (155, 22), cv2.FONT_HERSHEY_SIMPLEX,
                        0.42, (235, 235, 235), 1, cv2.LINE_AA)
            cv2.putText(tile, "DELTA X5", (305, 22), cv2.FONT_HERSHEY_SIMPLEX,
                        0.40, (210, 210, 210), 1, cv2.LINE_AA)
            tiles.append(tile)
            recipe_result = row.get("incremental_clip_gate_against_foundation", {})
            recipe_evidence[recipe] = {
                "output": str(final_path.relative_to(ROOT)),
                "final_alpha_mae_against_source": row.get("final_alpha_mae_against_source"),
                "incremental_clip_gate_against_foundation": recipe_result,
            }
        columns.append(np.vstack(tiles))
        evidence[plate_name] = {
            "source": str(source_path.relative_to(ROOT)),
            "foundation": str(foundation_path.relative_to(ROOT)),
            "input_sha256": metrics["input_sha256"],
            "roi_name": roi_name,
            "roi_bounds_normalized": bounds,
            "recipe_results": recipe_evidence,
            "capture_class": metrics["capture_class"],
            "limitations": metrics.get("limitations", []),
        }

    output.mkdir(parents=True, exist_ok=True)
    sheet = np.hstack(columns)
    sheet_path = output / "glow-suite-crossplate-recipe-atlas.jpg"
    cv2.imwrite(str(sheet_path), sheet, [cv2.IMWRITE_JPEG_QUALITY, 95])
    report = {
        "intent": "visual review atlas for recipe diversity across three separate source plates",
        "recipes": list(RECIPE_ORDER),
        "not_a_quality_ranking_or_acceptance": True,
        "plates": evidence,
        "limitations": [
            "each plate uses its own image and foundation calibration",
            "the ordered recipes use an 8-bit PNG handoff rather than one float-preserving stack",
            "installed CPU effect captures are not final Composition Viewer or GPU parity",
            "visual aesthetic acceptance remains open",
        ],
    }
    report_path = output / "glow-suite-crossplate-recipe-atlas.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Saved {sheet_path}")
    print(f"Saved {report_path}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.output)


if __name__ == "__main__":
    main()
