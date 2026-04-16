#ifndef __AUDIO_H__
#define __AUDIO_H__

#include "pci.h"
#include "stdint.h"
#include "virtio.h"

#define QUEUE_SIZE_AUDIO 128

/* VirtIO Sound control request codes */
enum {
    VIRTIO_SND_R_JACK_INFO = 1,
    VIRTIO_SND_R_PCM_INFO = 0x0100,
    VIRTIO_SND_R_PCM_SET_PARAMS = 0x0101,
    VIRTIO_SND_R_PCM_PREPARE = 0x0102,
    VIRTIO_SND_R_PCM_RELEASE = 0x0103,
    VIRTIO_SND_R_PCM_START = 0x0104,
    VIRTIO_SND_R_PCM_STOP = 0x0105
};

/* VirtIO Sound status codes */
enum {
    VIRTIO_SND_S_OK = 0x8000,
    VIRTIO_SND_S_BAD_MSG = 0x8001,
    VIRTIO_SND_S_NOT_SUPP = 0x8002,
    VIRTIO_SND_S_IO_ERR = 0x8003
};

/* PCM format - VirtIO Sound spec values */
enum {
    VIRTIO_SND_PCM_FMT_IMA_ADPCM = 0,
    VIRTIO_SND_PCM_FMT_MU_LAW = 1,
    VIRTIO_SND_PCM_FMT_A_LAW = 2,
    VIRTIO_SND_PCM_FMT_S8 = 3,
    VIRTIO_SND_PCM_FMT_U8 = 4,
    VIRTIO_SND_PCM_FMT_S16 = 5,
    VIRTIO_SND_PCM_FMT_U16 = 6,
    VIRTIO_SND_PCM_FMT_S18_3 = 7,
    VIRTIO_SND_PCM_FMT_U18_3 = 8,
    VIRTIO_SND_PCM_FMT_S20_3 = 9,
    VIRTIO_SND_PCM_FMT_U20_3 = 10,
    VIRTIO_SND_PCM_FMT_S24_3 = 11,
    VIRTIO_SND_PCM_FMT_U24_3 = 12,
    VIRTIO_SND_PCM_FMT_S20 = 13,
    VIRTIO_SND_PCM_FMT_U20 = 14,
    VIRTIO_SND_PCM_FMT_S24 = 15,
    VIRTIO_SND_PCM_FMT_U24 = 16,
    VIRTIO_SND_PCM_FMT_S32 = 17,
    VIRTIO_SND_PCM_FMT_U32 = 18,
    VIRTIO_SND_PCM_FMT_FLOAT = 19,
    VIRTIO_SND_PCM_FMT_FLOAT64 = 20,
    VIRTIO_SND_PCM_FMT_DSD_U8 = 21,
    VIRTIO_SND_PCM_FMT_DSD_U16 = 22,
    VIRTIO_SND_PCM_FMT_DSD_U32 = 23,
    VIRTIO_SND_PCM_FMT_IEC958_SUBFRAME = 24
};

/* PCM rates - VirtIO Sound spec values */
enum {

    VIRTIO_SND_PCM_RATE_5512 = 0,
    VIRTIO_SND_PCM_RATE_8000 = 1,
    VIRTIO_SND_PCM_RATE_11025 = 2,
    VIRTIO_SND_PCM_RATE_16000 = 3,
    VIRTIO_SND_PCM_RATE_22050 = 4,
    VIRTIO_SND_PCM_RATE_32000 = 5,
    VIRTIO_SND_PCM_RATE_44100 = 6,
    VIRTIO_SND_PCM_RATE_48000 = 7,
    VIRTIO_SND_PCM_RATE_64000 = 8,
    VIRTIO_SND_PCM_RATE_88200 = 9,
    VIRTIO_SND_PCM_RATE_96000 = 10,
    VIRTIO_SND_PCM_RATE_176400 = 11,
    VIRTIO_SND_PCM_RATE_192000 = 12,
    VIRTIO_SND_PCM_RATE_384000 = 13
};

/* Virtqueue descriptor flags */
enum { VIRTQ_DESC_F_NEXT = 1, VIRTQ_DESC_F_WRITE = 2 };

/* Virtqueue descriptor */
typedef struct {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed, aligned(4))) virtq_desc_audio;

