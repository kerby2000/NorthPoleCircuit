param(
    [string]$SdkRoot = "C:\WCH\CH592EVT",
    [string]$MounRiverRoot = "C:\MounRiver\MounRiver_Studio2"
)

$ErrorActionPreference = "Stop"

function Get-CheckedDirectory {
    param(
        [string]$Root,
        [string]$Child
    )

    $rootFull = [System.IO.Path]::GetFullPath($Root)
    $childFull = [System.IO.Path]::GetFullPath((Join-Path $Root $Child))
    if (!$childFull.StartsWith($rootFull, [System.StringComparison]::OrdinalIgnoreCase)) {
        Write-Error "Refusing path outside $rootFull`: $childFull"
    }
    return $childFull
}

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$FirmwareRoot = Join-Path $RepoRoot "Firmware"
$BuildRoot = Join-Path $FirmwareRoot "build"
$SourceBuildDir = Join-Path $BuildRoot "bringup"
$MvpBuildDir = Get-CheckedDirectory -Root $BuildRoot -Child "mvp_demo"

$defines = @(
    'APP_BUILD_PROFILE_NAME=\"mvp-demo\"',
    'APP_FIRMWARE_VERSION=\"0.1.0-mvp-demo\"',
    'APP_RGB_WS2812_USE_SPI0_MOSI_PA14=1',
    'APP_MOTOR_PWM_BACKEND_ENABLE=1',
    'APP_MOTOR_PWM_MAX_DUTY_PERMILLE=1000',
    'APP_DEMO_SCENE_ENABLE=1',
    'APP_DEMO_USB_POWER_ONLY=1',
    'APP_DEMO_DEFAULT_HALL_ENABLE=0',
    'APP_DEMO_DEFAULT_AUDIO_VOLUME=31',
    'APP_DEMO_DEFAULT_RGB_BRIGHTNESS=24',
    'APP_DEMO_DEFAULT_RGB_EFFECT=1',
    'APP_MOTOR_WAVE_TABLE_SIZE=256',
    'APP_MOTOR_CONTROL_UPDATE_HZ=8000',
    'APP_MOTOR_PWM_CARRIER_HZ=100000',
    'APP_MOTION_RAMP_ENABLE=1',
    'APP_MOTION_TOUCH_CONTROL_ENABLE=0',
    'APP_MOTION_DEFAULT_SPEED_HZ_X1000=8000',
    'APP_MOTION_DEFAULT_AMPLITUDE_PERMILLE=1000',
    'APP_MOTION_GUARD_MODE=2',
    'APP_MOTION_GUARD_DUTY_PERMILLE=600'
)

& (Join-Path $PSScriptRoot "build.ps1") `
    -Profile bringup `
    -SdkRoot $SdkRoot `
    -MounRiverRoot $MounRiverRoot `
    -ExtraDefine $defines

if (!(Test-Path -LiteralPath $SourceBuildDir)) {
    Write-Error "Expected bringup build directory not found: $SourceBuildDir"
}

if (Test-Path -LiteralPath $MvpBuildDir) {
    Remove-Item -LiteralPath $MvpBuildDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $MvpBuildDir | Out-Null

$artifactNames = @(
    "northpole_ch592_bringup.hex",
    "northpole_ch592_bringup.elf",
    "northpole_ch592_bringup.map",
    "build_manifest.txt"
)

foreach ($artifactName in $artifactNames) {
    $source = Join-Path $SourceBuildDir $artifactName
    if (Test-Path -LiteralPath $source) {
        Copy-Item -LiteralPath $source -Destination (Join-Path $MvpBuildDir $artifactName) -Force
    }
}

$hex = Join-Path $MvpBuildDir "northpole_ch592_bringup.hex"
if (!(Test-Path -LiteralPath $hex)) {
    Write-Error "MVP demo hex was not produced: $hex"
}

$manifestPath = Join-Path $MvpBuildDir "build_manifest.txt"
$manifestAppend = @(
    "",
    "mvp_profile=1",
    "mvp_build_wrapper=Firmware/tools/build_mvp_demo.ps1",
    "mvp_output=$MvpBuildDir",
    "mvp_defines=$($defines -join ';')"
)
Add-Content -Encoding ascii -Path $manifestPath -Value $manifestAppend

Write-Host "MVP_DEMO_BUILD_OK $hex"
