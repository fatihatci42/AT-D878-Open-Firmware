# ==============================================================================
# Makefile - AnyTone AT-D878UVII Plus Open Source Firmware
# Target MCU: GD32F303VGT6 (ARM Cortex-M4 @ 120 MHz, 1MB Flash, 96KB SRAM)
# Callsign:   TA5THK / Fatih ATCI
# ==============================================================================

CC      := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE    := arm-none-eabi-size
PYTHON  := python

TARGET  := build/at_d878_fw

SRCS := \
    core/src/startup_gd32f303.c \
    core/src/main.c \
    hal/src/hal_gpio.c \
    hal/src/hal_spi.c \
    hal/src/hal_i2c.c \
    hal/src/hal_systick.c \
    hal/src/hal_uart.c \
    hal/src/hal_adc.c \
    drivers/src/driver_display.c \
    drivers/src/driver_at1846s.c \
    drivers/src/driver_audio.c \
    drivers/src/driver_tones.c \
    drivers/src/driver_keyboard.c \
    drivers/src/driver_sct3258.c \
    drivers/src/driver_spi_flash.c \
    drivers/src/driver_linkboard.c \
    drivers/src/driver_rda5802.c \
    radio/src/radio_core.c \
    radio/src/calibration.c \
    radio/src/codeplug_storage.c \
    radio/src/dmr_protocol.c \
    radio/src/aprs_packet.c \
    radio/src/scanner.c \
    radio/src/cps_protocol.c \
    ui/src/ui_screen.c \
    ui/src/ui_menu.c

INCLUDES := \
    -Ihal/include \
    -Idrivers/include \
    -Iradio/include \
    -Iui/include

CFLAGS := \
    -mcpu=cortex-m4 \
    -mthumb \
    -mfloat-abi=soft \
    -O2 \
    -Wall \
    -Wextra \
    -ffunction-sections \
    -fdata-sections \
    --specs=nano.specs \
    --specs=nosys.specs \
    $(INCLUDES)

LDFLAGS := \
    -Tcore/gd32f303vgt6.ld \
    -Wl,--gc-sections \
    -nostartfiles

all: $(TARGET).bin package

$(TARGET).elf: $(SRCS)
	@mkdir -p build
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SRCS)
	$(SIZE) $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	@echo "[✓] Binary output generated: $@"

package: $(TARGET).bin
	@mkdir -p ../output_firmware
	$(PYTHON) ../tools/firmware_analyzer/make_firmware_pkg.py $(TARGET).bin -o ../output_firmware

clean:
	rm -rf build/*.elf build/*.bin build/*.o
