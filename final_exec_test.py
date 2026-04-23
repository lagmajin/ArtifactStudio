#!/usr/bin/env python3
"""Final attempt to execute fix_nonwidget_gmf.py"""
import subprocess
import sys

try:
    # Try direct execution
    result = subprocess.Popen(
        [sys.executable, r'X:\Dev\ArtifactStudio\fix_nonwidget_gmf.py'],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        cwd=r'X:\Dev\ArtifactStudio'
    )
    stdout, stderr = result.communicate(timeout=60)
    print(stdout)
    if stderr:
        print("STDERR:", stderr)
    sys.exit(result.returncode)
except Exception as e:
    print(f"Error: {e}")
    sys.exit(1)
