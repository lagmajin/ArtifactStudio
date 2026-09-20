"""Insert a `_meta` block at the top of each locale JSON (P2-2)."""

import json
from pathlib import Path

META = {
    "en": {
        "version": 1,
        "language": "English",
        "locale": "en",
        "translators": ["Artifact Team"],
        "lastUpdated": "2026-09-18",
    },
    "ja": {
        "version": 1,
        "language": "Japanese",
        "locale": "ja",
        "translators": ["Artifact Team"],
        "lastUpdated": "2026-09-18",
    },
}

for locale, meta in META.items():
    path = Path("Artifact/translations") / f"{locale}.json"
    data = json.loads(path.read_text(encoding="utf-8"))
    if "_meta" in data:
        print(f"{locale}: _meta already present, updating")
        data["_meta"] = meta
    else:
        # Insert at the top (preserve the rest of the ordering)
        new_data = {"_meta": meta}
        new_data.update(data)
        data = new_data
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"{locale}: wrote _meta")
