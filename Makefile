##############################################################################
# Makefile for ATmega328P Memory Monitoring Framework
# 
# Professional embedded build system for production firmware
##############################################################################

# Target MCU
MCU = atmega328p

# CPU frequency (16 MHz for Arduino Nano)
F_CPU = 16000000UL

# Target executable
TARGET = memory_monitor

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

# Source files
SOURCES = $(SRC_DIR)/main.cpp $(SRC_DIR)/memory_monitor.cpp $(SRC_DIR)/uart_driver.cpp

# Include paths
INCLUDES = -I$(INC_DIR)

# Compiler
CC = avr-gcc
CXX = avr-g++
OBJCOPY = avr-objcopy
OBJDUMP = avr-objdump
SIZE = avr-size

# Compiler flags
CFLAGS = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall -Wextra -std=gnu++11
CFLAGS += -ffunction-sections -fdata-sections -fno-exceptions -fno-threadsafe-statics
CFLAGS += -flto $(INCLUDES)

# Linker flags
LDFLAGS = -mmcu=$(MCU) -Wl,--gc-sections -flto
# CRITICAL: Add --wrap flags for malloc/free interception
LDFLAGS += -Wl,--wrap=malloc -Wl,--wrap=free
# Generate map file for memory analysis
LDFLAGS += -Wl,-Map=$(TARGET).map

# Object files (placed in build directory)
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES))

# Output files
HEX_FLASH = $(BUILD_DIR)/$(TARGET).hex
ELF_FILE = $(BUILD_DIR)/$(TARGET).elf
MAP_FILE = $(BUILD_DIR)/$(TARGET).map
LST_FILE = $(BUILD_DIR)/$(TARGET).lst

##############################################################################
# Build targets
##############################################################################

.PHONY: all clean size flash

all: $(ELF_FILE) $(HEX_FLASH) size

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Link
$(ELF_FILE): $(OBJECTS) | $(BUILD_DIR)
	$(CXX) $(OBJECTS) $(LDFLAGS) -Wl,-Map=$(MAP_FILE) -o $@

# Compile
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CFLAGS) -c $< -o $@

# Generate hex file
$(HEX_FLASH): $(ELF_FILE)
	$(OBJCOPY) -O ihex -R .eeprom $< $@

# Display size information
size: $(ELF_FILE)
	@echo ""
	@echo "========== Memory Usage =========="
	$(SIZE) -C --mcu=$(MCU) $(ELF_FILE)
	@echo ""
	@echo "========== Section Sizes =========="
	$(SIZE) -A $(ELF_FILE)
	@echo ""

# Generate assembly listing
disasm: $(ELF_FILE)
	$(OBJDUMP) -d $(ELF_FILE) > $(LST_FILE)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Flash to device (using avrdude)
flash: $(HEX_FLASH)
	avrdude -p $(MCU) -c arduino -P /dev/ttyUSB0 -b 115200 -U flash:w:$(HEX_FLASH):i

# Display detailed memory map
memmap: $(ELF_FILE)
	@echo "========== Detailed Memory Map =========="
	@grep -E "(\\.data|\\.bss|\\.text)" $(MAP_FILE) || true
	@echo ""
	@echo "See $(MAP_FILE) for complete linker map"

##############################################################################
# Help
##############################################################################

help:
	@echo "ATmega328P Memory Monitor Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build firmware (default)"
	@echo "  clean    - Remove build artifacts"
	@echo "  size     - Display memory usage"
	@echo "  flash    - Upload to device via avrdude"
	@echo "  disasm   - Generate assembly listing"
	@echo "  memmap   - Show detailed memory map"
	@echo "  help     - Show this help"
	@echo ""
	@echo "Configuration:"
	@echo "  MCU   = $(MCU)"
	@echo "  F_CPU = $(F_CPU)"
