/* ============================================================
 * colors.c
 * ELEC327 Simon Game — APA102 LED message definitions
 *
 * APA102 protocol (per LED, 32 bits total):
 *   Byte 0: blue intensity
 *   Byte 1: {[7:5]=111b header, [4:0]=brightness}
 *   Byte 2: red intensity
 *   Byte 3: green intensity
 *
 * The struct byte order is already pre-arranged so that casting
 * leds_message_t to uint16_t[] and sending MSb-first over SPI
 * produces a correct APA102 bit stream.  See leds.h for details.
 *
 * Brightness is set to 5 (out of 31) for reasonable coin-cell
 * power consumption.
 * ============================================================ */
#include "colors.h"

/* ---------------------------------------------------------------
 * Helper macros — expand to apa102_led_t designated initializers
 * --------------------------------------------------------------- */
#define LED_OFF    { .blue=0x00, .brightness=0, ._header=7, .red=0x00, .green=0x00 }
#define LED_BLUE   { .blue=0xF0, .brightness=5, ._header=7, .red=0x00, .green=0x00 }
#define LED_RED    { .blue=0x00, .brightness=5, ._header=7, .red=0xF0, .green=0x00 }
#define LED_GREEN  { .blue=0x00, .brightness=5, ._header=7, .red=0x00, .green=0xF0 }
#define LED_YELLOW { .blue=0x00, .brightness=5, ._header=7, .red=0xD0, .green=0xA0 }

#define LED_WHITE  { .blue=0xB0, .brightness=5, ._header=7, .red=0xB0, .green=0xB0 }
#define LED_PURPLE { .blue=0xD0, .brightness=5, ._header=7, .red=0xC0, .green=0x00 }
#define LED_CYAN   { .blue=0xD0, .brightness=5, ._header=7, .red=0x00, .green=0xD0 }
#define LED_ORANGE { .blue=0x00, .brightness=5, ._header=7, .red=0xF0, .green=0x60 }

/* SPI start/end frames for APA102 */
#define SPI_START  { 0x0000, 0x0000 }
#define SPI_END    { 0xFFFF, 0xFFFF }

/* ===========================================================
 * Basic patterns
 * =========================================================== */

const leds_message_t leds_off = {
    .start = SPI_START,
    .led   = { LED_OFF, LED_OFF, LED_OFF, LED_OFF },
    .end   = SPI_END,
};

const leds_message_t leds_all_on = {
    .start = SPI_START,
    .led   = { LED_BLUE, LED_RED, LED_GREEN, LED_YELLOW },
    .end   = SPI_END,
};

const leds_message_t leds_on = {
    .start = SPI_START,
    .led   = { LED_BLUE, LED_RED, LED_GREEN, LED_YELLOW },
    .end   = SPI_END,
};

/* ===========================================================
 * Single-button LED array (indexed 0–3)
 * =========================================================== */
const leds_message_t led_single[4] = {
    { .start = SPI_START, .led = { LED_BLUE,  LED_OFF, LED_OFF, LED_OFF    }, .end = SPI_END },
    { .start = SPI_START, .led = { LED_OFF,   LED_RED, LED_OFF, LED_OFF    }, .end = SPI_END },
    { .start = SPI_START, .led = { LED_OFF,   LED_OFF, LED_GREEN, LED_OFF  }, .end = SPI_END },
    { .start = SPI_START, .led = { LED_OFF,   LED_OFF, LED_OFF, LED_YELLOW }, .end = SPI_END },
};

/* ===========================================================
 * All six pair combinations
 * =========================================================== */
const leds_message_t leds_pair_01 = {
    .start = SPI_START,
    .led   = { LED_BLUE, LED_RED, LED_OFF, LED_OFF },
    .end   = SPI_END,
};

const leds_message_t leds_pair_02 = {
    .start = SPI_START,
    .led   = { LED_BLUE, LED_OFF, LED_GREEN, LED_OFF },
    .end   = SPI_END,
};

const leds_message_t leds_pair_03 = {
    .start = SPI_START,
    .led   = { LED_BLUE, LED_OFF, LED_OFF, LED_YELLOW },
    .end   = SPI_END,
};

