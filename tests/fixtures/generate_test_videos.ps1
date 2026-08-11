param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot "generated")
)

$ErrorActionPreference = "Stop"

if (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) {
    throw "ffmpeg was not found on PATH. Install FFmpeg or pass a shell with ffmpeg available."
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

foreach ($duration in @(10, 20, 30)) {
    $outputPath = Join-Path $OutputDirectory ("ffmpeg_test_{0}s.mp4" -f $duration)
    & ffmpeg -y `
        -f lavfi -i "testsrc2=size=1280x720:rate=30" `
        -t $duration `
        -an `
        -pix_fmt yuv420p `
        -c:v libx264 `
        -preset veryfast `
        -crf 23 `
        -movflags +faststart `
        $outputPath

    if ($LASTEXITCODE -ne 0) {
        throw "ffmpeg failed while generating $outputPath"
    }
}

Write-Host "Generated 10s, 20s, and 30s MP4 fixtures in $OutputDirectory"
