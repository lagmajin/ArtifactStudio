import os
import re

dirs_to_search = [
    r"X:\Dev\ArtifactStudio\ArtifactCore\include",
    r"X:\Dev\ArtifactStudio\Artifact\include"
]

export_module_pattern = re.compile(r'export\s+module\s+')
opencv_include_pattern = re.compile(r'#include\s+<opencv2/')

violations = []

def should_skip(path_str):
    lower_path = path_str.lower()
    return "libs\\" in lower_path or "libs/" in lower_path or "artifactwidgets" in lower_path

def check_file(file_path):
    if should_skip(file_path):
        return
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()
    except:
        return
    
    export_line = None
    for i, line in enumerate(lines, 1):
        if export_module_pattern.search(line):
            export_line = i
            break
    
    if export_line is None:
        return
    
    for i in range(export_line, len(lines)):
        if opencv_include_pattern.search(lines[i]):
            violations.append((file_path, export_line, i+1, lines[i].rstrip()))

for dir_path in dirs_to_search:
    if os.path.exists(dir_path):
        for root, dirs, files in os.walk(dir_path):
            if should_skip(root):
                dirs.clear()
                continue
            for f in files:
                if f.endswith('.ixx'):
                    check_file(os.path.join(root, f))

if violations:
    print("Violations found:\n")
    for fpath, eline, iline, icontent in sorted(violations):
        print(f"File: {fpath}")
        print(f"  export module at line {eline}")
        print(f"  #include <opencv2/ at line {iline}: {icontent}")
        print()
else:
    print("No files with OpenCV in module body")
