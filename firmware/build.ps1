# ==============================================================================
# AnyTone AT-D878UVII Plus Open Firmware Build Script (PowerShell)
# Target: GD32F303VGT6 Cortex-M4 @ 120 MHz
# Callsign: TA5THK / Fatih ATCI
# ==============================================================================

$ErrorActionPreference = "Stop"

$ArmGcc = "C:\Program Files\Arm\GNU Toolchain mingw-w64-x86_64-arm-none-eabi\bin\arm-none-eabi-gcc.exe"
$ArmObjcopy = "C:\Program Files\Arm\GNU Toolchain mingw-w64-x86_64-arm-none-eabi\bin\arm-none-eabi-objcopy.exe"
$ArmSize = "C:\Program Files\Arm\GNU Toolchain mingw-w64-x86_64-arm-none-eabi\bin\arm-none-eabi-size.exe"
$Python = "$PSScriptRoot\..\desktop\at_d878_studio\.venv\Scripts\python.exe"

if (-not (Test-Path $ArmGcc)) {
    $ArmGcc = (Get-Command arm-none-eabi-gcc -ErrorAction SilentlyContinue).Source
}

if (-not (Test-Path $ArmGcc)) {
    Write-Error "arm-none-eabi-gcc not found! Please install ARM GNU Toolchain."
    exit 1
}

Write-Host "==> Compiling AT-D878 Open Firmware..." -ForegroundColor Cyan

$Srcs = @(
    "core/src/startup_gd32f303.c",
    "core/src/main.c",
    "hal/src/hal_gpio.c",
    "hal/src/hal_spi.c",
    "hal/src/hal_i2c.c",
    "hal/src/hal_systick.c",
    "hal/src/hal_uart.c",
    "hal/src/hal_adc.c",
    "drivers/src/driver_display.c",
    "drivers/src/driver_at1846s.c",
    "drivers/src/driver_audio.c",
    "drivers/src/driver_tones.c",
    "drivers/src/driver_keyboard.c",
    "drivers/src/driver_sct3258.c",
    "drivers/src/driver_spi_flash.c",
    "drivers/src/driver_linkboard.c",
    "radio/src/radio_core.c",
    "radio/src/codeplug_storage.c",
    "radio/src/dmr_protocol.c",
    "radio/src/aprs_packet.c",
    "radio/src/scanner.c",
    "radio/src/cps_protocol.c",
    "ui/src/ui_screen.c",
    "ui/src/ui_menu.c"
)

$Flags = @(
    "-mcpu=cortex-m4", "-mthumb", "-mfloat-abi=soft", "-O2",
    "-Wall", "-Wextra", "-ffunction-sections", "-fdata-sections",
    "--specs=nano.specs", "--specs=nosys.specs",
    "-Ihal/include", "-Idrivers/include", "-Iradio/include", "-Iui/include",
    "-Tcore/gd32f303vgt6.ld", "-Wl,--gc-sections", "-nostartfiles",
    "-o", "build/at_d878_fw.elf"
)

if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}

& $ArmGcc $Flags $Srcs
if ($LASTEXITCODE -ne 0) {
    Write-Error "Compilation failed!"
    exit $LASTEXITCODE
}

& $ArmSize "build/at_d878_fw.elf"
& $ArmObjcopy -O binary "build/at_d878_fw.elf" "build/at_d878_fw.bin"

Write-Host "==> Packaging firmware (.CDD, .CDI, .spi)..." -ForegroundColor Green
& $Python "../tools/firmware_analyzer/make_firmware_pkg.py" "build/at_d878_fw.bin" -o "../output_firmware"

Write-Host "`n[OK] Firmware Build and Packaging Succeeded!" -ForegroundColor Green
