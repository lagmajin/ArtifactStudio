from __future__ import annotations

import json
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent
OUT = ROOT / "captures" / "rd0"


def stats(values: np.ndarray) -> dict[str, object]:
    return {
        "pixels": int(values.shape[0]),
        "mean_rgb": values.mean(axis=0).tolist(),
        "std_rgb": values.std(axis=0).tolist(),
        "p95_rgb": np.percentile(values, 95, axis=0).tolist(),
    }


def main() -> None:
    # Small deterministic synthetic alpha counterfactual. The public API's
    # source currently has no alpha-weighted statistics contract; this script
    # measures the risk pattern, not the implementation's output.
    yy, xx = np.mgrid[0:192, 0:320]
    x, y = xx / 319.0, yy / 191.0
    visible = np.stack((0.34 + 0.32 * x, 0.24 + 0.26 * y,
                        0.16 + 0.14 * x + 0.07 * y), axis=-1).astype(np.float32)
    alpha = np.ones((192, 320), np.float32)
    alpha[:, :80] = 0.0
    alpha[:, 80:104] = np.linspace(0.0, 1.0, 24, dtype=np.float32)[None, :]
    hidden_a = visible.copy()
    hidden_b = visible.copy()
    hidden_a[alpha == 0] = (0.0, 0.0, 0.0)
    hidden_b[alpha == 0] = (1.0, 0.0, 1.0)
    opaque = alpha >= (1.0 - 1e-6)
    partial = (alpha > 0) & (alpha < 1.0)
    report = {
        "purpose": "alpha-statistics counterfactual only; no AutoColorMatcher execution or product output",
        "condition": "synthetic straight RGB float32; common visible pixels and alpha; hidden RGB varies only where alpha is zero",
        "alpha_counts": {"transparent": int(np.sum(alpha == 0)), "partial": int(np.sum(partial)),
                         "opaque": int(np.sum(opaque))},
        "all_pixels_hidden_rgb_variant_a": stats(hidden_a.reshape(-1, 3)),
        "all_pixels_hidden_rgb_variant_b": stats(hidden_b.reshape(-1, 3)),
        "opaque_only_variant_a": stats(hidden_a[opaque]),
        "opaque_only_variant_b": stats(hidden_b[opaque]),
        "opaque_only_visible_input_difference_max": float(np.max(np.abs(hidden_a[opaque] - hidden_b[opaque]))),
        "interpretation": "If a whole-image method derives statistics from transparent RGB, identical visible content can produce different transforms. Source inspection indicates all current methods include every pixel; this counterfactual does not quantify their actual output difference.",
        "unverified": ["actual C++ method outputs", "partial-alpha compositing difference", "alpha-weighted alternatives", "human visible review"],
    }
    path = OUT / "alpha_statistics_counterfactual.json"
    path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
