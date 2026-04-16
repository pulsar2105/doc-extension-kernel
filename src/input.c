/*
 * input.c - virtio PCI input driver (keyboard + mouse)
 *
 * This file initializes virtio input devices (keyboard and mouse),
 * processes events coming from the virtqueue used ring, and displays
 * the current key and mouse coordinates on the screen.
 */

#include "input.h"

#include "keyboard.h"
#include "macros.h"
#include "mouse.h"
#include "pci.h"
#include "platform.h"
#include "plic.h"
#include "stdint.h"
#include "virtio.h"

// array of struct cfg_types for each device
volatile input_virtio_device virtio_device_inputs[2] = {0};
volatile uint8_t virtio_input_irqs[2] = {0};

/* mouse */
/* mouse buttons status */
volatile uint8_t lb_status = BUTTON_UP;
volatile uint8_t rb_status = BUTTON_UP;
volatile uint8_t mb_status = BUTTON_UP;
/* mouse position */
volatile int32_t mouse_x = 0;
volatile int32_t mouse_y = 0;
volatile int32_t mouse_x_old = 0;
volatile int32_t mouse_y_old = 0;

static uint8_t compute_pci_intx_irq(uint64_t pci_header_addr,
                                    uint8_t interrupt_pin) {
    uint8_t device =
        (uint8_t)((pci_header_addr - PCI_ECAM_BASE_ADDRESS) >> 15) & 0x1F;
    uint8_t pin = interrupt_pin;

    if (pin == 0 || pin > 4) {
        pin = 1; // default to INTA# for malformed/unknown pin value
    }
    return (uint8_t)(32 + ((device + pin - 1) % 4));
}

/* CONFIGURATION PCI DES DEUX DEVICES INPUT */

/*
 * Configure les registres PCI du périphérique virtio-keyboard-pci :
 *   - Active IO + Memory + Bus Master dans le Command Register
 *   - Écrit les adresses de nos BARs dans les registres BAR du device
 *   - Configure le numéro d'IRQ
 */
void pci_config_inputs() {
    // get pci header addrresses found
    volatile pci_header_t0x0 *pci_header1 =
        (volatile pci_header_t0x0 *)pci_devices_found[0];
    volatile pci_header_t0x0 *pci_header2 =
        (volatile pci_header_t0x0 *)pci_devices_found[1];

    INPUTS_DBG("[input] found devices @ 0x%lx and 0x%lx\n",
               pci_devices_found[0], pci_devices_found[1]);

    /*
     * Command Register (offset 0x04) :
     *   bit 0 : I/O Space Enable    → autorise les accès en I/O
     *   bit 1 : Memory Space Enable → autorise nos accès mémoire au device
     *   bit 2 : Bus Master Enable   → autorise le DMA (devi    ce accède à la
     * RAM)
     */
    pci_header1->command = pci_header1->command | 0x7;
    pci_header2->command = pci_header2->command | 0x7;

    /* BARs registers */
    pci_header1->bar[1] = VIRTIO_INPUT1_BASE_ADDRESS;
    pci_header1->bar[4] = VIRTIO_INPUT1_CONFIG_BASE_ADDRESS;
    pci_header1->bar[5] = 0;

    pci_header2->bar[1] = VIRTIO_INPUT2_BASE_ADDRESS;
    pci_header2->bar[4] = VIRTIO_INPUT2_CONFIG_BASE_ADDRESS;
    pci_header2->bar[5] = 0;

    /*
     * IRQ INTx:
     * - interrupt_pin décrit INTA#/INTB#/INTC#/INTD# côté device
     * - l'IRQ finale est dérivée du slot PCI (swizzling), donc non hardcodée
     */
    if (pci_header1->interrupt_pin == 0) {
        pci_header1->interrupt_pin = 1; // INTA#
    }
    if (pci_header2->interrupt_pin == 0) {
        pci_header2->interrupt_pin = 1; // INTA#
    }

    virtio_input_irqs[0] =
        compute_pci_intx_irq(pci_devices_found[0], pci_header1->interrupt_pin);
    virtio_input_irqs[1] =
        compute_pci_intx_irq(pci_devices_found[1], pci_header2->interrupt_pin);

    /*
     * interrupt_line is informational for the guest; real routing is done by
     * PCI INTx swizzling in the host bridge. We still expose computed IRQ.
     */
    pci_header1->interrupt_line = virtio_input_irqs[0];
    pci_header2->interrupt_line = virtio_input_irqs[1];
}

