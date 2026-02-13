/**
 * @file main.cpp
 * @brief Memory monitoring framework test harness
 * 
 * Demonstrates production-quality memory diagnostics on ATmega328P:
 * - Stack growth monitoring through recursive functions
 * - Heap allocation/deallocation tracking
 * - Fragmentation analysis
 * - Collision detection
 * 
 * Test scenarios:
 * 1. Baseline measurement
 * 2. Recursive stack stress test
 * 3. Heap fragmentation test (alternating alloc/free)
 * 4. Large buffer stress test
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include "uart_driver.h"
#include "memory_monitor.h"

// ============================================================================
// CONFIGURATION
// ============================================================================

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#define UART_BAUD 115200
#define DIAGNOSTIC_INTERVAL_MS 2000

// ============================================================================
// TEST FUNCTIONS
// ============================================================================

/**
 * @brief Recursive function to stress test stack
 * @param depth Current recursion depth
 * 
 * Each recursion level allocates:
 * - Return address (2 bytes)
 * - Frame pointer (2 bytes)
 * - Local variables (buffer + depth)
 * 
 * This demonstrates stack growth detection and max usage tracking.
 */
void recursive_stack_test(uint8_t depth) {
    // Local buffer to consume stack space
    volatile char buffer[32];
    
    // Initialize buffer to prevent optimization
    for (uint8_t i = 0; i < sizeof(buffer); i++) {
        buffer[i] = depth + i;
    }
    
    // Print current depth
    uart_puts_P(PSTR("  Recursion depth: "));
    uart_print_u16(depth);
    uart_puts_P(PSTR(", Stack usage: "));
    uart_print_u16(mem_monitor_get_current_stack_usage());
    uart_puts_P(PSTR(" bytes\r\n"));
    
    // Update memory monitor
    mem_monitor_update();
    
    // Recurse deeper
    if (depth < 10) {
        recursive_stack_test(depth + 1);
    }
    
    // Use buffer to prevent optimization
    volatile uint8_t dummy = buffer[0];
    (void)dummy;
}

/**
 * @brief Heap fragmentation stress test
 * 
 * Allocates multiple blocks, frees alternating blocks, then allocates again.
 * This creates fragmentation that the monitor should detect.
 */
void heap_fragmentation_test(void) {
    uart_puts_P(PSTR("\r\n=== Heap Fragmentation Test ===\r\n"));
    
    void* blocks[8];
    
    // Allocate 8 blocks of varying sizes
    uart_puts_P(PSTR("Allocating 8 blocks...\r\n"));
    blocks[0] = malloc(16);
    blocks[1] = malloc(32);
    blocks[2] = malloc(16);
    blocks[3] = malloc(64);
    blocks[4] = malloc(16);
    blocks[5] = malloc(32);
    blocks[6] = malloc(16);
    blocks[7] = malloc(48);
    
    mem_monitor_update();
    uart_puts_P(PSTR("  Heap used: "));
    uart_print_u16(mem_monitor_get_heap_used());
    uart_puts_P(PSTR(" bytes\r\n"));
    
    // Free alternating blocks to create fragmentation
    uart_puts_P(PSTR("Freeing alternating blocks (1, 3, 5, 7)...\r\n"));
    free(blocks[1]);
    free(blocks[3]);
    free(blocks[5]);
    free(blocks[7]);
    
    mem_monitor_update();
    uart_puts_P(PSTR("  Heap used: "));
    uart_print_u16(mem_monitor_get_heap_used());
    uart_puts_P(PSTR(" bytes\r\n"));
    uart_puts_P(PSTR("  Fragmentation: "));
    uart_print_float(mem_monitor_get_fragmentation_ratio() * 100.0f);
    uart_puts_P(PSTR("%\r\n"));
    
    // Allocate new blocks (may not fit in fragmented space)
    uart_puts_P(PSTR("Allocating new blocks...\r\n"));
    void* new1 = malloc(24);
    void* new2 = malloc(40);
    
    mem_monitor_update();
    uart_puts_P(PSTR("  Heap used: "));
    uart_print_u16(mem_monitor_get_heap_used());
    uart_puts_P(PSTR(" bytes\r\n"));
    
    // Cleanup
    uart_puts_P(PSTR("Cleaning up...\r\n"));
    free(blocks[0]);
    free(blocks[2]);
    free(blocks[4]);
    free(blocks[6]);
    free(new1);
    free(new2);
    
    mem_monitor_update();
    uart_puts_P(PSTR("  Final heap used: "));
    uart_print_u16(mem_monitor_get_heap_used());
    uart_puts_P(PSTR(" bytes\r\n\r\n"));
}

/**
 * @brief Large buffer allocation test
 * 
 * Allocates large buffer on stack to stress test stack/heap collision detection.
 */
