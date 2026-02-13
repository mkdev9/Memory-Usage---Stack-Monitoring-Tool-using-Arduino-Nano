# File Manifest - ATmega328P Memory Monitoring Framework

## Complete Project File Listing

This document provides a comprehensive overview of all delivered files, their purpose, and interdependencies.

---

## Project Structure

```
ATmega328P_Memory_Monitor/
│
├── Source Code (5 files, 1,004 lines)
│   ├── uart_driver.h          [62 lines]   UART driver interface
│   ├── uart_driver.cpp        [91 lines]   UART implementation
│   ├── memory_monitor.h       [158 lines]  Memory monitor API
│   ├── memory_monitor.cpp     [377 lines]  Memory monitor implementation
│   └── main.cpp               [316 lines]  Test harness and application
│
├── Build System (2 files, 296 lines)
│   ├── Makefile               [89 lines]   Professional build configuration
│   └── build_and_verify.sh    [207 lines]  Automated build verification
│
└── Documentation (4 files, 1,416 lines)
    ├── README.md              [460 lines]  User guide and API reference
    ├── TECHNICAL_NOTES.md     [454 lines]  Implementation deep-dive
    ├── SAMPLE_OUTPUT.txt      [169 lines]  Example UART output
    └── PROJECT_SUMMARY.md     [333 lines]  Executive overview

Total: 11 files, 2,716 lines
```

---

## File Details

### 1. uart_driver.h (62 lines)

**Purpose**: UART driver interface for diagnostic output

**Key Functions**:
```cpp
void uart_init(uint32_t baud, uint32_t f_cpu);
void uart_putc(char data);
void uart_puts(const char* str);
void uart_puts_P(const char* str);  // PROGMEM strings
void uart_print_u16(uint16_t value);
void uart_print_hex16(uint16_t value);
void uart_print_float(float value);
void uart_newline(void);
```

**Dependencies**: 
- `<avr/io.h>`
- `<stdint.h>`

**Used By**: `memory_monitor.cpp`, `main.cpp`

---

### 2. uart_driver.cpp (91 lines)

**Purpose**: UART driver implementation for ATmega328P USART0

**Features**:
- Blocking transmission (simple, minimal overhead)
- 115200 baud @ 16 MHz
- Optimized integer/float formatting (no printf)
- PROGMEM string support

**Hardware Configuration**:
- USART0 (TX on PD1/Arduino Pin 1)
- 8N1 frame format
- TX-only (no RX implementation)

**Dependencies**:
- `uart_driver.h`
- `<avr/pgmspace.h>`

---

### 3. memory_monitor.h (158 lines)

**Purpose**: Memory monitoring framework API and documentation

**Key Constants**:
```cpp
#define MAX_HEAP_ALLOCATIONS 32      // Allocation tracking table size
#define STACK_SENTINEL 0xAA          // Sentinel pattern
#define COLLISION_SAFETY_MARGIN 128  // Warning threshold
```

**Key Data Structures**:
```cpp
struct AllocationEntry {
    void* ptr;
    uint16_t size;
    uint8_t active;
};

struct MemoryStats {
    uint16_t total_sram;
    uint16_t static_data;
    uint16_t static_bss;
    uint16_t heap_used;
    uint16_t current_stack_usage;
    uint16_t max_stack_usage;
    uint16_t free_ram;
    float fragmentation_ratio;
    uint8_t collision_warning;
};
```

**API Functions**:
```cpp
// Initialization
void mem_monitor_init(void);

// Update and query
void mem_monitor_update(void);
void mem_monitor_get_stats(MemoryStats* stats);

// Specific queries
uint16_t mem_monitor_get_stack_pointer(void);
uint16_t mem_monitor_get_current_stack_usage(void);
uint16_t mem_monitor_get_max_stack_usage(void);
uint16_t mem_monitor_get_free_stack_space(void);
uint16_t mem_monitor_get_heap_used(void);
float mem_monitor_get_fragmentation_ratio(void);
uint8_t mem_monitor_check_collision(void);

// Output
void mem_monitor_print_diagnostics(void);

// Internal tracking (called by wrappers)
void mem_monitor_track_alloc(void* ptr, uint16_t size);
void mem_monitor_track_free(void* ptr);
```

