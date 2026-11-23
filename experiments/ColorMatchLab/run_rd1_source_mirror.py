from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
CAPTURE = ROOT / "captures" / "rd1_source_mirror"
SOURCE = ROOT.parent / "CinematicLooksLab" / "assets" / "portrait-2024-1536.jpg"
MASK = ROOT / "captures" / "rd0" / "portrait_skin_roi_candidate.png"
CORE_SOURCE = ROOT.parent.parent / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
EXPECTED_CORE_SHA256 = "62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def gamma_to_linear(c: np.ndarray) -> np.ndarray:
    c = np.clip(c, 0.0, 1.0)
    return np.where(c > 0.04045, ((c + 0.055) / 1.055) ** 2.4, c / 12.92)


def linear_to_gamma(c: np.ndarray) -> np.ndarray:
    c = np.clip(c, 0.0, 1.0)
    return np.where(c > 0.0031308, 1.055 * c ** (1.0 / 2.4) - 0.055, c * 12.92)


def linear_to_gamma_core(c: np.ndarray) -> np.ndarray:
    # AutoColorMatch.cpp clamps linear RGB before applying the transfer curve.
    clipped = np.clip(c, 0.0, 1.0)
    return np.where(c > 0.0031308,
                    1.055 * clipped ** (1.0 / 2.4) - 0.055,
                    clipped * 12.92)


def labf(t: np.ndarray) -> np.ndarray:
    return np.where(t > 0.008856, np.cbrt(t), 7.787 * t + 16.0 / 116.0)


def labf_inv(t: np.ndarray) -> np.ndarray:
    return np.where(t > 0.206893, t ** 3, (t - 16.0 / 116.0) / 7.787)


def rgb_to_lab(rgb: np.ndarray) -> np.ndarray:
    linear = gamma_to_linear(rgb.astype(np.float64))
    r, g, b = np.moveaxis(linear, -1, 0)
    x = (0.4124564*r + 0.3575761*g + 0.1804375*b) / 0.95047
    y = 0.2126729*r + 0.7151522*g + 0.0721750*b
    z = (0.0193339*r + 0.1191920*g + 0.9503041*b) / 1.08883
    fx, fy, fz = labf(x), labf(y), labf(z)
    return np.stack((116*fy-16, 500*(fx-fy), 200*(fy-fz)), axis=-1)


def lab_to_rgb(lab: np.ndarray) -> np.ndarray:
    L, a, bb = np.moveaxis(lab.astype(np.float64), -1, 0)
    fy = (L + 16) / 116
    fx, fz = a / 500 + fy, fy - bb / 200
    x, y, z = 0.95047*labf_inv(fx), labf_inv(fy), 1.08883*labf_inv(fz)
    r = 3.2404542*x - 1.5371385*y - 0.4985314*z
    g = -0.9692660*x + 1.8760108*y + 0.0415560*z
    b = 0.0556434*x - 0.2040259*y + 1.0572252*z
    return linear_to_gamma_core(np.stack((r, g, b), axis=-1))


def apply_reinhard(src: np.ndarray, ref: np.ndarray, intensity: float) -> np.ndarray:
    a, b = rgb_to_lab(src), rgb_to_lab(ref)
    out = a.copy()
    for ch in range(3):
        sm, ss = float(a[..., ch].mean()), max(float(a[..., ch].std()), 0.0001)
        rm, rs = float(b[..., ch].mean()), max(float(b[..., ch].std()), 0.0001)
        mapped = (a[..., ch] - sm) * (rs / ss) + rm
        out[..., ch] = a[..., ch] + (mapped - a[..., ch]) * intensity
    return lab_to_rgb(out)


def apply_mean_std(src: np.ndarray, ref: np.ndarray, intensity: float) -> np.ndarray:
    out = src.astype(np.float64).copy()
    for ch in range(3):
        sm, ss = float(src[..., ch].mean()), max(float(src[..., ch].std()), 0.0001)
        rm, rs = float(ref[..., ch].mean()), max(float(ref[..., ch].std()), 0.0001)
        mapped = (src[..., ch] - sm) * (rs / ss) + rm
        out[..., ch] = src[..., ch] + (mapped - src[..., ch]) * intensity
    return out


