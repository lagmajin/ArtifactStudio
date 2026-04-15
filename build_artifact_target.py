#!/usr/bin/env python3
import subprocess
import sys
import os

os.chdir(r'X:\dev\artifactstudio')

# Check if build tree exists
if not os.path.exists('build'):
    print("ERROR: 'build' directory not found at X:\\dev\\artifactstudio\\build")
    sys.exit(1)

print("=== Building Artifact target ===\n")

# Try building with cmake
result = subprocess.run(
    ['cmake', '--build', 'build', '--target', 'Artifact', '--config', 'Debug'],
    capture_output=True,
    text=True
)

print(result.stdout)
if result.stderr:
    print("STDERR:", result.stderr)

if result.returncode != 0:
    print(f"\n=== Build FAILED with return code: {result.returncode} ===")
    # Try to capture more error details
    if 'C1116' in result.stdout or 'C1116' in result.stderr:
        print("\n[ERROR] C1116 compiler error detected!")
    if 'QStringView' in result.stdout or 'QStringView' in result.stderr:
        print("\n[ERROR] QStringView-related issue detected!")
else:
    print(f"\n=== Build SUCCEEDED ===")

sys.exit(result.returncode)
