@echo off
setlocal enabledelayedexpansion
cd /d "X:\dev\artifactstudio"

REM Use Python to run the build since direct shell commands aren't available
python -c "
import subprocess
import sys

result = subprocess.run(
    ['cmake', '--build', 'build', '--target', 'Artifact', '--config', 'Debug'],
    capture_output=False
)
sys.exit(result.returncode)
"

pause
