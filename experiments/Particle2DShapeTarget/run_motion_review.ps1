param(
    [Parameter(Mandatory = $true)]
    [string]$Executable
)

$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path -LiteralPath $Executable).Path
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$run = Join-Path $repo ("output/particle-motion/" + [guid]::NewGuid().ToString('N'))
$previousBackend = $env:ARTIFACT_RENDER_BACKEND
$previousPlatform = $env:QT_QPA_PLATFORM
$env:QT_QPA_PLATFORM = 'offscreen'
try {
    foreach ($backend in @('d3d12', 'vulkan')) {
        $env:ARTIFACT_RENDER_BACKEND = $backend
        foreach ($shot in @('sand', 'title')) {
            $flag = if ($shot -eq 'sand') { '--capture-sand-flow' } else { '--capture-particle-title' }
            $destination = Join-Path $run "$backend/$shot"
            Write-Host "Capturing $backend $shot to $destination"
            & $exe $flag $destination
            if ($LASTEXITCODE -ne 0) {
                throw "Capture failed for $backend $shot (exit $LASTEXITCODE)"
            }
            python (Join-Path $PSScriptRoot 'analyze_motion_sequence.py') $shot $destination (Join-Path $destination 'review')
            if ($LASTEXITCODE -ne 0) {
                throw "Analysis failed for $backend $shot (exit $LASTEXITCODE)"
            }
        }
    }
    Write-Output $run
}
finally {
    $env:ARTIFACT_RENDER_BACKEND = $previousBackend
    $env:QT_QPA_PLATFORM = $previousPlatform
}
