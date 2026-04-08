#!/usr/bin/env python3
"""
Direct CMake build executor for ArtifactCore
Uses subprocess to call cmake directly without shell dependence
"""

import subprocess
import sys
import os
import time

def main():
    # Change to the project directory
    project_dir = r'X:\dev\artifactstudio'
    os.chdir(project_dir)
    
    # Paths
    cmake_path = r'C:\Program Files\CMake\bin\cmake.exe'
    vcvars_path = r'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
    build_dir = r'out\build\x64-Debug'
    
    print("=" * 100)
    print("ArtifactCore Build - Qt Module Purview Compilation Check")
    print("=" * 100)
    print(f"Project directory: {project_dir}")
    print(f"Build directory: {build_dir}")
    print(f"CMake executable: {cmake_path}")
    print()
    
    # First, try to set up the MSVC environment
    print("Setting up MSVC environment...")
    try:
        # Create a temporary batch file that sets up the environment and prints it
        temp_env_batch = r'X:\dev\artifactstudio\temp_env_setup.bat'
        with open(temp_env_batch, 'w') as f:
            f.write(f'@echo off\n')
            f.write(f'call "{vcvars_path}"\n')
            f.write(f'set\n')
        
        # Run it and capture the environment
        result = subprocess.run(
            ['cmd.exe', '/c', temp_env_batch],
            capture_output=True,
            text=True,
            timeout=30
        )
        
        # Build the environment
        env = os.environ.copy()
        for line in result.stdout.split('\n'):
            if '=' in line and not line.startswith(' '):
                try:
                    key, value = line.split('=', 1)
                    env[key.strip()] = value.strip()
                except:
                    pass
        
        # Clean up temp file
        try:
            os.remove(temp_env_batch)
        except:
            pass
        
        print("✓ MSVC environment configured")
    except Exception as e:
        print(f"⚠ Warning: Could not set up MSVC environment: {e}")
        env = os.environ.copy()
    
    print()
    print("=" * 100)
    print("STARTING CMAKE BUILD")
    print("=" * 100)
    print()
    
    # Build command - using cmake directly
    build_cmd = [
        cmake_path,
        '--build', build_dir,
        '--target', 'ArtifactCore',
        '--verbose'
    ]
    
    print(f"Command: {' '.join(build_cmd)}")
    print()
    
    start_time = time.time()
    
    try:
        # Run the build
        result = subprocess.run(
            build_cmd,
            cwd=project_dir,
            env=env,
            capture_output=True,
            text=True,
            timeout=1200  # 20 minute timeout
        )
        
        elapsed = time.time() - start_time
        
        # Print the output
        print(result.stdout)
        if result.stderr:
            print("STDERR:")
            print(result.stderr)
        
        print()
        print("=" * 100)
        print("BUILD RESULT")
        print("=" * 100)
        print()
        
        if result.returncode == 0:
            print("✓✓✓ SUCCESS ✓✓✓")
            print(f"Build completed successfully in {elapsed:.1f} seconds")
            print()
            print("The following Qt modules with purview fixes compiled cleanly:")
            print("  • IO.Async.ImageWriterManager")
            print("  • IO.ImageExporter")
            print("  • asio_async_file_writer")
            return 0
        else:
            print("✗✗✗ FAILURE ✗✗✗")
            print(f"Build failed with exit code {result.returncode} after {elapsed:.1f} seconds")
            print()
            
            # Try to find and display the error
            full_output = result.stdout + '\n' + (result.stderr or '')
            lines = full_output.split('\n')
            
            # Look for error lines
            error_indices = []
            for i, line in enumerate(lines):
                if 'error' in line.lower() or 'fatal' in line.lower():
                    error_indices.append(i)
            
            if error_indices:
                print("First error block:")
                print("-" * 100)
                start_idx = max(0, error_indices[0] - 3)
                end_idx = min(len(lines), error_indices[0] + 20)
                for i in range(start_idx, end_idx):
                    print(lines[i])
                print("-" * 100)
            
            return 1
    
    except subprocess.TimeoutExpired:
        elapsed = time.time() - start_time
        print("✗✗✗ TIMEOUT ✗✗✗")
        print(f"Build timed out after {elapsed:.1f} seconds")
        return 2
    except Exception as e:
        print("✗✗✗ EXCEPTION ✗✗✗")
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        return 3

if __name__ == '__main__':
    try:
        exit_code = main()
        sys.exit(exit_code)
    except KeyboardInterrupt:
        print("\n\n✗ Build interrupted by user")
        sys.exit(130)
    except Exception as e:
        print(f"\n\n✗ Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
