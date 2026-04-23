#!/usr/bin/env python3
import subprocess
import sys
import os

os.chdir(r'X:\Dev\ArtifactStudio')

# Direct inline execution
exec(open('fix_nonwidget_gmf.py').read())
