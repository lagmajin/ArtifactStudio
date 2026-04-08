"""
Fix OpenCV includes in .ixx module files:
Move #include <opencv2/...> lines from the module body (after 'export module X;')
BACK to the GMF section (before 'export module X;').

This is the CORRECT placement because OpenCV transitively includes <immintrin.h>
which redefines SIMD types that are compiler built-ins, causing C1117 errors when
baked into the IFC module-purview section.
"""
import os
import re

IXX_DIRS = [
    r"X:\Dev\ArtifactStudio\ArtifactCore\include",
    r"X:\Dev\ArtifactStudio\Artifact\include",
]

OPENCV_INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]opencv2/')

def collect_ixx_files():
    result = []
    for d in IXX_DIRS:
        for root, dirs, files in os.walk(d):
            for fname in files:
                if fname.endswith('.ixx'):
                    result.append(os.path.join(root, fname))
    return result

def process_file(filepath):
    with open(filepath, 'rb') as f:
        raw = f.read()

    has_bom = raw[:3] == b'\xef\xbb\xbf'
    text = raw.decode('utf-8-sig')
    lines = text.splitlines(keepends=True)

    # Locate 'module;' (GMF start) and 'export module X;' line
    gmf_line_idx = -1
    export_module_idx = -1
    for i, line in enumerate(lines):
        stripped = line.strip()
        if stripped == 'module;' and gmf_line_idx == -1:
            gmf_line_idx = i
        if re.match(r'^\s*export\s+module\s+', line) and export_module_idx == -1 and gmf_line_idx != -1:
            export_module_idx = i
            break

    if gmf_line_idx == -1 or export_module_idx == -1:
        return None  # no GMF structure

    # Find opencv includes AFTER export module (in module body) — these need moving
    body_opencv_indices = []
    for i in range(export_module_idx + 1, len(lines)):
        if OPENCV_INCLUDE_RE.match(lines[i]):
            body_opencv_indices.append(i)

    if not body_opencv_indices:
        return None  # nothing to do

    # Collect the unique include keys already in GMF (to avoid duplicates)
    existing_in_gmf = set()
    for i in range(gmf_line_idx + 1, export_module_idx):
        if OPENCV_INCLUDE_RE.match(lines[i]):
            existing_in_gmf.add(lines[i].strip())

    lf = '\r\n' if '\r\n' in text else '\n'

    # Determine which body includes need to be added to GMF (deduplicate)
    to_insert_to_gmf = []
    seen = set()
    for i in body_opencv_indices:
        key = lines[i].strip()
        if key not in existing_in_gmf and key not in seen:
            to_insert_to_gmf.append(lines[i].rstrip('\r\n'))
            seen.add(key)

    # Build new line list:
    # 1. Keep all lines except body opencv includes
    # 2. Insert new includes right before export module line
    remove_set = set(body_opencv_indices)
    new_lines = []
    for i, line in enumerate(lines):
        if i == export_module_idx:
            # Insert collected includes just before this line
            for inc in to_insert_to_gmf:
                new_lines.append(inc + lf)
        if i not in remove_set:
            new_lines.append(line)

    new_text = ''.join(new_lines)
    if new_text == text:
        return None

    out_bytes = (b'\xef\xbb\xbf' if has_bom else b'') + new_text.encode('utf-8')
    with open(filepath, 'wb') as f:
        f.write(out_bytes)

    return {
        'moved_count': len(body_opencv_indices),
        'inserted_count': len(to_insert_to_gmf),
        'deduped_count': len(body_opencv_indices) - len(to_insert_to_gmf),
    }

def main():
    files = collect_ixx_files()
    print(f"Scanning {len(files)} .ixx files...")
    modified = []
    for path in files:
        result = process_file(path)
        if result:
            relpath = os.path.relpath(path, r"X:\Dev\ArtifactStudio")
            print(f"  FIXED: {relpath}")
            print(f"         removed {result['moved_count']} from body, "
                  f"added {result['inserted_count']} to GMF, "
                  f"skipped {result['deduped_count']} duplicates")
            modified.append(relpath)

    print(f"\nTotal modified: {len(modified)}")

if __name__ == '__main__':
    main()
