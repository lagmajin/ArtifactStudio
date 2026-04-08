#!/usr/bin/env python3
"""
Script to remove threading-related headers from the GMF (Global Module Fragment)
section of C++ module files (.ixx).

The GMF section is everything BEFORE the 'export module' line.
This script removes these headers only from that section:
- #include <mutex>
- #include <thread>
- #include <condition_variable>
- #include <semaphore>
- #include <latch>
- #include <barrier>
"""

import os
import re
from pathlib import Path
from collections import defaultdict

# Headers to remove from GMF section
HEADERS_TO_REMOVE = {
    '#include <mutex>',
    '#include <thread>',
    '#include <condition_variable>',
    '#include <semaphore>',
    '#include <latch>',
    '#include <barrier>',
}


def detect_encoding_and_bom(file_path):
    """Detect if file has UTF-8 BOM and return appropriate encoding."""
    with open(file_path, 'rb') as f:
        raw_bytes = f.read(3)
    
    if raw_bytes.startswith(b'\xef\xbb\xbf'):
        return 'utf-8-sig', True
    return 'utf-8', False


def process_ixx_file(file_path):
    """
    Process a single .ixx file to remove threading headers from GMF section.
    
    Returns:
        tuple: (was_modified, removed_headers_dict)
            - was_modified: bool indicating if file was changed
            - removed_headers_dict: dict with header names as keys, counts as values
    """
    # Detect encoding and BOM
    encoding, has_bom = detect_encoding_and_bom(file_path)
    
    # Read the file
    with open(file_path, 'r', encoding=encoding) as f:
        content = f.read()
    
    # Split lines to preserve formatting
    lines = content.split('\n')
    
    # Find the "export module" line
    export_module_index = -1
    for i, line in enumerate(lines):
        if re.search(r'export\s+module\b', line):
            export_module_index = i
            break
    
    if export_module_index == -1:
        # No "export module" found, skip this file
        return False, {}
    
    # Process GMF section (before export module)
    gmf_section = lines[:export_module_index]
    removed_headers = defaultdict(int)
    
    # Remove matching include lines from GMF section
    new_gmf_section = []
    for line in gmf_section:
        stripped = line.strip()
        
        # Check if this line matches any header we want to remove
        matched = False
        for header in HEADERS_TO_REMOVE:
            # Match exact header or header followed by optional whitespace/comment
            if stripped == header or (stripped.startswith(header) and (
                len(stripped) == len(header) or stripped[len(header)] in ' \t\r\n//'
            )):
                removed_headers[header] += 1
                matched = True
                break
        
        if not matched:
            new_gmf_section.append(line)
    
    # Check if anything was actually removed
    if not removed_headers:
        return False, {}
    
    # Reconstruct the file
    rest_of_file = lines[export_module_index:]
    new_lines = new_gmf_section + rest_of_file
    new_content = '\n'.join(new_lines)
    
    # Write back with original encoding
    with open(file_path, 'w', encoding=encoding) as f:
        f.write(new_content)
    
    return True, dict(removed_headers)


def main():
    """Main entry point."""
    base_dir = Path(r'X:\Dev\ArtifactStudio\ArtifactCore\include')
    
    if not base_dir.exists():
        print(f"Error: Directory does not exist: {base_dir}")
        return
    
    # Find all .ixx files
    ixx_files = list(base_dir.rglob('*.ixx'))
    
    if not ixx_files:
        print(f"No .ixx files found in {base_dir}")
        return
    
    print(f"Found {len(ixx_files)} .ixx files to process\n")
    
    modified_files = []
    total_removed = defaultdict(int)
    
    # Process each file
    for ixx_file in sorted(ixx_files):
        was_modified, removed_headers = process_ixx_file(ixx_file)
        
        if was_modified:
            modified_files.append((ixx_file, removed_headers))
            for header, count in removed_headers.items():
                total_removed[header] += count
            
            # Print details for this file
            rel_path = ixx_file.relative_to(base_dir)
            print(f"✓ Modified: {rel_path}")
            for header, count in sorted(removed_headers.items()):
                print(f"  - Removed {count}x {header}")
    
    # Print summary
    print("\n" + "="*70)
    print("SUMMARY")
    print("="*70)
    print(f"Total files modified: {len(modified_files)}")
    
    if total_removed:
        print("\nHeaders removed (totals across all files):")
        for header in sorted(total_removed.keys()):
            print(f"  - {header}: {total_removed[header]} occurrence(s)")
    else:
        print("No headers were removed.")
    
    print("\nModified files:")
    for ixx_file, _ in sorted(modified_files):
        print(f"  - {ixx_file.relative_to(base_dir)}")


if __name__ == '__main__':
    main()
