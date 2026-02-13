# Quick Start Guide - ATmega328P Memory Monitor

**Get up and running in 5 minutes**

---

## 🚀 Installation

### 1. Install AVR Toolchain

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install gcc-avr avr-libc avrdude
```

**macOS:**
```bash
brew tap osx-cross/avr
brew install avr-gcc avrdude
```

**Windows:**
- Download [WinAVR](https://sourceforge.net/projects/winavr/) or
- Install via [MSYS2](https://www.msys2.org/): `pacman -S mingw-w64-x86_64-avr-gcc`

### 2. Verify Installation

```bash
avr-gcc --version
# Should output: avr-gcc (GCC) 5.4.0 or newer
```

---

## ⚡ Build & Flash

### Build Firmware

```bash
# Navigate to project directory
cd ATmega328P-Memory-Monitor

# Build (creates build/memory_monitor.hex)
make

# Check memory usage
make size
```

### Expected Output:
```
========== Memory Usage ==========
AVR Memory Usage
----------------
Device: atmega328p

Program:    4856 bytes (14.8% Full)
(.text + .data + .bootloader)

Data:        245 bytes (12.0% Full)
(.data + .bss + .noinit)

========== Section Sizes ==========
   text    data     bss     dec     hex filename
   4856     245      87    5188    1444 build/memory_monitor.elf
```

### Flash to Arduino Nano

**Linux/macOS:**
```bash
# Auto-detect port and flash
make flash

# Or specify port manually
avrdude -p atmega328p -c arduino -P /dev/ttyUSB0 -b 115200 -U flash:w:build/memory_monitor.hex:i
```

**Windows:**
```powershell
# Specify COM port
avrdude -p atmega328p -c arduino -P COM3 -b 115200 -U flash:w:build/memory_monitor.hex:i
```

---

## 📡 Monitor Serial Output

### Linux
```bash
# Option 1: screen
screen /dev/ttyUSB0 115200

# Option 2: minicom
minicom -D /dev/ttyUSB0 -b 115200

# Option 3: picocom
picocom /dev/ttyUSB0 -b 115200
```

### macOS
```bash
screen /dev/cu.usbserial-* 115200
```

### Windows
```powershell
# Use PuTTY or Arduino IDE Serial Monitor
# Baud: 115200, 8N1, No flow control
```

### Arduino IDE
1. Open Arduino IDE
2. Tools → Serial Monitor
3. Set baud rate to **115200**
4. You should see:

```
[MEM] ========================================
[MEM] ATmega328P Memory Monitor - Initialized
[MEM] ========================================
[MEM] SRAM Total:    2048 bytes
[MEM] Static (.data + .bss): 245 bytes
[MEM] Stack Sentinel: Initialized
[MEM] ========================================

[MEM] Runtime Memory Diagnostics
[MEM] SRAM Total:     2048 bytes
[MEM] Static Memory:   245 bytes
[MEM] Heap Used:       0 bytes
[MEM] Stack Used:      67 bytes
[MEM] Max Stack:       67 bytes
[MEM] Free RAM:       1736 bytes
[MEM] Fragmentation:  0.0%
[MEM] ========================================
```

---

## 🧪 Run Tests

The firmware includes built-in test harness that runs automatically:

1. **Idle State** (2 seconds) - Baseline measurements
2. **Stack Growth Test** - Recursive function calls
3. **Heap Fragmentation Test** - Multiple malloc/free cycles
4. **Large Buffer Test** - Stack pressure simulation
5. **Repeat** - Continuous monitoring

Watch the serial output to see:
- Stack usage increasing during recursion
- Heap allocations/deallocations
- Fragmentation percentage changes
- Free RAM decreasing under load

---

## 📂 Project Navigation

| What You Need | Where to Look |
|---------------|---------------|
| **API Reference** | `docs/README.md` |
| **Technical Details** | `docs/TECHNICAL_NOTES.md` |
| **Modify Tests** | `src/main.cpp` |
| **Customize Monitor** | `src/memory_monitor.cpp` |
| **Change UART Baud** | `include/uart_driver.h` (line 15) |
| **Build Options** | `Makefile` |

---

## 🔧 Common Customizations

### Change UART Baud Rate

**File:** `include/uart_driver.h`
```cpp
// Change from 115200 to 9600
#define BAUD 9600
#define MYUBRR F_CPU/16/BAUD-1
```

### Adjust Safety Margin

**File:** `include/memory_monitor.h`
```cpp
// Change collision detection margin
#define STACK_HEAP_SAFETY_MARGIN 256  // Was 128
```

### Modify Update Interval

**File:** `src/main.cpp`
```cpp
// Change from 2 seconds to 5 seconds
_delay_ms(5000);  // Was 2000
```

### Change Stack Sentinel Pattern

**File:** `include/memory_monitor.h`
```cpp
// Change sentinel byte
#define STACK_SENTINEL_PATTERN 0x55  // Was 0xAA
```

---

## 🐛 Troubleshooting

### Build Issues

**Problem:** `avr-gcc: command not found`
- **Solution:** Install AVR toolchain (see Installation section)

**Problem:** `fatal error: avr/io.h: No such file or directory`
- **Solution:** Install `avr-libc` package

**Problem:** `make: *** No rule to make target 'build/main.o'`
- **Solution:** Run `make clean` then `make`

### Flash Issues

**Problem:** `avrdude: ser_open(): can't open device "/dev/ttyUSB0"`
- **Solution:** Check port name: `ls /dev/tty*`
- **Solution:** Add user to dialout group: `sudo usermod -a -G dialout $USER`

