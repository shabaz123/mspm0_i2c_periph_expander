# Force GNU Make to run recipes inside cmd.exe on Windows
ifeq ($(OS),Windows_NT)
SHELL := cmd.exe
.SHELLFLAGS := /d /s /c
# Windows paths
GCC_ARMCOMPILER_MSP ?= C:/DEV/arm_gnu_toolchains/15.2.rel1
MSPM0_SDK_INSTALL_DIR ?= C:/ti/mspm0_sdk_2_10_00_04
# Standard SDK install has ti/ and third_party/ inside source/
SDK_ROOT = $(MSPM0_SDK_INSTALL_DIR)/source
else
# Linux paths (e.g. GitHub Actions)
GCC_ARMCOMPILER_MSP ?= /usr
# Bundled SDK in repo has ti/ and third_party/ directly in mspm0-sdk/
SDK_ROOT = $(CURDIR)/source/mspm0-sdk
endif

CC = "$(GCC_ARMCOMPILER_MSP)/bin/arm-none-eabi-gcc"
LNK = "$(GCC_ARMCOMPILER_MSP)/bin/arm-none-eabi-gcc"
OBJCOPY = "$(GCC_ARMCOMPILER_MSP)/bin/arm-none-eabi-objcopy"

PROJECT = mspm0_i2c_periph_expander
NAME = $(PROJECT)
ELF = $(NAME).out
HEX = $(NAME).hex

# Source directories
SRC_DIR = source
BRD_DIR = $(SRC_DIR)/board
MOD_DIR = $(SRC_DIR)/modules
EMU_DIR = $(MOD_DIR)/emulation
COM_DIR = $(MOD_DIR)/communication

# Include paths
CFLAGS = -I$(BRD_DIR) \
    -I$(MOD_DIR) \
    -I$(EMU_DIR) \
    -I$(COM_DIR) \
    -D__MSPM0L1105__ \
    -O2 \
    "-I$(SDK_ROOT)/third_party/CMSIS/Core/Include" \
    "-I$(SDK_ROOT)" \
    -mcpu=cortex-m0plus \
    -march=armv6-m \
    -mthumb \
    -std=c99 \
    -mfloat-abi=soft \
    -ffunction-sections \
    -fdata-sections \
    -g \
    -gstrict-dwarf \
    -Wall \
    "-I$(GCC_ARMCOMPILER_MSP)/arm-none-eabi/include/newlib-nano" \
    "-I$(GCC_ARMCOMPILER_MSP)/arm-none-eabi/include"

# Linker flags
LFLAGS = "-L$(SDK_ROOT)/ti/driverlib/lib/gcc/m0p/mspm0l11xx_l13xx" \
    -nostartfiles \
    -Wl,-T,$(SDK_ROOT)/ti/devices/msp/m0p/linker_files/gcc/mspm0l1105.lds \
    "-Wl,-Map,$(NAME).map" \
    "-l:driverlib.a" \
    -march=armv6-m \
    -mthumb \
    -static \
    -Wl,--gc-sections \
    "-L$(GCC_ARMCOMPILER_MSP)/arm-none-eabi/lib/thumb/v6-m/nofp" \
    -lgcc \
    -lc \
    -lm \
    -lnosys \
    --specs=nano.specs

# Object files
OBJDIR = obj
OBJ_DIR = $(OBJDIR)
OBJECTS = $(OBJ_DIR)/main.o \
          $(OBJ_DIR)/system.o \
          $(OBJ_DIR)/ti_msp_dl_config.o \
          $(OBJ_DIR)/ad7291_emulation.o \
          $(OBJ_DIR)/sc16is740_emulation.o \
          $(OBJ_DIR)/eeprom_emulation_type_b.o \
          $(OBJ_DIR)/i2c_tar_driver.o \
          $(OBJ_DIR)/startup_mspm0l110x_gcc.o

all: $(OBJ_DIR) $(ELF) $(HEX)

$(OBJ_DIR):
ifeq ($(OS),Windows_NT)
	@ if not exist $(OBJ_DIR) mkdir $(OBJ_DIR)
else
	@ mkdir -p $(OBJ_DIR)
endif

$(OBJ_DIR)/main.o: $(BRD_DIR)/main.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -c -o $@

$(OBJ_DIR)/system.o: $(BRD_DIR)/system.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -c -o $@

$(OBJ_DIR)/ti_msp_dl_config.o: $(BRD_DIR)/ti_msp_dl_config.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -c -o $@

$(OBJ_DIR)/ad7291_emulation.o: $(EMU_DIR)/ad7291_emulation.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -c -o $@

$(OBJ_DIR)/sc16is740_emulation.o: $(EMU_DIR)/sc16is740_emulation.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -c -o $@

$(OBJ_DIR)/eeprom_emulation_type_b.o: $(EMU_DIR)/eeprom_emulation_type_b.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -c -o $@

$(OBJ_DIR)/i2c_tar_driver.o: $(COM_DIR)/i2c_tar_driver.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -c -o $@

$(OBJ_DIR)/startup_mspm0l110x_gcc.o: $(SDK_ROOT)/ti/devices/msp/m0p/startup_system_files/gcc/startup_mspm0l110x_gcc.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -c -o $@

$(ELF): $(OBJECTS)
	@ echo linking $@
	@ $(LNK) $(OBJECTS) $(LFLAGS) -o $@

$(HEX): $(ELF)
	@ echo generating $@
	@ $(OBJCOPY) -O ihex --gap-fill 0xFF $< $@

.PHONY: clean
clean:
	$(info Cleaning...)
ifeq ($(OS),Windows_NT)
	- @if exist "$(OBJDIR)" rmdir /s /q "$(OBJDIR)"
	- @if exist "$(PROJECT).out" del /q /f "$(PROJECT).out"
	- @if exist "$(PROJECT).hex" del /q /f "$(PROJECT).hex"
	- @if exist "$(PROJECT).map" del /q /f "$(PROJECT).map"
else
	- @rm -rf $(OBJDIR) $(PROJECT).out $(PROJECT).hex $(PROJECT).map
endif
