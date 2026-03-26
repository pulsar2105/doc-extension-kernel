#include "keyboard.h"

#include "stdint.h"

/* Variable that store the text buffer*/
/* Variable that store the text buffer*/
volatile char keyboard_input_chr = 'A';

/* modifier keys */
volatile uint8_t keyboard_input_maj = 0;
volatile uint8_t keyboard_input_ctrl = 0;
volatile uint8_t keyboard_input_alt = 0;
volatile uint8_t keyboard_input_alt_gr = 0;
volatile uint8_t keyboard_input_maj_lock = 0;

/* arrow keys
 * up = 1
 * right = 2
 * down = 3
 * left = 4
 */
volatile uint8_t keyboard_input_arrow = 0;

volatile uint8_t pending_character = 0;

/* Some key are */
volatile uint8_t key_to_ascii_std[126] = {
    0x1B, 0,    '&',   0xE9, '"', '\'',  '(', '-', 0xE8, '_', 0xE7, 0xE0, ')',
    '=',  127,  ENTER, 'a',  'z', 'e',   'r', 't', 'y',  'u', 'i',  'o',  'p',
    0,    '$',  '\r',  0,    'q', 's',   'd', 'f', 'g',  'h', 'j',  'k',  'l',
    'm',  0xF9, 0xB2,  0,    '*', 'w',   'x', 'c', 'v',  'b', 'n',  ',',  ';',
    ':',  '!',  0,     '*',  0,   ' ',   0,   0,   0,    0,   0,    0,    0,
    0,    0,    0,     0,    0,   0,     '7', '8', '9',  '-', '4',  '5',  '6',
    '+',  '1',  '2',   '3',  0,   '.',   0,   0,   '<',  0,   0,    0,    0,
    0,    0,    0,     0,    0,   ENTER, 0,   '/', 0,    0,   0,    0,    0,
    0,    0,    0,     0,    0,   0,     0,   0,   0,    0,   0,    0,    0,
    0,    0,    0,     0,    0,   0,     0,   0,   SUPER};
volatile uint8_t key_to_ascii_maj[126] = {
    0x1B,  0,   '1', '2',   '3',  '4',  '5', '6',  '7', '8',      '9', '0',
    0xB0,  '+', 127, ENTER, 'A',  'Z',  'E', 'R',  'T', 'Y',      'U', 'I',
    'O',   'P', 0,   0xA3,  '\r', 0,    'Q', 'S',  'D', 'F',      'G', 'H',
    'J',   'K', 'L', 'M',   0x25, 0x7E, 0,   0xB5, 'W', 'X',      'C', 'V',
    'B',   'N', '?', '.',   '/',  0xA7, 0,   '*',  0,   ' ',      0,   0,
    0,     0,   0,   0,     0,    0,    0,   0,    0,   VERR_NUM, 0,   '7',
    '8',   '9', '-', '4',   '5',  '6',  '+', '1',  '2', '3',      0,   '.',
    0,     0,   '>', 0,     0,    0,    0,   0,    0,   0,        0,   0,
    ENTER, 0,   '/', 0,     0,    0,    0,   0,    0,   0,        0,   0,
    0,     0,   0,   0,     0,    0,    0,   0,    0,   0,        0,   0,
    0,     0,   0,   0,     0,    SUPER};
volatile uint8_t key_to_ascii_alt_gr[126] = {
    0x1B,  0,    0xB9, 0x7E,  '#',  '{',  '[',  '|',  '`',  '\\', '^', '@',
    ']',   '}',  127,  ENTER, 0xE6, 0xAB, 0x80, 0xB6, 0,    0,    0,   0,
    0xD8,  0xDE, 0,    0xA4,  '\r', 0,    '@',  0xDF, 0xD0, 0,    0,   0,
    0,     0,    0,    0xB5,  0,    0xAC, 0,    '*',  'w',  'x',  'c', 'v',
    'b',   'n',  ',',  ';',   ':',  '!',  0,    '*',  0,    ' ',  0,   0,
    0,     0,    0,    0,     0,    0,    0,    0,    0,    0,    0,   '7',
    '8',   '9',  '-',  '4',   '5',  '6',  '+',  '1',  '2',  '3',  0,   '.',
    0,     0,    '<',  0,     0,    0,    0,    0,    0,    0,    0,   0,
    ENTER, 0,    '/',  0,     0,    0,    0,    0,    0,    0,    0,   0,
    0,     0,    0,    0,     0,    0,    0,    0,    0,    0,    0,   0,
    0,     0,    0,    0,     0,    SUPER};

/* Function to convert AZERTY to UTF8 */
extern uint8_t azerty_to_utf8(uint8_t maj, uint8_t alt_gr, uint8_t key_code) {
    if (maj) {
        return key_to_ascii_maj[key_code];
    } else if (alt_gr) {
        return key_to_ascii_alt_gr[key_code];
    } else {
        return key_to_ascii_std[key_code];
    }
}

/* Function to write a character into the main buffer,
 * and set character available. */
void write_chr_buffer(uint8_t c) {
    keyboard_input_chr = c;
    pending_character = 1;
}

/* Function to read a character from the main buffer,
 * and set character not available. */
uint8_t read_chr_buffer() {
    if (pending_character) {
        pending_character = 0;
        return keyboard_input_chr;
    } else {
        return 0; // no character available
    }
}

/* Function to read pending status. */
uint8_t read_chr_pending_status() { return pending_character; }

/* Function set maj touch status. */
void write_maj_status(uint8_t b) { keyboard_input_maj = b; }

/* Function read maj touch status. */
uint8_t read_maj_status() { return keyboard_input_maj; }

/* Function write ctrl touch status. */
void write_ctrl_status(uint8_t b) { keyboard_input_ctrl = b; }

/* Function read ctral touch status. */
uint8_t read_ctrl_status() { return keyboard_input_ctrl; }

/* Function write alt touch status. */
void write_alt_status(uint8_t b) { keyboard_input_alt = b; }

/* Function read maj touch status. */
uint8_t read_alt_status() { return keyboard_input_alt; }

/* Function write alt_gr touch status. */
void write_alt_gr_status(uint8_t b) { keyboard_input_alt_gr = b; }

/* Function read alt_gr touch status. */
uint8_t read_alt_gr_status() { return keyboard_input_alt_gr; }

/* Function write maj_lock touch status. */
void write_maj_lock_status(uint8_t b) { keyboard_input_maj_lock = b; }

/* Function read maj_lock touch status. */
uint8_t read_maj_lock_status() { return keyboard_input_maj_lock; }

/* Function to read arrow key status. */
uint8_t read_arrow_status() { return keyboard_input_arrow; }

/* Function to write arrow key status. */
void write_arrow_status(uint8_t n) { keyboard_input_arrow = n; }
