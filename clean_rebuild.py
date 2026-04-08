#!/usr/bin/env python3
"""
Clean rebuild script: removes stale build artifacts and reconfigures CMake
"""
import os
import shutil
import subprocess
import sys

def run_command(cmd, description=""):
    """Run a command and return success status"""
    if description:
        print(f"\n[{description}]")
    print(f"Running: {' '.join(cmd)}")
    try:
        result = subprocess.run(cmd, check=True)
        return result.returncode == 0
    except subprocess.CalledProcessError as e:
        print(f"ERROR: Command failed with return code {e.returncode}")
        return False
    except Exception as e:
        print(f"ERROR: {e}")
        return False

def main():
    os.chdir(r"X:\Dev\ArtifactStudio")
    
    # Remove stale build directory
    build_dir = r"out\build\x64-Debug"
    if os.path.exists(build_dir):
        print(f"[Removing stale build directory: {build_dir}]")
        try:
            shutil.rmtree(build_dir)
            print("Build directory removed successfully")
        except Exception as e:
            print(f"Warning: Could not remove build directory: {e}")
    
    # Reconfigure CMake
    if not run_command(["cmake", "--preset", "x64-Debug"], "Reconfiguring CMake"):
        print("CMake configuration failed!")
        return False
    
    # Build
    if not run_command(["cmake", "--build", build_dir], "Building Artifact"):
        print("Build failed - see output above for details")
        return False
    
    print("\n[Clean rebuild completed successfully]")
    return True

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
