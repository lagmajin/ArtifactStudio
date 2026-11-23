from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT=Path(__file__).resolve().parent
CAPTURE=ROOT/"captures"/"rd18_lamp_white_roi"
RD7=ROOT/"captures"/"rd7_natural_scene_pair"
CORE=ROOT.parent.parent/"ArtifactCore"/"src"/"Color"/"AutoColorMatch.cpp"
EXPECTED_CORE="62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab"
REGIONS=("left_lampshade","right_lampshade")

def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def rgb_to_lab(rgb:np.ndarray)->np.ndarray:
    rgb=np.clip(rgb.astype(np.float64),0,1)
    linear=np.where(rgb<=0.04045,rgb/12.92,((rgb+0.055)/1.055)**2.4)
    r,g,b=np.moveaxis(linear,-1,0)
    x=(0.4124564*r+0.3575761*g+0.1804375*b)/0.95047
    y=0.2126729*r+0.7151522*g+0.0721750*b
    z=(0.0193339*r+0.1191920*g+0.9503041*b)/1.08883
    def f(t:np.ndarray)->np.ndarray:
        return np.where(t>0.008856,np.cbrt(t),7.787*t+16/116)
    fx,fy,fz=f(x),f(y),f(z)
    return np.stack((116*fy-16,500*(fx-fy),200*(fy-fz)),axis=-1)

def lab_summary(values:np.ndarray)->dict:
    lab=rgb_to_lab(values)
    mean=lab.mean(axis=0)
    cps=np.hypot(lab[:,1],lab[:,2])
    hues=np.arctan2(lab[:,2],lab[:,1])
    hue=float(np.degrees(np.arctan2(np.sin(hues).mean(),np.cos(hues).mean()))%360)
    return {"L_mean":float(mean[0]),"a_mean":float(mean[1]),"b_mean":float(mean[2]),
            "mean_vector_Cab":float(np.hypot(mean[1],mean[2])),"circular_mean_hue_degrees":hue,
            "pixel_Cab_mean":float(cps.mean()),"pixel_Cab_p95":float(np.percentile(cps,95))}

def region_metrics(output:np.ndarray,source:np.ndarray,reference:np.ndarray,mask:np.ndarray)->dict:
    o,s,r=output[mask],source[mask],reference[mask]
    dref=np.abs(o-r); dsrc=np.abs(o-s)
    de_ref=np.linalg.norm(rgb_to_lab(o)-rgb_to_lab(r),axis=-1)
    de_src=np.linalg.norm(rgb_to_lab(o)-rgb_to_lab(s),axis=-1)
    return {"pixels":int(mask.sum()),"reference":{"encoded_srgb_mae":float(dref.mean()),"encoded_srgb_p95":float(np.percentile(dref,95)),
                "deltaE76_mean":float(de_ref.mean()),"deltaE76_p95":float(np.percentile(de_ref,95))},
            "source_relative":{"encoded_srgb_mae":float(dsrc.mean()),"deltaE76_mean":float(de_src.mean()),"deltaE76_p95":float(np.percentile(de_src,95))},
            "source_lab":lab_summary(s),"reference_lab":lab_summary(r),"output_lab":lab_summary(o),
            "output_channel_min":float(o.min()),"output_channel_max":float(o.max()),
            "output_clip_low_ratio":float(np.mean(o<0)),"output_clip_high_ratio":float(np.mean(o>1)),
            "non_finite_values":int(o.size-np.count_nonzero(np.isfinite(o)))}

def save_preview(path:Path,image:np.ndarray)->None:
    u8=np.uint8(np.clip(np.rint(np.clip(image,0,1)*255),0,255))
    Image.fromarray(u8,"RGB").save(path,format="PNG",optimize=False)

