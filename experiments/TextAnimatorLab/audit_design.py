"""Design-level text animator audit; intentionally independent of ArtifactCore."""

from __future__ import annotations

import argparse
import json
import math
import unicodedata
import re
import random
from collections import defaultdict
from pathlib import Path
from typing import Any


ROOT = Path(__file__).parent


def is_json_number(value: Any) -> bool:
    """JSON number semantics: bool is not a number even though Python says it is."""
    return type(value) in {int, float}


def load_fixtures() -> list[dict[str, Any]]:
    return json.loads((ROOT / "fixtures.json").read_text(encoding="utf-8"))["fixtures"]


def virtual_glyphs(text: str) -> list[dict[str, Any]]:
    """Build a conservative grapheme-like model, not a font shaper.

    Combining marks, variation selectors, emoji modifiers, and ZWJ-linked
    codepoints stay in one cluster. The ArtifactCore adapter must later
    compare this model with the real shaping result.
    """
    result: list[dict[str, Any]] = []
    cluster = -1
    line = 0
    join_next = False
    regional_count = 0
    for index, char in enumerate(text):
        code = ord(char)
        combining = unicodedata.combining(char) != 0
        variation = 0xFE00 <= code <= 0xFE0F
        modifier = 0x1F3FB <= code <= 0x1F3FF
        regional = 0x1F1E6 <= code <= 0x1F1FF
        zwj = code == 0x200D
        newline = char == "\n"
        if newline:
            line += 1
            join_next = False
            regional_count = 0
            continue
        if cluster < 0 or (not combining and not variation and not modifier and
                           not zwj and not join_next and
                           not (regional and regional_count == 1)):
            cluster += 1
        regional_count = regional_count + 1 if regional else 0
        result.append({"index": index, "char": char, "cluster": cluster, "line": line})
        join_next = zwj
    return result


def ordered_rank(index: int, count: int, order: str, seed: int = 0) -> int:
    if count <= 0:
        return index
    sequence = list(range(count))
    if order in {"reverse", "right_to_left"}:
        sequence.reverse()
    elif order == "random_stable":
        random.Random(seed).shuffle(sequence)
    elif order == "center_out":
        center = (count - 1) * 0.5
        sequence.sort(key=lambda value: (abs(value - center), value))
    elif order == "edge_in":
        sequence = []
        left, right = 0, count - 1
        while left <= right:
            sequence.append(left)
            left += 1
            if left <= right:
                sequence.append(right)
                right -= 1
    return sequence.index(index)


def ordered_ranks(count: int, order: str, seed: int = 0) -> list[int]:
    """Build the inverse order map once for an evaluation pass."""
    sequence = list(range(count))
    if order in {"reverse", "right_to_left"}:
        sequence.reverse()
    elif order == "random_stable":
        random.Random(seed).shuffle(sequence)
    elif order == "center_out":
        center = (count - 1) * 0.5
        sequence.sort(key=lambda value: (abs(value - center), value))
    elif order == "edge_in":
        sequence = []
        left, right = 0, count - 1
        while left <= right:
            sequence.append(left)
            left += 1
            if left <= right:
                sequence.append(right)
                right -= 1
    ranks = [0] * count
    for rank, domain_index in enumerate(sequence):
        ranks[domain_index] = rank
    return ranks


def wiggly_weight(index: int, time: float, enabled: bool = False,
                  rate: float = 2.0, correlation: float = 50.0,
                  phase: float = 0.0, seed: int = 12345) -> float:
    """Deterministic 0..1 Wiggly application weight, matching Core semantics."""
    if not enabled:
        return 1.0
    safe_correlation = max(0.0, min(100.0, correlation if math.isfinite(correlation) else 50.0))
    safe_rate = max(0.0, rate if math.isfinite(rate) else 0.0)
    safe_time = time if math.isfinite(time) else 0.0
    safe_phase = phase if math.isfinite(phase) else 0.0
    raw_t = safe_time * safe_rate + index * (100.0 - safe_correlation) / 100.0 + safe_phase
    floor_tick = math.floor(raw_t)
    tick = int(floor_tick) % (2 ** 32)
    def sample(sample_tick: int) -> float:
        return random.Random((seed + sample_tick) % (2 ** 32)).uniform(-1.0, 1.0)
    v1, v2 = sample(tick), sample((tick + 1) % (2 ** 32))
    fraction = raw_t - floor_tick
    smooth = 0.5 - 0.5 * math.cos(fraction * math.pi)
    return max(0.0, min(1.0, 0.5 + 0.5 * (v1 + (v2 - v1) * smooth)))


