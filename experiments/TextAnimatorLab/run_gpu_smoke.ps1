param(
    [string]$Text = 'Text Sample1',
    [string]$Output = 'gpu_text_sample1.png'
)

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$exe = Join-Path $repo 'build_gpu_text_clean\Artifact\Debug\ArtifactTextGlyphSmoke.exe'
$contractSources = @(
    (Join-Path $repo 'ArtifactCore\src\Text\GlyphAtlas.cppm'),
    (Join-Path $repo 'ArtifactCore\src\Text\TextShapingBackend.cppm'),
    (Join-Path $repo 'Artifact\src\Render\ShaderManager.cppm'),
    (Join-Path $repo 'Artifact\src\Render\ArtifactTextGlyphShaderSources.cppm'),
    (Join-Path $repo 'Artifact\src\Render\ArtifactTextGlyphSubmitter.cppm')
)
$vcpkgCandidates = @(
    (Join-Path $repo 'build_gpu_text_clean\vcpkg_installed\x64-windows'),
    (Join-Path $repo 'out\vcpkg_installed\x64-windows')
)
$vcpkg = $vcpkgCandidates | Where-Object {
    Test-Path -LiteralPath (Join-Path $_ 'debug\Qt6\plugins\platforms\qwindowsd.dll')
} | Select-Object -First 1
if (-not $vcpkg) {
    throw "Qt Debug QPA plugin not found in: $($vcpkgCandidates -join ', ')"
}
$buildCandidates = @((Join-Path $repo 'build_gpu_text_clean\Artifact\Debug'))
$build = $buildCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1

if (-not (Test-Path -LiteralPath $exe)) {
    throw "GPU smoke executable not found: $exe"
}

$newestContract = ($contractSources | ForEach-Object { (Get-Item -LiteralPath $_).LastWriteTime } | Measure-Object -Maximum).Maximum
if ((Get-Item -LiteralPath $exe).LastWriteTime -lt $newestContract) {
    throw "GPU smoke executable is stale relative to text renderer sources: $exe"
}

$platforms = Join-Path $vcpkg 'debug\Qt6\plugins\platforms'
if (-not (Test-Path -LiteralPath (Join-Path $platforms 'qwindowsd.dll'))) {
    throw "Qt Windows QPA plugin not found: $platforms"
}

$env:QT_QPA_PLATFORM = 'windows'
$env:QT_QPA_PLATFORM_PLUGIN_PATH = $platforms
$env:PATH = "$(Join-Path $vcpkg 'debug\bin');$build;$env:PATH"

$outputPath = if ([System.IO.Path]::IsPathRooted($Output)) {
    $Output
} else {
    Join-Path (Get-Location) $Output
}
& $exe $Text $outputPath
exit $LASTEXITCODE
