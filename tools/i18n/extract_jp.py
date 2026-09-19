"""Extract unique Japanese string literals from a source file."""
import re
import sys
from pathlib import Path

content = Path(sys.argv[1]).read_text(encoding="utf-8")
jp_char = re.compile(r"[\u3040-\u309f\u30a0-\u30ff\u4e00-\u9fff]")

# Match string literals: QStringLiteral("...") and bare "..."
strs = re.findall(r'"([^"]*)"', content)
jp_strs = sorted(set(s for s in strs if jp_char.search(s)))

print(f"File: {sys.argv[1]}")
print(f"Total strings: {len(strs)}")
print(f"Japanese strings: {len(jp_strs)}")
print()
for s in jp_strs:
    print(repr(s))