def lerp_color(start: list[float], end: list[float], amount: float) -> list[float]:
    if (len(start) != 4 or len(end) != 4 or
            not all(is_json_number(value) and math.isfinite(value)
                    for value in start + end)):
        raise ValueError("invalid color")
    t = max(0.0, min(1.0, amount))
    return [a + (b - a) * t for a, b in zip(start, end)]


def weight(index: int, count: int, start: float, end: float, shape: str,
           offset: float = 0.0, ease_high: float = 0.0,
           ease_low: float = 0.0, order: str = "natural", seed: int = 0,
           rank_map: list[int] | None = None) -> float:
    if count <= 0 or index < 0 or index >= count:
        return 0.0
    low, high = sorted((start + offset, end + offset))
    index = rank_map[index] if rank_map is not None else ordered_rank(index, count, order, seed)
    position = 0.0 if count == 1 else index * 100.0 / (count - 1)
    if position < low or position > high:
        return 0.0
    t = 1.0 if abs(high - low) < 1e-9 else (position - low) / (high - low)
    if ease_high > 0.001:
        t = t ** max(0.01, 1.0 + ease_high * 0.1)
    if ease_low > 0.001:
        t = 1.0 - (1.0 - t) ** max(0.01, 1.0 + ease_low * 0.1)
    t = max(0.0, min(1.0, t))
    if shape == "Square":
        return 1.0
    if shape == "RampUp":
        return t
    if shape == "RampDown":
        return 1.0 - t
    if shape == "Triangle":
        return t * 2.0 if t < 0.5 else (1.0 - t) * 2.0
    if shape == "Round":
        return math.sqrt(max(0.0, 1.0 - (t * 2.0 - 1.0) ** 2))
    if shape == "Smooth":
        return 0.5 - 0.5 * math.cos(t * math.pi)
    raise ValueError(f"unknown shape: {shape}")


def evaluate(glyphs: list[dict[str, Any]], animators: list[dict[str, Any]]) -> list[dict[str, Any]]:
    result = [
        {"index": glyph["index"], "position": [0.0, 0.0], "scale": 1.0,
         "rotation": 0.0, "opacity": 1.0, "skew": 0.0, "tracking": 0.0,
         "z": 0.0, "blur": 0.0, "strokeWidth": 0.0,
         "fillColor": [1.0, 1.0, 1.0, 1.0],
         "strokeColor": [1.0, 1.0, 1.0, 1.0],
         "hasColorOverride": False, "hasStrokeOverride": False}
        for glyph in glyphs
    ]
    count = len(glyphs)
    for animator in animators:
        selector = animator["selector"]
        properties = animator["properties"]
        # Core's default RangeSelector is Percentage, whose domain is the
        # logical cluster domain rather than raw codepoint order.
        unit = selector.get("unit", "percentage")
        cluster_count = len({glyph["cluster"] for glyph in glyphs})
        line_count = len({glyph["line"] for glyph in glyphs})
        cumulative_tracking = 0.0
        order = selector.get("order", "natural")
        seed = selector.get("seed", 0)
        rank_count = (cluster_count if unit in {"cluster", "percentage"}
                      else line_count if unit == "line" else count)
        rank_map = ordered_ranks(rank_count, order, seed)
        for item_index, item in enumerate(result):
            source = glyphs[item_index]
            item["position"][0] += cumulative_tracking
            if unit in {"cluster", "percentage"}:
                domain_index, domain_count = source["cluster"], cluster_count
            elif unit == "line":
                domain_index, domain_count = source["line"], line_count
            else:
                domain_index, domain_count = item_index, count
            amount = weight(domain_index, domain_count, selector["start"],
                            selector["end"], selector["shape"],
                            selector.get("offset", 0.0),
                            selector.get("easeHigh", selector.get("ease_high", 0.0)),
                            selector.get("easeLow", selector.get("ease_low", 0.0)),
                            order, seed, rank_map)
            wiggle = selector.get("wiggly", {})
            if isinstance(wiggle, dict):
                amount *= wiggly_weight(source["cluster"], selector.get("time", 0.0),
                                        wiggle.get("enabled", False),
                                        wiggle.get("rate", 2.0),
                                        wiggle.get("correlation", 50.0),
                                        wiggle.get("phase", 0.0),
                                        wiggle.get("seed", 12345))
            extra_weights = selector.get("extraWeights", [])
            if isinstance(extra_weights, list) and item_index < len(extra_weights):
                extra = extra_weights[item_index]
                if not is_json_number(extra) or not math.isfinite(extra):
                    extra = 0.0
                amount *= max(0.0, min(1.0, extra))
            item["position"][0] += properties["position"][0] * amount
            item["position"][1] += properties["position"][1] * amount
            item["scale"] *= 1.0 + (properties["scale"] - 1.0) * amount
            item["rotation"] += properties["rotation"] * amount
            item["opacity"] *= 1.0 - (1.0 - properties["opacity"]) * amount
            item["skew"] += properties.get("skew", 0.0) * amount
            item["z"] += properties.get("z", 0.0) * amount
            item["blur"] += properties.get("blur", 0.0) * amount
            item["strokeWidth"] += properties.get("strokeWidth", 0.0) * amount
            item["tracking"] += properties.get("tracking", 0.0) * amount
            if properties.get("colorEnabled"):
                color = properties.get("fillColor")
                if not isinstance(color, list) or len(color) != 4:
                    raise ValueError("invalid fill color")
                item["fillColor"] = lerp_color(item["fillColor"], color, amount)
                item["hasColorOverride"] = amount > 0.0
            if properties.get("strokeEnabled"):
                color = properties.get("strokeColor")
                if not isinstance(color, list) or len(color) != 4:
                    raise ValueError("invalid stroke color")
                item["strokeColor"] = lerp_color(item["strokeColor"], color, amount)
                item["hasStrokeOverride"] = amount > 0.0
            if (item_index + 1 == len(glyphs) or
                    glyphs[item_index + 1]["cluster"] != source["cluster"]):
                cumulative_tracking += properties.get("tracking", 0.0) * amount
    return result


