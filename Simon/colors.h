/* ============================================================
 * colors.h
 * ELEC327 Simon Game — LED message declarations
 *
 * Declares every const leds_message_t used by the FSM.
 * Definitions are in colors.c.
 *
 * Button-to-LED color mapping:
 *   Button 0 (SW1) → Blue
 *   Button 1 (SW2) → Red
 *   Button 2 (SW3) → Green
 *   Button 3 (SW4) → Yellow
 * ============================================================ */
#ifndef colors_include
#define colors_include

#include "leds.h"   /* leds_message_t */

/* ---- All LEDs off ---- */
extern const leds_message_t leds_off;

/* ---- All four LEDs on in their button colors ---- */
extern const leds_message_t leds_all_on;

/* ---- Legacy alias for leds_all_on (backward compatibility) ---- */
extern const leds_message_t leds_on;

/* ---- Single-button feedback: led_single[i] lights only LED i ----
 * Used during sequence playback and player input.                   */
extern const leds_message_t led_single[4];

/* ---- All six pair combinations ---- */
extern const leds_message_t leds_pair_01;
extern const leds_message_t leds_pair_02;
extern const leds_message_t leds_pair_03;
extern const leds_message_t leds_pair_12;
extern const leds_message_t leds_pair_13;
extern const leds_message_t leds_pair_23;

/* ---- All four triple combinations ---- */
extern const leds_message_t leds_triple_012;
extern const leds_message_t leds_triple_013;
extern const leds_message_t leds_triple_023;
extern const leds_message_t leds_triple_123;

/* ---- Special color patterns (all 4 LEDs, same color) ---- */
extern const leds_message_t leds_all_white;
extern const leds_message_t leds_all_purple;
extern const leds_message_t leds_all_cyan;
extern const leds_message_t leds_all_orange;

/* ---- Difficulty indicator patterns (all 4 LEDs, themed color) ----
 *   [0] Easy   = green   (calming)
 *   [1] Normal = blue    (standard)
 *   [2] Hard   = orange  (warning)
 *   [3] Expert = red     (danger)                                   */
extern const leds_message_t leds_difficulty[4];

/* ---- Round progress indicators (cumulative left-to-right fill) ----
 *   [0] = LED 0 only
 *   [1] = LED 0+1
 *   [2] = LED 0+1+2
 *   [3] = all four LEDs                                             */
extern const leds_message_t leds_progress[4];

#endif /* colors_include */
