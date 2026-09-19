import json
from pathlib import Path

def flatten(d, prefix=""):
    for k, v in d.items():
        fk = f"{prefix}.{k}" if prefix else k
        if isinstance(v, dict):
            yield from flatten(v, fk)
        else:
            yield fk

en = json.loads(Path("Artifact/translations/en.json").read_text(encoding="utf-8"))
ja = json.loads(Path("Artifact/translations/ja.json").read_text(encoding="utf-8"))

en_keys = set(flatten(en))
ja_keys = set(flatten(ja))

print(f"en keys: {len(en_keys)}")
print(f"ja keys: {len(ja_keys)}")
print(f"en - ja: {len(en_keys - ja_keys)}")
print(f"ja - en: {len(ja_keys - en_keys)}")

# Show keys containing specific substrings
for term in ["save", "export", "dialog", "button", "backup", "recent", "numbered", "import", "project"]:
    matching = sorted(k for k in en_keys if term in k.lower())
    if matching:
        print(f"\n--- {term} ---")
        for k in matching:
            print(f"  {k}")