def audit_fixture(fixture: dict[str, Any]) -> dict[str, Any]:
    glyphs = virtual_glyphs(fixture["text"])
    animators = [{
        "selector": {"start": 0.0, "end": 100.0, "shape": "Smooth"},
        "properties": {"position": [12.0, -4.0], "scale": 0.8,
                       "rotation": 15.0, "opacity": 0.5},
    }]
    states = evaluate(glyphs, animators)
    issues: list[str] = []
    if len(states) != len(glyphs):
        issues.append("glyph/state count mismatch")
    for state in states:
        values = state["position"] + [state["scale"], state["rotation"], state["opacity"]]
        if not all(math.isfinite(value) for value in values):
            issues.append(f"non-finite state at glyph {state['index']}")
        if not 0.0 <= state["opacity"] <= 1.0:
            issues.append(f"opacity out of range at glyph {state['index']}")
        if state["scale"] < 0.0:
            issues.append(f"negative scale at glyph {state['index']}")
    state_sample = states[:2] + (states[-2:] if len(states) > 2 else [])
    return {
        "id": fixture["id"], "tier": fixture["tier"],
        "text": fixture["text"], "codepointCount": len(fixture["text"]),
        "virtualGlyphCount": len(glyphs),
        "virtualClusterCount": len({g["cluster"] for g in glyphs}),
        "virtualLineCount": (fixture["text"].count("\n") + 1
                              if fixture["text"] else 0),
        "stateCount": len(states),
        "stateFinite": not any("non-finite state" in issue for issue in issues),
        "stateSample": state_sample,
        "issues": sorted(set(issues)),
        "status": "error" if issues else "pass",
    }


def rotation_demo(fixture: dict[str, Any], amount: float = 90.0) -> dict[str, Any]:
    """Preview practical per-character/per-word rotation without Core or a renderer."""
    glyphs = virtual_glyphs(fixture["text"])

    def run(selector: dict[str, Any], weights: list[float] | None = None) -> list[float]:
        selector = dict(selector)
        if weights is not None:
            selector["extraWeights"] = weights
        states = evaluate(glyphs, [{
            "selector": selector,
            "properties": {"position": [0.0, 0.0], "scale": 1.0,
                           "rotation": amount, "opacity": 1.0},
        }])
        return [state["rotation"] for state in states]

    # Natural order: each rendered glyph gets its own rotation weight.
    character = run({"start": 0.0, "end": 100.0, "shape": "RampUp"})

    # Word order: all glyphs in one word share the same weight; spaces stay still.
    words = []
    current_word = -1
    previous_space = True
    for glyph in glyphs:
        if glyph["char"].isspace():
            words.append(None)
            previous_space = True
        else:
            if previous_space:
                current_word += 1
            words.append(current_word)
            previous_space = False
    word_count = max(1, current_word + 1)
    word_weights = [
        0.0 if word is None else (word + 1) / word_count
        for word in words
    ]
    word = run({"start": 0.0, "end": 100.0, "shape": "Square"}, word_weights)

    # Center-out order: the selector order changes only the assignment, not the range.
    center_out = run({"start": 0.0, "end": 100.0, "shape": "RampUp",
                      "order": "center_out"})
    return {"fixture": fixture["id"], "text": fixture["text"],
            "amount": amount, "rotationDegrees": {
                "characterNatural": character,
                "wordProgressive": word,
                "centerOut": center_out,
            }}


