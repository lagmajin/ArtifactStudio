import os, re

def fix_ixx_file(path):
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()
    
    lines = content.splitlines(keepends=True)
    
    # Find the export module line
    export_idx = None
    for i, line in enumerate(lines):
        stripped = line.lstrip()
        if stripped.startswith('export module '):
            export_idx = i
            break
    
    if export_idx is None:
        return False  # No export module line found
    
    # Lines before export module = GMF
    gmf_lines = lines[:export_idx]
    
    # Find Qt includes in GMF
    qt_pattern = re.compile(r'^\s*#include\s*[<"](Q[^>"]*|wobject[^>"]*)[>"]\s*(\r?\n)?$')
    qt_in_gmf = [(i, l) for i, l in enumerate(gmf_lines) if qt_pattern.match(l)]
    
    if not qt_in_gmf:
        return False  # Nothing to fix
    
    # Extract Qt include lines from GMF
    qt_indices = {i for i, _ in qt_in_gmf}
    qt_lines = [l for i, l in qt_in_gmf]
    
    new_gmf = [l for i, l in enumerate(gmf_lines) if i not in qt_indices]
    
    # Ensure module; is present if GMF is not empty
    non_empty_gmf = [l for l in new_gmf if l.strip() not in ('', 'module;')]
    if not non_empty_gmf:
        new_gmf = ['module;\n']  # minimal GMF
    
    # Insert Qt lines after export module line
    export_line = lines[export_idx]
    after_export = lines[export_idx + 1:]
    
    # Add blank line separator after export module if not present, then Qt lines
    separator = ['\n'] if after_export and after_export[0].strip() else []
    new_content_lines = new_gmf + [export_line] + separator + qt_lines + after_export
    
    new_content = ''.join(new_content_lines)
    
    if new_content == content:
        return False
    
    with open(path, 'w', encoding='utf-8') as f:
        f.write(new_content)
    
    return True

# Already fixed (skip by filename)
already_fixed = {
    'ArtifactApplicationManager.ixx', 'ArtifactIRenderer.ixx', 'DiligentDeviceManager.ixx',
    'TreeFilterProxyModel.ixx', 'ArtifactProjectModel.ixx', 'ArtifactTimelineIconModel.ixx',
    'AssetMenuModel.ixx', 'AssetDirectoryModel.ixx', 'ArtifactHierarchyModel.ixx',
    'ClipboardManager.ixx', 'WindowStyleCSS.ixx', 'RotoMaskEditor.ixx',
}

fixed = []
skipped_already = []
skipped_clean = []
errors = []

def process_dir(base_dir, exclude_subdir=None):
    for root, dirs, files in os.walk(base_dir):
        # Exclude Widgets directory
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
                if fix_ixx_file(path):
                    fixed.append(path)
                else:
                    skipped_clean.append(path)
            except Exception as e:
                errors.append((path, str(e)))

process_dir(r'X:\Dev\ArtifactStudio\Artifact\include', exclude_subdir='Widgets')
process_dir(r'X:\Dev\ArtifactStudio\ArtifactCore\include')

print(f'Fixed: {len(fixed)} files')
print(f'Skipped (already fixed): {len(skipped_already)} files')
print(f'Skipped (no Qt in GMF): {len(skipped_clean)} files')
print(f'Errors: {len(errors)}')
if errors:
    print('ERRORS:')
    for path, err in errors:
        print(f'  {path}: {err}')
print('\nModified files:')
for f in fixed:
    print(' ', f)
