"""CPU image-math probes. Run with python -B hdr_contract.py."""

import numpy as np

from reference import neon_ignition, paper_cutout, time_mosaic


def split_shadow_reference(source, distance, opacity):
    """Straight-alpha linear RGBA, with additive shadow ink."""
    height, width = source.shape[:2]
    rgb = np.zeros_like(source[..., :3])
    alpha = np.zeros_like(source[..., 3])
    for dx, color in ((-1, (1, .04, .03)), (1, (.04, .19, 1)), (0, (1, .78, .03))):
        coverage = np.zeros_like(alpha)
        for step in range(1, min(distance, height - 1) + 1):
            shift = dx * step
            left, right = max(0, shift), min(width, width + shift)
            if left < right:
                target = coverage[step:, left:right]
                np.maximum(target, source[:-step, left-shift:right-shift, 3],
                           out=target)
        weight = np.clip(coverage, 0, 1) * opacity
        rgb += weight[..., None] * color
        alpha += weight * (1 - alpha)
    face = source[..., 3]
    combined = face + alpha * (1 - face)
    premult = source[..., :3] * face[..., None] + rgb * (1 - face[..., None])
    output = source.copy()
    np.divide(premult, combined[..., None], out=output[..., :3],
              where=combined[..., None] > 0)
    output[..., 3] = combined
    return output


def main():
    source = np.zeros((17, 19, 4), np.float32)
    source[3, 9] = (8, 2, .5, 1)
    source[4, 9] = (4, .5, .25, .5)
    shadow = split_shadow_reference(source, 3, .5)
    np.testing.assert_array_equal(shadow[3, 9], source[3, 9])
    np.testing.assert_allclose(shadow[5, 7], [1, .04, .03, .5], atol=1e-7)
    np.testing.assert_array_equal(split_shadow_reference(source, 0, .5), source)
    np.testing.assert_array_equal(split_shadow_reference(source, 3, 0), source)
    np.testing.assert_allclose(shadow[4, 9, 3], .75)

    curve = [[(2, 8), (8, 8), (12, 8), (17, 8)]]
    neon = neon_ignition((17, 19), curve, .5, 3, [1, .6, .2], 4)
    assert neon[..., :3].max() > 8
    assert not neon_ignition((17, 19), curve, 0, 3, [1, .6, .2], 4).any()

    frames = [source.copy() for _ in range(9)]
    for i, frame in enumerate(frames):
        frame[...] = (4 + i, 2, .25, .5)
    mosaic = time_mosaic(frames.__getitem__, 8, 0)
    np.testing.assert_array_equal(mosaic[0, 0], frames[8][0, 0])
    np.testing.assert_array_equal(mosaic[-1, -1], frames[0][-1, -1])

    palette = [(1, .49, .24), (.14, .44, .41), (1, .92, .74)]
    paper = paper_cutout(source, palette, thickness=0)
    np.testing.assert_array_equal(paper[..., 3], source[..., 3])
    assert paper[..., :3].max() <= 1  # Intentional mapping into the paper palette.
    for name, image in (("shadow", shadow), ("neon", neon),
                        ("mosaic", mosaic), ("paper", paper)):
        assert image.dtype == np.float32 and np.isfinite(image).all()
        assert image[..., 3].min() >= 0 and image[..., 3].max() <= 1
        print(f"{name}: peak linear RGB={image[..., :3].max():.3f}")


if __name__ == "__main__":
    main()
