# Based on weekly_hw/w1/Makefile.
PROJECT = firmware
CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

CPU = -mcpu=cortex-m3
MCU = $(CPU) -mthumb -mfloat-abi=soft
BUILD_DIR = Build
BIN_DIR = Bin
LINKER_DIR = bsp
HAL_DIR = hal
CMSIS_DIR = bsp/CMSIS
FREERTOS_DIR = middlewares/FreeRTOS/Source
LINKER_SCRIPT = $(LINKER_DIR)/STM32F103C8.ld

# Templates define alternative MSP/time bases and must not be built together.
C_SOURCES = main.c $(wildcard bsp/*.c app/src/*.c) \
    $(filter-out %_template.c,$(wildcard $(HAL_DIR)/src/*.c)) \
    $(wildcard $(FREERTOS_DIR)/*.c) \
    $(FREERTOS_DIR)/portable/GCC/ARM_CM3/port.c \
    $(FREERTOS_DIR)/portable/MemMang/heap_4.c
ASM_SOURCES = bsp/startup_stm32f103xb.s
OBJECTS = $(addprefix $(BUILD_DIR)/,$(C_SOURCES:.c=.o) $(ASM_SOURCES:.s=.o))

CPPFLAGS = -DUSE_HAL_DRIVER -DSTM32F103xB \
    -Ibsp -Iapp/inc -I$(HAL_DIR)/inc -I$(CMSIS_DIR)/Include \
    -I$(CMSIS_DIR)/Device/ST/STM32F1xx/Include \
    -I$(FREERTOS_DIR)/include -I$(FREERTOS_DIR)/portable/GCC/ARM_CM3
CFLAGS = $(MCU) -std=c11 -O0 -g3 -Wall -Wextra \
    -ffunction-sections -fdata-sections -MMD -MP
LDFLAGS = $(MCU) -T$(LINKER_SCRIPT) --specs=nano.specs --specs=nosys.specs \
    -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map \
    -Wl,--print-memory-usage

.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

all: $(BIN_DIR)/$(PROJECT).bin $(BIN_DIR)/$(PROJECT).hex

$(BUILD_DIR)/%.o: %.c Makefile
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile
	mkdir -p $(dir $@)
	$(CC) $(MCU) -g3 -x assembler-with-cpp -c $< -o $@

$(BIN_DIR)/$(PROJECT).elf: $(OBJECTS) $(LINKER_SCRIPT) Makefile
	mkdir -p $(BIN_DIR)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BIN_DIR)/$(PROJECT).bin: $(BIN_DIR)/$(PROJECT).elf
	$(OBJCOPY) -O binary $< $@

$(BIN_DIR)/$(PROJECT).hex: $(BIN_DIR)/$(PROJECT).elf
	$(OBJCOPY) -O ihex $< $@

flash: all
	st-flash write $(BIN_DIR)/$(PROJECT).bin 0x08000000
	st-flash reset

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

-include $(OBJECTS:.o=.d)
.PHONY: all flash clean
