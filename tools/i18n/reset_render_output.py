"""Reset the dialog source, re-add the import, and fix mapping newlines."""
import json
import subprocess
from pathlib import Path

# 1) Fix mapping values: real newline -> literal backslash + n
MAP = Path("tools/i18n/mappings/render_output_dialog.json")
mapping = json.loads(MAP.read_text(encoding="utf-8"))
fixed = 0
for e in mapping:
    for field in ("jp", "en"):
        v = e.get(field)
        if isinstance(v, str) and "\n" in v:
            e[field] = v.replace("\n", r"\n")
            fixed += 1
MAP.write_text(json.dumps(mapping, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
print(f"mapping values fixed: {fixed}")

# 2) Re-add the import after git checkout
SRC = Path("Artifact/src/Widgets/Dialog/ArtifactRenderOutputSettingDialog.cppm")
c = SRC.read_text(encoding="utf-8")
if "import Translation.Manager;" not in c:
    c = c.replace(
        "import Artifact.Widgets.RelativeSpinBox;",
        "import Artifact.Widgets.RelativeSpinBox;\nimport Translation.Manager;",
        1,
    )
    SRC.write_text(c, encoding="utf-8")
    print("import added")
else:
    print("import already present")