def validate_intent(intent: dict[str, Any], fixture: dict[str, Any]) -> list[dict[str, str]]:
    diagnostics: list[dict[str, str]] = []
    if not isinstance(intent, dict):
        return [{"code": "invalidIntentType", "severity": "error"}]
    allowed_intent = {"action", "target", "selection", "operators", "timing", "options"}
    for key in intent:
        if key not in allowed_intent:
            diagnostics.append({"code": "unknownIntentField", "severity": "error"})
    if intent.get("action") != "create_text_animation":
        diagnostics.append({"code": "unsupportedAction", "severity": "error"})
    if not isinstance(intent.get("target"), str) or not intent.get("target"):
        diagnostics.append({"code": "targetNotFound", "severity": "error"})
    selection = intent.get("selection", {})
    if not isinstance(selection, dict):
        return [{"code": "invalidSelectionType", "severity": "error"}]
    for key in selection:
        if key not in {"unit", "order", "value", "pattern"}:
            diagnostics.append({"code": "unknownSelectionField", "severity": "error"})
    unit = selection.get("unit")
    if unit not in {"character", "grapheme", "word", "line", "paragraph", "tag", "regex"}:
        diagnostics.append({"code": "unsupportedSelectionUnit", "severity": "error"})
    if selection.get("order") not in {"natural", "reverse", "random_stable", "center_out", "edge_in"}:
        diagnostics.append({"code": "unsupportedSelectionOrder", "severity": "error"})
    if unit == "regex" and not isinstance(selection.get("pattern"), str):
        diagnostics.append({"code": "invalidRegex", "severity": "error"})
    if unit == "regex" and isinstance(selection.get("pattern"), str) and not selection["pattern"]:
        diagnostics.append({"code": "invalidRegex", "severity": "error"})
    if unit == "regex" and isinstance(selection.get("pattern"), str) and len(selection["pattern"]) > 4096:
        diagnostics.append({"code": "invalidRegex", "severity": "error"})
    if unit == "regex" and isinstance(selection.get("pattern"), str) and selection.get("pattern"):
        try:
            re.compile(selection["pattern"])
        except re.error:
            diagnostics.append({"code": "invalidRegex", "severity": "error"})
    if unit == "tag" and not selection.get("value"):
        diagnostics.append({"code": "missingSelectionValue", "severity": "error"})
    if unit == "tag" and selection.get("value") and not isinstance(selection.get("value"), str):
        diagnostics.append({"code": "invalidSelectionValue", "severity": "error"})
    if unit == "tag" and selection.get("value") and not fixture.get("tags"):
        diagnostics.append({"code": "tagMetadataUnavailable", "severity": "warning"})
    if unit == "line" and (type(selection.get("value")) is not int):
        diagnostics.append({"code": "missingSelectionValue", "severity": "error"})
    if unit == "line" and type(selection.get("value")) is int and selection["value"] < 0:
        diagnostics.append({"code": "invalidSelectionValue", "severity": "error"})
    if unit == "word" and not fixture["text"].split():
        diagnostics.append({"code": "selectionEmpty", "severity": "warning"})
    timing = intent.get("timing", {})
    if not isinstance(timing, dict):
        diagnostics.append({"code": "invalidTimingType", "severity": "error"})
        timing = {}
    for key in timing:
        if key not in {"duration", "stagger", "easing", "start"}:
            diagnostics.append({"code": "unknownTimingField", "severity": "error"})
    for key in ("duration", "stagger"):
        value = timing.get(key)
        if not is_json_number(value) or not math.isfinite(value) or value < 0:
            diagnostics.append({"code": "timelineOutOfRange", "severity": "error"})
        elif value > 86400:
            diagnostics.append({"code": "timelineOutOfRange", "severity": "error"})
    if "easing" in timing and timing["easing"] not in {
            "linear", "smooth", "sharp", "spring", "custom"}:
        diagnostics.append({"code": "unsupportedEasing", "severity": "error"})
    operators = intent.get("operators", [])
    if not isinstance(operators, list):
        diagnostics.append({"code": "invalidOperatorList", "severity": "error"})
        operators = []
    if not operators:
        diagnostics.append({"code": "operatorConflict", "severity": "error"})
    if len(operators) > 32:
        diagnostics.append({"code": "operatorConflict", "severity": "error"})
    supported_operators = {"position", "scale", "rotation", "opacity", "skew",
                           "tracking", "color", "stroke", "blur", "noise"}
    supported_modes = {"add", "replace", "multiply", "blend", "min", "max"}
    scalar_operators = {"scale", "rotation", "opacity", "skew", "tracking", "stroke", "blur", "noise"}
    for operator in operators:
        if not isinstance(operator, dict):
            diagnostics.append({"code": "invalidOperatorType", "severity": "error"})
            continue
        operator_type = operator.get("type")
        for key in operator:
            if key not in {"type", "mode", "from", "to", "amount", "axis", "color", "seed"}:
                diagnostics.append({"code": "unknownOperatorField", "severity": "error"})
        if operator_type not in supported_operators:
            diagnostics.append({"code": "unsupportedOperator", "severity": "error"})
            continue
        if operator.get("mode", "add") not in supported_modes:
            diagnostics.append({"code": "unsupportedOperatorMode", "severity": "error"})
        if "axis" in operator and operator["axis"] not in {"x", "y", "z", "xy", "xyz"}:
            diagnostics.append({"code": "unsupportedOperatorAxis", "severity": "error"})
        if "seed" in operator and type(operator["seed"]) is not int:
            diagnostics.append({"code": "invalidOperatorSeed", "severity": "error"})
        if "from" not in operator or "to" not in operator:
            diagnostics.append({"code": "missingOperatorRange", "severity": "error"})
            continue
        values = (operator.get("from"), operator.get("to"))
        if operator_type in scalar_operators:
            if not all(is_json_number(value) and math.isfinite(value)
                       for value in values):
                diagnostics.append({"code": "invalidOperatorRange", "severity": "error"})
            if operator_type == "opacity" and all(
                    is_json_number(value) for value in values) and not all(
                    0.0 <= value <= 1.0 for value in values):
                diagnostics.append({"code": "operatorValueOutOfRange", "severity": "error"})
            if operator_type == "scale" and all(
                    is_json_number(value) for value in values) and any(
                    value < 0.0 for value in values):
                diagnostics.append({"code": "operatorValueOutOfRange", "severity": "error"})
        elif operator_type == "position":
            axis = operator.get("axis")
            if axis in {"x", "y", "z"}:
                valid_position = all(is_json_number(value) and
                                     math.isfinite(value) for value in values)
            else:
                valid_position = all(isinstance(value, list) and len(value) in {2, 3} and
                                     all(is_json_number(component) and math.isfinite(component)
                                         for component in value) for value in values)
            if not valid_position:
                diagnostics.append({"code": "invalidOperatorRange", "severity": "error"})
        elif operator_type == "color":
            color_values = (operator.get("from"), operator.get("to"))
            if not all(isinstance(value, str) and re.fullmatch(r"#[0-9A-Fa-f]{6,8}", value)
                       for value in color_values):
                diagnostics.append({"code": "invalidOperatorColor", "severity": "error"})
    options = intent.get("options", {})
    if not isinstance(options, dict):
        diagnostics.append({"code": "invalidOptionsType", "severity": "error"})
    else:
        for key in options:
            if key not in {"name", "replaceExisting", "previewOnly"}:
                diagnostics.append({"code": "unknownOptionsField", "severity": "error"})
    return diagnostics


