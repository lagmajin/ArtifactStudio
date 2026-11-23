from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent
CAPTURE = ROOT / "captures" / "rd17_white_object_roi"
RD7 = ROOT / "captures" / "rd7_natural_scene_pair"
RECT = (384, 228, 496, 435)
THRESHOLDS = (0.36, 0.33, 0.30)
MAX_SPREAD = 0.16


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    source_path = RD7 / "source_srgb_from_linear_raw.npy"
    source = np.load(source_path).astype(np.float64)
    x0, y0, x1, y1 = RECT
    bounded = np.zeros(source.shape[:2], dtype=bool)
    bounded[y0:y1, x0:x1] = True
    mask = (bounded & np.all(source > np.asarray(THRESHOLDS), axis=-1)
            & ((source.max(axis=-1)-source.min(axis=-1)) < MAX_SPREAD))
    CAPTURE.mkdir(parents=True, exist_ok=True)
    mask_path = CAPTURE / "white_fridge_candidate_mask.png"
    Image.fromarray(mask.astype(np.uint8)*255, "L").save(mask_path, format="PNG", optimize=False)
    overlay = source.copy()
    overlay[mask] = overlay[mask]*0.45 + np.array([0.05,0.75,0.18])*0.55
    Image.fromarray(np.uint8(np.clip(np.rint(overlay*255),0,255)),"RGB").save(
        CAPTURE/"source_mask_review.png", format="PNG", optimize=False)
    review = {
        "rd": "rd17-fridge-white-candidate-mask-review",
        "status": "rejected before ROI-specific candidate scoring",
        "source": str(source_path.relative_to(ROOT)),
        "source_sha256": sha(source_path),
        "roi_rect_xyxy": list(RECT),
        "source_channel_min_exclusive": list(THRESHOLDS),
        "source_max_channel_spread_exclusive": MAX_SPREAD,
        "mask_pixels": int(mask.sum()),
        "frame_fraction": float(mask.mean()),
        "mask_sha256": sha(mask_path),
        "overlay_sha256": sha(CAPTURE/"source_mask_review.png"),
        "review": "The fixed source-only threshold includes visible magnets, printed photos/stickers, flowers/stems, and panel edges. It does not isolate the refrigerator's white surface, so the predeclared rejection rule applies. No candidate output was viewed or scored for this ROI.",
        "next_action": "Do not tune this ROI after candidate review. A new independently predeclared ROI or another rights-cleared source is required for a new white-object measurement.",
    }
    (CAPTURE/"roi_review.json").write_text(json.dumps(review,indent=2)+"\n",encoding="utf-8",newline="\n")
    print(json.dumps({"mask_pixels":int(mask.sum()),"frame_fraction":float(mask.mean()),
                      "status":review["status"],"review":review["review"]},indent=2))


if __name__=="__main__":
    main()
