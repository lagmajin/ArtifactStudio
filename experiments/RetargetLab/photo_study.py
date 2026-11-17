"""Real-photo content-aware retarget study (rd1).

Same lanes and same pre-registered gates as the synthetic rd0 study, applied to three
license-recorded CC0 photographs. Photographs have no ground-truth matte, so this study
does NOT invent a subject mask. It measures mask-free, identity-tracked structure
retention: edges, one-pixel-wide ridges (hair / branches / lettering proxy), gradient
energy, plus seam geometry (jaggedness, localized compression).

Gates are pre-registered in EVALUATION_PLAN.md (rd1 section) and must not be tuned after
seeing these numbers.
"""

from __future__ import annotations

import hashlib
import io
import json
import time
from pathlib import Path

import numpy as np
from PIL import Image

import retarget_study as rs

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[1]
OUT = ROOT / "captures" / "rd1"
WORK_H = 432

PLATES = [
    {
        "name": "portrait",
        "path": "experiments/CinematicLooksLab/assets/portrait-2024-1536.jpg",
        "sha256": "9c14e3c0206ad6fe080c349368bf29219071892002fc01f1b6fc947af3bd40e2",
        "provenance": "experiments/CinematicLooksLab/assets/SOURCES.md",
        "source_page": "https://commons.wikimedia.org/wiki/File:Portrait_2024.jpg",
        "license": "CC0 1.0",
        "role": "face/skin, hair, window highlights",
    },
    {
        "name": "forest",
        "path": "experiments/CinematicLooksLab/assets/forest-wild-1536.jpg",
        "sha256": "b417d6ffe3b494ee80906d6b433544ee7a2afa51c1557dcdf3b8feae9536ce6c",
        "provenance": "experiments/CinematicLooksLab/assets/SOURCES.md",
        "source_page": "https://commons.wikimedia.org/wiki/File:Landscape_of_a_wild_forest.jpg",
        "license": "CC0 1.0",
        "role": "foliage, branches, deep shadows, highlights",
    },
    {
        "name": "neon",
        "path": "experiments/GlowLab/assets/cc0-neon-streets-1536.jpg",
        "sha256": "8791bd593b699be76dcfb181d72dd8655caaae2e88c235b89ed5d9097aa319d4",
        "provenance": "experiments/GlowLab/assets/SOURCES.md",
        "source_page": "https://commons.wikimedia.org/wiki/File:Neon_streets_(Unsplash).jpg",
        "license": "CC0 1.0",
        "role": "signage / lettering, point lights, night texture",
    },
]

LANES = ["crop_center", "stretch", "seam_backward", "seam_forward"]


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    return sha256_bytes(path.read_bytes())


def load_plate(plate: dict) -> tuple[np.ndarray, dict]:
    path = REPO / plate["path"]
    data = path.read_bytes()
    actual = sha256_bytes(data)
    if actual != plate["sha256"]:
        raise SystemExit(f"Plate hash mismatch for {plate['path']}: {actual} != {plate['sha256']}")
    image = Image.open(io.BytesIO(data)).convert("RGB")
    width = max(1, int(round(image.width * (WORK_H / image.height))))
    image = image.resize((width, WORK_H), Image.LANCZOS)
    srgb = np.asarray(image).astype(np.float32) / 255.0
    work = rs.srgb_decode(srgb).astype(np.float32)
    info = {
        "source_path": plate["path"],
        "source_sha256": plate["sha256"],
        "source_size": [int(Image.open(io.BytesIO(data)).width), int(Image.open(io.BytesIO(data)).height)],
        "preprocessing": f"Pillow LANCZOS to height {WORK_H}, sRGB 8-bit, then sRGB-decode to linear float32",
        "working_size": [int(work.shape[1]), int(work.shape[0])],
    }
    return work, info


