param(
    [ValidateSet("bringup", "production", "evt-baseline")]
    [string]$Profile = "bringup",

    [switch]$AuditOnly,

    [string]$SdkRoot = "C:\WCH\CH592EVT",

    [string]$MounRiverRoot = "C:\MounRiver\MounRiver_Studio2",

    [string[]]$ExtraDefine = @(),

    [string[]]$ExtraCFlag = @()
)

$ErrorActionPreference = "Stop"

function Resolve-ToolPath {
    param(
        [string]$Root,
        [string]$RelativePath,
        [string]$Description
    )

    $path = Join-Path $Root $RelativePath
    if (!(Test-Path -LiteralPath $path)) {
        Write-Error "$Description not found: $path"
    }
    return $path
}

function Get-CheckedBuildDir {
    param([string]$Name)

    $buildRoot = Join-Path $FirmwareRoot "build"
    New-Item -ItemType Directory -Force -Path $buildRoot | Out-Null
    $buildRootResolved = Resolve-Path $buildRoot
    $buildDir = [System.IO.Path]::GetFullPath((Join-Path $buildRoot $Name))

    if (!$buildDir.StartsWith($buildRootResolved.Path, [System.StringComparison]::OrdinalIgnoreCase)) {
        Write-Error "Refusing build path outside Firmware\build: $buildDir"
    }
    if (Test-Path -LiteralPath $buildDir) {
        Remove-Item -LiteralPath $buildDir -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
    return $buildDir
}

function Add-Includes {
    param([string[]]$Paths)

    $args = @()
    foreach ($path in $Paths) {
        $args += @("-I", $path)
    }
    return $args
}

function Invoke-Checked {
    param(
        [string]$Exe,
        [string[]]$Arguments,
        [string]$FailureMessage
    )

    $oldErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    & $Exe @Arguments 2>&1 | ForEach-Object { Write-Host $_ }
    $exitCode = $LASTEXITCODE
    $ErrorActionPreference = $oldErrorActionPreference

    if ($exitCode -ne 0) {
        Write-Error $FailureMessage
    }
}

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$FirmwareRoot = Join-Path $RepoRoot "Firmware"

Write-Host "Running KiCad hardware audit..."
python (Join-Path $FirmwareRoot "tools\extract_kicad_pinmap.py") --repo-root $RepoRoot

if ($AuditOnly) {
    Write-Host "Audit-only run complete."
    exit 0
}

$gccBin = Resolve-ToolPath `
    -Root $MounRiverRoot `
    -RelativePath "resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC\bin" `
    -Description "MounRiver RISC-V Embedded GCC bin directory"
$gcc = Resolve-ToolPath -Root $gccBin -RelativePath "riscv-none-embed-gcc.exe" -Description "GCC"
$objcopy = Resolve-ToolPath -Root $gccBin -RelativePath "riscv-none-embed-objcopy.exe" -Description "objcopy"
$size = Resolve-ToolPath -Root $gccBin -RelativePath "riscv-none-embed-size.exe" -Description "size"

$sdkExamRoot = Join-Path $SdkRoot "EVT\EXAM"
$bleRoot = Join-Path $sdkExamRoot "BLE"
$srcRoot = Join-Path $sdkExamRoot "SRC"

if ($Profile -eq "evt-baseline") {
    $projectRoot = Join-Path $bleRoot "Peripheral"
    $buildName = "evt_peripheral_unmodified"
    $artifact = "Peripheral"
    $profileDefine = $null
} else {
    $projectRoot = Join-Path $FirmwareRoot "northpole_ch592_bringup"
    $buildName = $Profile
    $artifact = "northpole_ch592_bringup"
    $profileDefine = if ($Profile -eq "production") { "FIRMWARE_PROFILE_PRODUCTION" } else { $null }

    $projectPinNotes = Join-Path $projectRoot "APP\include\board_pins_autogen_notes.h"
    if (!(Test-Path -LiteralPath $projectPinNotes)) {
        Write-Error "Generated board pin notes not found: $projectPinNotes. Run Firmware\tools\extract_kicad_pinmap.py first."
    }
}

foreach ($required in @($projectRoot, $bleRoot, $srcRoot)) {
    if (!(Test-Path -LiteralPath $required)) {
        Write-Error "Required path not found: $required"
    }
}

$paths = @{
    APP = Join-Path $projectRoot "APP"
    Profile = Join-Path $projectRoot "Profile"
    HAL = Join-Path $bleRoot "HAL"
    LIB = Join-Path $bleRoot "LIB"
    Ld = Join-Path $srcRoot "Ld"
    RVMSIS = Join-Path $srcRoot "RVMSIS"
    Startup = Join-Path $srcRoot "Startup"
    Std = Join-Path $srcRoot "StdPeriphDriver"
}

$includePaths = @(
    $paths.Startup,
    (Join-Path $paths.APP "include"),
    (Join-Path $paths.Profile "include"),
    (Join-Path $paths.Std "inc"),
    (Join-Path $paths.HAL "include"),
    $paths.Ld,
    $paths.LIB,
    $paths.RVMSIS
)
$includeArgs = Add-Includes $includePaths

$commonArgs = @(
    "-march=rv32imac",
    "-mabi=ilp32",
    "-mcmodel=medany",
    "-msmall-data-limit=8",
    "-Os",
    "-fmessage-length=0",
    "-fsigned-char",
    "-ffunction-sections",
    "-fdata-sections",
    "-fno-common",
    "-DDEBUG=1"
)
if ($profileDefine) {
    $commonArgs += "-D$profileDefine"
}
foreach ($define in $ExtraDefine) {
    if ($define) {
        $commonArgs += "-D$define"
    }
}
foreach ($flag in $ExtraCFlag) {
    if ($flag) {
        $commonArgs += $flag
    }
}

$buildDir = Get-CheckedBuildDir $buildName

$sources = @()
$sources += Get-ChildItem -LiteralPath $paths.APP -Recurse -File | Where-Object { $_.Extension -in @(".c", ".S") }
$sources += Get-ChildItem -LiteralPath $paths.Profile -File -Filter *.c
$sources += Get-ChildItem -LiteralPath $paths.HAL -File -Filter *.c | Where-Object { $_.Name -notin @("KEY.c", "LED.c") }
$sources += Get-ChildItem -LiteralPath $paths.LIB -File -Filter *.S
$sources += Get-ChildItem -LiteralPath $paths.Startup -File -Filter *.S
$sources += Get-ChildItem -LiteralPath $paths.Std -File -Filter *.c | Where-Object {
    $_.Name -notin @(
        "CH59x_usbdev.c",
        "CH59x_usbhostClass.c",
        "CH59x_usbhostBase.c",
        "CH59x_spi0.c",
        "CH59x_timer0.c",
        "CH59x_timer3.c",
        "CH59x_uart0.c",
        "CH59x_uart2.c",
        "CH59x_uart3.c"
    )
}

$objects = @()
foreach ($source in $sources) {
    $relative = $source.FullName.Replace($projectRoot, "Project").Replace($bleRoot, "BLE").Replace($srcRoot, "SRC").Replace(":", "")
    $object = Join-Path $buildDir ($relative + ".o")
    New-Item -ItemType Directory -Force -Path (Split-Path $object -Parent) | Out-Null

    $compileArgs = @("-c") + $commonArgs + $includeArgs + @("-o", $object, $source.FullName)
    Invoke-Checked -Exe $gcc -Arguments $compileArgs -FailureMessage "Compile failed: $($source.FullName)"
    $objects += $object
}

$elf = Join-Path $buildDir "$artifact.elf"
$hex = Join-Path $buildDir "$artifact.hex"
$linkArgs = $commonArgs + @(
    "-nostartfiles",
    "--specs=nano.specs",
    "--specs=nosys.specs",
    "-Wl,--gc-sections",
    "-Wl,--print-memory-usage",
    "-T", (Join-Path $paths.Ld "Link.ld")
) + $objects + @(
    "-L", $paths.LIB,
    "-L", $paths.Std,
    "-L", "..",
    "-lISP592",
    "-lCH59xBLE",
    "-o", $elf
)

Invoke-Checked -Exe $gcc -Arguments $linkArgs -FailureMessage "Link failed"
Invoke-Checked -Exe $objcopy -Arguments @("-O", "ihex", $elf, $hex) -FailureMessage "Hex generation failed"
Invoke-Checked -Exe $size -Arguments @($elf) -FailureMessage "Size failed"

$compilerVersion = (& $gcc --version | Select-Object -First 1)
$gitCommit = git -C $RepoRoot rev-parse --short HEAD
$manifest = @"
profile=$Profile
artifact=$artifact
sdk_root=$SdkRoot
mounriver_root=$MounRiverRoot
compiler=$gcc
compiler_version=$compilerVersion
git_commit=$gitCommit
build_date=$(Get-Date -Format "yyyy-MM-ddTHH:mm:ssK")
hex=$hex
"@
$manifest | Set-Content -Encoding ascii -Path (Join-Path $buildDir "build_manifest.txt")

Write-Host "BUILD_OK $hex"
