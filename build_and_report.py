#!/usr/bin/env python3
"""Build ArtifactCore and report results focusing on Qt purview module fixes."""

import subprocess
import sys
import os
import re

def run_cmake_build():
    """Run the CMake build for ArtifactCore."""
    os.chdir(r'X:\dev\artifactstudio')
    
    build_dir = r'out\build\x64-Debug'
    target = 'ArtifactCore'
    
    print("=" * 80)
    print(f"Building ArtifactCore from {build_dir}")
    print("=" * 80)
    print()
    
    cmd = [
        'cmake',
        '--build', build_dir,
        '--target', target,
        '--verbose',
        '--'
    ]
    
    try:
        # Run the build command
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=600  # 10 minute timeout
        )
        
        # Print output
        if result.stdout:
            print(result.stdout)
        if result.stderr:
            print("STDERR:")
            print(result.stderr)
        
        # Check for specific module mentions in output
        full_output = result.stdout + (result.stderr or "")
        
        modules_to_check = [
            'IO.Async.ImageWriterManager',
            'IO.ImageExporter',
            'asio_async_file_writer'
        ]
        
        print()
        print("=" * 80)
        print("MODULE COMPILATION CHECK")
        print("=" * 80)
        
        for module in modules_to_check:
            if module in full_output or module.replace('.', '\\') in full_output:
                print(f"✓ {module} - found in build output")
            else:
                print(f"- {module} - not explicitly mentioned (may still be compiled)")
        
        print()
        print("=" * 80)
        if result.returncode == 0:
            print("✓ BUILD SUCCEEDED")
            print("=" * 80)
            return 0
        else:
            print(f"✗ BUILD FAILED (exit code: {result.returncode})")
            print("=" * 80)
            
            # Try to extract the first error
            lines = full_output.split('\n')
            error_section = []
            in_error = False
            
            for line in lines:
                if 'error' in line.lower() or 'fatal' in line.lower():
                    in_error = True
                
                if in_error:
                    error_section.append(line)
                    if len(error_section) > 20:  # Capture first error + context
                        break
            
            if error_section:
                print()
                print("First error block:")
                print('\n'.join(error_section[:20]))
            
            return 1
            
    except subprocess.TimeoutExpired:
        print("ERROR: Build timed out after 600 seconds")
        return 2
    except Exception as e:
        print(f"ERROR: Failed to run build: {e}")
        return 3

if __name__ == '__main__':
    sys.exit(run_cmake_build())
