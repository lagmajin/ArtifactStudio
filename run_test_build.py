#!/usr/bin/env python3
import subprocess
import sys
import os

os.chdir(r'X:\dev\artifactstudio')

# Run the CMake build
print("=== Running: cmake --build build --target ArtifactCoreTest ===\n")
result = subprocess.run(
    ['cmake', '--build', 'build', '--target', 'ArtifactCoreTest'],
    capture_output=True,
    text=True
)

print(result.stdout)
if result.stderr:
    print(result.stderr)

print(f"\n=== Build completed with return code: {result.returncode} ===")
sys.exit(result.returncode)
