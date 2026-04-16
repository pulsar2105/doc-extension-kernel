#include "pci.h"
#include "platform.h"
#include "stdint.h"

/* List of all PCIe devices found in the system that respect vendor_id and
 * device_id return the address of the device in memory, or 0 if not found. */
volatile uint64_t pci_devices_found[32] = {0};

/* Function to search screen among all PCIe devices. */
extern uint64_t pci_search(uint64_t base_addr, uint64_t vendor_id,
                           uint64_t device_id) {
    // clear the list of found devices
    for (int i = 0; i < 32; i++) {
        pci_devices_found[i] = 0;
    }

    uint16_t pci_index = 0;

    for (uint8_t device = 0; device < 32; device++) {
        uint64_t offset = device << 15;

        // read all data with the struct
        volatile pci_header_t0x0 *header =
            (pci_header_t0x0 *)(base_addr + offset);

        uint16_t current_vendor_id = header->vendor_id;
        uint16_t current_device_id = header->device_id;

        if (current_vendor_id == vendor_id && current_device_id == device_id) {
            pci_devices_found[pci_index] = base_addr + offset;
            pci_index++;
        }
    }

    if (pci_index > 0) {
        return pci_devices_found[0]; // return the first found device
    }

    return 0; // no device found
}

/* Function to compute the intx irq for each device. */
uint8_t compute_pci_intx_irq(uint64_t pci_header_addr, uint8_t interrupt_pin) {
    uint8_t device =
        (uint8_t)((pci_header_addr - PCI_ECAM_BASE_ADDRESS) >> 15) & 0x1F;
    uint8_t pin = interrupt_pin;

    if (pin == 0 || pin > 4) {
        pin = 1; // default to INTA# for malformed/unknown pin value
    }
    return (uint8_t)(32 + ((device + pin - 1) % 4));
}
