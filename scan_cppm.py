#!/usr/bin/env python3
import os
import re
from pathlib import Path

# Base directory
base_dir = r'X:\Dev\ArtifactStudio\ArtifactCore\src'

# Find all .cppm files
cppm_files = list(Path(base_dir).rglob('*.cppm'))
print(f'Found {len(cppm_files)} .cppm files\n')

# Pattern to match module declaration
module_pattern = re.compile(r'^\s*module\s+[\w:]+\s*;', re.MULTILINE)

# Pattern to match cv:: or CV_ usage
opencv_usage_pattern = re.compile(r'cv::\w+|CV_\w+')

# Pattern to match OpenCV include
opencv_include_pattern = re.compile(r'#include\s+<opencv2/')

results = []

for cpp_file in sorted(cppm_files):
    try:
        with open(cpp_file, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
    except Exception as e:
        print(f"Error reading {cpp_file}: {e}")
        continue
    
    # Find module declaration position
    module_match = module_pattern.search(content)
    if not module_match:
        continue  # Skip files without module declaration
    
    module_pos = module_match.start()
    
    # Split into GMF (before module) and body (after module)
    gmf = content[:module_pos]
    body = content[module_pos:]
    
    # Check if GMF has OpenCV include
    has_opencv_include = bool(opencv_include_pattern.search(gmf))
    
    # Check if body uses cv:: or CV_
    opencv_usages = list(opencv_usage_pattern.finditer(body))
    
    # Report if body uses OpenCV but GMF doesn't have include
    if opencv_usages and not has_opencv_include:
        first_usage = opencv_usages[0]
        first_usage_text = first_usage.group()
        first_usage_line_num = body[:first_usage.start()].count('\n') + 1
        
        # Get first 5 lines of file
        first_5_lines = '\n'.join(content.split('\n')[:5])
        
        # Get relative path
        rel_path = os.path.relpath(cpp_file, base_dir)
        
        results.append({
            'file': rel_path,
            'full_path': str(cpp_file),
            'gmf': first_5_lines,
            'first_usage': first_usage_text,
            'usage_line': first_usage_line_num
        })

print(f'RESULT: Found {len(results)} files with OpenCV usage but missing include in GMF\n')
print('=' * 100)

if results:
    for i, result in enumerate(results, 1):
        print(f'\n[{i}] FILE: {result["file"]}')
        print(f'    Full path: {result["full_path"]}')
        print(f'\n    First 5 GMF lines:')
        for line_num, line in enumerate(result['gmf'].split('\n')[:5], 1):
            print(f'      L{line_num}: {line}')
        print(f'\n    First cv:: or CV_ usage: "{result["first_usage"]}" (appears on body line {result["usage_line"]})')
        print('-' * 100)
else:
    print("\nNo issues found! All .cppm files that use cv:: or CV_ have OpenCV in their GMF.")

print('\n' + '=' * 100)
print(f"SUMMARY: {len(results)} file(s) need OpenCV include in their Global Module Fragment")