def histogram_map(src_ch: np.ndarray, ref_ch: np.ndarray) -> np.ndarray:
    src_values = np.asarray(src_ch, dtype=np.float32)
    ref_values = np.asarray(ref_ch, dtype=np.float32)

    # AutoColorMatch.cpp multiplies float channels by 255.0f, truncates to an
    # integer bin, stores each CDF entry as float, and accumulates that CDF in
    # float. A float64 cumsum is a smoother mathematical reference but can pick
    # different quantiles on large images; preserve the source precision here.
    src_bins = np.clip(
        (src_values * np.float32(255.0)).astype(np.int32), 0, 255)
    ref_bins = np.clip(
        (ref_values * np.float32(255.0)).astype(np.int32), 0, 255)
    src_hist = np.bincount(src_bins.ravel(), minlength=256).astype(np.uint64)
    ref_hist = np.bincount(ref_bins.ravel(), minlength=256).astype(np.uint64)

    src_count = max(1, src_bins.size)
    ref_count = max(1, ref_bins.size)
    src_inv_count = 1.0 / float(src_count)
    ref_inv_count = 1.0 / float(ref_count)
    src_cdf = np.empty(256, dtype=np.float32)
    ref_cdf = np.empty(256, dtype=np.float32)
    src_cdf[0] = np.float32(float(src_hist[0]) * src_inv_count)
    ref_cdf[0] = np.float32(float(ref_hist[0]) * ref_inv_count)
    for i in range(1, 256):
        src_increment = np.float32(float(src_hist[i]) * src_inv_count)
        ref_increment = np.float32(float(ref_hist[i]) * ref_inv_count)
        src_cdf[i] = np.float32(src_cdf[i - 1] + src_increment)
        ref_cdf[i] = np.float32(ref_cdf[i - 1] + ref_increment)

    lut = np.empty(256, dtype=np.float32)
    for i in range(256):
        src_cdf_value = src_cdf[i]
        best_bin = 0
        best_distance = np.float32(2.0)
        for ref_bin in range(256):
            distance = np.abs(np.float32(ref_cdf[ref_bin] - src_cdf_value))
            if distance < best_distance:
                best_distance = distance
                best_bin = ref_bin
        lut[i] = np.float32(best_bin) / np.float32(255.0)
    return lut[src_bins]


def apply_histogram(src: np.ndarray, ref: np.ndarray, intensity: float) -> np.ndarray:
    src_values = np.asarray(src, dtype=np.float32)
    ref_values = np.asarray(ref, dtype=np.float32)
    strength = np.float32(intensity)
    out = src_values.copy()
    for ch in range(3):
        mapped = histogram_map(src_values[..., ch], ref_values[..., ch])
        delta = np.float32(mapped - src_values[..., ch])
        weighted_delta = np.float32(delta * strength)
        out[..., ch] = np.float32(src_values[..., ch] + weighted_delta)
    return out


def metric(src: np.ndarray, out: np.ndarray, ref: np.ndarray, mask: np.ndarray) -> dict[str, float | int]:
    a, b, r = src[mask].astype(np.float64), out[mask], ref[mask].astype(np.float64)
    dl = np.abs(b-r)
    return {
        "pixels": int(mask.sum()),
        "source_to_reference_rgb_mae": float(np.mean(np.abs(a-r))),
        "output_to_reference_rgb_mae": float(np.mean(dl)),
        "output_to_reference_rgb_p95": float(np.percentile(dl,95)),
        "output_to_reference_rgb_max": float(np.max(dl)),
        "output_minus_source_rgb_mean": [float(v) for v in np.mean(b-a,axis=0)],
        "output_clipped_low_ratio": float(np.mean(b < 0)),
        "output_clipped_high_ratio": float(np.mean(b > 1)),
        "output_non_finite_values": int(b.size-np.count_nonzero(np.isfinite(b))),
    }


def save_preview(path: Path, rgb: np.ndarray) -> None:
    encoded = np.where(rgb <= .0031308, 12.92*rgb,
                       1.055*np.maximum(rgb,0)**(1/2.4)-.055)
    Image.fromarray(np.uint8(np.clip(np.rint(encoded*255),0,255)),"RGB").save(path,format="PNG",optimize=False)


