#!/usr/bin/env python3
"""
Remove threading-related includes from GMF section of .ixx files.
GMF section is everything BEFORE the 'export module' line.
"""

import os
import re
from pathlib import Path
from typing import Tuple, List

# Headers to remove from GMF section
HEADERS_TO_REMOVE = {
    '<thread>',
    '<condition_variable>',
    '<mutex>',
    '<semaphore>',
    '<latch>',
    '<barrier>',
}

def has_utf8_bom(file_path: str) -> bool:
    """Check if file has UTF-8 BOM by reading first 3 bytes."""
    try:
        with open(file_path, 'rb') as f:
            return f.read(3) == b'\xef\xbb\xbf'
    except Exception:
        return False

def find_export_module_line(lines: List[str]) -> int:
    """
    Find the line index where 'export module ' appears.
    Returns the line index, or -1 if not found.
    """
    for i, line in enumerate(lines):
        if 'export module ' in line:
            return i
    return -1

def should_remove_include(line: str) -> bool:
    """
    Check if line is an include that should be removed.
    Line format: optional whitespace + #include + whitespace + header
    """
    stripped = line.lstrip()
    if not stripped.startswith('#include'):
        return False
    
    # Extract the header part
    match = re.match(r'#include\s+(<[^>]+>)', stripped)
    if not match:
        return False
    
    header = match.group(1)
    return header in HEADERS_TO_REMOVE

def process_file(file_path: str) -> Tuple[bool, List[str]]:
    """
    Process a single .ixx file.
    Returns (was_modified, list_of_removed_headers)
    """
    # Detect UTF-8 BOM
    has_bom = has_utf8_bom(file_path)
    encoding = 'utf-8-sig' if has_bom else 'utf-8'
    
    # Read file
    try:
        with open(file_path, 'r', encoding=encoding) as f:
            content = f.read()
    except Exception as e:
        print(f"ERROR reading {file_path}: {e}")
        return False, []
    
    lines = content.splitlines(keepends=True)
    
    # Find export module line
    export_line_idx = find_export_module_line(lines)
    if export_line_idx == -1:
        # No export module found, don't process
        return False, []
    
    # GMF section is lines before export module
    gmf_end = export_line_idx
    removed_headers = []
    new_gmf_lines = []
    
    for i in range(gmf_end):
        line = lines[i]
        if should_remove_include(line):
            # Extract header name for logging
            match = re.search(r'<([^>]+)>', line)
            if match:
                removed_headers.append(match.group(1))
        else:
            new_gmf_lines.append(line)
    
    # Check if any lines were removed
    was_modified = len(removed_headers) > 0
    
    if was_modified:
        # Reconstruct file: GMF (modified) + export section
        new_content = ''.join(new_gmf_lines) + ''.join(lines[gmf_end:])
        
        # Write back with same encoding
        try:
            with open(file_path, 'w', encoding=encoding) as f:
                f.write(new_content)
        except Exception as e:
            print(f"ERROR writing {file_path}: {e}")
            return False, removed_headers
    
    return was_modified, removed_headers

def main():
    """Main entry point."""
    base_dir = r'X:\Dev\ArtifactStudio\ArtifactCore\include'
    
    if not os.path.isdir(base_dir):
        print(f"ERROR: Directory not found: {base_dir}")
        return
    
    print(f"Scanning for .ixx files in: {base_dir}\n")
    
    # Find all .ixx files recursively
    ixx_files = list(Path(base_dir).rglob('*.ixx'))
    
    if not ixx_files:
        print("No .ixx files found.")
        return
    
    print(f"Found {len(ixx_files)} .ixx files\n")
    
    modified_files = []
    total_removed = {}
    
    for file_path in sorted(ixx_files):
        was_modified, removed_headers = process_file(str(file_path))
        
        if was_modified:
            rel_path = file_path.relative_to(base_dir)
            modified_files.append((str(rel_path), removed_headers))
            print(f"✓ Modified: {rel_path}")
            for header in removed_headers:
                total_removed[header] = total_removed.get(header, 0) + 1
                print(f"  - Removed: #include <{header}>")
    
    # Print summary
    print("\n" + "="*60)
    print("SUMMARY")
    print("="*60)
    print(f"Total files processed: {len(ixx_files)}")
    print(f"Files modified: {len(modified_files)}")
    
    if total_removed:
        print("\nHeaders removed:")
        for header in sorted(total_removed.keys()):
            count = total_removed[header]
            print(f"  <{header}>: {count} occurrence(s)")
    else:
        print("\nNo headers were removed.")

if __name__ == '__main__':
    main()