/*
 * INITIALISATION VIRTIO (spec virtio 1.2, section 3.1 "Device Initialization")
 *
 * Official sequence is :
 *   1. Reset du device                    (status = 0)
 *   2. ACKNOWLEDGE                        (status |= 1)
 *   3. DRIVER                             (status |= 2)
 *   4. Read/negociate features
 *   5. FEATURES_OK                        (status |= 8)
 *   6. Verify if FEATURES_OK is accepted
 *   7. Virtqueues configuration
 *   8. DRIVER_OK                          (status |= 4)
 */

void init_virtio_inputs() {
    for (uint32_t i = 0; i < 2; i++) {
        // get pci header addrresses found
        volatile pci_header_t0x0 *pci_header =
            (volatile pci_header_t0x0 *)pci_devices_found[i];

        /* --- Step 1 : common_cfg dans les BARs via les caps PCI --- */
        find_virtio_cap(pci_header, &(virtio_device_inputs[i].cfg_mem_map));
        uint64_t common_cfg_addr =
            virtio_device_inputs[i].cfg_mem_map.common_cfg;

        if (common_cfg_addr == 0) {
            INPUTS_DBG("[input] ERREUR: VIRTIO_PCI_CAP_COMMON_CFG not found\n");
            return;
        }

        volatile virtio_pci_common_cfg *cfg =
            (volatile virtio_pci_common_cfg *)common_cfg_addr;

        /* --- Étape 1 : Reset du device --- */
        cfg->device_status = 0;

        /* Attendre que le device confirme le reset (il repasse status à 0) */
        while (cfg->device_status != 0) {
        }

        /* --- Step 2-3 : ACKNOWLEDGE + DRIVER --- */
        cfg->device_status |= VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;

        /* --- Step 4 : Négociation des features --- */

        /* Features negociation (Bit 32 = Version 1) */
        cfg->driver_feature_select = 1; // page 1
        cfg->driver_feature =
            cfg->driver_feature | 1; // bit 32 VIRTIO_F_VERSION_1

        /*
         * On lit les features que le device supporte, tranche par tranche de 32
         * bits. On sélectionne d'abord la tranche 0 (bits 0..31). Les features
         * basique normalement.
         */
        cfg->device_feature_select = 0x0;
        uint32_t features = cfg->device_feature;

        INPUTS_DBG("[input][dev%u] features page0: 0x%x\n", i, features);

        /* --- Step 5 : FEATURES_OK --- */
        cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;

        /* --- Step 6 : Vérification que FEATURES_OK est accepté --- */
        uint8_t status = cfg->device_status;
        if (!(status & VIRTIO_STATUS_FEATURES_OK)) {
            INPUTS_DBG(
                "[input] Step 6 : ERREUR: FEATURES_OK refused by the device\n");

            cfg->device_status = VIRTIO_STATUS_FAILED;
            return;
        }

        INPUTS_DBG("[input][dev%u] num_queues=%u\n", i, cfg->num_queues);

        /* --- Step 7 : device specific configuration --- */

        /* Configuration virtqueue 0
         * Inputs :
         * 0 : eventq
         * 1 : statusq */
        cfg->queue_select = 0;
        cfg->queue_size = (uint16_t)QUEUE_SIZE;

        // spécifie l'adresse du descripteur de la queue 0
        cfg->queue_desc = (uint64_t)&virtio_device_inputs[i].queue0.descriptors;
        cfg->queue_driver =
            (uint64_t)&virtio_device_inputs[i].queue0.available_ring;
        cfg->queue_device = (uint64_t)&virtio_device_inputs[i].queue0.used_ring;

        /* initial filling of the queue */
        for (uint16_t j = 0; j < QUEUE_SIZE; j++) {
            virtio_device_inputs[i].queue0.descriptors[j].addr =
                (uint64_t)&virtio_device_inputs[i].event_pool[j];
            virtio_device_inputs[i].queue0.descriptors[j].len =
                sizeof(virtio_input_event);
            virtio_device_inputs[i].queue0.descriptors[j].flags =
                VIRTQ_DESC_F_WRITE;
            virtio_device_inputs[i].queue0.available_ring.ring[j] = j;
        }

        /*
         * Memory fence: ensure descriptors and available ring entries are
         * visible to the device before we update the idx and notify.
         */
        __asm__ __volatile__("fence" ::: "memory");

        /* initialisation des index */
        virtio_device_inputs[i].last_used_idx = 0;
        virtio_device_inputs[i].queue0.available_ring.idx = QUEUE_SIZE;

        // specifie event handler for each device
        if (i == 0) {
            virtio_device_inputs[i].handle_event = handle_keyboard_event;
        } else {
            virtio_device_inputs[i].handle_event = handle_mouse_event;
        }

        // activation de la queue
        cfg->queue_enable = 1;

        if (cfg->queue_enable != 1) {
            INPUTS_DBG("[input] Step 7 : ERREUR: queue activation failed\n");
            return;
        }

        /* --- Étape 8 : DRIVER_OK → le device est prêt --- */
        cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;

        /* Lire notify_off_multiplier depuis la notify BAR s'il existe */
        uint32_t notify_multiplier = 0;
        if (virtio_device_inputs[i].cfg_mem_map.notify_cfg != 0) {
            notify_multiplier = *((volatile uint32_t *)virtio_device_inputs[i]
                                      .cfg_mem_map.notify_cfg);
            virtio_device_inputs[i].notify_off = notify_multiplier;
        } else {
            INPUTS_DBG("[input] WARNING: notify_cfg absent\n");
        }

        /* Premier kick: notifier la queue via la notify BAR */
        volatile input_virtio_device *dev = &virtio_device_inputs[i];
        uint64_t notify_base = dev->cfg_mem_map.notify_cfg;
        uint64_t notify_addr = notify_base + ((uint64_t)cfg->queue_notify_off *
                                              (uint64_t)dev->notify_off);

        INPUTS_DBG("[input][dev%u] common=0x%lx notify=0x%lx isr=0x%lx qoff=%u "
                   "mult=%u naddr=0x%lx\n",
                   i, common_cfg_addr, notify_base, dev->cfg_mem_map.isr_status,
                   cfg->queue_notify_off, dev->notify_off, notify_addr);

        /* Ensure memory operations are visible to the device before notify */
        __asm__ __volatile__("fence" ::: "memory");

        /* Lire l'index used avant le kick pour vérifier que le device met à
         * jour la used ring */
        volatile virtq_used *used = &dev->queue0.used_ring;
        uint16_t used_before = used->idx;
        INPUTS_DBG("[input][dev%u] used_before=%u last_used=%u\n", i,
                   used_before, dev->last_used_idx);

        /* Write notify (kick) */
        if (notify_addr != 0) {
            *((volatile uint16_t *)notify_addr) = 0; // 0 = index de la queue
        }

        /* fence then re-read used idx */
        __asm__ __volatile__("fence" ::: "memory");
        uint16_t used_after = used->idx;
        INPUTS_DBG("[input][dev%u] used_after=%u\n", i, used_after);

        /* High-signal init summary kept enabled by default. */
        INPUTS_DBG("[input][dev%u] init OK (common=0x%lx irq=%u)\n", i,
                   common_cfg_addr, virtio_input_irqs[i]);
    }
}

