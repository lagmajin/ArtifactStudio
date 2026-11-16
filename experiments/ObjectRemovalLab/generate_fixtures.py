from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parent
OUT = ROOT / "captures" / "rd0"
WIDTH, HEIGHT = 512, 320
SEED = 20261116


def save_png(path: Path, rgb: np.ndarray) -> None:
    encoded = np.clip(np.rint(np.clip(rgb, 0.0, 1.0) ** (1.0 / 2.2) * 255.0), 0, 255).astype(np.uint8)
    Image.fromarray(encoded, "RGB").save(path, format="PNG", optimize=False)


def make_cases() -> dict[str, tuple[np.ndarray, np.ndarray]]:
    y, x = np.mgrid[0:HEIGHT, 0:WIDTH]
    xf, yf = x / (WIDTH - 1), y / (HEIGHT - 1)
    rng = np.random.default_rng(SEED)

    gradient = np.stack((0.12 + 0.62 * xf, 0.16 + 0.58 * yf,
                         0.18 + 0.30 * xf + 0.22 * yf), axis=-1).astype(np.float32)
    gx, gy = np.meshgrid(np.linspace(0.15, 0.85, 16), np.linspace(0.18, 0.82, 10))
    grid = (((np.floor(xf * 16) + np.floor(yf * 10)) % 2) * 0.45 + 0.22).astype(np.float32)
    periodic = np.stack((grid, 0.16 + grid * 0.72, 0.68 - grid * 0.50), axis=-1)
    del gx, gy
    lines = np.full((HEIGHT, WIDTH, 3), 0.52, dtype=np.float32)
    lines[(np.abs(y - (0.56 * x + 38)) <= 1) | (np.abs(y - (-0.42 * x + 225)) <= 1)] = (0.08, 0.18, 0.32)
    for k in range(0, WIDTH, 32):
        lines[:, k:k + 1] = (0.76, 0.38, 0.12)
    noise = rng.normal(0.0, 0.018, (HEIGHT, WIDTH, 1)).astype(np.float32)
    smooth = np.stack((0.25 + 0.25 * xf, 0.27 + 0.26 * yf,
                       0.32 + 0.16 * (xf + yf)), axis=-1).astype(np.float32)
    natural = np.clip(smooth + noise, 0.0, 1.0)

    cases: dict[str, tuple[np.ndarray, np.ndarray]] = {}
    for name, plate, shape in (
        ("gradient_patch", gradient, "rect"),
        ("periodic_patch", periodic.astype(np.float32), "rect"),
        ("fine_lines", lines, "circle"),
        ("textured_wire", natural, "wire"),
    ):
        mask = np.zeros((HEIGHT, WIDTH), dtype=np.uint8)
        if shape == "rect":
            mask[132:188, 224:288] = 255
        elif shape == "circle":
            mask[((x - 256) ** 2 + (y - 160) ** 2) <= 22 ** 2] = 255
        else:
            for t in range(94, 420):
                yy = int(160 + 0.38 * (t - 256))
                mask[max(0, yy - 3):min(HEIGHT, yy + 4), t] = 255

        corrupted = plate.copy()
        corrupted[mask > 0] = (0.88, 0.16, 0.12)
        cases[name] = (plate, np.concatenate((corrupted, mask[..., None].astype(np.float32)), axis=-1))
    return cases


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    records = []
    for name, (clean, packed) in make_cases().items():
        corrupted, mask = packed[..., :3], packed[..., 3]
        paths = {
            "clean": OUT / f"{name}__clean.npy",
            "input": OUT / f"{name}__input.npy",
            "mask": OUT / f"{name}__mask.png",
            "clean_preview": OUT / f"{name}__clean.png",
            "input_preview": OUT / f"{name}__input.png",
        }
        np.save(paths["clean"], clean.astype(np.float32), allow_pickle=False)
        np.save(paths["input"], corrupted.astype(np.float32), allow_pickle=False)
        Image.fromarray(mask.astype(np.uint8), "L").save(paths["mask"], format="PNG", optimize=False)
        save_png(paths["clean_preview"], clean)
        save_png(paths["input_preview"], corrupted)
        records.append({
            "id": name,
            "width": WIDTH,
            "height": HEIGHT,
            "channels": "linear RGB float32; mask is binary 0/255",
            "mask_pixels": int(np.count_nonzero(mask)),
            "files": {
                key: {"path": str(path.relative_to(ROOT)), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                for key, path in paths.items()
            },
        })
    manifest = {
        "rd": "rd0",
        "purpose": "deterministic fixture generation and integrity record; no candidate processing or quality verdict",
        "seed": SEED,
        "dimensions": [WIDTH, HEIGHT],
        "color_contract": "linear RGB float32 in [0,1]; PNG previews use a fixed approximate gamma 2.2 encoding",
        "generator_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "cases": records,
    }
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
