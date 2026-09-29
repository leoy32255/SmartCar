# ==============================================================================
#  SmartCar —— STM32 智能循迹小车
#  基于 arm-none-eabi-gcc + Make 的构建脚本
#
#  目录结构（与 CubeMX 生成的布局一致）：
#    Core/Inc, Core/Src          业务代码（本工程手写）
#    Core/Startup                启动文件（CubeMX 生成）
#    Drivers/                    HAL 库 + CMSIS（CubeMX 生成，不要手改）
#    build/                      编译输出
#
#  用法：
#    make            编译
#    make -j8        并行编译（推荐）
#    make clean      清理
#    make flash      用 ST-Link 烧录
#    make size       查看 Flash/RAM 占用
#
#  换 F407VET6：见文件末尾的 MCU_FAMILY 切换说明，或直接
#    make MCU_FAMILY=F4
# ==============================================================================

TARGET      = SmartCar
BUILD_DIR   = build

# ------------------------------------------------------------------------------
# MCU 选择：F1 = STM32F103C8T6(当前)  F4 = STM32F407VET6(升级目标)
# ------------------------------------------------------------------------------
MCU_FAMILY ?= F1

ifeq ($(MCU_FAMILY),F1)
  CPU            = -mcpu=cortex-m3
  FPU            =
  HAL_DIR        = STM32F1xx_HAL_Driver
  MCU_DEF        = STM32F103xB
  STARTUP        = Core/Startup/startup_stm32f103xb.s
  LDSCRIPT       = STM32F103C8Tx_FLASH.ld
  CMSIS_DEV_INC  = Drivers/CMSIS/Device/ST/STM32F1xx/Include
else ifeq ($(MCU_FAMILY),F4)
  CPU            = -mcpu=cortex-m4
  # F4 带单精度硬件浮点，姿态解算的 atan2/sqrt 会快很多
  FPU            = -mfpu=fpv4-sp-d16 -mfloat-abi=hard
  HAL_DIR        = STM32F4xx_HAL_Driver
  MCU_DEF        = STM32F407xx
  STARTUP        = Core/Startup/startup_stm32f407xx.s
  LDSCRIPT       = STM32F407VETx_FLASH.ld
  CMSIS_DEV_INC  = Drivers/CMSIS/Device/ST/STM32F4xx/Include
else
  $(error MCU_FAMILY 只能是 F1 或 F4)
endif

# ------------------------------------------------------------------------------
# 工具链
# ------------------------------------------------------------------------------
PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
AS      = $(PREFIX)gcc -x assembler-with-cpp
CP      = $(PREFIX)objcopy
SZ      = $(PREFIX)size
HEX     = $(CP) -O ihex
BIN     = $(CP) -O binary -S

# ------------------------------------------------------------------------------
# 源文件
# ------------------------------------------------------------------------------
C_SOURCES  = $(wildcard Core/Src/*.c)
C_SOURCES += $(wildcard Drivers/$(HAL_DIR)/Src/*.c)

# 过滤掉 HAL 里的模板文件，它们需要用户改名后才生效，
# 直接编进来会和正常实现重复定义。
C_SOURCES := $(filter-out %_template.c, $(C_SOURCES))

ASM_SOURCES = $(STARTUP)

# ------------------------------------------------------------------------------
# 编译选项
# ------------------------------------------------------------------------------
C_DEFS = -D$(MCU_DEF) -DUSE_HAL_DRIVER

C_INCLUDES  = -ICore/Inc
C_INCLUDES += -IDrivers/$(HAL_DIR)/Inc
C_INCLUDES += -IDrivers/$(HAL_DIR)/Inc/Legacy
C_INCLUDES += -I$(CMSIS_DEV_INC)
C_INCLUDES += -IDrivers/CMSIS/Include

# -ffunction-sections/-fdata-sections 配合 --gc-sections 剔除未用代码，
# F103C8T6 只有 64KB Flash，这一步能省下不少空间。
CFLAGS  = $(CPU) -mthumb $(FPU) -Wall -Wextra -fdata-sections -ffunction-sections
CFLAGS += -g -gdwarf-2 -O2
CFLAGS += $(C_DEFS) $(C_INCLUDES)
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"

ASFLAGS = $(CPU) -mthumb $(FPU) -g -gdwarf-2

# 姿态解算用到 atan2f / sqrtf，必须链接数学库
LIBS    = -lc -lm -lnosys
LIBDIR  =

LDFLAGS  = $(CPU) -mthumb $(FPU) -specs=nano.specs -T$(LDSCRIPT)
LDFLAGS += $(LIBDIR) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref
LDFLAGS += -Wl,--gc-sections

# ------------------------------------------------------------------------------
# 构建规则
# ------------------------------------------------------------------------------
OBJECTS  = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	@echo "  CC      $<"
	@$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	@echo "  AS      $<"
	@$(AS) -c $(ASFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	@echo "  LD      $@"
	@$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	@$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	@$(HEX) $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	@$(BIN) $< $@

$(BUILD_DIR):
	@mkdir -p $@

# ------------------------------------------------------------------------------
# 辅助目标
# ------------------------------------------------------------------------------
size: $(BUILD_DIR)/$(TARGET).elf
	@$(SZ) $<

clean:
	@echo "  CLEAN"
	@rm -rf $(BUILD_DIR)

# 烧录（需要 ST-Link + STM32CubeProgrammer 的 CLI 或 st-flash）
flash: $(BUILD_DIR)/$(TARGET).bin
	@echo "  烧录 $(BUILD_DIR)/$(TARGET).bin ..."
	-STM32_Programmer_CLI -c port=SWD -w $(BUILD_DIR)/$(TARGET).bin 0x08000000 -v -rst

# OpenOCD 方式烧录（另一条常用路径）
flash-ocd: $(BUILD_DIR)/$(TARGET).elf
	openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
	        -c "program $(BUILD_DIR)/$(TARGET).elf verify reset exit"

.PHONY: all clean size flash flash-ocd

# 依赖文件（由 -MMD 生成）
-include $(wildcard $(BUILD_DIR)/*.d)
