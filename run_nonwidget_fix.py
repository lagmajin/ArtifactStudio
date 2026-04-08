import subprocess
import sys

result = subprocess.run(
    [sys.executable, r'X:\Dev\ArtifactStudio\fix_nonwidget_gmf.py'],
    capture_output=True,
    text=True,
    timeout=300
)

print(result.stdout)
if result.stderr:
    print("STDERR:", result.stderr)

sys.exit(result.returncode)
