/**
 * @file memory_monitor.h
 * @brief Production-quality memory monitoring framework for ATmega328P
 * 
 * MEMORY ARCHITECTURE (ATmega328P):
 * 
 * Flash (Program Memory): 32 KB
 * SRAM: 2048 bytes (0x0100 - 0x08FF)
 * 
 * SRAM LAYOUT (grows from low to high addresses):
 * 
 *  Low Address (0x0100)
 *  +------------------+
 *  | .data (init vars)|  <- Initialized global/static variables
 *  +------------------+
 *  | .bss (uninit)    |  <- Zero-initialized global/static variables
 *  +------------------+
 *  | Heap (grows up)  |  <- malloc/free allocations (grows toward high mem)
 *  |        ↓         |
 *  |                  |
 *  | Free RAM         |  <- Critical gap - must not reach zero!
 *  |                  |
 *  |        ↑         |
 *  | Stack (grows dn) |  <- Local variables, return addresses (grows toward low mem)
 *  +------------------+
 *  High Address (RAMEND = 0x08FF)
 * 
 * COLLISION RISK:
 * Stack and heap grow toward each other. If they collide:
 * - Stack overwrites heap data = corruption
 * - Heap overwrites stack = crashes, undefined behavior
 * 
 * This framework monitors the gap and detects dangerous conditions.
 */

#ifndef MEMORY_MONITOR_H
#define MEMORY_MONITOR_H

#include <stdint.h>

// Maximum number of heap allocations to track simultaneously
#define MAX_HEAP_ALLOCATIONS 32

// Stack sentinel fill pattern (used to detect stack high-water mark)
#define STACK_SENTINEL 0xAA

// Safety margin between heap and stack (bytes)
#define COLLISION_SAFETY_MARGIN 128

/**
 * @brief Heap allocation tracking entry
 */
struct AllocationEntry {
    void* ptr;          // Pointer to allocated block
    uint16_t size;      // Size of allocation
    uint8_t active;     // 1 if allocated, 0 if freed
};

/**
 * @brief Memory statistics structure
 */
struct MemoryStats {
    uint16_t total_sram;           // Total SRAM available
    uint16_t static_data;          // .data segment size
    uint16_t static_bss;           // .bss segment size
    uint16_t heap_used;            // Current heap usage
    uint16_t heap_total_allocated; // Total ever allocated
    uint16_t heap_total_freed;     // Total ever freed
    uint16_t alloc_count;          // Number of malloc calls
    uint16_t free_count;           // Number of free calls
    uint16_t current_stack_usage;  // Current stack depth
    uint16_t max_stack_usage;      // Peak stack usage
    uint16_t free_ram;             // Free RAM between heap and stack
    float fragmentation_ratio;     // Heap fragmentation (0.0 - 1.0)
    uint8_t collision_warning;     // 1 if heap/stack collision risk detected
};

/**
 * @brief Initialize memory monitoring framework
 * 
 * MUST be called early in main() before any malloc/free operations.
 * 
 * Actions:
 * - Initializes linker symbol pointers
 * - Fills unused stack region with sentinel pattern (0xAA)
 * - Resets heap tracking structures
 * - Establishes baseline stack pointer
 */
void mem_monitor_init(void);

/**
 * @brief Update memory statistics
 * 
 * Call periodically (e.g., every loop iteration or timer tick).
 * 
 * Actions:
 * - Reads current stack pointer
 * - Scans stack sentinel pattern
 * - Calculates free RAM
 * - Updates fragmentation metrics
 * - Checks for collision conditions
 */
void mem_monitor_update(void);

/**
 * @brief Get current memory statistics
 * @param stats Pointer to MemoryStats structure to fill
 */
void mem_monitor_get_stats(MemoryStats* stats);

/**
 * @brief Print formatted memory diagnostics via UART
 * 
 * Output format:
 * [MEM]
 * SRAM Total: XXXX
 * Static: XXXX (.data + .bss)
 * Heap Used: XXXX
 * Stack Used: XXXX
 * Max Stack: XXXX
 * Free RAM: XXXX
 * Fragmentation: XX.X%
 * Collision Warning: YES/NO
 */
void mem_monitor_print_diagnostics(void);

/**
 * @brief Get current stack pointer value
 * @return Current SP register value
 * 
 * Uses inline assembly to read AVR stack pointer (SPH:SPL).
 * Stack grows downward on AVR architecture.
 */
uint16_t mem_monitor_get_stack_pointer(void);

/**
 * @brief Get current stack usage in bytes
 * @return Bytes of stack currently in use
 */
uint16_t mem_monitor_get_current_stack_usage(void);

/**
 * @brief Get maximum stack usage since initialization
 * @return Peak stack usage in bytes
 */
uint16_t mem_monitor_get_max_stack_usage(void);

/**
 * @brief Get free stack space remaining
 * @return Bytes of stack space still available
 */
uint16_t mem_monitor_get_free_stack_space(void);

/**
 * @brief Get current heap usage
 * @return Bytes of heap currently allocated
 */
uint16_t mem_monitor_get_heap_used(void);

/**
 * @brief Get heap fragmentation ratio
 * @return Fragmentation ratio (0.0 = no fragmentation, 1.0 = fully fragmented)
 * 
 * Formula: 1 - (largest_free_block / total_free_heap)
 */
float mem_monitor_get_fragmentation_ratio(void);

/**
 * @brief Check if heap/stack collision risk exists
 * @return 1 if collision warning active, 0 otherwise
 */
uint8_t mem_monitor_check_collision(void);

// Heap tracking functions (called by malloc/free wrappers)
void mem_monitor_track_alloc(void* ptr, uint16_t size);
void mem_monitor_track_free(void* ptr);

#endif // MEMORY_MONITOR_H
