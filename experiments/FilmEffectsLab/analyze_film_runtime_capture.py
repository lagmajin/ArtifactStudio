#!/usr/bin/env python3
"""Analyze manually exported Film suite PNGs against prepared ROI fixtures."""
from __future__ import annotations

import hashlib
import json
import platform
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
CAPTURE = ROOT / "experiments/FilmEffectsLab/captures/film-suite-composition-runtime-capture-r1"
FIXTURES = ROOT / "experiments/FilmEffectsLab/fixtures/film-suite-runtime-r1"
EXPECTED_EFFECTS = {"grunge": ("grunge-neutral", "grunge-grain-only"),
                    "damage": ("damage-neutral", "damage-overlay-only")}
BACKGROUNDS = ("black", "gray-0.18", "white")
RESOLUTIONS = {"512": (512, 256), "1024": (1024, 512)}
REGIONS = ("whole", "transparent", "low", "edge", "near_opaque", "opaque", "fine_detail", "flat_opaque")


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_rgba(path: Path) -> np.ndarray:
    return np.asarray(Image.open(path).convert("RGBA"), dtype=np.float32) / np.float32(255.0)


def load_rgb(path: Path) -> np.ndarray:
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.float32) / np.float32(255.0)


def stats(delta: np.ndarray, mask: np.ndarray) -> dict:
    pixels = delta[mask]
    if not pixels.size:
        return {"pixel_count": 0, "mae": None, "rmse": None, "max_abs": None, "changed_fraction": None}
    return {"pixel_count": int(mask.sum()), "mae": float(np.mean(np.abs(pixels))),
            "rmse": float(np.sqrt(np.mean(pixels*pixels))), "max_abs": float(np.max(np.abs(pixels))),
            "changed_fraction": float(np.mean(np.any(np.abs(pixels)>0,axis=-1)))}


