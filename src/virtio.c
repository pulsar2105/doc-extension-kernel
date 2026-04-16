#include "virtio.h"
#include "macros.h"
#include "pci.h"
#include "stdint.h"
#include "stdio.h"

/*
 * Parcourt la liste chaînée des capabilities PCI pour trouver celle
 * dont le cfg_type correspond à ce qu'on cherche.
 *
 * Retourne l'adresse ABSOLUE en mémoire de la structure visée dans le BAR,
 * ou 0 si non trouvée.
 *
 * Paramètres :
 *   pci_header  : adresse de base de la config PCI du device
 *   target_type  : type de cap virtio cherché (ex: VIRTIO_PCI_CAP_COMMON_CFG)
 */
uint64_t find_virtio_cap(volatile pci_header_t0x0 *pci_header,
                         volatile cfg_virtio_device *cfg_types_found) {
    /* test du bit 4 du registre  Status avant d'utiliser le registre
     * Capabilities Pointer.*/

    VCAP_DBG("[virtio] PCI status: 0x%x\n", pci_header->status);

    if ((pci_header->status & 0x10) != 0x10) {
        VCAP_DBG(
            "[virtio] The PCI device did not support capabilities list.\n");
        return 1;
    }

    /*
     * Offset vers la première capability dans l'espace de config PCI.
     * On masque avec 0xFC pour aligner sur 4 octets (bits 1:0 réservés).
     */
    uint8_t cap_offset = pci_header->capabilities_ptr & 0xFC;

    /* Parcours de la liste chaînée des capabilities */
    while (cap_offset != 0) {
        /* Raw capability trace (disabled by default). */
        uint8_t raw_cap_id =
            *((volatile uint8_t *)((uint64_t)pci_header + cap_offset));
        VCAP_DBG("[virtio] PCI cap at 0x%02x id=0x%02x\n", cap_offset,
                 raw_cap_id);

        /* Lecture capability PCI générique avec la struct pci_header */
        virtio_pci_cap *cap =
            (virtio_pci_cap *)((uint64_t)pci_header + cap_offset);

        /* On ne traite que les capabilities "vendor specific" (0x09) qui
         * correspondent aux caps virtio.
         */
        if (cap->cap_vndr == PCI_CAP_ID_VNDR) {
            if (cap->bar < 6) {
                /*
                 * Adresse finale = BAR (masqué) + offset.
                 * On masque le premier octect du BAR (qui indique I/O vs
                 * Memory) space).
                 * https://wiki.osdev.org/PCI#Base_Address_Registers
                 */
                uint64_t addr_bar_offset =
                    (pci_header->bar[cap->bar] & 0xFFFFFFF0) + cap->offset;

                switch (cap->cfg_type) {
                case VIRTIO_PCI_CAP_COMMON_CFG:
                    cfg_types_found->common_cfg = addr_bar_offset;
                    break;
                case VIRTIO_PCI_CAP_NOTIFY_CFG:
                    cfg_types_found->notify_cfg = addr_bar_offset;
                    /* read notify_off_multiplier from capability */
                    if (cap->cap_len >= sizeof(virtio_pci_notify_cap)) {
                        virtio_pci_notify_cap *ncap =
                            (virtio_pci_notify_cap *)cap;
                        cfg_types_found->notify_off_multiplier =
                            ncap->notify_off_multiplier;
                    }
                    break;
                case VIRTIO_PCI_CAP_ISR_CFG:
                    cfg_types_found->isr_status = addr_bar_offset;
                    break;
                case VIRTIO_PCI_CAP_DEVICE_CFG:
                    cfg_types_found->device_specific_cfg = addr_bar_offset;
                    break;
                case VIRTIO_PCI_CAP_PCI_CFG:
                    cfg_types_found->pci_cfg_access = addr_bar_offset;
                    break;
                case VIRTIO_PCI_CAP_SHARED_MEMORY_CFG:
                    cfg_types_found->shared_mem_cfg = addr_bar_offset;
                    break;
                case VIRTIO_PCI_CAP_VENDOR_CFG:
                    cfg_types_found->vendor_cfg = addr_bar_offset;
                    break;
                }
            }
        } else {
            /* Non-vendor specific capabilities (e.g., MSI/MSI-X) */
            if (raw_cap_id == 0x11) {
                VCAP_DBG("[virtio] MSI-X capability detected at 0x%02x\n",
                         cap_offset);
            } else if (raw_cap_id == 0x05) {
                VCAP_DBG("[virtio] MSI capability detected at 0x%02x\n",
                         cap_offset);
            }
        }

        cap_offset = cap->cap_next;
    }

    VCAP_DBG("[virtio] Capabilities found:\n");
    VCAP_DBG("  COMMON_CFG: 0x%lx\n", cfg_types_found->common_cfg);
    VCAP_DBG("  NOTIFY_CFG: 0x%lx\n", cfg_types_found->notify_cfg);
    VCAP_DBG("  ISR_CFG: 0x%lx\n", cfg_types_found->isr_status);
    VCAP_DBG("  DEVICE_CFG: 0x%lx\n", cfg_types_found->device_specific_cfg);
    VCAP_DBG("  PCI_CFG_ACCESS: 0x%lx\n", cfg_types_found->pci_cfg_access);
    VCAP_DBG("  SHARED_MEM_CFG: 0x%lx\n", cfg_types_found->shared_mem_cfg);
    VCAP_DBG("  VENDOR_CFG: 0x%lx\n", cfg_types_found->vendor_cfg);

    return 0;
}
