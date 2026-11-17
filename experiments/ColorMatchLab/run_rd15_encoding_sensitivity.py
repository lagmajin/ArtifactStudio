from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent
CAPTURE = ROOT / "captures" / "rd15_encoding_sensitivity"
CORE_SOURCE = ROOT.parent.parent / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
sys.path.insert(0, str(ROOT))
import run_rd1_source_mirror as mirror  # noqa: E402


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def srgb_to_linear(c: np.ndarray) -> np.ndarray:
    c = np.clip(c, 0.0, 1.0)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def linear_to_srgb(c: np.ndarray) -> np.ndarray:
    c = np.clip(c, 0.0, 1.0)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1.0 / 2.4) - 0.055)


def scene() -> tuple[np.ndarray, dict[str, np.ndarray]]:
    h, w = 192, 256
    y, x = np.mgrid[0:h, 0:w]
    base = np.stack((0.18 + 0.56*x/(w-1), 0.16 + 0.60*y/(h-1), 0.22 + 0.42*(x+y)/(w+h-2)), axis=-1)
    base = np.clip(base, 0, 1)
    masks = {
        "skin_proxy": np.zeros((h, w), bool),
        "neutral_white": np.zeros((h, w), bool),
        "saturated": np.zeros((h, w), bool),
        "gradient_detail": np.zeros((h, w), bool),
    }
    masks["skin_proxy"][24:88, 16:80] = True
    base[24:88, 16:80] = np.array([0.72, 0.43, 0.31]) + ((x[24:88,16:80]-48)/64)[...,None] * np.array([0.035,0.027,0.020])
    masks["neutral_white"][24:88, 96:160] = True
    ramp = np.linspace(0.70, 0.94, 64)[None, :, None]
    base[24:88, 96:160] = np.broadcast_to(ramp, (64,64,3))
    masks["saturated"][104:168, 16:80] = True
    base[104:168, 16:80] = np.array([0.78, 0.20, 0.48])
    masks["gradient_detail"][104:168, 96:240] = True
    base[104:168, 96:240] = np.stack((0.25+0.68*(x[104:168,96:240]-96)/143,
                                      0.82-0.55*(x[104:168,96:240]-96)/143,
                                      0.32+0.18*np.sin((x[104:168,96:240]-96)*np.pi/12)),axis=-1)
    # Small fixed texture avoids a wholly constant-patch-only distribution.
    texture = ((x*17 + y*29) % 11 - 5) * 0.001
    base = np.clip(base + texture[...,None], 0, 1)
    masks["whole_frame"] = np.ones((h,w),bool)
    return base.astype(np.float64), masks


def stats(a: np.ndarray, b: np.ndarray, mask: np.ndarray) -> dict[str, float | int]:
    aa, bb = a[mask], b[mask]
    delta = np.abs(aa-bb)
    return {
        "pixels": int(mask.sum()),
        "rgb_mae": float(delta.mean()),
        "rgb_p95": float(np.percentile(delta,95)),
        "rgb_max": float(delta.max()),
        "output_below_zero_ratio": float(np.mean(bb < 0)),
        "output_above_one_ratio": float(np.mean(bb > 1)),
        "output_non_finite_values": int(bb.size-np.count_nonzero(np.isfinite(bb))),
    }


