/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) */
/* NOTE RUSH 37 (on Vector37) in the browser: the firmware's engine, UI and screen (the same sources) on the simulated panel
 * (tests/panel.h), compiled to WebAssembly (web/sim/build.sh). The page's AudioWorklet calls sim_render(n): the
 * device clock runs with the audio, the main loop (input, LEDs, screen) every 15 ms of it. */
#define EXPORT __attribute__((used, visibility("default")))
#define MK_VERSION "sim"
#include "../../tests/panel.h"

static float out_l[1024], out_r[1024];

EXPORT void sim_init(void) { panel_init(); }
/* the store (your sounds, the settings): the simulated flash's data area, kept by the page (localStorage) and given
 * back at the next visit: sim_init(), the saved bytes into sim_store(), then sim_restart() (a power cycle: the store
 * kept, read as at boot) */
EXPORT uint8_t *sim_store(void) { return panel_nor + 0x9D000u; }
EXPORT uint32_t sim_store_size(void) { return 0xBB000u - 0x9D000u; }
EXPORT void sim_restart(void) { panel_init(); }
EXPORT uint32_t sim_game(void)   /* (tests) the state << 24, the phase << 16, the notes unlocked << 8, the streak */
{
    return (uint32_t)ui.gs << 24 | (uint32_t)ui.phase << 16 | (uint32_t)ui.unlocked << 8 | ui.streak;
}
EXPORT uint32_t sim_combo(void) { return ui.combo; }
EXPORT uint32_t sim_target(void) { return ui.gs == GS_ASK ? note_midi(ui.cur) : 0u; }   /* (tests) the answer */
EXPORT float *sim_out_l(void) { return out_l; }
EXPORT float *sim_out_r(void) { return out_r; }
EXPORT uint32_t sim_rate(void) { return FS; }
EXPORT void sim_render(uint32_t n) { panel_render(out_l, out_r, n > 1024u ? 1024u : n); }   /* n frames */
EXPORT uint16_t *sim_fb(void) { return fb; }
EXPORT uint32_t sim_draws(void) { return fb_draws; }
EXPORT uint32_t *sim_pads(void) { return pad_rgb; }
EXPORT uint32_t sim_leds(void) { return btn_leds; }
EXPORT void sim_pad(uint32_t pad, uint32_t vel) { panel_pad(pad, vel); }
EXPORT void sim_key(uint32_t k, uint32_t down) { panel_key(k, down, 100); }
EXPORT void sim_button(uint32_t b, uint32_t down) { panel_button(b, down); }
EXPORT void sim_enc(uint32_t e, int32_t steps) { panel_enc(e, steps); }
EXPORT void sim_fader(uint32_t i, uint32_t v) { panel_fader(i, v); }
EXPORT void sim_wheel(uint32_t i, uint32_t raw) { panel_wheel(i, raw); }
EXPORT void sim_midi(uint32_t st, uint32_t d1, uint32_t d2) { panel_midi(st, d1, d2); }
