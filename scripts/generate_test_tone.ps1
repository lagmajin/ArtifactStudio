param(
    [string]$OutputPath = "resources/testdata/audio/test_tone_440hz_10s.wav",
    [double]$FrequencyHz = 440.0,
    [double]$DurationSeconds = 10.0,
    [int]$SampleRate = 48000,
    [double]$Amplitude = 0.25
)

$ErrorActionPreference = "Stop"

$outDir = Split-Path -Parent $OutputPath
if ($outDir -and -not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

$channels = 1
$bitsPerSample = 16
$bytesPerSample = $bitsPerSample / 8
$sampleCount = [int]([Math]::Round($DurationSeconds * $SampleRate))
$dataSize = $sampleCount * $channels * $bytesPerSample
$byteRate = $SampleRate * $channels * $bytesPerSample
$blockAlign = $channels * $bytesPerSample

$stream = [System.IO.File]::Open($OutputPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
$writer = New-Object System.IO.BinaryWriter($stream)

try {
    $writer.Write([System.Text.Encoding]::ASCII.GetBytes("RIFF"))
    $writer.Write([int](36 + $dataSize))
    $writer.Write([System.Text.Encoding]::ASCII.GetBytes("WAVE"))

    $writer.Write([System.Text.Encoding]::ASCII.GetBytes("fmt "))
    $writer.Write([int]16)
    $writer.Write([int16]1)
    $writer.Write([int16]$channels)
    $writer.Write([int]$SampleRate)
    $writer.Write([int]$byteRate)
    $writer.Write([int16]$blockAlign)
    $writer.Write([int16]$bitsPerSample)

    $writer.Write([System.Text.Encoding]::ASCII.GetBytes("data"))
    $writer.Write([int]$dataSize)

    for ($i = 0; $i -lt $sampleCount; $i++) {
        $t = $i / [double]$SampleRate
        $sample = [Math]::Sin(2.0 * [Math]::PI * $FrequencyHz * $t)
        $sample *= $Amplitude
        $pcm = [int16]([Math]::Max([Math]::Min($sample * 32767.0, 32767.0), -32768.0))
        $writer.Write($pcm)
    }
}
finally {
    $writer.Dispose()
    $stream.Dispose()
}

Write-Host "Generated test tone at $OutputPath"
