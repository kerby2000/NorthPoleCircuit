param(
    [ValidateSet("bringup", "production")]
    [string]$Profile = "bringup",

    [ValidateSet("mounriver-gui", "openocd")]
    [string]$Method = "mounriver-gui",

    [string]$ImagePath = "",

    [string]$MounRiverRoot = "C:\MounRiver\MounRiver_Studio2"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$BuildDir = Join-Path $RepoRoot "Firmware\build\$Profile"

if ([string]::IsNullOrWhiteSpace($ImagePath)) {
    $elf = Join-Path $BuildDir "northpole_ch592_bringup.elf"
    $hex = Join-Path $BuildDir "northpole_ch592_bringup.hex"
} else {
    $elf = $ImagePath
    $hex = $ImagePath
}

if ($Method -eq "mounriver-gui") {
    if (!(Test-Path -LiteralPath $hex)) {
        Write-Error "Built HEX not found: $hex. Run Firmware\tools\build.ps1 first."
    }

    Write-Host "Built image: $hex"
    Write-Host "Headless MounRiver flashing is not configured in this repo yet."
    Write-Host "Open/import Firmware\northpole_ch592_bringup in MounRiver Studio, select the obj profile, then use Download."
    exit 2
}

if (!(Test-Path -LiteralPath $elf)) {
    Write-Error "Built ELF not found: $elf. Run Firmware\tools\build.ps1 first."
}

$openocdDir = Join-Path $MounRiverRoot "resources\app\resources\win32\components\WCH\OpenOCD\OpenOCD\bin"
$openocd = Join-Path $openocdDir "openocd.exe"
$cfg = Join-Path $openocdDir "wch-riscv.cfg"

if (!(Test-Path -LiteralPath $openocd)) {
    Write-Error "OpenOCD not found: $openocd"
}
if (!(Test-Path -LiteralPath $cfg)) {
    Write-Error "WCH RISC-V OpenOCD config not found: $cfg"
}

Write-Host "Flashing $elf with WCH OpenOCD. Confirm the board is connected through a supported WCH-Link path."
& $openocd -f $cfg -c "program $elf verify reset exit"
if ($LASTEXITCODE -ne 0) {
    Write-Error "OpenOCD flash failed"
}
