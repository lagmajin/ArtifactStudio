import subprocess
import sys

result = subprocess.run(
    'cmake --build . --target Artifact -- /nologo /v:m',
    cwd=r'X:\Dev\ArtifactStudio\out\build\x64-Debug',
    shell=True, 
    capture_output=True, 
    text=True, 
    timeout=300
)

output = result.stdout + result.stderr
error_count = 0
error_lines = []

for line in output.splitlines():
    if 'error C' in line or 'error:' in line:
        error_lines.append(line)
        error_count += 1
        if error_count >= 30:
            break

# Determine success/failure
if result.returncode == 0:
    print("BUILD RESULT: SUCCESS")
else:
    print("BUILD RESULT: FAILURE (exit code: {})".format(result.returncode))

print("\nTotal errors found: {}\n".format(error_count))
print("=" * 100)
for i, line in enumerate(error_lines, 1):
    print("{:2d}. {}".format(i, line))
print("=" * 100)

sys.exit(result.returncode)
