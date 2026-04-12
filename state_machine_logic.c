/* ============================================================
 * state_machine_logic.c
 * ELEC327 Simon Game — Full FSM implementation (Advanced)
 *
 * GetNextState() is the heart of the game.  It is called once
 * per TIMG0 tick (every 0.625 ms) by the main loop and returns
 * a completely new state_t.
 *
 * Advanced features implemented here:
 *
 *   1. Difficulty selection — which button starts the game
 *      determines timeout, playback speed, win length, and
 *      whether combo presses appear.
 *
 *   2. Multi-button reset — pressing all 4 buttons at once
 *      returns to boot animation from any mode.
 *
 *   3. Double-button combo elements — on Hard/Expert, later
 *      rounds include elements requiring two simultaneous presses.
 *      A short grace period allows the second button to be added.
 *
 *   4. Creative LED usage — difficulty color flash, round
 *      progress indicator, special colors for new-record and
 *      combo elements.
 *
 *   5. Fastest performance tracking — per-difficulty best times
 *      stored in RAM.  Beating a record triggers a special
 *      celebration animation before the win animation.
 *
 *   6. Music sequencer mode — hold SW1+SW4 during boot animation
 *      to enter a free-play mode where each button plays its
 *      tone/LED.  All 4 buttons exits.
 * ============================================================ */

#include <ti/devices/msp/msp.h>
#include "state_machine_logic.h"
#include "buzzer.h"
#include "buttons.h"
#include "leds.h"
#include "colors.h"
#include "music.h"
#include "random.h"

/* ---------------------------------------------------------------
 * Difficulty configuration table
 *
 * Each row: { timeout, elem_on, elem_gap, win_length, combo_start }
 *   combo_start = 0 means no combo elements ever.
 *   combo_start = N means combos appear starting at round N.
 * --------------------------------------------------------------- */
const difficulty_config_t difficulty_table[4] = {
    [DIFF_EASY]   = { 4800, 1040, 480,  4,  0 },
    [DIFF_NORMAL] = { 2400,  640, 320,  5,  0 },
    [DIFF_HARD]   = { 1600,  480, 240,  7,  5 },
    [DIFF_EXPERT] = { 1200,  320, 160, 10,  4 },
};

/* ---------------------------------------------------------------
 * Per-difficulty best times (0 = no record yet).
 * Persists across games within a single power cycle.
 * --------------------------------------------------------------- */
static uint32_t best_time[4] = { 0, 0, 0, 0 };

/* ---------------------------------------------------------------
 * Button masks and tones (indexed 0–3 matching LEDs/sequence)
 * --------------------------------------------------------------- */
static const uint32_t button_mask[4] = { SW1, SW2, SW3, SW4 };

static const uint16_t button_tones[4] = {
    TONE_BTN_0, TONE_BTN_1, TONE_BTN_2, TONE_BTN_3,
};

/* ---------------------------------------------------------------
 * Combo LED lookup table — maps a 4-bit bitmask to a const
 * leds_message_t pointer.  Bit i set = button i active.
 * --------------------------------------------------------------- */
static const leds_message_t *combo_led_lookup[16] = {
    &leds_off,           /* 0x00 */
    &led_single[0],      /* 0x01 = btn0 */
    &led_single[1],      /* 0x02 = btn1 */
    &leds_pair_01,       /* 0x03 = btn0+1 */
    &led_single[2],      /* 0x04 = btn2 */
    &leds_pair_02,       /* 0x05 = btn0+2 */
    &leds_pair_12,       /* 0x06 = btn1+2 */
    &leds_triple_012,    /* 0x07 = btn0+1+2 */
    &led_single[3],      /* 0x08 = btn3 */
    &leds_pair_03,       /* 0x09 = btn0+3 */
    &leds_pair_13,       /* 0x0A = btn1+3 */
    &leds_triple_013,    /* 0x0B = btn0+1+3 */
    &leds_pair_23,       /* 0x0C = btn2+3 */
    &leds_triple_023,    /* 0x0D = btn0+2+3 */
    &leds_triple_123,    /* 0x0E = btn1+2+3 */
    &leds_all_on,        /* 0x0F = all */
};

