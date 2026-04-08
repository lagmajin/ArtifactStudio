#!/usr/bin/env python3
"""
Scan .cppm files for cv:: usage without OpenCV includes in GMF
"""
import os
import re
import glob

# Configuration
base_dir = r"X:\Dev\ArtifactStudio\ArtifactCore\src"

# Find all .cppm files using glob
search_pattern = os.path.join(base_dir, "**", "*.cppm")
cppm_files = sorted(glob.glob(search_pattern, recursive=True))

print(f"Found {len(cppm_files)} .cppm files\n")
print("=" * 100)

results = []

for cppm_file in cppm_files:
    try:
        with open(cppm_file, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
            lines = content.split('\n')
        
        # Find the first module declaration (the actual module X line, not module;)
        # The GMF includes the initial "module;" line and anything up to "module X;"
        module_line_idx = -1
        for i, line in enumerate(lines):
            # Look for "module <identifier>" (not just "module;")
            if re.search(r'^\s*module\s+[A-Za-z_][\w.]*\s*;', line):
                module_line_idx = i
                break
        
        if module_line_idx == -1:
            # No module declaration found
            continue
        
        # GMF is everything before the actual module declaration
        gmf_lines = lines[:module_line_idx]
        # Body is everything after the module declaration
        body_lines = lines[module_line_idx + 1:]
        
        # Check if GMF has opencv2 include
        gmf_text = '\n'.join(gmf_lines)
        has_opencv_in_gmf = '#include <opencv2/' in gmf_text
        
        # Check if body has cv:: or CV_ usage
        body_text = '\n'.join(body_lines)
        cv_pattern = re.compile(r'cv::|CV_')
        cv_match = cv_pattern.search(body_text)
        
        # We want files that:
        # 1. Use cv:: or CV_ in body (cv_match is not None)
        # 2. Do NOT have opencv in GMF (has_opencv_in_gmf is False)
        if cv_match and not has_opencv_in_gmf:
            # Find the line number and content of first cv:: or CV_ usage
            cv_line_idx = -1
            first_cv_usage = ""
            for i, line in enumerate(body_lines):
                if cv_pattern.search(line):
                    cv_line_idx = i
                    first_cv_usage = line.strip()
                    break
            
            relative_path = os.path.relpath(cppm_file, base_dir)
            results.append({
                'file_path': cppm_file,
                'relative_path': relative_path,
                'gmf_lines': gmf_lines[:5],  # First 5 lines of GMF
                'first_cv_usage': first_cv_usage,
                'module_line_idx': module_line_idx,
                'cv_line_body_idx': cv_line_idx
            })
    
    except Exception as e:
        print(f"Error processing {cppm_file}: {e}")
        continue

# Output results
print(f"\nFiles with cv:: or CV_ usage but missing OpenCV in GMF: {len(results)}\n")

if len(results) == 0:
    print("✓ No issues found! All files using cv:: or CV_ have OpenCV in GMF.")
else:
    for i, result in enumerate(results, 1):
        print(f"\n{'=' * 100}")
        print(f"File {i}/{len(results)}: {result['relative_path']}")
        print(f"{'=' * 100}")
        
        print("\nFirst 5 lines (GMF):")
        for j, line in enumerate(result['gmf_lines'], 1):
            print(f"  L{j}: {line}")
        
        print(f"\n  -> module declaration at line {result['module_line_idx'] + 1}")
        print(f"\nFirst cv:: or CV_ usage in body:")
        print(f"  -> Line {result['module_line_idx'] + 2 + result['cv_line_body_idx']}: {result['first_cv_usage']}")

print(f"\n{'=' * 100}")
print(f"\nSummary:")
print(f"  • Total .cppm files scanned: {len(cppm_files)}")
print(f"  • Files using cv:: or CV_: {len([r for r in results if r])}")
print(f"  • Files MISSING OpenCV in GMF: {len(results)}")

if len(results) > 0:
    print(f"\n⚠️  ACTION REQUIRED: Add '#include <opencv2/...>' to GMF of {len(results)} file(s)")

