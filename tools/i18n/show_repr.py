from pathlib import Path

c = Path("Artifact/src/Widgets/Dialog/ArtifactRenderOutputSettingDialog.cppm").read_text(encoding="utf-8")
lines = c.split("\n")
for n in (160, 161, 162, 163, 164, 884, 885, 886):
    print(f"{n}: {lines[n-1]!r}")
