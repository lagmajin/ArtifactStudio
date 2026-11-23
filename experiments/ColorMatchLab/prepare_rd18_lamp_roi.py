from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT=Path(__file__).resolve().parent
CAPTURE=ROOT/"captures"/"rd18_lamp_white_roi"
SOURCE=ROOT/"captures"/"rd7_natural_scene_pair"/"source_srgb_from_linear_raw.npy"
POLYGONS={
    "left_lampshade":[(384,124),(426,124),(437,130),(437,140),(427,146),(388,146),(379,138),(379,131)],
    "right_lampshade":[(700,125),(741,125),(755,131),(757,139),(749,146),(708,146),(700,139)],
}

def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main()->None:
    CAPTURE.mkdir(parents=True,exist_ok=True)
    source=np.load(SOURCE).astype(np.float64)
    masks={}
    for name,polygon in POLYGONS.items():
        layer=Image.new("L",(source.shape[1],source.shape[0]),0)
        ImageDraw.Draw(layer).polygon(polygon,fill=255)
        mask=np.asarray(layer)>0
        masks[name]=mask
        layer.save(CAPTURE/f"{name}_mask.png",format="PNG",optimize=False)
    overlay=source.copy()
    colors={"left_lampshade":np.array([0.05,0.82,0.20]),"right_lampshade":np.array([0.10,0.65,0.95])}
    for name,mask in masks.items():
        overlay[mask]=overlay[mask]*0.42+colors[name]*0.58
    overlay_path=CAPTURE/"source_mask_review.png"
    Image.fromarray(np.uint8(np.clip(np.rint(overlay*255),0,255)),"RGB").save(overlay_path,format="PNG",optimize=False)
    review={"rd":"rd18-lamp-white-roi-source-review","status":"pending_source_overlay_review",
            "source_path":str(SOURCE.relative_to(ROOT)),"source_sha256":sha(SOURCE),
            "polygons_xy":[{"name":name,"vertices":poly,"pixels":int(masks[name].sum()),
                            "mask_sha256":sha(CAPTURE/f"{name}_mask.png")} for name,poly in POLYGONS.items()],
            "overlay_sha256":sha(overlay_path),
            "candidate_outputs_opened":False,
            "review_instruction":"Inspect source_mask_review.png only. Set status accepted_for_candidate_scoring or rejected_before_candidate_scoring, and record visual rationale before opening any candidate output."}
    (CAPTURE/"roi_review.json").write_text(json.dumps(review,indent=2)+"\n",encoding="utf-8",newline="\n")
    print(json.dumps({"capture":str(CAPTURE),"review":review},indent=2))

if __name__=="__main__":main()
