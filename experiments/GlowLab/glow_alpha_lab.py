"""Compare alpha policies for a glow applied to a transparent raster layer."""

from __future__ import annotations

import argparse
from pathlib import Path

import cv2
import numpy as np

from glow_lab import (add_label, linear_to_srgb, make_fixture,
                      srgb_to_linear, to_u8)
from glow_pyramid_lab import pyramid_bloom
from light_leak_alpha_lab import checkerboard, composite_over
from surface_alpha_contract import (composite_linear_premultiplied,
                                    composite_straight_misread_as_premultiplied,
                                    premultiplied_contract_error,
                                    premultiply_linear)


def transparent_neon_plate() -> np.ndarray:
    plate = make_fixture()
    h, w = plate.shape[:2]
    alpha = np.zeros((h, w), dtype=np.float32)
    cv2.line(alpha, (115, 340), (835, 340), 1.0, 16, cv2.LINE_AA)
    cv2.circle(alpha, (260, 205), 74, 1.0, 18, cv2.LINE_AA)
    cv2.circle(alpha, (260, 205), 13, 1.0, -1, cv2.LINE_AA)
    cv2.circle(alpha, (700, 190), 18, 1.0, -1, cv2.LINE_AA)
    cv2.circle(alpha, (760, 265), 9, 1.0, -1, cv2.LINE_AA)
    cv2.putText(alpha, "GLOW", (365, 235), cv2.FONT_HERSHEY_DUPLEX, 1.35,
                1.0, 7, cv2.LINE_AA)
    cv2.putText(alpha, "IMAGE LAB  /  01", (365, 275), cv2.FONT_HERSHEY_SIMPLEX,
                0.55, 1.0, 2, cv2.LINE_AA)
    alpha = cv2.GaussianBlur(alpha, (0, 0), 0.8)
    plate[..., 3] = np.clip(alpha, 0.0, 1.0)
    # The fixture is straight-alpha. Remove hidden RGB only where coverage is
    # exactly zero; the compositor applies the feather coverage once.
    plate[..., :3] *= (alpha > 0.0)[..., None]
    return plate


def core_contract_glow(rgba: np.ndarray, expand_alpha: bool) -> np.ndarray:
    """Reference the Core contract: threshold straight color, blur premult energy."""
    source_linear = srgb_to_linear(rgba[..., :3])
    alpha = np.clip(rgba[..., 3:4], 0.0, 1.0)
    luminance = (source_linear[..., 0] * 0.2126 +
                 source_linear[..., 1] * 0.7152 +
                 source_linear[..., 2] * 0.0722)
    threshold, knee, amount, radius = 0.24, 0.26, 2.2, 6.0
    knee_position = np.clip((luminance - threshold + knee) / (2.0 * knee), 0.0, 1.0)
    soft_mask = knee_position * knee_position * (3.0 - 2.0 * knee_position)
    premultiplied_seed = source_linear * (soft_mask[..., None] * alpha)
    bloom, _levels = pyramid_bloom(premultiplied_seed, radius)
    tint = np.asarray((1.0, 0.92, 0.82), dtype=np.float32)[None, None, :]
    contribution = bloom * amount * tint
    source_premultiplied = source_linear * alpha
    if expand_alpha:
        contribution_luma = (contribution[..., 0] * 0.2126 +
                             contribution[..., 1] * 0.7152 +
                             contribution[..., 2] * 0.0722)
        coverage = np.clip(1.0 - np.exp(-20.0 * np.maximum(contribution_luma, 0.0)),
                           0.0, 0.96)[..., None]
        output_alpha = alpha + (1.0 - alpha) * coverage
        output_premultiplied = source_premultiplied + contribution
    else:
        output_alpha = alpha
        output_premultiplied = source_premultiplied + contribution * alpha
    output_straight = np.divide(
        output_premultiplied, output_alpha,
        out=np.zeros_like(output_premultiplied), where=output_alpha > 1.0e-6)
    rgb = np.clip(linear_to_srgb(np.maximum(output_straight, 0.0)), 0.0, 1.0)
    return np.concatenate((rgb, output_alpha), axis=-1)


def run(output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    source = transparent_neon_plate()
    preserved = core_contract_glow(source, expand_alpha=False)
    clipped = preserved.copy()
    alpha = source[..., 3:4]
    expanded = core_contract_glow(source, expand_alpha=True)

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
        ("SOURCE / TRANSPARENT RASTER", source),
        ("PRESERVE SOURCE ALPHA", preserved),
        ("CLIP CONTRIBUTION TO SOURCE COVERAGE", clipped),
        ("EXPAND ALPHA FROM GLOW ENERGY", expanded),
    ]
    for label, image in panels:
        name = label.split("/")[0].strip().lower().replace(" ", "-") + ".png"
        cv2.imwrite(str(output_dir / name), cv2.cvtColor(
            to_u8(composite_over(image, board)), cv2.COLOR_RGBA2BGRA))

    sheet = np.vstack([
        cv2.resize(add_label(to_u8(composite_over(image, board)), label), None,
                   fx=0.5, fy=0.5, interpolation=cv2.INTER_AREA)
        for label, image in panels
    ])
    cv2.imwrite(str(output_dir / "glow-alpha-policies.png"),
                cv2.cvtColor(sheet, cv2.COLOR_RGBA2BGRA))
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
    cv2.imwrite(str(output_dir / "glow-premult-contract.png"), cv2.cvtColor(
        contract_sheet, cv2.COLOR_RGBA2BGRA))

    # Keep subpixel feather coverage separate from fully transparent pixels:
    # even a 1e-5 alpha remains visible in float linear-light composition.
    fully_transparent = source[..., 3] == 0.0
    preserved_visible = composite_over(preserved, board)
    source_visible = composite_over(source, board)
    expanded_count = int(np.count_nonzero(expanded[..., 3][fully_transparent] > 0.01))
    visible_halo = float(np.max(np.abs(
        preserved_visible[..., :3] - source_visible[..., :3]
    )[fully_transparent]))
    assert np.isfinite(expanded).all()
    assert np.array_equal(preserved[..., 3], source[..., 3])
    assert np.array_equal(clipped[..., 3], source[..., 3])
    assert visible_halo < 1.0e-6, "preserved source alpha exposed a halo outside matte"
    assert expanded_count > 0, "glow-energy policy did not expand alpha"
    assert float(np.max(np.abs(
        composite_over(clipped, board)[..., :3] - source_visible[..., :3]
    )[fully_transparent])) < 1.0e-6
    print(f"Saved glow alpha-policy captures to {output_dir.resolve()}")
    print(f"preserved-alpha visible outside halo delta={visible_halo:.8f}; "
          f"expanded alpha pixels={expanded_count}; premultiplied contract error="
          f"{contract_error:.8f}; straight/premult visual max delta="
          f"{contract_visual_error:.5f}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures" / "glow-alpha")
    run(parser.parse_args().output)