def carve(img: np.ndarray, target_w: int, mode: str):
    """Remove seams while tracking each surviving pixel's original column."""
    work = img.astype(np.float32).copy()
    rows, width = work.shape[:2]
    source_x = np.broadcast_to(np.arange(width, dtype=np.int32), (rows, width)).copy()
    seams: list[np.ndarray] = []
    while work.shape[1] > target_w:
        if mode == "backward":
            seam = rs.find_seam_backward(rs.backward_energy(work))
        else:
            seam = rs.find_seam_forward(work, None, 0.0)
        seams.append(seam.copy())
        keep = np.ones(work.shape[:2], bool)
        keep[np.arange(rows), seam] = False
        work = work[keep].reshape(rows, -1, 3)
        source_x = source_x[keep].reshape(rows, -1)
    return work, source_x, seams


def alive_map(source_x: np.ndarray, width: int) -> np.ndarray:
    rows = source_x.shape[0]
    alive = np.zeros((rows, width), bool)
    alive[np.arange(rows)[:, None], source_x] = True
    return alive


def compression_runs(source_x: np.ndarray, width: int) -> tuple[float, float]:
    """Contiguous source columns removed between surviving ones, per row."""
    maxima = []
    p95s = []
    for row in range(source_x.shape[0]):
        kept = np.sort(source_x[row])
        # Pad with the frame edges so a whole removed band (as in crop) counts as one run.
        padded = np.concatenate(([-1], kept, [width]))
        gaps = np.diff(padded) - 1
        maxima.append(float(gaps.max()) if gaps.size else 0.0)
        p95s.append(float(np.percentile(gaps, 95)) if gaps.size else 0.0)
    return float(np.max(maxima)), float(np.percentile(p95s, 95))


def jaggedness(seams: list[np.ndarray]) -> tuple[float, float]:
    if not seams:
        return 0.0, 0.0
    means = []
    maximum = 0.0
    for seam in seams:
        step = np.abs(np.diff(seam.astype(np.int32)))
        means.append(float(step.mean()))
        maximum = max(maximum, float(step.max()))
    return float(np.mean(means)), maximum


