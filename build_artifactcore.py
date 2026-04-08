#!/usr/bin/env python3
"""Build ArtifactCore with comprehensive output capture and analysis."""

import subprocess
import sys
import os
import re
from pathlib import Path

def setup_msvc_env():
    """Set up MSVC environment variables."""
    vcvars_path = r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    
    if not os.path.exists(vcvars_path):
        print(f"Warning: vcvars64.bat not found at {vcvars_path}")
        return {}
    
    # Run vcvars64.bat to get environment
    try:
        result = subprocess.run(
            f'"{vcvars_path}" && set',
            shell=True,
            capture_output=True,
            text=True,
            timeout=30
        )
        
        env = os.environ.copy()
        for line in result.stdout.split('\n'):
            if '=' in line:
                key, value = line.split('=', 1)
                env[key] = value
        
        return env
    except Exception as e:
        print(f"Warning: Failed to set up MSVC environment: {e}")
        return os.environ.copy()

def run_build():
    """Execute the CMake build."""
    os.chdir(r'X:\dev\artifactstudio')
    
    build_dir = r'out\build\x64-Debug'
    target = 'ArtifactCore'
    cmake_exe = r'C:\Program Files\CMake\bin\cmake.exe'
    
    print("=" * 90)
    print("ArtifactCore Build - Qt Module Purview Compilation Check")
    print("=" * 90)
    print()
    print(f"Build directory: {build_dir}")
    print(f"Target: {target}")
    print(f"CMake: {cmake_exe}")
    print()
    
    # Set up environment
    env = setup_msvc_env()
    
    # Build command
    cmd = [
        cmake_exe,
        '--build', build_dir,
        '--target', target,
        '--verbose'
    ]
    
    print(f"Command: {' '.join(cmd)}")
    print()
    print("=" * 90)
    print("BUILD OUTPUT")
    print("=" * 90)
    print()
    
    try:
        result = subprocess.run(
            cmd,
            env=env,
            capture_output=True,
            text=True,
            timeout=900  # 15 minutes
        )
        
        # Capture output
        output = result.stdout + ('\n' + result.stderr if result.stderr else '')
        print(output)
        
        # Analyze results
        print()
        print("=" * 90)
        print("BUILD RESULT ANALYSIS")
        print("=" * 90)
        print()
        
        # Check for specific modules
        modules_to_check = [
            'IO.Async.ImageWriterManager',
            'IO.ImageExporter',
            'asio_async_file_writer'
        ]
        
        print("Target modules being compiled:")
        for module in modules_to_check:
            # Check in various forms
            found = (module in output or 
                    module.replace('.', '_') in output or
                    module.replace('.', '\\') in output or
                    'ImageWriterManager' in output or
                    'ImageExporter' in output or
                    'asio_async_file_writer' in output)
            status = "✓ Found" if found else "- Not explicitly mentioned"
            print(f"  {module}: {status}")
        
        print()
        
        if result.returncode == 0:
            print("✓✓✓ BUILD SUCCEEDED ✓✓✓")
            print()
            print("The Qt module purview fixes for IO.Async.ImageWriterManager,")
            print("IO.ImageExporter, and asio_async_file_writer compiled cleanly.")
            return 0
        else:
            print("✗✗✗ BUILD FAILED ✗✗✗")
            print(f"Exit code: {result.returncode}")
            print()
            
            # Extract error information
            lines = output.split('\n')
            error_lines = []
            
            for i, line in enumerate(lines):
                if 'error' in line.lower() or 'fatal' in line.lower() or 'failed' in line.lower():
                    # Get context
                    start = max(0, i - 2)
                    end = min(len(lines), i + 10)
                    error_lines.extend(lines[start:end])
                    if len(error_lines) > 30:
                        break
            
            if error_lines:
                print("First error block:")
                print("-" * 90)
                for line in error_lines[:30]:
                    print(line)
                print("-" * 90)
            
            return 1
            
    except subprocess.TimeoutExpired:
        print()
        print("✗✗✗ BUILD TIMEOUT ✗✗✗")
        print("Build timed out after 900 seconds (15 minutes)")
        return 2
    except Exception as e:
        print()
        print(f"✗✗✗ BUILD EXECUTION ERROR ✗✗✗")
        print(f"Exception: {e}")
        return 3

if __name__ == '__main__':
    exit_code = run_build()
    sys.exit(exit_code)
