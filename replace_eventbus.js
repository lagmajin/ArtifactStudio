const fs = require('fs');
const path = require('path');

const files = [
    "X:\\Dev\\ArtifactStudio\\Artifact\\src\\Widgets\\Timeline\\ArtifactLayerPanelWidget.cpp",
    "X:\\Dev\\ArtifactStudio\\Artifact\\src\\Widgets\\Render\\ArtifactTimelineLayerTestWidget.cppm",
    "X:\\Dev\\ArtifactStudio\\Artifact\\src\\Widgets\\Render\\ArtifactRenderQueueManagerWidget.cpp",
    "X:\\Dev\\ArtifactStudio\\Artifact\\src\\Widgets\\ReactiveEventEditorWindow.cppm",
    "X:\\Dev\\ArtifactStudio\\Artifact\\src\\Widgets\\Asset\\ArtifactAssetBrowser.cppm",
    "X:\\Dev\\ArtifactStudio\\Artifact\\src\\Widgets\\ArtifactCompositionGraphWidget.cppm",
    "X:\\Dev\\ArtifactStudio\\Artifact\\src\\Widgets\\ArtifactInspectorWidget.cppm",
    "X:\\Dev\\ArtifactStudio\\Artifact\\src\\Widgets\\ArtifactCompositionAudioMixerWidget.cppm",
    "X:\\Dev\\ArtifactStudio\\Artifact\\src\\Widgets\\ArtifactProjectManagerWidget.cppm"
];

const oldText = "ArtifactCore::EventBus eventBus_;";
const newText = "ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();";

const results = [];

files.forEach(filePath => {
    try {
        // Check if file exists
        if (!fs.existsSync(filePath)) {
            results.push(`NOT FOUND: ${filePath}`);
            return;
        }
        
        // Read file as UTF-8
        const content = fs.readFileSync(filePath, 'utf-8');
        
        // Check if old text is present
        if (!content.includes(oldText)) {
            results.push(`NO MATCH: ${filePath}`);
            return;
        }
        
        // Replace the text
        const newContent = content.replace(new RegExp(oldText.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), 'g'), newText);
        
        // Write back as UTF-8
        fs.writeFileSync(filePath, newContent, 'utf-8');
        
        results.push(`UPDATED: ${filePath}`);
    } catch (error) {
        results.push(`ERROR: ${filePath} - ${error.message}`);
    }
});

console.log("\n" + "=".repeat(80));
console.log("REPLACEMENT RESULTS");
console.log("=".repeat(80));
results.forEach(result => console.log(result));

console.log("\n" + "=".repeat(80));
const successful = results.filter(r => r.startsWith("UPDATED")).length;
console.log(`Summary: ${successful}/${files.length} files successfully updated`);
console.log("=".repeat(80));