def select_glyphs(glyphs: list[dict[str, Any]], selection: dict[str, Any],
                  fixture: dict[str, Any]) -> list[dict[str, Any]]:
    """Select rendered glyph records while keeping unit semantics explicit."""
    unit = selection.get("unit")
    if unit == "regex":
        try:
            pattern = selection.get("pattern", "")
            matched_indices = set()
            for match in re.finditer(pattern, fixture["text"]):
                for glyph in glyphs:
                    if glyph["index"] < match.end() and glyph["index"] + 1 > match.start():
                        matched_indices.add(glyph["index"])
            matched_clusters = {g["cluster"] for g in glyphs if g["index"] in matched_indices}
            return [g for g in glyphs if g["cluster"] in matched_clusters]
        except re.error:
            return []
    if unit == "line":
        return [g for g in glyphs if g["line"] == selection.get("value")]
    if unit == "word":
        return [g for g in glyphs if not g["char"].isspace()]
    if unit == "grapheme":
        # A grapheme is one selection unit, but every shaped/rendered glyph
        # belonging to that cluster receives the same selection weight.
        return list(glyphs)
    if unit == "paragraph":
        return glyphs if fixture["text"] else []
    if unit == "tag":
        tags = fixture.get("tags", {})
        selected_indices = set(tags.get(str(selection.get("value")), []))
        return [g for g in glyphs if g["index"] in selected_indices]
    return glyphs