/* Fonction principale pour confgurer le périphérique audio. */
uint64_t init_inputs() {
    uint64_t pci_header_addr =
        pci_search(PCI_ECAM_BASE_ADDRESS, 0x1af4, 0x1052);

    if (pci_header_addr == 0) {
        INPUTS_DBG("[input] ERREUR: virtio input devices (Vendor=0x1AF4, "
                   "Device=0x1052) not found on PCI bus.\n");
        return 1;
    }

    // configuration de base dans les headers PCI avec pci_devices_found
    pci_config_inputs();
    init_virtio_inputs();

    // configure PLIC for keyboard and mouse with computed INTx IRQs
    init_plic_pci(virtio_input_irqs[0], 3);
    init_plic_pci(virtio_input_irqs[1], 3);

    INPUTS_DBG("[input] using INTx IRQs: keyboard=%u mouse=%u\n",
               virtio_input_irqs[0], virtio_input_irqs[1]);

    return 0;
}

/* Called by trap_handler in interrupt.c when PLIC routes a VirtIO input IRQ. */
void virtio_input_handle_interrupt(int device_index) {
    volatile input_virtio_device *dev = &virtio_device_inputs[device_index];

    // Récupérer l'adresse de configuration pour accéder au queue_notify_off
    volatile virtio_pci_common_cfg *cfg =
        (volatile virtio_pci_common_cfg *)dev->cfg_mem_map.common_cfg;

    /* Read/ack ISR status as 8-bit, per virtio PCI spec (INTx deassert). */
    if (dev->cfg_mem_map.isr_status != 0) {
        uint8_t isr = *((volatile uint8_t *)dev->cfg_mem_map.isr_status);
        if ((isr & 0x1) == 0) {
            return; // not a virtqueue interrupt
        }
    }

    // access to the used ring
    volatile virtq_used *used = &dev->queue0.used_ring;

    while (dev->last_used_idx != used->idx) {
        // Récupérer l'élément de la used ring
        uint16_t idx = dev->last_used_idx % QUEUE_SIZE;
        volatile virtq_used_elem *elem = &used->ring[idx];

        // Récupérer l'événement dans le pool (basé sur l'ID du buffer)
        volatile virtio_input_event *event = &dev->event_pool[elem->id];

        // Appeler le handler spécifique (si défini)
        // On caste le pointeur pour enlever le qualificateur 'volatile'
        if (dev->handle_event) {
            dev->handle_event((virtio_input_event *)event);
        }

        // --- Ré-enfiler le buffer pour qu'il soit réutilisable ---
        // On remet l'index du buffer utilisé dans la available_ring
        uint16_t avail_idx = dev->queue0.available_ring.idx % QUEUE_SIZE;
        dev->queue0.available_ring.ring[avail_idx] = elem->id;
        dev->queue0.available_ring.idx++;

        dev->last_used_idx++;
    }

    // --- Kick le device pour prévenir qu'on a rendu des buffers
    uint64_t notify_base = dev->cfg_mem_map.notify_cfg;
    uint64_t notify_addr = notify_base + ((uint64_t)cfg->queue_notify_off *
                                          (uint64_t)dev->notify_off);
    *((volatile uint16_t *)notify_addr) = 0; // 0 = index de la queue
}

