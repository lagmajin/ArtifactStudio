"""Small grayscale-histogram cut score shared by the independent preview tools."""

from __future__ import annotations

import cv2
import numpy as np


def grayscale_histogram(bgr: np.ndarray, bins: int = 32) -> np.ndarray:
    gray = cv2.cvtColor(bgr, cv2.COLOR_BGR2GRAY)
    return grayscale_histogram_image(gray, bins)


def grayscale_histogram_image(gray: np.ndarray, bins: int = 32) -> np.ndarray:
    histogram = cv2.calcHist([gray], [0], None, [bins], [0, 256])
    return cv2.normalize(histogram, histogram, alpha=1.0, norm_type=cv2.NORM_L1)


def histogram_cut_distance(previous: np.ndarray, current: np.ndarray) -> float:
    """Return a 0..1 Bhattacharyya distance between normalized grayscale histograms."""
    return float(cv2.compareHist(previous, current, cv2.HISTCMP_BHATTACHARYYA))


def grayscale_cut_metrics(previous_bgr: np.ndarray, current_bgr: np.ndarray,
                          bins: int = 32, max_dimension: int = 160) -> tuple[float, float]:
    """Return histogram distance and thumbnail mean-absolute grayscale difference (0..1)."""
    previous_gray = cv2.cvtColor(previous_bgr, cv2.COLOR_BGR2GRAY)
    current_gray = cv2.cvtColor(current_bgr, cv2.COLOR_BGR2GRAY)
    if current_gray.shape != previous_gray.shape:
        current_gray = cv2.resize(current_gray,
                                  (previous_gray.shape[1], previous_gray.shape[0]),
                                  interpolation=cv2.INTER_AREA)
    scale = min(1.0, max_dimension / max(previous_gray.shape[:2]))
    if scale < 1.0:
        size = (max(1, round(previous_gray.shape[1] * scale)),
                max(1, round(previous_gray.shape[0] * scale)))
        previous_gray = cv2.resize(previous_gray, size, interpolation=cv2.INTER_AREA)
        current_gray = cv2.resize(current_gray, size, interpolation=cv2.INTER_AREA)
    histogram_distance = histogram_cut_distance(
        grayscale_histogram_image(previous_gray, bins),
        grayscale_histogram_image(current_gray, bins))
    spatial_difference = float(np.abs(previous_gray.astype(np.float32) -
                                      current_gray.astype(np.float32)).mean() / 255.0)
    return histogram_distance, spatial_difference
