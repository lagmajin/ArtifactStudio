"""Shared JSON preset handling for the standalone flash-fill R&D tools."""

from __future__ import annotations

import json
import math
from pathlib import Path
from typing import Any


DEFAULTS: dict[str, Any] = {
    "mode": "radial",
    "source_position": [0.50, 0.14],
    "direction": [-1.0, 0.22],
    "strength": 0.8,
    "softness": 0.19,
    "scatter": 0.6,
    "scatter_front_coupling": 0.0,
    "veil_gain": 0.025,
    "global_fill": 0.03,
    "front_duration": 0.72,
    "decay_rate_per_second": 3.0,
    "highlight_rolloff": 1.0,
    "temperature_slider": 100,
    "temperature": [1.0, 1.0, 1.0],
    "attack_duration": 0.72,
    "peak_time": 0.78,
    "radial_falloff": 2.7,
    "highlight_threshold": 0.28,
    "highlight_range": 0.52,
    "highlight_adaptive_mode": 0,
    "highlight_threshold_percentile": 70.0,
    "highlight_range_end_percentile": 95.0,
    "highlight_smoothing_seconds": 0.4,
}


def load_preset(path: Path | None) -> dict[str, Any]:
    settings = dict(DEFAULTS)
    if path is not None:
        with path.open("r", encoding="utf-8") as stream:
            data = json.load(stream)
        if not isinstance(data, dict) or data.get("schema") != "glowlab.flash.v1":
            raise ValueError(f"Unsupported flash preset: {path}")
        values = data.get("settings")
        if not isinstance(values, dict):
            raise ValueError(f"Preset has no settings object: {path}")
        settings.update(values)
    if settings["mode"] not in ("radial", "directional"):
        raise ValueError("preset mode must be radial or directional")
    position = settings["source_position"]
    if not isinstance(position, list) or len(position) != 2:
        raise ValueError("preset source_position must contain two numbers")
    settings["source_position"] = [float(position[0]), float(position[1])]
    direction = settings.get("direction", [-1.0, 0.22])
    if not isinstance(direction, list) or len(direction) != 2:
        raise ValueError("preset direction must contain two numbers")
    direction_x, direction_y = float(direction[0]), float(direction[1])
    direction_length = math.hypot(direction_x, direction_y)
    if direction_length <= 1.0e-8:
        raise ValueError("preset direction must not be zero")
    settings["direction"] = [direction_x / direction_length, direction_y / direction_length]
    for key in ("strength", "softness", "scatter", "scatter_front_coupling", "veil_gain", "global_fill", "front_duration",
                "decay_rate_per_second", "highlight_rolloff", "attack_duration",
                "peak_time", "radial_falloff", "highlight_threshold", "highlight_range",
                "highlight_threshold_percentile", "highlight_range_end_percentile",
                "highlight_smoothing_seconds"):
        settings[key] = float(settings[key])
    settings["highlight_adaptive_mode"] = int(settings["highlight_adaptive_mode"])
    if settings["highlight_adaptive_mode"] not in (0, 1, 2):
        raise ValueError("highlight_adaptive_mode must be 0 (fixed), 1 (frame percentile), or 2 (EMA)")
    if not (0.0 <= settings["highlight_threshold_percentile"] <
            settings["highlight_range_end_percentile"] <= 100.0):
        raise ValueError("highlight percentile controls must satisfy 0 <= threshold < range end <= 100")
    if settings["highlight_smoothing_seconds"] < 0.0:
        raise ValueError("highlight_smoothing_seconds must be nonnegative")
    settings["temperature_slider"] = int(settings["temperature_slider"])
    tint = settings["temperature"]
    if not isinstance(tint, list) or len(tint) != 3:
        raise ValueError("preset temperature must contain three multipliers")
    settings["temperature"] = [float(v) for v in tint]
    return settings


def save_preset(path: Path, settings: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as stream:
        json.dump({"schema": "glowlab.flash.v1", "settings": settings},
                  stream, indent=2, ensure_ascii=False)
        stream.write("\n")
