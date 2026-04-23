import subprocess
import sys

result = subprocess.run(
    [sys.executable, r'X:\Dev\ArtifactStudio\fix_widget_gmf.py'],
    cwd=r'X:\Dev\ArtifactStudio',
    capture_output=True,
    text=True
)

print(result.stdout)
if result.stderr:
    print("STDERR:")
    print(result.stderr)
print(f"Return code: {result.returncode}")
