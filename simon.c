/* ============================================================
 * simon.c
 * ELEC327 Simon Game — Entry point
 *
 * Responsibilities:
 *   1. Initialize all hardware peripherals
 *   2. Seed the LFSR RNG from the True Random Number Generator
 *      so each power cycle produces a different sequence
 *   3. Initialize the FSM state (MODE_BOOT_ANIM)
 *   4. Run the main timer-driven loop:
 *        a. Apply buzzer state to hardware
 *        b. Send LED state over SPI
 *        c. Sample GPIO buttons
 *        d. Advance FSM
 *        e. Sleep until next tick
 *
 * Copyright (c) 2026, Caleb Kemere / Rice University ECE
 * All rights reserved, see LICENSE.md
 * ============================================================ */

#include <ti/devices/msp/msp.h>
#include "delay.h"
#include "buttons.h"
#include "timing.h"
#include "buzzer.h"
#include "leds.h"
#include "colors.h"
#include "music.h"
#include "random.h"
#include "state_machine_logic.h"

/* ---------------------------------------------------------------
 * Helper: read-modify-write a 32-bit MMIO register.
 * --------------------------------------------------------------- */
static __STATIC_INLINE void update_reg(volatile uint32_t *reg,
                                        uint32_t           val,
                                        uint32_t           mask)
{
    uint32_t tmp = *reg;
    tmp  &= ~mask;
    *reg  = tmp | (val & mask);
}

/* ---------------------------------------------------------------
 * InitializeTRNGSeed
 *
 * Turns on the TRNG peripheral, collects one hardware-random word,
 * and seeds the LFSR RNG so each power cycle produces a different
 * sequence.
 * --------------------------------------------------------------- */
static void InitializeTRNGSeed(void)
{
    TRNG->GPRCM.RSTCTL = TRNG_RSTCTL_RESETASSERT_ASSERT |
                          TRNG_RSTCTL_KEY_UNLOCK_W;
    TRNG->GPRCM.PWREN  = TRNG_PWREN_KEY_UNLOCK_W        |
                          TRNG_PWREN_ENABLE_ENABLE;
    delay_cycles(POWER_STARTUP_DELAY);

    TRNG->CLKDIVIDE = (uint32_t)TRNG_CLKDIVIDE_RATIO_DIV_BY_2;

    update_reg(&TRNG->CTL,
               (uint32_t)TRNG_CTL_CMD_NORM_FUNC,
               TRNG_CTL_CMD_MASK);
    while (!((TRNG->CPU_INT.RIS & TRNG_RIS_IRQ_CMD_DONE_MASK) ==
              TRNG_RIS_IRQ_CMD_DONE_SET));
    TRNG->CPU_INT.ICLR = TRNG_IMASK_IRQ_CMD_DONE_MASK;

    update_reg(&TRNG->CTL,
               ((uint32_t)0x3 << TRNG_CTL_DECIM_RATE_OFS),
               TRNG_CTL_DECIM_RATE_MASK);

    while (!((TRNG->CPU_INT.RIS & TRNG_RIS_IRQ_CAPTURED_RDY_MASK) ==
              TRNG_RIS_IRQ_CAPTURED_RDY_SET));
    TRNG->CPU_INT.ICLR = TRNG_IMASK_IRQ_CAPTURED_RDY_MASK;

    uint32_t raw  = TRNG->DATA_CAPTURE;
    uint16_t seed = (uint16_t)(raw & 0xFFFFu);
    if (seed == 0u) seed = 0xACE1u;
    srand(seed);

    TRNG->GPRCM.PWREN = TRNG_PWREN_KEY_UNLOCK_W | TRNG_PWREN_ENABLE_DISABLE;
}

/* ---------------------------------------------------------------
 * main
 * --------------------------------------------------------------- */
int main(void)
{
    /* 1. Hardware initialization */
    InitializeButtonGPIO();
    InitializeBuzzer();
    InitializeLEDInterface();
    InitializeTimerG0();
    DisableBuzzer();

    /* 2. Seed the RNG from TRNG */
    InitializeTRNGSeed();

    /* 3. Initialize the FSM state */
    state_t state;

    for (int i = 0; i < 4; i++) {
        state.buttons[i].state             = BUTTON_IDLE;
        state.buttons[i].depressed_counter = 0;
    }

    state.buzzer = (buzzer_state_t){ .period = 0, .sound_on = false };
    state.leds   = &leds_off;

    state.game_mode    = MODE_BOOT_ANIM;
    state.mode_counter = 0;
    state.difficulty   = DIFF_NORMAL;

    state.seq_len        = 0;
    state.show_idx       = 0;
    state.elem_active    = false;
    state.input_idx      = 0;
    state.active_buttons = 0;
    state.chord_locked   = false;
    state.chord_timer    = 0;
    state.timeout_counter = 0;

    state.game_ticks  = 0;
    state.new_record  = false;

    for (int i = 0; i < MAX_SEQUENCE_LENGTH; i++) {
        state.sequence[i] = 0;
    }

    InitAnimation(&state.anim_state, boot_animation, boot_animation_length);

    /* 4. Start the periodic FSM timer */
    const int message_len = (int)(sizeof(leds_message_t) / sizeof(uint16_t));
    SetTimerG0Delay(PERIOD);
    EnableTimerG0();

    /* 5. Main loop */
    while (1) {
        if (timer_wakeup) {

            SetBuzzerState(state.buzzer);

            while (!SendSPIMessage((uint16_t *)state.leds, message_len)) {
                /* busy-wait for previous SPI frame to finish */
            }

            uint32_t gpio_input = GPIOA->DIN31_0 & (SW1 | SW2 | SW3 | SW4);

            state = GetNextState(state, gpio_input);

            timer_wakeup = false;
            __WFI();
        }
    }
}
