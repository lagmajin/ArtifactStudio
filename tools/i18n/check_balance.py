from pathlib import Path

FILES = [
    "Artifact/src/Widgets/Menu/ArtifactOptionMenu.cppm",
    "Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm",
    "Artifact/src/Widgets/Menu/ArtifactEffectMenu.cppm",
    "Artifact/src/Widgets/Menu/ArtifactTimeMenu.cppm",
    "Artifact/src/Widgets/Menu/ArtifactTestMenu.cppm",
]
for f in FILES:
    c = Path(f).read_text(encoding="utf-8")
    print(f"{f}: paren={c.count('(') - c.count(')')} brace={c.count('{') - c.count('}')}")