void large_buffer_test(void) {
    uart_puts_P(PSTR("\r\n=== Large Stack Buffer Test ===\r\n"));
    
    // Allocate large buffer on stack
    volatile uint8_t large_buffer[256];
    
    // Initialize to prevent optimization
    for (uint16_t i = 0; i < sizeof(large_buffer); i++) {
        large_buffer[i] = i & 0xFF;
    }
    
    mem_monitor_update();
    
    uart_puts_P(PSTR("  Large buffer allocated: "));
    uart_print_u16(sizeof(large_buffer));
    uart_puts_P(PSTR(" bytes\r\n"));
    uart_puts_P(PSTR("  Current stack usage: "));
    uart_print_u16(mem_monitor_get_current_stack_usage());
    uart_puts_P(PSTR(" bytes\r\n"));
    uart_puts_P(PSTR("  Free RAM: "));
    uart_print_u16(mem_monitor_get_free_stack_space());
    uart_puts_P(PSTR(" bytes\r\n"));
    
    // Check collision warning
    if (mem_monitor_check_collision()) {
        uart_puts_P(PSTR("  *** COLLISION WARNING TRIGGERED ***\r\n"));
    }
    
    // Use buffer to prevent optimization
    volatile uint16_t checksum = 0;
    for (uint16_t i = 0; i < sizeof(large_buffer); i++) {
        checksum += large_buffer[i];
    }
    
    uart_puts_P(PSTR("  Buffer checksum: "));
    uart_print_u16(checksum);
    uart_newline();
    uart_newline();
}

/**
 * @brief Combined stress test
 * 
 * Allocates heap memory, then recurses to stress both heap and stack.
 */
void combined_stress_test(void) {
    uart_puts_P(PSTR("\r\n=== Combined Heap + Stack Stress ===\r\n"));
    
    // Allocate heap blocks
    void* block1 = malloc(128);
    void* block2 = malloc(96);
    void* block3 = malloc(64);
    
    uart_puts_P(PSTR("Allocated heap blocks: 128 + 96 + 64 = 288 bytes\r\n"));
    
    mem_monitor_update();
    uart_puts_P(PSTR("  Heap used: "));
    uart_print_u16(mem_monitor_get_heap_used());
    uart_puts_P(PSTR(" bytes\r\n"));
    uart_puts_P(PSTR("  Free RAM before recursion: "));
    uart_print_u16(mem_monitor_get_free_stack_space());
    uart_puts_P(PSTR(" bytes\r\n\r\n"));
    
    // Now recurse with heap allocated
    uart_puts_P(PSTR("Starting recursion with heap allocated...\r\n"));
    recursive_stack_test(1);
    
    mem_monitor_update();
    uart_puts_P(PSTR("\r\n  Free RAM after recursion: "));
    uart_print_u16(mem_monitor_get_free_stack_space());
    uart_puts_P(PSTR(" bytes\r\n"));
    
    // Cleanup
    free(block1);
    free(block2);
    free(block3);
    
    uart_puts_P(PSTR("Heap blocks freed\r\n\r\n"));
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    // Initialize UART for diagnostics
    uart_init(UART_BAUD, F_CPU);
    
    // Small delay for serial terminal to connect
    _delay_ms(100);
    
    // Print banner
    uart_newline();
    uart_puts_P(PSTR("================================================================================\r\n"));
    uart_puts_P(PSTR("  ATmega328P Memory Monitoring Framework\r\n"));
    uart_puts_P(PSTR("  Production-Quality Runtime Diagnostics\r\n"));
    uart_puts_P(PSTR("================================================================================\r\n"));
    uart_newline();
    
    // Initialize memory monitor (MUST be called before any malloc/free)
    mem_monitor_init();
    
    uart_puts_P(PSTR("Memory monitor initialized\r\n"));
    uart_puts_P(PSTR("Stack sentinel pattern filled\r\n"));
    uart_newline();
    
    // Print initial baseline
    uart_puts_P(PSTR("=== BASELINE MEASUREMENTS ===\r\n"));
    mem_monitor_update();
    mem_monitor_print_diagnostics();
    
    _delay_ms(1000);
    
    // ========================================================================
    // TEST SEQUENCE
    // ========================================================================
    
    // Test 1: Recursive stack test
    uart_puts_P(PSTR("=== Test 1: Recursive Stack Growth ===\r\n"));
    recursive_stack_test(1);
    uart_newline();
    mem_monitor_update();
    mem_monitor_print_diagnostics();
    _delay_ms(1000);
    
    // Test 2: Heap fragmentation
    heap_fragmentation_test();
    mem_monitor_print_diagnostics();
    _delay_ms(1000);
    
    // Test 3: Large buffer
    large_buffer_test();
    mem_monitor_print_diagnostics();
    _delay_ms(1000);
    
    // Test 4: Combined stress
    combined_stress_test();
    mem_monitor_print_diagnostics();
    _delay_ms(1000);
    
    // ========================================================================
    // CONTINUOUS MONITORING LOOP
    // ========================================================================
    
    uart_puts_P(PSTR("=== Entering Continuous Monitoring Mode ===\r\n"));
    uart_puts_P(PSTR("Diagnostics printed every 2 seconds\r\n"));
    uart_newline();
    
    uint32_t last_report_ms = 0;
    
    while (1) {
        // Update memory statistics
        mem_monitor_update();
        
        // Print diagnostics periodically
        // Note: using delay approximation since we don't have timer setup
        static uint16_t loop_count = 0;
        loop_count++;
        
        if (loop_count >= 100) {  // Roughly 2 seconds with delays
            loop_count = 0;
            
            uart_puts_P(PSTR("--- Periodic Status ---\r\n"));
            mem_monitor_print_diagnostics();
            
            // Demonstrate periodic allocation
            void* test_block = malloc(32);
            _delay_ms(10);
            free(test_block);
        }
        
        _delay_ms(20);
    }
    
    return 0;
}
