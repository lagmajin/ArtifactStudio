import os
import re

root = r'X:\dev\artifactstudio\Artifact\include'
changed_files = []

def is_qt_include(line):
    stripped = line.strip()
    return (re.match(r'#include\s*<Q', stripped) or 
            re.match(r'#include\s*<wobject', stripped) or 
            re.match(r'#include\s*<ads_globals', stripped))

for dirpath, dirs, files in os.walk(root):
    for fname in files:
        if not fname.endswith('.ixx'):
            continue
        fpath = os.path.join(dirpath, fname)
        try:
            with open(fpath, 'r', encoding='utf-8-sig', errors='ignore') as f:
                content = f.read()
        except:
            continue
        
        lines = content.splitlines(keepends=True)
        
        # Find export module line index
        export_idx = None
        for i, line in enumerate(lines):
            if re.match(r'\s*export\s+module\s+', line):
                export_idx = i
                break
        if export_idx is None:
            continue
        
        # Check if any Qt includes exist in GMF
        gmf_qt = [(i, lines[i]) for i in range(export_idx) if is_qt_include(lines[i])]
        if not gmf_qt:
            continue
        
        # Build new file:
        # 1. GMF section without Qt includes
        new_lines = []
        removed_qt = []
        for i, line in enumerate(lines[:export_idx]):
            if is_qt_include(line):
                removed_qt.append(line)
            else:
                new_lines.append(line)
        
        # Remove trailing blank lines from GMF (before export module line)
        while new_lines and new_lines[-1].strip() == '':
            new_lines.pop()
        new_lines.append('\n')  # One blank line before export module
        
        # 2. export module line
        new_lines.append(lines[export_idx])
        
        # 3. Blank line + Qt includes
        new_lines.append('\n')
        for qt_line in removed_qt:
            new_lines.append(qt_line)
        
        # 4. Rest of file (after export module)
        rest = lines[export_idx + 1:]
        # Skip leading blank lines in rest
        start = 0
        while start < len(rest) and rest[start].strip() == '':
            start += 1
        new_lines.append('\n')
        new_lines.extend(rest[start:])
        
        new_content = ''.join(new_lines)
        
        # Check if BOM was present
        try:
            with open(fpath, 'rb') as f:
                raw = f.read(3)
            has_bom = raw[:3] == b'\xef\xbb\xbf'
        except:
            has_bom = False
        
        with open(fpath, 'w', encoding='utf-8', newline='') as f:
            if has_bom:
                f.write('\ufeff')
            f.write(new_content)
        
        changed_files.append((fpath, [l.strip() for l in removed_qt]))

print(f'Total files changed: {len(changed_files)}')
for fpath, incs in changed_files:
    print(f'\nFILE: {fpath}')
    for inc in incs:
        print(f'  Moved: {inc}')
