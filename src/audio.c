#include "audio.h"

#include "cpu.h"
#include "macros.h"
#include "mem_tools.h"
#include "pci.h"
#include "platform.h"
#include "plic.h"
#include "stdint.h"
#include "time.h"
#include "virtio.h"

/* Data for beep */
enum {
    SAMPLE_RATE = 5512,
    FREQUENCY = 440,
    DURATION_MS = 200, // 200ms beep
    NUM_SAMPLES = ((SAMPLE_RATE * DURATION_MS) / 1000),
    SND_VOLUME = 15000
};

volatile cfg_virtio_device cfg_types_found_audio = {0};
volatile audio_virtio_device virtio_audio_dev = {0};
volatile uint8_t virtio_audio_irq = 0;

/* CONFIGURATION PCI DU DEVICE AUDIO */

/*
 * Configure les registres PCI du périphérique virtio-sound :
 *   - Active IO + Memory + Bus Master dans le Command Register
 *   - Écrit les adresses de nos BARs dans les registres BAR du device
 *   - Configure le numéro d'IRQ
 */
void pci_config_audio(volatile pci_header_t0x0 *pci_header,
                      uint64_t pci_header_addr) {
    /*
     * Command Register (offset 0x04) :
     *   bit 0 : I/O Space Enable    → autorise les accès en I/O
     *   bit 1 : Memory Space Enable → autorise nos accès mémoire au device
     *   bit 2 : Bus Master Enable   → autorise le DMA (devi    ce accède à la
     * RAM)
     */
    pci_header->command = pci_header->command | 0x7;

    // BAR1 (offset 0x14) : zone principale virtio
    pci_header->bar[1] = VIRTIO_SND_BASE_ADDRESS;

    // BAR4 (offset 0x20) : zone de config spécifique au device audio
    pci_header->bar[4] = VIRTIO_SND_CONFIG_BASE_ADDRESS;
    // ad hoc we know here that BAR is in 64 bits
    pci_header->bar[5] = 0;

    /* IRQ INTx */
    if (pci_header->interrupt_pin == 0) {
        pci_header->interrupt_pin = 1;
    }

    virtio_audio_irq =
        compute_pci_intx_irq(pci_header_addr, pci_header->interrupt_pin);
    pci_header->interrupt_line = virtio_audio_irq;
}

/*
 * INITIALISATION VIRTIO (spec virtio 1.2, section 3.1 "Device Initialization")
 *
 * La séquence officielle est :
 *   1. Reset du device                    (status = 0)
 *   2. ACKNOWLEDGE                        (status |= 1)
 *   3. DRIVER                             (status |= 2)
 *   4. Lire/négocier les features
 *   5. FEATURES_OK                        (status |= 8)
 *   6. Vérifier FEATURES_OK accepté
 *   7. Configurer les virtqueues
 *   8. DRIVER_OK                          (status |= 4)
 */

