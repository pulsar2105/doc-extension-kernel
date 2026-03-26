#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__

#include "platform.h"
#include "stdint.h"

/*
 * Function to treat interrupts at the cpu level.
 * The function definition is in treat.S
 */
extern void treater(void);

/* Function to initialize the interrupt handler. */
extern void init_traitant(void (*treater)(void));

/* Function to enable timer interrupts. */
extern void enable_timer();

/* Function to disabel timer interrupts. */
extern void disable_timer();

/* Function to enable UART interrupts. */
extern void enable_external_it();

/* Function to disable UART interrupts. */
extern void disable_external_it();

/* Function to handle traps. */
extern void trap_handler(uint64_t mcause);

#endif
