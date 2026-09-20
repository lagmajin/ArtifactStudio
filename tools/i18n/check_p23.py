from pathlib import Path

f = "Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm"
c = Path(f).read_text(encoding="utf-8")
print(f"{f}: paren={c.count('(') - c.count(')')} brace={c.count('{') - c.count('}')}")
for n in ["import Localization.LocaleFormatting;", "LocaleFormatting::formatFileSize",
          "LocaleFormatting::formatPercentage"]:
    print(f"  {'OK' if n in c else 'MISSING'}: {n}")

# Check LocaleFormatting module is referenced by CMake
import subprocess, sys
hits = []
for cm in Path("ArtifactCore").rglob("CMakeLists.txt"):
    txt = cm.read_text(encoding="utf-8", errors="ignore")
    if "LocaleFormatting" in txt:
        hits.append(str(cm))
print("CMakeLists mentioning LocaleFormatting:", hits or "none (likely GLOB-based)")
