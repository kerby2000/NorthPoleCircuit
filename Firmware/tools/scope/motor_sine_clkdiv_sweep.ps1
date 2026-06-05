param(
    [string]$Port = "COM19",
    [string]$Dividers = "1024,512,256,128,64,32,16,8,4,2,1",
    [int]$AmplitudePermille = 1000,
    [int]$DurationMs = 5000,
    [string]$Target = "AB",
    [string]$ChannelMap = "ab-physical",
    [string]$DisplayChannels = "",
    [switch]$SaveWaveform,
    [switch]$SkipPreflight
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..\..")
$captureScript = Join-Path $repoRoot "Firmware\tools\scope\motor_bridge_pwm_capture.py"

function Invoke-SerialCommand {
    param(
        [string]$PortName,
        [string]$Command,
        [int]$BaudRate = 115200,
        [int]$TimeoutMs = 1200
    )

    $serial = [System.IO.Ports.SerialPort]::new($PortName, $BaudRate)
    $serial.ReadTimeout = 100
    $serial.WriteTimeout = 500

    try {
        $serial.Open()
        $serial.DiscardInBuffer()
        $serial.Write("$Command`r`n")

        $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMs)
        $chunks = New-Object System.Text.StringBuilder

        while ([DateTime]::UtcNow -lt $deadline) {
            try {
                $chunk = $serial.ReadExisting()
                if ($chunk.Length -gt 0) {
                    [void]$chunks.Append($chunk)
                }
            } catch [System.TimeoutException] {
            }
            Start-Sleep -Milliseconds 40
        }

        return $chunks.ToString()
    } finally {
        if ($serial.IsOpen) {
            $serial.Close()
        }
        $serial.Dispose()
    }
}

function Test-ContinuousSineCommand {
    param(
        [string]$PortName,
        [string]$TargetName
    )

    Write-Host "Checking target firmware on $PortName before scope capture ..."
    $version = Invoke-SerialCommand -PortName $PortName -Command "version"
    if ($version.Trim()) {
        Write-Host $version.Trim()
    } else {
        Write-Warning "No response to version command."
    }

    $testCommand = "motor sine-scope-clkdiv 1024 1000 20 $TargetName"
    $response = Invoke-SerialCommand -PortName $PortName -Command $testCommand -TimeoutMs 1800
    if ($response.Trim()) {
        Write-Host $response.Trim()
    } else {
        Write-Warning "No response to $testCommand"
    }

    if ($response -match "bad motor command|bad motor sine|unknown command") {
        throw "Target firmware does not support 'motor sine-scope-clkdiv'. Flash Firmware\build\bringup\northpole_ch592_bringup.hex and rerun this sweep."
    }

    if (-not ($response -match "sine-scope-clkdiv|sine-scope-run-us|done|continuous=1")) {
        throw "Could not prove target support for 'motor sine-scope-clkdiv'. Response was: $($response.Trim())"
    }
}

if (-not $SkipPreflight) {
    Test-ContinuousSineCommand -PortName $Port -TargetName $Target
}

foreach ($rawDivider in $Dividers.Split(",")) {
    $dividerText = $rawDivider.Trim()
    if (-not $dividerText) {
        continue
    }

    $divider = [int]$dividerText
    Write-Host "Capturing NorthPole sine scope waveform with fast_clkdiv=$divider ..."

    $argsList = @(
        $captureScript,
        "--port", $Port,
        "--mode", "sine-scope-clkdiv",
        "--sine-target", $Target,
        "--channel-map", $ChannelMap,
        "--fast-clkdiv", "$divider",
        "--amplitude-permille", "$AmplitudePermille",
        "--duration-ms", "$DurationMs",
        "--acquire-mode", "run-stop",
        "--scope-horizontal-offset-div", "0",
        "--scope-acquire-mode", "sample",
        "--scope-memory-depth", "5000",
        "--pair-overlay",
        "--verbose-shell"
    )

    if ($DisplayChannels.Trim()) {
        $argsList += @("--display-channels", $DisplayChannels.Trim())
    }

    if (-not $SaveWaveform) {
        $argsList += "--no-waveform"
    }

    & python @argsList
    if ($LASTEXITCODE -ne 0) {
        throw "Capture failed for fast_clkdiv=$divider"
    }
}
