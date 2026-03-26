#ifndef __INPUT_H__
#define __INPUT_H__

#include "pci.h"
#include "stdint.h"
#include "virtio.h"

// Virtqueues (spec Oasis v1.2 section 2.6)

// CONSTANTS

#define QUEUE_SIZE 128 // must be a power of 2

/* This marks a buffer as continuing via the next field. */
#define VIRTQ_DESC_F_NEXT 1
/* This marks a buffer as device write-only (otherwise device read-only). */
#define VIRTQ_DESC_F_WRITE 2
/* This means the buffer contains a list of buffer descriptors. */
#define VIRTQ_DESC_F_INDIRECT 4

#define VIRTQ_AVAIL_F_NO_INTERRUPT 1

#define VIRTQ_USED_F_NO_NOTIFY 1

typedef enum { Sync = 0, Key = 1, Relative = 2, Absolute = 3 } input_event_type;

typedef enum { X = 0, Y = 1, Wheel = 8 } input_event_code_relative;

typedef enum {
    LB = 272,
    RB = 273,
    MB = 274,
    WHEEL_DOWN = 336,
    WHEEL_UP = 337
} input_event_code_abs;

typedef enum {
    Released = 0,
    Pressed = 1,
    Automatic_Repetition = 2,
} input_event_code_value;

typedef enum {
    BUTTON_UP = 0,
    BUTTON_DOWN = 1,
} mouse_button_status;

typedef struct __attribute__((packed)) {
    uint16_t event_type;
    uint16_t code;
    uint32_t value;
} virtio_input_event;

typedef struct __attribute__((packed)) {
    uint16_t event_type;
    uint16_t code; // input_event_code_relative
    uint32_t value;
} virtio_input_relative_event;

typedef struct __attribute__((packed)) {
    uint16_t event_type;
    uint16_t code;
    uint32_t value; // input_event_code_value
} virtio_key_event;

/* Virtqueue descriptors: 16 bytes.
 * These can chain together via "next". */
typedef struct {
    uint64_t addr;  // Address (guest-physical).
    uint32_t len;   // Length
    uint16_t flags; // The flags as indicated above.
    uint16_t next;  // Next field if flags & NEXT
} __attribute__((packed, aligned(4))) virtq_desc;

/* The device writes available ring entries with buffer head indexes. */
typedef struct {
    uint16_t flags;
    uint16_t idx;              // ATOMIC INSTRUCT
    uint16_t ring[QUEUE_SIZE]; // ring of elements
    uint16_t used_event;       // Only if VIRTIO_F_EVENT_IDX
} __attribute__((packed, aligned(4))) virtq_avail;

/* uint32_t is used here for ids for padding reasons. */
typedef struct {
    uint32_t id; // Index of start of used descriptor chain.
    /*
     * The number of bytes written into the device writable portion of
     * the buffer described by the descriptor chain.
     */
    uint32_t len;
} __attribute__((packed, aligned(4))) virtq_used_elem;

/* The device writes used elements into this ring. */
typedef struct {
    uint16_t flags;
    uint16_t idx; // ATOMIC INSTRUCT
    virtq_used_elem ring[QUEUE_SIZE];
    uint16_t avail_event; // Only if VIRTIO_F_EVENT_IDX
} __attribute__((packed, aligned(4))) virtq_used;

/* A virtqueue: a set of descriptors, an "available" ring, and a "used" ring.
 * These are all packed together in memory. */
typedef struct {
    virtq_desc descriptors[QUEUE_SIZE];
    virtq_avail available_ring;
    virtq_used used_ring;
} __attribute__((packed, aligned(4096))) virtq;

/* Inputs virtio device structure, include virtio_device */
typedef struct {
    cfg_virtio_device cfg_mem_map;             // cfg memory map
    virtq queue0;                              // input virtqueue
    virtio_input_event event_pool[QUEUE_SIZE]; // event pool
    uint16_t last_used_idx;
    uint32_t notify_off; // Multiplier from PCI notify capability (used to
                         // compute notify address)
    void (*handle_event)(virtio_input_event *event);
} input_virtio_device;

// GLOBAL variables

/* Virtio */
extern volatile input_virtio_device virtio_device_inputs[2];
extern volatile uint8_t virtio_input_irqs[2];

/* mouse buttons */
extern volatile uint8_t lb_status; // left mouse button status
extern volatile uint8_t rb_status; // right mouse button status
extern volatile uint8_t mb_status; // middle mouse button status

/* mouse position */
extern volatile int32_t mouse_x;
extern volatile int32_t mouse_y;

/* Configure PCI registers of virtio-input devices. */
void pci_config_inputs();

/* Initialise virtio input devices (spec virtio 1.2, section 3.1) */
void init_virtio_inputs();

/* Function to handle input events */
void virtio_input_handle_interrupt(int device_index);

/* Entry point to initialize input devices */
uint64_t init_inputs();

/* Function to handle keyboard events */
void handle_keyboard_event(virtio_input_event *event);

/* Function called to handle mouse events */
void handle_mouse_event(virtio_input_event *event);

#endif /* __INPUT_H__ */
