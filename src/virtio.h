#ifndef __VIRTIO_H__
#define __VIRTIO_H__

#include "pci.h"
#include "stdint.h"

/* CONSTANTES VIRTIO (spec virtio 1.2, section 4.1) */

/* Types de capabilities PCI virtio (cfg_type dans virtio_pci_cap) */
#define VIRTIO_PCI_CAP_COMMON_CFG 1 // Common configuration
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2 // Notifications (queues)
#define VIRTIO_PCI_CAP_ISR_CFG 3    // ISR Status (interrupt registers)
#define VIRTIO_PCI_CAP_DEVICE_CFG 4 // Device specific configuration
#define VIRTIO_PCI_CAP_PCI_CFG 5    // PCI configuration access

#define VIRTIO_PCI_CAP_SHARED_MEMORY_CFG 8 // Shared memory region
#define VIRTIO_PCI_CAP_VENDOR_CFG 9        // Vendor specific data

/* ID de la capability "vendor specific" dans l'espace de config PCI standard */
#define PCI_CAP_ID_VNDR 0x09

/* Device Status Bits (spec virtio 1.2, section 2.1) */
#define VIRTIO_STATUS_ACKNOWLEDGE 1 // L'OS a trouvé le device
#define VIRTIO_STATUS_DRIVER 2      // L'OS sait comment driver ce device
#define VIRTIO_STATUS_DRIVER_OK 4   // Le driver est prêt
#define VIRTIO_STATUS_FEATURES_OK 8 // Le driver a accepté les features
#define VIRTIO_STATUS_FAILED 128    // Quelque chose s'est mal passé

/* STRUCTURES VIRTIO PCI (spec virtio 1.2, section 4.1.4) */

/*
 * En-tête de chaque capability PCI virtio.
 * Ces structures sont chaînées dans l'espace de configuration PCI du device
 * à partir du "Capabilities Pointer" (offset 0x34).
 * Chacune décrit où trouver une zone mémoire dans un BAR.
 */
typedef struct {
    uint8_t cap_vndr;   // Doit valoir PCI_CAP_ID_VNDR (0x09)
    uint8_t cap_next;   // Offset vers la prochaine capability (0 = fin)
    uint8_t cap_len;    // Taille de cette capability en octets
    uint8_t cfg_type;   // Type de structure virtio (COMMON, NOTIFY, etc.)
    uint8_t bar;        // Numéro de BAR (0-5) dans lequel se trouve la zone
    uint8_t id;         // ID pour distinguer plusieurs caps du même type
    uint8_t padding[2]; // Pad to full dword.
    uint32_t offset;    // Offset depuis le début du BAR
    uint32_t length;    // Taille de la structure dans le BAR
} __attribute__((packed, aligned(4))) virtio_pci_cap;

/* struct to store all type of config zone found */
typedef struct {
    /* memory map */
    uint64_t common_cfg;
    uint64_t notify_cfg;
    uint64_t isr_status;
    uint64_t device_specific_cfg;
    uint64_t pci_cfg_access;
    uint64_t shared_mem_cfg;
    uint64_t vendor_cfg;
    /* notify multiplier from VIRTIO_PCI_CAP_NOTIFY_CFG */
    uint32_t notify_off_multiplier;
} cfg_virtio_device;

/* Notify capability extends virtio_pci_cap with a multiplier */
typedef struct {
    virtio_pci_cap cap;
    uint32_t notify_off_multiplier;
} __attribute__((packed, aligned(4))) virtio_pci_notify_cap;

/*
 * Structure de configuration commune virtio.
 * Mappée en MMIO à l'adresse : BAR[cap.bar] + cap.offset
 * C'est ici qu'on contrôle le device : reset, features, queues, statut.
 */
typedef struct {
    /* --- Champs contrôlés par le DEVICE (lecture seule pour le driver) --- */
    uint32_t device_feature_select; // Sélectionne la tranche de features à lire
    uint32_t device_feature;        // Features supportées par le device
    /* --- Champs contrôlés par le DRIVER --- */
    uint32_t
        driver_feature_select; // Sélectionne la tranche de features à écrire
    uint32_t driver_feature;   // Features que le driver accepte
    /* --- Config MSI-X (ignoré sans MSI-X) --- */
    uint16_t config_msix_vector;
    /* --- Informations sur les queues --- */
    uint16_t num_queues; // Nombre de virtqueues disponibles (lecture)
    /* --- Statut et génération --- */
    uint8_t device_status;     // Registre de statut du device (R/W)
    uint8_t config_generation; // Incrémenté si la config device a changé
    /* --- Configuration de la queue sélectionnée --- */
    uint16_t queue_select;      // Sélectionne la queue à configurer
    uint16_t queue_size;        // Taille de la queue (nombre de descripteurs)
    uint16_t queue_msix_vector; // Vecteur MSI-X pour cette queue
    uint16_t queue_enable;      // 1 = queue activée
    uint16_t queue_notify_off;  // Offset pour calculer l'adresse de notif.
    uint64_t queue_desc;        // Adresse physique du descriptor table
    uint64_t queue_driver;      // Adresse physique du driver ring (avail)
    uint64_t queue_device;      // Adresse physique du device ring (used)
    uint16_t queue_notify_data;
    uint16_t queue_reset;
} __attribute__((packed, aligned(4))) virtio_pci_common_cfg;

/*
 * Parcourt la liste chaînée des capabilities PCI virtio et stocke tout dans
 * cfg_types_found.
 *   pci_header_addr : adresse de base du header PCI du
 * device (depuis pci_search)
 */
uint64_t find_virtio_cap(volatile pci_header_t0x0 *pci_header,
                         volatile cfg_virtio_device *cfg_types_found);

#endif
