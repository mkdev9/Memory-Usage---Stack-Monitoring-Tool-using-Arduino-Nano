# Technical Implementation Notes

## Memory Monitor Framework - Deep Dive

---

## 1. AVR Stack Pointer Access

### Why Inline Assembly?

The AVR stack pointer is stored in I/O registers `SPH:SPL` (0x3D:0x3E). Direct C access is not portable across compilers.

### Implementation

```cpp
uint16_t mem_monitor_get_stack_pointer(void) {
    uint8_t sreg;
    uint16_t sp;
    
    // Save SREG and disable interrupts
    sreg = SREG;
    __asm__ __volatile__ ("cli");
    
    // Read SPL before SPH (AVR requirement)
    sp = SPL;
    sp |= ((uint16_t)SPH << 8);
    
    // Restore SREG
    SREG = sreg;
    
    return sp;
}
```

### Critical Details

1. **Interrupt Safety**: SP read must be atomic (interrupts disabled)
2. **Read Order**: SPL MUST be read before SPH on AVR architecture
3. **SREG Preservation**: Restore interrupt state to avoid side effects

---

## 2. Linker Symbol Integration

### Symbol Declarations

```cpp
extern uint8_t __heap_start;   // Heap start address
extern uint8_t *__brkval;      // Current heap end (set by malloc)
extern uint8_t __data_start;   // .data segment start
extern uint8_t __data_end;     // .data segment end
extern uint8_t __bss_start;    // .bss segment start
extern uint8_t __bss_end;      // .bss segment end
```

### Memory Map Interpretation

```
Symbol          Address     Meaning
------          -------     -------
__data_start    0x0100      First initialized global variable
__data_end      0x010C      End of .data (12 bytes)
__bss_start     0x010C      First zero-init global variable
__bss_end       0x01B8      End of .bss (172 bytes)
__heap_start    0x01B8      Available heap starts here
__brkval        0x0220      Current heap end (104 bytes allocated)
<gap>           0x0220-SP   Free RAM (critical safety margin)
SP              0x07F0      Current stack pointer
RAMEND          0x08FF      Top of SRAM
```

### Usage Example

```cpp
// Calculate static memory usage
uint16_t static_data = (uint16_t)&__data_end - (uint16_t)&__data_start;
uint16_t static_bss = (uint16_t)&__bss_end - (uint16_t)&__bss_start;
uint16_t total_static = static_data + static_bss;

// Get heap end
uint8_t* heap_end = __brkval ? __brkval : &__heap_start;

// Calculate free RAM
uint16_t current_sp = mem_monitor_get_stack_pointer();
uint16_t free_ram = current_sp - (uint16_t)heap_end;
```

---

## 3. Stack Sentinel Pattern Technique

### Concept

Fill unused SRAM with known pattern (0xAA). As stack grows, it overwrites sentinels. Scanning for first non-sentinel reveals deepest penetration.

### Visual Representation

```
Initial State (after mem_monitor_init):
┌──────────┐
│ Heap     │ 0x0200
├──────────┤
│ 0xAA     │
│ 0xAA     │ Sentinel fill
│ 0xAA     │
│ 0xAA     │
├──────────┤
│ Stack    │ SP = 0x08F0
└──────────┘

After Function Call:
┌──────────┐
│ Heap     │ 0x0200
├──────────┤
│ 0xAA     │ Untouched sentinels
│ 0xAA     │
│ 0xAA     │
├──────────┤
│ 0x1F     │ Stack data (overwrote sentinel)
│ 0x42     │
├──────────┤
│ Stack    │ SP = 0x08E8 (grew 8 bytes)
└──────────┘

Scanning upward from heap:
- 0xAA, 0xAA, 0xAA → continue
- 0x1F → STOP! This is max penetration point
- Max stack = RAMEND - this_address
```

### Implementation