**Problem:** `avrdude: stk500_recv(): programmer is not responding`
- **Solution:** Reset Arduino before flashing
- **Solution:** Check USB cable connection
- **Solution:** Try different baud rate: `-b 57600`

### Serial Monitor Issues

**Problem:** No output visible
- **Solution:** Verify baud rate is **115200**
- **Solution:** Check USB connection
- **Solution:** Press reset button on Arduino

**Problem:** Garbage characters
- **Solution:** Incorrect baud rate - must be 115200
- **Solution:** Wrong line ending settings

---

## 📊 Understanding Output

### Memory Sections

```
[MEM] SRAM Total:     2048 bytes  ← Total available RAM
[MEM] Static Memory:   245 bytes  ← .data + .bss (global variables)
[MEM] Heap Used:       128 bytes  ← malloc() allocations
[MEM] Stack Used:       87 bytes  ← Local variables, call stack
[MEM] Free RAM:       1588 bytes  ← Unused space between heap & stack
```

### Fragmentation

```
[MEM] Fragmentation:  12.5%
```

- **0%** = Perfect (heap is contiguous)
- **< 25%** = Good
- **25-50%** = Moderate
- **> 50%** = High fragmentation

### Collision Warning

```
[MEM] WARNING: Stack/Heap Collision Imminent!
[MEM] Free RAM: 64 bytes (below safety margin)
```

This means your stack and heap are dangerously close!

---

## 📈 Next Steps

### For Learning
1. Read `docs/TECHNICAL_NOTES.md` to understand internals
2. Modify test harness in `src/main.cpp`
3. Experiment with different allocation patterns
4. Study generated `build/memory_monitor.map` file

### For Integration
1. Copy `src/memory_monitor.cpp` and `include/memory_monitor.h` to your project
2. Copy `src/uart_driver.cpp` and `include/uart_driver.h` (or use your own)
3. Call `MemoryMonitor_Init()` at startup
4. Call `printMemoryDiagnostics()` periodically
5. Update `Makefile` to include `--wrap=malloc --wrap=free`

### For Advanced Use
1. Add EEPROM logging (see `docs/TECHNICAL_NOTES.md`)
2. Integrate watchdog reset on collision
3. Create custom allocation tracking
4. Port to other AVR devices

---

## 🎯 Minimal Integration Example

```cpp
#include "memory_monitor.h"
#include "uart_driver.h"
#include <avr/io.h>
#include <util/delay.h>

int main(void) {
    // Initialize
    UART_Init();
    MemoryMonitor_Init();
    
    // Your application code
    while(1) {
        // Your main loop
        
        // Periodic diagnostics (every 5 seconds)
        printMemoryDiagnostics();
        _delay_ms(5000);
    }
    
    return 0;
}
```

**Compile with:**
```bash
avr-gcc -mmcu=atmega328p -DF_CPU=16000000UL -Os \
    -I./include \
    -Wl,--wrap=malloc -Wl,--wrap=free \
    main.cpp memory_monitor.cpp uart_driver.cpp \
    -o app.elf
```

---

## 📞 Support

- **Documentation:** See `docs/` directory
- **Examples:** See `src/main.cpp` for reference
- **Issues:** Check `docs/TECHNICAL_NOTES.md` FAQ section

---

**Happy Embedded Development! 🚀**
