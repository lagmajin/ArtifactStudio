"""Sanity-check the migrated files for known breakage patterns."""
import re
from pathlib import Path

FILES = [
    "Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm",
    "Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm",
    "Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm",
    "Artifact/src/Widgets/Dialog/ArtifactRenderOutputSettingDialog.cppm",
]

# A string literal immediately followed by a function call = broken concatenation
ADJACENT = re.compile(r'"[^"\n]*"\s+[A-Za-z_][A-Za-z0-9_:]*\s*\(')
NESTED_MENUTEXT = re.compile(r"menuText\(\s*QStringLiteral\([^)]*\)\s*,\s*menuText\(")

for f in FILES:
    c = Path(f).read_text(encoding="utf-8")
    problems = []
    for i, line in enumerate(c.split("\n"), 1):
        if ADJACENT.search(line):
            problems.append(("adjacent", i, line.strip()[:110]))
        if NESTED_MENUTEXT.search(line):
            problems.append(("nested", i, line.strip()[:110]))
    # paren balance for the whole file
    bal = c.count("(") - c.count(")")
    print(f"{f}: paren_balance={bal}, problems={len(problems)}")
    for kind, i, l in problems[:8]:
        print(f"   [{kind}] {i}: {l}")
