import os
import re

root = r'X:\dev\artifactstudio\Artifact\include'
results = []

for dirpath, dirs, files in os.walk(root):
    for fname in files:
        if fname.endswith('.ixx'):
            fpath = os.path.join(dirpath, fname)
            try:
                with open(fpath, 'r', encoding='utf-8-sig', errors='ignore') as f:
                    content = f.read()
            except:
                continue
            lines = content.splitlines()
            # Find export module line
            export_line = None
            for i, line in enumerate(lines):
                if re.match(r'\s*export\s+module\s+', line):
                    export_line = i
                    break
            if export_line is None:
                continue
            # Check GMF section for Qt includes
            gmf_lines = lines[:export_line]
            qt_includes = [l.strip() for l in gmf_lines if re.match(r'\s*#include\s*<Q', l) or re.match(r'\s*#include\s*<wobject', l) or re.match(r'\s*#include\s*<ads_globals', l)]
            if qt_includes:
                results.append((fpath, qt_includes))

print(f"Files with Qt includes in GMF section: {len(results)}")
for fpath, incs in results:
    print(f'\nFILE: {fpath}')
    for inc in incs:
        print(f'  {inc}')
