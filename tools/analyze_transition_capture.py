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
import os
import re
import statistics
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
    input_figures = [
        ("input_A.png", "Input A"), ("input_B.png", "Input B"),
    ]
    for relative, title in (("alpha_input_A.png", "Alpha probe A"),
                            ("alpha_input_B.png", "Alpha probe B")):
        if relative in result["images"]:
            input_figures.append((relative, title))
    sections = [
        '<section><h2>Inputs</h2><div class="images">' + "".join(
            f'<figure><a href="{escape(relative)}"><img src="{escape(relative)}" '
            f'alt="{escape(title)}"></a><figcaption>{escape(title)}</figcaption></figure>'
            for relative, title in input_figures
        ) + '</div></section>',
        '<section><h2>All transitions by progress</h2><div class="images">'
        + "".join(
            f'<figure><a href="all_p{math.floor(progress * 100.0 + 0.5):03d}.png">'
            f'<img src="all_p{math.floor(progress * 100.0 + 0.5):03d}.png" '
            f'alt="All transitions at progress {progress:g}"></a>'
            f'<figcaption>Progress {progress:g}</figcaption></figure>'
            for progress in result["progress_steps"]
        ) + '</div></section>',
    ]
    comparison = result.get("comparison")
    if comparison:
        baseline_link = comparison.get("baseline_review_href")
        changed = comparison.get("changed_images", [])
        added = comparison.get("added_images", [])
        removed = comparison.get("removed_images", [])
        links = []
        for item in changed[:30]:
            relative = item["path"]
            if relative in result["images"]:
                links.append(f'<li><a href="{escape(relative)}">{escape(relative)}</a></li>')
            else:
                links.append(f"<li>{escape(relative)}</li>")
        baseline_anchor = (
            f'<a href="{escape(baseline_link)}">Open baseline review</a>'
            if baseline_link else "Baseline review is on another volume; open its bundle separately."
        )
        sections.insert(0,
            '<section><h2>Baseline comparison</h2>'
            f'<p>Status: {"comparable settings" if comparison["conditions_match"] else "capture conditions differ"}; '
            f'{len(changed)} changed images, {len(added)} added, {len(removed)} removed. '
            f'Input fixtures {"match" if comparison["input_fixtures_match"] else "differ"}. '
            f'{baseline_anchor}</p>'
            f'{"<details><summary>Changed image files</summary><ul>" + "".join(links) + "</ul></details>" if links else ""}'
            '</section>'
        )
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
        alpha_sheet = f"transition_alpha_{name}_sheet.png"
        if alpha_sheet in result["images"]:
            figures.append(
                f'<figure><a href="{escape(alpha_sheet)}">'
                f'<img src="{escape(alpha_sheet)}" '
                f'alt="{escape(name)} alpha probe progress strip"></a>'
                f'<figcaption>Alpha 77 probe strip</figcaption></figure>'
            )
        for index, progress in enumerate(result["progress_steps"]):
            stem = f"{math.floor(progress * 100.0 + 0.5):03d}"
            relative = f"transition_{name}_p{stem}.png"
            if relative not in result["images"]:
                continue
            timing = (kind["timing_by_progress"][index]
                      if index < len(kind["timing_by_progress"]) else None)
            timing_caption = (f" · median {timing['median_ms']:.3f} ms"
                              if timing is not None else "")
            figures.append(
                f'<figure><a href="{escape(relative)}"><img src="{escape(relative)}" '
                f'alt="{escape(name)} at progress {progress:g}"></a>'
                f'<figcaption>p={progress:g}{timing_caption}</figcaption></figure>'
            )
            alpha_relative = f"transition_alpha_{name}_p{stem}.png"
            if alpha_relative in result["images"]:
                figures.append(
                    f'<figure><a href="{escape(alpha_relative)}"><img src="{escape(alpha_relative)}" '
                    f'alt="{escape(name)} alpha probe at progress {progress:g}"></a>'
                    f'<figcaption>alpha 77 · p={progress:g}</figcaption></figure>'
                )
        sections.append(
            f'<section><h2>{escape(name)} <span class="{check_class}">'
            f'{check_status}</span></h2><p>Direction: {escape(str(kind["direction"]))}; '
            f'changed pixels by step: {escape(json.dumps(kind["measured_changed_pixels"]))}</p>'
            f'<div class="review"><label>Visual review '
            f'<select class="review-status" data-kind="{escape(name, quote=True)}">'
            '<option value="not-reviewed">Not reviewed</option>'
            '<option value="looks-good">Looks good</option>'
            '<option value="needs-attention">Needs attention</option></select></label> '
            f'<label>Observations <textarea class="review-notes" '
            f'data-kind="{escape(name, quote=True)}" rows="2" '
            f'placeholder="Direction, edge quality, artifacts, temporal feel"></textarea></label></div>'
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
button,select,textarea{{font:inherit;color:inherit;background:#252a32;border:1px solid #656c78;border-radius:4px;padding:6px}}
.review{{display:grid;grid-template-columns:minmax(200px,300px) minmax(250px,1fr);gap:12px;margin:10px 0}}
.review label{{display:grid;gap:5px}} textarea{{resize:vertical}}
@media(max-width:700px){{.review{{grid-template-columns:1fr}}}}
</style></head><body>
<h1>Transition capture review</h1>
<div class="summary"><strong class="{status_class}">{status}</strong> · {result["captured_kind_count"]}/{len(EXPECTED_KINDS)} expected kinds · {len(result["images"])} images checked<br>
Source revision: {escape(str(result["source_revision"] or "not recorded"))} · Runtime: {escape(str(result["runtime_measurements"]))}<br>
Automatic checks verify capture integrity, exact endpoints, alpha, and per-step pixel changes. Review direction, edge quality, artifacts, frame boundaries, and temporal feel. Notes save in this browser; export them with the button.</div>
<p><button id="export-review" type="button">Export visual review JSON</button> <span id="save-state" role="status">No visual review exported yet.</span></p>
{"<ul>" + global_failures + "</ul>" if global_failures else ""}
{"".join(sections)}
<script>
(() => {{
  const storageKey = "transition-review-{result["capture_report"]["sha256"]}";
  const statusFields = [...document.querySelectorAll(".review-status")];
  const noteFields = [...document.querySelectorAll(".review-notes")];
  const status = document.getElementById("save-state");
  const read = () => {{
    try {{ return JSON.parse(localStorage.getItem(storageKey) || "{{}}"); }}
    catch (_) {{ return {{}}; }}
  }};
  const save = () => {{
    const reviews = {{}};
    for (const field of statusFields) reviews[field.dataset.kind] = {{status: field.value, notes: ""}};
    for (const field of noteFields) reviews[field.dataset.kind].notes = field.value;
    try {{ localStorage.setItem(storageKey, JSON.stringify(reviews)); status.textContent = "Saved in this browser."; }}
    catch (_) {{ status.textContent = "Browser storage unavailable; export your notes before closing."; }}
  }};
  const saved = read();
  for (const field of statusFields) if (saved[field.dataset.kind]) field.value = saved[field.dataset.kind].status;
  for (const field of noteFields) if (saved[field.dataset.kind]) field.value = saved[field.dataset.kind].notes;
  for (const field of [...statusFields, ...noteFields]) field.addEventListener("change", save);
  for (const field of noteFields) field.addEventListener("input", save);
  document.getElementById("export-review").addEventListener("click", () => {{
    const reviews = {{}};
    for (const field of statusFields) reviews[field.dataset.kind] = {{status: field.value, notes: ""}};
    for (const field of noteFields) reviews[field.dataset.kind].notes = field.value;
    const payload = {{schema_version: 1, capture_report_sha256: "{result["capture_report"]["sha256"]}",
                     exported_at: new Date().toISOString(), reviews}};
    const blob = new Blob([JSON.stringify(payload, null, 2) + "\\n"], {{type: "application/json"}});
    const link = document.createElement("a");
    link.href = URL.createObjectURL(blob); link.download = "transition_visual_review.json";
    link.click(); setTimeout(() => URL.revokeObjectURL(link.href), 1000);
    status.textContent = "Visual review exported.";
    try {{ localStorage.setItem(storageKey, JSON.stringify(reviews)); }} catch (_) {{}}
  }});
}})();
</script>
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

    run_manifest_path = directory / "run_manifest.json"
    run_manifest: dict[str, Any] | None = None
    if run_manifest_path.is_file():
        try:
            run_manifest = json.loads(run_manifest_path.read_text(encoding="utf-8"))
            if run_manifest.get("schema_version") != 1:
                failures.append("unsupported run manifest schema")
            return_code = run_manifest.get("test_process", {}).get("return_code")
            if return_code != 0:
                failures.append(f"transition test process returned {return_code!r}")
            source = run_manifest.get("source", {})
            binary = run_manifest.get("binary", {})
            if not source.get("revision"):
                warnings.append("source revision is unavailable in run manifest")
            if not binary.get("sha256"):
                warnings.append("test executable hash is unavailable in run manifest")
            if run_manifest.get("build_config") in (None, "unknown"):
                warnings.append("build configuration is unknown")
        except (OSError, json.JSONDecodeError, TypeError, AttributeError) as error:
            failures.append(f"invalid run manifest: {error}")
            run_manifest = None
    else:
        warnings.append("run provenance is unavailable; use run_transition_capture.py")
    timing_metadata = report.get("timing")
    timing_sample_count = (int(timing_metadata.get("sampleCount", 0))
                           if isinstance(timing_metadata, dict) else 0)
    if timing_sample_count <= 0:
        warnings.append("capture report has no raw timing contract")

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
    alpha_probe_count = sum(isinstance(entry.get("alphaProbe"), dict)
                            for entry in transitions)
    if alpha_probe_count == 0:
        if run_manifest is not None:
            failures.append("runner capture has no alpha probe lane; rebuild the test executable")
        else:
            warnings.append("capture has no semi-transparent alpha probe lane")
    else:
        if alpha_probe_count != len(transitions):
            failures.append("semi-transparent alpha probe lane is missing for some transitions")
        inspect_image("alpha_input_A.png", input_size, keep_pixels=True)
        inspect_image("alpha_input_B.png", input_size, keep_pixels=True)
        inspect_image("alpha_inputs.png", (max(1, width // 2) * 2,
                                            max(1, height // 2)))
        for input_name in ("alpha_input_A.png", "alpha_input_B.png"):
            pixels = pixel_data.get(input_name)
            if pixels is not None:
                unexpected = sum(1 for value in pixels[3::4] if value != 77)
                if unexpected:
                    failures.append(
                        f"{input_name} has {unexpected} pixels with alpha other than 77"
                    )
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
        nonopaque_pixels: list[int | None] = []
        timing_summaries: list[dict[str, Any] | None] = []
        alpha_probe = entry.get("alphaProbe")
        alpha_probe_changes: list[int | None] = []
        alpha_probe_bad_alpha: list[int | None] = []
        previous_pixels: bytes | None = None
        previous_alpha_pixels: bytes | None = None
        for index, item in enumerate(kind_steps):
            stats = item.get("stats", {})
            counts = [int(stats.get(key, -1)) for key in
                      ("leftPixels", "rightPixels", "mixedPixels")]
            if any(value < 0 for value in counts) or sum(counts) != width * height:
                kind_failures.append(f"step {index} source pixel counts do not sum to frame size")
            timing_values = item.get("timingMs")
            timing_summary: dict[str, Any] | None = None
            if timing_sample_count > 0:
                if not isinstance(timing_values, list) or len(timing_values) != timing_sample_count:
                    kind_failures.append(
                        f"step {index} timing sample count does not match metadata"
                    )
                else:
                    try:
                        samples = [float(value) for value in timing_values]
                    except (TypeError, ValueError):
                        samples = []
                    if not samples or any(not math.isfinite(value) or value < 0.0
                                          for value in samples):
                        kind_failures.append(f"step {index} has invalid raw timing samples")
                    else:
                        timing_summary = {
                            "raw_ms": samples,
                            "min_ms": min(samples),
                            "median_ms": statistics.median(samples),
                            "max_ms": max(samples),
                        }
            timing_summaries.append(timing_summary)
            stem = (f"{math.floor(steps[index] * 100.0 + 0.5):03d}"
                    if index < len(steps) else "invalid")
            frame_path = f"transition_{name}_p{stem}.png"
            inspect_image(frame_path, input_size, keep_pixels=True)
            frame_pixels = pixel_data.get(frame_path)
            alpha_errors = (sum(1 for value in frame_pixels[3::4] if value != 255)
                            if frame_pixels is not None else None)
            nonopaque_pixels.append(alpha_errors)
            if alpha_errors not in (None, 0):
                kind_failures.append(
                    f"step {index} has {alpha_errors} pixels with alpha other than 255"
                )
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
            previous_pixels = frame_pixels

        if isinstance(alpha_probe, dict):
            if int(alpha_probe.get("alphaValue", -1)) != 77:
                kind_failures.append("alpha probe does not declare the expected alpha value 77")
            alpha_steps = alpha_probe.get("steps", [])
            alpha_progress = [float(item.get("progress", -1.0)) for item in alpha_steps]
            if alpha_progress != steps:
                kind_failures.append("alpha probe progress sequence does not match suite steps")
            for index, item in enumerate(alpha_steps):
                stem = (f"{math.floor(steps[index] * 100.0 + 0.5):03d}"
                        if index < len(steps) else "invalid")
                frame_path = f"transition_alpha_{name}_p{stem}.png"
                inspect_image(frame_path, input_size, keep_pixels=True)
                frame_pixels = pixel_data.get(frame_path)
                unexpected_alpha = (
                    sum(1 for value in frame_pixels[3::4] if value != 77)
                    if frame_pixels is not None else None
                )
                alpha_probe_bad_alpha.append(unexpected_alpha)
                if unexpected_alpha not in (None, 0):
                    kind_failures.append(
                        f"alpha probe step {index} has {unexpected_alpha} pixels not equal to alpha 77"
                    )
                changed = (
                    changed_pixel_count(previous_alpha_pixels, frame_pixels, input_size)
                    if previous_alpha_pixels is not None and frame_pixels is not None else None
                )
                alpha_probe_changes.append(changed)
                if index > 0:
                    reported = int(item.get("changedFromPrevious", -1))
                    if changed is None or changed == 0:
                        kind_failures.append(
                            f"alpha probe step {index} did not change from the previous image"
                        )
                    elif reported != changed:
                        kind_failures.append(
                            f"alpha probe step {index} reports {reported} changed pixels, measured {changed}"
                        )
                if index == 0 and frame_pixels is not None and "alpha_input_A.png" in pixel_data:
                    endpoint_diff = changed_pixel_count(
                        frame_pixels, pixel_data["alpha_input_A.png"], input_size)
                    if endpoint_diff:
                        kind_failures.append(
                            f"alpha probe progress 0 differs from alpha input A at {endpoint_diff} pixels"
                        )
                if index == len(steps) - 1 and frame_pixels is not None and "alpha_input_B.png" in pixel_data:
                    endpoint_diff = changed_pixel_count(
                        frame_pixels, pixel_data["alpha_input_B.png"], input_size)
                    if endpoint_diff:
                        kind_failures.append(
                            f"alpha probe progress 1 differs from alpha input B at {endpoint_diff} pixels"
                        )
                previous_alpha_pixels = frame_pixels
            inspect_image(f"transition_alpha_{name}_sheet.png",
                          (max(1, width // 2) * len(steps), max(1, height // 2)))
        elif alpha_probe_count > 0:
            kind_failures.append("semi-transparent alpha probe lane is missing")

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
                             "nonopaque_pixel_count": nonopaque_pixels,
                             "alpha_probe_changed_pixels": alpha_probe_changes,
                             "alpha_probe_bad_alpha_pixel_count": alpha_probe_bad_alpha,
                             "timing_by_progress": timing_summaries,
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
        "run_manifest": {
            "path": "run_manifest.json",
            "sha256": sha256(run_manifest_path),
            "metadata": run_manifest,
        } if run_manifest is not None else None,
        "review_html": "review.html",
        "source_revision": (run_manifest or {}).get("source", {}).get("revision"),
        "execution_path": "ArtifactCore CPU contract renderer",
        "runtime_measurements": timing_metadata or "not present in source capture report",
        "visual_quality": "requires human review of emitted PNGs",
        "tool": {"python": sys.version.split()[0], "pillow": PILLOW_VERSION,
                 "analyzer_sha256": sha256(Path(__file__).resolve())},
        "frame": {"width": width, "height": height,
                  "frame_index": report.get("frameIndex"), "fps": report.get("fps")},
        "endpoint_pixel_checks": "exact RGBA comparison against input_A.png and input_B.png",
        "alpha_probe_checks": "alpha=77 inputs preserved exactly at endpoints and every progress sample",
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


def compare_captures(current: dict[str, Any], baseline_path: Path,
                     current_directory: Path) -> dict[str, Any]:
    report_path = baseline_path / "verification_report.json" if baseline_path.is_dir() else baseline_path
    baseline_directory = report_path.parent.resolve()
    baseline_raw = report_path.read_bytes()
    baseline = json.loads(baseline_raw.decode("utf-8"))
    if baseline.get("schema_version") != 1 or not isinstance(baseline.get("images"), dict):
        raise ValueError(f"unsupported baseline verification report: {report_path}")

    current_images = current["images"]
    baseline_images = baseline["images"]
    current_names, baseline_names = set(current_images), set(baseline_images)
    added = sorted(current_names - baseline_names)
    removed = sorted(baseline_names - current_names)
    changed = [
        {"path": name, "baseline_sha256": baseline_images[name].get("sha256"),
         "current_sha256": current_images[name].get("sha256")}
        for name in sorted(current_names & baseline_names)
        if current_images[name].get("sha256") != baseline_images[name].get("sha256")
    ]
    input_names = {"input_A.png", "input_B.png", "alpha_input_A.png", "alpha_input_B.png"}
    shared_inputs = input_names & current_names & baseline_names
    input_fixtures_match = bool(shared_inputs) and all(
        current_images[name].get("sha256") == baseline_images[name].get("sha256")
        for name in shared_inputs
    ) and (input_names & current_names) == (input_names & baseline_names)
    settings_match = (
        current.get("frame") == baseline.get("frame")
        and current.get("progress_steps") == baseline.get("progress_steps")
        and current.get("expected_kinds") == baseline.get("expected_kinds")
        and current.get("alpha_probe_checks") == baseline.get("alpha_probe_checks")
    )
    conditions_match = settings_match and input_fixtures_match and (
        current.get("execution_path") == baseline.get("execution_path")
    )
    current_run = (current.get("run_manifest") or {}).get("metadata") or {}
    baseline_run = (baseline.get("run_manifest") or {}).get("metadata") or {}
    current_source = current_run.get("source", {}).get("revision")
    baseline_source = baseline_run.get("source", {}).get("revision")
    current_binary = current_run.get("binary", {}).get("sha256")
    baseline_binary = baseline_run.get("binary", {}).get("sha256")
    current_host = current_run.get("host")
    baseline_host = baseline_run.get("host")
    current_build = current_run.get("build_config")
    baseline_build = baseline_run.get("build_config")
    build_config_match = bool(current_build) and current_build == baseline_build
    timing_contract_match = current.get("runtime_measurements") == baseline.get("runtime_measurements")
    try:
        baseline_review_path = baseline_directory / "review.html"
        baseline_review_href = (
            os.path.relpath(baseline_review_path, current_directory.resolve()).replace("\\", "/")
            if baseline_review_path.is_file() else None
        )
    except ValueError:
        baseline_review_href = None

    return {
        "baseline_report_path": str(report_path.resolve()),
        "baseline_report_sha256": hashlib.sha256(baseline_raw).hexdigest(),
        "baseline_review_href": baseline_review_href,
        "conditions_match": conditions_match,
        "settings_match": settings_match,
        "input_fixtures_match": input_fixtures_match,
        "execution_path_match": current.get("execution_path") == baseline.get("execution_path"),
        "source_revision_match": bool(current_source) and current_source == baseline_source,
        "binary_hash_match": bool(current_binary) and current_binary == baseline_binary,
        "host_match": bool(current_host) and current_host == baseline_host,
        "build_config_match": build_config_match,
        "timing_contract_match": timing_contract_match,
        "timing_conditions_match": settings_match and input_fixtures_match
        and bool(current_host) and current_host == baseline_host
        and build_config_match and timing_contract_match,
        "added_images": added,
        "removed_images": removed,
        "changed_images": changed,
        "images_identical": not added and not removed and not changed,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture_directory", type=Path,
                        help="directory written by TransitionRenderContractTest")
    parser.add_argument("--output", type=Path,
                        help="verification JSON path (default: <capture>/verification_report.json)")
    parser.add_argument("--baseline", type=Path,
                        help="previous capture directory or verification_report.json to compare")
    args = parser.parse_args()
    directory = args.capture_directory.resolve()
    output = args.output.resolve() if args.output else directory / "verification_report.json"
    try:
        result = check_capture(directory)
        if args.baseline:
            result["comparison"] = compare_captures(
                result, args.baseline.expanduser().resolve(), directory
            )
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as error:
        print(f"capture verification could not run: {error}", file=sys.stderr)
        return 2
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    if result.get("comparison"):
        (directory / "capture_delta.json").write_text(
            json.dumps(result["comparison"], indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
    review_path = write_review_html(directory, result)
    print(f"capture verification: {'PASS' if result['passed'] else 'FAIL'}")
    print(f"transitions: {result['captured_kind_count']}/{len(EXPECTED_KINDS)}")
    print(f"images checked: {len(result['images'])}")
    print(f"report: {output}")
    print(f"visual review: {review_path}")
    if result.get("comparison"):
        comparison = result["comparison"]
        print(f"baseline comparison: {len(comparison['changed_images'])} changed, "
              f"{len(comparison['added_images'])} added, "
              f"{len(comparison['removed_images'])} removed images")
    for failure in result["failures"]:
        print(f"FAIL: {failure}")
    for warning in result["warnings"]:
        print(f"WARN: {warning}")
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