def preview_intent(intent: dict[str, Any], fixture: dict[str, Any]) -> dict[str, Any]:
    glyphs = virtual_glyphs(fixture["text"])
    diagnostics = validate_intent(intent, fixture)
    intent_object = intent if isinstance(intent, dict) else {}
    selection = intent_object.get("selection", {})
    unit = selection.get("unit") if isinstance(selection, dict) else None
    selected = select_glyphs(glyphs, selection, fixture)
    if not selected and unit in {"regex", "word", "line", "tag", "grapheme", "paragraph"}:
        diagnostics.append({"code": "selectionEmpty", "severity": "warning"})
    selected_units = (len({g["cluster"] for g in selected}) if unit == "grapheme"
                      else len(selected))
    operators = intent_object.get("operators", [])
    if not isinstance(operators, list):
        operators = []
    status = "error" if any(d["severity"] == "error" for d in diagnostics) else (
        "warning" if diagnostics else "pass")
    return {
        "fixture": fixture["id"],
        "target": intent_object.get("target"),
        "selection": intent_object.get("selection", {}),
        "selectedGlyphCount": len(selected),
        "selectedUnitCount": selected_units,
        "operatorCount": len(operators),
        "operators": [operator.get("type") if isinstance(operator, dict) else None
                      for operator in operators],
        "timing": intent_object.get("timing", {}),
        "diagnostics": diagnostics,
        "status": status,
    }


def word_identity_diff(before: str, after: str) -> dict[str, Any]:
    """Match word occurrences conservatively for content-aware animation.

    Duplicate words are only auto-matched by occurrence order. The report
    marks duplicates as ambiguous so a product adapter can require preview
    confirmation instead of silently moving animation state.
    """
    before_words = before.split()
    after_words = after.split()
    before_positions: dict[str, list[int]] = defaultdict(list)
    after_positions: dict[str, list[int]] = defaultdict(list)
    for index, word in enumerate(before_words):
        before_positions[word].append(index)
    for index, word in enumerate(after_words):
        after_positions[word].append(index)
    matches = []
    ambiguous = []
    for word, positions in before_positions.items():
        if word not in after_positions:
            continue
        if len(positions) > 1 or len(after_positions[word]) > 1:
            ambiguous.append(word)
        for old, new in zip(positions, after_positions[word]):
            matches.append({"word": word, "beforeIndex": old, "afterIndex": new})
    removed = [word for word in before_words if word not in after_positions]
    added = [word for word in after_words if word not in before_positions]
    return {"matches": matches, "added": added, "removed": removed,
            "ambiguous": sorted(set(ambiguous)),
            "status": "warning" if ambiguous else "pass"}


def preserve_layout(positions: list[float], widths: list[float], box_width: float,
                    min_gap: float = 0.0) -> dict[str, Any]:
    """Apply a deterministic 1D layout constraint to proposed glyph positions."""
    corrected = list(positions)
    corrections = []
    if (len(corrected) != len(widths) or
            not is_json_number(box_width) or not math.isfinite(box_width) or
            not is_json_number(min_gap) or not math.isfinite(min_gap) or
            box_width < 0 or min_gap < 0):
        return {"status": "error", "code": "invalidLayoutConstraint"}
    for index, width in enumerate(widths):
        if width < 0 or not math.isfinite(width):
            return {"status": "error", "code": "invalidGlyphWidth"}
        if (not is_json_number(corrected[index]) or
                not math.isfinite(corrected[index])):
            return {"status": "error", "code": "invalidGlyphPosition"}
        left_limit = 0.0 if index == 0 else corrected[index - 1] + widths[index - 1] + min_gap
        next_limit = box_width - width
        target = min(max(corrected[index], left_limit), next_limit)
        if target != corrected[index]:
            corrections.append({"index": index, "from": corrected[index], "to": target})
            corrected[index] = target
    overflow = any(position < 0.0 or position + width > box_width
                   for position, width in zip(corrected, widths))
    return {"status": "warning" if corrections or overflow else "pass",
            "positions": corrected, "corrections": corrections,
            "overflow": overflow}


def timeline_progress(count: int, duration: float, stagger: float, time: float,
                      easing: str = "smooth") -> list[float]:
    if count < 0 or duration < 0 or stagger < 0 or not all(
            math.isfinite(value) for value in (duration, stagger, time)):
        raise ValueError("invalid timeline")
    result = []
    for index in range(count):
        local = (time - index * stagger) / duration if duration > 0 else (1.0 if time >= index * stagger else 0.0)
        local = max(0.0, min(1.0, local))
        if easing == "smooth":
            local = 0.5 - 0.5 * math.cos(local * math.pi)
        elif easing == "sharp":
            local = local * local
        elif easing == "spring":
            # Deliberate, deterministic overshoot. The caller may clamp the
            # final property (e.g. opacity), but progress itself stays
            # explainable and is not silently treated as linear.
            local = 1.0 - math.exp(-6.0 * local) * math.cos(10.0 * local)
        elif easing not in {"linear", "custom"}:
            raise ValueError("unknown easing")
        result.append(local)
    return result


