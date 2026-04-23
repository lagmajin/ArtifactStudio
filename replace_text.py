import os

files_to_update = [
    r"X:\Dev\ArtifactStudio\Artifact\src\Widgets\Timeline\ArtifactLayerPanelWidget.cpp",
    r"X:\Dev\ArtifactStudio\Artifact\src\Widgets\Render\ArtifactTimelineLayerTestWidget.cppm",
    r"X:\Dev\ArtifactStudio\Artifact\src\Widgets\Render\ArtifactRenderQueueManagerWidget.cpp",
    r"X:\Dev\ArtifactStudio\Artifact\src\Widgets\ReactiveEventEditorWindow.cppm",
    r"X:\Dev\ArtifactStudio\Artifact\src\Widgets\Asset\ArtifactAssetBrowser.cppm",
    r"X:\Dev\ArtifactStudio\Artifact\src\Widgets\ArtifactCompositionGraphWidget.cppm",
    r"X:\Dev\ArtifactStudio\Artifact\src\Widgets\ArtifactInspectorWidget.cppm",
    r"X:\Dev\ArtifactStudio\Artifact\src\Widgets\ArtifactCompositionAudioMixerWidget.cppm",
    r"X:\Dev\ArtifactStudio\Artifact\src\Widgets\ArtifactProjectManagerWidget.cppm",
]

old_text = "ArtifactCore::EventBus eventBus_;"
new_text = "ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();"

# Convert to bytes
old_bytes = old_text.encode('utf-8')
new_bytes = new_text.encode('utf-8')

results = []

for file_path in files_to_update:
    try:
        # Check if file exists
        if not os.path.exists(file_path):
            results.append(f"NOT FOUND: {file_path}")
            continue
        
        # Read file in binary mode
        with open(file_path, 'rb') as f:
            content = f.read()
        
        # Check if old text is present
        if old_bytes not in content:
            results.append(f"NO MATCH: {file_path}")
            continue
        
        # Replace the bytes
        new_content = content.replace(old_bytes, new_bytes)
        
        # Write back in binary mode (preserves encoding and line endings)
        with open(file_path, 'wb') as f:
            f.write(new_content)
        
        results.append(f"UPDATED: {file_path}")
    
    except Exception as e:
        results.append(f"ERROR: {file_path} - {str(e)}")

# Print results
print("\n" + "="*80)
print("REPLACEMENT RESULTS")
print("="*80)
for result in results:
    print(result)

print("\n" + "="*80)
successful = sum(1 for r in results if r.startswith("UPDATED"))
print(f"Summary: {successful}/{len(files_to_update)} files successfully updated")
print("="*80)
