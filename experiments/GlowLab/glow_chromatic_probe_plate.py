"""Generate a deterministic synthetic plate for isolating chromatic bloom roles."""

from __future__ import annotations

import argparse
from pathlib import Path

import cv2
import numpy as np


def make_plate() -> np.ndarray:
    h, w = 540, 960
    image = np.zeros((h, w, 3), dtype=np.uint8)
    image[:] = (5, 7, 12)
    # Isolated point lights with a range of diameters expose dispersion scale.
    for x, y, radius, value in ((145, 140, 2, 255), (260, 140, 5, 235),
                                (370, 140, 9, 255)):
        cv2.circle(image, (x, y), radius, (value, value, value), -1, cv2.LINE_AA)
    # High contrast bars reveal the direction and width of RGB-separated bloom.
    cv2.rectangle(image, (535, 95), (550, 225), (250, 250, 250), -1)
    cv2.rectangle(image, (640, 95), (770, 110), (245, 245, 245), -1)
    cv2.rectangle(image, (830, 95), (845, 225), (255, 255, 255), -1)
    # Saturated emitters test whether tinting survives distinct source colors.
    cv2.circle(image, (130, 390), 8, (255, 40, 20), -1, cv2.LINE_AA)
    cv2.circle(image, (260, 390), 8, (20, 220, 255), -1, cv2.LINE_AA)
    cv2.rectangle(image, (75, 455), (365, 470), (235, 235, 235), -1)
    # A smooth neutral patch makes unintended low-frequency haze easy to spot.
    cv2.rectangle(image, (545, 330), (875, 500), (34, 38, 45), -1)
    cv2.putText(image, "PIN LIGHTS / DIAMETER", (68, 70), cv2.FONT_HERSHEY_SIMPLEX,
                0.62, (180, 190, 205), 1, cv2.LINE_AA)
    cv2.putText(image, "EDGE SPLIT / ANGLE 0", (540, 70), cv2.FONT_HERSHEY_SIMPLEX,
                0.62, (180, 190, 205), 1, cv2.LINE_AA)
    cv2.putText(image, "SATURATED SOURCES", (70, 350), cv2.FONT_HERSHEY_SIMPLEX,
                0.62, (180, 190, 205), 1, cv2.LINE_AA)
    cv2.putText(image, "DARK FIELD CONTROL", (550, 310), cv2.FONT_HERSHEY_SIMPLEX,
                0.62, (180, 190, 205), 1, cv2.LINE_AA)
    return image


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not cv2.imwrite(str(args.output), make_plate()):
        raise RuntimeError(f"Could not write {args.output}")
    print(f"Saved {args.output}")


if __name__ == "__main__":
    main()