def structure_masks(work: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    g = rs.luma(work)
    gx = np.zeros_like(g)
    gy = np.zeros_like(g)
    gx[:, 1:-1] = np.abs(g[:, 2:] - g[:, :-2])
    gy[1:-1, :] = np.abs(g[2:, :] - g[:-2, :])
    magnitude = gx + gy
    edge = magnitude >= np.percentile(magnitude, 90.0)

    step = np.abs(np.diff(g, axis=1))
    threshold = np.percentile(step, 99.0)
    left = np.zeros_like(g)
    left[:, 1:] = step
    right = np.zeros_like(g)
    right[:, :-1] = step
    # A one-pixel-wide structure differs strongly from BOTH horizontal neighbours.
    ridge = (left > threshold) & (right > threshold)
    return magnitude, edge, ridge


def retention(alive: np.ndarray, mask: np.ndarray) -> float | None:
    total = int(mask.sum())
    if total == 0:
        return None
    return float(alive[mask].mean())


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    report: dict[str, object] = {
        "rd": "rd1",
        "study": "real-photo content-aware retarget",
        "work_height": WORK_H,
        "target": "1:1 (height preserved, width reduced to height)",
        "generator_sha256": sha256_file(Path(__file__)),
        "shared_module_sha256": sha256_file(ROOT / "retarget_study.py"),
        "lanes": LANES,
        "plates": {},
        "notes": [
            "Photographs have no ground-truth matte; no subject mask is invented and rd0 subject gates are not applied.",
            "Retention metrics use exact pixel identity tracking through seam removal.",
            "thin_ridge = pixel differing strongly from both horizontal neighbours (1-px feature proxy).",
            "Gate thresholds are pre-registered in EVALUATION_PLAN.md (rd1) and were not tuned after measurement.",
            "No visual review was performed by a human judge in this run.",
        ],
    }

    for plate in PLATES:
        work, info = load_plate(plate)
        rows, width = work.shape[:2]
        working_png = OUT / f"{plate['name']}_working.png"
        rs.write_png(working_png, work)
        info["working_png_sha256"] = sha256_file(working_png)

        magnitude, edge, ridge = structure_masks(work)
        total_energy = float(magnitude.sum())
        metrics: dict[str, object] = {}
        tiles: list[np.ndarray] = []

        for lane in LANES:
            t0 = time.perf_counter()
            anisotropy = 1.0
            if lane == "crop_center":
                target = rows
                left = (width - target) // 2
                kept = np.zeros(width, bool)
                kept[left:left + target] = True
                out = work[:, kept]
                source_x = np.broadcast_to(np.nonzero(kept)[0], (rows, target)).copy()
            elif lane == "stretch":
                out = rs.resize_stretch(work, rows, rows)
                source_x = np.broadcast_to(np.arange(width, dtype=np.int32), (rows, width)).copy()
                anisotropy = 1.0 / (rows / width)  # vertical scale 1.0 vs horizontal scale rows/width
            elif lane == "seam_backward":
                out, source_x, seams = carve(work, rows, "backward")
            else:
                out, source_x, seams = carve(work, rows, "forward")
            elapsed_ms = (time.perf_counter() - t0) * 1000.0

            alive = alive_map(source_x, width) if lane != "stretch" else np.ones((rows, width), bool)
            png = OUT / f"{plate['name']}_{lane}.png"
            rs.write_png(png, out)
            tiles.append(out)

            entry: dict[str, object] = {
                "size": [int(out.shape[1]), int(out.shape[0])],
                "runtime_ms": round(elapsed_ms, 1),
                "anisotropy": round(float(anisotropy), 4),
                "edge_pixel_retention": None if lane == "stretch" else round(retention(alive, edge), 4),
                "thin_ridge_retention": None if lane == "stretch" else round(retention(alive, ridge), 4),
                "gradient_energy_retention": (
                    None if lane == "stretch"
                    else round(float(magnitude[alive].sum()) / total_energy, 4)
                ),
                "compression_run_max": None if lane == "stretch" else compression_runs(source_x, width)[0],
                "compression_run_p95": None if lane == "stretch" else compression_runs(source_x, width)[1],
                "removed_gradient_ratio": (
                    None if lane == "stretch" or not (~alive).any()
                    else round(float(magnitude[~alive].mean() / magnitude[alive].mean()), 4)
                ),
                "seam_jaggedness_mean": None,
                "seam_jaggedness_max": None,
                "output_png_sha256": sha256_file(png),
            }
            if lane.startswith("seam"):
                entry["seam_jaggedness_mean"], entry["seam_jaggedness_max"] = (
                    round(v, 3) for v in jaggedness(seams)
                )
            metrics[lane] = entry

        sheet = np.concatenate(tiles, axis=1)
        rs.write_png(OUT / f"{plate['name']}_contact_sheet.png", sheet)
        info["contact_sheet_sha256"] = sha256_file(OUT / f"{plate['name']}_contact_sheet.png")
        info.update(
            {
                "license": plate["license"],
                "source_page": plate["source_page"],
                "provenance": plate["provenance"],
                "role": plate["role"],
                "edge_pixels": int(edge.sum()),
                "thin_ridge_pixels": int(ridge.sum()),
                "metrics": metrics,
            }
        )
        report["plates"][plate["name"]] = info

    (OUT / "metrics.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    for name, info in report["plates"].items():
        print(f"== {name} ({info['working_size'][0]}x{info['working_size'][1]})")
        for lane, entry in info["metrics"].items():
            keys = ("edge_pixel_retention", "thin_ridge_retention", "gradient_energy_retention",
                    "removed_gradient_ratio", "compression_run_max", "compression_run_p95",
                    "seam_jaggedness_mean", "seam_jaggedness_max", "anisotropy", "runtime_ms")
            print("  ", lane, {k: entry[k] for k in keys})


if __name__ == "__main__":
    main()
