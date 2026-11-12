#!/usr/bin/env python3
"""Prepare deterministic synthetic inputs/ROIs for a later Film suite app capture."""
from __future__ import annotations

import hashlib
import json
import platform
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
PLAN = ROOT / "experiments/FilmEffectsLab/plans/film-suite-composition-runtime-capture-r1.json"
OUT = ROOT / "experiments/FilmEffectsLab/fixtures/film-suite-runtime-r1"
CAPTURE = ROOT / "experiments/FilmEffectsLab/captures/film-suite-composition-runtime-capture-r1"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def alpha_edge(size: tuple[int, int]) -> tuple[Image.Image, np.ndarray]:
    w, h = size
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    cx, cy, rx, ry = w*.5, h*.5, w*.305, h*.36
    normalized = np.sqrt(((xx-cx)/rx)**2 + ((yy-cy)/ry)**2)
    signed_pixels = (normalized-1)*min(rx,ry)
    feather = max(1.0, w/64.0)
    alpha = np.clip(.5-signed_pixels/feather, 0, 1).astype(np.float32)
    # Straight RGBA PNG: preserve the same saturated hidden RGB outside the shape.
    rgba = np.zeros((h,w,4),np.uint8)
    rgba[...,0],rgba[...,1],rgba[...,2] = 220,82,28
    alpha8 = np.uint8(alpha*255+.5)
    rgba[...,3] = alpha8
    image=Image.fromarray(rgba,"RGBA")
    draw=ImageDraw.Draw(image)
    # High-contrast and fine detail inside the opaque center region.
    draw.line((cx-w*.12,cy,cx+w*.12,cy),fill=(16,16,16,255),width=max(1,round(w/256)))
    draw.line((cx,cy-h*.12,cx,cy+h*.12),fill=(248,248,248,255),width=max(1,round(w/256)))
    for n in range(1,9):
        x=round(cx-w*.08+n*w*.02)
        draw.line((x,cy-h*.08,x,cy+h*.08),fill=(32+n*20,220-n*12,40+n*10,255),width=1)
    # ROI masks reflect the actual quantized PNG alpha samples, not the
    # pre-quantization analytic values.
    return image,alpha8.astype(np.float32)/np.float32(255.0)


