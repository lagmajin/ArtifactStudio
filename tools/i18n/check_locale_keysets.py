import json
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
