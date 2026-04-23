#!/usr/bin/env python3
import subprocess
import sys
import os

os.chdir(r'X:\dev\artifactstudio')

# Run the CMake build for ArtifactCore
print("=== Running: cmake --build out/build/x64-Debug --target ArtifactCore ===\n")
result = subprocess.run(
    ['cmake', '--build', r'out\build\x64-Debug', '--target', 'ArtifactCore', '--verbose'],
    capture_output=False,
    text=True
)

print(f"\n=== Build completed with return code: {result.returncode} ===")
sys.exit(result.returncode)
