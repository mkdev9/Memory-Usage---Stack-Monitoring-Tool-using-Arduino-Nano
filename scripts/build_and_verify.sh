#!/bin/bash
################################################################################
# Build and Verification Script for ATmega328P Memory Monitor
# 
# This script performs comprehensive build verification:
# - Checks for required toolchain
# - Compiles the firmware
# - Analyzes memory usage
# - Generates reports
################################################################################

set -e  # Exit on error

echo "================================================================================"
echo "  ATmega328P Memory Monitor - Build & Verification Script"
echo "================================================================================"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
MCU="atmega328p"
F_CPU="16000000UL"
TARGET="memory_monitor"

################################################################################
# Step 1: Check Toolchain
################################################################################

echo -e "${CYAN}[1/6] Checking AVR Toolchain...${NC}"
echo ""

MISSING_TOOLS=0

if command -v avr-gcc &> /dev/null; then
    echo -e "${GREEN}✓${NC} avr-gcc found: $(avr-gcc --version | head -n1)"
else
    echo -e "${RED}✗${NC} avr-gcc not found"
    MISSING_TOOLS=1
fi

if command -v avr-g++ &> /dev/null; then
    echo -e "${GREEN}✓${NC} avr-g++ found: $(avr-g++ --version | head -n1)"
else
    echo -e "${RED}✗${NC} avr-g++ not found"
    MISSING_TOOLS=1
fi

if command -v avr-objcopy &> /dev/null; then
    echo -e "${GREEN}✓${NC} avr-objcopy found"
else
    echo -e "${RED}✗${NC} avr-objcopy not found"
    MISSING_TOOLS=1
fi

if command -v avr-size &> /dev/null; then
    echo -e "${GREEN}✓${NC} avr-size found"
else
    echo -e "${RED}✗${NC} avr-size not found"
    MISSING_TOOLS=1
fi

if [ $MISSING_TOOLS -eq 1 ]; then
    echo ""
    echo -e "${RED}ERROR: Missing required tools. Install with:${NC}"
    echo "  Ubuntu/Debian: sudo apt-get install gcc-avr avr-libc binutils-avr"
    echo "  macOS:         brew tap osx-cross/avr && brew install avr-gcc"
    exit 1
fi

echo ""
echo -e "${GREEN}All required tools found!${NC}"
echo ""

################################################################################
# Step 2: Clean Previous Build
################################################################################

echo -e "${CYAN}[2/6] Cleaning previous build...${NC}"
make clean 2>/dev/null || true
echo -e "${GREEN}✓${NC} Clean complete"
echo ""

################################################################################
# Step 3: Compile Firmware
################################################################################

echo -e "${CYAN}[3/6] Compiling firmware...${NC}"
echo ""

if make all; then
    echo ""
    echo -e "${GREEN}✓${NC} Compilation successful!"
else
    echo ""
    echo -e "${RED}✗${NC} Compilation failed!"
    exit 1
fi

echo ""

################################################################################
# Step 4: Analyze Memory Usage
################################################################################

echo -e "${CYAN}[4/6] Analyzing memory usage...${NC}"
echo ""

# Display detailed size information
avr-size -C --mcu=${MCU} ${TARGET}.elf

echo ""

# Extract numeric values for analysis
FLASH_USED=$(avr-size ${TARGET}.elf | tail -n1 | awk '{print $1}')
DATA_USED=$(avr-size ${TARGET}.elf | tail -n1 | awk '{print $2}')
BSS_USED=$(avr-size ${TARGET}.elf | tail -n1 | awk '{print $3}')

FLASH_TOTAL=32768
SRAM_TOTAL=2048

SRAM_USED=$((DATA_USED + BSS_USED))
FLASH_PERCENT=$((FLASH_USED * 100 / FLASH_TOTAL))
SRAM_PERCENT=$((SRAM_USED * 100 / SRAM_TOTAL))

echo "Summary:"
echo "  Flash: ${FLASH_USED} / ${FLASH_TOTAL} bytes (${FLASH_PERCENT}%)"
echo "  SRAM:  ${SRAM_USED} / ${SRAM_TOTAL} bytes (${SRAM_PERCENT}%)"
echo ""

