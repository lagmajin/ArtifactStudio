import os
import re

ROOT = r'X:\dev\artifactstudio'
SCAN_DIR = os.path.join(ROOT, 'Artifact', 'include')

QT_PATTERNS = [
    re.compile(r'#include\s*[<"]Q'),
    re.compile(r'#include\s*[<"]wobjectimpl\.h'),
    re.compile(r'#include\s*[<"]ads_globals\.h'),
    re.compile(r'W_OBJECT'),
]

def is_qt_line(line):
    s = line.strip()
    return any(p.search(s) for p in QT_PATTERNS)

def rel(path):
    return os.path.relpath(path, ROOT)

def process_file(fpath):
    with open(fpath, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    # Find module; line
    gmf_start = None
    for i, line in enumerate(lines):
        if line.strip() == 'module;':
            gmf_start = i
            break

    if gmf_start is None:
        return None

    # Find export module line after gmf_start
    export_mod = None
    for i in range(gmf_start + 1, len(lines)):
        if lines[i].strip().startswith('export module'):
            export_mod = i
            break

    if export_mod is None:
        return None

    # Collect Qt lines and non-Qt lines between module; and export module
    gmf_qt_lines = []
    gmf_keep_lines = []
    for i in range(gmf_start + 1, export_mod):
        if is_qt_line(lines[i]):
            gmf_qt_lines.append(lines[i])
        else:
            gmf_keep_lines.append(lines[i])

    if not gmf_qt_lines:
        return None

    # Build set of normalized lines already after export module
    existing_after = set()
    for i in range(export_mod + 1, len(lines)):
        existing_after.add(lines[i].strip())

    # Deduplicate: only insert Qt lines not already present
    to_insert = []
    for ql in gmf_qt_lines:
        if ql.strip() not in existing_after:
            to_insert.append(ql)

    # Rebuild file
    new_lines = []
    new_lines.extend(lines[:gmf_start + 1])
    new_lines.extend(gmf_keep_lines)
    new_lines.append(lines[export_mod])
    new_lines.extend(to_insert)
    new_lines.extend(lines[export_mod + 1:])

    new_content = ''.join(new_lines)
    old_content = ''.join(lines)

    if new_content == old_content:
        return None

    with open(fpath, 'w', encoding='utf-8') as f:
        f.write(new_content)

    return (gmf_qt_lines, to_insert)

def main():
    ixx_files = []
    for root, dirs, files in os.walk(SCAN_DIR):
        for fn in files:
            if fn.endswith('.ixx'):
                ixx_files.append(os.path.join(root, fn))
    ixx_files.sort()

    print(f'Scanning {len(ixx_files)} .ixx files in Artifact\\include...')
    print()

    modified = []
    for fpath in ixx_files:
        rp = rel(fpath)
        result = process_file(fpath)
        if result is None:
            print(f'OK:    {rp}')
        else:
            gmf_qt_lines, to_insert = result
            print(f'FIXED: {rp}')
            for ql in gmf_qt_lines:
                print(f'       GMF had: {ql.strip()}')
            modified.append((rp, gmf_qt_lines))

    print()
    print('=' * 60)
    print(f'Total scanned:  {len(ixx_files)} files')
    print(f'Total modified: {len(modified)} files')
    if modified:
        print()
        print('Modified files summary:')
        for rp, qt_lines in modified:
            print(f'  {rp}')
            for ql in qt_lines:
                print(f'    moved: {ql.strip()}')

if __name__ == '__main__':
    main()
