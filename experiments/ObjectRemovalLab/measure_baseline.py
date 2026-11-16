from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parent
CAPTURE = ROOT / "captures" / "rd0"


def summarize(values: np.ndarray) -> dict[str, float]:
    if values.size == 0:
        return {"mae": 0.0, "rmse": 0.0, "p95": 0.0, "max": 0.0}
    absolute = np.abs(values.astype(np.float64)).reshape(-1)
    return {
        "mae": float(np.mean(absolute)),
        "rmse": float(np.sqrt(np.mean(np.square(absolute)))),
        "p95": float(np.percentile(absolute, 95)),
        "max": float(np.max(absolute)),
    }


def main() -> None:
    manifest_path = CAPTURE / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    results = []
    for case in manifest["cases"]:
        files = case["files"]
        clean = np.load(ROOT / files["clean"]["path"], allow_pickle=False)
        corrupted = np.load(ROOT / files["input"]["path"], allow_pickle=False)
        mask = np.asarray(Image.open(ROOT / files["mask"]["path"])) > 0
        dilated = np.asarray(Image.fromarray(mask.astype(np.uint8) * 255).filter(
            __import__("PIL.ImageFilter", fromlist=["MaxFilter"]).MaxFilter(7))) > 0
        boundary = dilated & ~mask
        results.append({
            "id": case["id"],
            "mask_pixels": int(mask.sum()),
            "boundary_pixels": int(boundary.sum()),
            "masked_error": summarize(corrupted[mask] - clean[mask]),
            "boundary_error": summarize(corrupted[boundary] - clean[boundary]),
            "outside_mask_max_abs": float(np.max(np.abs(corrupted[~mask] - clean[~mask]))),
        })
    record = {
        "rd": "rd0",
        "purpose": "corrupted-input baseline against known synthetic clean plate; not a candidate result or quality gate",
        "input_manifest_sha256": hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
        "metric_space": "linear RGB float32; absolute error aggregated per channel sample",
        "boundary_definition": "7x7 binary dilation of mask minus original mask (3px neighborhood)",
        "cases": results,
    }
    (CAPTURE / "baseline_metrics.json").write_text(
        json.dumps(record, indent=2) + "\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
