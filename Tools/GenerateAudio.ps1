param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\Media\Audio')
)

# Compatibility wrapper retained because an earlier interrupted generator may
# still have this file open. The compiled generator below is the maintained path.
& (Join-Path $PSScriptRoot 'GenerateAudioAssets.ps1') -OutputDirectory $OutputDirectory
return

$sampleRate = 44100
$random = [System.Random]::new(4210)
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

function Write-MonoWave {
    param([string]$Path, [double[]]$Samples)

    $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Create)
    $writer = [System.IO.BinaryWriter]::new($stream)
    try {
        $dataBytes = $Samples.Length * 2
        $writer.Write([System.Text.Encoding]::ASCII.GetBytes('RIFF'))
        $writer.Write([int](36 + $dataBytes))
        $writer.Write([System.Text.Encoding]::ASCII.GetBytes('WAVEfmt '))
        $writer.Write([int]16)
        $writer.Write([int16]1)
        $writer.Write([int16]1)
        $writer.Write([int]$sampleRate)
        $writer.Write([int]($sampleRate * 2))
        $writer.Write([int16]2)
        $writer.Write([int16]16)
        $writer.Write([System.Text.Encoding]::ASCII.GetBytes('data'))
        $writer.Write([int]$dataBytes)
        foreach ($sample in $Samples) {
            $clamped = [Math]::Max(-1.0, [Math]::Min(1.0, $sample))
            $writer.Write([int16]($clamped * 32767.0))
        }
    }
    finally {
        $writer.Dispose()
        $stream.Dispose()
    }
}

function New-Samples([double]$seconds) {
    return [double[]]::new([int]($seconds * $sampleRate))
}

# Seamless synthetic rain: many periodic components plus a quiet low rumble.
$rainSeconds = 8.0
$rain = New-Samples $rainSeconds
$partials = 42
$cycles = @()
$phases = @()
$levels = @()
for ($p = 0; $p -lt $partials; $p++) {
    $cycles += $random.Next(37, 1900)
    $phases += $random.NextDouble() * 2.0 * [Math]::PI
    $levels += 0.6 + $random.NextDouble() * 0.4
}
for ($i = 0; $i -lt $rain.Length; $i++) {
    $position = $i / $rain.Length
    $value = 0.0
    for ($p = 0; $p -lt $partials; $p++) {
        $value += [Math]::Sin(2.0 * [Math]::PI * $cycles[$p] * $position + $phases[$p]) * $levels[$p]
    }
    $rain[$i] = 0.055 * $value / [Math]::Sqrt($partials) +
        0.035 * [Math]::Sin(2.0 * [Math]::PI * 56.0 * $i / $sampleRate)
}
Write-MonoWave (Join-Path $OutputDirectory 'rain.wav') $rain

$blackout = New-Samples 1.4
for ($i = 0; $i -lt $blackout.Length; $i++) {
    $t = $i / $sampleRate
    $envelope = [Math]::Exp(-2.6 * $t)
    $noise = $random.NextDouble() * 2.0 - 1.0
    $blackout[$i] = $envelope * (0.44 * [Math]::Sin(2.0 * [Math]::PI * (72.0 - 18.0 * $t) * $t) + 0.11 * $noise)
}
Write-MonoWave (Join-Path $OutputDirectory 'blackout.wav') $blackout

$correct = New-Samples 0.78
for ($i = 0; $i -lt $correct.Length; $i++) {
    $t = $i / $sampleRate
    $attack = [Math]::Min(1.0, $t * 35.0)
    $envelope = $attack * [Math]::Exp(-3.8 * $t)
    $correct[$i] = $envelope * (0.34 * [Math]::Sin(2.0 * [Math]::PI * 523.25 * $t) +
        0.23 * [Math]::Sin(2.0 * [Math]::PI * 659.25 * $t))
}
Write-MonoWave (Join-Path $OutputDirectory 'correct.wav') $correct

$incorrect = New-Samples 0.95
for ($i = 0; $i -lt $incorrect.Length; $i++) {
    $t = $i / $sampleRate
    $envelope = [Math]::Min(1.0, $t * 25.0) * [Math]::Exp(-2.5 * $t)
    $incorrect[$i] = $envelope * (0.36 * [Math]::Sin(2.0 * [Math]::PI * 110.0 * $t) +
        0.30 * [Math]::Sin(2.0 * [Math]::PI * 103.0 * $t))
}
Write-MonoWave (Join-Path $OutputDirectory 'incorrect.wav') $incorrect

Write-Output "Generated original mono WAV assets in $OutputDirectory"
