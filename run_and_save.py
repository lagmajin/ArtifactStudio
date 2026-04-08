#!/usr/bin/env python3
"""Run fix_nonwidget_gmf.py and save output to file"""
import subprocess
import sys

result = subprocess.run(
    [sys.executable, r'X:\Dev\ArtifactStudio\fix_nonwidget_gmf.py'],
    capture_output=True,
    text=True,
    cwd=r'X:\Dev\ArtifactStudio'
)

with open(r'X:\Dev\ArtifactStudio\fix_output.txt', 'w') as f:
    f.write(result.stdout)
    if result.stderr:
        f.write("\n--- STDERR ---\n")
        f.write(result.stderr)

print("Output saved to X:\Dev\ArtifactStudio\fix_output.txt")
sys.exit(result.returncode)
