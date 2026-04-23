#!/usr/bin/env python
import subprocess
import sys

result = subprocess.run([sys.executable, r'X:\dev\artifactstudio\fix_nonwidget_gmf.py'], 
                       capture_output=True, text=True)
print(result.stdout)
if result.stderr:
    print("STDERR:", result.stderr, file=sys.stderr)
sys.exit(result.returncode)