```cpp
void mem_monitor_init(void) {
    uint8_t* heap_end = __brkval ? __brkval : &__heap_start;
    uint8_t* stack_ptr = (uint8_t*)mem_monitor_get_stack_pointer();
    
    // Fill gap with sentinel
    for (uint8_t* ptr = heap_end; ptr < stack_ptr; ptr++) {
        *ptr = STACK_SENTINEL;
    }
}

static uint16_t scan_stack_usage(void) {
    uint8_t* heap_end = __brkval ? __brkval : &__heap_start;
    uint8_t* scan_ptr = heap_end;
    
    // Scan upward
    while (*scan_ptr == STACK_SENTINEL && scan_ptr < (uint8_t*)RAMEND) {
        scan_ptr++;
    }
    
    return RAMEND - (uint16_t)scan_ptr;
}
```

### Advantages

- Detects maximum stack usage **even after stack unwinds**
- No runtime overhead (scan only when reporting)
- Works with interrupts and nested calls

### Limitations

- Assumes application doesn't intentionally write 0xAA to stack region
- Cannot distinguish between stack and heap writes (both overwrite sentinel)

---

## 4. malloc/free Wrapper Implementation

### GCC --wrap Mechanism

Linker flag `-Wl,--wrap=malloc` creates:
- `__wrap_malloc`: Your wrapper function
- `__real_malloc`: Original malloc implementation

```
Application Code:
    ptr = malloc(100);
         ↓
    Linker redirects to __wrap_malloc
         ↓
    __wrap_malloc(100) {
        void* p = __real_malloc(100);  ← Calls real allocator
        track_alloc(p, 100);
        return p;
    }
```

### Wrapper Implementation

```cpp
extern "C" {
    extern void* __real_malloc(size_t size);
    extern void __real_free(void* ptr);
    
    void* __wrap_malloc(size_t size) {
        void* ptr = __real_malloc(size);  // Call original
        mem_monitor_track_alloc(ptr, (uint16_t)size);  // Track
        return ptr;
    }
    
    void __wrap_free(void* ptr) {
        mem_monitor_track_free(ptr);  // Track
        __real_free(ptr);  // Call original
    }
}
```

### Tracking Table

Fixed-size array (no dynamic allocation in tracker itself):

```cpp
struct AllocationEntry {
    void* ptr;          // Allocated pointer
    uint16_t size;      // Block size
    uint8_t active;     // 1=allocated, 0=freed
};

static AllocationEntry s_alloc_table[MAX_HEAP_ALLOCATIONS];

void mem_monitor_track_alloc(void* ptr, uint16_t size) {
    for (uint8_t i = 0; i < MAX_HEAP_ALLOCATIONS; i++) {
        if (s_alloc_table[i].active == 0) {
            s_alloc_table[i].ptr = ptr;
            s_alloc_table[i].size = size;
            s_alloc_table[i].active = 1;
            
            s_mem_state.heap_used += size;
            s_mem_state.alloc_count++;
            return;
        }
    }
    // Table full - critical error!
}
```

### Build Configuration

Makefile must include:

```make
LDFLAGS += -Wl,--wrap=malloc -Wl,--wrap=free
```

---

## 5. Fragmentation Analysis

### Challenge

True fragmentation requires scanning malloc's internal free list, which is implementation-dependent. We use heuristic approach.

### Heuristic Metric

```cpp
float mem_monitor_get_fragmentation_ratio(void) {
    uint16_t total_heap = (uint16_t)heap_end - (uint16_t)heap_start;
    uint16_t total_free = total_heap - s_mem_state.heap_used;
    
    if (total_free == 0) return 0.0f;
    
    // Heuristic: many small active allocations = fragmentation
    if (s_mem_state.alloc_count > s_mem_state.free_count + 5) {
        float frag = (float)(s_mem_state.alloc_count - s_mem_state.free_count) / 
                     (float)MAX_HEAP_ALLOCATIONS;
        return (frag > 1.0f) ? 1.0f : frag;
    }
    
    return 0.0f;
}
```

### Ideal Metric (Requires malloc internals)

```cpp
// Hypothetical - requires access to free list
float calculate_exact_fragmentation(void) {
    uint16_t largest_free_block = find_largest_free_block();
    uint16_t total_free = calculate_total_free();
    
    return 1.0f - ((float)largest_free_block / (float)total_free);
}
```

### Interpretation