def main()->None:
    review_path=CAPTURE/"roi_review.json"
    review=json.loads(review_path.read_text(encoding="utf-8"))
    if review.get("status")!="accepted_for_candidate_scoring":
        raise RuntimeError("ROI must pass source-only review before candidate scoring")
    source_path=RD7/"source_srgb_from_linear_raw.npy"; ref_path=RD7/"reference_camera_jpeg_srgb.npy"
    source=np.load(source_path).astype(np.float64); reference=np.load(ref_path).astype(np.float64)
    if sha(source_path)!=review["source_sha256"]:
        raise RuntimeError("Source changed since ROI review")
    core_hash=sha(CORE)
    if core_hash!=EXPECTED_CORE:
        raise RuntimeError(f"Core source changed; re-audit before updating pin: {core_hash}")
    masks={"whole_frame":np.ones(source.shape[:2],dtype=bool)}
    masks.update({name:np.asarray(Image.open(CAPTURE/f"{name}_mask.png").convert("L"))>0 for name in REGIONS})
    baseline={roi:region_metrics(source,source,reference,mask) for roi,mask in masks.items()}
    candidates=[]; previews={"source":source,"camera_jpeg":reference}
    for method in ("Reinhard","MeanStddev","Histogram"):
        for intensity in (0.25,0.5,1.0):
            name=f"{method}_i{intensity:.2f}"; path=RD7/f"{name}.npy"
            out=np.load(path).astype(np.float64); previews[name]=out
            candidates.append({"name":name,"method":method,"intensity":intensity,"source_output_sha256":sha(path),
                "regions":{roi:region_metrics(out,source,reference,mask) for roi,mask in masks.items()}})
    sheet_records={}
    for roi in REGIONS:
        mask=masks[roi]
        ys,xs=np.where(mask); x0=max(0,int(xs.min())-16); x1=min(mask.shape[1],int(xs.max())+17)
        y0=max(0,int(ys.min())-16); y1=min(mask.shape[0],int(ys.max())+17)
        names=["source","camera_jpeg"]+[f"{m}_i{i:.2f}" for m in ("Reinhard","MeanStddev","Histogram") for i in (0.25,0.5,1.0)]
        tw,th,hh=220,150,28
        sheet=Image.new("RGB",(tw*len(names),th+hh),(30,30,30)); draw=ImageDraw.Draw(sheet)
        for idx,name in enumerate(names):
            draw.text((idx*tw+5,7),name,fill=(245,245,245))
            crop=np.uint8(np.clip(np.rint(np.clip(previews[name][y0:y1,x0:x1],0,1)*255),0,255))
            im=Image.fromarray(crop,"RGB"); im.thumbnail((tw,th))
            px=idx*tw+(tw-im.width)//2; py=hh+(th-im.height)//2
            sheet.paste(im,(px,py))
        sheet_path=CAPTURE/f"{roi}_comparison.png"; sheet.save(sheet_path,format="PNG",optimize=False)
        sheet_records[roi]={"crop_xyxy":[x0,y0,x1,y1],"sheet":sheet_path.name,"sheet_sha256":sha(sheet_path)}
    review["candidate_outputs_opened"]=True
    review["candidate_scoring_completed"]=True
    review_path.write_text(json.dumps(review,indent=2)+"\n",encoding="utf-8",newline="\n")
    files={p.name:sha(p) for p in sorted(CAPTURE.iterdir()) if p.is_file() and p.name not in ("manifest.json",)}
    manifest={"rd":"rd18-natural-lampshade-roi","status":"single natural scene ROI comparison using equation mirrors; no quality gate",
        "pair":{"source_dataset":"Middlebury registered EOS 5D natural-scene RAW/JPEG pair","source_sha256":sha(source_path),
               "reference_sha256":sha(ref_path),"reference_role":"camera rendition, not calibrated color truth","dimensions":[1000,667],"transfer":"both measured as encoded sRGB; source converted from dataset linear RAW with sRGB OETF"},
        "roi_review":review,"roi_masks":{n:{"pixels":int(m.sum()),"sha256":sha(CAPTURE/f"{n}_mask.png") if n in REGIONS else None,
                                              "definition":"implicit full-frame mask" if n=="whole_frame" else "saved polygon mask"} for n,m in masks.items()},
        "conditions":{"methods":["Reinhard","MeanStddev","Histogram"],"intensities":[0.25,0.5,1.0],
                      "metrics":"encoded-sRGB error; Lab D65/2-degree DeltaE76 on [0,1]-clipped values; mean-vector chroma/hue and pixel chroma distribution",
                      "interpretation":"camera-JPEG closeness and source-relative change are separate; no thresholds or winner"},
        "implementation":{"core_source_sha256":core_hash,"equation_runner":"run_rd1_source_mirror.py","equation_runner_sha256":sha(ROOT/"run_rd1_source_mirror.py"),
                          "candidate_origin":"pre-existing RD7 Python equation-mirror arrays; no C++ differential"},
        "visual_review":{"sheets":sheet_records,"status":"generated after source-only ROI acceptance; exploratory and unblinded"},
        "limitations":["one kitchen scene/camera","lamp shades not colorimeter-confirmed neutral","camera JPEG is processed reference, not ground truth",
                       "ROI manually drawn by one analyst","no Core execution, ICC/HDR, alpha, product path, GPU parity, independent review, or performance"],
        "baseline_unchanged_source":baseline,"candidates":candidates,"files":files}
    (CAPTURE/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8",newline="\n")
    print(json.dumps({"capture":str(CAPTURE),"roi_counts":{n:int(m.sum()) for n,m in masks.items()},
        "baseline":{n:baseline[n]["reference"] for n in masks},
        "i1_metrics":[{"method":c["method"],"whole_frame":c["regions"]["whole_frame"]["reference"],
                        "left":c["regions"]["left_lampshade"]["reference"],"right":c["regions"]["right_lampshade"]["reference"]}
                       for c in candidates if c["intensity"]==1.0]},indent=2))

if __name__=="__main__":main()
