/**
 * @file memory_monitor.cpp
 * @brief Memory monitoring framework implementation
 * 
 * Implements runtime memory diagnostics for ATmega328P:
 * - Stack growth monitoring via sentinel pattern
 * - Heap tracking via malloc/free interception
 * - Collision detection between stack and heap
 * - Fragmentation analysis
 */

#include "memory_monitor.h"
#include "uart_driver.h"
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <string.h>
#include <stdlib.h>

// ============================================================================
// LINKER SYMBOLS (defined by avr-gcc linker)
// ============================================================================

extern uint8_t __heap_start;   // Start of heap region
extern uint8_t *__brkval;       // Current heap end (updated by malloc)
extern uint8_t __data_start;    // Start of .data segment
extern uint8_t __data_end;      // End of .data segment
extern uint8_t __bss_start;     // Start of .bss segment
extern uint8_t __bss_end;       // End of .bss segment

// ============================================================================
// STATIC STATE
// ============================================================================

// Heap allocation tracking table (fixed size, no dynamic allocation)
static AllocationEntry s_alloc_table[MAX_HEAP_ALLOCATIONS];

// Memory statistics
static struct {
    uint16_t init_stack_pointer;    // SP value at initialization
    uint16_t max_stack_usage;       // Peak stack penetration
    uint16_t heap_used;             // Current heap allocation
    uint16_t heap_total_allocated;  // Cumulative allocated
    uint16_t heap_total_freed;      // Cumulative freed
    uint16_t alloc_count;           // Number of malloc calls
    uint16_t free_count;            // Number of free calls
    uint8_t collision_warning;      // Collision flag
} s_mem_state;

// ============================================================================
// STACK POINTER ACCESS (inline assembly)
// ============================================================================

/**
 * @brief Read AVR stack pointer (SPH:SPL)
 * @return 16-bit stack pointer value
 * 
 * AVR stack pointer is stored in two 8-bit registers:
 * - SPH: Stack Pointer High byte
 * - SPL: Stack Pointer Low byte
 * 
 * Must disable interrupts during read to ensure atomicity.
 */
