# Command-line build of the STM32GpsSimulator firmware (STM32F756ZG, Cortex-M7).
#
# Uses the same settings as the STM32CubeIDE "Debug" configuration (.cproject).
# STM32CubeIDE does not use this file: it keeps building with its own makefiles
# in Debug/, so the project opens and builds in the IDE exactly as before.
#
# Requirements: GNU Arm Embedded Toolchain (arm-none-eabi-gcc) and make.
# Usage:
#   make          - build build/STM32GpsSimulator.elf, .hex and .bin
#   make clean    - remove the build directory

TARGET    := STM32GpsSimulator
BUILD_DIR := build

PREFIX  := arm-none-eabi-
CC      := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
SIZE    := $(PREFIX)size

# ---- MCU (from .cproject) ----
MCU := -mcpu=cortex-m7 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard

DEFS := -DUSE_HAL_DRIVER -DSTM32F756xx -DDEBUG

INCS := \
	-ICore/Inc \
	-IDrivers/STM32F7xx_HAL_Driver/Inc \
	-IDrivers/STM32F7xx_HAL_Driver/Inc/Legacy \
	-IDrivers/CMSIS/Device/ST/STM32F7xx/Include \
	-IDrivers/CMSIS/Include

# ---- Sources ----
# All HAL files in Drivers/.../Src are taken automatically, so a peripheral
# added later in CubeMX is built without editing this file.
C_SRCS := \
	$(wildcard Core/Src/*.c) \
	$(wildcard Drivers/STM32F7xx_HAL_Driver/Src/*.c)

ASM_SRCS := Core/Startup/startup_stm32f756zgtx.s

LDSCRIPT := STM32F756ZGTX_FLASH.ld

# ---- Flags (STM32CubeIDE defaults for Debug) ----
CFLAGS := $(MCU) $(DEFS) $(INCS) \
	-std=gnu11 -g3 -O0 -Wall \
	-ffunction-sections -fdata-sections \
	--specs=nano.specs

ASFLAGS := $(MCU) $(DEFS) -g3 -x assembler-with-cpp --specs=nano.specs

# -u _printf_float: printf("%f") support, enabled in the CubeIDE project settings.
LDFLAGS := $(MCU) -T$(LDSCRIPT) \
	--specs=nosys.specs --specs=nano.specs -u _printf_float \
	-Wl,-Map=$(BUILD_DIR)/$(TARGET).map -Wl,--gc-sections -static \
	-Wl,--start-group -lc -lm -Wl,--end-group

# ---- Objects ----
OBJS := \
	$(addprefix $(BUILD_DIR)/,$(C_SRCS:.c=.o)) \
	$(addprefix $(BUILD_DIR)/,$(ASM_SRCS:.s=.o))

# ---- Rules ----
.PHONY: all clean

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin
	$(SIZE) $(BUILD_DIR)/$(TARGET).elf

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJS) $(LDSCRIPT)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf $(BUILD_DIR)
