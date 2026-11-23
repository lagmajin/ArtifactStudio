"""Capture ordered multi-effect Glow recipes from a shared foundation."""

from __future__ import annotations

import argparse
import json
import time
from datetime import datetime, timezone
from pathlib import Path

from glow_suite_recipe_capture import DEFAULT_EXE, ROOT, capture, digest


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", type=Path, required=True)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    args = parser.parse_args()
    profile_path, input_path, exe = args.profile.resolve(), args.input.resolve(), args.exe.resolve()
    profile = json.loads(profile_path.read_text(encoding="utf-8"))
    args.output_dir.mkdir(parents=True, exist_ok=True)
    foundation = profile.get("foundation")
    if foundation is None:
        # A null foundation means start the specialist branch from the exact input
        # pixels. Do not use Glow gain 0 as an identity: the installed CPU effect
        # treats a nonpositive gain as an unscaled threshold mask.
        base = {
            "label": "identity-input",
            "status": "captured",
            "identity_input": True,
            "input": str(input_path.relative_to(ROOT)) if input_path.is_relative_to(ROOT)
                     else str(input_path),
            "input_sha256": digest(input_path),
            "effect_id": None,
            "properties": {},
        }
        foundation_path = input_path
    else:
        try:
            base = capture(exe, input_path, args.output_dir, "shared-foundation",
                           foundation["effect_id"], foundation["properties"])
            base["status"] = "captured"
            foundation_path = args.output_dir / base["product_output"]
        except Exception as error:
            base = {"label": "shared-foundation", "status": "failed", "error": str(error)}
            foundation_path = None

    recipes = []
    for recipe_name, stages in profile["recipes"].items():
        entry = {"recipe": recipe_name, "status": "captured", "foundation_output_sha256": None,
                 "stages": []}
        if foundation_path is None:
            entry.update(status="skipped", reason="shared foundation capture failed")
            recipes.append(entry)
            continue
        current_path = foundation_path
        entry["foundation_output_sha256"] = digest(foundation_path)
        for stage_index, stage in enumerate(stages, start=1):
            label = f"stack-{recipe_name}-stage-{stage_index:02d}"
            start_time = time.perf_counter()
            try:
                captured = capture(exe, current_path, args.output_dir, label,
                                   stage["effect_id"], stage["properties"])
            except Exception as error:
                entry["status"] = "failed"
                entry["error"] = str(error)
                break
            captured.update(stage_index=stage_index,
                            previous_stage_input_sha256=digest(current_path),
                            capture_elapsed_seconds=time.perf_counter() - start_time,
                            status="captured")
            entry["stages"].append(captured)
            current_path = args.output_dir / captured["product_output"]
        if entry["status"] == "captured" and len(entry["stages"]) != len(stages):
            entry["status"] = "failed"
        recipes.append(entry)

    payload = {
        "capture_class": "Ordered ArtifactUiTest installed-effect CPU output with 8-bit PNG handoff",
        "not_single_float_stack_or_final_viewer": True,
        "profile": str(profile_path.relative_to(ROOT)) if profile_path.is_relative_to(ROOT) else str(profile_path),
        "profile_sha256": digest(profile_path),
        "input": str(input_path.relative_to(ROOT)) if input_path.is_relative_to(ROOT) else str(input_path),
        "input_sha256": digest(input_path),
        "exe": str(exe.relative_to(ROOT)) if exe.is_relative_to(ROOT) else str(exe),
        "exe_sha256": digest(exe),
        "language": "ja",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "foundation": base,
        "recipes": recipes,
    }
    manifest_path = args.output_dir / "ordered-stack-manifest.json"
    manifest_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Saved ordered stack manifest: {manifest_path}")
    for recipe in recipes:
        print(f"{recipe['recipe']}: {recipe['status']} ({len(recipe['stages'])} stages)")


if __name__ == "__main__":
    main()