/* ---------------------------------------------------------------
 * Helper: get the buzzer tone for a sequence element bitmask.
 * Single-button → that button's tone.
 * Multi-button  → TONE_CHORD (distinct from any single tone).
 * --------------------------------------------------------------- */
static uint16_t GetElementTone(uint8_t elem_mask)
{
    int count = 0;
    int first = -1;
    for (int i = 0; i < 4; i++) {
        if (elem_mask & (1 << i)) {
            if (first < 0) first = i;
            count++;
        }
    }
    if (count <= 1 && first >= 0)
        return button_tones[first];
    return TONE_CHORD;
}

/* ---------------------------------------------------------------
 * Helper: count set bits in a byte
 * --------------------------------------------------------------- */
static int PopCount4(uint8_t mask)
{
    int c = 0;
    for (int i = 0; i < 4; i++)
        if (mask & (1 << i)) c++;
    return c;
}

/* ===============================================================
 * UpdateButton — per-button debounce state machine
 * =============================================================== */
static button_t UpdateButton(button_t btn, uint32_t gpio_input, uint32_t mask)
{
    button_t next = btn;

    if ((gpio_input & mask) == 0) {
        switch (btn.state) {
            case BUTTON_IDLE:
                next.state = BUTTON_BOUNCING;
                next.depressed_counter = 1;
                break;
            case BUTTON_BOUNCING:
                next.depressed_counter++;
                if (next.depressed_counter > BUTTON_BOUNCE_LIMIT)
                    next.state = BUTTON_PRESS;
                break;
            case BUTTON_PRESS:
                break;
        }
    } else {
        next.state = BUTTON_IDLE;
        next.depressed_counter = 0;
    }
    return next;
}

/* ===============================================================
 * InitAnimation — reset a song_state_t to the first frame
 * =============================================================== */
void InitAnimation(song_state_t *anim, const animation_note_t *song, int length)
{
    anim->song         = song;
    anim->song_length  = length;
    anim->index        = 0;
    anim->note_counter = 0;
}

/* ===============================================================
 * AdvanceAnimation — step animation by one tick
 * =============================================================== */
static void AdvanceAnimation(state_t *s)
{
    song_state_t           *a     = &s->anim_state;
    const animation_note_t *frame = &a->song[a->index];

    s->leds   = frame->leds;
    s->buzzer = frame->note;

    a->note_counter++;
    uint32_t frame_ticks = (uint32_t)frame->duration * (uint32_t)SIXTEENTH_NOTE;
    if (a->note_counter >= frame_ticks) {
        a->note_counter = 0;
        a->index = (a->index + 1) % a->song_length;
    }
}

/* ===============================================================
 * GenerateSequence
 *
 * Fills sequence[0..length-1].  Elements before combo_start_round
 * are single-button bitmasks (0x01/0x02/0x04/0x08).  From
 * combo_start_round onward, roughly 1-in-3 elements become
 * two-button combos.
 *
 * current_len: how many elements are already locked in from the
 * previous round (preserves the earlier part of the sequence).
 * Only generates elements from current_len onward.
 * =============================================================== */
void GenerateSequence(uint8_t *sequence, int length, int combo_start_round, int current_len)
{
    for (int i = current_len; i < length; i++) {
        uint8_t r1 = (uint8_t)rand();

        if (combo_start_round > 0 && i >= combo_start_round) {
            uint8_t r2 = (uint8_t)rand();
            if (r2 == 0) {
                /* ~25% chance of a double-button combo */
                uint8_t r3 = (uint8_t)rand();
                uint8_t btn_a = r1 & 0x03;
                uint8_t btn_b = r3 & 0x03;
                if (btn_b == btn_a)
                    btn_b = (btn_a + 1) & 0x03;
                sequence[i] = (1 << btn_a) | (1 << btn_b);
            } else {
                sequence[i] = (1 << (r1 & 0x03));
            }
        } else {
            sequence[i] = (1 << (r1 & 0x03));
        }
    }
}

/* ===============================================================
 * SetBuzzerState — apply buzzer_state_t to TIMA1 hardware
 * =============================================================== */
void SetBuzzerState(buzzer_state_t buzzer)
{
    if (buzzer.sound_on) {
        SetBuzzerPeriod(buzzer.period);
        EnableBuzzer();
    } else {
        DisableBuzzer();
    }
}

