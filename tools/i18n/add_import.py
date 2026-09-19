from pathlib import Path

p = Path("Artifact/src/Widgets/Dialog/ArtifactRenderOutputSettingDialog.cppm")
c = p.read_text(encoding="utf-8")
if "import Translation.Manager;" not in c:
    old = "import Artifact.Widgets.RelativeSpinBox;"
    new = "import Artifact.Widgets.RelativeSpinBox;\nimport Translation.Manager;"
    c = c.replace(old, new, 1)
    p.write_text(c, encoding="utf-8")
    print("import added")
else:
    print("import already present")