/* Function to handle keyboard events */
void handle_keyboard_event(virtio_input_event *event) {
    if (event->event_type == Key) {
        if (event->value == Pressed) {
            uint8_t code = event->code;

            // maj, ctrl, alt, alt_gr et maj_lock
            switch (code) {
            case RIGHT_MAJ:
            case LEFT_MAJ:
                write_maj_status(1);
                break;
            case RIGHT_CTRL:
            case LEFT_CTRL:
                write_ctrl_status(1);
                break;
            case ALT:
                write_alt_status(1);
                break;
            case ALT_GR:
                write_alt_gr_status(1);
                break;
            case MAJ_LOCK: {
                uint8_t maj_lock = read_maj_lock_status();
                write_maj_lock_status(!maj_lock);
                break;
            }
            case UP:
                write_arrow_status(1);
                break;
            case RIGHT:
                write_arrow_status(2);
                break;
            case DOWN:
                write_arrow_status(3);
                break;
            case LEFT:
                write_arrow_status(4);
                break;

            default:
                break;
            }

            char c = 0;

            if (read_maj_lock_status() ^ read_maj_status()) {
                c = (char)key_to_ascii_maj[code];
            } else if (read_alt_gr_status()) {
                c = (char)key_to_ascii_alt_gr[code];
            } else {
                c = (char)key_to_ascii_std[code];
            }

            if (c) {
                write_chr_buffer(c);

                INPUTS_DBG("[Keyboard] Key Pressed: code %d -> %c\n", code, c);
            } else {
                INPUTS_DBG("[Keyboard] Key Pressed: code %d (no mapping)\n",
                           code);
            }
        } else if (event->value == Released) {
            uint8_t code = event->code;

            // maj, ctrl, alt, alt_gr et maj_lock
            switch (code) {
            case RIGHT_MAJ:
            case LEFT_MAJ:
                write_maj_status(0);
                break;
            case RIGHT_CTRL:
            case LEFT_CTRL:
                write_ctrl_status(0);
                break;
            case ALT:
                write_alt_status(0);
                break;
            case ALT_GR:
                write_alt_gr_status(0);
                break;
            case UP:
            case RIGHT:
            case DOWN:
            case LEFT:
                write_arrow_status(0);
                break;

            default:
                break;
            }

            INPUTS_DBG("[Keyboard] Key Released: code %d\n", event->code);
        }
    }
}