- **0.0**: No fragmentation (all free space contiguous)
- **0.5**: Moderate fragmentation
- **1.0**: Severe fragmentation (no large blocks available)

---

## 6. Collision Detection Logic

### Safety Margin

```cpp
#define COLLISION_SAFETY_MARGIN 128  // bytes

uint8_t mem_monitor_check_collision(void) {
    uint16_t current_sp = mem_monitor_get_stack_pointer();
    uint8_t* heap_end = __brkval ? __brkval : &__heap_start;
    
    uint16_t gap = current_sp - (uint16_t)heap_end;
    
    if (gap < COLLISION_SAFETY_MARGIN) {
        s_mem_state.collision_warning = 1;
        return 1;
    }
    
    return 0;
}
```

### Why 128 Bytes?

- Typical ISR stack frame: 20-40 bytes
- Nested ISRs: 2-3 levels = 60-120 bytes
- Safety factor: 128 bytes handles most cases

### Response Actions

```cpp
if (mem_monitor_check_collision()) {
    uart_puts_P(PSTR("*** CRITICAL: COLLISION WARNING ***\r\n"));
    mem_monitor_print_diagnostics();
    
    // Optional: Trigger watchdog reset
    wdt_enable(WDTO_15MS);
    while(1);
}
```

---

## 7. UART Driver Design

### Blocking vs. Interrupt-Driven

**Choice**: Blocking transmission  
**Rationale**:
- Simpler implementation (no ring buffers)
- Acceptable for diagnostics (not time-critical)
- Minimal memory overhead

### Integer Formatting

Avoid `printf()` overhead (1-2 KB flash):

```cpp
void uart_print_u16(uint16_t value) {
    static char buffer[6];
    char* ptr = buffer + sizeof(buffer) - 1;
    *ptr = '\0';
    
    if (value == 0) {
        uart_putc('0');
        return;
    }
    
    while (value > 0) {
        *--ptr = '0' + (value % 10);
        value /= 10;
    }
    
    uart_puts(ptr);
}
```

### PROGMEM Strings

Save SRAM by storing strings in flash:

```cpp
uart_puts_P(PSTR("Static string in flash\r\n"));

void uart_puts_P(const char* str) {
    char c;
    while ((c = pgm_read_byte(str++))) {
        uart_putc(c);
    }
}
```

---

## 8. Performance Analysis

### Memory Overhead

| Component | Bytes | Calculation |
|-----------|-------|-------------|
| Allocation table | 160 | 32 × (4 + 2 + 1) = 32 × 5 |
| State structure | 18 | 6 × uint16_t + 1 × uint8_t |
| UART buffers | 6 | Static formatting buffers |
| **Total** | **184** | **9.0% of 2048 bytes** |

### CPU Overhead

| Operation | Cycles | Frequency |
|-----------|--------|-----------|
| Stack pointer read | 15 | Per update (~1 Hz) |
| Stack scan | ~500 | Per update |
| Heap track (alloc) | ~30 | Per malloc |
| Heap track (free) | ~25 | Per free |
| Collision check | 10 | Per update |

**Impact**: <0.1% CPU @ 16 MHz with 1 Hz updates

---

## 9. Build System Details

### Compiler Flags

```make
CFLAGS = -mmcu=atmega328p        # Target MCU
         -DF_CPU=16000000UL      # CPU frequency
         -Os                     # Optimize for size
         -Wall -Wextra           # All warnings
         -std=gnu++11            # C++11 standard
         -ffunction-sections     # Each function in own section
         -fdata-sections         # Each data in own section
         -fno-exceptions         # No C++ exceptions (save flash)
         -fno-threadsafe-statics # No thread-safe init (single-threaded)
         -flto                   # Link-time optimization
```

### Linker Flags

```make
LDFLAGS = -mmcu=atmega328p
          -Wl,--gc-sections      # Remove unused sections
          -flto                  # Link-time optimization
          -Wl,--wrap=malloc      # Wrap malloc
          -Wl,--wrap=free        # Wrap free
          -Wl,-Map=$(TARGET).map # Generate memory map
```

### Size Optimization

