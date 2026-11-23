"""Compare screen-, material-, and frame-random grain with exact synthetic motion."""

from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from adaptive_palette_temporal_stability_study import ASSETS, load_asset  # noqa: E402

CAPTURE = ROOT / "captures" / "grain-motion-coordinate"
ASSET_BY_NAME = {spec[3]: spec for spec in ASSETS}
WIDTH, HEIGHT = 384, 256
WORLD_WIDTH, WORLD_HEIGHT = 768, 384
FRAME_COUNT = 32
CAMERA_DX = 2
SIGMA = 0.025
SEED = 7319
POLICIES = ("screen-fixed", "world-and-object-fixed", "frame-varying")


def make_background() -> np.ndarray:
    yy, xx = np.mgrid[0:WORLD_HEIGHT, 0:WORLD_WIDTH].astype(np.float32)
    rng = np.random.default_rng(319)
    coarse = rng.normal(0, 1, (48, 96)).astype(np.float32)
    coarse = np.asarray(Image.fromarray(coarse, "F").resize((WORLD_WIDTH, WORLD_HEIGHT), Image.Resampling.BILINEAR))
    fine = rng.normal(0, 0.012, (WORLD_HEIGHT, WORLD_WIDTH)).astype(np.float32)
    base = np.stack((
        0.23 + 0.11 * np.sin(xx * 0.018) + 0.035 * np.sin(yy * 0.045),
        0.28 + 0.10 * np.cos(yy * 0.021) + 0.03 * np.sin((xx + yy) * 0.035),
        0.36 + 0.09 * np.sin((xx - yy) * 0.016),
    ), axis=2)
    base += (coarse[:, :, None] * 0.018 + fine[:, :, None])
    return np.clip(base, 0, 1).astype(np.float32)


def load_sprite(name: str, size: tuple[int, int]) -> np.ndarray:
    rgba, _, _ = load_asset(ASSET_BY_NAME[name])
    image = Image.fromarray(rgba, "RGBA").resize(size, Image.Resampling.LANCZOS)
    return np.asarray(image, dtype=np.float32) / 255.0


def add_noise(rgb: np.ndarray, field: np.ndarray) -> np.ndarray:
    return np.clip(rgb + field[:, :, None] * SIGMA, 0, 1)


def composite_layer(dst: np.ndarray, sprite: np.ndarray, x: int, y: int) -> np.ndarray:
    sh, sw = sprite.shape[:2]
    x0, y0 = max(0, x), max(0, y)
    x1, y1 = min(dst.shape[1], x + sw), min(dst.shape[0], y + sh)
    if x1 <= x0 or y1 <= y0:
        return dst
    sx0, sy0 = x0 - x, y0 - y
    src = sprite[sy0:sy0 + (y1 - y0), sx0:sx0 + (x1 - x0)]
    alpha = src[:, :, 3:4]
    dst[y0:y1, x0:x1] = src[:, :, :3] * alpha + dst[y0:y1, x0:x1] * (1 - alpha)
    return dst


def paste_mask(mask: np.ndarray, shape: tuple[int, int], x: int, y: int) -> np.ndarray:
    out = np.zeros(shape, dtype=bool)
    h, w = mask.shape
    x0, y0 = max(0, x), max(0, y)
    x1, y1 = min(shape[1], x + w), min(shape[0], y + h)
    if x1 > x0 and y1 > y0:
        out[y0:y1, x0:x1] = mask[y0 - y:y1 - y, x0 - x:x1 - x]
    return out


def gather_screen_region(array: np.ndarray, x: int, y: int, size: tuple[int, int]) -> np.ndarray:
    height, width = size
    if array.ndim == 3:
        out = np.zeros((height, width, array.shape[2]), dtype=array.dtype)
    else:
        out = np.zeros((height, width), dtype=array.dtype)
    x0, y0 = max(0, x), max(0, y)
    x1, y1 = min(array.shape[1], x + width), min(array.shape[0], y + height)
    if x1 > x0 and y1 > y0:
        out[y0 - y:y1 - y, x0 - x:x1 - x] = array[y0:y1, x0:x1]
    return out


def sprite_position(name: str, frame: int) -> tuple[int, int]:
    if name == "clover_full":
        return 70 + 3 * frame, 68 + frame // 5
    return 425 - 6 * frame, 122 - frame // 10