def main() -> None:
    manifest_path=CAPTURE/"capture-manifest.json"
    if not manifest_path.exists():
        raise FileNotFoundError(f"Fill capture template and save it as {manifest_path}")
    manifest=json.loads(manifest_path.read_text(encoding="utf-8"))
    required=[]
    for res in RESOLUTIONS:
        for bg in BACKGROUNDS:
            required.append(f"baseline-disabled-{bg}-{res}.png")
            for neutral,candidate in EXPECTED_EFFECTS.values():
                required.extend((f"{neutral}-{bg}-{res}.png",f"{candidate}-{bg}-{res}.png"))
        required.extend((f"grunge-grain-only-transparent-{res}.png",f"damage-overlay-only-transparent-{res}.png"))
    missing=[name for name in required if not (CAPTURE/name).exists()]
    if missing:
        raise FileNotFoundError("Missing runtime exports:\n"+"\n".join(missing))
    if not manifest.get("application_version"):
        raise ValueError("capture-manifest.json must include application_version")
    results={}
    sheets=[]
    for res,(width,height) in RESOLUTIONS.items():
        source_path=FIXTURES/f"film-alpha-edge-{res}.png"
        source=load_rgba(source_path)
        if source.shape[:2]!=(height,width):
            raise ValueError(f"Fixture dimensions mismatch: {source_path}")
        masks={}
        for region in REGIONS:
            mask_path=FIXTURES/f"roi-{region}-{res}.png"
            masks[region]=np.asarray(Image.open(mask_path).convert("L"))>127
            if masks[region].shape!=(height,width):
                raise ValueError(f"ROI dimensions mismatch: {mask_path}")
        baseline={bg:load_rgb(CAPTURE/f"baseline-disabled-{bg}-{res}.png") for bg in BACKGROUNDS}
        for bg,image in baseline.items():
            if image.shape[:2]!=(height,width): raise ValueError(f"Export dimensions mismatch: {bg}/{res}")
        results[res]={"source_fixture_sha256":sha(source_path),"roi_pixel_counts":{k:int(v.sum()) for k,v in masks.items()},
                      "flattened":{},"transparent_exports":{}}
        for effect,(neutral,candidate) in EXPECTED_EFFECTS.items():
            results[res]["flattened"][effect]={}
            for state in (neutral,candidate):
                state_result={}
                for bg in BACKGROUNDS:
                    image=load_rgb(CAPTURE/f"{state}-{bg}-{res}.png")
                    if image.shape[:2]!=(height,width): raise ValueError(f"Export dimensions mismatch: {state}/{bg}/{res}")
                    state_result[bg]={region:stats(image-baseline[bg],mask) for region,mask in masks.items()}
                    if res=="512":
                        sheets.append((effect,state,bg,image))
                results[res]["flattened"][effect][state]=state_result
            results[res]["flattened"][effect]["candidate_minus_neutral"]={}
            for bg in BACKGROUNDS:
                neutral_img=load_rgb(CAPTURE/f"{neutral}-{bg}-{res}.png")
                candidate_img=load_rgb(CAPTURE/f"{candidate}-{bg}-{res}.png")
                results[res]["flattened"][effect]["candidate_minus_neutral"][bg]= {
                    region:stats(candidate_img-neutral_img,mask) for region,mask in masks.items()}
        for state in ("grunge-grain-only","damage-overlay-only"):
            path=CAPTURE/f"{state}-transparent-{res}.png"
            image=load_rgba(path)
            if image.shape[:2]!=(height,width): raise ValueError(f"Transparent export dimensions mismatch: {state}/{res}")
            delta_alpha=image[...,3]-source[...,3]
            results[res]["transparent_exports"][state]={
                "sha256":sha(path),"alpha_delta":stats(delta_alpha[...,None],np.ones((height,width),bool)),
                "alpha_mean":float(image[...,3].mean()),"alpha_min":float(image[...,3].min()),"alpha_max":float(image[...,3].max()),
                "rgb_min":float(image[...,:3].min()),"rgb_max":float(image[...,:3].max()),
                "rgb_mean_abs_in_zero_alpha":float(np.mean(np.abs(image[...,:3][image[...,3]==0]))) if np.any(image[...,3]==0) else None,
            }
    report={"plan_id":"film-suite-composition-runtime-capture-r1","capture_manifest_sha256":sha(manifest_path),
      "analyzer_sha256":sha(Path(__file__)),"application_version":manifest["application_version"],
      "python":platform.python_version(),"numpy":np.__version__,"pillow":Image.__version__,
      "results_by_resolution":results,
      "limits":"Flattened PNG code-value differences and exported alpha only. The PNG exporter may convert alpha association or color space; no raw effect-entry float descriptor is measured. No aesthetic acceptance, GPU parity, temporal result, or controlled performance result.",
      "judge":"Research author; inspect emitted contact sheets at the plan's declared scales."}
    for res in RESOLUTIONS:
        if res!="512": continue
        for effect in EXPECTED_EFFECTS:
            row_h=276
            sheet=Image.new("RGB",(3*512,len(BACKGROUNDS)*row_h),(28,29,32)); draw=ImageDraw.Draw(sheet); font=ImageFont.load_default()
            for row,bg in enumerate(BACKGROUNDS):
                states=("baseline-disabled",*EXPECTED_EFFECTS[effect])
                for col,state in enumerate(states):
                    path=CAPTURE/f"{state}-{bg}-{res}.png"
                    img=Image.open(path).convert("RGB")
                    x,y=col*512,row*row_h
                    draw.text((x+3,y+2),f"{state} / {bg}",fill=(245,245,245),font=font)
                    sheet.paste(img,(x,y+18))
            out=CAPTURE/f"{effect}-runtime-contact-{res}.png"; sheet.save(out)
            report.setdefault("contact_sheets",[]).append(str(out.relative_to(ROOT)))
    out_path=CAPTURE/"runtime-analysis.json"
    out_path.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",encoding="utf-8",newline="\n")
    checksum=hashlib.sha256(out_path.read_bytes()).hexdigest()
    (CAPTURE/"runtime-analysis.sha256").write_text(f"{checksum}  runtime-analysis.json\n",encoding="ascii",newline="\n")
    print(f"Analyzed {len(required)} PNG exports into {out_path}")


if __name__=="__main__": main()
