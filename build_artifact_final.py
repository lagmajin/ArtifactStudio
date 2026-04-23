#!/usr/bin/env python3
"""
Build Artifact target - comprehensive script with error detection
"""
import subprocess
import sys
import os

def main():
    # Change to project directory
    project_dir = r'X:\dev\artifactstudio'
    os.chdir(project_dir)
    
    # Verify build tree exists
    build_dir = os.path.join(project_dir, 'build')
    if not os.path.isdir(build_dir):
        print(f"FATAL: Build directory not found: {build_dir}")
        return 1
    
    print("\n" + "="*70)
    print("BUILDING ARTIFACT TARGET")
    print("="*70)
    print(f"Project:     {project_dir}")
    print(f"Build Tree:  {build_dir}")
    print(f"Target:      Artifact")
    print(f"Config:      Debug")
    print("="*70 + "\n")
    
    # Build command
    cmd = [
        'cmake',
        '--build', build_dir,
        '--target', 'Artifact',
        '--config', 'Debug',
        '--verbose'
    ]
    
    print(f"Running: {' '.join(cmd)}\n")
    
    # Execute build
    try:
        result = subprocess.run(cmd, capture_output=False, text=True)
        exit_code = result.returncode
    except Exception as e:
        print(f"ERROR executing build: {e}")
        return 1
    
    print("\n" + "="*70)
    if exit_code == 0:
        print("BUILD RESULT: SUCCESS ✓")
    else:
        print(f"BUILD RESULT: FAILED (exit code: {exit_code})")
    print("="*70 + "\n")
    
    return exit_code

if __name__ == '__main__':
    sys.exit(main())
