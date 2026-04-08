"""
fix_qt_gmf.py - Move Qt includes out of the Global Module Fragment (GMF)
in C++ module interface files (.ixx).

Qt headers must NOT appear before 'export module' in .ixx files.
This script relocates them to just after the 'export module X;' line.
"""

import os
import re
import sys
from pathlib import Path

SCAN_ROOT = Path(r"X:\dev\artifactstudio\Artifact\include")
REPO_ROOT = Path(r"X:\dev\artifactstudio")

# Patterns that identify Qt-related includes that must NOT be in GMF
QT_INCLUDE_PATTERNS = [
    re.compile(r'^\s*#\s*include\s*[<"]Q[A-Za-z]'),         # <QObject>, "QString", etc.
    re.compile(r'^\s*#\s*include\s*[<"]wobjectimpl\.h[>"]'), # wobjectimpl.h
    re.compile(r'^\s*#\s*include\s*[<"]ads_globals\.h[>"]'), # Qt Advanced Docking
    re.compile(r'\bW_OBJECT\b'),                              # W_OBJECT macro line
]

def is_qt_line(line: str) -> bool:
    stripped = line.rstrip('\r\n')
    return any(p.search(stripped) for p in QT_INCLUDE_PATTERNS)

def detect_line_ending(raw: bytes) -> str:
    """Return '\r\n' if CRLF is predominant, else '\n'."""
    crlf = raw.count(b'\r\n')
    lf = raw.count(b'\n') - crlf
    return '\r\n' if crlf >= lf else '\n'

def process_file(filepath: Path) -> tuple[bool, list[str]]:
    """
    Process a single .ixx file.
    Returns (was_modified, list_of_moved_includes).
    """
    raw = filepath.read_bytes()

    # Detect and strip BOM
    has_bom = raw.startswith(b'\xef\xbb\xbf')
    if has_bom:
        raw_content = raw[3:]
    else:
        raw_content = raw

    line_ending = detect_line_ending(raw_content)

    text = raw_content.decode('utf-8', errors='replace')
    lines = text.splitlines(keepends=True)

    # Find 'module;' line (GMF start marker)
    gmf_start_idx = None
    for i, line in enumerate(lines):
        if re.match(r'^\s*module\s*;\s*$', line.rstrip('\r\n')):
            gmf_start_idx = i
            break

    if gmf_start_idx is None:
        return False, []

    # Find 'export module' line (GMF end)
    export_module_idx = None
    for i in range(gmf_start_idx + 1, len(lines)):
        if re.match(r'^\s*export\s+module\b', lines[i].rstrip('\r\n')):
            export_module_idx = i
            break

    if export_module_idx is None:
        return False, []

    # Collect Qt includes from GMF (between module; and export module)
    gmf_qt_lines: list[str] = []
    gmf_keep_lines: list[str] = []

    for i in range(gmf_start_idx + 1, export_module_idx):
        line = lines[i]
        if is_qt_line(line):
            gmf_qt_lines.append(line)
        else:
            gmf_keep_lines.append(line)

    if not gmf_qt_lines:
        return False, []

    # Build set of normalised includes already present after 'export module'
    existing_after = set()
    for line in lines[export_module_idx + 1:]:
        existing_after.add(line.rstrip('\r\n').strip())

    # Deduplicate: only keep Qt lines not already present after export module
    to_insert: list[str] = []
    for line in gmf_qt_lines:
        normalised = line.rstrip('\r\n').strip()
        if normalised not in existing_after:
            # Normalise line endings
            to_insert.append(normalised.replace('\r\n', line_ending)
                                        .replace('\n', line_ending) + line_ending)
            existing_after.add(normalised)

    # Rebuild file:
    # [0 .. gmf_start_idx]           <- keep as-is (module; line included)
    # gmf_keep_lines                 <- non-Qt GMF content
    # [export_module_idx]            <- export module X; line
    # to_insert                      <- Qt includes moved here
    # [export_module_idx+1 .. end]   <- rest of file

    new_lines = (
        lines[:gmf_start_idx + 1]
        + gmf_keep_lines
        + [lines[export_module_idx]]
        + to_insert
        + lines[export_module_idx + 1:]
    )

    new_text = ''.join(new_lines)
    # Re-normalise all line endings to detected style
    if line_ending == '\r\n':
        new_text = new_text.replace('\r\n', '\n').replace('\n', '\r\n')
    else:
        new_text = new_text.replace('\r\n', '\n')

    new_raw = new_text.encode('utf-8')
    if has_bom:
        new_raw = b'\xef\xbb\xbf' + new_raw

    if new_raw == raw:
        return False, []

    filepath.write_bytes(new_raw)

    moved_labels = [ln.rstrip('\r\n').strip() for ln in gmf_qt_lines]
    return True, moved_labels


def main():
    ixx_files = sorted(SCAN_ROOT.rglob('*.ixx'))
    total = len(ixx_files)
    print(f"Scanning {total} .ixx files in Artifact\\include...\n")

    modified_files: list[tuple[str, list[str]]] = []
    ok_files: list[str] = []

    for filepath in ixx_files:
        rel = filepath.relative_to(REPO_ROOT)
        try:
            changed, moved = process_file(filepath)
        except Exception as exc:
            print(f"ERROR: {rel} — {exc}")
            continue

        if changed:
            modified_files.append((str(rel), moved))
            print(f"FIXED: {rel}")
            for inc in moved:
                print(f"       GMF had: {inc}")
        else:
            ok_files.append(str(rel))
            print(f"OK:    {rel}")

    print()
    print("=" * 60)
    print(f"Total scanned:  {total} files")
    print(f"Total modified: {len(modified_files)} files")

    if modified_files:
        print()
        print("Modified files summary:")
        for path, moved in modified_files:
            print(f"  {path}")
            for inc in moved:
                print(f"    moved: {inc}")

if __name__ == '__main__':
    main()
