#ifndef __KEYMAP_H__
#define __KEYMAP_H__

#include "stdint.h"

enum {
    UP = 103,
    DOWN = 108,
    LEFT = 105,
    RIGHT = 106,

    RIGHT_MAJ = 42,
    LEFT_MAJ = 54,

    MAJ_LOCK = 58,

    RIGHT_CTRL = 29,
    LEFT_CTRL = 97,

    ALT = 56,
    ALT_GR = 100,
    SUPER,
    ENTER = '\t',
    VERR_NUM = 69
};

/* Variable that store the text buffer*/
extern volatile char keyboard_input_chr;

/* modifier keys */
extern volatile uint8_t keyboard_input_maj;
extern volatile uint8_t keyboard_input_ctrl;
extern volatile uint8_t keyboard_input_alt;
extern volatile uint8_t keyboard_input_alt_gr;
extern volatile uint8_t keyboard_input_maj_lock;

/* arrow keys
 * up = 1
 * right = 2
 * down = 3
 * left = 4
 */
extern volatile uint8_t keyboard_input_arrow;

extern volatile uint8_t pending_character;

/* array to convert the keyboard code as index to ASCII */
extern volatile uint8_t key_to_ascii_std[126];
extern volatile uint8_t key_to_ascii_maj[126];
extern volatile uint8_t key_to_ascii_alt_gr[126];

/* Function to convert AZERTY to UTF8 */
extern uint8_t azerty_to_utf8(uint8_t maj, uint8_t alt_gr, uint8_t key_code);

/* Function to write a character into the main buffer,
 * and set character available. */
extern void write_chr_buffer(uint8_t c);

/* Function to read a character from the main buffer,
 * and set character not available. */
extern uint8_t read_chr_buffer();

/* Function to read pending status. */
extern uint8_t read_chr_pending_status();

/* Function write maj touch status. */
extern void write_maj_status(uint8_t b);

/* Function read maj touch status. */
extern uint8_t read_maj_status();

/* Function write ctrl touch status. */
extern void write_ctrl_status(uint8_t b);

/* Function read ctral touch status. */
extern uint8_t read_ctrl_status();

/* Function write alt touch status. */
extern void write_alt_status(uint8_t b);

/* Function read maj touch status. */
extern uint8_t read_alt_status();

/* Function write alt_gr touch status. */
extern void write_alt_gr_status(uint8_t b);

/* Function read alt_gr touch status. */
extern uint8_t read_alt_gr_status();

/* Function write maj_lock touch status. */
extern void write_maj_lock_status(uint8_t b);

/* Function read maj_lock touch status. */
extern uint8_t read_maj_lock_status();

/* Function to read arrow key status. */
extern uint8_t read_arrow_status();

/* Function to write arrow key status. */
extern void write_arrow_status(uint8_t n);

#endif /* __KEYMAP_H__ */
