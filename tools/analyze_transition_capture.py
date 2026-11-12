#!/usr/bin/env python3
"""Validate and summarize ArtifactCore transition PNG capture bundles.

Consumes the artifacts emitted by TransitionRenderContractTest and writes a
hash-addressed verification report. This checks capture integrity and basic
behaviour invariants; it does not replace visual review or measure runtime.
"""

from __future__ import annotations

import argparse
import hashlib
import html
import json
import math
import re
import sys
from pathlib import Path
from typing import Any

try:
    from PIL import Image, ImageChops, __version__ as PILLOW_VERSION
except ImportError as error:  # pragma: no cover - depends on caller environment
    raise SystemExit("Pillow is required: python -m pip install Pillow") from error


EXPECTED_KINDS = {
    "Crossfade", "Dissolve", "Wipe", "Slide", "Zoom", "GlitchDisplace",
    "Spin", "LinearWipe", "RadialWipe", "Flip", "Cube", "Doors",
    "LightLeak", "GradientWipe", "IrisWipe", "BlockDissolve",
}
SAFE_KIND = re.compile(r"^[A-Za-z0-9_-]+$")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_png(path: Path) -> tuple[tuple[int, int], str, str, bytes]:
    with Image.open(path) as image:
        image.load()
        return image.size, image.mode, str(image.format), image.tobytes()


def changed_pixel_count(left: bytes, right: bytes, size: tuple[int, int]) -> int:
    left_image = Image.frombytes("RGBA", size, left)
    right_image = Image.frombytes("RGBA", size, right)
    difference = ImageChops.difference(left_image, right_image)
    changed = ImageChops.lighter(
        ImageChops.lighter(difference.getchannel("R"), difference.getchannel("G")),
        ImageChops.lighter(difference.getchannel("B"), difference.getchannel("A")),
    )
    return sum(changed.histogram()[1:])


def write_review_html(directory: Path, result: dict[str, Any]) -> Path:
    escape = html.escape
    status = "PASS" if result["passed"] else "FAIL"
    status_class = "pass" if result["passed"] else "fail"
    sections = [
        '<section><h2>Inputs</h2><div class="images">'
        '<figure><a href="input_A.png"><img src="input_A.png" alt="Input A"></a>'
        '<figcaption>Input A</figcaption></figure>'
        '<figure><a href="input_B.png"><img src="input_B.png" alt="Input B"></a>'
        '<figcaption>Input B</figcaption></figure></div></section>',
        '<section><h2>All transitions by progress</h2><div class="images">'
        + "".join(
            f'<figure><a href="all_p{math.floor(progress * 100.0 + 0.5):03d}.png">'
            f'<img src="all_p{math.floor(progress * 100.0 + 0.5):03d}.png" '
            f'alt="All transitions at progress {progress:g}"></a>'
            f'<figcaption>Progress {progress:g}</figcaption></figure>'
            for progress in result["progress_steps"]
        ) + '</div></section>',
    ]
    for kind in result["kinds"]:
        name = str(kind["name"])
        if not SAFE_KIND.fullmatch(name):
            continue
        check_status = "PASS" if kind["checks_passed"] else "FAIL"
        check_class = "pass" if kind["checks_passed"] else "fail"
        failures = "".join(f"<li>{escape(message)}</li>" for message in kind["failures"])
        figures = [
            f'<figure><a href="transition_{escape(name)}_sheet.png">'
            f'<img src="transition_{escape(name)}_sheet.png" '
            f'alt="{escape(name)} progress strip"></a><figcaption>Progress strip</figcaption></figure>'
        ]
        for progress in result["progress_steps"]:
            stem = f"{math.floor(progress * 100.0 + 0.5):03d}"
            relative = f"transition_{name}_p{stem}.png"
            if relative not in result["images"]:
                continue
            figures.append(
                f'<figure><a href="{escape(relative)}"><img src="{escape(relative)}" '
                f'alt="{escape(name)} at progress {progress:g}"></a>'
                f'<figcaption>p={progress:g}</figcaption></figure>'
            )
        sections.append(
            f'<section><h2>{escape(name)} <span class="{check_class}">'
            f'{check_status}</span></h2><p>Direction: {escape(str(kind["direction"]))}; '
            f'changed pixels by step: {escape(json.dumps(kind["measured_changed_pixels"]))}</p>'
            f'{"<ul>" + failures + "</ul>" if failures else ""}'
            f'<div class="images">{"".join(figures)}</div></section>'
        )
    global_failures = "".join(f"<li>{escape(message)}</li>" for message in result["failures"])
    page = f'''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Transition capture review</title>
<style>
body{{font:15px/1.45 system-ui,sans-serif;margin:24px;background:#17191d;color:#e7e9ed}}
h1,h2{{line-height:1.2}} section{{border-top:1px solid #454952;padding:16px 0}}
.images{{display:flex;flex-wrap:wrap;gap:12px;align-items:flex-start}}
figure{{margin:0;width:min(320px,100%)}} img{{display:block;width:100%;height:auto;background:#242830;border:1px solid #555b66}}
figcaption{{color:#b9bec8;margin-top:4px}} .pass{{color:#72d69a}} .fail{{color:#ff7777}}
.summary{{padding:12px;border:1px solid #555b66;background:#22252b}}
</style></head><body>
<h1>Transition capture review</h1>
<div class="summary"><strong class="{status_class}">{status}</strong> · {result["captured_kind_count"]}/{len(EXPECTED_KINDS)} expected kinds · {len(result["images"])} images checked<br>
Source revision: {escape(str(result["source_revision"] or "not recorded"))} · Runtime: {escape(str(result["runtime_measurements"]))}<br>
Automatic checks verify capture integrity, exact endpoints, and per-step pixel changes. Review images visually for direction, geometry, artifacts, and temporal feel.</div>
{"<ul>" + global_failures + "</ul>" if global_failures else ""}
{"".join(sections)}
</body></html>
'''
    path = directory / "review.html"
    path.write_text(page, encoding="utf-8")
    return path


