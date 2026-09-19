import re
from pathlib import Path
import sys

content = Path(sys.argv[1]).read_text(encoding="utf-8")
jp_re = re.compile(r"[\u3040-\u309f\u30a0-\u30ff\u4e00-\u9fff]")
lines = content.split("\n")
for i, line in enumerate(lines, 1):
    if jp_re.search(line):
        if "menuText" not in line and "tt(" not in line and "tr(" not in line and "TranslationManager" not in line:
            print(f"{i}: {line.strip()[:160]}")
