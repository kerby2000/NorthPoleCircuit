param(
    [Parameter(Mandatory = $true)]
    [string]$Port,

    [int]$Baud = 115200
)

$serial = New-Object System.IO.Ports.SerialPort $Port, $Baud, "None", 8, "One"
$serial.NewLine = "`n"
$serial.Open()

try {
    Write-Host "Reading $Port at $Baud baud. Press Ctrl+C to stop."
    while ($true) {
        $line = $serial.ReadLine()
        Write-Host $line
    }
}
finally {
    $serial.Close()
}