def make_scene_frames(background: np.ndarray, sprites: dict[str, np.ndarray],
                      background_grain: np.ndarray, sprite_grain: dict[str, np.ndarray],
                      policy: str) -> tuple[list[np.ndarray], list[np.ndarray], list[dict[str, np.ndarray]]]:
    outputs, clean_outputs, masks_out = [], [], []
    screen_rng = np.random.default_rng(SEED + 11)
    frame_rng = np.random.default_rng(SEED + 29)
    screen_field = screen_rng.normal(0, 1, (HEIGHT, WIDTH)).astype(np.float32)
    for frame in range(FRAME_COUNT):
        x_cam, y_cam = CAMERA_DX * frame, 32
        clean_world = background.copy()
        grain_world = background.copy()
        if policy == "world-and-object-fixed":
            grain_world = add_noise(grain_world, background_grain)
        for name in ("clover_full", "silver_feather"):
            x, y = sprite_position(name, frame)
            clean_world = composite_layer(clean_world, sprites[name], x, y)
            if policy == "world-and-object-fixed":
                textured_sprite = sprites[name].copy()
                textured_sprite[:, :, :3] = add_noise(textured_sprite[:, :, :3], sprite_grain[name])
                grain_world = composite_layer(grain_world, textured_sprite, x, y)
        clean = clean_world[y_cam:y_cam + HEIGHT, x_cam:x_cam + WIDTH].copy()
        if policy == "world-and-object-fixed":
            output = grain_world[y_cam:y_cam + HEIGHT, x_cam:x_cam + WIDTH].copy()
        elif policy == "screen-fixed":
            output = add_noise(clean, screen_field)
        else:
            field = frame_rng.normal(0, 1, (HEIGHT, WIDTH)).astype(np.float32)
            output = add_noise(clean, field)
        outputs.append(output)
        clean_outputs.append(clean)
        frame_masks = {}
        for name in ("clover_full", "silver_feather"):
            x, y = sprite_position(name, frame)
            frame_masks[name] = paste_mask(sprites[name][:, :, 3] > 0.98, (WORLD_HEIGHT, WORLD_WIDTH), x, y)[y_cam:y_cam + HEIGHT, x_cam:x_cam + WIDTH]
            frame_masks[name + "_coverage"] = paste_mask(sprites[name][:, :, 3] > 0.01, (WORLD_HEIGHT, WORLD_WIDTH), x, y)[y_cam:y_cam + HEIGHT, x_cam:x_cam + WIDTH]
        # Later-drawn feather occludes the clover at its nontrivial coverage.
        frame_masks["clover_full"] &= ~frame_masks["silver_feather_coverage"]
        frame_masks["background"] = ~(frame_masks["clover_full_coverage"] | frame_masks["silver_feather_coverage"])
        frame_masks["clip_safe"] = np.all((clean > 0.05) & (clean < 0.95), axis=2)
        masks_out.append(frame_masks)
    return outputs, clean_outputs, masks_out


def adjacent_metrics(outputs: list[np.ndarray], clean: list[np.ndarray], masks: list[dict[str, np.ndarray]],
                     sprites: dict[str, np.ndarray]) -> dict[str, object]:
    residuals = [out - base for out, base in zip(outputs, clean)]
    result: dict[str, object] = {}
    screen_pairs, bg_pairs = [], []
    for index in range(FRAME_COUNT - 1):
        valid = masks[index]["clip_safe"] & masks[index + 1]["clip_safe"]
        delta = residuals[index + 1] - residuals[index]
        values = delta[valid]
        screen_pairs.append(float(np.mean(np.abs(values))) if values.size else 0.0)
        # World-background overlap uses exact camera translation and excludes moving sprites.
        cam0, cam1 = CAMERA_DX * index, CAMERA_DX * (index + 1)
        wx0, wx1 = max(cam0, cam1), min(cam0 + WIDTH, cam1 + WIDTH)
        left = residuals[index][:, wx0 - cam0:wx1 - cam0]
        right = residuals[index + 1][:, wx0 - cam1:wx1 - cam1]
        bg0 = masks[index]["background"][:, wx0 - cam0:wx1 - cam0]
        bg1 = masks[index + 1]["background"][:, wx0 - cam1:wx1 - cam1]
        safe = bg0 & bg1 & masks[index]["clip_safe"][:, wx0 - cam0:wx1 - cam0] & masks[index + 1]["clip_safe"][:, wx0 - cam1:wx1 - cam1]
        bg_values = (right - left)[safe]
        bg_pairs.append(float(np.mean(np.abs(bg_values))) if bg_values.size else 0.0)
    result["screen-space residual adjacent MAE by frame"] = screen_pairs
    result["mean screen-space residual adjacent MAE"] = float(np.mean(screen_pairs))
    result["same-world background residual adjacent MAE by frame"] = bg_pairs
    result["mean same-world background residual adjacent MAE"] = float(np.mean(bg_pairs))

    objects = {}
    for name in ("clover_full", "silver_feather"):
        object_pairs, common_counts = [], []
        sprite_h, sprite_w = sprites[name].shape[:2]
        for index in range(FRAME_COUNT - 1):
            # Integer-only sprite translation allows exact source-coordinate alignment by gathering from each screen crop.
            x0, y0 = sprite_position(name, index)
            x1, y1 = sprite_position(name, index + 1)
            cam0, cam1 = CAMERA_DX * index, CAMERA_DX * (index + 1)
            sx0, sy0 = x0 - cam0, y0 - 32
            sx1, sy1 = x1 - cam1, y1 - 32
            local_valid = sprites[name][:, :, 3] > 0.98
            if name == "clover_full":
                local_valid &= ~gather_screen_region(masks[index]["silver_feather_coverage"], sx0, sy0, (sprite_h, sprite_w))
                local_valid &= ~gather_screen_region(masks[index + 1]["silver_feather_coverage"], sx1, sy1, (sprite_h, sprite_w))
            local_valid &= gather_screen_region(masks[index]["clip_safe"], sx0, sy0, (sprite_h, sprite_w))
            local_valid &= gather_screen_region(masks[index + 1]["clip_safe"], sx1, sy1, (sprite_h, sprite_w))
            a = gather_screen_region(residuals[index], sx0, sy0, (sprite_h, sprite_w))
            b = gather_screen_region(residuals[index + 1], sx1, sy1, (sprite_h, sprite_w))
            if np.any(local_valid):
                values = (b - a)[local_valid]
                object_pairs.append(float(np.mean(np.abs(values))))
                common_counts.append(int(np.count_nonzero(local_valid)))
        objects[name] = {
            "mean material-coordinate residual adjacent MAE": float(np.mean(object_pairs)) if object_pairs else 0.0,
            "valid material-coordinate pixels per pair": common_counts,
            "pair count": len(object_pairs),
        }
    result["object-coordinate metrics"] = objects
    return result