/* ===============================================================
 * GetNextState — the FSM core
 * =============================================================== */
state_t GetNextState(state_t current, uint32_t gpio_input)
{
    state_t new = current;

    /* ----- Advance tick counters ----- */
    new.mode_counter++;
    new.timeout_counter++;

    /* Accumulate game time during active play modes */
    if (current.game_mode == MODE_SHOW_SEQUENCE ||
        current.game_mode == MODE_PLAYER_INPUT  ||
        current.game_mode == MODE_INTER_SEQUENCE) {
        new.game_ticks++;
    }

    /* ----- Debounce all four buttons ----- */
    bool just_pressed[4]  = {false, false, false, false};
    bool just_released[4] = {false, false, false, false};
    bool is_held[4]       = {false, false, false, false};

    for (int i = 0; i < 4; i++) {
        button_state_t prev = current.buttons[i].state;
        new.buttons[i]      = UpdateButton(current.buttons[i], gpio_input, button_mask[i]);
        button_state_t curr = new.buttons[i].state;

        just_pressed[i]  = (prev != BUTTON_PRESS && curr == BUTTON_PRESS);
        just_released[i] = (prev == BUTTON_PRESS && curr != BUTTON_PRESS);
        is_held[i]       = (curr == BUTTON_PRESS);
    }

    bool any_just_pressed = just_pressed[0] || just_pressed[1] ||
                            just_pressed[2] || just_pressed[3];

    /* Build current held-buttons bitmask */
    uint8_t held_mask = 0;
    for (int i = 0; i < 4; i++)
        if (is_held[i]) held_mask |= (1 << i);

    /* ============================================================
     * GLOBAL RESET: all 4 buttons pressed simultaneously → reboot
     * Works in every mode except boot animation itself.
     * ============================================================ */
    if (current.game_mode != MODE_BOOT_ANIM && held_mask == 0x0F) {
        new.game_mode      = MODE_BOOT_ANIM;
        new.mode_counter   = 0;
        new.active_buttons = 0;
        new.chord_timer    = 0;
        new.leds           = &leds_off;
        new.buzzer         = (buzzer_state_t){ .period = 0, .sound_on = false };
        InitAnimation(&new.anim_state, boot_animation, boot_animation_length);
        return new;
    }

    /* ----- Default outputs: off ----- */
    new.leds   = &leds_off;
    new.buzzer = (buzzer_state_t){ .period = 0, .sound_on = false };

    /* ----- Difficulty config shorthand ----- */
    const difficulty_config_t *dc = &difficulty_table[current.difficulty];

    /* ============================================================
     * Mode-specific FSM logic
     * ============================================================ */
    switch (current.game_mode) {

    /* -----------------------------------------------------------
     * MODE_BOOT_ANIM
     *
     * Plays boot_animation[] on loop.
     *
     * Button input uses a 100 ms detection window (SEQ_DETECT_TICKS):
     *   - Single button pressed and window expires → start game
     *     with that button's difficulty level.
     *   - Two buttons pressed within the window → enter sequencer.
     *   - Button released before window expires → immediate start.
     *
     * active_buttons accumulates presses; chord_timer tracks the
     * window duration.
     * ----------------------------------------------------------- */
    case MODE_BOOT_ANIM:
        AdvanceAnimation(&new);

        /* Accumulate any new presses into the detection bitmask */
        for (int i = 0; i < 4; i++) {
            if (just_pressed[i])
                new.active_buttons |= (1 << i);
        }

        if (new.active_buttons != 0 && current.active_buttons == 0) {
            new.chord_timer = 0;
        }

        if (current.active_buttons != 0) {
            new.chord_timer++;

            /* Two+ unique buttons detected → sequencer mode */
            if (PopCount4(new.active_buttons) >= 2) {
                new.game_mode      = MODE_SEQUENCER;
                new.mode_counter   = 0;
                new.active_buttons = 0;
                new.chord_timer    = 0;
                new.leds           = &leds_off;
                new.buzzer         = (buzzer_state_t){ .sound_on = false };
                break;
            }

            /* Detection window expired with single button → start game */
            if (new.chord_timer >= SEQ_DETECT_TICKS) {
                for (int i = 0; i < 4; i++) {
                    if (new.active_buttons & (1 << i)) {
                        new.difficulty = (difficulty_t)i;
                        break;
                    }
                }
                new.active_buttons = 0;
                new.chord_timer    = 0;
                new.game_mode      = MODE_DIFFICULTY_FLASH;
                new.mode_counter   = 0;
                new.leds           = &leds_off;
                new.buzzer         = (buzzer_state_t){ .sound_on = false };
            }
        }

        /* Button released before window expires → immediate game start */
        if (held_mask == 0 && current.active_buttons != 0 &&
            PopCount4(current.active_buttons) == 1) {
            for (int i = 0; i < 4; i++) {
                if (current.active_buttons & (1 << i)) {
                    new.difficulty = (difficulty_t)i;
                    break;
                }
            }
            new.active_buttons = 0;
            new.chord_timer    = 0;
            new.game_mode      = MODE_DIFFICULTY_FLASH;
            new.mode_counter   = 0;
            new.leds           = &leds_off;
            new.buzzer         = (buzzer_state_t){ .sound_on = false };
        }
        break;

    /* -----------------------------------------------------------
     * MODE_DIFFICULTY_FLASH
     *
     * Briefly displays the selected difficulty's color on all LEDs
     * so the player gets visual confirmation of their choice.
     * ----------------------------------------------------------- */
    case MODE_DIFFICULTY_FLASH:
        new.leds = &leds_difficulty[current.difficulty];
        new.buzzer.period   = button_tones[current.difficulty];
        new.buzzer.sound_on = true;

        if (new.mode_counter >= DIFF_FLASH_TICKS) {
            new.game_mode    = MODE_PRE_GAME;
            new.mode_counter = 0;
        }
        break;

    /* -----------------------------------------------------------
     * MODE_PRE_GAME
     *
     * Silent pause before the first sequence.  Generates the
     * full random sequence for this game.
     * ----------------------------------------------------------- */
    case MODE_PRE_GAME:
        if (new.mode_counter >= PRE_GAME_TICKS) {
            GenerateSequence(new.sequence, dc->win_length,
                             dc->combo_start_round, 0);

            new.seq_len      = 1;
            new.show_idx     = 0;
            new.elem_active  = true;
            new.mode_counter = 0;
            new.game_ticks   = 0;
            new.game_mode    = MODE_SHOW_SEQUENCE;
        }
        break;

    /* -----------------------------------------------------------
     * MODE_SHOW_SEQUENCE
     *
     * Plays sequence[0..seq_len-1] to the player.  Each element:
     *   LED(s) on + tone for elem_on_ticks
     *   all off for elem_gap_ticks
     *
     * Element bitmask selects which LEDs and which tone via the
     * combo lookup table.
     * ----------------------------------------------------------- */
    case MODE_SHOW_SEQUENCE:
        if (current.elem_active) {
            uint8_t elem = current.sequence[current.show_idx];
            new.leds            = combo_led_lookup[elem & 0x0F];
            new.buzzer.period   = GetElementTone(elem);
            new.buzzer.sound_on = true;

            if (new.mode_counter >= dc->elem_on_ticks) {
                new.elem_active  = false;
                new.mode_counter = 0;
                new.show_idx++;

                if (new.show_idx >= current.seq_len) {
                    new.game_mode       = MODE_PLAYER_INPUT;
                    new.input_idx       = 0;
                    new.active_buttons  = 0;
                    new.chord_locked    = false;
                    new.chord_timer     = 0;
                    new.timeout_counter = 0;
                }
            }
        } else {
            if (new.mode_counter >= dc->elem_gap_ticks) {
                new.elem_active  = true;
                new.mode_counter = 0;
            }
        }
        break;

    /* -----------------------------------------------------------
     * MODE_PLAYER_INPUT
     *
     * Player reproduces the sequence.  Supports both single and
     * multi-button elements.
     *
     * For multi-button (combo) elements, a CHORD_GRACE_TICKS
     * window after the first press allows the second button to
     * be added before validation occurs.
     * ----------------------------------------------------------- */
    case MODE_PLAYER_INPUT: {

        uint8_t expected = current.sequence[current.input_idx];
        int expected_count = PopCount4(expected);

        /* ---- Timeout (only when no button is held) ---- */
        if (current.active_buttons == 0 &&
            new.timeout_counter >= dc->timeout_ticks) {
            new.game_mode    = MODE_LOSE_ANIM;
            new.mode_counter = 0;
            InitAnimation(&new.anim_state, lose_animation, lose_animation_length);
            break;
        }

        if (current.active_buttons != 0) {
            /* ========== Buttons are being held ========== */

            /* Update active_buttons with any newly pressed buttons */
            for (int i = 0; i < 4; i++) {
                if (just_pressed[i])
                    new.active_buttons |= (1 << i);
            }

            /* LED + tone feedback for whatever is currently held */
            new.leds            = combo_led_lookup[new.active_buttons & 0x0F];
            new.buzzer.period   = GetElementTone(new.active_buttons);
            new.buzzer.sound_on = true;

            /* Advance chord timer */
            new.chord_timer++;

            /* Check if chord is fully locked in */
            if (!current.chord_locked) {
                if (expected_count <= 1) {
                    /* Single-button element: lock immediately */
                    new.chord_locked = true;
                } else if (PopCount4(new.active_buttons) >= expected_count) {
                    /* All required combo buttons are pressed */
                    new.chord_locked = true;
                } else if (new.chord_timer >= CHORD_GRACE_TICKS) {
                    /* Grace period expired — validate with what we have */
                    new.chord_locked = true;
                }
            }

            /* Validate once locked: any wrong button → LOSE */
            if (new.chord_locked && !current.chord_locked) {
                if (new.active_buttons != expected) {
                    new.game_mode    = MODE_LOSE_ANIM;
                    new.mode_counter = 0;
                    InitAnimation(&new.anim_state, lose_animation, lose_animation_length);
                    break;
                }
            }

            /* Check for releases */
            bool any_still_held = false;
            for (int i = 0; i < 4; i++) {
                if (new.active_buttons & (1 << i)) {
                    if (just_released[i])
                        new.active_buttons &= ~(1 << i);
                    if (new.active_buttons & (1 << i))
                        any_still_held = true;
                }
            }

            if (!any_still_held && current.chord_locked) {
                /* All buttons released after a locked chord */
                new.timeout_counter = 0;
                new.input_idx       = current.input_idx + 1;
                new.chord_locked    = false;
                new.chord_timer     = 0;

                if (new.input_idx >= current.seq_len) {
                    if (current.seq_len >= dc->win_length) {
                        /* ======== PLAYER WINS ======== */
                        uint32_t bt = best_time[current.difficulty];
                        if (bt == 0 || new.game_ticks < bt) {
                            best_time[current.difficulty] = new.game_ticks;
                            new.new_record   = true;
                            new.game_mode    = MODE_NEW_RECORD_ANIM;
                            new.mode_counter = 0;
                            InitAnimation(&new.anim_state, record_animation, record_animation_length);
                        } else {
                            new.new_record   = false;
                            new.game_mode    = MODE_WIN_ANIM;
                            new.mode_counter = 0;
                            InitAnimation(&new.anim_state, win_animation, win_animation_length);
                        }
                    } else {
                        new.game_mode    = MODE_INTER_SEQUENCE;
                        new.mode_counter = 0;
                    }
                }
            }
        } else {
            /* ========== No button held — waiting for press ========== */
            for (int i = 0; i < 4; i++) {
                if (!just_pressed[i]) continue;

                /* First button of a new chord */
                new.active_buttons = (1 << i);
                new.chord_locked   = false;
                new.chord_timer    = 0;

                /* For single-button elements, validate immediately */
                if (expected_count <= 1) {
                    new.chord_locked = true;
                    if ((1 << i) != expected) {
                        new.game_mode    = MODE_LOSE_ANIM;
                        new.mode_counter = 0;
                        InitAnimation(&new.anim_state, lose_animation, lose_animation_length);
                    }
                }
                break;
            }
        }
        break;
    }

    /* -----------------------------------------------------------
     * MODE_INTER_SEQUENCE
     *
     * Pause between rounds.  Shows a round progress indicator
     * using white LEDs filling left-to-right based on how many
     * rounds the player has completed.
     * ----------------------------------------------------------- */
    case MODE_INTER_SEQUENCE: {
        /* Show round progress: how many rounds completed (clamped to 0-3) */
        int progress_idx = current.seq_len - 1;
        if (progress_idx < 0) progress_idx = 0;
        if (progress_idx > 3) progress_idx = 3;

        /* Pulse on/off: on for first 2/3, off for last 1/3 of the pause */
        if (new.mode_counter < (INTER_SEQ_TICKS * 2 / 3)) {
            new.leds = &leds_progress[progress_idx];
        }

        if (new.mode_counter >= INTER_SEQ_TICKS) {
            new.seq_len++;
            new.show_idx     = 0;
            new.elem_active  = true;
            new.mode_counter = 0;
            new.game_mode    = MODE_SHOW_SEQUENCE;
        }
        break;
    }

    /* -----------------------------------------------------------
     * MODE_NEW_RECORD_ANIM
     *
     * Special celebration for beating the per-difficulty best time.
     * Plays the record animation for RECORD_ANIM_TICKS, then
     * transitions to the normal win animation.
     * ----------------------------------------------------------- */
    case MODE_NEW_RECORD_ANIM:
        AdvanceAnimation(&new);

        if (new.mode_counter >= RECORD_ANIM_TICKS) {
            new.game_mode    = MODE_WIN_ANIM;
            new.mode_counter = 0;
            InitAnimation(&new.anim_state, win_animation, win_animation_length);
        }

        if (any_just_pressed) {
            new.leds           = &leds_off;
            new.buzzer         = (buzzer_state_t){ .sound_on = false };
            new.game_mode      = MODE_BOOT_ANIM;
            new.mode_counter   = 0;
            new.active_buttons = 0;
            new.chord_timer    = 0;
            InitAnimation(&new.anim_state, boot_animation, boot_animation_length);
        }
        break;

    /* -----------------------------------------------------------
     * MODE_WIN_ANIM
     * ----------------------------------------------------------- */
    case MODE_WIN_ANIM:
        AdvanceAnimation(&new);

        if (any_just_pressed) {
            new.leds           = &leds_off;
            new.buzzer         = (buzzer_state_t){ .sound_on = false };
            new.game_mode      = MODE_BOOT_ANIM;
            new.mode_counter   = 0;
            new.active_buttons = 0;
            new.chord_timer    = 0;
            InitAnimation(&new.anim_state, boot_animation, boot_animation_length);
        }
        break;

    /* -----------------------------------------------------------
     * MODE_LOSE_ANIM
     * ----------------------------------------------------------- */
    case MODE_LOSE_ANIM:
        AdvanceAnimation(&new);

        if (any_just_pressed) {
            new.leds           = &leds_off;
            new.buzzer         = (buzzer_state_t){ .sound_on = false };
            new.game_mode      = MODE_BOOT_ANIM;
            new.mode_counter   = 0;
            new.active_buttons = 0;
            new.chord_timer    = 0;
            InitAnimation(&new.anim_state, boot_animation, boot_animation_length);
        }
        break;

    /* -----------------------------------------------------------
     * MODE_SEQUENCER
     *
     * Free-play music mode.  Each button plays its tone and lights
     * its LED while held.  Multiple buttons can be held at once
     * (plays the tone of the lowest-index held button).
     * Pressing all 4 buttons exits to boot animation (handled by
     * the global reset check above).  Single press of any button
     * just plays it.
     * ----------------------------------------------------------- */
    case MODE_SEQUENCER:
        if (held_mask != 0) {
            new.leds            = combo_led_lookup[held_mask & 0x0F];
            new.buzzer.period   = GetElementTone(held_mask);
            new.buzzer.sound_on = true;
        }
        break;

    /* -----------------------------------------------------------
     * Safety net
     * ----------------------------------------------------------- */
    default:
        new.game_mode      = MODE_BOOT_ANIM;
        new.mode_counter   = 0;
        new.active_buttons = 0;
        new.chord_timer    = 0;
        InitAnimation(&new.anim_state, boot_animation, boot_animation_length);
        break;
    }

    return new;
}
