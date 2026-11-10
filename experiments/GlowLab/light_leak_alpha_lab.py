"""Probe transparent-edge policies for still-image light-leak effects."""

from __future__ import annotations

import argparse
from pathlib import Path

import cv2
import numpy as np

from glow_lab import add_label, linear_to_srgb, srgb_to_linear, to_u8
from light_leak_lab import edge_wash, make_fixture, organic_gate_leak
from surface_alpha_contract import (composite_linear_premultiplied,
                                    composite_straight_misread_as_premultiplied,
                                    premultiplied_contract_error,
                                    premultiply_linear)


def make_transparent_plate() -> np.ndarray:
    plate = make_fixture()
    h, w = plate.shape[:2]
    alpha = np.zeros((h, w), dtype=np.float32)
    cv2.rectangle(alpha, (190, 92), (770, 450), 1.0, -1, cv2.LINE_AA)
    alpha = cv2.GaussianBlur(alpha, (0, 0), 7.0)
    plate[..., 3] = np.clip(alpha, 0.0, 1.0)
    plate[..., :3] *= (plate[..., 3:4] > 0.0)
    return plate


def checkerboard(height: int, width: int, cell: int = 24) -> np.ndarray:
    y, x = np.mgrid[0:height, 0:width]
    checks = ((x // cell + y // cell) % 2).astype(bool)
    dark = np.asarray((0.20, 0.22, 0.26), dtype=np.float32)
    light = np.asarray((0.43, 0.45, 0.49), dtype=np.float32)
    return np.where(checks[..., None], light[None, None, :], dark[None, None, :])


def composite_over(rgba: np.ndarray, background_srgb: np.ndarray) -> np.ndarray:
    rgb = srgb_to_linear(np.maximum(rgba[..., :3], 0.0))
    background = srgb_to_linear(np.maximum(background_srgb, 0.0))
    alpha = np.clip(rgba[..., 3:4], 0.0, 1.0)
    output = linear_to_srgb(rgb * alpha + background * (1.0 - alpha))
    return np.dstack((np.clip(output, 0.0, 1.0), np.ones_like(alpha[..., 0])))


def run(output_dir: Path, look: str = "edge-wash") -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    source = make_transparent_plate()
    render = edge_wash if look == "edge-wash" else organic_gate_leak
    preserved = render(source)
    clipped = preserved.copy()

    source_linear = srgb_to_linear(source[..., :3])
    effect_linear = srgb_to_linear(preserved[..., :3])
    alpha = source[..., 3:4]
    # Policy 2: attenuate the leak delta by source coverage. Outside the matte,
    # the image color cannot acquire a hidden glow fringe.
    clipped[..., :3] = linear_to_srgb(source_linear + (effect_linear - source_linear) * alpha)

    # Policy 3: treat the leak as light emitted across the image plane and let
    # it create coverage on transparent pixels. This intentionally changes alpha.
    delta = np.maximum(effect_linear - source_linear, 0.0)
    leak_luma = delta[..., 0] * 0.2126 + delta[..., 1] * 0.7152 + delta[..., 2] * 0.0722
    leak_coverage = np.clip(1.0 - np.exp(-1.1 * leak_luma), 0.0, 0.82)[..., None]
    expanded = preserved.copy()
    expanded[..., 3:4] = alpha + (1.0 - alpha) * leak_coverage

    board = checkerboard(source.shape[0], source.shape[1])
    valid_premultiplied = premultiply_linear(expanded)
    invalid_interpretation = composite_straight_misread_as_premultiplied(expanded, board)
    valid_composite = composite_linear_premultiplied(valid_premultiplied, board)
    contract_error = premultiplied_contract_error(expanded)
    contract_visual_error = float(np.max(np.abs(
        invalid_interpretation[..., :3] - valid_composite[..., :3])))
    assert contract_error < 1.0e-6, f"invalid linear-premultiplied RGB: {contract_error}"
    assert contract_visual_error > 0.01
    panels = [
        ("SOURCE / CHECKERBOARD", composite_over(source, board)),
        ("PRESERVE ALPHA / HIDDEN RGB MAY CHANGE", composite_over(preserved, board)),
        ("CLIP LEAK DELTA TO SOURCE COVERAGE", composite_over(clipped, board)),
        ("EXPAND ALPHA FROM LEAK ENERGY", composite_over(expanded, board)),
    ]
    for name, image in panels:
        file_name = name.split("/")[0].strip().lower().replace(" ", "-") + ".png"
        cv2.imwrite(str(output_dir / file_name),
                    cv2.cvtColor(to_u8(image), cv2.COLOR_RGBA2BGRA))

    checker_sheet = np.vstack([
        cv2.resize(add_label(to_u8(image), label), None, fx=0.5, fy=0.5,
                   interpolation=cv2.INTER_AREA)
        for label, image in panels
    ])
    cv2.imwrite(str(output_dir / "light-leak-alpha-policies.png"),
                cv2.cvtColor(checker_sheet, cv2.COLOR_RGBA2BGRA))
    contract_sheet = np.vstack([
        cv2.resize(add_label(to_u8(image), label), None, fx=0.5, fy=0.5,
                   interpolation=cv2.INTER_AREA)
        for label, image in (("STRAIGHT RGB MISREAD AS PREMULTIPLIED", invalid_interpretation),
                             ("VALID LINEAR PREMULTIPLIED OUTPUT", valid_composite),
                             ("CONTRACT ERROR x6", np.dstack((np.clip(
                                 np.abs(invalid_interpretation[..., :3] -
                                        valid_composite[..., :3]) * 6.0, 0.0, 1.0),
                                 np.ones_like(source[..., 3])))))
    ])
    cv2.imwrite(str(output_dir / "light-leak-premult-contract.png"), cv2.cvtColor(
        contract_sheet, cv2.COLOR_RGBA2BGRA))

    transparent = source[..., 3] <= 1.0e-5
    outside_delta = np.max(np.abs(
        composite_over(clipped, board)[..., :3] - composite_over(source, board)[..., :3]
    )[transparent])
    expanded_pixels = int(np.count_nonzero(expanded[..., 3][transparent] > 0.01))
    assert np.isfinite(expanded).all()
    assert np.array_equal(preserved[..., 3], source[..., 3])
    assert np.array_equal(clipped[..., 3], source[..., 3])
    assert outside_delta < 1.0e-6, f"coverage-clipped leak escaped matte: {outside_delta}"
    assert expanded_pixels > 0, "leak-energy policy expanded no transparent pixels"
    print(f"Saved {look} alpha-policy captures to {output_dir.resolve()}")
    print(f"transparent RGB spill is compositor-hidden under preserve-alpha; "
          f"clipped outside-matte composite max delta={outside_delta:.8f}")
    print(f"alpha expansion affected {expanded_pixels} fully transparent pixels; "
          f"premultiplied contract error={contract_error:.8f}; "
          f"straight/premult visual max delta={contract_visual_error:.5f}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--look", choices=("edge-wash", "organic"), default="edge-wash",
                        help="Light-leak look used for the transparent-edge capture")
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures" / "light-leak-alpha")
    args = parser.parse_args()
    run(args.output, "organic" if args.look == "organic" else "edge-wash")
