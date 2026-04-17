#!/usr/bin/env python3
"""
Integrated Module Fixer for ArtifactStudio C++20 Modules

This script automatically fixes common C++20 module configuration issues:
- Qt headers misplaced in GMF (Global Module Fragment)
- OpenCV headers misplaced in module purview
- module ; not at the beginning of files
- Duplicate module declarations

Usage:
    python tools/fix_all_modules.py [--check-only] [--target-file FILE]
"""

import os
import re
import sys
import argparse
from pathlib import Path

class ModuleFixer:
    def __init__(self):
        self.project_root = self.find_project_root()
        self.fixed_files = []

    def find_project_root(self):
        """Find the project root by looking for CMakeLists.txt"""
        current = Path.cwd()
        for parent in [current] + list(current.parents):
            if (parent / "CMakeLists.txt").exists():
                return parent
        return Path.cwd()

    def find_ixx_files(self):
        """Find all .ixx files in the project"""
        ixx_files = []
        for root, dirs, files in os.walk(self.project_root):
            # Skip build directories and third_party
            if any(skip in root for skip in ['build', 'third_party', '.git', 'libs/DiligentEngine']):
                continue
            for file in files:
                if file.endswith('.ixx'):
                    ixx_files.append(Path(root) / file)
        return ixx_files

    def fix_file(self, file_path, check_only=False):
        """Fix a single .ixx file"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
        except UnicodeDecodeError:
            print(f"Warning: Skipping {file_path} (encoding issue)")
            return False

        original_content = content

        # Fix 1: Ensure module ; is at the beginning (after comments)
        content = self.fix_module_declaration(content)

        # Fix 2: Move Qt includes from GMF to module purview
        content = self.fix_qt_includes(content)

        # Fix 3: Move OpenCV includes from module purview to GMF
        content = self.fix_opencv_includes(content)

        # Check for duplicate module declarations (warning only)
        self.check_duplicate_modules(content, file_path)

        if content != original_content:
            if not check_only:
                with open(file_path, 'w', encoding='utf-8') as f:
                    f.write(content)
                print(f"Fixed: {file_path}")
                self.fixed_files.append(file_path)
            else:
                print(f"Would fix: {file_path}")
            return True

        return False

    def fix_module_declaration(self, content):
        """Ensure module ; is at the beginning after comments"""
        lines = content.split('\n')
        if not lines:
            return content

        # Find the first non-comment, non-empty line
        first_code_line = -1
        for i, line in enumerate(lines):
            stripped = line.strip()
            if stripped and not stripped.startswith('//'):
                first_code_line = i
                break

        if first_code_line == -1 or lines[first_code_line].strip() == 'module ;':
            return content

        # Insert module ; at the beginning
        new_lines = ['module ;'] + lines
        return '\n'.join(new_lines)

    def fix_qt_includes(self, content):
        """Move Qt includes from GMF to module purview"""
        # Pattern for GMF (before export module)
        gmf_pattern = r'(module ;\s*(?://.*\n)*)(.*?)(?=export module)'
        gmf_match = re.search(gmf_pattern, content, re.DOTALL)

        if not gmf_match:
            return content

        gmf_content = gmf_match.group(2)

        # Find Qt includes in GMF
        qt_include_pattern = r'(#include <Q[^>]+>)'
        qt_includes = re.findall(qt_include_pattern, gmf_content)

        if not qt_includes:
            return content

        # Remove Qt includes from GMF
        for include in qt_includes:
            gmf_content = gmf_content.replace(include, '')

        # Find module purview (after export module)
        purview_pattern = r'(export module [^;]+;\s*)(.*)'
        purview_match = re.search(purview_pattern, content, re.DOTALL)

        if purview_match:
            purview_start = purview_match.group(1)
            purview_content = purview_match.group(2)

            # Add Qt includes to purview
            qt_includes_text = '\n'.join(qt_includes) + '\n'
            new_purview = purview_start + qt_includes_text + purview_content
            new_gmf = gmf_match.group(1) + gmf_content + 'export module'

            # Reconstruct content
            content = new_gmf + new_purview

        return content

    def fix_opencv_includes(self, content):
        """Move OpenCV includes from module purview to GMF"""
        # Find module purview
        purview_pattern = r'export module [^;]+;(.*)'
        purview_match = re.search(purview_pattern, content, re.DOTALL)

        if not purview_match:
            return content

        purview_content = purview_match.group(1)

        # Find OpenCV includes in purview
        opencv_pattern = r'(#include <opencv2/[^>]+>)'
        opencv_includes = re.findall(opencv_pattern, purview_content)

        if not opencv_includes:
            return content

        # Remove OpenCV includes from purview
        for include in opencv_includes:
            purview_content = purview_content.replace(include, '')

        # Add to GMF
        gmf_pattern = r'(module ;\s*(?://.*\n)*)'
        gmf_match = re.search(gmf_pattern, content, re.DOTALL)

        if gmf_match:
            opencv_text = '\n'.join(opencv_includes) + '\n'
            new_gmf = gmf_match.group(1) + opencv_text
            content = content.replace(gmf_match.group(0), new_gmf)

        return content

    def check_duplicate_modules(self, content, file_path):
        """Check for duplicate module declarations"""
        module_pattern = r'export module ([^;\s]+);'
        modules = re.findall(module_pattern, content)

        if len(modules) > 1:
            print(f"Warning: Multiple module declarations in {file_path}: {modules}")

    def run(self, check_only=False, target_file=None):
        """Run the fixer on all or specific files"""
        if target_file:
            files = [Path(target_file)]
        else:
            files = self.find_ixx_files()

        total_files = len(files)
        fixed_count = 0

        print(f"Processing {total_files} .ixx files...")
        print(f"Mode: {'Check only' if check_only else 'Fix'}")
        print("-" * 50)

        for file_path in files:
            if self.fix_file(file_path, check_only):
                fixed_count += 1

        print("-" * 50)
        print(f"Processed: {total_files} files")
        print(f"Fixed: {fixed_count} files")

        if self.fixed_files and not check_only:
            print("\nFixed files:")
            for f in self.fixed_files:
                print(f"  {f}")

def main():
    parser = argparse.ArgumentParser(description="Fix C++20 module configuration issues")
    parser.add_argument('--check-only', action='store_true',
                       help='Only check for issues, do not fix')
    parser.add_argument('--target-file', type=str,
                       help='Fix only this specific file')

    args = parser.parse_args()

    fixer = ModuleFixer()
    fixer.run(check_only=args.check_only, target_file=args.target_file)

if __name__ == '__main__':
    main()