import os, re

def fix_ixx_file(path):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    lines = content.splitlines(keepends=True)
    
    # Find the export module line
    export_idx = None
    for i, line in enumerate(lines):
        if re.match(r'^export module\s', line.lstrip()):
            # only match at start of line (allowing leading whitespace)
            if line.lstrip().startswith('export module '):
                export_idx = i
                break
    
    if export_idx is None:
        return False  # No export module line found
    
    # Lines before export module = GMF
    gmf_lines = lines[:export_idx]
    
    # Find Qt includes in GMF
    qt_pattern = re.compile(r'^\s*#include\s*[<"](Q[^>"]*|wobject[^>"]*)[>"]\s*$')
    qt_in_gmf = [(i, l) for i, l in enumerate(gmf_lines) if qt_pattern.match(l)]
    
    if not qt_in_gmf:
        return False  # Nothing to fix
    
    # Extract Qt include lines from GMF
    qt_indices = {i for i, _ in qt_in_gmf}
    qt_lines = [l for i, l in qt_in_gmf]
    
    new_gmf = [l for i, l in enumerate(gmf_lines) if i not in qt_indices]
    
    # If GMF now has only empty lines and/or just 'module;\n', clean it up
    # Ensure module; is present if GMF is not empty
    non_empty_gmf = [l for l in new_gmf if l.strip() not in ('', 'module;')]
    if not non_empty_gmf:
        new_gmf = ['module;\n']  # minimal GMF
    
    # Insert Qt lines after export module line
    export_line = lines[export_idx]
    after_export = lines[export_idx + 1:]
    
    new_content_lines = new_gmf + [export_line] + ['\n'] + qt_lines + after_export
    
    new_content = ''.join(new_content_lines)
    
    if new_content == content:
        return False
    
    with open(path, 'w', encoding='utf-8') as f:
        f.write(new_content)
    
    return True

# Walk and fix
widget_dir = r'X:\Dev\ArtifactStudio\Artifact\include\Widgets'
fixed = []
skipped = []

# Already fixed files (by filename only, no path needed)
already_fixed = {
    'ArtifactTimelineWidget.ixx', 'ArtifactPlaybackControlWidget.ixx',
    'ArtifactCompositionAudioMixerWidget.ixx', 'ArtifactMainWindow.ixx',
    'ArtifactMarkdownNoteEditorWidget.ixx', 'ArtifactUndoHistoryWidget.ixx',
    'ArtifactPythonHookManagerWidget.ixx', 'ArtifactProjectManagerWidget.ixx',
    'ArtifactPropertyWidget.ixx', 'ArtifactRenderQueueManagerWidget.ixx',
    'ArtifactCompositionEditor.ixx', 'ArtifactRenderLayerEditor.ixx',
    'ArtifactSoftwareRenderInspectors.ixx', 'ArtifactRenderCenterWindow.ixx',
    'ArtifactContentsViewer.ixx', 'ArtifactDebugConsoleWidget.ixx',
    'ArtifactScrollPoC.ixx', 'ArtifactInspectorWidget.ixx',
    'ArtifactStatusBar.ixx', 'ArtifactToolBar.ixx',
    'AudioMixerWidget.ixx', 'AudioPreviewWidget.ixx',
}

for root, dirs, files in os.walk(widget_dir):
    for fname in sorted(files):
        if not fname.endswith('.ixx'):
            continue
        if fname in already_fixed:
            continue
        path = os.path.join(root, fname)
        if fix_ixx_file(path):
            fixed.append(path)
        else:
            skipped.append(path)

print(f'Fixed {len(fixed)} files, skipped {len(skipped)} files.')
print('\nFixed:')
for f in fixed:
    print(' ', f)
