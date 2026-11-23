param(
    [Parameter(Mandatory = $true)]
    [string]$Executable
)

$ErrorActionPreference = 'Stop'
$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$outputRoot = Join-Path (Get-Location) 'output/particle-shape-target'
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null

& $resolvedExecutable --check-shape-target-persistence
if ($LASTEXITCODE -ne 0) {
    throw "Particle shape-target persistence check failed with exit code $LASTEXITCODE"
}

foreach ($backend in @('vulkan', 'd3d12')) {
    $env:ARTIFACT_RENDER_BACKEND = $backend
    $destination = Join-Path $outputRoot $backend
    & $resolvedExecutable --capture-shape-target $destination
    if ($LASTEXITCODE -ne 0) {
        throw "Particle shape-target $backend capture failed with exit code $LASTEXITCODE"
    }
}

Remove-Item Env:ARTIFACT_RENDER_BACKEND -ErrorAction SilentlyContinue
