import re
from pathlib import Path

str_re = re.compile(r'"(?:\\.|[^"\\])*"', re.S)
for f in ["Artifact/src/Widgets/Dialog/FloatColorPickerHooks.cppm"]:
    c = Path(f).read_text(encoding="utf-8")
    s = str_re.sub('""', c)
    print(f, "paren", s.count("(") - s.count(")"), "brace", s.count("{") - s.count("}"))
