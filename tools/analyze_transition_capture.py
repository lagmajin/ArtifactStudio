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
import shutil
import subprocess
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
EXPECTED_REVIEW_FIXTURES = {"fine_lines", "natural_texture", "transparent_edge"}
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
    for fixture in result.get("review_fixtures", []):
        input_figures = []
        for key, title in (("input_a", "Input A"), ("input_b", "Input B")):
            relative = fixture.get(key)
            if relative and relative in result["images"]:
                input_figures.append(f'<figure><a href="{escape(relative)}"><img src="{escape(relative)}" alt="{escape(title)}"></a><figcaption>{escape(title)}</figcaption></figure>')
        transition_sections = []
        for transition in fixture.get("transitions", []):
            kind_name = transition.get("kind", "unknown")
            figures = []
            for key, title in (("sheet", "Progress strip"), ("alpha_sheet", "Alpha 77 strip")):
                relative = transition.get(key)
                if relative and relative in result["images"]:
                    figures.append(f'<figure><a href="{escape(relative)}"><img src="{escape(relative)}" alt="{escape(kind_name)} {escape(title)}"></a><figcaption>{escape(title)}</figcaption></figure>')
            for lane in ("sequence", "alpha_sequence"):
                sequence = transition.get(lane)
                if not sequence or not sequence.get("frames"):
                    continue
                paths = [frame["path"] for frame in sequence["frames"]]
                lane_title = "Alpha=77 probe" if lane == "alpha_sequence" else ("Source alpha" if fixture.get("alpha_contract") != "opaque RGBA8" else "Opaque")
                figures.append(
                    f'<figure class="sequence-figure"><figcaption>{escape(lane_title)} · {len(paths)} lossless PNG frames</figcaption>'
                    f'<div class="sequence-player" data-kind="{escape(fixture["name"] + "-" + kind_name + "-" + lane, quote=True)}" data-fps="{sequence["fps"]}" data-frames="{escape(json.dumps(paths), quote=True)}">'
                    f'<img class="sequence-image" src="{escape(paths[0])}" alt="{escape(fixture["name"])} {escape(kind_name)} frame">'
                    f'<div><button type="button" class="sequence-play">Play</button> '
                    f'<button type="button" class="sequence-step" data-delta="-1">◀ Frame</button> '
                    f'<button type="button" class="sequence-step" data-delta="1">Frame ▶</button> '
                    f'<label>Frame <input class="sequence-index" type="number" min="0" max="{len(paths)-1}" value="0"></label> / {len(paths)-1} '
                    f'<span class="sequence-progress">p=0</span> · {sequence["fps"]} fps</div></div>'
                    + (f'<video controls loop muted playsinline src="{escape(transition.get(lane + "_video", ""))}"></video>' if transition.get(lane + "_video") else "")
                    + '</figure>')
            reviewer_fields = []
            for reviewer_index in range(2):
                attrs = (f'data-fixture="{escape(fixture["name"], quote=True)}" '
                         f'data-transition="{escape(kind_name, quote=True)}" data-reviewer="{reviewer_index}"')
                reviewer_fields.append(
                    f'<fieldset class="fixture-review"><legend>Independent reviewer {reviewer_index + 1}</legend>'
                    f'<label>Reviewer ID <input class="fixture-reviewer" {attrs} value="Reviewer {reviewer_index + 1}"></label> '
                    f'<label>Visual judgment <select class="fixture-status" {attrs}>'
                    '<option value="not-reviewed">Not reviewed</option>'
                    '<option value="looks-good">Looks good</option>'
                    '<option value="needs-attention">Needs attention</option></select></label> '
                    f'<label>Observations <textarea class="fixture-notes" {attrs} rows="2" '
                    'placeholder="Direction, temporal feel, boundary motion, artifacts, alpha fringe"></textarea></label>'
                    '</fieldset>')
            transition_sections.append(
                f'<details><summary>{escape(kind_name)} · 2 sequences · frame review</summary>'
                f'<div class="images">{"".join(figures)}</div><h4>Independent visual review</h4>'
                f'<div class="fixture-reviews">{"".join(reviewer_fields)}</div></details>')
        sections.append(f'<section><h2>Fixture: {escape(fixture["name"])}</h2><p>{escape(fixture.get("description", ""))} · {escape(fixture.get("alpha_contract", ""))}</p><p>SHA-256: A {escape(fixture.get("input_a_sha256", ""))} · B {escape(fixture.get("input_b_sha256", ""))}</p><div class="images">{"".join(input_figures)}</div><p>For each transition, watch the clip at normal speed, then use the frame controls to inspect boundaries and defects. Record judgments independently.</p>{"".join(transition_sections)}</section>')
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
            f'Temporal review matrix {"matches" if comparison.get("temporal_review_matrix_match") else "differs"}. '
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
        sequence = kind.get("temporal_sequence")
        if sequence and sequence.get("frames"):
            frame_paths = [item["path"] for item in sequence["frames"]]
            sequence_markup = (
                f'<div class="sequence-player" data-kind="{escape(name, quote=True)}" '
                f'data-fps="{sequence["fps"]}" '
                f'data-frames="{escape(json.dumps(frame_paths), quote=True)}">'
                f'<img class="sequence-image" src="{escape(frame_paths[0])}" alt="{escape(name)} sequence frame">'
                f'<div><button type="button" class="sequence-play">Play</button> '
                f'<button type="button" class="sequence-step" data-delta="-1">◀ Frame</button> '
                f'<button type="button" class="sequence-step" data-delta="1">Frame ▶</button> '
                f'<label>Frame <input class="sequence-index" type="number" min="0" '
                f'max="{len(frame_paths)-1}" value="0"></label> / {len(frame_paths)-1} '
                f'<span class="sequence-progress">p=0</span> · {sequence["fps"]} fps</div></div>'
            )
            video_path = kind.get("temporal_video")
            if video_path:
                sequence_markup += (f'<video controls loop muted playsinline '
                                    f'src="{escape(video_path)}"></video>')
            figures.insert(1, f'<figure class="sequence-figure"><figcaption>Continuous sequence · '
                            f'{sequence["frame_count"]} lossless PNG frames</figcaption>{sequence_markup}</figure>')
        alpha_sequence = kind.get("temporal_alpha_sequence")
        if alpha_sequence and alpha_sequence.get("frames"):
            paths = [item["path"] for item in alpha_sequence["frames"]]
            alpha_markup = (
                f'<div class="sequence-player" data-kind="{escape(name + "-alpha", quote=True)}" '
                f'data-fps="{alpha_sequence["fps"]}" '
                f'data-frames="{escape(json.dumps(paths), quote=True)}">'
                f'<img class="sequence-image" src="{escape(paths[0])}" alt="{escape(name)} alpha sequence frame">'
                f'<div><button type="button" class="sequence-play">Play</button> '
                f'<button type="button" class="sequence-step" data-delta="-1">◀ Frame</button> '
                f'<button type="button" class="sequence-step" data-delta="1">Frame ▶</button> '
                f'<label>Frame <input class="sequence-index" type="number" min="0" '
                f'max="{len(paths)-1}" value="0"></label> / {len(paths)-1} '
                f'<span class="sequence-progress">p=0</span> · alpha=77 · {alpha_sequence["fps"]} fps</div></div>'
            )
            if kind.get("temporal_alpha_video"):
                alpha_markup += (f'<video controls loop muted playsinline '
                                 f'src="{escape(kind["temporal_alpha_video"])}"></video>')
            figures.insert(2, f'<figure class="sequence-figure"><figcaption>Transparent input sequence · '
                            f'{len(paths)} lossless PNG frames</figcaption>{alpha_markup}</figure>')
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
input{{font:inherit;color:inherit;background:#252a32;border:1px solid #656c78;border-radius:4px;padding:5px;width:5em}}
.review{{display:grid;grid-template-columns:minmax(200px,300px) minmax(250px,1fr);gap:12px;margin:10px 0}}
.review label{{display:grid;gap:5px}} textarea{{resize:vertical}}
.fixture-reviews{{display:grid;grid-template-columns:repeat(2,minmax(240px,1fr));gap:12px}}
.fixture-review{{border:1px solid #454952;border-radius:5px;padding:10px;display:grid;gap:8px}}
.fixture-review label{{display:grid;gap:5px}} .fixture-review input{{width:auto}}
details{{margin:12px 0;border:1px solid #454952;border-radius:5px;padding:10px}}
summary{{cursor:pointer;font-weight:600;padding:4px}}
.sequence-figure{{width:min(760px,100%)}} .sequence-player img{{width:min(640px,100%);image-rendering:auto}}
.sequence-player video{{display:block;width:min(640px,100%);margin-top:8px;background:#000}}
.sequence-player>div{{display:flex;align-items:center;gap:6px;flex-wrap:wrap;margin-top:6px}}
@media(max-width:700px){{.review,.fixture-reviews{{grid-template-columns:1fr}}}}
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
  for (const player of document.querySelectorAll(".sequence-player")) {{
    const frames = JSON.parse(player.dataset.frames);
    const image = player.querySelector(".sequence-image");
    const indexField = player.querySelector(".sequence-index");
    const progress = player.querySelector(".sequence-progress");
    const play = player.querySelector(".sequence-play");
    let index = 0, timer = null;
    const show = (next) => {{
      index = Math.max(0, Math.min(frames.length - 1, next));
      image.src = frames[index]; indexField.value = index;
      progress.textContent = `p=${{(index / (frames.length - 1)).toFixed(3)}}`;
      if (index === frames.length - 1 && timer !== null) {{ clearInterval(timer); timer = null; play.textContent = "Play"; }}
    }};
    player.querySelectorAll(".sequence-step").forEach(button => button.addEventListener("click", () => show(index + Number(button.dataset.delta))));
    indexField.addEventListener("change", () => show(Number(indexField.value) || 0));
    play.addEventListener("click", () => {{
      if (timer !== null) {{ clearInterval(timer); timer = null; play.textContent = "Play"; return; }}
      if (index >= frames.length - 1) show(0);
      play.textContent = "Pause"; timer = setInterval(() => show(index + 1), 1000 / Number(player.dataset.fps));
    }});
  }}
  const storageKey = "transition-review-{result["capture_report"]["sha256"]}";
  const statusFields = [...document.querySelectorAll(".review-status")];
  const noteFields = [...document.querySelectorAll(".review-notes")];
  const fixtureReviewerFields = [...document.querySelectorAll(".fixture-reviewer")];
  const fixtureStatusFields = [...document.querySelectorAll(".fixture-status")];
  const fixtureNoteFields = [...document.querySelectorAll(".fixture-notes")];
  const status = document.getElementById("save-state");
  const read = () => {{
    try {{ return JSON.parse(localStorage.getItem(storageKey) || "{{}}"); }}
    catch (_) {{ return {{}}; }}
  }};
  const save = () => {{
    const reviews = {{}};
    for (const field of statusFields) reviews[field.dataset.kind] = {{status: field.value, notes: ""}};
    for (const field of noteFields) reviews[field.dataset.kind].notes = field.value;
    const fixtureReviews = {{}};
    for (const field of fixtureReviewerFields) {{
      const byTransition = fixtureReviews[field.dataset.fixture] ||= {{}};
      const reviewers = byTransition[field.dataset.transition] ||= [];
      reviewers[Number(field.dataset.reviewer)] ||= {{}};
      reviewers[Number(field.dataset.reviewer)].reviewer_id = field.value;
    }}
    for (const field of fixtureStatusFields) {{
      const reviewer = fixtureReviews[field.dataset.fixture][field.dataset.transition][Number(field.dataset.reviewer)];
      reviewer.status = field.value; reviewer.notes = "";
    }}
    for (const field of fixtureNoteFields) fixtureReviews[field.dataset.fixture][field.dataset.transition][Number(field.dataset.reviewer)].notes = field.value;
    const data = {{reviews, fixture_reviews: fixtureReviews}};
    try {{ localStorage.setItem(storageKey, JSON.stringify(data)); status.textContent = "Saved in this browser."; }}
    catch (_) {{ status.textContent = "Browser storage unavailable; export your notes before closing."; }}
  }};
  const saved = read();
  const savedReviews = saved.reviews || saved;
  for (const field of statusFields) if (savedReviews[field.dataset.kind]) field.value = savedReviews[field.dataset.kind].status;
  for (const field of noteFields) if (savedReviews[field.dataset.kind]) field.value = savedReviews[field.dataset.kind].notes;
  for (const field of fixtureReviewerFields) {{
    const reviewer = saved.fixture_reviews?.[field.dataset.fixture]?.[field.dataset.transition]?.[Number(field.dataset.reviewer)];
    if (reviewer?.reviewer_id) field.value = reviewer.reviewer_id;
  }}
  for (const field of fixtureStatusFields) {{
    const reviewer = saved.fixture_reviews?.[field.dataset.fixture]?.[field.dataset.transition]?.[Number(field.dataset.reviewer)];
    if (reviewer?.status) field.value = reviewer.status;
  }}
  for (const field of fixtureNoteFields) {{
    const reviewer = saved.fixture_reviews?.[field.dataset.fixture]?.[field.dataset.transition]?.[Number(field.dataset.reviewer)];
    if (reviewer?.notes) field.value = reviewer.notes;
  }}
  for (const field of [...statusFields, ...fixtureReviewerFields, ...fixtureStatusFields, ...fixtureNoteFields]) field.addEventListener("change", save);
  for (const field of [...noteFields, ...fixtureNoteFields]) field.addEventListener("input", save);
  document.getElementById("export-review").addEventListener("click", () => {{
    const reviews = {{}};
    for (const field of statusFields) reviews[field.dataset.kind] = {{status: field.value, notes: ""}};
    for (const field of noteFields) reviews[field.dataset.kind].notes = field.value;
    const fixtureReviews = {{}};
    for (const field of fixtureReviewerFields) {{
      const byTransition = fixtureReviews[field.dataset.fixture] ||= {{}};
      const reviewers = byTransition[field.dataset.transition] ||= [];
      reviewers[Number(field.dataset.reviewer)] ||= {{}};
      reviewers[Number(field.dataset.reviewer)].reviewer_id = field.value;
    }}
    for (const field of fixtureStatusFields) {{
      const reviewer = fixtureReviews[field.dataset.fixture][field.dataset.transition][Number(field.dataset.reviewer)];
      reviewer.status = field.value; reviewer.notes = "";
    }}
    for (const field of fixtureNoteFields) fixtureReviews[field.dataset.fixture][field.dataset.transition][Number(field.dataset.reviewer)].notes = field.value;
    const payload = {{schema_version: 1, capture_report_sha256: "{result["capture_report"]["sha256"]}",
                     exported_at: new Date().toISOString(), reviews, fixture_reviews: fixtureReviews}};
    const blob = new Blob([JSON.stringify(payload, null, 2) + "\\n"], {{type: "application/json"}});
    const link = document.createElement("a");
    link.href = URL.createObjectURL(blob); link.download = "transition_visual_review.json";
    link.click(); setTimeout(() => URL.revokeObjectURL(link.href), 1000);
    status.textContent = "Visual review exported.";
    try {{ localStorage.setItem(storageKey, JSON.stringify({{reviews, fixture_reviews: fixtureReviews}})); }} catch (_) {{}}
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

        temporal = entry.get("temporalSequence")
        temporal_summary = None
        temporal_alpha_summary = None
        if isinstance(temporal, dict):
            temporal_frames = temporal.get("frames", [])
            frame_count = int(temporal.get("frameCount", 0))
            temporal_fps = int(temporal.get("fps", 0))
            if frame_count != len(temporal_frames) or frame_count < 2 or temporal_fps <= 0:
                kind_failures.append("temporal sequence has invalid frame count or fps")
            frame_indices = [int(item.get("frameIndex", -1)) for item in temporal_frames]
            progresses = [float(item.get("progress", -1)) for item in temporal_frames]
            if frame_indices != list(range(frame_count)):
                kind_failures.append("temporal frame indices are missing or out of order")
            expected_progresses = [i / (frame_count - 1) for i in range(frame_count)] if frame_count > 1 else []
            if len(progresses) != len(expected_progresses) or any(
                    abs(actual - expected) > 1e-9
                    for actual, expected in zip(progresses, expected_progresses)):
                kind_failures.append("temporal progress mapping is inconsistent")
            sequence_image_records = []
            for item in temporal_frames:
                relative = str(item.get("path", ""))
                if not relative.startswith(f"sequences/{name}/") or ".." in Path(relative).parts:
                    kind_failures.append(f"unsafe temporal frame path: {relative}")
                    continue
                if inspect_image(relative, input_size):
                    sequence_image_records.append(images[relative])
            if len(sequence_image_records) != frame_count:
                kind_failures.append("temporal sequence image set is incomplete")
            temporal_summary = {"frame_count": frame_count, "fps": temporal_fps,
                                "duration_seconds": float(temporal.get("durationSeconds", 0)),
                                "frames": temporal_frames}
        elif run_manifest is not None:
            kind_failures.append("runner capture has no temporal sequence; rebuild the test executable")
        else:
            warnings.append(f"{name} has no temporal sequence")

        temporal_alpha = entry.get("temporalAlphaSequence")
        if isinstance(temporal_alpha, dict):
            alpha_frames = temporal_alpha.get("frames", [])
            alpha_count = int(temporal_alpha.get("frameCount", 0))
            alpha_fps = int(temporal_alpha.get("fps", 0))
            if int(temporal_alpha.get("alphaValue", -1)) != 77:
                kind_failures.append("temporal alpha lane does not declare alpha 77")
            if alpha_count != len(alpha_frames) or alpha_count < 2 or alpha_fps <= 0:
                kind_failures.append("temporal alpha lane has invalid frame count or fps")
            if alpha_frames and isinstance(temporal, dict) and alpha_frames[0].get("path"):
                previous_alpha: bytes | None = None
                for index, item in enumerate(alpha_frames):
                    relative = str(item.get("path", ""))
                    if not relative.startswith(f"sequences/{name}/") or ".." in Path(relative).parts:
                        kind_failures.append(f"unsafe temporal alpha frame path: {relative}")
                        continue
                    if not inspect_image(relative, input_size, keep_pixels=True):
                        continue
                    pixels = pixel_data.get(relative)
                    if pixels is None:
                        continue
                    if any(value != 77 for value in pixels[3::4]):
                        kind_failures.append(f"temporal alpha frame {index} does not preserve alpha 77")
                    if index == 0 and "alpha_input_A.png" in pixel_data and pixels != pixel_data["alpha_input_A.png"]:
                        kind_failures.append("temporal alpha first frame differs from alpha input A")
                    if index == alpha_count - 1 and "alpha_input_B.png" in pixel_data and pixels != pixel_data["alpha_input_B.png"]:
                        kind_failures.append("temporal alpha last frame differs from alpha input B")
                    if previous_alpha is not None:
                        measured = changed_pixel_count(previous_alpha, pixels, input_size)
                        if measured == 0 or measured != int(item.get("changedFromPrevious", -1)):
                            kind_failures.append(f"temporal alpha frame {index} has invalid adjacent pixel change count")
                    previous_alpha = pixels
            temporal_alpha_summary = {"frame_count": alpha_count, "fps": alpha_fps,
                                      "alpha_value": temporal_alpha.get("alphaValue"),
                                      "frames": alpha_frames}

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
                             "temporal_sequence": temporal_summary,
                             "temporal_alpha_sequence": temporal_alpha_summary,
                             "checks_passed": not kind_failures,
                             "failures": kind_failures})

    for progress in steps:
        stem = f"{math.floor(progress * 100.0 + 0.5):03d}"
        kind_count = len(transitions)
        columns = 4
        rows = (kind_count + columns - 1) // columns
        inspect_image(f"all_p{stem}.png", (max(1, width // 2) * columns,
                                            max(1, height // 2) * rows))

    review_fixtures: list[dict[str, Any]] = []
    seen_review_fixtures: set[str] = set()
    temporal_contract = report.get("temporalSequenceContract")
    expected_fixture_frame_count = (int(temporal_contract.get("frameCount", 0))
                                    if isinstance(temporal_contract, dict) else 0)
    expected_fixture_fps = (int(temporal_contract.get("fps", 0))
                            if isinstance(temporal_contract, dict) else 0)
    if expected_fixture_frame_count < 2 or expected_fixture_fps <= 0:
        failures.append("temporalSequenceContract must declare frameCount and fps for fixture review")
    for fixture in report.get("reviewFixtures", []):
        name = str(fixture.get("name", ""))
        if name not in EXPECTED_REVIEW_FIXTURES or name in seen_review_fixtures:
            failures.append(f"unexpected or duplicate review fixture: {name!r}")
        seen_review_fixtures.add(name)
        checked: dict[str, Any] = {"name": name, "description": fixture.get("description", ""),
                                   "alpha_contract": fixture.get("alphaContract", "")}
        for source_key, output_key in (("inputA", "input_a"), ("inputB", "input_b")):
            relative = fixture.get(source_key)
            if not isinstance(relative, str) or ".." in Path(relative).parts or not inspect_image(relative, input_size):
                failures.append(f"invalid review fixture image for {name}: {relative!r}")
                continue
            checked[output_key] = relative
            checked[output_key + "_sha256"] = images[relative]["sha256"]
        checked_transitions: list[dict[str, Any]] = []
        seen_fixture_kinds: set[str] = set()
        for capture in fixture.get("transitionCaptures", []):
            kind_name = str(capture.get("kind", ""))
            if kind_name not in EXPECTED_KINDS or kind_name in seen_fixture_kinds:
                failures.append(f"review fixture {name} has unexpected or duplicate transition kind: {kind_name!r}")
            seen_fixture_kinds.add(kind_name)
            checked_capture: dict[str, Any] = {"kind": kind_name}
            for source_key, output_key in (("sheet", "sheet"), ("alphaSheet", "alpha_sheet")):
                relative = capture.get(source_key)
                if not isinstance(relative, str) or ".." in Path(relative).parts or not inspect_image(relative, input_size):
                    failures.append(f"invalid review fixture {name} {kind_name} {output_key}: {relative!r}")
                    continue
                checked_capture[output_key] = relative
            for source_key, output_key, alpha_lane in (("sequence", "sequence", False),
                                                        ("alphaSequence", "alpha_sequence", True)):
                frames = capture.get(source_key, [])
                if len(frames) != int(capture.get("frameCount", 0)) or len(frames) < 2:
                    failures.append(f"review fixture {name} {kind_name} {output_key} has incomplete frame count")
                if len(frames) != expected_fixture_frame_count:
                    failures.append(f"review fixture {name} {kind_name} {output_key} does not match the declared frame count")
                if int(capture.get("fps", 0)) != expected_fixture_fps:
                    failures.append(f"review fixture {name} {kind_name} {output_key} has invalid fps")
                previous: bytes | None = None
                for index, frame in enumerate(frames):
                    relative = str(frame.get("path", ""))
                    relative_path = Path(relative)
                    expected_folder = Path("sequences") / f"review_{name}" / kind_name
                    expected_stem = "alpha_frame_" if alpha_lane else "frame_"
                    if (relative_path.parent != expected_folder or
                            not relative_path.name.startswith(expected_stem) or
                            ".." in relative_path.parts or
                            not inspect_image(relative, input_size, keep_pixels=True)):
                        failures.append(f"invalid review fixture sequence frame for {name} {kind_name}: {relative}")
                        continue
                    expected_progress = index / (len(frames) - 1) if len(frames) > 1 else 0.0
                    if int(frame.get("frameIndex", -1)) != index or not math.isclose(
                            float(frame.get("progress", -1.0)), expected_progress,
                            rel_tol=0.0, abs_tol=1e-9):
                        failures.append(f"review fixture {name} {kind_name} {output_key} frame {index} has invalid frame/progress mapping")
                    pixels = pixel_data.get(relative)
                    if pixels is None:
                        continue
                    if alpha_lane and any(value != 77 for value in pixels[3::4]):
                        failures.append(f"review fixture {name} {kind_name} alpha frame {index} changed alpha 77")
                    if index == 0:
                        reference = pixel_data.get(checked.get("input_a", ""))
                        endpoint_matches = reference is None or (
                            pixels[::4] == reference[::4] and pixels[1::4] == reference[1::4]
                            and pixels[2::4] == reference[2::4]
                            and (alpha_lane or pixels[3::4] == reference[3::4]))
                        if not endpoint_matches:
                            failures.append(f"review fixture {name} {kind_name} sequence start differs from input A")
                    if index == len(frames) - 1:
                        reference = pixel_data.get(checked.get("input_b", ""))
                        endpoint_matches = reference is None or (
                            pixels[::4] == reference[::4] and pixels[1::4] == reference[1::4]
                            and pixels[2::4] == reference[2::4]
                            and (alpha_lane or pixels[3::4] == reference[3::4]))
                        if not endpoint_matches:
                            failures.append(f"review fixture {name} {kind_name} sequence end differs from input B")
                    if previous is not None and changed_pixel_count(previous, pixels, input_size) != int(frame.get("changedFromPrevious", -1)):
                        failures.append(f"review fixture {name} {kind_name} sequence frame {index} changed-pixel count mismatch")
                    previous = pixels
                checked_capture[output_key] = {"fps": int(capture.get("fps", 0)), "frames": frames}
            for key, output_key in (("progressFrames", "progress"), ("alphaProgressFrames", "alpha_progress")):
                paths = capture.get(key, [])
                checked_capture[output_key] = paths
                for relative in paths:
                    if not isinstance(relative, str) or ".." in Path(relative).parts or not inspect_image(relative, input_size):
                        failures.append(f"invalid review fixture progress image for {name} {kind_name}: {relative!r}")
                    elif output_key == "alpha_progress":
                        _, _, _, pixels = read_png(directory / relative)
                        if any(value != 77 for value in pixels[3::4]):
                            failures.append(f"review fixture {name} {kind_name} alpha progress image does not preserve alpha 77")
            checked_transitions.append(checked_capture)
        if seen_fixture_kinds != EXPECTED_KINDS:
            failures.append(f"review fixture {name} transition set is incomplete: missing={sorted(EXPECTED_KINDS - seen_fixture_kinds)}")
        checked["transitions"] = checked_transitions
        review_fixtures.append(checked)
    if seen_review_fixtures != EXPECTED_REVIEW_FIXTURES:
        failures.append(f"review fixture set is incomplete: missing={sorted(EXPECTED_REVIEW_FIXTURES - seen_review_fixtures)}")

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
        "review_fixtures": review_fixtures,
        "images": images,
        "warnings": warnings,
        "failures": failures,
        "passed": not failures,
    }
    temporal_contract = report.get("temporalSequenceContract")
    if temporal_contract is not None and not isinstance(temporal_contract, dict):
        failures.append("temporalSequenceContract must be an object")
        result["passed"] = False
    result["temporal_sequence_contract"] = temporal_contract
    return result


def encode_temporal_videos(directory: Path, result: dict[str, Any]) -> None:
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        result["warnings"].append("ffmpeg unavailable; lossless source PNG sequences remain reviewable")
        result["video_encoding"] = {"available": False, "reason": "ffmpeg not found",
                                     "source_frames": "lossless PNG; pixel checks use these frames"}
        return
    encoded: list[dict[str, Any]] = []
    for kind in result["kinds"]:
        for lane, sequence_key, video_key, stem in (
                ("opaque", "temporal_sequence", "temporal_video", "frame_%04d.png"),
                ("alpha", "temporal_alpha_sequence", "temporal_alpha_video", "alpha_frame_%04d.png")):
            sequence = kind.get(sequence_key)
            if not sequence or not sequence.get("frames"):
                continue
            relative = f"sequences/{kind['name']}_{lane}.mkv"
            target = directory / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            first = Path(sequence["frames"][0]["path"])
            pattern = first.parent / stem
            command = [ffmpeg, "-hide_banner", "-loglevel", "error", "-y",
                       "-framerate", str(sequence["fps"]), "-start_number", "0",
                       "-i", str(directory / pattern), "-an", "-c:v", "ffv1",
                       "-level", "3", "-pix_fmt", "bgra", str(target)]
            completed = subprocess.run(command, capture_output=True, text=True, check=False)
            if completed.returncode:
                result["warnings"].append(f"{kind['name']} {lane} FFV1 encoding failed: {completed.stderr.strip()}")
                continue
            kind[video_key] = relative
            encoded.append({"path": relative, "sha256": sha256(target),
                            "codec": "FFV1", "container": "Matroska", "pixel_format": "bgra",
                            "fps": sequence["fps"], "frame_count": sequence["frame_count"],
                            "lane": lane})
    for fixture in result.get("review_fixtures", []):
        for transition in fixture.get("transitions", []):
            for lane in ("sequence", "alpha_sequence"):
                sequence = transition.get(lane)
                if not sequence or not sequence.get("frames"):
                    continue
                name = fixture["name"]
                kind_name = transition["kind"]
                lane_name = "alpha" if lane == "alpha_sequence" else "source"
                relative = f"sequences/review_{name}_{kind_name}_{lane_name}.mkv"
                target = directory / relative
                first = Path(sequence["frames"][0]["path"])
                pattern = first.parent / ("alpha_frame_%04d.png" if lane_name == "alpha" else "frame_%04d.png")
                command = [ffmpeg, "-hide_banner", "-loglevel", "error", "-y", "-framerate",
                           str(sequence["fps"]), "-start_number", "0", "-i", str(directory / pattern),
                           "-an", "-c:v", "ffv1", "-level", "3", "-pix_fmt", "bgra", str(target)]
                completed = subprocess.run(command, capture_output=True, text=True, check=False)
                if completed.returncode:
                    result["warnings"].append(f"review fixture {name} {kind_name} {lane_name} FFV1 encoding failed: {completed.stderr.strip()}")
                    continue
                transition[lane_name + "_video"] = relative
                encoded.append({"path": relative, "sha256": sha256(target), "codec": "FFV1",
                                "container": "Matroska", "pixel_format": "bgra", "fps": sequence["fps"],
                                "frame_count": len(sequence["frames"]), "lane": lane_name,
                                "fixture": name, "transition": kind_name})
    result["temporal_videos"] = encoded
    result["video_encoding"] = {
        "available": True, "encoder_path": str(Path(ffmpeg).resolve()),
        "codec": "FFV1", "container": "Matroska", "pixel_format": "bgra",
        "lossless": True,
        "display_conversion": "player display conversion is external; source PNG frames remain pixel-check authority",
    }


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
    current_fixture_inputs = {f"review_fixture:{item['name']}:A": item.get("input_a_sha256")
                              for item in current.get("review_fixtures", [])}
    current_fixture_inputs.update({f"review_fixture:{item['name']}:B": item.get("input_b_sha256")
                                   for item in current.get("review_fixtures", [])})
    baseline_fixture_inputs = {f"review_fixture:{item['name']}:A": item.get("input_a_sha256")
                               for item in baseline.get("review_fixtures", [])}
    baseline_fixture_inputs.update({f"review_fixture:{item['name']}:B": item.get("input_b_sha256")
                                    for item in baseline.get("review_fixtures", [])})
    fixture_sets_match = current_fixture_inputs == baseline_fixture_inputs and all(
        digest for digest in current_fixture_inputs.values())
    shared_inputs = input_names & current_names & baseline_names
    input_fixtures_match = bool(shared_inputs) and all(
        current_images[name].get("sha256") == baseline_images[name].get("sha256")
        for name in shared_inputs
    ) and (input_names & current_names) == (input_names & baseline_names) and fixture_sets_match
    def temporal_review_matrix(report: dict[str, Any]) -> dict[str, dict[str, tuple[int, int, int, int]]]:
        return {
            fixture["name"]: {
                transition["kind"]: (
                    transition.get("sequence", {}).get("fps", 0),
                    len(transition.get("sequence", {}).get("frames", [])),
                    transition.get("alpha_sequence", {}).get("fps", 0),
                    len(transition.get("alpha_sequence", {}).get("frames", [])),
                )
                for transition in fixture.get("transitions", [])
            }
            for fixture in report.get("review_fixtures", [])
        }
    temporal_review_match = temporal_review_matrix(current) == temporal_review_matrix(baseline)
    settings_match = (
        current.get("frame") == baseline.get("frame")
        and current.get("progress_steps") == baseline.get("progress_steps")
        and current.get("expected_kinds") == baseline.get("expected_kinds")
        and current.get("alpha_probe_checks") == baseline.get("alpha_probe_checks")
    )
    conditions_match = settings_match and input_fixtures_match and temporal_review_match and (
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
        "temporal_review_matrix_match": temporal_review_match,
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
        encode_temporal_videos(directory, result)
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
