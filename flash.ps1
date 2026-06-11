param(
    [ValidateSet("prog", "recover", "help")]
    [string]$Mode = "prog"
)

# Configuration
$OPENOCD_PATH = "C:\DEV\tools\openocd_mspm0"

$OPENOCD_EXE = Join-Path $OPENOCD_PATH "bin\openocd.exe"
$CMSIS_DAP_CFG = Join-Path $OPENOCD_PATH "share\openocd\scripts\interface\cmsis-dap.cfg"
$MSPM0_CFG = Join-Path $OPENOCD_PATH "share\openocd\scripts\target\ti\mspm0.cfg"

# Firmware image
# $HEX_FILE = "C:\DEV\projects\junk\mspm0_i2c_periph_expander.hex"
$HEX_FILE = Join-Path $PSScriptRoot "mspm0_i2c_periph_expander.hex"

$HEX_FILE_TCL = $HEX_FILE -replace '\\','/'

# Common OpenOCD arguments
$CommonArgs = @(
    "-f", $CMSIS_DAP_CFG,
    "-c", "transport select swd",
    "-c", "adapter speed 500",
    "-f", $MSPM0_CFG
)

switch ($Mode) {

    "help" {
        Write-Host "Usage: .\flash.ps1 -Mode <prog|recover|help>" -ForegroundColor Cyan
        Write-Host "  prog    - Program the device with the specified firmware (default)" -ForegroundColor Green
        Write-Host "  recover  - Perform a mass erase to recover a bricked device" -ForegroundColor Yellow
        Write-Host "  help     - Show this help message" -ForegroundColor Cyan
        Write-Host "             Note: If you get an error, ensure no GDB server is running" -ForegroundColor Cyan
        Write-Host "             To recover a bricked device, try holding down RESET" -ForegroundColor Cyan
    }

    "prog" {
        Write-Host "File: $HEX_FILE_TCL"
        Write-Host "Programming device..." -ForegroundColor Green

        & $OPENOCD_EXE @CommonArgs `
            -c "init; program $HEX_FILE_TCL verify reset exit"

        $exitCode = $LASTEXITCODE
    }

    "recover" {
        Write-Host "Recovering device (mass erase)..." -ForegroundColor Yellow

        & $OPENOCD_EXE @CommonArgs `
            -c "init; mspm0_mass_erase; shutdown"

        $exitCode = $LASTEXITCODE
    }
}

if ($exitCode -eq 0) {
    Write-Host "Operation completed successfully." -ForegroundColor Green
}
else {
    Write-Host "Operation failed (OpenOCD exit code $exitCode)." -ForegroundColor Red
    exit $exitCode
}
