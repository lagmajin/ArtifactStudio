#!/usr/bin/env python3
import subprocess
import sys

result = subprocess.run([sys.executable, r'X:\dev\artifactstudio\fix_gmf_qt.py'], capture_output=False, text=True)
sys.exit(result.returncode)
