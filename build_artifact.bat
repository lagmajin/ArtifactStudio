@echo off
cd /d "X:\dev\artifactstudio\build"
cmake --build . --target Artifact --config Release -- -k 0