def apply_timed_property(start: float, end: float, progress: float,
                         property_type: str) -> float:
    if not all(is_json_number(value) and math.isfinite(value)
               for value in (start, end, progress)):
        raise ValueError("non-finite timed property")
    value = start + (end - start) * progress
    if property_type == "opacity":
        return max(0.0, min(1.0, value))
    if property_type == "scale":
        return max(0.0, value)
    if property_type in {"position", "rotation", "skew", "tracking"}:
        return value
    raise ValueError("unsupported timed property")


def compare_core_snapshot(fixture: dict[str, Any], snapshot: dict[str, Any]) -> dict[str, Any]:
    """Compare a future ArtifactCore adapter snapshot with the design model."""
    if not isinstance(snapshot, dict):
        return {"fixture": fixture["id"], "status": "error",
                "differences": [{"code": "invalidCoreSnapshot",
                                 "field": "snapshot"}]}
    design = audit_fixture(fixture)
    differences = []
    for key in ("virtualGlyphCount", "virtualClusterCount", "virtualLineCount"):
        core_key = key.removeprefix("virtual")
        if core_key not in snapshot:
            differences.append({"code": "missingCoreField", "field": core_key})
        elif snapshot[core_key] != design[key]:
            differences.append({"code": "structureMismatch", "field": core_key,
                                "design": design[key], "core": snapshot[core_key]})
    states = snapshot.get("states", [])
    if not isinstance(states, list):
        differences.append({"code": "invalidCoreStates", "field": "states"})
        states = []
    glyph_count = design["virtualGlyphCount"]
    seen_indices = set()
    for state in states:
        if not isinstance(state, dict):
            differences.append({"code": "invalidCoreState", "field": "states"})
            continue
        index = state.get("index")
        if type(index) is not int or index < 0 or index >= glyph_count:
            differences.append({"code": "coreIndexOutOfRange", "index": index})
        elif index in seen_indices:
            differences.append({"code": "duplicateCoreIndex", "index": index})
        else:
            seen_indices.add(index)
        for field in ("position", "scale", "rotation", "opacity", "skew", "tracking",
                      "z", "blur", "strokeWidth"):
            value = state.get(field)
            if field == "position" and isinstance(value, list):
                valid = len(value) == 2 and all(
                    is_json_number(component) and math.isfinite(component)
                    for component in value)
            elif field == "position" and value is None:
                # Partial snapshots may omit fields that are not under test.
                continue
            else:
                valid = value is None or (
                    is_json_number(value) and math.isfinite(value))
            if not valid:
                differences.append({"code": "coreNonFinite", "field": field,
                                    "index": index})
        if is_json_number(state.get("opacity")) and not 0.0 <= state["opacity"] <= 1.0:
            differences.append({"code": "coreOpacityOutOfRange", "index": index})
        if is_json_number(state.get("scale")) and state["scale"] < 0.0:
            differences.append({"code": "coreScaleOutOfRange", "index": index})
        for color_field in ("fillColor", "strokeColor"):
            color = state.get(color_field)
            if color is None:
                continue
            valid_color = (isinstance(color, list) and len(color) == 4 and
                           all(is_json_number(component) and math.isfinite(component)
                               and 0.0 <= component <= 1.0 for component in color))
            if not valid_color:
                differences.append({"code": "coreColorOutOfRange",
                                    "field": color_field, "index": index})
    if "InstanceCount" in snapshot:
        if type(snapshot["InstanceCount"]) is not int:
            differences.append({"code": "invalidInstanceCount", "field": "InstanceCount"})
        elif snapshot["InstanceCount"] != glyph_count:
            differences.append({"code": "instanceCountMismatch", "design": glyph_count,
                                "core": snapshot["InstanceCount"]})
    if "StateCount" in snapshot:
        if type(snapshot["StateCount"]) is not int:
            differences.append({"code": "invalidStateCount", "field": "StateCount"})
        elif snapshot["StateCount"] != len(states):
            differences.append({"code": "stateCountMismatch", "design": len(states),
                                "core": snapshot["StateCount"]})
    return {"fixture": fixture["id"], "status": "error" if differences else "pass",
            "differences": differences}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tier", choices=("smoke", "contract", "stress"))
    parser.add_argument("--json-out", type=Path)
    parser.add_argument("--intent", type=Path,
                        help="validate and preview one Intent JSON against the first matching fixture")
    parser.add_argument("--intent-index", type=int, default=None)
    parser.add_argument("--fixture", default="text_sample1")
    parser.add_argument("--word-diff", nargs=2, metavar=("BEFORE", "AFTER"))
    parser.add_argument("--layout-check", nargs=2, metavar=("POSITIONS", "WIDTHS"))
    parser.add_argument("--box-width", type=float, default=100.0)
    parser.add_argument("--core-snapshot", type=Path,
                        help="compare an ArtifactCore adapter snapshot JSON")
    parser.add_argument("--timeline", nargs=4, metavar=("COUNT", "DURATION", "STAGGER", "TIME"))
    parser.add_argument("--easing", default="smooth")
    parser.add_argument("--rotation-demo", action="store_true",
                        help="preview natural, word, and center-out rotation for a fixture")
    args = parser.parse_args()
    all_fixtures = load_fixtures()
    if args.rotation_demo:
        fixture = next((f for f in all_fixtures if f["id"] == args.fixture), None)
        if fixture is None:
            print(json.dumps({"status": "error", "code": "fixtureNotFound"}, indent=2))
            return 1
        print(json.dumps(rotation_demo(fixture), ensure_ascii=False, indent=2))
        return 0
    if args.word_diff:
        print(json.dumps(word_identity_diff(args.word_diff[0], args.word_diff[1]),
                         ensure_ascii=False, indent=2))
        return 0
    if args.layout_check:
        try:
            positions = json.loads(args.layout_check[0])
            widths = json.loads(args.layout_check[1])
            if not isinstance(positions, list) or not isinstance(widths, list):
                raise ValueError("layout arrays required")
            result = preserve_layout(positions, widths, args.box_width)
        except (json.JSONDecodeError, TypeError, ValueError) as error:
            print(json.dumps({"status": "error", "code": str(error)}, indent=2))
            return 1
        print(json.dumps(result, indent=2))
        return 1 if result.get("status") == "error" else 0
    if args.core_snapshot:
        fixture = next((f for f in all_fixtures if f["id"] == args.fixture), None)
        if fixture is None:
            print(json.dumps({"status": "error", "code": "fixtureNotFound"}, indent=2))
            return 1
        try:
            snapshot = json.loads(args.core_snapshot.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            print(json.dumps({"status": "error", "code": "invalidJson"}, indent=2))
            return 1
        result = compare_core_snapshot(fixture, snapshot)
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 1 if result["status"] == "error" else 0
    if args.timeline:
        try:
            values = timeline_progress(int(args.timeline[0]), float(args.timeline[1]),
                                       float(args.timeline[2]), float(args.timeline[3]),
                                       args.easing)
            print(json.dumps({"progress": values}, indent=2))
            return 0
        except ValueError as error:
            print(json.dumps({"status": "error", "code": str(error)}, indent=2))
            return 1
    if args.intent:
        fixture = next((f for f in all_fixtures if f["id"] == args.fixture), None)
        if fixture is None:
            print(json.dumps({"status": "error", "code": "fixtureNotFound"}, indent=2))
            return 1
        try:
            loaded_intent = json.loads(args.intent.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            print(json.dumps({"status": "error", "code": "invalidJson"}, indent=2))
            return 1
        if isinstance(loaded_intent, list):
            if not loaded_intent:
                print(json.dumps({"status": "error", "code": "emptyIntentList"}, indent=2))
                return 1
            index = 0 if args.intent_index is None else args.intent_index
            if index < 0 or index >= len(loaded_intent):
                print(json.dumps({"status": "error", "code": "intentIndexOutOfRange"}, indent=2))
                return 1
            intent = loaded_intent[index]
            if isinstance(intent, dict) and isinstance(intent.get("intent"), dict):
                intent = intent["intent"]
        else:
            intent = loaded_intent
        report = {"model": "design-reference-v1", "mode": "preview",
                  "intentIndex": args.intent_index if isinstance(loaded_intent, list) else None,
                  "result": preview_intent(intent, fixture)}
        encoded = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
        if args.json_out:
            args.json_out.parent.mkdir(parents=True, exist_ok=True)
            args.json_out.write_text(encoded, encoding="utf-8")
        else:
            print(encoded, end="")
        return 1 if report["result"]["status"] == "error" else 0

    fixtures = [f for f in all_fixtures if not args.tier or f["tier"] == args.tier]
    report = {"model": "design-reference-v1", "fixtureCount": len(fixtures),
              "results": [audit_fixture(f) for f in fixtures]}
    report["status"] = "error" if any(r["status"] == "error" for r in report["results"]) else "pass"
    encoded = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(encoded, encoding="utf-8")
    else:
        print(encoded, end="")
    return 1 if report["status"] == "error" else 0


if __name__ == "__main__":
    raise SystemExit(main())
