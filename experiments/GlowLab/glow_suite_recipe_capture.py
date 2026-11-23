"""Capture one shared Glow foundation and branch it through signature effects."""

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


def capture(exe: Path, source: Path, output_dir: Path, label: str,
            effect_id: str, properties: dict[str, object]) -> dict[str, object]:
    output = output_dir / f"{label}.png"
    command = [
        sys.executable, "tools/capture_ui_test.py", "--exe", str(exe),
        "--output", str(output), "--language", "ja",
        "--effect-suite-id", effect_id, "--effect-suite-compute", "cpu",
        "--effect-suite-input", str(source), "--effect-suite-properties",
        json.dumps(properties, separators=(",", ":")),
    ]
    subprocess.run(command, cwd=ROOT, check=True)
    product = output.with_name(f"{output.stem}-{effect_id}-cpu.png")
    source_copy = product.with_name(f"{product.stem}-fixture-input.png")
    state = product.with_name(f"{product.name}.state.txt")
    return {
        "label": label,
        "effect_id": effect_id,
        "properties": properties,
        "requested_compute": "cpu",
        "input": str(source.relative_to(ROOT)) if source.is_relative_to(ROOT) else str(source),
        "input_sha256": digest(source),
        "product_output": product.name,
        "product_output_sha256": digest(product),
        "captured_input": source_copy.name,
        "captured_input_sha256": digest(source_copy),
        "state_sidecar": state.name,
    }


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
    entries: list[dict[str, object]] = []
    foundation = profile["foundation"]
    try:
        base = capture(exe, input_path, args.output_dir, "suite-foundation",
                       foundation["effect_id"], foundation["properties"])
        entries.append(base)
        foundation_path = args.output_dir / str(base["product_output"])
    except (subprocess.CalledProcessError, OSError, KeyError) as error:
        entries.append({"label": "suite-foundation", "status": "failed",
                        "error": str(error)})
        foundation_path = None

    for recipe_name, recipe in profile["recipes"].items():
        if foundation_path is None:
            entries.append({"label": recipe_name, "status": "skipped",
                            "reason": "shared Glow foundation capture failed"})
            continue
        try:
            entry = capture(exe, foundation_path, args.output_dir,
                            f"suite-foundation-plus-{recipe_name}",
                            recipe["effect_id"], recipe["properties"])
            entry["status"] = "captured"
            entry["shared_foundation"] = "suite-foundation"
            entries.append(entry)
        except (subprocess.CalledProcessError, OSError, KeyError) as error:
            entries.append({"label": recipe_name, "effect_id": recipe.get("effect_id"),
                            "status": "failed", "error": str(error)})

    payload = {
        "capture_class": "Chained ArtifactUiTest installed-effect CPU output",
        "not_single_stack_or_final_viewer": True,
        "intermediate_png_quantization": "Each recipe branch reimports the 8-bit PNG foundation; this is a comparative R&D proxy, not the in-memory float stacked pipeline.",
        "profile": str(profile_path.relative_to(ROOT)) if profile_path.is_relative_to(ROOT)
                   else str(profile_path),
        "profile_sha256": digest(profile_path),
        "input": str(input_path.relative_to(ROOT)) if input_path.is_relative_to(ROOT)
                 else str(input_path),
        "input_sha256": digest(input_path),
        "exe": str(exe.relative_to(ROOT)) if exe.is_relative_to(ROOT) else str(exe),
        "exe_sha256": digest(exe),
        "language": "ja",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "entries": entries,
    }
    (args.output_dir / "suite-recipe-manifest.json").write_text(
        json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(f"Saved recipe manifest: {args.output_dir / 'suite-recipe-manifest.json'}")


if __name__ == "__main__":
    main()
