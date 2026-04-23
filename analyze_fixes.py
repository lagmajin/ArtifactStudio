#!/usr/bin/env python3
"""Analyze what fix_nonwidget_gmf.py would fix"""
import os
import re

def would_fix_ixx_file(path):
    """Check if file would be fixed (returns True if would change)"""
    try:
        with open(path, 'r', encoding='utf-8', errors='replace') as f:
            content = f.read()
    except:
        return False, "read_error"
    
    lines = content.splitlines(keepends=True)
    export_idx = None
    for i, line in enumerate(lines):
        if line.lstrip().startswith('export module '):
            export_idx = i
            break
    
    if export_idx is None:
        return False, "no_export"
    
    gmf_lines = lines[:export_idx]
    qt_pattern = re.compile(r'^\s*#include\s*[<"](Q[^>"]*|wobject[^>"]*)[>"]\s*(\r?\n)?$')
    qt_in_gmf = [(i, l) for i, l in enumerate(gmf_lines) if qt_pattern.match(l)]
    
    if not qt_in_gmf:
        return False, "no_qt_in_gmf"
    
    return True, "has_qt_in_gmf"

already_fixed = {
    'ArtifactApplicationManager.ixx', 'ArtifactIRenderer.ixx', 'DiligentDeviceManager.ixx',
    'TreeFilterProxyModel.ixx', 'ArtifactProjectModel.ixx', 'ArtifactTimelineIconModel.ixx',
    'AssetMenuModel.ixx', 'AssetDirectoryModel.ixx', 'ArtifactHierarchyModel.ixx',
    'ClipboardManager.ixx', 'WindowStyleCSS.ixx', 'RotoMaskEditor.ixx',
}

fixed_list = []
skipped_already = []
skipped_clean = []
errors = []

def process_dir(base_dir, exclude_subdir=None):
    for root, dirs, files in os.walk(base_dir):
        if exclude_subdir:
            norm_root = os.path.normpath(root)
            norm_excl = os.path.normpath(os.path.join(base_dir, exclude_subdir))
            if norm_root.startswith(norm_excl):
                continue
        for fname in sorted(files):
            if not fname.endswith('.ixx'):
                continue
            if fname in already_fixed:
                skipped_already.append(fname)
                continue
            path = os.path.join(root, fname)
            try:
                would_fix, reason = would_fix_ixx_file(path)
                if would_fix:
                    fixed_list.append((path, reason))
                else:
                    skipped_clean.append((path, reason))
            except Exception as e:
                errors.append((path, str(e)))

os.chdir(r'X:\Dev\ArtifactStudio')
process_dir(r'X:\Dev\ArtifactStudio\Artifact\include', exclude_subdir='Widgets')
process_dir(r'X:\Dev\ArtifactStudio\ArtifactCore\include')

print(f'Would Fix: {len(fixed_list)} files')
print(f'Skipped (already fixed): {len(skipped_already)} files')
print(f'Skipped (no Qt in GMF): {len(skipped_clean)} files')
print(f'Errors: {len(errors)}')

print('\nFiles to be fixed:')
for path, _ in fixed_list[:20]:  # Show first 20
    print(f'  {os.path.basename(path)}')

if len(fixed_list) > 20:
    print(f'  ... and {len(fixed_list) - 20} more')

if errors:
    print('\nERRORS:')
    for path, err in errors[:10]:
        print(f'  {os.path.basename(path)}: {err}')
