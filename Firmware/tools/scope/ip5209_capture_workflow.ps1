param(
    [string]$FirmwareImage = "Firmware\build\bringup\northpole_ch592_bringup.hex",
    [string]$BatteryVoltage = "unknown",
    [string]$UsbState = "disconnected",
    [string]$OutDir = "Firmware\docs\ip5209_scope_evidence",
    [string]$Ch1DmmV = "",
    [string]$Ch2DmmV = "",
    [string]$Ch3DmmV = "",
    [string]$Ch4DmmV = ""
)

$ErrorActionPreference = "Stop"

Write-Host "IP5209 LX scope capture workflow"
Write-Host ""
Write-Host "Confirm before continuing:"
Write-Host "  CH1 = L2 CSIN / battery side of boost inductor"
Write-Host "  CH2 = L2 LX side / IP5209 LX switching node"
Write-Host "  CH3 = +5V/VOUT"
Write-Host "  CH4 = VREG"
Write-Host "  Scope GND = local board GND near U7/IP5209"
Write-Host "  Scope ground is earth-referenced. Do not connect it to LX, VBAT, VOUT, or KEY."
Write-Host "  Start with 1x probes."
Write-Host ""

$confirm = Read-Host "Type YES when probing is safe"
if ($confirm -ne "YES") {
    throw "Aborted by user."
}

$commonArgs = @(
    "--out-dir", $OutDir,
    "--firmware-image", $FirmwareImage,
    "--battery-voltage", $BatteryVoltage,
    "--usb-state", $UsbState,
    "--ch3", "vout",
    "--ch4", "vreg"
)

if ($Ch1DmmV -eq "") { $Ch1DmmV = Read-Host "Optional DMM CH1/CSIN voltage, blank to skip" }
if ($Ch2DmmV -eq "") { $Ch2DmmV = Read-Host "Optional DMM CH2/LX voltage, blank to skip" }
if ($Ch3DmmV -eq "") { $Ch3DmmV = Read-Host "Optional DMM CH3/VOUT voltage, blank to skip" }
if ($Ch4DmmV -eq "") { $Ch4DmmV = Read-Host "Optional DMM CH4/VREG voltage, blank to skip" }

if ($Ch1DmmV -ne "") { $commonArgs += @("--ch1-dmm-v", $Ch1DmmV) }
if ($Ch2DmmV -ne "") { $commonArgs += @("--ch2-dmm-v", $Ch2DmmV) }
if ($Ch3DmmV -ne "") { $commonArgs += @("--ch3-dmm-v", $Ch3DmmV) }
if ($Ch4DmmV -ne "") { $commonArgs += @("--ch4-dmm-v", $Ch4DmmV) }

python Firmware\tools\scope\ip5209_lx_capture.py `
    --mode idle `
    @commonArgs

Write-Host ""
Write-Host "Next capture will arm the scope. Press SW3 when prompted by the Python script."
python Firmware\tools\scope\ip5209_lx_capture.py `
    --mode wake-single `
    @commonArgs

$steady = Read-Host "Run steady-switching timebase sweep now? (y/N)"
if ($steady -eq "y" -or $steady -eq "Y") {
    python Firmware\tools\scope\ip5209_lx_capture.py `
        --mode steady-switching `
        @commonArgs
}

$load = Read-Host "Connect 220 ohm load from +5V to GND and run load test? (y/N)"
if ($load -eq "y" -or $load -eq "Y") {
    python Firmware\tools\scope\ip5209_lx_capture.py `
        --mode load-test `
        --load-ohms 220 `
        @commonArgs
}

Write-Host "Done. Report: Firmware\docs\ip5209_lx_scope_report.md"