**Dependencies**: `<stdint.h>`

**Used By**: `memory_monitor.cpp`, `main.cpp`

---

### 4. memory_monitor.cpp (377 lines)

**Purpose**: Core memory monitoring implementation

**Key Algorithms**:

1. **Stack Pointer Access** (inline assembly):
```cpp
uint16_t mem_monitor_get_stack_pointer(void) {
    // Atomic read of SPH:SPL with interrupts disabled
    // AVR-specific: SPL must be read before SPH
}
```

2. **Sentinel Pattern Fill**:
```cpp
void mem_monitor_init(void) {
    // Fill gap between heap and stack with 0xAA
    for (uint8_t* ptr = heap_end; ptr < stack_ptr; ptr++) {
        *ptr = STACK_SENTINEL;
    }
}
```

3. **Stack Usage Scanning**:
```cpp
static uint16_t scan_stack_usage(void) {
    // Scan upward from heap until non-sentinel found
    // This reveals maximum stack penetration
}
```

4. **malloc/free Wrapping** (GCC --wrap mechanism):
```cpp
extern "C" {
    void* __wrap_malloc(size_t size) {
        void* ptr = __real_malloc(size);
        mem_monitor_track_alloc(ptr, size);
        return ptr;
    }
    
    void __wrap_free(void* ptr) {
        mem_monitor_track_free(ptr);
        __real_free(ptr);
    }
}
```

**Linker Symbols Used**:
- `__heap_start` - Heap region start
- `__brkval` - Current heap end (updated by malloc)
- `__data_start`, `__data_end` - .data segment bounds
- `__bss_start`, `__bss_end` - .bss segment bounds
- `RAMEND` - Top of SRAM (0x08FF for ATmega328P)

**Dependencies**:
- `memory_monitor.h`
- `uart_driver.h`
- `<avr/io.h>`
- `<avr/pgmspace.h>`
- `<string.h>`
- `<stdlib.h>`

---

### 5. main.cpp (316 lines)

**Purpose**: Test harness demonstrating memory monitoring

**Test Functions**:

1. **Recursive Stack Test**:
```cpp
void recursive_stack_test(uint8_t depth) {
    volatile char buffer[32];  // Consume stack
    if (depth < 10) {
        recursive_stack_test(depth + 1);
    }
}
```

2. **Heap Fragmentation Test**:
```cpp
void heap_fragmentation_test(void) {
    // Allocate 8 blocks
    // Free alternating blocks (creates fragmentation)
    // Allocate new blocks (may not fit)
}
```

3. **Large Buffer Test**:
```cpp
void large_buffer_test(void) {
    volatile uint8_t large_buffer[256];
    // Triggers collision warning if heap also allocated
}
```

4. **Combined Stress Test**:
```cpp
void combined_stress_test(void) {
    // Allocate heap blocks
    // Then recurse (stresses both heap and stack)
}
```

**Main Loop**:
```cpp
int main(void) {
    uart_init(115200, F_CPU);
    mem_monitor_init();
    
    // Run test sequence
    // Enter continuous monitoring loop
}
```

**Dependencies**:
- `<avr/io.h>`
- `<avr/interrupt.h>`
- `<util/delay.h>`
- `<stdlib.h>`
- `uart_driver.h`
- `memory_monitor.h`

---

### 6. Makefile (89 lines)

**Purpose**: Professional build system for AVR firmware

**Key Configuration**:
```make
MCU = atmega328p
F_CPU = 16000000UL
TARGET = memory_monitor

CFLAGS = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall -Wextra
         -std=gnu++11 -ffunction-sections -fdata-sections
         -fno-exceptions -fno-threadsafe-statics -flto

LDFLAGS = -mmcu=$(MCU) -Wl,--gc-sections -flto
          -Wl,--wrap=malloc -Wl,--wrap=free
          -Wl,-Map=$(TARGET).map
```