def main() -> None:
    actual_core_hash = sha(CORE_SOURCE)
    if actual_core_hash != EXPECTED_CORE_SHA256:
        raise RuntimeError(f"Core source changed; re-audit the mirror before updating its pin: {actual_core_hash}")
    CAPTURE.mkdir(parents=True,exist_ok=True)
    source_srgb = np.asarray(Image.open(SOURCE).convert("RGB"),dtype=np.float64)/255.0
    # Known synthetic color cast in encoded RGB. The source itself remains the
    # pixel-aligned target; inverse recovery is an algorithmic diagnostic.
    # Keep the transform invertible before clipping so the target remains a
    # legitimate exact recovery reference for this constructed experiment.
    cast_scale=np.array([.94,1.015,1.075])
    cast_offset=np.array([.012,.006,-.008])
    cast_unclipped=source_srgb*cast_scale+cast_offset
    # Keep the SDR source in the [0,1] range consumed by the Lab path.
    source_srgb=np.clip(source_srgb,0.0,1.0)
    ref_srgb=np.clip(cast_unclipped,0,1)
    saturation_mask=np.any((cast_unclipped<0)|(cast_unclipped>1),axis=-1)
    # Insert a known neutral card patch into a background region. It is a
    # controlled color reference, not a natural white object or photo truth.
    neutral_source=np.full((96,128,3),0.82,dtype=np.float64)
    neutral_cast=neutral_source*cast_scale+cast_offset
    source_srgb[640:736,64:192]=neutral_source
    ref_srgb[640:736,64:192]=np.clip(neutral_cast,0,1)
    cast_unclipped[640:736,64:192]=neutral_cast
    saturation_mask[640:736,64:192]=np.any((neutral_cast<0)|(neutral_cast>1),axis=-1)
    source_linear = gamma_to_linear(source_srgb).astype(np.float32)
    ref_linear = gamma_to_linear(ref_srgb).astype(np.float32)
    skin_roi = np.asarray(Image.open(MASK).convert("L")) > 0
    h,w=skin_roi.shape
    # Candidate neutral region: a bright window pane is selected spatially, but
    # its actual neutral chromaticity is not established; retain as candidate only.
    white_roi=np.zeros((h,w),bool); white_roi[215:420,80:300]=True
    neutral_roi=np.zeros((h,w),bool); neutral_roi[640:736,64:192]=True
    regions={"whole_frame":np.ones((h,w),bool),"skin_roi_candidate":skin_roi,
             "bright_window_roi_candidate":white_roi,"inserted_neutral_patch":neutral_roi}
    regions["unclipped_invertible_pixels"]=~saturation_mask
    regions["skin_roi_candidate_unclipped"]=skin_roi&~saturation_mask
    regions["bright_window_roi_candidate_unclipped"]=white_roi&~saturation_mask
    regions["saturated_by_cast"]=saturation_mask
    np.save(CAPTURE/"portrait_source_linear.npy",source_linear,allow_pickle=False)
    np.save(CAPTURE/"portrait_reference_linear.npy",ref_linear,allow_pickle=False)
    save_preview(CAPTURE/"portrait_source.png",source_srgb)
    save_preview(CAPTURE/"portrait_synthetic_reference.png",ref_srgb)
    results=[]
    methods={"Reinhard_source_mirror":apply_reinhard,"MeanStddev_source_mirror":apply_mean_std,"Histogram_source_mirror":apply_histogram}
    for method,fn in methods.items():
        for intensity in (.25,.5,1.0):
            out=fn(source_srgb,ref_srgb,intensity)
            key=f"{method}_i{intensity:.2f}"
            save_preview(CAPTURE/f"{key}.png",out)
            np.save(CAPTURE/f"{key}.npy",out.astype(np.float32),allow_pickle=False)
            results.append({"method":method,"intensity":intensity,"regions":{name:metric(source_srgb,out,ref_srgb,mask) for name,mask in regions.items()}})
    save_preview(CAPTURE/"face_roi_overlay.png",np.where(skin_roi[...,None],source_srgb*.65+np.array([0,.35,0]),source_srgb))
    sheet_names=["portrait_source.png","portrait_synthetic_reference.png",
                 "Reinhard_source_mirror_i0.50.png","MeanStddev_source_mirror_i0.50.png",
                 "Histogram_source_mirror_i0.50.png"]
    labels=["Source","Synthetic reference","Reinhard mirror 0.5",
            "MeanStddev mirror 0.5","Histogram mirror 0.5"]
    thumb_w,thumb_h,pad,header=384,256,18,32
    sheet=Image.new("RGB",(pad*2+len(sheet_names)*thumb_w,pad*2+thumb_h+header),(32,32,32))
    draw=ImageDraw.Draw(sheet)
    for i,(name,label) in enumerate(zip(sheet_names,labels)):
        im=Image.open(CAPTURE/name).convert("RGB"); im.thumbnail((thumb_w,thumb_h))
        x,y=pad+i*thumb_w,pad+header
        sheet.paste(im,(x,y)); draw.text((x,y-header+4),label,fill=(240,240,240))
    sheet.save(CAPTURE/"comparison_sheet.png")
    files={p.name:sha(p) for p in sorted(CAPTURE.iterdir()) if p.is_file() and p.name!="manifest.json"}
    recovery_from_cast={}
    for method,fn in methods.items():
        for intensity in (.25,.5,1.0):
            out=fn(ref_srgb,source_srgb,intensity)
            key=f"{method}_inverse_i{intensity:.2f}"
            save_preview(CAPTURE/f"{key}.png",out)
            np.save(CAPTURE/f"{key}.npy",out.astype(np.float32),allow_pickle=False)
            recovery_from_cast[key]={name:metric(source_srgb,out,source_srgb,mask) for name,mask in regions.items()}
    files={p.name:sha(p) for p in sorted(CAPTURE.iterdir()) if p.is_file() and p.name!="manifest.json"}
    manifest={
      "rd":"rd1-source-mirror",
      "status":"exploratory synthetic cast recovery; not Core execution and not a visual-quality acceptance",
      "source":{"path":"../../CinematicLooksLab/assets/portrait-2024-1536.jpg","sha256":sha(SOURCE),"license":"CC0 1.0 per adjacent source record","icc_profile":"absent; treated as sRGB encoded for this diagnostic"},
      "reference":{"generation":"clip(source_sRGB*[0.94,1.015,1.075]+[0.012,0.006,-0.008],0,1); replace source card at xyxy [64,640,192,736] by neutral RGB 0.82 and apply same cast","role":"known synthetic cast with an inserted neutral card patch; not an independent photographic style target","pixelwise_inverse":"(reference-offset)/scale before clipping; only pixels unaffected by forward clipping have exact recoverability ground truth","saturated_pixels":int(saturation_mask.sum()),"unclipped_pixel_ratio":float(np.mean(~saturation_mask)),"input_domain":"source and reference encoded RGB explicitly constrained to [0,1] for this SDR experiment"},
      "conditions":{"dimensions":[w,h],"working_measurement":"linear RGB float32 copies stored; method mirrors operate on encoded sRGB float channels because that is what the inspected current methods consume","alpha":"opaque RGB source","masks":{"skin_roi_candidate":{"path":"../rd0/portrait_skin_roi_candidate.png","sha256":sha(MASK),"status":"unreviewed candidate"},"bright_window_roi_candidate":{"rect_xyxy":[80,215,300,420],"status":"brightness-selected area; neutrality unverified"},"inserted_neutral_patch":{"rect_xyxy":[64,640,192,736],"status":"synthetic neutral color-card patch, known RGB 0.82 in source; placement and composite semantics are artificial"}},"gate":"none; descriptive only"},
      "implementation_boundary":{"source":"ArtifactCore/src/Color/AutoColorMatch.cpp","source_sha256":sha(CORE_SOURCE),"mirror_script_sha256":sha(Path(__file__).resolve()),"mirror":"independent Python transcription of inspected formulas; not compiled C++ or numerical differential evidence","unverified":["Core execution and C++ differential","ROI approval","natural paired photos","visual review","quality gate","performance","ICC/HDR/alpha/GPU/product route"]},
      "files":files,"results":results,"inverse_cast_recovery":recovery_from_cast
    }
    (CAPTURE/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8",newline="\n")
    print(json.dumps({"capture":str(CAPTURE),"results":results},indent=2))

if __name__=="__main__":main()

