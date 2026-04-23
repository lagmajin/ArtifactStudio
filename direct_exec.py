#!/usr/bin/env python3
"""Direct execution of fix_nonwidget_gmf.py"""
import os
import sys

# Change to the artifact studio directory
os.chdir(r'X:\Dev\ArtifactStudio')

# Now exec the script directly
with open('fix_nonwidget_gmf.py', 'r') as f:
    code = f.read()
    exec(code)