**Targets**:
- `all` - Build firmware (default)
- `clean` - Remove build artifacts
- `size` - Display memory usage
- `flash` - Upload to device
- `disasm` - Generate assembly listing
- `memmap` - Show detailed memory map
- `help` - Display help

**Output Files**:
- `memory_monitor.elf` - Executable with debug symbols
- `memory_monitor.hex` - Flash image for programming
- `memory_monitor.map` - Linker memory map
- `memory_monitor.lst` - Disassembly listing (optional)

**Critical Linker Flags**:
- `--wrap=malloc` - Intercept malloc calls
- `--wrap=free` - Intercept free calls
- `--gc-sections` - Remove unused code
- `-Map=<file>` - Generate memory map

---

### 7. build_and_verify.sh (207 lines)

**Purpose**: Automated build verification script (Linux/macOS)

**Verification Steps**:
1. Check for AVR toolchain (avr-gcc, avr-g++, avr-objcopy, avr-size)
2. Clean previous build
3. Compile firmware
4. Analyze memory usage (Flash/SRAM)
5. Verify critical features (malloc/free wrappers, key functions)
6. Generate reports (disassembly, memory map analysis)

**Usage**:
```bash
chmod +x build_and_verify.sh
./build_and_verify.sh
```

**Exit Codes**:
- 0: Success (all checks passed)
- 1: Failure (missing tools, compilation error, or verification failed)

---

### 8. README.md (460 lines)

**Purpose**: Comprehensive user guide and API reference

**Sections**:
- Overview and features
- Hardware requirements
- ATmega328P memory architecture explanation
- Project structure
- Build instructions
- API reference with examples
- Stack monitoring mechanism
- Heap tracking implementation
- Collision detection logic
- Test harness description
- Runtime overhead analysis
- Example output
- Advanced extensions
- Engineering constraints
- Troubleshooting
- References

**Target Audience**: Embedded systems developers integrating the framework

---

### 9. TECHNICAL_NOTES.md (454 lines)

**Purpose**: Deep-dive technical implementation details

**Sections**:
1. AVR stack pointer access (inline assembly)
2. Linker symbol integration
3. Stack sentinel pattern technique
4. malloc/free wrapper implementation
5. Fragmentation analysis
6. Collision detection logic
7. UART driver design
8. Performance analysis
9. Build system details
10. Testing strategy
11. Common pitfalls
12. Production deployment checklist
13. Future enhancements
14. References

**Target Audience**: Advanced developers wanting to understand or modify implementation

---

### 10. SAMPLE_OUTPUT.txt (169 lines)

**Purpose**: Example UART diagnostic output for verification

**Contents**:
- Baseline measurements
- Recursive stack test output
- Heap fragmentation test results
- Large buffer test results
- Combined stress test output
- Continuous monitoring samples
- Collision warning scenario example

**Usage**: Compare actual UART output against this reference to verify correct operation

---

### 11. PROJECT_SUMMARY.md (333 lines)

**Purpose**: Executive overview and project deliverable summary

**Sections**:
- Executive summary
- Deliverables table
- Key features implemented
- Technical architecture
- Performance characteristics
- Engineering standards met
- Test coverage
- Compilation instructions
- Usage examples
- Advanced extensions
- Compliance and standards
- Project statistics
- Conclusion

**Target Audience**: Project managers, technical leads, and stakeholders

---

## Dependency Graph

```
main.cpp
├── uart_driver.h
│   └── uart_driver.cpp
└── memory_monitor.h
    └── memory_monitor.cpp
        └── uart_driver.h
            └── uart_driver.cpp

Makefile
└── [all .cpp files]
    └── [builds .elf, .hex, .map]

build_and_verify.sh
└── Makefile
    └── [verifies build output]
```

---

## Compilation Order

