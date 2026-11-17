from __future__ import annotations
import json
from pathlib import Path
import sys
import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
OUT = ROOT / "captures" / "rd2_white_axis"
RD0 = ROOT / "captures" / "rd0"
sys.path.insert(0, str(ROOT))
from run_rd1_source_mirror import apply_reinhard, apply_mean_std, apply_histogram, sha, CORE_SOURCE, EXPECTED_CORE_SHA256

LEVELS = (0.58, 0.70, 0.82, 0.92)
WIDTH, HEIGHT = 512, 320

def encode_srgb(linear):
    return np.where(linear <= 0.0031308, 12.92 * linear,
                    1.055 * np.maximum(linear, 0) ** (1 / 2.4) - 0.055)

def linear_to_lab(rgb):
    rgb = np.clip(rgb, 0, 1).astype(np.float64)
    lin = np.where(rgb > 0.04045, ((rgb + 0.055) / 1.055) ** 2.4, rgb / 12.92)
    r, g, b = np.moveaxis(lin, -1, 0)
    x = (0.4124564*r + 0.3575761*g + 0.1804375*b) / 0.95047
    y = 0.2126729*r + 0.7151522*g + 0.0721750*b
    z = (0.0193339*r + 0.1191920*g + 0.9503041*b) / 1.08883
    f = lambda t: np.where(t > 0.008856, np.cbrt(t), 7.787*t + 16/116)
    fx, fy, fz = f(x), f(y), f(z)
    return np.stack((116*fy-16, 500*(fx-fy), 200*(fy-fz)), -1)

def measure(out_linear, ref_linear, mask, encoded_output):
    o = out_linear[mask].astype(np.float64)
    r = ref_linear[mask].astype(np.float64)
    dlab = np.linalg.norm(linear_to_lab(o) - linear_to_lab(r), axis=1)
    delta = np.abs(o-r)
    y = o @ np.array([0.2126, 0.7152, 0.0722])
    yr = r @ np.array([0.2126, 0.7152, 0.0722])
    return {
        "pixels": int(mask.sum()),
        "output_linear_rgb_mean": [float(v) for v in o.mean(0)],
        "output_neutral_axis_mean_channel_spread": float(np.mean(np.max(o, axis=1)-np.min(o, axis=1))),
        "target_neutral_axis_mean_channel_spread": float(np.mean(np.max(r, axis=1)-np.min(r, axis=1))),
        "neutral_chroma_deltaE76_mean": float(dlab.mean()),
        "neutral_chroma_deltaE76_p95": float(np.percentile(dlab, 95)),
        "luminance_shift_mean": float(np.mean(y-yr)),
        "luminance_shift_p95_abs": float(np.percentile(np.abs(y-yr), 95)),
        "rgb_mae_to_target": float(delta.mean()),
        "rgb_p95_to_target": float(np.percentile(delta, 95)),
        "encoded_candidate_below_zero_ratio": float(np.mean(encoded_output[mask] < 0)),
        "encoded_candidate_above_one_ratio": float(np.mean(encoded_output[mask] > 1)),
        "non_finite_values": int(o.size - np.count_nonzero(np.isfinite(o))),
    }

def save_preview(path, linear_rgb):
    encoded = encode_srgb(linear_rgb)
    Image.fromarray(np.uint8(np.clip(np.rint(encoded*255), 0, 255)), "RGB").save(path, format="PNG", optimize=False)

