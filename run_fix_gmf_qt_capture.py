#!/usr/bin/env python3
import subprocess
import sys
import os

# Change to the directory where the script is
os.chdir(r'X:\dev\artifactstudio')

result = subprocess.run([sys.executable, 'fix_gmf_qt.py'], 
                       capture_output=True, text=True)
print(result.stdout)
if result.stderr:
    print("STDERR:", result.stderr, file=sys.stderr)
sys.exit(result.returncode)