void initialise_audio(volatile pci_header_t0x0 *pci_header) {
    /* --- Étape 1 : trouver la common_cfg dans les BARs via les caps PCI --- */
    find_virtio_cap(pci_header, &cfg_types_found_audio);
    uint64_t common_cfg_addr = cfg_types_found_audio.common_cfg;

    if (common_cfg_addr == 0) {
        AUDIO_DBG("[audio] ERREUR: VIRTIO_PCI_CAP_COMMON_CFG introuvable\n");
        return;
    }

    AUDIO_DBG("[audio] Audio Initialisation\n");
    AUDIO_DBG("[audio] common_cfg @ 0x%lx\n", common_cfg_addr);

    /* Copy cfg to device structure */
    virtio_audio_dev.cfg_mem_map = cfg_types_found_audio;

    /*
     * On mappe la structure virtio_pci_common_cfg sur l'adresse trouvée.
     */
    volatile virtio_pci_common_cfg *cfg =
        (volatile virtio_pci_common_cfg *)common_cfg_addr;

    /* --- Étape 1 : Reset du device --- */
    /* Écrire 0 dans device_status déclenche un reset complet. */
    cfg->device_status = 0;

    /* Attendre que le device confirme le reset (il repasse status à 0) */
    while (cfg->device_status != 0) {
    }

    AUDIO_DBG("[audio] Step 1 : reset OK\n");

    /* --- Étape 2-3 : ACKNOWLEDGE + DRIVER --- */
    /*
     * ACKNOWLEDGE (bit 0) : "j'ai trouvé et reconnu le device"
     * DRIVER      (bit 1) : "je sais comment piloter ce device"
     */
    cfg->device_status |= VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;

    AUDIO_DBG("[audio] Step 2-3 : ACKNOWLEDGE + DRIVER OK\n");

    /* --- Étape 4 : Négociation des features --- */
    /*
     * On lit les features que le device supporte, tranche par tranche de 32
     * bits. On sélectionne d'abord la tranche 0 (bits 0..31). Les features
     * basique normalement.
     */
    cfg->device_feature_select = 0x0;
    uint32_t features = cfg->device_feature;

    AUDIO_DBG("[audio] Step 4 : device features : 0x%x\n", features);

    /* On accepte 0 feature optionnelle (on ne veut que le fonctionnement de
     * base) */
    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0;

    /* --- Étape 5 : FEATURES_OK --- */
    /*
     * On signale qu'on a fini la négociation des features.
     * IMPORTANT : après cet écriture, il faut RE-LIRE device_status pour
     * vérifier que le device a bien accepté (bit FEATURES_OK toujours présent).
     */
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;

    AUDIO_DBG("[audio] Step 5 : Try FEATURES_OK\n");

    /* --- Étape 6 : Vérification que FEATURES_OK est accepté --- */
    uint8_t status = cfg->device_status;
    if (!(status & VIRTIO_STATUS_FEATURES_OK)) {
        AUDIO_DBG(
            "[audio] Step 6 : ERREUR: FEATURES_OK refused by the device\n");

        cfg->device_status = VIRTIO_STATUS_FAILED;
        return;
    }

    AUDIO_DBG("[audio] Step 6 : features OK\n");

    /* --- Étape 7 : Infos sur les queues disponibles --- */
    AUDIO_DBG("[audio] Step 7 : %d virtqueue available\n", cfg->num_queues);

    /*
     * Un device virtio-sound expose 4 queues :
     *   Queue 0 : controlq  => envoyer des commandes (ex: set_params)
     *   Queue 1 : eventq    => recevoir des événements du device
     *   Queue 2 : txq       => envoyer des samples audio (playback)
     *   Queue 3 : rxq       => recevoir des samples audio (capture)
     *
     * La configuration des queues (adresses des descriptor tables, etc.)
     * sera faite dans une étape ultérieure lors de l'initialisation des
     * virtqueues.
     */

    /* */

    /* Configure controlq (queue 0) */
    cfg->queue_select = 0;
    cfg->queue_size = (uint16_t)QUEUE_SIZE_AUDIO;
    cfg->queue_desc = (uint64_t)&virtio_audio_dev.controlq.descriptors;
    cfg->queue_driver = (uint64_t)&virtio_audio_dev.controlq.available_ring;
    cfg->queue_device = (uint64_t)&virtio_audio_dev.controlq.used_ring;

    /* Initialize control queue indices */
    virtio_audio_dev.last_used_idx_ctrl = 0;
    virtio_audio_dev.controlq.available_ring.idx = 0;

    __asm__ __volatile__("fence" ::: "memory");
    cfg->queue_enable = 1;

    if (cfg->queue_enable != 1) {
        AUDIO_DBG("[audio] ERREUR: controlq activation failed\n");
        return;
    }

    AUDIO_DBG("[audio] controlq (queue 0) configured\n");

    /* EVENTQ */

    /* Configure eventq (queue 1) */
    cfg->queue_select = 1;
    cfg->queue_size = (uint16_t)QUEUE_SIZE_AUDIO;
    cfg->queue_desc = (uint64_t)&virtio_audio_dev.eventq.descriptors;
    cfg->queue_driver = (uint64_t)&virtio_audio_dev.eventq.available_ring;
    cfg->queue_device = (uint64_t)&virtio_audio_dev.eventq.used_ring;

    virtio_audio_dev.eventq.available_ring.idx = 0;

    __asm__ __volatile__("fence" ::: "memory");
    cfg->queue_enable = 1;

    if (cfg->queue_enable != 1) {
        AUDIO_DBG("[audio] ERREUR: eventq activation failed\n");
        return;
    }
    AUDIO_DBG("[audio] eventq (queue 1) configured\n");

    /* TXQ */

    /* Configure txq (queue 2) */
    cfg->queue_select = 2;
    cfg->queue_size = (uint16_t)QUEUE_SIZE_AUDIO;
    cfg->queue_desc = (uint64_t)&virtio_audio_dev.txq.descriptors;
    cfg->queue_driver = (uint64_t)&virtio_audio_dev.txq.available_ring;
    cfg->queue_device = (uint64_t)&virtio_audio_dev.txq.used_ring;

    /* Initialize tx queue indices */
    virtio_audio_dev.last_used_idx_tx = 0;
    virtio_audio_dev.txq.available_ring.idx = 0;

    __asm__ __volatile__("fence" ::: "memory");
    cfg->queue_enable = 1;

    if (cfg->queue_enable != 1) {
        AUDIO_DBG("[audio] ERREUR: txq activation failed\n");
        return;
    }

    AUDIO_DBG("[audio] txq (queue 2) configured\n");

    /* RXQ - not use here */

    /* Configure rxq (queue 3) */
    cfg->queue_select = 3;
    cfg->queue_size = (uint16_t)QUEUE_SIZE_AUDIO;
    cfg->queue_desc = (uint64_t)&virtio_audio_dev.rxq.descriptors;
    cfg->queue_driver = (uint64_t)&virtio_audio_dev.rxq.available_ring;
    cfg->queue_device = (uint64_t)&virtio_audio_dev.rxq.used_ring;

    virtio_audio_dev.rxq.available_ring.idx = 0;

    __asm__ __volatile__("fence" ::: "memory");
    cfg->queue_enable = 1;

    if (cfg->queue_enable != 1) {
        AUDIO_DBG("[audio] ERREUR: rxq activation failed\n");
        return;
    }
    AUDIO_DBG("[audio] rxq (queue 3) configured\n");

    /* Get notify multiplier from capability (preferred) */
    if (cfg_types_found_audio.notify_off_multiplier != 0) {
        virtio_audio_dev.notify_off =
            cfg_types_found_audio.notify_off_multiplier;
    } else {
        /* Fallback: read first dword at notify_cfg (legacy pattern) */
        if (virtio_audio_dev.cfg_mem_map.notify_cfg != 0) {
            virtio_audio_dev.notify_off =
                *((volatile uint32_t *)virtio_audio_dev.cfg_mem_map.notify_cfg);
        } else {
            AUDIO_DBG("[audio] WARNING: notify_cfg absent\n");
            virtio_audio_dev.notify_off = 4; /* safe default */
        }
    }

    AUDIO_DBG("[audio] Step 7 : device-specific configuration done. \n");

    /* --- Étape 8 : DRIVER_OK → le device est prêt --- */
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;

    AUDIO_DBG("[audio] Step 8 : init virtio-sound OK (irq=%u)\n",
              virtio_audio_irq);
}