- LTO (Link-Time Optimization): Inlines across compilation units
- Section garbage collection: Removes unused code
- No exceptions/RTTI: Saves 1-2 KB flash

---

## 10. Testing Strategy

### Test Coverage

| Test | Purpose | Stack | Heap |
|------|---------|-------|------|
| Baseline | Initial state | Minimal | None |
| Recursive | Stack growth | High | None |
| Fragmentation | Heap patterns | Low | Fragmented |
| Large buffer | Stack pressure | Very high | None |
| Combined | Both stressed | High | High |

### Test Sequence

```cpp
main() {
    mem_monitor_init();
    
    // 1. Baseline
    mem_monitor_print_diagnostics();
    
    // 2. Stack test
    recursive_stack_test(10);
    
    // 3. Heap test
    heap_fragmentation_test();
    
    // 4. Combined
    combined_stress_test();
    
    // 5. Continuous monitoring
    while(1) {
        mem_monitor_update();
        periodic_report();
    }
}
```

---

## 11. Common Pitfalls

### Issue: Stack Usage Shows Zero

**Cause**: Sentinel pattern overwritten during initialization  
**Fix**: Call `mem_monitor_init()` early, before large stack usage

### Issue: Heap Tracking Table Full

**Cause**: More than 32 simultaneous allocations  
**Fix**: Increase `MAX_HEAP_ALLOCATIONS` or redesign allocation strategy

### Issue: Fragmentation Always Zero

**Cause**: Heuristic doesn't trigger (few allocations)  
**Fix**: Implement true free-list scanning or adjust heuristic threshold

### Issue: Collision Warning Spurious

**Cause**: Safety margin too large for application  
**Fix**: Reduce `COLLISION_SAFETY_MARGIN` constant

---

## 12. Production Deployment Checklist

- [ ] Tune `MAX_HEAP_ALLOCATIONS` for application needs
- [ ] Adjust `COLLISION_SAFETY_MARGIN` based on ISR stack usage
- [ ] Test with worst-case workload scenarios
- [ ] Verify UART output doesn't block time-critical paths
- [ ] Log peak usage to EEPROM for field diagnostics
- [ ] Add watchdog integration for collision response
- [ ] Disable diagnostic printing in production (keep tracking)
- [ ] Validate stack sentinel assumptions (no 0xAA usage)
- [ ] Test with compiler optimizations enabled (-Os, -O2, -O3)
- [ ] Review map file for unexpected memory layout

---

## 13. Future Enhancements

### Advanced Features

1. **Guard Bytes**: Add canary values at heap boundaries
2. **Stack Painting**: Use multiple sentinel patterns for finer resolution
3. **ISR Stack Analysis**: Separate ISR stack tracking
4. **Heap Visualization**: Dump heap map via UART
5. **Runtime Assertions**: `assert(free_ram > MIN_FREE)` with halt
6. **Telemetry**: Export metrics via I2C/SPI to external logger
7. **Historical Logging**: Track usage over time in EEPROM
8. **Compile-Time Bounds**: Static assertion on maximum stack usage

### Example: Guard Bytes

```cpp
#define HEAP_GUARD_SIZE 4
static uint8_t heap_guard[HEAP_GUARD_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};

void check_heap_guard(void) {
    uint8_t* guard_location = &__heap_start - HEAP_GUARD_SIZE;
    for (uint8_t i = 0; i < HEAP_GUARD_SIZE; i++) {
        if (guard_location[i] != heap_guard[i]) {
            // Guard overwritten - heap corruption!
            trigger_fault();
        }
    }
}
```

---

## 14. References

### AVR Architecture
- AVR Instruction Set Manual (0856)
- ATmega328P Datasheet (DS40002061A)
- avr-libc User Manual (nongnu.org/avr-libc)

### Memory Management
- "Dynamic Memory Allocation on Embedded Systems" - Jack Ganssle
- "Embedded C Coding Standard" - Michael Barr
- "Making Embedded Systems" - Elecia White

### GCC/Linker
- GCC AVR Options Documentation
- GNU ld --wrap documentation
- avr-libc Linker Scripts

---

**End of Technical Notes**