def check_capture(directory: Path) -> dict[str, Any]:
    report_path = directory / "transition_report.json"
    if not report_path.is_file():
        raise ValueError(f"missing capture report: {report_path}")
    raw = report_path.read_bytes()
    report = json.loads(raw.decode("utf-8"))
    if report.get("schemaVersion") != 1:
        raise ValueError(f"unsupported report schema: {report.get('schemaVersion')!r}")

    width, height = int(report["width"]), int(report["height"])
    steps = [float(value) for value in report["steps"]]
    if width <= 0 or height <= 0 or len(steps) < 3:
        raise ValueError("report must contain positive dimensions and at least 3 steps")
    if steps != sorted(set(steps)) or steps[0] != 0.0 or steps[-1] != 1.0:
        raise ValueError("progress steps must be unique, increasing, and include 0 and 1")

    transitions = report.get("transitions")
    if not isinstance(transitions, list):
        raise ValueError("report.transitions must be an array")
    names = [str(entry.get("name", "")) for entry in transitions]
    if len(names) != len(set(names)):
        raise ValueError("duplicate transition names in report")

    missing_kinds = sorted(EXPECTED_KINDS - set(names))
    unexpected_kinds = sorted(set(names) - EXPECTED_KINDS)
    failures: list[str] = []
    warnings: list[str] = []
    if missing_kinds:
        failures.append("missing transition kinds: " + ", ".join(missing_kinds))
    if unexpected_kinds:
        warnings.append("unrecognized transition kinds: " + ", ".join(unexpected_kinds))

    images: dict[str, dict[str, Any]] = {}
    pixel_data: dict[str, bytes] = {}

    def inspect_image(relative: str, expected_size: tuple[int, int] | None = None,
                      keep_pixels: bool = False) -> bool:
        path = directory / relative
        if not path.is_file():
            failures.append(f"missing image: {relative}")
            return False
        try:
            size, mode, image_format, pixels = read_png(path)
        except Exception as error:  # malformed or unreadable image
            failures.append(f"unreadable image {relative}: {error}")
            return False
        if image_format != "PNG" or mode != "RGBA":
            failures.append(
                f"unexpected format for {relative}: {image_format}/{mode}, expected PNG/RGBA"
            )
            return False
        if expected_size is not None and size != expected_size:
            failures.append(f"wrong dimensions for {relative}: {size}, expected {expected_size}")
            return False
        images[relative] = {"sha256": sha256(path), "width": size[0],
                            "height": size[1], "format": image_format, "mode": mode}
        if keep_pixels:
            pixel_data[relative] = pixels
        return True

    input_size = (width, height)
    inspect_image("input_A.png", input_size, keep_pixels=True)
    inspect_image("input_B.png", input_size, keep_pixels=True)
    inspect_image("inputs.png", (max(1, width // 2) * 2,
                                  max(1, height // 2)))
    kind_results: list[dict[str, Any]] = []
    for entry in transitions:
        name = str(entry.get("name", ""))
        if not SAFE_KIND.fullmatch(name):
            failures.append(f"unsafe transition name in report: {name!r}")
            continue

        kind_failures: list[str] = []
        kind_steps = entry.get("steps", [])
        observed_progress = [float(item.get("progress", -1.0)) for item in kind_steps]
        if observed_progress != steps:
            kind_failures.append("reported progress sequence does not match suite steps")
        if entry.get("startSource") != "Left":
            kind_failures.append(f"start source is {entry.get('startSource')!r}, expected Left")
        if entry.get("endSource") != "Right":
            kind_failures.append(f"end source is {entry.get('endSource')!r}, expected Right")
        if entry.get("direction") != "A->B":
            kind_failures.append(f"direction is {entry.get('direction')!r}, expected A->B")

        changed_steps: list[int] = []
        actual_changed_pixels: list[int | None] = []
        previous_pixels: bytes | None = None
        for index, item in enumerate(kind_steps):
            stats = item.get("stats", {})
            counts = [int(stats.get(key, -1)) for key in
                      ("leftPixels", "rightPixels", "mixedPixels")]
            if any(value < 0 for value in counts) or sum(counts) != width * height:
                kind_failures.append(f"step {index} source pixel counts do not sum to frame size")
            stem = (f"{math.floor(steps[index] * 100.0 + 0.5):03d}"
                    if index < len(steps) else "invalid")
            frame_path = f"transition_{name}_p{stem}.png"
            inspect_image(frame_path, input_size, keep_pixels=True)
            frame_pixels = pixel_data.get(frame_path)
            changed = (changed_pixel_count(previous_pixels, frame_pixels, input_size)
                       if previous_pixels is not None and frame_pixels is not None else None)
            actual_changed_pixels.append(changed)
            if index > 0:
                reported_changed = int(item.get("changedFromPrevious", -1))
                if changed is None:
                    kind_failures.append(f"step {index} image pixels are unavailable for comparison")
                elif changed == 0:
                    kind_failures.append(f"step {index} did not change from the previous image")
                else:
                    changed_steps.append(index)
                if changed is not None and reported_changed != changed:
                    kind_failures.append(
                        f"step {index} reports {reported_changed} changed pixels, measured {changed}"
                    )
            if index == 0 and frame_pixels is not None and "input_A.png" in pixel_data:
                endpoint_diff = changed_pixel_count(frame_pixels, pixel_data["input_A.png"], input_size)
                if endpoint_diff != 0:
                    kind_failures.append(f"progress 0 differs from input A at {endpoint_diff} pixels")
            if index == len(steps) - 1 and frame_pixels is not None and "input_B.png" in pixel_data:
                endpoint_diff = changed_pixel_count(frame_pixels, pixel_data["input_B.png"], input_size)
                if endpoint_diff != 0:
                    kind_failures.append(f"progress 1 differs from input B at {endpoint_diff} pixels")
        if kind_failures:
            failures.extend(f"{name}: {message}" for message in kind_failures)

        sheet_size = (max(1, width // 2) * len(steps),
                      max(1, height // 2))
        inspect_image(f"transition_{name}_sheet.png", sheet_size)
        kind_results.append({"name": name, "start_source": entry.get("startSource"),
                             "end_source": entry.get("endSource"),
                             "direction": entry.get("direction"),
                             "changed_progress_indices": changed_steps,
                             "measured_changed_pixels": actual_changed_pixels,
                             "checks_passed": not kind_failures,
                             "failures": kind_failures})

    for progress in steps:
        stem = f"{math.floor(progress * 100.0 + 0.5):03d}"
        kind_count = len(transitions)
        columns = 4
        rows = (kind_count + columns - 1) // columns
        inspect_image(f"all_p{stem}.png", (max(1, width // 2) * columns,
                                            max(1, height // 2) * rows))

    result = {
        "schema_version": 1,
        "suite": "Transition Effects R&D",
        "capture_report": {"path": "transition_report.json",
                           "sha256": hashlib.sha256(raw).hexdigest()},
        "review_html": "review.html",
        "source_revision": None,
        "execution_path": "ArtifactCore CPU contract renderer",
        "runtime_measurements": "not present in source capture report",
        "visual_quality": "requires human review of emitted PNGs",
        "tool": {"python": sys.version.split()[0], "pillow": PILLOW_VERSION},
        "frame": {"width": width, "height": height,
                  "frame_index": report.get("frameIndex"), "fps": report.get("fps")},
        "endpoint_pixel_checks": "exact RGBA comparison against input_A.png and input_B.png",
        "progress_steps": steps,
        "expected_kinds": sorted(EXPECTED_KINDS),
        "captured_kind_count": len(names),
        "kinds": kind_results,
        "images": images,
        "warnings": warnings,
        "failures": failures,
        "passed": not failures,
    }
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture_directory", type=Path,
                        help="directory written by TransitionRenderContractTest")
    parser.add_argument("--output", type=Path,
                        help="verification JSON path (default: <capture>/verification_report.json)")
    args = parser.parse_args()
    directory = args.capture_directory.resolve()
    output = args.output.resolve() if args.output else directory / "verification_report.json"
    try:
        result = check_capture(directory)
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as error:
        print(f"capture verification could not run: {error}", file=sys.stderr)
        return 2
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    review_path = write_review_html(directory, result)
    print(f"capture verification: {'PASS' if result['passed'] else 'FAIL'}")
    print(f"transitions: {result['captured_kind_count']}/{len(EXPECTED_KINDS)}")
    print(f"images checked: {len(result['images'])}")
    print(f"report: {output}")
    print(f"visual review: {review_path}")
    for failure in result["failures"]:
        print(f"FAIL: {failure}")
    for warning in result["warnings"]:
        print(f"WARN: {warning}")
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
