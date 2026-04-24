/* ============================================================
 * music.c
 * ELEC327 Simon Game — Animation data arrays
 *
 * Each animation is a const array of animation_note_t frames.
 * Each frame specifies:
 *   .note      – buzzer_state_t {period, sound_on}
 *   .leds      – pointer to an leds_message_t (from colors.c)
 *   .duration  – how many SIXTEENTH_NOTE (200-tick, 125 ms) units
 *                to display this frame before advancing
 *
 * AdvanceAnimation() in state_machine_logic.c steps through these
 * arrays and wraps back to frame 0 after the last frame, creating
 * a seamless loop.
 *
 * The four animations are deliberately distinct:
 *
 *   boot_animation   : rotating LED chase + all-flash.       ~2 s cycle.
 *   win_animation    : diagonal pairs, ascending tones.      ~3.5 s cycle.
 *   lose_animation   : adjacent pairs, low tones.            ~2 s cycle.
 *   record_animation : white/purple/cyan flash, high tones.  ~1.5 s cycle.
 * ============================================================ */
#include "music.h"
#include "state_machine_logic.h"
#include "colors.h"

/* ===========================================================
 * boot_animation
 * Rotating LED chase (Blue→Red→Green→Yellow), each lit with its
 * own tone, followed by two full-flash pulses.
 * Duration: 8 frames × 2 × 125 ms = 2.0 s per cycle.
 * =========================================================== */
const animation_note_t boot_animation[] = {
    { .note = {TONE_BTN_0, true},  .leds = &led_single[0], .duration = 2 },
    { .note = {TONE_BTN_1, true},  .leds = &led_single[1], .duration = 2 },
    { .note = {TONE_BTN_2, true},  .leds = &led_single[2], .duration = 2 },
    { .note = {TONE_BTN_3, true},  .leds = &led_single[3], .duration = 2 },
    { .note = {TONE_MID,   true},  .leds = &leds_all_on,   .duration = 2 },
    { .note = {0,          false}, .leds = &leds_off,       .duration = 2 },
    { .note = {TONE_MID,   true},  .leds = &leds_all_on,   .duration = 2 },
    { .note = {0,          false}, .leds = &leds_off,       .duration = 2 },
};
const int boot_animation_length = sizeof(boot_animation) / sizeof(animation_note_t);


/* ===========================================================
 * win_animation
 * Diagonal LED pairs (0+2 and 1+3) alternate with ascending
 * tones, culminating in an all-LED flash at the highest note.
 * Duration: (3+3+3+3+4+2) × 125 ms ≈ 2.25 s per cycle.
 * =========================================================== */
const animation_note_t win_animation[] = {
    { .note = {TONE_BTN_2, true},  .leds = &leds_pair_02,  .duration = 3 },
    { .note = {TONE_BTN_3, true},  .leds = &leds_pair_13,  .duration = 3 },
    { .note = {TONE_BTN_2, true},  .leds = &leds_pair_02,  .duration = 3 },
    { .note = {TONE_HIGH,  true},  .leds = &leds_pair_13,  .duration = 3 },
    { .note = {TONE_HIGH,  true},  .leds = &leds_all_on,   .duration = 4 },
    { .note = {0,          false}, .leds = &leds_off,       .duration = 2 },
};
const int win_animation_length = sizeof(win_animation) / sizeof(animation_note_t);


/* ===========================================================
 * lose_animation
 * Adjacent LED pairs (0+1 and 2+3) alternate with low
 * descending tones — a somber "game over" feel.
 * Duration: (6+6+4) × 125 ms = 2.0 s per cycle.
 * =========================================================== */
const animation_note_t lose_animation[] = {
    { .note = {TONE_LOW_A, true},  .leds = &leds_pair_01,  .duration = 6 },
    { .note = {TONE_LOW_E, true},  .leds = &leds_pair_23,  .duration = 6 },
    { .note = {0,          false}, .leds = &leds_off,       .duration = 4 },
};
const int lose_animation_length = sizeof(lose_animation) / sizeof(animation_note_t);


/* ===========================================================
 * record_animation
 * Rapid white → purple → cyan flash with ascending celebration
 * tones (F6 → A6 → D7).  Plays once through before the FSM
 * transitions to the normal win animation.
 * Duration: (2+2+2+2+2+2) × 125 ms = 1.5 s per cycle.
 * =========================================================== */
const animation_note_t record_animation[] = {
    { .note = {TONE_REC_1, true},  .leds = &leds_all_white,  .duration = 2 },
    { .note = {TONE_REC_2, true},  .leds = &leds_all_purple, .duration = 2 },
    { .note = {TONE_REC_3, true},  .leds = &leds_all_cyan,   .duration = 2 },
    { .note = {TONE_REC_1, true},  .leds = &leds_all_purple, .duration = 2 },
    { .note = {TONE_REC_2, true},  .leds = &leds_all_white,  .duration = 2 },
    { .note = {TONE_REC_3, true},  .leds = &leds_all_cyan,   .duration = 2 },
};
const int record_animation_length = sizeof(record_animation) / sizeof(animation_note_t);
