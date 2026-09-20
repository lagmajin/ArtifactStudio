"""Verify the zh-TW -> zh -> en fallback chain improves effective coverage.

Mirrors LocalizationManager::fallbackChainFor + translate() resolution:
for each baseline (en) key, resolve through the chain and report which
language actually supplied the value.
"""

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
en = leaves(json.loads((base / "en.json").read_text(encoding="utf-8")))
zh = leaves(json.loads((base / "zh.json").read_text(encoding="utf-8")))
zh_tw = leaves(json.loads((base / "zh-TW.json").read_text(encoding="utf-8")))

keys = sorted(en)
direct = 0
via_zh = 0
via_en = 0
for k in keys:
    if k in zh_tw:
        direct += 1
    elif k in zh:
        via_zh += 1
    else:
        via_en += 1

total = len(keys)
print(f"baseline (en) keys: {total}")
print(f"zh-TW direct:      {direct}")
print(f"zh-TW via zh:      {via_zh}   <-- chain gain (was en before)")
print(f"zh-TW via en:      {via_en}")
print()
print(f"effective zh-TW coverage with chain: {(direct + via_zh) / total * 100:.1f}%")
print(f"effective zh-TW coverage without chain (old behavior): {direct / total * 100:.1f}%")
