# ATmega328P Memory Monitoring Framework

**Professional embedded firmware for runtime memory diagnostics on AVR microcontrollers**

[![Platform](https://img.shields.io/badge/Platform-ATmega328P-blue.svg)](https://www.microchip.com/en-us/product/ATmega328P)
[![Language](https://img.shields.io/badge/Language-C%2B%2B11-orange.svg)](https://en.cppreference.com/w/cpp/11)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

---

## 🎯 Overview

This is a **production-quality** memory monitoring framework designed for the ATmega328P microcontroller. It provides real-time diagnostics of SRAM usage, stack growth, heap fragmentation, and collision detection—all with minimal runtime overhead.


---

## 📁 Project Structure

```
.
├── src/                      # Source files
│   ├── main.cpp              # Application entry point & test harness
│   ├── memory_monitor.cpp    # Core memory monitoring implementation
│   └── uart_driver.cpp       # Lightweight UART driver
│
├── include/                  # Header files
│   ├── memory_monitor.h      # Memory monitor API
│   └── uart_driver.h         # UART driver API
│
├── build/                    # Build artifacts (auto-generated)
│   ├── *.o                   # Object files
│   ├── *.elf                 # Executable & Linkable Format
│   ├── *.hex                 # Intel HEX for flashing
│   ├── *.map                 # Linker memory map
│   └── *.lst                 # Assembly listing
│
├── docs/                     # Documentation
│   ├── README.md             # Full API reference & user guide
│   ├── TECHNICAL_NOTES.md    # Implementation deep-dive
│   ├── PROJECT_SUMMARY.md    # Executive overview
│   ├── SAMPLE_OUTPUT.txt     # Example UART output
│   └── FILE_MANIFEST.md      # Complete file listing
│
├── scripts/                  # Build & utility scripts
│   └── build_and_verify.sh   # Automated build verification
│
├── Makefile                  # Professional build system
└── README.md                 # This file
```

---

## ⚡ Quick Start

### Prerequisites

```bash
# Install AVR toolchain
sudo apt-get install avr-gcc avr-libc avrdude

# Verify installation
avr-gcc --version
```

### Build

```bash
# Build firmware
make

# View memory usage
make size

# Generate assembly listing
make disasm

# Clean build artifacts
make clean
```

### Flash to Device

```bash
# Upload via avrdude (adjust port as needed)
make flash
```

### Monitor Output

```bash
# Connect serial terminal at 115200 baud
screen /dev/ttyUSB0 115200
# or
minicom -D /dev/ttyUSB0 -b 115200
```

---

## 🔧 Features

### ✅ Memory Layout Analysis
- Reads AVR linker symbols (`__heap_start`, `__brkval`, `RAMEND`)
- Calculates static memory (.data + .bss)
- Tracks heap and stack regions
- Reports free RAM between heap/stack

### ✅ Stack Monitoring
- Sentinel pattern fill (0xAA) for unused SRAM
- Detects maximum stack penetration
- Low-overhead current stack usage tracking
- Stack growth direction analysis

### ✅ Heap Tracking
- Wraps `malloc()`/`free()` using GCC `--wrap` mechanism
- Tracks allocations, deallocations, and fragmentation
- Fixed-size allocation table (no dynamic memory)
- Fragmentation ratio calculation

### ✅ Collision Detection
- Monitors stack pointer vs heap boundary
- Configurable safety margin
- UART alert on dangerous proximity
- Optional watchdog reset trigger

### ✅ UART Diagnostics
- 115200 baud output
- Structured text format
- 2-second periodic updates
- Zero dynamic allocation

---

## 📊 Sample Output

```
[MEM] ========================================
[MEM] ATmega328P Memory Monitor - Initialized
[MEM] ========================================
[MEM] SRAM Total:    2048 bytes
[MEM] Static (.data + .bss): 245 bytes
[MEM] Stack Sentinel: Initialized
[MEM] ========================================

[MEM] ========================================
[MEM] Runtime Memory Diagnostics
[MEM] ========================================
[MEM] SRAM Total:     2048 bytes
[MEM] Static Memory:   245 bytes
[MEM] Heap Used:       128 bytes
[MEM] Stack Used:      87 bytes
[MEM] Max Stack:       87 bytes
[MEM] Free RAM:       1588 bytes
[MEM] Fragmentation:  12.5%
[MEM] Allocations:    5
[MEM] Deallocations:  2
[MEM] ========================================
```

---

## 🧪 Test Harness

The framework includes comprehensive stress tests:

- **Recursive Stack Test**: Forces deep stack growth
- **Heap Fragmentation Test**: Alternating malloc/free patterns
- **Large Buffer Test**: Stack pressure simulation
- **Collision Detection**: Validates safety margin triggers

Run all tests via `main.cpp` demonstration loop.

---

## 📚 Documentation

| Document | Description |
|----------|-------------|
| [`docs/README.md`](docs/README.md) | Complete API reference & usage guide |
| [`docs/TECHNICAL_NOTES.md`](docs/TECHNICAL_NOTES.md) | Deep-dive: AVR memory model, algorithms, overhead |
| [`docs/PROJECT_SUMMARY.md`](docs/PROJECT_SUMMARY.md) | Executive summary & architecture |
| [`docs/SAMPLE_OUTPUT.txt`](docs/SAMPLE_OUTPUT.txt) | Example runtime output |
| [`docs/FILE_MANIFEST.md`](docs/FILE_MANIFEST.md) | Complete file listing with line counts |

---

## 🏗️ Build System

Professional Makefile with:

- **Optimization**: `-Os` (size), LTO (Link-Time Optimization)
- **Section Garbage Collection**: Removes unused code
- **Wrap Mechanism**: `--wrap=malloc --wrap=free`
- **Memory Map**: Detailed `.map` file generation
- **Disassembly**: Optional `.lst` output
- **Size Analysis**: Automatic memory usage reporting

---

## 🎓 Engineering Principles

### Memory Safety
- No hidden heap allocation (no Arduino `String`)
- No STL containers
- Static allocation for tracking structures
- Deterministic behavior

### Performance
- Minimal ISR interference
- Low-overhead stack pointer reads (inline ASM)
- Efficient sentinel scanning
- Zero malloc during monitoring

### Modularity
- Clean header/source separation
- Well-defined API boundaries
- No cross-module dependencies
- Professional code organization

---

## 🔬 Technical Specifications

| Parameter | Value |
|-----------|-------|
| **MCU** | ATmega328P |
| **Clock** | 16 MHz |
| **SRAM** | 2 KB |
| **Flash** | 32 KB |
| **Toolchain** | avr-gcc (C++11) |
| **UART Baud** | 115200 |
| **Stack Sentinel** | 0xAA |
| **Safety Margin** | 128 bytes |

---

## 🚀 Advanced Features (Optional Extensions)

- **EEPROM Logging**: Persist peak stack usage across resets
- **Compile-Time Analysis**: Parse `.map` file for static analysis
- **Watchdog Integration**: Auto-reset on collision
- **Brown-Out Detection**: Log memory state on power issues
- **Guard Regions**: Memory protection zones

---

## 🤝 Contributing

This is a reference implementation for embedded systems  and professional use.

Contributions welcome:
- Bug fixes
- Performance improvements
- Additional test cases
- Documentation enhancements
