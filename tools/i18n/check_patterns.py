from pathlib import Path
import sys

c = Path(sys.argv[1]).read_text(encoding="utf-8")
for name in ["menuText", "tt(", "tr(", "TranslationManager", "QString tt"]:
    count = c.count(name)
    print(f"{name}: {count}")