if [ $FLASH_PERCENT -gt 90 ]; then
    echo -e "${YELLOW}⚠${NC}  Warning: Flash usage > 90%"
fi

if [ $SRAM_PERCENT -gt 50 ]; then
    echo -e "${YELLOW}⚠${NC}  Warning: Static SRAM usage > 50%"
fi

echo -e "${GREEN}✓${NC} Memory analysis complete"
echo ""

################################################################################
# Step 5: Verify Critical Features
################################################################################

echo -e "${CYAN}[5/6] Verifying critical features...${NC}"
echo ""

# Check for malloc/free wrappers in object dump
if avr-objdump -t ${TARGET}.elf | grep -q "__wrap_malloc"; then
    echo -e "${GREEN}✓${NC} malloc wrapper found"
else
    echo -e "${RED}✗${NC} malloc wrapper NOT found (check linker flags)"
    exit 1
fi

if avr-objdump -t ${TARGET}.elf | grep -q "__wrap_free"; then
    echo -e "${GREEN}✓${NC} free wrapper found"
else
    echo -e "${RED}✗${NC} free wrapper NOT found (check linker flags)"
    exit 1
fi

# Check for key functions
if avr-objdump -t ${TARGET}.elf | grep -q "mem_monitor_init"; then
    echo -e "${GREEN}✓${NC} mem_monitor_init found"
else
    echo -e "${RED}✗${NC} mem_monitor_init NOT found"
    exit 1
fi

if avr-objdump -t ${TARGET}.elf | grep -q "uart_init"; then
    echo -e "${GREEN}✓${NC} uart_init found"
else
    echo -e "${RED}✗${NC} uart_init NOT found"
    exit 1
fi

# Verify no C++ exceptions or RTTI (bloat)
if avr-objdump -t ${TARGET}.elf | grep -q "__cxa_"; then
    echo -e "${YELLOW}⚠${NC}  Warning: C++ runtime overhead detected"
else
    echo -e "${GREEN}✓${NC} No C++ exceptions/RTTI overhead"
fi

echo ""
echo -e "${GREEN}✓${NC} Feature verification complete"
echo ""

################################################################################
# Step 6: Generate Reports
################################################################################

echo -e "${CYAN}[6/6] Generating reports...${NC}"
echo ""

# Generate disassembly
if avr-objdump -d ${TARGET}.elf > ${TARGET}.lst; then
    echo -e "${GREEN}✓${NC} Disassembly listing: ${TARGET}.lst"
else
    echo -e "${YELLOW}⚠${NC}  Could not generate disassembly"
fi

# Analyze map file
if [ -f ${TARGET}.map ]; then
    echo -e "${GREEN}✓${NC} Linker map file: ${TARGET}.map"
    
    # Extract key memory regions
    echo ""
    echo "Memory Map Highlights:"
    grep -A5 "Memory Configuration" ${TARGET}.map | head -n6 || true
    
    echo ""
    echo "Largest .text symbols (code):"
    grep "\.text\." ${TARGET}.map | awk '{print $3, $1}' | sort -rn | head -n5 || true
    
    echo ""
    echo "Largest .bss symbols (uninitialized data):"
    grep "\.bss\." ${TARGET}.map | awk '{print $3, $1}' | sort -rn | head -n5 || true
else
    echo -e "${YELLOW}⚠${NC}  Map file not found"
fi

echo ""
echo -e "${GREEN}✓${NC} Report generation complete"
echo ""

################################################################################
# Summary
################################################################################

echo "================================================================================"
echo -e "${GREEN}BUILD SUCCESSFUL!${NC}"
echo "================================================================================"
echo ""
echo "Generated files:"
echo "  ${TARGET}.elf - Executable with debug symbols"
echo "  ${TARGET}.hex - Flash image for programming"
echo "  ${TARGET}.map - Linker memory map"
echo "  ${TARGET}.lst - Disassembly listing"
echo ""
echo "Next steps:"
echo "  1. Review memory usage above"
echo "  2. Flash to device: make flash"
echo "  3. Monitor UART output at 115200 baud"
echo ""
echo "================================================================================"
