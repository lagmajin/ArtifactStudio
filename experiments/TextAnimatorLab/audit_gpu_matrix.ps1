$ErrorActionPreference = 'Continue'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$runner = Join-Path $PSScriptRoot 'run_gpu_smoke.ps1'
$exe = Join-Path $repo 'build_gpu_text_standalone\Debug\ArtifactRenderTextSmokeStandalone.exe'
$contractSources = @(
    (Join-Path $repo 'ArtifactCore\src\Text\GlyphAtlas.cppm'),
    (Join-Path $repo 'Artifact\src\Render\ShaderManager.cppm'),
    (Join-Path $repo 'Artifact\src\Render\DiligentImmediateSubmitter.cppm')
)
$artifactLibraries = @(
    (Join-Path $repo 'out\build\x64-Debug\Artifact\ArtifactRender.lib'),
    (Join-Path $repo 'out\build\x64-Debug\ArtifactCore\ArtifactCore.lib')
)
$newestContract = ($contractSources | ForEach-Object { (Get-Item -LiteralPath $_).LastWriteTime } | Measure-Object -Maximum).Maximum
$binaryFresh = (Test-Path -LiteralPath $exe) -and ((Get-Item -LiteralPath $exe).LastWriteTime -ge $newestContract) -and (($artifactLibraries | ForEach-Object { (Test-Path -LiteralPath $_) -and ((Get-Item -LiteralPath $_).LastWriteTime -ge $newestContract) } | Where-Object { -not $_ }).Count -eq 0)
$fixtures = @(
    @{ id = 'latin'; file = 'fixtures\gpu_unicode_latin.txt'; category = 'monochrome' },
    @{ id = 'cjk'; file = 'fixtures\gpu_unicode_cjk.txt'; category = 'monochrome' },
    @{ id = 'emoji'; file = 'fixtures\gpu_unicode_emoji.txt'; category = 'color-required' },
    @{ id = 'zwj'; file = 'fixtures\gpu_unicode_zwj.txt'; category = 'color-required' }
)

$results = foreach ($fixture in $fixtures) {
    $input = Join-Path $PSScriptRoot $fixture.file
    $output = Join-Path $repo "gpu_audit_$($fixture.id).png"
    $log = & powershell -ExecutionPolicy Bypass -File $runner -Text "@$input" -Output $output 2>&1 | Out-String
    $smokeLine = ($log.Trim() -split "`r?`n" | Select-String 'gpu-smoke:' | ForEach-Object Line) -join "`n"
    $renderedSuccessfully = ($smokeLine -match 'image=(\d+)x(\d+) saved=1' -and [int]$Matches[1] -gt 0 -and [int]$Matches[2] -gt 0)
    [pscustomobject]@{
        id = $fixture.id
        category = $fixture.category
        rendered = (Test-Path -LiteralPath $output) -and $renderedSuccessfully
        colorAtlasRequired = ($fixture.category -eq 'color-required')
        # A color-required sample must not be considered product-ready merely
        # because a fallback square was rasterized into a PNG.
        binaryFresh = $binaryFresh
        passed = $binaryFresh -and $renderedSuccessfully -and ($fixture.category -ne 'color-required')
        status = if (-not (Test-Path -LiteralPath $artifactLibraries[0]) -or -not (Test-Path -LiteralPath $artifactLibraries[1])) { 'missing-library' } elseif (-not $binaryFresh) { 'stale-library-or-binary' } elseif (-not $renderedSuccessfully) { 'render-failed' } elseif ($fixture.category -eq 'color-required') { 'needs-color-atlas' } else { 'pass' }
        output = $output
        log = $smokeLine
    }
}

$results | ConvertTo-Json -Depth 4