/* Available ring */
typedef struct {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[QUEUE_SIZE_AUDIO];
    uint16_t used_event;
} __attribute__((packed, aligned(4))) virtq_avail_audio;

/* Used element */
typedef struct {
    uint32_t id;
    uint32_t len;
} __attribute__((packed, aligned(4))) virtq_used_elem_audio;

/* Used ring */
typedef struct {
    uint16_t flags;
    uint16_t idx;
    virtq_used_elem_audio ring[QUEUE_SIZE_AUDIO];
    uint16_t avail_event;
} __attribute__((packed, aligned(4))) virtq_used_audio;

/* Virtqueue */
typedef struct {
    virtq_desc_audio descriptors[QUEUE_SIZE_AUDIO];
    virtq_avail_audio available_ring;
    virtq_used_audio used_ring;
} __attribute__((packed, aligned(4096))) virtq_audio;

/* VirtIO Sound control request header */
typedef struct {
    uint32_t code;
} __attribute__((packed)) virtio_snd_hdr;

/* VirtIO Sound PCM set params request */
typedef struct {
    virtio_snd_hdr hdr;
    uint32_t stream_id;
    uint32_t buffer_bytes;
    uint32_t period_bytes;
    uint32_t features;
    uint8_t channels;
    uint8_t format;
    uint8_t rate;
    uint8_t padding;
} __attribute__((packed, aligned(4))) virtio_snd_pcm_set_params;

/* VirtIO Sound simple request (for prepare, start, stop) */
typedef struct {
    virtio_snd_hdr hdr;
    uint32_t stream_id;
} __attribute__((packed, aligned(4))) virtio_snd_pcm_simple;

/* VirtIO Sound status response */
typedef struct {
    uint32_t status;
} __attribute__((packed, aligned(4))) virtio_snd_status;

/* VirtIO Sound PCM transfer header */
typedef struct {
    uint32_t stream_id;
} __attribute__((packed, aligned(4))) virtio_snd_pcm_xfer;

/* Audio device structure */
typedef struct {
    cfg_virtio_device cfg_mem_map;
    virtq_audio controlq;
    virtq_audio eventq;
    virtq_audio txq;
    virtq_audio rxq;
    uint16_t last_used_idx_ctrl;
    uint16_t last_used_idx_tx;
    uint32_t notify_off;
} audio_virtio_device;

// GLOBAL variables

extern volatile cfg_virtio_device cfg_types_found_audio;
extern volatile audio_virtio_device virtio_audio_dev;
extern volatile uint8_t virtio_audio_irq;

/*
 * Configure les registres PCI du périphérique virtio-sound :
 *   - Active IO + Memory + Bus Master dans le Command Register (offset 0x04)
 *   - Écrit VIRTIO_SOUND_BASE_ADDRESS dans BAR1 (offset 0x14)
 *   - Écrit VIRTIO_SOUND_CONFIG_BASE_ADDRESS dans BAR4 (offset 0x20)
 *   - Configure le numéro d'IRQ 33 dans l'Interrupt Line Register (offset 0x3C)
 *
 *   pci_header : adresse de base du header PCI du device
 *   pci_header_addr : adresse du header PCI
 */
void pci_config_audio(volatile pci_header_t0x0 *pci_header,
                      uint64_t pci_header_addr);

/*
 * Exécute la séquence d'initialisation virtio (spec virtio 1.2, section 3.1) :
 *   1. Reset du device
 *   2. ACKNOWLEDGE + DRIVER
 *   3. Négociation des features
 *   4. FEATURES_OK + vérification
 *   5. DRIVER_OK
 *
 *   pci_header_addr : adresse de base du header PCI du device
 */
void initialise_audio(volatile pci_header_t0x0 *pci_header);

/*
 * Point d'entrée principal :
 *   - Recherche le device virtio-sound sur le bus PCI (Vendor=0x1AF4,
 * Device=0x1059)
 *   - Appelle pci_config_audio() puis config_audio()
 *   Retourne 0 en cas de succès, 1 si le device est introuvable.
 */
uint64_t init_audio();

/*
 * Play a simple beep (440 Hz tone for a short duration)
 */
void play_beep();

/*
 * Handle VirtIO sound interrupt
 */
void virtio_audio_handle_interrupt();

#endif /* __AUDIO_H__ */
