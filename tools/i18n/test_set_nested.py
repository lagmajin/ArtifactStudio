import re, json, sys, argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("source")
parser.add_argument("--mapping", required=True)
args = parser.parse_args()

mapping = json.loads(Path(args.mapping).read_text(encoding="utf-8"))
en_data = {}
ja_data = {}
existing_en = set()
existing_ja = set()

# Load existing JSON files
artifact_dir = Path("Artifact")
for locale, data in [("en", en_data), ("ja", ja_data)]:
    jf = artifact_dir / "translations" / f"{locale}.json"
    if jf.exists():
        loaded = json.loads(jf.read_text(encoding="utf-8"))
        for k, v in loaded.items():
            data[k] = v
            def collect_keys(d, prefix=""):
                for k2, v2 in d.items():
                    if isinstance(v2, dict):
                        collect_keys(v2, f"{prefix}{k2}.")
                    elif isinstance(v2, str):
                        full = f"{prefix}{k2}"
                        if locale == "en":
                            existing_en.add(full)
                        else:
                            existing_ja.add(full)
            if isinstance(v, dict):
                collect_keys(v, f"{k}.")

all_existing = existing_en | existing_ja

def set_nested(data, parts, value):
    d = data
    for part in parts[:-1]:
        if part not in d or not isinstance(d[part], dict):
            d[part] = {}
        d = d[part]
    d[parts[-1]] = value

new_keys = []
for entry in mapping:
    jp_text = entry["jp"]
    key = entry["key"]
    en_text = entry.get("en", jp_text)
    
    if key not in all_existing:
        try:
            set_nested(en_data, key.split("."), en_text)
            set_nested(ja_data, key.split("."), jp_text)
            new_keys.append(key)
            all_existing.add(key)
        except Exception as e:
            print(f"ERROR at key {key}: {e}")
            print(f"  en_text: {en_text[:60]}")
            print(f"  jp_text: {jp_text[:60]}")
    else:
        pass  # Skip existing keys

print(f"OK. New keys: {len(new_keys)}")
for k in new_keys:
    print(f"  {k}")