def make_sheet(outputs: dict[str, list[np.ndarray]], asset_names: tuple[str, ...]) -> Path:
    tile_w, tile_h, label_h = 256, 170, 20
    selected = (0, FRAME_COUNT // 2, FRAME_COUNT - 1)
    rows = len(POLICIES)
    sheet = Image.new("RGB", (len(selected) * tile_w, rows * (tile_h + label_h)), (28, 29, 33))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    for row, policy in enumerate(POLICIES):
        for col, frame in enumerate(selected):
            x, y = col * tile_w, row * (tile_h + label_h)
            draw.text((x + 3, y + 3), f"{policy} · frame {frame:02d}", fill=(245, 245, 245), font=font)
            image = Image.fromarray(np.uint8(np.clip(outputs[policy][frame] * 255 + .5, 0, 255)), "RGB")
            image = image.resize((tile_w, tile_h), Image.Resampling.NEAREST)
            sheet.paste(image, (x, y + label_h))
    CAPTURE.mkdir(parents=True, exist_ok=True)
    path = CAPTURE / "grain-motion-coordinate-contact-sheet.png"
    sheet.save(path)
    return path


def main() -> dict[str, object]:
    background = make_background()
    sprites = {
        "clover_full": load_sprite("clover_full", (142, 100)),
        "silver_feather": load_sprite("silver_feather", (96, 96)),
    }
    rng = np.random.default_rng(SEED)
    background_grain = rng.normal(0, 1, (WORLD_HEIGHT, WORLD_WIDTH)).astype(np.float32)
    sprite_grain = {name: rng.normal(0, 1, image.shape[:2]).astype(np.float32) for name, image in sprites.items()}
    outputs, clean_outputs, masks = {}, {}, {}
    for policy in POLICIES:
        outputs[policy], clean_outputs[policy], masks[policy] = make_scene_frames(
            background, sprites, background_grain, sprite_grain, policy)
    summary = {policy: adjacent_metrics(outputs[policy], clean_outputs[policy], masks[policy], sprites) for policy in POLICIES}
    sheet = make_sheet(outputs, tuple(sprites))
    report = {
        "question": "With exact transforms and occlusion known, which grain coordinate policy is stable in screen, world-background, and object-material coordinates?",
        "method": "A deterministic 768x384 textured world is viewed through a 384x256 camera panning 2px/frame for 32 frames. Two SHA-verified CC0 cutout sprites move independently and cross with known integer translation; feather is drawn after clover and occludes it. Same sigma=.025 monochrome Gaussian field is applied either once in screen coordinates, once per world/background and sprite-local coordinate, or freshly per frame in screen coordinates. Grain residual is output minus same-frame grain-free scene. Adjacent residual MAE is measured in screen coordinates, exact same-world background overlap, and object-local coordinates; clipped display values are excluded.",
        "scene": {"world dimensions": [WORLD_WIDTH, WORLD_HEIGHT], "output dimensions": [WIDTH, HEIGHT], "frames": FRAME_COUNT,
                  "camera x per frame": CAMERA_DX, "sprite positions generated by": "integer deterministic linear paths with occasional 1px y steps", "occlusion order": ["clover_full", "silver_feather"]},
        "grain sigma": SIGMA, "seed": SEED,
        "assets": [{"asset": name, "source": ASSET_BY_NAME[name][0], "sha256": load_asset(ASSET_BY_NAME[name])[2],
                    "provenance": load_asset(ASSET_BY_NAME[name])[1]} for name in sprites],
        "metrics by policy": summary,
        "contact sheet": str(sheet.relative_to(ROOT)),
        "limits": "Synthetic deterministic integer motion, not video realism. No optical-flow uncertainty, subpixel transforms, rotation/scale, scene cuts, human preference, color management, Core/GPU parity or performance. Screen/object policies are look choices, not a universal ranking.",
    }
    (CAPTURE / "grain-motion-coordinate-metrics.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    report = main()
    print(f"Compared grain coordinate policies across {FRAME_COUNT} exact-motion frames.")
