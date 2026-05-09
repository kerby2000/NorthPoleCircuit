param(
    [string]$SdkRoot = "C:\WCH\CH592EVT",
    [string]$MounRiverRoot = "C:\MounRiver\MounRiver_Studio2"
)

$ErrorActionPreference = "Continue"

$buildScript = Join-Path $PSScriptRoot "build.ps1"
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$logDir = Join-Path $repoRoot "Firmware\build\matrix_logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$cases = @(
    @{
        Name = "bringup default"
        Params = @{ Profile = "bringup" }
    },
    @{
        Name = "production default"
        Params = @{ Profile = "production" }
    },
    @{
        Name = "bringup pwm backend"
        Params = @{ Profile = "bringup"; ExtraDefine = @("APP_MOTOR_PWM_BACKEND_ENABLE=1") }
    },
    @{
        Name = "bringup usb cdc disabled"
        Params = @{ Profile = "bringup"; ExtraDefine = @("APP_USB_CDC_SHELL_ENABLE=0") }
    },
    @{
        Name = "bringup extra warnings"
        Params = @{ Profile = "bringup"; ExtraCFlag = @("-Wall", "-Wextra") }
    }
)

$results = @()

foreach ($case in $cases) {
    $safeName = ($case.Name -replace "[^A-Za-z0-9_-]", "_")
    $logPath = Join-Path $logDir "$safeName.log"
    $params = $case.Params.Clone()
    $params["SdkRoot"] = $SdkRoot
    $params["MounRiverRoot"] = $MounRiverRoot

    Write-Host "BUILD_START $($case.Name)"
    & $buildScript @params *> $logPath
    $scriptSucceeded = $?
    $exitCode = if ($scriptSucceeded -and ($LASTEXITCODE -eq 0)) { 0 } else { 1 }

    $results += [pscustomobject]@{
        Name = $case.Name
        Pass = ($exitCode -eq 0)
        Log = $logPath
    }

    if ($exitCode -eq 0) {
        Write-Host "BUILD_PASS  $($case.Name)"
    } else {
        Write-Host "BUILD_FAIL  $($case.Name) log=$logPath"
    }
}

Write-Host ""
Write-Host "Build matrix summary"
Write-Host "--------------------"
foreach ($result in $results) {
    $status = if ($result.Pass) { "PASS" } else { "FAIL" }
    Write-Host ("{0,-6} {1}" -f $status, $result.Name)
}

if ([array]($results | Where-Object { -not $_.Pass }).Count -gt 0) {
    exit 1
}
exit 0
