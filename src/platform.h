#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#if __ASSEMBLER__ == 0
#include "stdint.h"
#endif

// Bad Apple memory acces
#define BAD_APPLE_DATA 0x81000000

// Inputs memory base addresses
#define VIRTIO_INPUT1_BASE_ADDRESS 0x60000000
#define VIRTIO_INPUT1_CONFIG_BASE_ADDRESS 0x61000000
#define VIRTIO_INPUT2_BASE_ADDRESS 0x62000000
#define VIRTIO_INPUT2_CONFIG_BASE_ADDRESS 0x63000000

// Audio memory base addresses
#define VIRTIO_SND_BASE_ADDRESS 0x64000000
#define VIRTIO_SND_CONFIG_BASE_ADDRESS 0x65000000

// Video memory base address
#define BOCHS_DISPLAY_BASE_ADDRESS 0x50000000
#define BOCHS_CONFIG_BASE_ADDRESS 0x40000000

/* Adresse des différents registres de config */
#define VBE_DISPI_INDEX_ID 0x00
#define VBE_DISPI_INDEX_XRES 0x01
#define VBE_DISPI_INDEX_YRES 0x02
#define VBE_DISPI_INDEX_BPP 0x03
#define VBE_DISPI_INDEX_ENABLE 0x04
#define VBE_DISPI_INDEX_BANK 0x05
#define VBE_DISPI_INDEX_VIRT_WIDTH 0x06
#define VBE_DISPI_INDEX_VIRT_HEIGHT 0x07
#define VBE_DISPI_INDEX_X_OFFSET 0x08
#define VBE_DISPI_INDEX_Y_OFFSET 0x09
#define VBE_DISPI_INDEX_VIDEO_MEMORY_64K 0x0a
#define VBE_DISPI_INDEX_ENDIAN 0x82

#define VBE_DISPI_ID0 0xb0c0
#define VBE_DISPI_ID1 0xb0c1
#define VBE_DISPI_ID2 0xb0c2
#define VBE_DISPI_ID3 0xb0c3
#define VBE_DISPI_ID4 0xb0c4
#define VBE_DISPI_ID5 0xb0c5

#define VBE_DISPI_DISABLED 0x00
#define VBE_DISPI_ENABLED 0x01
#define VBE_DISPI_GETCAPS 0x02
#define VBE_DISPI_8BIT_DAC 0x20
#define VBE_DISPI_LFB_ENABLED 0x40
#define VBE_DISPI_NOCLEARMEM 0x80

#define VBE_MAX_WIDTH 1024
#define VBE_MAX_HEIGHT 768

#define DISPLAY_ENDIAN 0x1e1e1e1e
#define DISPLAY_WIDTH 1024
#define DISPLAY_HEIGHT 768
#define DISPLAY_BPP 32
#define DISPLAY_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT)

#define DISPLAY_CHAR_WIDTH DISPLAY_WIDTH / 8
#define DISPLAY_CHAR_HEIGHT DISPLAY_HEIGHT / 8

// Info bus PCI
#define PCI_ECAM_BASE_ADDRESS 0x30000000
#define DISPLAY_PCI_ID 0x11111234

// PLIC registers addresses
#define PLIC_SOURCE 0x0c000000
#define PLIC_PENDING 0x0c001000
#define PLIC_ENABLE 0x0c002000
#define PLIC_TARGET 0x0c200000
#define PLIC_IRQ_CLAIM 0x0c200004

// PLIC pushbutton irq
#define PLIC_IRQ_2 0x2

// CLINT registers addresses
#define CLINT_MSIP 0x02000000
#define CLINT_TIMER_CMP 0x02004000
#define CLINT_TIMER_CMP_LO 0x02004000
#define CLINT_TIMER_CMP_HI 0x02004004
#define CLINT_TIMER 0x0200bff8
#define CLINT_TIMER_LOW 0x0200bff8
#define CLINT_TIMER_HI 0x0200bffc

// Timer options
#define TIMER_FREQ 10000000 // 10MHz
// TIMER_IT_FREQ must be a divisor of TIMER_FREQ
#define TIMER_IT_FREQ 200                        // 1000 interrupts per second
#define TIMER_RATIO (TIMER_FREQ / TIMER_IT_FREQ) // time between interruption

// Bit in mstatus
#define MSTATUS_MIE 0x00000008
// Bit in mie/mip
#define IRQ_M_TMR 7
#define IRQ_M_EXT 11

// UART
#define UART_BASE 0x10000000
#define UART_CLOCK_FREQ 1843200
#define UART_BAUD_RATE 115200

#define UART_IER_MSI 0x08  /* Enable Modem status interrupt */
#define UART_IER_RLSI 0x04 /* Enable receiver line status interrupt */
#define UART_IER_THRI 0x02 /* Enable Transmitter holding register int. */
#define UART_IER_RDI 0x01  /* Enable receiver data interrupt */

#define UART_IIR_NO_INT 0x01 /* No interrupts pending */
#define UART_IIR_ID 0x06     /* Mask for the interrupt ID */

enum {
    UART_RBR = 0x00, /* Receive Buffer Register */
    UART_THR = 0x00, /* Transmit Hold Register */
    UART_IER = 0x01, /* Interrupt Enable Register */
    UART_DLL = 0x00, /* Divisor LSB (LCR_DLAB) */
    UART_DLH = 0x01, /* Divisor MSB (LCR_DLAB) */
    UART_FCR = 0x02, /* FIFO Control Register */
    UART_LCR = 0x03, /* Line Control Register */
    UART_MCR = 0x04, /* Modem Control Register */
    UART_LSR = 0x05, /* Line Status Register */
    UART_MSR = 0x06, /* Modem Status Register */
    UART_SCR = 0x07, /* Scratch Register */

    UART_LCR_DLAB = 0x80, /* Divisor Latch Bit */
    UART_LCR_8BIT = 0x03, /* 8-bit */
    UART_LCR_PODD = 0x08, /* Parity Odd */

    UART_LSR_DA = 0x01, /* Data Available */
    UART_LSR_OE = 0x02, /* Overrun Error */
    UART_LSR_PE = 0x04, /* Parity Error */
    UART_LSR_FE = 0x08, /* Framing Error */
    UART_LSR_BI = 0x10, /* Break indicator */
    UART_LSR_RE = 0x20, /* THR is empty */
    UART_LSR_RI = 0x40, /* THR is empty and line is idle */
    UART_LSR_EF = 0x80, /* Erroneous data in FIFO */
};

enum {
    /* UART Registers */
    UART_REG_TXFIFO = 0,
    UART_REG_RXFIFO = 1,
    UART_REG_TXCTRL = 2,
    UART_REG_RXCTRL = 3,
    UART_REG_IE = 4,
    UART_REG_IP = 5,
    UART_REG_DIV = 6,

    /* TXCTRL register */
    UART_TXEN = 1,
    UART_TXSTOP = 2,

    /* RXCTRL register */
    UART_RXEN = 1,

    /* IP register */
    UART_IP_TXWM = 1,
    UART_IP_RXWM = 2,

    /* INTERRUPT ENABLE */
    UART_RX_IT_EN = 2,
    UART_TX_IT_EN = 1
};

// UART Keyboard IRQ
#define UART_KEYBOARD_IRQ 10

#if __ASSEMBLER__ == 0
extern void timer_set(uint32_t period, uint32_t start_value);
extern void timer_wait();
extern void timer_set_and_wait(uint32_t period, uint32_t time);
extern void led_set(uint32_t value);
extern uint32_t push_button_get();
#endif

#endif // __PLATFORM_H__