const leds_message_t leds_pair_12 = {
    .start = SPI_START,
    .led   = { LED_OFF, LED_RED, LED_GREEN, LED_OFF },
    .end   = SPI_END,
};

const leds_message_t leds_pair_13 = {
    .start = SPI_START,
    .led   = { LED_OFF, LED_RED, LED_OFF, LED_YELLOW },
    .end   = SPI_END,
};

const leds_message_t leds_pair_23 = {
    .start = SPI_START,
    .led   = { LED_OFF, LED_OFF, LED_GREEN, LED_YELLOW },
    .end   = SPI_END,
};

/* ===========================================================
 * All four triple combinations
 * =========================================================== */
const leds_message_t leds_triple_012 = {
    .start = SPI_START,
    .led   = { LED_BLUE, LED_RED, LED_GREEN, LED_OFF },
    .end   = SPI_END,
};

const leds_message_t leds_triple_013 = {
    .start = SPI_START,
    .led   = { LED_BLUE, LED_RED, LED_OFF, LED_YELLOW },
    .end   = SPI_END,
};

const leds_message_t leds_triple_023 = {
    .start = SPI_START,
    .led   = { LED_BLUE, LED_OFF, LED_GREEN, LED_YELLOW },
    .end   = SPI_END,
};

const leds_message_t leds_triple_123 = {
    .start = SPI_START,
    .led   = { LED_OFF, LED_RED, LED_GREEN, LED_YELLOW },
    .end   = SPI_END,
};

/* ===========================================================
 * Special color patterns (all 4 LEDs, uniform color)
 * =========================================================== */
const leds_message_t leds_all_white = {
    .start = SPI_START,
    .led   = { LED_WHITE, LED_WHITE, LED_WHITE, LED_WHITE },
    .end   = SPI_END,
};

const leds_message_t leds_all_purple = {
    .start = SPI_START,
    .led   = { LED_PURPLE, LED_PURPLE, LED_PURPLE, LED_PURPLE },
    .end   = SPI_END,
};

const leds_message_t leds_all_cyan = {
    .start = SPI_START,
    .led   = { LED_CYAN, LED_CYAN, LED_CYAN, LED_CYAN },
    .end   = SPI_END,
};

const leds_message_t leds_all_orange = {
    .start = SPI_START,
    .led   = { LED_ORANGE, LED_ORANGE, LED_ORANGE, LED_ORANGE },
    .end   = SPI_END,
};

/* ===========================================================
 * Difficulty indicator patterns (indexed by difficulty_t 0–3)
 *   Easy=green, Normal=blue, Hard=orange, Expert=red
 * =========================================================== */
const leds_message_t leds_difficulty[4] = {
    { .start = SPI_START, .led = { LED_GREEN,  LED_GREEN,  LED_GREEN,  LED_GREEN  }, .end = SPI_END },
    { .start = SPI_START, .led = { LED_BLUE,   LED_BLUE,   LED_BLUE,   LED_BLUE   }, .end = SPI_END },
    { .start = SPI_START, .led = { LED_ORANGE, LED_ORANGE, LED_ORANGE, LED_ORANGE }, .end = SPI_END },
    { .start = SPI_START, .led = { LED_RED,    LED_RED,    LED_RED,    LED_RED    }, .end = SPI_END },
};

/* ===========================================================
 * Round progress indicators (cumulative left-to-right fill)
 * Used during MODE_INTER_SEQUENCE to show round completion.
 * =========================================================== */
const leds_message_t leds_progress[4] = {
    { .start = SPI_START, .led = { LED_WHITE, LED_OFF,   LED_OFF,   LED_OFF   }, .end = SPI_END },
    { .start = SPI_START, .led = { LED_WHITE, LED_WHITE, LED_OFF,   LED_OFF   }, .end = SPI_END },
    { .start = SPI_START, .led = { LED_WHITE, LED_WHITE, LED_WHITE, LED_OFF   }, .end = SPI_END },
    { .start = SPI_START, .led = { LED_WHITE, LED_WHITE, LED_WHITE, LED_WHITE }, .end = SPI_END },
};
