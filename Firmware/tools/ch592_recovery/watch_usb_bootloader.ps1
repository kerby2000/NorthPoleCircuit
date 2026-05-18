param(
    [int]$Seconds = 60,
    [int]$IntervalMs = 100
)

$ErrorActionPreference = "SilentlyContinue"

$Patterns = @(
    "VID_4348&PID_55E0", # CH592 native USB Download bootloader
    "VID_1A86&PID_8010"  # WCH-LinkE
)

function Get-MatchingDevices {
    $devices = Get-PnpDevice -PresentOnly
    foreach ($pattern in $Patterns) {
        $devices | Where-Object { $_.InstanceId -like "*$pattern*" } | ForEach-Object {
            [PSCustomObject]@{
                Pattern = $pattern
                Status = $_.Status
                Class = $_.Class
                FriendlyName = $_.FriendlyName
                InstanceId = $_.InstanceId
            }
        }
    }
}

$deadline = (Get-Date).AddSeconds($Seconds)
$last = ""

Write-Host "Watching USB/PnP devices for $Seconds seconds..."
Write-Host "Targets:"
Write-Host "  VID_4348&PID_55E0  CH592 USB Download bootloader"
Write-Host "  VID_1A86&PID_8010  WCH-LinkE"
Write-Host ""

while ((Get-Date) -lt $deadline) {
    $current = Get-MatchingDevices | Sort-Object Pattern, InstanceId
    $text = ($current | Format-Table -AutoSize | Out-String).Trim()

    if ($text -ne $last) {
        $timestamp = Get-Date -Format "HH:mm:ss.fff"
        if ($text) {
            Write-Host "[$timestamp] device state changed:"
            Write-Host $text
        } else {
            Write-Host "[$timestamp] no matching devices present"
        }
        Write-Host ""
        $last = $text
    }

    Start-Sleep -Milliseconds $IntervalMs
}