uint16_t mem_monitor_get_stack_pointer(void) {
    uint8_t sreg;
    uint16_t sp;
    
    // Save interrupt state and disable interrupts
    sreg = SREG;
    __asm__ __volatile__ ("cli");
    
    // Read stack pointer (SPL must be read before SPH on AVR)
    sp = SPL;
    sp |= ((uint16_t)SPH << 8);
    
    // Restore interrupt state
    SREG = sreg;
    
    return sp;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void mem_monitor_init(void) {
    // Clear allocation tracking table
    memset(s_alloc_table, 0, sizeof(s_alloc_table));
    
    // Reset memory state
    memset(&s_mem_state, 0, sizeof(s_mem_state));
    
    // Record initial stack pointer (baseline for measurements)
    s_mem_state.init_stack_pointer = mem_monitor_get_stack_pointer();
    
    // Fill stack region with sentinel pattern
    // This allows us to detect maximum stack penetration
    // 
    // Fill from heap_start (or bss_end) up to current SP
    uint8_t* heap_end = __brkval ? __brkval : &__heap_start;
    uint8_t* stack_ptr = (uint8_t*)mem_monitor_get_stack_pointer();
    
    // Fill the gap between heap and current stack with sentinel
    for (uint8_t* ptr = heap_end; ptr < stack_ptr; ptr++) {
        *ptr = STACK_SENTINEL;
    }
}

// ============================================================================
// HEAP TRACKING
// ============================================================================

/**
 * @brief Track a new heap allocation
 * @param ptr Pointer returned by malloc
 * @param size Size of allocation in bytes
 */
void mem_monitor_track_alloc(void* ptr, uint16_t size) {
    if (ptr == NULL) {
        return; // Failed allocation, nothing to track
    }
    
    // Find free slot in allocation table
    for (uint8_t i = 0; i < MAX_HEAP_ALLOCATIONS; i++) {
        if (s_alloc_table[i].active == 0) {
            s_alloc_table[i].ptr = ptr;
            s_alloc_table[i].size = size;
            s_alloc_table[i].active = 1;
            
            // Update statistics
            s_mem_state.heap_used += size;
            s_mem_state.heap_total_allocated += size;
            s_mem_state.alloc_count++;
            return;
        }
    }
    
    // Table full - this is a critical error in production
    // In a real system, you might trigger a fault handler here
}

/**
 * @brief Track a heap deallocation
 * @param ptr Pointer passed to free
 */
void mem_monitor_track_free(void* ptr) {
    if (ptr == NULL) {
        return; // free(NULL) is valid, nothing to track
    }
    
    // Find allocation in table
    for (uint8_t i = 0; i < MAX_HEAP_ALLOCATIONS; i++) {
        if (s_alloc_table[i].active && s_alloc_table[i].ptr == ptr) {
            // Update statistics
            s_mem_state.heap_used -= s_alloc_table[i].size;
            s_mem_state.heap_total_freed += s_alloc_table[i].size;
            s_mem_state.free_count++;
            
            // Mark slot as free
            s_alloc_table[i].active = 0;
            return;
        }
    }
    
    // Freeing untracked pointer - possible double-free or corruption
}

// ============================================================================
// STACK MONITORING
// ============================================================================

/**
 * @brief Scan stack sentinel pattern to find maximum penetration
 * @return Maximum stack usage in bytes
 * 
 * Scans from heap end upward until non-sentinel byte found.
 * This indicates the deepest stack growth since initialization.
 */
static uint16_t scan_stack_usage(void) {
    uint8_t* heap_end = __brkval ? __brkval : &__heap_start;
    uint8_t* scan_ptr = heap_end;
    
    // Scan upward until we find a non-sentinel byte
    // This marks the deepest point the stack has reached
    while (*scan_ptr == STACK_SENTINEL && scan_ptr < (uint8_t*)RAMEND) {
        scan_ptr++;
    }
    
    // Stack usage is distance from RAMEND to first non-sentinel
    uint16_t max_usage = RAMEND - (uint16_t)scan_ptr;
    
    return max_usage;
}

uint16_t mem_monitor_get_current_stack_usage(void) {
    uint16_t current_sp = mem_monitor_get_stack_pointer();
    return RAMEND - current_sp;
}

uint16_t mem_monitor_get_max_stack_usage(void) {
    return s_mem_state.max_stack_usage;
}

uint16_t mem_monitor_get_free_stack_space(void) {
    uint16_t current_sp = mem_monitor_get_stack_pointer();
    uint8_t* heap_end = __brkval ? __brkval : &__heap_start;
    
    // Free space is gap between heap end and current SP
    if (current_sp > (uint16_t)heap_end) {
        return current_sp - (uint16_t)heap_end;
    }
    return 0; // Already collided!
}

// ============================================================================
// HEAP ANALYSIS
// ============================================================================

uint16_t mem_monitor_get_heap_used(void) {
    return s_mem_state.heap_used;
}

/**
 * @brief Calculate heap fragmentation ratio
 * @return Fragmentation ratio (0.0 = perfect, 1.0 = fully fragmented)
 * 
 * Algorithm:
 * 1. Determine total free heap
 * 2. Find largest contiguous free block
 * 3. Fragmentation = 1 - (largest_block / total_free)
 * 
 * This is a simplified fragmentation metric. Real malloc implementations
 * maintain free lists, but we approximate by analyzing allocation table.
 */
float mem_monitor_get_fragmentation_ratio(void) {
    // Get heap bounds
    uint8_t* heap_start = &__heap_start;
    uint8_t* heap_end = __brkval ? __brkval : heap_start;
    
    uint16_t total_heap = (uint16_t)heap_end - (uint16_t)heap_start;
    uint16_t total_free = total_heap - s_mem_state.heap_used;
    
    if (total_free == 0 || s_mem_state.alloc_count == 0) {
        return 0.0f; // No fragmentation if heap empty or fully allocated
    }
    
    // Simple heuristic: if we have many small allocations, fragmentation increases
    // More sophisticated: scan actual free blocks in heap (requires malloc internals)
    
    // For this implementation, estimate based on allocation patterns
    if (s_mem_state.alloc_count > s_mem_state.free_count + 5) {
        // Many active allocations suggest fragmentation
        float frag = (float)(s_mem_state.alloc_count - s_mem_state.free_count) / 
                     (float)MAX_HEAP_ALLOCATIONS;
        return (frag > 1.0f) ? 1.0f : frag;
    }
    
    return 0.0f;
}

// ============================================================================
// COLLISION DETECTION
// ============================================================================

uint8_t mem_monitor_check_collision(void) {
    uint16_t current_sp = mem_monitor_get_stack_pointer();
    uint8_t* heap_end = __brkval ? __brkval : &__heap_start;
    
    // Check if gap is below safety threshold
    uint16_t gap = current_sp - (uint16_t)heap_end;
    
    if (gap < COLLISION_SAFETY_MARGIN) {
        s_mem_state.collision_warning = 1;
        return 1;
    }
    
    s_mem_state.collision_warning = 0;
    return 0;
}

// ============================================================================
// UPDATE & STATISTICS
// ============================================================================

void mem_monitor_update(void) {
    // Scan stack for maximum penetration
    uint16_t max_stack = scan_stack_usage();
    if (max_stack > s_mem_state.max_stack_usage) {
        s_mem_state.max_stack_usage = max_stack;
    }
    
    // Check for collision
    mem_monitor_check_collision();
}

void mem_monitor_get_stats(MemoryStats* stats) {
    if (stats == NULL) return;
    
    // Calculate memory regions
    uint16_t data_size = (uint16_t)&__data_end - (uint16_t)&__data_start;
    uint16_t bss_size = (uint16_t)&__bss_end - (uint16_t)&__bss_start;
    
    stats->total_sram = RAMEND - 0x0100 + 1; // ATmega328P SRAM range
    stats->static_data = data_size;
    stats->static_bss = bss_size;
    stats->heap_used = s_mem_state.heap_used;
    stats->heap_total_allocated = s_mem_state.heap_total_allocated;
    stats->heap_total_freed = s_mem_state.heap_total_freed;
    stats->alloc_count = s_mem_state.alloc_count;
    stats->free_count = s_mem_state.free_count;
    stats->current_stack_usage = mem_monitor_get_current_stack_usage();
    stats->max_stack_usage = s_mem_state.max_stack_usage;
    stats->free_ram = mem_monitor_get_free_stack_space();
    stats->fragmentation_ratio = mem_monitor_get_fragmentation_ratio();
    stats->collision_warning = s_mem_state.collision_warning;
}

// ============================================================================
// DIAGNOSTIC OUTPUT
// ============================================================================

void mem_monitor_print_diagnostics(void) {
    MemoryStats stats;
    mem_monitor_get_stats(&stats);
    
    // Print formatted diagnostics
    uart_puts_P(PSTR("\r\n[MEM DIAGNOSTICS]\r\n"));
    uart_puts_P(PSTR("SRAM Total:    "));
    uart_print_u16(stats.total_sram);
    uart_puts_P(PSTR(" bytes\r\n"));
    
    uart_puts_P(PSTR("Static (.data): "));
    uart_print_u16(stats.static_data);
    uart_puts_P(PSTR(" bytes\r\n"));
    
    uart_puts_P(PSTR("Static (.bss):  "));
    uart_print_u16(stats.static_bss);
    uart_puts_P(PSTR(" bytes\r\n"));
    
    uart_puts_P(PSTR("Heap Used:     "));
    uart_print_u16(stats.heap_used);
    uart_puts_P(PSTR(" bytes ("));
    uart_print_u16(stats.alloc_count);
    uart_puts_P(PSTR(" allocs, "));
    uart_print_u16(stats.free_count);
    uart_puts_P(PSTR(" frees)\r\n"));
    
    uart_puts_P(PSTR("Stack Current: "));
    uart_print_u16(stats.current_stack_usage);
    uart_puts_P(PSTR(" bytes\r\n"));
    
    uart_puts_P(PSTR("Stack Peak:    "));
    uart_print_u16(stats.max_stack_usage);
    uart_puts_P(PSTR(" bytes\r\n"));
    
    uart_puts_P(PSTR("Free RAM:      "));
    uart_print_u16(stats.free_ram);
    uart_puts_P(PSTR(" bytes\r\n"));
    
    uart_puts_P(PSTR("Fragmentation: "));
    uart_print_float(stats.fragmentation_ratio * 100.0f);
    uart_puts_P(PSTR("%\r\n"));
    
    uart_puts_P(PSTR("Collision:     "));
    if (stats.collision_warning) {
        uart_puts_P(PSTR("*** WARNING ***\r\n"));
    } else {
        uart_puts_P(PSTR("OK\r\n"));
    }
    
    uart_puts_P(PSTR("\r\n"));
}

// ============================================================================
// MALLOC/FREE WRAPPERS (override default allocator)
// ============================================================================

/**
 * @brief Custom malloc wrapper with tracking
 * 
 * This uses the weak symbol mechanism or direct wrapping.
 * For avr-libc, we provide our own malloc/free that call the real
 * implementations and add tracking.
 */

// Store original malloc/free pointers (avr-libc approach)
extern "C" {
    // We'll use --wrap linker flag approach for cleaner interception
    // Declare the real malloc/free as __real_malloc/__real_free
    extern void* __real_malloc(size_t size);
    extern void __real_free(void* ptr);
    
    /**
     * @brief Wrapped malloc with tracking
     */
    void* __wrap_malloc(size_t size) {
        void* ptr = __real_malloc(size);
        mem_monitor_track_alloc(ptr, (uint16_t)size);
        return ptr;
    }
    
    /**
     * @brief Wrapped free with tracking
     */
    void __wrap_free(void* ptr) {
        mem_monitor_track_free(ptr);
        __real_free(ptr);
    }
}
