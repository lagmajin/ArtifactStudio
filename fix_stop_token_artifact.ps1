# PowerShell script to remove threading headers from GMF section of .ixx files

$baseDir = "X:\Dev\ArtifactStudio\Artifact\include"
$headersToRemove = @('<thread>', '<condition_variable>', '<mutex>', '<semaphore>', '<latch>', '<barrier>')

Write-Host "Scanning for .ixx files in: $baseDir`n"

$ixxFiles = Get-ChildItem -Path $baseDir -Filter "*.ixx" -Recurse
Write-Host "Found $($ixxFiles.Count) .ixx files`n"

$modifiedCount = 0
$totalRemoved = @{}

foreach ($file in $ixxFiles) {
    # Read file with BOM detection
    $content = Get-Content -Path $file.FullName -Raw -Encoding UTF8
    $hasBOM = $content.StartsWith([char]0xFEFF)
    
    # Remove BOM for processing
    if ($hasBOM) {
        $content = $content.Substring(1)
    }
    
    $lines = $content -split "`n"
    
    # Find export module line
    $exportLineIdx = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match 'export module ') {
            $exportLineIdx = $i
            break
        }
    }
    
    if ($exportLineIdx -eq -1) {
        continue
    }
    
    # Process GMF section (before export module)
    $newGmfLines = @()
    $removedHeaders = @()
    $modified = $false
    
    for ($i = 0; $i -lt $exportLineIdx; $i++) {
        $line = $lines[$i]
        $shouldRemove = $false
        
        foreach ($header in $headersToRemove) {
            if ($line -match "#include\s+\Q$header\E") {
                $shouldRemove = $true
                $headerName = $header -replace '<|>', ''
                $removedHeaders += $headerName
                $modified = $true
                break
            }
        }
        
        if (-not $shouldRemove) {
            $newGmfLines += $line
        }
    }
    
    # Write back if modified
    if ($modified) {
        $modifiedCount++
        $newContent = ($newGmfLines -join "`n") + "`n" + ($lines[$exportLineIdx..($lines.Count-1)] -join "`n")
        
        if ($hasBOM) {
            $newContent = [char]0xFEFF + $newContent
        }
        
        Set-Content -Path $file.FullName -Value $newContent -Encoding UTF8 -NoNewline
        
        $relPath = Resolve-Path -Path $file.FullName -Relative
        Write-Host "✓ Modified: $relPath"
        foreach ($header in $removedHeaders) {
            $totalRemoved[$header]++
            Write-Host "  - Removed: #include <$header>"
        }
    }
}

# Print summary
Write-Host "`n$('=' * 60)"
Write-Host "SUMMARY"
Write-Host "$('=' * 60)"
Write-Host "Total files processed: $($ixxFiles.Count)"
Write-Host "Files modified: $modifiedCount"

if ($totalRemoved.Count -gt 0) {
    Write-Host "`nHeaders removed:"
    $totalRemoved.GetEnumerator() | Sort-Object Name | ForEach-Object {
        Write-Host "  <$($_.Name)>: $($_.Value) occurrence(s)"
    }
} else {
    Write-Host "`nNo headers were removed."
}
