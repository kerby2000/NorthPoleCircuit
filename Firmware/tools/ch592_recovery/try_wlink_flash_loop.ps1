param(
    [string]$HexPath = "Firmware\mounriver_ch592_led_probe\obj\mounriver_ch592_led_probe.hex",
    [int]$Attempts = 30,
    [switch]$EraseFirst,
    [switch]$NoRun
)

$ErrorActionPreference = "Continue"
$wlink = Join-Path $env:USERPROFILE ".platformio\packages\tool-wlink\wlink.exe"

if (-not (Test-Path $wlink)) {
    throw "wlink.exe not found: $wlink"
}

if (-not (Test-Path $HexPath)) {
    throw "HEX file not found: $HexPath"
}

Write-Host "Trying WCH-Link flash loop"
Write-Host "Tool: $wlink"
Write-Host "HEX:  $HexPath"
Write-Host "Attempts: $Attempts"
Write-Host "EraseFirst: $EraseFirst"
Write-Host "NoRun: $NoRun"
Write-Host ""

for ($i = 1; $i -le $Attempts; $i++) {
    Write-Host "=== attempt $i / $Attempts ==="

    if ($EraseFirst -and $i -eq 1) {
        & $wlink erase --chip CH59X --speed low --method default
        Write-Host "erase exit code: $LASTEXITCODE"
    }

    $args = @("flash", "--chip", "CH59X", "--speed", "low")
    if ($NoRun) {
        $args += "-R"
    }
    $args += $HexPath

    & $wlink @args
    $code = $LASTEXITCODE
    Write-Host "flash exit code: $code"

    if ($code -eq 0) {
        Write-Host "WCH-Link flash loop succeeded on attempt $i"
        exit 0
    }

    Start-Sleep -Milliseconds 250
}

Write-Host "WCH-Link flash loop failed after $Attempts attempts"
exit 1
