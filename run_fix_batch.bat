@echo off
REM Run the fix_nonwidget_gmf.py script and capture output
cd /d X:\Dev\ArtifactStudio
python fix_nonwidget_gmf.py > fix_output.txt 2>&1
echo Script execution complete. Output saved to fix_output.txt
type fix_output.txt
