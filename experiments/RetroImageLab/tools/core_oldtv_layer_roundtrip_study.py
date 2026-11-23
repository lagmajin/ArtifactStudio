"""Model OldTV alpha representations across the composition layer intermediate."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image

from core_oldtv_natural_alpha_study import artifact_gpu_model, region_masks, summarize_float

ROOT = Path(__file__).resolve().parents[1]
CORPUS = ROOT / "eval-corpus" / "cc0-white-clover"
CAPTURE = ROOT / "captures" / "core-oldtv-layer-roundtrip"


def composite_straight(rgba: np.ndarray, background: tuple[float, float, float]) -> np.ndarray:
    alpha = rgba[:, :, 3:4]
    bg = np.asarray(background, dtype=np.float32).reshape(1, 1, 3)
    return rgba[:, :, :3] * alpha + bg * (1.0 - alpha)


def composite_premultiplied(rgba: np.ndarray, background: tuple[float, float, float]) -> np.ndarray:
    alpha = rgba[:, :, 3:4]
    bg = np.asarray(background, dtype=np.float32).reshape(1, 1, 3)
    return rgba[:, :, :3] + bg * (1.0 - alpha)


def layer_to_float_from_target(rgba: np.ndarray, producer_mode: str) -> np.ndarray:
    """Model source-over to a transparent target, then LayerToFloat HLSL."""
    alpha = np.clip(rgba[:, :, 3:4], 0.0, 1.0)
    if producer_mode == "straight":
        # Standard straight-alpha source-over writes premultiplied RGB to the
        # transparent layer render target.
        target_rgb = rgba[:, :, :3] * alpha
    else:
        # Premultiplied source-over writes the producer's RGB directly.
        target_rgb = rgba[:, :, :3]
    straight = np.zeros_like(target_rgb)
    visible = alpha[:, :, 0] > 1.0e-6
    straight[visible] = target_rgb[visible] / alpha[visible]
    straight = np.clip(straight, 0.0, 1.0)
    return np.concatenate((straight, alpha), axis=2)


def study_asset(entry: dict[str, object], backgrounds: dict[str, tuple[float, float, float]]) -> dict[str, object]:
    path = CORPUS / str(entry["file"])
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != entry["sha256"]:
        raise ValueError(f"provenance hash mismatch: {path.name}")
    source_u8 = np.asarray(Image.open(path).convert("RGBA"), dtype=np.uint8)
    masks = region_masks(source_u8[:, :, 3])
    straight_input = source_u8.astype(np.float32) / 255.0
    premult_input = straight_input.copy()
    premult_input[:, :, :3] *= premult_input[:, :, 3:4]
    rows = {}
    for mode, encoded in (("straight", straight_input), ("premultiplied", premult_input)):
        gpu, shader_stats = artifact_gpu_model(encoded)
        recovered = layer_to_float_from_target(gpu, mode)
        direct_composite = composite_straight(gpu, backgrounds["mid-gray"]) if mode == "straight" else composite_premultiplied(gpu, backgrounds["mid-gray"])
        roundtrip_composite = composite_straight(recovered, backgrounds["mid-gray"])
        rows[mode] = {
            "GPU equation model": shader_stats,
            "input encoding": "straight RGB" if mode == "straight" else "RGB multiplied by alpha",
            "roundtrip modeled": "source-over to transparent premultiplied layer target, then LayerToFloat src.rgb/src.a + saturate",
            "roundtrip vs direct consumer, edge": summarize_float(direct_composite, roundtrip_composite, masks["edge"]),
            "roundtrip vs direct consumer, partial alpha": summarize_float(direct_composite, roundtrip_composite, masks["partial-alpha"]),
            "GPU output premult invariant violation pixels": int(np.count_nonzero(np.any(gpu[:, :, :3] > gpu[:, :, 3:4] + 1e-6, axis=2))) if mode == "premultiplied" else None,
            "roundtrip pixels affected by saturate on edge": int(np.count_nonzero(np.any((recovered[:, :, :3] >= 1.0 - 1e-7) & (gpu[:, :, 3:4] < 1.0 - 1e-6), axis=2) & masks["edge"])) if mode == "premultiplied" else 0,
        }
    return {"asset": entry["file"], "sha256 verified": True, "dimensions": [int(source_u8.shape[1]), int(source_u8.shape[0])], "alpha-zero pixels": int(np.count_nonzero(source_u8[:, :, 3] == 0)), "partial-alpha pixels": int(np.count_nonzero(masks["partial-alpha"])), "encodings": rows}


def main() -> dict[str, object]:
    manifest = json.loads((CORPUS / "manifest.json").read_text(encoding="utf-8"))
    backgrounds = {"mid-gray": (0.5, 0.5, 0.5)}
    results = [study_asset(entry, backgrounds) for entry in manifest]
    report = {
        "question": "How does the source-derived OldTV GPU output behave when a straight or premultiplied producer is rendered into the composition layer intermediate and then converted by LayerToFloat?",
        "source contract": "CompositionView prepares layerSRV via LayerBlendPipeline.convertLayerToFloat before eligible deferred GPU raster passes. The shader unconditionally computes saturate(src.rgb / alpha), or zero RGB for alpha <= 1e-6. This study models a rendered transparent-target roundtrip; it does not prove OldTV's live caller chooses either producer association.",
        "model": "Artifact OldTV HLSL NumPy translation followed by alpha-correct source-over to a transparent premultiplied target and LayerToFloat's straight conversion.",
        "backgrounds": backgrounds,
        "limits": "Equation model only: no Artifact.exe, GPU execution, source readback, runtime effect routing, or performance measurement. The mode is parameterized; source caller's GPU blend convention and descriptor are not captured.",
        "results": results,
    }
    CAPTURE.mkdir(parents=True, exist_ok=True)
    (CAPTURE / "core-oldtv-layer-roundtrip-metrics.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    result = main()
    print(f"Measured {sum(len(asset['encodings']) for asset in result['results'])} OldTV layer-intermediate alpha roundtrips.")