def main():
    if sha(CORE_SOURCE) != EXPECTED_CORE_SHA256:
        raise RuntimeError("Core source changed; re-audit equation mirror before refreshing captures")
    OUT.mkdir(parents=True, exist_ok=True)
    source = np.load(RD0/"synthetic_source.npy", allow_pickle=False).astype(np.float64)
    reference = np.load(RD0/"synthetic_reference.npy", allow_pickle=False).astype(np.float64)
    white = np.zeros((HEIGHT, WIDTH), bool)
    full = np.ones_like(white)
    patch_specs = []
    xs = ((24,132),(144,252),(264,372),(384,492))
    for level, (x0,x1) in zip(LEVELS, xs):
        y0,y1 = 218,304
        src_rgb = np.clip(np.array([level*1.055, level*0.995, level*0.87]), 0, 0.985)
        ref_rgb = np.array([level,level,level])
        source[y0:y1,x0:x1] = src_rgb
        reference[y0:y1,x0:x1] = ref_rgb
        white[y0:y1,x0:x1] = True
        patch_specs.append({"name":f"neutral_level_{level:.2f}","xyxy":[x0,y0,x1,y1],
                            "linear_source_rgb":src_rgb.tolist(),"linear_target_rgb":ref_rgb.tolist()})
    nonwhite = ~white
    src_encoded, ref_encoded = encode_srgb(source), encode_srgb(reference)
    roi_path = OUT/"roi_neutral_axis.png"
    Image.fromarray(white.astype(np.uint8)*255,"L").save(roi_path,format="PNG",optimize=False)
    np.save(OUT/"source_linear.npy",source.astype(np.float32),allow_pickle=False)
    np.save(OUT/"reference_linear.npy",reference.astype(np.float32),allow_pickle=False)
    save_preview(OUT/"source.png",source)
    save_preview(OUT/"reference.png",reference)

    results=[]
    methods=(("Reinhard_source_mirror",apply_reinhard),
             ("MeanStddev_source_mirror",apply_mean_std),
             ("Histogram_source_mirror",apply_histogram))
    for method, fn in methods:
        for intensity in (0.25,0.5,1.0):
            encoded_out=fn(src_encoded,ref_encoded,intensity)
            out_linear=np.where(np.clip(encoded_out,0,1)<=0.04045,
                                np.clip(encoded_out,0,1)/12.92,
                                ((np.clip(encoded_out,0,1)+0.055)/1.055)**2.4)
            key=f"{method}_i{intensity:.2f}"
            np.save(OUT/f"{key}.npy",encoded_out.astype(np.float32),allow_pickle=False)
            save_preview(OUT/f"{key}.png",out_linear)
            per_level={}
            for spec in patch_specs:
                x0,y0,x1,y1=spec["xyxy"]
                mask=np.zeros_like(white); mask[y0:y1,x0:x1]=True
                per_level[spec["name"]]=measure(out_linear,reference,mask,encoded_out)
            results.append({"method":method,"intensity":intensity,
                            "neutral_axis":measure(out_linear,reference,white,encoded_out),
                            "per_level":per_level,
                            "whole_frame":measure(out_linear,reference,full,encoded_out),
                            "non_neutral_regions":measure(out_linear,reference,nonwhite,encoded_out)})

    response=[]
    level_names=[s["name"] for s in patch_specs]
    for method,_ in methods:
        runs=sorted((r for r in results if r["method"]==method),key=lambda r:r["intensity"])
        by_level={}
        for level in level_names:
            de=[r["per_level"][level]["neutral_chroma_deltaE76_mean"] for r in runs]
            lum=[abs(r["per_level"][level]["luminance_shift_mean"]) for r in runs]
            by_level[level]={"deltaE76_by_intensity":de,
                             "deltaE_nonincreasing":all(de[i+1]<=de[i]+1e-12 for i in range(2)),
                             "abs_luminance_shift_by_intensity":lum,
                             "abs_luminance_shift_non_decreasing":all(lum[i+1]>=lum[i]-1e-12 for i in range(2))}
        high=next(r for r in runs if r["intensity"]==1.0)
        high_de=[high["per_level"][level]["neutral_chroma_deltaE76_mean"] for level in level_names]
        response.append({"method":method,"per_level_intensity_response":by_level,
                         "intensity_1_deltaE_by_neutral_level":high_de,
                         "deltaE_non_decreasing_with_target_level":all(high_de[i+1]>=high_de[i]-1e-12 for i in range(3)),
                         "whole_frame_vs_neutral_roi_rgb_mae_at_intensity_1":{
                             "whole_frame":high["whole_frame"]["rgb_mae_to_target"],
                             "neutral_roi":high["neutral_axis"]["rgb_mae_to_target"],
                             "non_neutral_regions":high["non_neutral_regions"]["rgb_mae_to_target"]}})

    names=["source.png","reference.png","Reinhard_source_mirror_i1.00.png",
           "MeanStddev_source_mirror_i1.00.png","Histogram_source_mirror_i1.00.png"]
    labels=["Cast source","Neutral target","Reinhard 1.0","MeanStddev 1.0","Histogram 1.0"]
    tw,th,pad,header=384,240,16,30
    sheet=Image.new("RGB",(pad*2+len(names)*tw,pad*2+th+header),(32,32,32))
    draw=ImageDraw.Draw(sheet)
    for i,(name,label) in enumerate(zip(names,labels)):
        im=Image.open(OUT/name).convert("RGB"); im.thumbnail((tw,th))
        x,y=pad+i*tw,pad+header; sheet.paste(im,(x,y)); draw.text((x,y-header+4),label,fill="white")
    sheet.save(OUT/"comparison_sheet.png")
    files={p.name:sha(p) for p in sorted(OUT.iterdir()) if p.is_file() and p.name!="manifest.json"}
    manifest={"rd":"rd2-neutral-axis-source-mirror",
      "status":"synthetic neutral-axis response characterization; no quality gate or winner",
      "source":"experiments/ColorMatchLab/captures/rd0/synthetic_source.npy","source_sha256":sha(RD0/"synthetic_source.npy"),
      "base_reference":"experiments/ColorMatchLab/captures/rd0/synthetic_reference.npy","base_reference_sha256":sha(RD0/"synthetic_reference.npy"),
      "condition":{"dimensions":[WIDTH,HEIGHT],"base_values":"linear RGB float32; methods consume sRGB-encoded values",
        "alpha":"opaque","neutral_target":"same linear RGB level on all channels",
        "source_cast":"R×1.055, G×0.995, B×0.87, clamped to [0,0.985]","neutral_patches":patch_specs},
      "measurement":{"neutral_chroma":"CIE Lab D65/2° equivalent formula, ΔE76 after sRGB decode",
        "luminance":"linear Rec.709 Y","candidate_decode":"output clipped to [0,1] then sRGB decoded for display-domain metrics; out-of-range ratios use encoded method output before clipping",
        "reference_distance":"linear RGB MAE/p95","regions":"whole frame, neutral-axis ROI, non-neutral complement, four level ROIs"},
      "roi":{"path":roi_path.name,"sha256":sha(roi_path),"pixels":int(white.sum())},
      "method_source":{"path":"ArtifactCore/src/Color/AutoColorMatch.cpp","sha256":sha(CORE_SOURCE),
        "mirror_script":"run_rd1_source_mirror.py","mirror_script_sha256":sha(ROOT/"run_rd1_source_mirror.py"),
        "study_script":"white_axis_study.py","study_script_sha256":sha(Path(__file__).resolve()),
        "evidence":"independent Python equation transcription only; no C++ execution/differential"},
      "files":files,"results":results,"response_shape_analysis":response,
      "unverified":["natural white objects/photos","reviewed skin ROI","human visual review","gates","C++ parity","alpha/HDR/ICC","runtime/performance"]}
    (OUT/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8",newline="\n")
    print(json.dumps({"capture":str(OUT),"summary":response},indent=2))

if __name__=="__main__":main()
