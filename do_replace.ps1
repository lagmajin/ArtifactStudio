@"
Write-Host "Starting text replacement..."

`$files = @(
    "X:\Dev\ArtifactStudio\Artifact\src\Widgets\Timeline\ArtifactLayerPanelWidget.cpp",
    "X:\Dev\ArtifactStudio\Artifact\src\Widgets\Render\ArtifactTimelineLayerTestWidget.cppm",
    "X:\Dev\ArtifactStudio\Artifact\src\Widgets\Render\ArtifactRenderQueueManagerWidget.cpp",
    "X:\Dev\ArtifactStudio\Artifact\src\Widgets\ReactiveEventEditorWindow.cppm",
    "X:\Dev\ArtifactStudio\Artifact\src\Widgets\Asset\ArtifactAssetBrowser.cppm",
    "X:\Dev\ArtifactStudio\Artifact\src\Widgets\ArtifactCompositionGraphWidget.cppm",
    "X:\Dev\ArtifactStudio\Artifact\src\Widgets\ArtifactInspectorWidget.cppm",
    "X:\Dev\ArtifactStudio\Artifact\src\Widgets\ArtifactCompositionAudioMixerWidget.cppm",
    "X:\Dev\ArtifactStudio\Artifact\src\Widgets\ArtifactProjectManagerWidget.cppm"
)

`$oldText = "ArtifactCore::EventBus eventBus_;"
`$newText = "ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();"

`$results = @()

foreach (`$filePath in `$files) {
    try {
        if (-not (Test-Path `$filePath)) {
            `$results += "NOT FOUND: `$filePath"
            continue
        }
        
        # Read file with UTF-8 encoding, preserving line endings
        `$content = Get-Content -Path `$filePath -Raw -Encoding UTF8
        
        if (`$content -notmatch [regex]::Escape(`$oldText)) {
            `$results += "NO MATCH: `$filePath"
            continue
        }
        
        # Replace the text
        `$newContent = `$content -replace [regex]::Escape(`$oldText), `$newText
        
        # Write back with UTF-8 encoding
        Set-Content -Path `$filePath -Value `$newContent -Encoding UTF8 -NoNewline
        
        `$results += "UPDATED: `$filePath"
    }
    catch {
        `$results += "ERROR: `$filePath - `$_"
    }
}

Write-Host ""
Write-Host "========================================================================"
Write-Host "REPLACEMENT RESULTS"
Write-Host "========================================================================"
foreach (`$result in `$results) {
    Write-Host `$result
}

Write-Host ""
Write-Host "========================================================================"
`$successful = (`$results | Where-Object { `$_ -like "UPDATED*" }).Count
Write-Host "Summary: `$successful/$($files.Count) files successfully updated"
Write-Host "========================================================================"
"@ | Out-File -FilePath "X:\dev\artifactstudio\replace_eventbus.ps1" -Encoding UTF8

& "X:\dev\artifactstudio\replace_eventbus.ps1"
