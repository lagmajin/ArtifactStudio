"""Render a scene-linear exposure contour reference and its SDR preview."""

from pathlib import Path

import numpy as np
from PIL import Image


def contours(source, spacing=1, phase=0, width=.12, strength=.8):
    output = source.copy()
    if strength == 0:
        return output
    luma = source[..., :3] @ np.array([.2126, .7152, .0722], np.float32)
    valid = (luma > 1e-6) & np.isfinite(luma) & (source[..., 3] > 0)
    band = np.log2(luma[valid] / np.float32(.18)) / spacing - phase
    coverage = np.maximum(0, 1 - np.abs(band - np.rint(band)) / width)
    output[valid, :3] += (strength * coverage**2 * luma[valid])[:, None] * [
        .35, .8, 1]
    return output


def main():
    ramp = np.ones((1, 5, 4), np.float32)
    ramp[0, :, :3] = np.array([.18, .36, .72, 2**4.5 * .18, 8])[:, None]
    ramp[0, -1, 3] = 0
    output = contours(ramp)
    np.testing.assert_allclose(output[0, :3, 2] / ramp[0, :3, 2], 1.8)
    np.testing.assert_array_equal(output[0, 3:], ramp[0, 3:])
    np.testing.assert_array_equal(output[..., 3], ramp[..., 3])
    np.testing.assert_array_equal(contours(ramp, strength=0), ramp)

    y, x = np.mgrid[-1:1:540j, -1.8:1.8:960j]
    field = (4.3*np.exp(-((x+.47)**2 + (y+.19)**2)*2.4)
             + 3.7*np.exp(-((x-.73)**2*2 + (y-.3)**2*5))
             + .4*np.sin(5*x + 3*y)*np.exp(-.5*(x*x+y*y)))
    image = np.ones((540, 960, 4), np.float32)
    image[..., :3] = np.exp2(field[..., None] - 2) * [.07, .22, .29]
    result = contours(image, spacing=.35, strength=2)
    assert result.dtype == np.float32 and np.isfinite(result).all()
    directory = Path(__file__).parent / "captures" / "exposure-contours"
    directory.mkdir(parents=True, exist_ok=True)
    np.save(directory / "linear-rgba.npy", result, allow_pickle=False)
    rgb = result[..., :3] / (1 + result[..., :3])
    srgb = np.where(rgb <= .0031308, 12.92 * rgb, 1.055 * rgb**(1/2.4) - .055)
    Image.fromarray(np.rint(srgb * 255).astype(np.uint8)).save(directory / "preview.png")
    print(f"Exposure spacing, alpha and identity checks passed. Peak RGB {result[..., :3].max():.3f}")


if __name__ == "__main__":
    main()