def detail_plate(size: tuple[int,int]) -> Image.Image:
    w,h=size
    yy,xx=np.mgrid[0:h,0:w].astype(np.float32)
    bands=np.select([xx<w*.25,xx<w*.5,xx<w*.75],[.08,.32,.62],default=.9).astype(np.float32)
    grad=.18+.62*xx/max(1,w-1)
    gray=np.where(yy>h*.5,grad,bands)
    checker=((xx.astype(np.int32)//max(1,w//128)+yy.astype(np.int32)//max(1,h//64))%2)==0
    gray=np.where((yy>h*.65)&(yy<h*.9)&(xx>w*.55),np.where(checker,.82,.18),gray)
    rgb=np.stack((gray,np.clip(gray*.78+.1,0,1),np.clip(gray*.62+.18,0,1)),axis=-1)
    arr=np.uint8(np.clip(rgb,0,1)*255+.5)
    image=Image.fromarray(arr,"RGB").convert("RGBA")
    draw=ImageDraw.Draw(image)
    for i in range(1,17):
        x=round(i*w/18)
        color=(250,250,250,255) if i%2 else (8,8,8,255)
        draw.line((x,h*.12,x,h*.44),fill=color,width=1)
    return image


def mask_png(mask:np.ndarray,path:Path)->None:
    Image.fromarray(np.uint8(mask)*255,"L").save(path)


def main()->None:
    plan=json.loads(PLAN.read_text(encoding="utf-8"))
    OUT.mkdir(parents=True,exist_ok=True)
    CAPTURE.mkdir(parents=True,exist_ok=True)
    assets=[]
    previews=[]
    roi_summary={}
    for w,h,suffix in ((512,256,"512"),(1024,512,"1024")):
        image,alpha=alpha_edge((w,h))
        image_path=OUT/f"film-alpha-edge-{suffix}.png"
        image.save(image_path,optimize=False)
        assets.append({"id":f"alpha-edge-{suffix}","path":str(image_path.relative_to(ROOT)),"sha256":sha(image_path),"size":[w,h],"mode":"RGBA8 straight-alpha PNG","alpha_min":float(alpha.min()),"alpha_max":float(alpha.max())})
        if suffix=="512":
            rgba=np.asarray(image,dtype=np.float32)/np.float32(255.0)
            gray=np.full((h,w,3),.18,np.float32)
            preview=rgba[...,:3]*alpha[...,None]+gray*(1-alpha[...,None])
            preview_path=OUT/"preview-alpha-edge-512-on-gray.png"
            Image.fromarray(np.uint8(np.clip(preview,0,1)*255+.5),"RGB").save(preview_path)
            previews.append({"path":str(preview_path.relative_to(ROOT)),"sha256":sha(preview_path),"display_only":True})
        yy,xx=np.mgrid[0:h,0:w]
        fine=(alpha>=.99)&(np.abs(xx-w*.5)<=w*.14)&(np.abs(yy-h*.5)<=h*.14)
        conditions={"whole":np.ones((h,w),bool),"transparent":alpha==0,"low":(alpha>0)&(alpha<=.1),"edge":(alpha>.1)&(alpha<.9),"near_opaque":(alpha>=.9)&(alpha<1),"opaque":alpha==1,"fine_detail":fine,"flat_opaque":(alpha==1)&(~fine)}
        for name,mask in conditions.items():
            mask_path=OUT/f"roi-{name}-{suffix}.png"
            mask_png(mask,mask_path)
            roi_summary[f"{name}-{suffix}"]={"path":str(mask_path.relative_to(ROOT)),"sha256":sha(mask_path),"pixel_count":int(mask.sum())}
    detail=detail_plate((512,256))
    detail_path=OUT/"film-detail-512.png"
    detail.save(detail_path,optimize=False)
    assets.append({"id":"detail-512","path":str(detail_path.relative_to(ROOT)),"sha256":sha(detail_path),"size":[512,256],"mode":"RGBA8 opaque diagnostic PNG"})
    manifest={"plan_id":plan["plan_id"],"plan_sha256":sha(PLAN),"generator_sha256":sha(Path(__file__)),
              "python":platform.python_version(),"numpy":np.__version__,"pillow":Image.__version__,
              "assets":assets,"display_previews":previews,"roi_masks":roi_summary,
              "alpha_edge_definition":"Normalized ellipse with analytic smooth alpha transition; opaque center line/grid details; transparent exterior retains saturated hidden RGB in straight PNG data.",
              "capture_status":"inputs_prepared_application_capture_not_started",
              "capture_directory":str(CAPTURE.relative_to(ROOT))}
    (OUT/"input-manifest.json").write_text(json.dumps(manifest,indent=2,ensure_ascii=False)+"\n",encoding="utf-8",newline="\n")
    capture_template={"plan_id":plan["plan_id"],"capture_status":"pending_user_authorization",
      "application_version":None,"project_frame_rate":None,"project_saved":False,
      "input_manifest":"experiments/FilmEffectsLab/fixtures/film-suite-runtime-r1/input-manifest.json",
      "parameter_records":[],"exports":[],"viewer_screenshots":[],
      "export_filename_pattern":"{effect}-{state}-{background}-{resolution}.png",
      "required_states":["baseline-disabled","grunge-neutral","grunge-grain-only","damage-neutral","damage-overlay-only"],
      "transparent_exports":["grunge-grain-only","damage-overlay-only"],
      "backgrounds":["black","gray-0.18","white"],"notes":[]}
    (CAPTURE/"capture-manifest.template.json").write_text(json.dumps(capture_template,indent=2,ensure_ascii=False)+"\n",encoding="utf-8",newline="\n")
    print(f"Prepared {len(assets)} image assets and {len(roi_summary)} alpha ROIs at {OUT}")


if __name__=="__main__":
    main()
