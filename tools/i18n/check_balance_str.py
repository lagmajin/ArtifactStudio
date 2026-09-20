import re
from pathlib import Path

FILES = [
    "Artifact/src/Widgets/Menu/ArtifactTestMenu.cppm",
    "Artifact/src/Widgets/Menu/ArtifactOptionMenu.cppm",
    "Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm",
    "Artifact/src/Widgets/Menu/ArtifactTimeMenu.cppm",
]
str_re = re.compile(r'"(?:\\.|[^"\\])*"', re.S)
for f in FILES:
    c = Path(f).read_text(encoding="utf-8")
    stripped = str_re.sub('""', c)  # remove string literal contents
    print(f"{f}: paren={stripped.count('(') - stripped.count(')')} brace={stripped.count('{') - stripped.count('}')}")