1. **uart_driver.cpp** → `uart_driver.o`
   - Depends on: `uart_driver.h`, AVR headers

2. **memory_monitor.cpp** → `memory_monitor.o`
   - Depends on: `memory_monitor.h`, `uart_driver.h`, AVR headers

3. **main.cpp** → `main.o`
   - Depends on: `uart_driver.h`, `memory_monitor.h`, AVR headers

4. **Link** → `memory_monitor.elf`
   - Links: `uart_driver.o`, `memory_monitor.o`, `main.o`
   - Wraps: `malloc`, `free`
   - Generates: `.elf`, `.hex`, `.map`

---

## Size Breakdown

| Component | Files | Lines | Percentage |
|-----------|-------|-------|------------|
| Source Code | 5 | 1,004 | 37.0% |
| Build System | 2 | 296 | 10.9% |
| Documentation | 4 | 1,416 | 52.1% |
| **Total** | **11** | **2,716** | **100%** |

---

## Critical Files for Different Use Cases

### For Quick Integration
1. `uart_driver.h` + `uart_driver.cpp`
2. `memory_monitor.h` + `memory_monitor.cpp`
3. `Makefile` (modify LDFLAGS for your project)

### For Understanding Implementation
1. `README.md` (overview)
2. `TECHNICAL_NOTES.md` (deep-dive)
3. `memory_monitor.cpp` (core algorithms)

### For Testing and Verification
1. `main.cpp` (test harness)
2. `build_and_verify.sh` (automated verification)
3. `SAMPLE_OUTPUT.txt` (expected results)

### For Project Management
1. `PROJECT_SUMMARY.md` (executive overview)
2. `README.md` (feature list)
3. `Makefile` (build configuration)

---

## File Modification Guidelines

### Customization Points

**To adjust allocation table size**:
- Edit `MAX_HEAP_ALLOCATIONS` in `memory_monitor.h`

**To change collision margin**:
- Edit `COLLISION_SAFETY_MARGIN` in `memory_monitor.h`

**To modify UART baud rate**:
- Edit `UART_BAUD` in `main.cpp` or pass to `uart_init()`

**To change sentinel pattern**:
- Edit `STACK_SENTINEL` in `memory_monitor.h`

**To port to different AVR**:
- Edit `MCU` in `Makefile`
- Verify RAMEND value in `memory_monitor.cpp`
- Adjust UART registers if USART location differs

---

## Build Artifacts (Generated)

After running `make all`, the following files are generated:

```
uart_driver.o           Object file (intermediate)
memory_monitor.o        Object file (intermediate)
main.o                  Object file (intermediate)
memory_monitor.elf      Executable with debug symbols
memory_monitor.hex      Flash image (upload this)
memory_monitor.map      Linker memory map (for analysis)
memory_monitor.lst      Disassembly listing (optional, via make disasm)
```

**Note**: `.o` and `.lst` files can be deleted after build. Keep `.elf`, `.hex`, and `.map` for deployment and debugging.

---

## Version Control Recommendations

### Include in Repository
- All source files (.h, .cpp)
- Build system (Makefile, .sh)
- Documentation (.md, .txt)
- README with build instructions

### Exclude from Repository (.gitignore)
```
*.o
*.elf
*.hex
*.map
*.lst
```

---

## Checksums (for integrity verification)

Use `sha256sum` (Linux) or `shasum -a 256` (macOS) to verify file integrity:

```bash
sha256sum *.h *.cpp Makefile *.md *.txt *.sh > CHECKSUMS.txt
```

---

## License and Attribution

This framework is provided as a production-quality reference implementation for embedded systems education and professional use.

**Recommended Attribution**:
```
ATmega328P Memory Monitoring Framework
Production-quality runtime diagnostics for AVR microcontrollers
```

---

## End of File Manifest

**Last Updated**: 2026-02-13  
**Total Files**: 11  
**Total Lines**: 2,716  
**Framework Version**: 1.0  
**Target Platform**: ATmega328P (Arduino Nano compatible)
