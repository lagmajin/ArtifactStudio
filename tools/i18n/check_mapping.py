import json
from pathlib import Path

for name in ("en", "ja"):
    d = json.loads(Path(f"Artifact/translations/{name}.json").read_text(encoding="utf-8"))
    ro = d.get("dialog", {}).get("render_output", {})
    for k in ("detail_text", "package_image_sequence_full", "deep_exr_tooltip"):
        print(f"{name}.dialog.render_output.{k} = {'OK' if k in ro else '<MISSING>'}")
