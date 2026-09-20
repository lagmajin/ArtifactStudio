import re
import sys
from pathlib import Path

str_re = re.compile(r'"(?:\\.|[^"\\])*"', re.S)
for f in sys.argv[1:]:
    c = Path(f).read_text(encoding="utf-8")
    s = str_re.sub('""', c)
    print(f"{f}: paren={s.count('(') - s.count(')')} brace={s.count('{') - s.count('}')}")
