import subprocess
import sys
import os

os.chdir(r'X:\dev\artifactstudio')
result = subprocess.run([sys.executable, 'fix_nonwidget_gmf.py'], capture_output=True, text=True)
print(result.stdout)
if result.stderr:
    print(result.stderr)
