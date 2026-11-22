import json
import tempfile
from pathlib import Path


def leaves(value, prefix=""):
    if not isinstance(value, dict):
        return {}
    out = {}
    for k, v in value.items():
        if k.startswith("_"):
            continue
        full = f"{prefix}.{k}" if prefix else k
        if isinstance(v, dict):
            out.update(leaves(v, full))
        elif isinstance(v, str):
            out[full] = v
    return out


base = Path("Artifact/translations")
sets = {n: set(leaves(json.loads((base / f"{n}.json").read_text(encoding="utf-8"))))
        for n in ("en", "ja", "zh", "zh-TW", "ko", "fr", "de", "es", "pt", "ru", "ar")}
for n, s in sets.items():
    print(f"{n}: {len(s)}")
print()
print("zh - zh-TW:", sorted(sets["zh"] - sets["zh-TW"])[:10], f"(count={len(sets['zh'] - sets['zh-TW'])})")
print("zh-TW - zh:", sorted(sets["zh-TW"] - sets["zh"])[:10], f"(count={len(sets['zh-TW'] - sets['zh'])})")
print("zh-TW - ja:", len(sets["zh-TW"] - sets["ja"]))
print()
# Report keys defined in English but absent from each locale. Previously only
# zh <-> zh-TW and zh-TW - ja were printed, so the largest parity gap
# (en - zh / zh-TW) was invisible as a list. Useful as the audit next step.
for n in ("ja", "zh", "zh-TW", "ko", "fr", "de", "es", "pt", "ru", "ar"):
    missing = sorted(sets["en"] - sets[n])
    if not missing:
        continue
    sections = {}
    for key in missing:
        section = key.split(".", 1)[0]
        sections[section] = sections.get(section, 0) + 1
    print(f"en - {n}: {len(missing)} missing; sections={sections}")
    for key in missing[:20]:
        print(f"  {key}")
    if len(missing) > 20:
        print(f"  ... and {len(missing) - 20} more")
    # Optional: write the full list so translation work can be tracked section
    # by section without re-running with a different tool.
    out = Path(tempfile.gettempdir()) / f"artifact_missing_en_{n}.txt"
    out.write_text("\n".join(missing) + "\n", encoding="utf-8")
    print(f"  full list written to {out}")
