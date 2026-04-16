#ifndef __PCI_H__
#define __PCI_H__

#include "stdint.h"

// list of all PCIe devices found in the system that respect vendor_id and
// device_id return the address of the device in memory, or 0 if not found
extern volatile uint64_t pci_devices_found[32];

/* Offsets header PCI type 0x0.
 * https://wiki.osdev.org/PCI#Header_Type */

typedef struct {
    /* --- Common Header (offset 0x00 – 0x0F) --- */
    uint16_t vendor_id;      // 0x00 - 0xFFFF = invalid
    uint16_t device_id;      // 0x02
    uint16_t command;        // 0x04
    uint16_t status;         // 0x06
    uint8_t revision_id;     // 0x08
    uint8_t prog_if;         // 0x09 - Programming Interface
    uint8_t subclass;        // 0x0A
    uint8_t class_code;      // 0x0B
    uint8_t cache_line_size; // 0x0C - en unités de 32 bits
    uint8_t latency_timer;   // 0x0D
    uint8_t header_type;     // 0x0E - bit 7 = multi-function
    uint8_t bist;            // 0x0F - Built-In Self Test

    /* --- Type 0x0 Specific (offset 0x10 – 0x3F) --- */
    uint32_t bar[6]; // 0x10–0x27 - Base Address Registers

    uint32_t cardbus_cis_ptr;     // 0x28 - CardBus CIS Pointer
    uint16_t subsystem_vendor_id; // 0x2C
    uint16_t subsystem_id;        // 0x2E

    uint32_t expansion_rom_base; // 0x30

    uint8_t capabilities_ptr; // 0x34 - offset dans le config space
    uint8_t reserved0[3];     // 0x35–0x37
    uint32_t reserved1;       // 0x38

    uint8_t interrupt_line; // 0x3C - IRQ mappé par le BIOS/OS
    uint8_t interrupt_pin;  // 0x3D - 0x00 = pas d'IRQ, 0x01=INTA...
    uint8_t min_grant;      // 0x3E - burst period désiré (en 250ns)
    uint8_t max_latency;    // 0x3F - fréquence d'accès au bus voulue
} __attribute__((packed, aligned(4))) pci_header_t0x0;

/* Function to search screen among all PCIe devices. */
extern uint64_t pci_search(uint64_t base_addr, uint64_t vendor_id,
                           uint64_t device_id);

/* Function to compute the intx irq for each device. */
uint8_t compute_pci_intx_irq(uint64_t pci_header_addr, uint8_t interrupt_pin);

#endif