def main() -> None:
    actual_hash = sha(CORE_SOURCE)
    expected_hash = "62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab"
    if actual_hash != expected_hash:
        raise RuntimeError(f"Core source changed; re-audit before updating pin: {actual_hash}")
    CAPTURE.mkdir(parents=True, exist_ok=True)
    target_srgb, masks = scene()
    scale = np.array([1.018, 0.986, 0.974])
    offset = np.array([-0.006, 0.004, 0.008])
    source_srgb = np.clip(target_srgb * scale + offset, 0, 1)
    arms = {
        "encoded_srgb_float": (source_srgb, target_srgb, "sRGB encoded values; transfer=sRGB"),
        "linear_float": (srgb_to_linear(source_srgb), srgb_to_linear(target_srgb), "linear-light values; transfer=Linear"),
    }
    methods = {"Reinhard": mirror.apply_reinhard, "MeanStddev": mirror.apply_mean_std, "Histogram": mirror.apply_histogram}
    results = []
    for encoding, (src, ref, descriptor) in arms.items():
        baseline = linear_to_srgb(src) if encoding == "linear_float" else src
        for method, fn in methods.items():
            for intensity in (0.25, 0.5, 1.0):
                raw_out = fn(src, ref, intensity)
                display_out = linear_to_srgb(raw_out) if encoding == "linear_float" else raw_out
                item = {"encoding": encoding, "descriptor_contract": descriptor, "method": method,
                        "intensity": intensity, "regions": {name: {
                            "to_reference": stats(target_srgb, display_out, mask),
                            "from_source": stats(baseline, display_out, mask),
                        } for name, mask in masks.items()}}
                results.append(item)
                np.save(CAPTURE / f"{encoding}_{method}_i{intensity:.2f}.npy", raw_out.astype(np.float32), allow_pickle=False)
    cross = []
    for method in methods:
        for intensity in (0.25, 0.5, 1.0):
            a = next(x for x in results if x["encoding"] == "encoded_srgb_float" and x["method"] == method and x["intensity"] == intensity)
            b = next(x for x in results if x["encoding"] == "linear_float" and x["method"] == method and x["intensity"] == intensity)
            ar = np.load(CAPTURE / f"encoded_srgb_float_{method}_i{intensity:.2f}.npy").astype(np.float64)
            br = linear_to_srgb(np.load(CAPTURE / f"linear_float_{method}_i{intensity:.2f}.npy").astype(np.float64))
            cross.append({"method": method, "intensity": intensity,
                          "regions": {name: stats(ar, br, mask) for name, mask in masks.items()}})
    files = {p.name: sha(p) for p in sorted(CAPTURE.iterdir()) if p.is_file() and p.name != "manifest.json"}
    manifest = {
        "rd": "rd15-encoding-sensitivity",
        "status": "completed synthetic equation-mirror sensitivity probe; not Core execution, differential, or quality acceptance",
        "fixture": {"dimensions": [256,192], "generation": "deterministic analytic gradient plus synthetic skin/neutral/saturated/detail patches and fixed encoded-RGB cast", "seed": None,
                    "reference": "uncast synthetic sRGB target", "alpha": "opaque RGB only"},
        "conditions": {"transfer_arms": ["sRGB encoded float", "linear-light float decoded from same sRGB values"],
                       "primaries": "sRGB/Rec.709 D65 assumed for both arms", "methods": list(methods), "intensities": [0.25,0.5,1.0],
                       "comparison": "linear output re-encoded to sRGB before cross-arm and target metrics", "gate": "none; descriptive only"},
        "source_contract": {"ImageF32x4_RGBA_descriptor": "records transfer and primaries; setColorDescriptor retains those fields while canonicalizing float storage and channel order", "matcher_overload": "passes rgba32fData and dimensions only; descriptor is not read or transformed", "source_sha256": actual_hash,
                            "mirror": "independent Python equations imported from run_rd1_source_mirror.py; not C++ or runtime parity"},
        "unverified": ["compiled Core differential", "actual caller metadata population", "ICC/primaries conversion", "HDR and out-of-range float behavior", "alpha and hidden RGB", "visual review", "natural paired portraits and reviewed skin/white", "GPU/product route", "performance"],
        "cross_encoding_output_difference": cross,
        "results": results,
        "files": files,
        "mirror_script_sha256": sha(Path(__file__).resolve()),
        "equation_mirror_script_sha256": sha(Path(__file__).with_name("run_rd1_source_mirror.py")),
    }
    (CAPTURE/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8",newline="\n")
    print(json.dumps({"capture": str(CAPTURE), "cross_encoding_output_difference": cross},indent=2))


if __name__ == "__main__":
    main()