/* Fonction principale pour confgurer le périphérique audio. */
uint64_t init_audio() {
    uint64_t pci_header_addr =
        pci_search(PCI_ECAM_BASE_ADDRESS, 0x1af4, 0x1059);

    if (pci_header_addr == 0) {
        AUDIO_DBG("[audio] ERREUR: virtio-sound device (Vendor=0x1AF4, "
                  "Device=0x1059) not found on PCI bus.\n");
        return 1;
    }

    volatile pci_header_t0x0 *pci_header =
        (volatile pci_header_t0x0 *)pci_header_addr;

    AUDIO_DBG("[audio] founded device @ 0x%lx\n", pci_header_addr);

    // configuration de base dans le header PCI
    pci_config_audio(pci_header, pci_header_addr);

    // configuration du device audio
    initialise_audio(pci_header);

    // configure PLIC for audio with computed INTx IRQ
    init_plic_pci(virtio_audio_irq, 3);

    AUDIO_DBG("[audio] using INTx IRQ: %u\n", virtio_audio_irq);

    return 0;
}

/* Helper to send a control command to the audio device */
static void send_control_command(volatile void *request, uint32_t request_len,
                                 volatile void *response,
                                 uint32_t response_len) {
    volatile virtio_pci_common_cfg *cfg =
        (volatile virtio_pci_common_cfg *)
            virtio_audio_dev.cfg_mem_map.common_cfg;

    uint16_t idx =
        virtio_audio_dev.controlq.available_ring.idx % QUEUE_SIZE_AUDIO;
    uint16_t desc_idx = idx;

    /* First descriptor: request (device reads) */
    virtio_audio_dev.controlq.descriptors[desc_idx].addr = (uint64_t)request;
    virtio_audio_dev.controlq.descriptors[desc_idx].len = request_len;
    virtio_audio_dev.controlq.descriptors[desc_idx].flags = VIRTQ_DESC_F_NEXT;
    virtio_audio_dev.controlq.descriptors[desc_idx].next =
        (desc_idx + 1) % QUEUE_SIZE_AUDIO;

    /* Second descriptor: response (device writes) */
    uint16_t resp_idx = (desc_idx + 1) % QUEUE_SIZE_AUDIO;
    virtio_audio_dev.controlq.descriptors[resp_idx].addr = (uint64_t)response;
    virtio_audio_dev.controlq.descriptors[resp_idx].len = response_len;
    virtio_audio_dev.controlq.descriptors[resp_idx].flags = VIRTQ_DESC_F_WRITE;
    virtio_audio_dev.controlq.descriptors[resp_idx].next = 0;

    /* Add to available ring */
    virtio_audio_dev.controlq.available_ring.ring[idx] = desc_idx;

    __asm__ __volatile__("fence" ::: "memory");

    virtio_audio_dev.controlq.available_ring.idx++;

    /* Notify the device (queue 0) */
    cfg->queue_select = 0;
    uint64_t notify_base = virtio_audio_dev.cfg_mem_map.notify_cfg;
    uint64_t notify_addr =
        notify_base + ((uint64_t)cfg->queue_notify_off *
                       (uint64_t)virtio_audio_dev.notify_off);

    __asm__ __volatile__("fence" ::: "memory");

    if (notify_addr != 0) {
        *((volatile uint16_t *)notify_addr) = 0;
    }

    /* Wait for response */
    volatile virtq_used_audio *used = &virtio_audio_dev.controlq.used_ring;
    while (virtio_audio_dev.last_used_idx_ctrl == used->idx) {
        /* busy wait */
    }

    virtio_audio_dev.last_used_idx_ctrl = used->idx;
}

