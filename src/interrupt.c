#include "interrupt.h"
#include "colors.h"
#include "cpu.h"
#include "input.h"
#include "keyboard_uart.h"
#include "macros.h"
#include "mem_tools.h"
#include "ordonnancer.h"
#include "platform.h"
#include "process.h"
#include "screen.h"
#include "stdint.h"
#include "stdio.h"
#include "time.h"

static uint64_t last_timer_update = 0;

/*
 * Function to treat interrupts at the cpu level.
 * The function definition is in treat.S
 */
extern void treater();

/*
 * Function to initialize the interrupt handler.
 * Write the address of the treat function in the mtvec register.
 */
extern void init_traitant(void (*treat)(void)) {
    __asm__("csrw mtvec, %0" ::"r"(treat));
}

/* Function to enable timer interrupts. */
extern void enable_timer() {
    uint64_t current_tick = read_mem64(CLINT_TIMER, 0);
    timer_set(TIMER_RATIO, current_tick);

    // test if timer is already enabled
    uint64_t mie;
    __asm__("csrr %0, mie" : "=r"(mie));

    if ((mie & 0x80) == 0x80) {
        return; // timer already enabled
    }

    // display initial time
    write_string_topright("00:00:00", 8, TEXT_COLOR, BACKGROUND_COLOR);

    // launch timer with period and start value

    /* unmask timer interrupt by setting the compare value
       - 7th bit of mie register (MTIE) = 1
     */
    mie |= 0x80;
    __asm__("csrw mie, %0" ::"r"(mie));
}

/* Function to disable timer interrupts. */
extern void disable_timer() {
    // mask timer interrupt by setting a very high compare value
    uint64_t mie;
    __asm__("csrr %0, mie" : "=r"(mie));
    mie &= ~0x80;
    __asm__("csrw mie, %0" ::"r"(mie));
}

/* Function to enable UART interrupts. */
extern void enable_external_it() {
    // test if externe interrupts are enabled
    uint64_t mie;
    __asm__("csrr %0, mie" : "=r"(mie));

    /* unmask External interrupts
     * - 11th bit of mie register (MEIE) = 1
     */
    mie |= 0x800;
    __asm__("csrw mie, %0" ::"r"(mie));
}

/* Function to disable UART interrupts. */
extern void disable_external_it() {
    // test if externe interrupts are enabled
    uint64_t mie;
    __asm__("csrr %0, mie" : "=r"(mie));

    /* mask External interrupts
     * - 11th bit of mie register (MEIE) = 0
     */
    mie &= ~0x800;
    __asm__("csrw mie, %0" ::"r"(mie));
}

/* Function to handle traps. */
extern void trap_handler(uint64_t mcause) {
    // ignore the 63th bit
    uint64_t mcause_real = mcause & 0x7FFFFFFFFFFFFFFF;

    /* timer interruption, mcause = 7 */
    if (mcause_real == 7) {
        // reset the timer for the next interrupt
        update_timer_set(TIMER_RATIO);

        // generate text output every second
        if (nb_ticks() - last_timer_update > TIMER_FREQ) {
            uint64_t seconds = nb_seconds();
            uint64_t minutes = nb_minutes();
            uint64_t hours = nb_hours();

            // generate string output HH:MM:SS with sprintf
            char time_str[9];
            sprintf(time_str, "%02lu:%02lu:%02lu", hours % 24, minutes % 60,
                    seconds % 60);

            // print the time at the top right of the screen
            write_string_topright(time_str, 8, TEXT_COLOR, BACKGROUND_COLOR);

            last_timer_update = nb_ticks();
        }

        // call the ordonnance function to switch to the next process
        ordonnance();
    }

    /* Machine external interrupt, mcause = 11 */
    if (mcause_real == 11) {
        // Claim the interrupt
        uint32_t irq = read_mem32(PLIC_IRQ_CLAIM, 0);

        // Optional detailed trace of claim/pending/enable registers.
        if (EXTERNAL_IRQ_DEBUG_VERBOSE) {
            uint32_t pending0 = read_mem32(PLIC_PENDING, 0);
            uint32_t pending1 = read_mem32(PLIC_PENDING, 4);
            uint32_t enable0 = read_mem32(PLIC_ENABLE, 0);
            uint32_t enable1 = read_mem32(PLIC_ENABLE, 4);
            EXT_IRQ_DBG(
                "[PLIC] claim=%u pending=0x%08x%08x enable=0x%08x%08x\n", irq,
                pending1, pending0, enable1, enable0);
        }

        if (irq == 0) {
            printf("[PLIC] claim returned 0 (no IRQ)\n");
        } else if (irq == UART_KEYBOARD_IRQ) { // UART interrupt
            get_char_uart(); // IMPORTANT flush the UART in get_char
        } else if (irq == virtio_input_irqs[0]) {
            virtio_input_handle_interrupt(0); // Index 0 pour le premier device
        } else if (irq == virtio_input_irqs[1]) {
            virtio_input_handle_interrupt(1); // Index 1 pour le second
        } else {
            printf("[PLIC] Unhandled IRQ %u\n", irq);
        }

        // Complete the interrupt (write back the claimed irq)
        write_mem32(PLIC_IRQ_CLAIM, 0, irq);
    }
}
