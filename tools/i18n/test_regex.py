import re
import sys

s = "名前を付けて保存"
print(f"re.escape result: {re.escape(s)!r}")
# Test if the escaped version matches in a sample string
test = 'QFileDialog::getSaveFileName(menu_, "名前を付けて保存", QString())'
escaped = re.escape(s)
pattern = re.compile(r'"(' + escaped + r')"')
match = pattern.search(test)
print(f"Match: {match}")
if match:
    print(f"Matched: {match.group(0)!r}")
