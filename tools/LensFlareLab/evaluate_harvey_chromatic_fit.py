"""Create a synthetic chromatic Harvey capture for per-channel fitter checks.

The RGB coefficients are deliberately varied to exercise wavelength-dependent
fits. They are sensitivity-test inputs, not measured properties of a lens.
"""

import argparse
import csv
from pathlib import Path

import numpy as np

from evaluate_harvey_scatter_reference import pixel_integrated_scatter


CASES = (("red", -1.78, .36, .0028),
         ("green", -1.86, .36, .00204),
         ("blue", -2.02, .36, .00145))


def write_pfm(path, image):
    path.parent.mkdir(parents=True, exist_ok=True)
    height, width, _ = image.shape
    with path.open("wb") as stream:
        stream.write(f"PF\n{width} {height}\n-1.0\n".encode("ascii"))
        stream.write(np.flipud(image).astype("<f4").tobytes())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pfm", type=Path, default=Path(
        "tools/LensFlareLab/examples/harvey-chromatic-synthetic.pfm"))
    parser.add_argument("--csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/harvey-chromatic-truth.csv"))
    args = parser.parse_args()
    width = height = 512
    pitch = 5.5e-3
    image = np.zeros((height, width, 3), dtype=np.float64)
    for channel, (_, slope, b, l_rad) in enumerate(CASES):
        image[:, :, channel], _, _, _ = pixel_integrated_scatter(
            width, height, pitch, 100.0, slope, b, l_rad, 8)
    write_pfm(args.pfm, image)
    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(("channel", "synthetic_S", "synthetic_B_sr-1", "synthetic_L_mrad"))
        for channel, slope, b, l_rad in CASES:
            writer.writerow((channel, slope, b, l_rad * 1000.0))
    print(f"wrote {args.pfm} ({width}x{height}, 5.5um pixels, 100mm focal length)")
    print("Synthetic per-channel S/L values vary to check chromatic fitting; they are not lens measurements.")


if __name__ == "__main__":
    main()
