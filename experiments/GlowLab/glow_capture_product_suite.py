"""Capture a profile set through the installed ArtifactUiTest effect path."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_EXE = ROOT / "out/build/x64-Debug-Agent/bin/Debug/ArtifactUiTest.exe"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", type=Path, required=True)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--prefix", default="app-suite-")
    parser.add_argument("--compute", choices=("cpu", "gpu"), default="cpu")
    parser.add_argument("--language", default="ja")
    parser.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    args = parser.parse_args()
    profile_path = args.profile.resolve()
    input_path = args.input.resolve()
    exe_path = args.exe.resolve()
    profile = json.loads(profile_path.read_text(encoding="utf-8"))
    properties_by_effect = profile["profiles"]
    args.output_dir.mkdir(parents=True, exist_ok=True)
    manifest_entries: list[dict[str, object]] = []
    for profile_key, profile_entry in properties_by_effect.items():
        if "effect_id" in profile_entry and "properties" in profile_entry:
            effect_id = profile_entry["effect_id"]
            properties = profile_entry["properties"]
        else:
            effect_id = profile_key
            properties = profile_entry
        effect_key = effect_id.replace(".", "_")
        variant_key = profile_key.replace(".", "_")
        output_path = args.output_dir / f"{args.prefix}{variant_key}.png"
        command = [
            sys.executable, "tools/capture_ui_test.py", "--exe", str(exe_path),
            "--output", str(output_path), "--language", args.language,
            "--effect-suite-id", effect_id,
            "--effect-suite-compute", args.compute,
            "--effect-suite-input", str(input_path),
            "--effect-suite-properties",
            json.dumps(properties, separators=(",", ":")),
        ]
        print(f"Capturing {effect_id}", flush=True)
        try:
            subprocess.run(command, cwd=ROOT, check=True)
        except subprocess.CalledProcessError as error:
            manifest_entries.append({
                "profile_key": profile_key,
                "effect_id": effect_id,
                "properties": properties,
                "requested_compute": args.compute,
                "capture_error": f"capture_ui_test exited with {error.returncode}",
            })
            print(f"Capture failed for {effect_id}; continuing with the next profile",
                  flush=True)
            continue
        product_path = output_path.with_name(
            f"{output_path.stem}-{effect_id}-{args.compute}.png")
        source_path = product_path.with_name(
            f"{product_path.stem}-fixture-input{product_path.suffix}")
        manifest_entries.append({
            "profile_key": profile_key,
            "effect_id": effect_id,
            "properties": properties,
            "requested_compute": args.compute,
            "product_output": product_path.name,
            "source_capture": source_path.name,
            "source_capture_sha256": digest(source_path),
            "product_output_sha256": digest(product_path),
            "state_sidecar": f"{product_path.name}.state.txt",
        })
    manifest_path = args.output_dir / "product-suite-manifest.json"
    manifest_path.write_text(json.dumps({
        "capture_class": "ArtifactUiTest installed-effect product output",
        "not_final_viewer_output": True,
        "profile": str(profile_path.relative_to(ROOT)) if profile_path.is_relative_to(ROOT)
                   else str(profile_path),
        "profile_sha256": digest(profile_path),
        "input": str(input_path.relative_to(ROOT)) if input_path.is_relative_to(ROOT)
                 else str(input_path),
        "input_sha256": digest(input_path),
        "exe": str(exe_path.relative_to(ROOT)) if exe_path.is_relative_to(ROOT)
               else str(exe_path),
        "exe_sha256": digest(exe_path),
        "language": args.language,
        "requested_compute": args.compute,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "entries": manifest_entries,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Saved capture manifest {manifest_path}")


if __name__ == "__main__":
    main()