/* Function called to handle mouse events */
void handle_mouse_event(virtio_input_event *event) {
    mouse_x_old = mouse_x;
    mouse_y_old = mouse_y;

    if (event->event_type == Relative) {
        int32_t delta = (int32_t)event->value;

        switch (event->code) {
        case X:
            mouse_x += delta;
            if (mouse_x < 0) {
                mouse_x = 0;
            }
            if (mouse_x >= DISPLAY_WIDTH) {
                mouse_x = DISPLAY_WIDTH - 1;
            }

            INPUTS_DBG("[Mouse] pos: x=%d y=%d\n", mouse_x, mouse_y);
            break;
        case Y:
            mouse_y += delta;
            if (mouse_y < 0) {
                mouse_y = 0;
            }
            if (mouse_y >= DISPLAY_HEIGHT) {
                mouse_y = DISPLAY_HEIGHT - 1;
            }
            INPUTS_DBG("[Mouse] pos: x=%d y=%d\n", mouse_x, mouse_y);
            break;
        case Wheel:
            INPUTS_DBG("[Mouse] Wheel: %d\n", delta);
            break;
        default:
            INPUTS_DBG("[Mouse] other event %d\n", event->code);
            break;
        }

        // update mouse cursor position on screen
        update_mouse_cursor(mouse_x_old, mouse_y_old, mouse_x, mouse_y);

    } else if (event->event_type == Pressed) {
        switch (event->code) {
        case LB:
            lb_status = lb_status == BUTTON_UP ? BUTTON_DOWN : BUTTON_UP;

            INPUTS_DBG("[Mouse] Left button\n");
            break;
        case RB:
            rb_status = rb_status == BUTTON_UP ? BUTTON_DOWN : BUTTON_UP;

            INPUTS_DBG("[Mouse] Right button\n");
            break;
        case MB:
            mb_status = mb_status == BUTTON_UP ? BUTTON_DOWN : BUTTON_UP;

            INPUTS_DBG("[Mouse] Middle button\n");
            break;
        case WHEEL_UP:
            INPUTS_DBG("[Mouse] Wheel up\n");
            break;
        case WHEEL_DOWN:
            INPUTS_DBG("[Mouse] Wheel down\n");
            break;
        }
    } else {
        // INPUTS_DBG("other event %d\n", event->event_type);
    }
}