/* Generate a simple sine wave beep */
void play_beep() {
    AUDIO_DBG("[audio] Playing beep...\n");

    /* We will follow the PCM Command Lifecycle described by Oasis 5.14.6.6.1 */

    volatile virtio_pci_common_cfg *cfg =
        (volatile virtio_pci_common_cfg *)
            virtio_audio_dev.cfg_mem_map.common_cfg;

    /* Step 1: Set PCM parameters */
    volatile virtio_snd_pcm_set_params set_params;
    volatile virtio_snd_status status_response;

    /* Calculate buffer size for 200ms at 44.1kHz stereo S16 (2 bytes) */
    /* 48000 * 0.2 * 2 channels * 2 bytes = 38400 bytes */
    uint32_t buffer_size = (SAMPLE_RATE * DURATION_MS * 2 * 2) / 1000;

    set_params.hdr.code = VIRTIO_SND_R_PCM_SET_PARAMS;
    set_params.stream_id = 0;
    set_params.buffer_bytes = buffer_size;
    set_params.period_bytes = buffer_size; // Single period
    set_params.features = 0;
    set_params.channels = 2;
    set_params.format = VIRTIO_SND_PCM_FMT_S16;
    set_params.rate = VIRTIO_SND_PCM_RATE_5512;
    set_params.padding = 0;

    send_control_command(&set_params, sizeof(set_params), &status_response,
                         sizeof(status_response));

    if (status_response.status != VIRTIO_SND_S_OK) {
        AUDIO_DBG("[audio] SET_PARAMS failed: status=0x%x\n",
                  status_response.status);
        return;
    }

    AUDIO_DBG("[audio] SET_PARAMS OK\n");

    /* Step 2: Prepare stream */
    volatile virtio_snd_pcm_simple prepare_cmd;
    prepare_cmd.hdr.code = VIRTIO_SND_R_PCM_PREPARE;
    prepare_cmd.stream_id = 0;

    send_control_command(&prepare_cmd, sizeof(prepare_cmd), &status_response,
                         sizeof(status_response));

    if (status_response.status != VIRTIO_SND_S_OK) {
        AUDIO_DBG("[audio] PREPARE failed: status=0x%x\n",
                  status_response.status);
        return;
    }

    AUDIO_DBG("[audio] PREPARE OK\n");

    /* Step 3: Start stream */
    volatile virtio_snd_pcm_simple start_cmd;
    start_cmd.hdr.code = VIRTIO_SND_R_PCM_START;
    start_cmd.stream_id = 0;

    send_control_command(&start_cmd, sizeof(start_cmd), &status_response,
                         sizeof(status_response));

    if (status_response.status != VIRTIO_SND_S_OK) {
        AUDIO_DBG("[audio] START failed: status=0x%x\n",
                  status_response.status);
        return;
    }

    AUDIO_DBG("[audio] START OK\n");

    /* Step 4: Generate and send audio data (440 Hz beep) */

    volatile int16_t audio_buffer[NUM_SAMPLES * 2] __attribute__((aligned(16)));

    AUDIO_DBG("[audio] Generating %d samples\n", NUM_SAMPLES);

    /* Generate a simple square wave */
    for (int i = 0; i < NUM_SAMPLES; i++) {
        /* Simple square wave for testing - audible volume */
        int16_t sample;
        int phase = (i * FREQUENCY) / SAMPLE_RATE;

        if (phase % 2 == 0) {
            sample = SND_VOLUME;
        } else {
            sample = -SND_VOLUME; /* Negative */
        }

        audio_buffer[i * 2] = sample;     /* Left channel */
        audio_buffer[i * 2 + 1] = sample; /* Right channel */
    }

    /* Send audio data via txq (queue 2) */
    volatile virtio_snd_pcm_xfer xfer_hdr;
    xfer_hdr.stream_id = 0;

    uint16_t idx = virtio_audio_dev.txq.available_ring.idx % QUEUE_SIZE_AUDIO;
    uint16_t desc_idx = idx;

    /* First descriptor: transfer header */
    virtio_audio_dev.txq.descriptors[desc_idx].addr = (uint64_t)&xfer_hdr;
    virtio_audio_dev.txq.descriptors[desc_idx].len = sizeof(xfer_hdr);
    virtio_audio_dev.txq.descriptors[desc_idx].flags = VIRTQ_DESC_F_NEXT;
    virtio_audio_dev.txq.descriptors[desc_idx].next =
        (desc_idx + 1) % QUEUE_SIZE_AUDIO;

    /* Second descriptor: audio data (device reads) */
    uint16_t data_idx = (desc_idx + 1) % QUEUE_SIZE_AUDIO;
    virtio_audio_dev.txq.descriptors[data_idx].addr = (uint64_t)audio_buffer;
    virtio_audio_dev.txq.descriptors[data_idx].len =
        NUM_SAMPLES * 2 * sizeof(int16_t);
    virtio_audio_dev.txq.descriptors[data_idx].flags =
        VIRTQ_DESC_F_NEXT; /* device reads */
    virtio_audio_dev.txq.descriptors[data_idx].next =
        (data_idx + 1) % QUEUE_SIZE_AUDIO;

    /* Third descriptor: status (device writes) */
    static virtio_snd_status xfer_status;
    uint16_t status_idx = (data_idx + 1) % QUEUE_SIZE_AUDIO;
    xfer_status.status = 0;
    virtio_audio_dev.txq.descriptors[status_idx].addr = (uint64_t)&xfer_status;
    virtio_audio_dev.txq.descriptors[status_idx].len = sizeof(xfer_status);
    virtio_audio_dev.txq.descriptors[status_idx].flags = VIRTQ_DESC_F_WRITE;
    virtio_audio_dev.txq.descriptors[status_idx].next = 0;

    /* Add to available ring */
    virtio_audio_dev.txq.available_ring.ring[idx] = desc_idx;

    __asm__ __volatile__("fence" ::: "memory");

    virtio_audio_dev.txq.available_ring.idx++;

    /* Notify the device (queue 2) */
    cfg->queue_select = 2;
    uint64_t notify_base = virtio_audio_dev.cfg_mem_map.notify_cfg;
    uint64_t notify_addr =
        notify_base + ((uint64_t)cfg->queue_notify_off *
                       (uint64_t)virtio_audio_dev.notify_off);

    __asm__ __volatile__("fence" ::: "memory");

    if (notify_addr != 0) {
        write_mem16(notify_addr, 0, 2);
    }

    AUDIO_DBG("[audio] Audio data sent (size=%u bytes)\n",
              NUM_SAMPLES * 2 * (uint32_t)sizeof(int16_t));

    /* Wait for used ring update with timeout */
    volatile virtq_used_audio *used = &virtio_audio_dev.txq.used_ring;
    uint16_t start_used = used->idx;

    while (used->idx == start_used) {
        hlt();
    }

    if (used->idx == start_used) {
        AUDIO_DBG("[audio] TX timeout (used idx unchanged)\n");
    } else {
        AUDIO_DBG("[audio] TX completed (used idx=%u) status=0x%x\n", used->idx,
                  xfer_status.status);
    }

    /* Let the audio play: wait longer than buffer duration */
    AUDIO_DBG("[audio] Playing...\n");

    sleep_ms(DURATION_MS);

    AUDIO_DBG("[audio] Playback wait done\n");

    /* Step 5: Stop stream */
    static virtio_snd_pcm_simple stop_cmd;
    stop_cmd.hdr.code = VIRTIO_SND_R_PCM_STOP;
    stop_cmd.stream_id = 0;

    send_control_command(&stop_cmd, sizeof(stop_cmd), &status_response,
                         sizeof(status_response));

    if (status_response.status != VIRTIO_SND_S_OK) {
        AUDIO_DBG("[audio] STOP failed: status=0x%x\n", status_response.status);
        return;
    }
    AUDIO_DBG("[audio] STOP OK\n");

    /* Step 6: Release stream */
    static virtio_snd_pcm_simple release_cmd;
    release_cmd.hdr.code = VIRTIO_SND_R_PCM_RELEASE;
    release_cmd.stream_id = 0;

    send_control_command(&release_cmd, sizeof(release_cmd), &status_response,
                         sizeof(status_response));

    if (status_response.status != VIRTIO_SND_S_OK) {
        AUDIO_DBG("[audio] RELEASE failed: status=0x%x\n",
                  status_response.status);
        return;
    }
    AUDIO_DBG("[audio] RELEASE OK\n");

    AUDIO_DBG("[audio] Beep completed!\n");
}

/* Handle VirtIO sound interrupt */
void virtio_audio_handle_interrupt() {
    /* Read/ack ISR status */
    if (virtio_audio_dev.cfg_mem_map.isr_status != 0) {
        uint8_t isr =
            *((volatile uint8_t *)virtio_audio_dev.cfg_mem_map.isr_status);
        if ((isr & 0x1) == 0) {
            return;
        }
    }

    AUDIO_DBG("[audio] Interrupt received\n");
}
