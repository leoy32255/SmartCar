# Revision-3 F407 bring-up. F1 is retained as an unprovisioned legacy target.
MCU_FAMILY ?= F4
ifeq ($(MCU_FAMILY),F1)
include make/legacy-f1.mk
else ifeq ($(MCU_FAMILY),F4)
.DEFAULT_GOAL := all
TARGET := SmartCar
BUILD_DIR := build/F4
CROSS_COMPILE ?= arm-none-eabi-
export CROSS_COMPILE
PYTHON ?= python
CC := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
SIZE := $(CROSS_COMPILE)size
HAL_DIR := Drivers/STM32F4xx_HAL_Driver
DEVICE_DIR := Drivers/CMSIS/Device/ST/STM32F4xx
LDSCRIPT := STM32F407VETx_FLASH.ld
STARTUP := Core/Startup/startup_stm32f407xx.s
HAL_MODULES := hal hal_cortex hal_rcc hal_rcc_ex hal_pwr hal_pwr_ex hal_flash hal_flash_ex hal_gpio hal_tim hal_tim_ex hal_uart hal_spi hal_dma
C_SOURCES := Core/F407/main.c Core/F407/bsp.c Core/F407/interrupts.c \
 $(DEVICE_DIR)/Source/Templates/system_stm32f4xx.c \
 $(addprefix $(HAL_DIR)/Src/stm32f4xx_,$(addsuffix .c,$(HAL_MODULES)))
CPU := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
CPPFLAGS := -DSTM32F407xx -DUSE_HAL_DRIVER -DUSER_VECT_TAB_ADDRESS -ICore/Inc \
 -I$(HAL_DIR)/Inc -I$(HAL_DIR)/Inc/Legacy -I$(DEVICE_DIR)/Include -IDrivers/CMSIS/Include
CFLAGS := $(CPU) -std=c11 -Os -g3 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -MMD -MP
LDFLAGS := $(CPU) -nostartfiles --specs=nano.specs --specs=nosys.specs -T$(LDSCRIPT) \
 -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref,--gc-sections,--print-memory-usage
OBJECTS := $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o))) $(BUILD_DIR)/startup_stm32f407xx.o
vpath %.c $(sort $(dir $(C_SOURCES)))

# Upstream HAL has three unused Banks arguments on single-bank F407. Keep it unchanged.
$(BUILD_DIR)/stm32f4xx_hal_flash_ex.o: CFLAGS += -Wno-unused-parameter

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

$(BUILD_DIR):
	$(PYTHON) -c "from pathlib import Path; Path('$(BUILD_DIR)').mkdir(parents=True, exist_ok=True)"

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/startup_stm32f407xx.o: $(STARTUP) Makefile | $(BUILD_DIR)
	$(CC) $(CPU) -x assembler-with-cpp -g3 -c $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) $(LDSCRIPT) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -Wl,--start-group -lc -lm -lnosys -Wl,--end-group -o $@
	$(SIZE) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf
	$(OBJCOPY) -O binary $< $@

size: $(BUILD_DIR)/$(TARGET).elf
	$(SIZE) $<

test: all
	$(PYTHON) -m unittest discover -s tests -v

clean:
	$(PYTHON) tools/clean_f407.py

# Programming is an explicit, separate operation. This task does not flash.
flash flash-ocd:
	$(error F407 bring-up does not program hardware; see docs/F407最小构建.md for ROM DFU)

.PHONY: all size test clean flash flash-ocd
-include $(wildcard $(BUILD_DIR)/*.d)
else
$(error MCU_FAMILY must be F4 or legacy F1)
endif
