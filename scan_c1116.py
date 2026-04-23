import re
import os
from pathlib import Path

DIRS = [
    r"X:\Dev\ArtifactStudio\ArtifactCore\include",
    r"X:\Dev\ArtifactStudio\Artifact\include",
]

RE_MODULE = re.compile(r'export\s+module\s+([a-zA-Z0-9_.]+)\s*;')
RE_OPENCV  = re.compile(r'#include\s+<opencv2/[^>]+>')
RE_IMPORT  = re.compile(r'import\s+([a-zA-Z0-9_.]+)\s*;')

files = []
for d in DIRS:
    p = Path(d)
    if p.exists():
        files.extend(p.rglob("*.ixx"))

info = {}

for f in files:
    try:
        text = f.read_text(encoding="utf-8", errors="replace")
    except Exception as e:
        print(f"ERROR reading {f}: {e}")
        continue

    m = RE_MODULE.search(text)
    if not m:
        continue

    mod_name = m.group(1)
    split_pos = m.start()
    gmf  = text[:split_pos]
    body = text[split_pos:]

    gmf_opencv = RE_OPENCV.findall(gmf)
    imports    = RE_IMPORT.findall(body)

    info[mod_name] = {
        "file":       str(f),
        "gmf_opencv": gmf_opencv,
        "imports":    imports,
    }

opencv_mods = {k: v for k, v in info.items() if v["gmf_opencv"]}

print("=" * 72)
print("SECTION 1 – Modules with OpenCV in Global Module Fragment")
print("=" * 72)
if opencv_mods:
    for mod, d in sorted(opencv_mods.items()):
        print(f"\n  Module : {mod}")
        print(f"  File   : {d['file']}")
        for inc in d["gmf_opencv"]:
            print(f"           {inc}")
else:
    print("  (none)")

print()
print("=" * 72)
print("SECTION 2 – C1116 DANGER: OpenCV in GMF AND imports OpenCV-in-GMF module")
print("=" * 72)
danger = []
for mod, d in sorted(info.items()):
    if not d["gmf_opencv"]:
        continue
    bad_imports = [i for i in d["imports"] if i in opencv_mods]
    if bad_imports:
        danger.append((mod, d, bad_imports))

if danger:
    for mod, d, bad in danger:
        print(f"\n  *** RISKY MODULE: {mod}")
        print(f"  File   : {d['file']}")
        print(f"  Own GMF OpenCV:")
        for inc in d["gmf_opencv"]:
            print(f"           {inc}")
        print(f"  Imports OpenCV-in-GMF module(s):")
        for bi in bad:
            print(f"           {bi}  ({opencv_mods[bi]['file']})")
else:
    print("  (none found – no C1116 danger detected)")

print()
print("=" * 72)
print("SECTION 3 – Files that import an OpenCV-in-GMF module")
print("=" * 72)
importers = []
for mod, d in sorted(info.items()):
    bad_imports = [i for i in d["imports"] if i in opencv_mods]
    if bad_imports:
        importers.append((mod, d, bad_imports))

if importers:
    for mod, d, bad in importers:
        own_flag = " [also has own OpenCV in GMF]" if d["gmf_opencv"] else ""
        print(f"\n  Module : {mod}{own_flag}")
        print(f"  File   : {d['file']}")
        print(f"  Imports:")
        for bi in bad:
            print(f"           {bi}  ({opencv_mods[bi]['file']})")
else:
    print("  (none)")

print()
print(f"Total .ixx files scanned : {len(files)}")
print(f"Parsed (have export module): {len(info)}")
print(f"Modules with OpenCV in GMF: {len(opencv_mods)}")
print(f"C1116 danger count        : {len(danger)}")
print(f"Importers of risky mods   : {len(importers)}")
