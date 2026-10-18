/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Wayne (NOTE RUSH 37), on Vector37 by M. Geisler (makkuro) */
/* NOTE RUSH 37: the game (in Vector37's ui.c's place; the board layer and the synth engine are Vector37's).
 * Three phases, as in the web version (NOTE RUSH CYBERFUNK):
 *   NOME    the note's name on the screen (DÓ .. SI): play it, any octave
 *   PAUTA   the note on the treble staff (DÓ4 .. DÓ6): play that very key
 *   OUVIDO  the note heard: play it, any octave
 * A right answer adds to the combo (NOME 3, PAUTA 4, OUVIDO 5); three in a row move to the next phase; after OUVIDO
 * a new note is unlocked (centre-out from SI4) and shown. A wrong key, or 6 s without one: game over.
 * Controls: the keys answer (and sound: E.PIANO); title: < > or K1 the mode, a key or PLAY starts; PLAY or a pad
 * plays the OUVIDO note again; PATCH: back to the title. The pads are the combo meter; the record is kept (SETUP's
 * settings record, its spare bytes). */
#if !BOARD_SMK37
#error "NOTE RUSH 37 is for the SMK-37 (Pro / Elite / MKE-P37)"
#endif
enum { B_KNOBBANK, B_FADERBANK, B_LEFT, B_RIGHT, B_OCTDN, B_OCTUP, B_ARP, B_REPEAT, B_SCALE, B_CHORD, B_GLOBE,
       B_BT, B_PATCH, B_PARA, B_FX, B_SEQ, B_PLAY, B_REC };
#define PANEL_KEYS() fm1_in.keys

#define NR_SOUND 20u                               /* E.PIANO (FACTORY 2, its 5th) */
#define NR_NOTES 15u                               /* the white keys DÓ4 .. DÓ6 */
#define NR_START 3u                                /* notes at the start (the web's default) */
#define NR_SFP 3u                                  /* right answers a phase */
#define NR_LIMIT_MS 6000u                          /* the answer's time (the web's default) */
#define NR_TONE_MS 900u                            /* the OUVIDO note's length */
#define NR_OK_MS 300u                              /* a right answer shown */
#define NR_INTRO_MS 1500u                          /* a new note shown */
#define NR_PHASE_MS 1100u                          /* a new phase's banner */
#define NR_OVER_LOCK_MS 900u                       /* game over: keys ignored this long (a late key is no restart) */
#define NR_KEY0 48                                 /* the bottom key: DÓ3 */

enum { GS_TITLE, GS_INTRO, GS_PHASE, GS_ASK, GS_OK, GS_OVER };
enum { PH_NOME, PH_PAUTA, PH_OUVIDO, PH_N };
enum { MODE_ALL, MODE_NOME, MODE_PAUTA, MODE_OUVIDO, MODE_N };
enum { WHY_KEY = 1, WHY_TIME };
static const char *const MODE_NAME[MODE_N] = {"TODAS", "NOME", "PAUTA", "OUVIDO"};
static const char *const PHASE_NAME[PH_N] = {"NOME", "PAUTA", "OUVIDO"};
static const uint8_t PHASE_BONUS[PH_N] = {3, 4, 5};
static const uint8_t ORDER[NR_NOTES] = {6, 7, 5, 8, 4, 9, 3, 10, 2, 11, 1, 12, 0, 13, 14};   /* centre-out from SI4 */
static const uint8_t SEMI[7] = {0, 2, 4, 5, 7, 9, 11};
static const char *const SOLFA[7] = {"D\xD3", "R\xC9", "MI", "F\xC1", "SOL", "L\xC1", "SI"};
static const char *const CHROMA[12] = {"D\xD3", "R\xC9" "b", "R\xC9", "MIb", "MI", "F\xC1", "SOLb", "SOL", "L\xC1" "b",
                                       "L\xC1", "SIb", "SI"};   /* (as the web names them) */

#define D_HEAD 1u                                  /* the screen's strips (gdraw.c) */
#define D_BAR 2u
#define D_BODY 12u
#define D_FOOT 16u
#define D_ALL 31u

static struct {
    uint8_t gs, mode, phase, unlocked, cur, last, streak, intro_n, why, played, newbest, oct;
    uint16_t combo, best, gain;
    uint32_t t0;                                   /* the state entered at */
    uint32_t ask_t;                                /* the answer's clock started at */
    uint32_t limit;                                /* ... and runs this long */
    uint32_t tone_off, tone_due;                   /* the heard note: its note off at; (game over) the right note at */
    uint8_t tone_note, due_note;
    uint64_t keys;
    uint32_t btn_t[FM1_NBTN];
    uint32_t flash_t;                              /* the pads flash (right: green, game over: red) from */
    uint32_t rnd;
    uint8_t dirty;                                 /* everything to draw again (main.c, console.c set it) */
    uint8_t redraw;                                /* the strips to draw again (D_*) */
    char msg[28];
    uint32_t msg_until;
} ui = {.rnd = 0x9E3779B9u};

static void ui_say(const char *a, const char *b)   /* a line over the footer (the board layer: UPDATE MODE ..) */
{
    str_cpy(ui.msg, a, sizeof ui.msg);
    if (b)
        str_cat(ui.msg, b, sizeof ui.msg);
    ui.msg_until = fm1_ms + 2000u;
    ui.redraw |= D_FOOT;
}

static uint32_t rnd(void)
{
    ui.rnd ^= ui.rnd << 13;
    ui.rnd ^= ui.rnd >> 17;
    ui.rnd ^= ui.rnd << 5;
    return ui.rnd;
}
static uint32_t note_midi(uint32_t d) { return 60u + SEMI[d % 7u] + 12u * (d / 7u); }   /* step 0 = DÓ4 (60) */
static uint32_t phase_first(void) { return ui.mode == MODE_ALL ? PH_NOME : ui.mode - 1u; }
static uint32_t new_note(void) { return ORDER[ui.unlocked - ui.intro_n]; }   /* (the intro's note) */

/* ------------------------------------------------------------- sound --- */
static void tone_stop(void)
{
    if (ui.tone_off) {
        ev_push(EV_OFF, ui.tone_note, 0);
        ui.tone_off = 0;
        ui.redraw |= D_BODY;
    }
}
static void tone(uint32_t n)                       /* a note played for the player (not a key) */
{
    tone_stop();
    ui.tone_note = (uint8_t)n;
    ev_push(EV_ON, n, 100);
    ui.tone_off = (fm1_ms + NR_TONE_MS) | 1u;
    ui.redraw |= D_BODY;
}

/* ------------------------------------------------------------- the game --- */
static void enter(uint32_t gs)
{
    ui.gs = (uint8_t)gs;
    ui.t0 = fm1_ms;
    ui.dirty = 1;
}
static void ask(void)
{
    uint32_t n = ui.unlocked, d;
    do
        d = ORDER[rnd() % n];
    while (n > 1u && d == ui.last);
    ui.cur = ui.last = (uint8_t)d;
    enter(GS_ASK);
    ui.ask_t = fm1_ms;
    ui.limit = NR_LIMIT_MS;
    if (ui.phase == PH_OUVIDO) {
        tone(note_midi(d));
        ui.limit += NR_TONE_MS;                    /* (the clock as for the others once the note has sounded) */
    }
}
static void intro(void)
{
    enter(GS_INTRO);
    tone(note_midi(new_note()));
}
static void game_start(void)
{
    ui.rnd ^= fm1_ms * 2654435761u;
    ui.combo = 0;
    ui.streak = 0;
    ui.newbest = 0;
    ui.unlocked = NR_START;
    ui.intro_n = NR_START;                         /* the first notes shown, one by one */
    ui.phase = (uint8_t)phase_first();
    ui.last = 0xFF;
    intro();
}
static void next(void)                             /* after a right answer, or a banner */
{
    if (ui.streak >= NR_SFP) {
        ui.streak = 0;
        if (ui.mode == MODE_ALL && ui.phase + 1u < PH_N) {
            ui.phase++;
            enter(GS_PHASE);
            return;
        }
        ui.phase = (uint8_t)phase_first();
        if (ui.unlocked < NR_NOTES) {              /* all done for these notes: one more */
            ui.unlocked++;
            ui.intro_n = 1;
            intro();
            return;
        }
        if (ui.mode == MODE_ALL) {
            enter(GS_PHASE);
            return;
        }
    }
    ask();
}
static void hit(void)
{
    ui.gain = PHASE_BONUS[ui.phase];
    ui.combo = (uint16_t)(ui.combo + ui.gain);
    ui.streak++;
    ui.flash_t = fm1_ms | 1u;
    enter(GS_OK);
}
static void miss(uint32_t why, uint32_t played)
{
    ui.why = (uint8_t)why;
    ui.played = (uint8_t)played;
    ui.flash_t = fm1_ms | 1u;
    if (ui.combo > ui.best) {
        ui.best = ui.combo;
        ui.newbest = 1;
        settings.rsv[0] = (uint8_t)ui.best;        /* the record: the settings' spare bytes */
        settings.rsv[1] = (uint8_t)(ui.best >> 8);
        settings_save();
    }
    ui.due_note = (uint8_t)note_midi(ui.cur);      /* the right note, heard after the wrong one */
    ui.tone_due = (fm1_ms + 450u) | 1u;
    enter(GS_OVER);
}
static int answer_ok(uint32_t n)
{
    uint32_t t = note_midi(ui.cur);
    return ui.phase == PH_PAUTA ? n == t : n % 12u == t % 12u;
}
static void game_key(uint32_t n)                   /* a key pressed (the keybed, or USB MIDI), MIDI note n */
{
    ui.rnd ^= fm1_ms;
    switch (ui.gs) {
    case GS_TITLE:
        game_start();
        break;
    case GS_ASK:
        if (answer_ok(n))
            hit();
        else
            miss(WHY_KEY, n);
        break;
    case GS_OVER:
        if (fm1_ms - ui.t0 >= NR_OVER_LOCK_MS)
            game_start();
        break;
    default:                                       /* (the intro, a banner, a right answer shown: it just sounds) */
        break;
    }
}
static void game_tick(void)
{
    uint32_t now = fm1_ms;
    if (ui.tone_off && (int32_t)(now - ui.tone_off) >= 0)
        tone_stop();
    if (ui.tone_due && (int32_t)(now - ui.tone_due) >= 0) {
        ui.tone_due = 0;
        tone(ui.due_note);
    }
    if (ui.msg[0] && (int32_t)(now - ui.msg_until) >= 0) {
        ui.msg[0] = 0;
        ui.redraw |= D_FOOT;
    }
    switch (ui.gs) {
    case GS_INTRO:
        if (now - ui.t0 >= NR_INTRO_MS) {
            if (--ui.intro_n)
                intro();
            else if (ui.mode == MODE_ALL && ui.unlocked == NR_START)
                enter(GS_PHASE);                   /* (the game's first phase: its banner too) */
            else
                ask();
        }
        break;
    case GS_PHASE:
        if (now - ui.t0 >= NR_PHASE_MS)
            ask();
        break;
    case GS_ASK:
        if (now - ui.ask_t >= ui.limit)
            miss(WHY_TIME, 0);
        break;
    case GS_OK:
        if (now - ui.t0 >= NR_OK_MS)
            next();
        break;
    case GS_TITLE:
    case GS_OVER: {
        static uint32_t blink;                     /* (the blinking line) */
        if (now / 500u != blink) {
            blink = now / 500u;
            ui.redraw |= D_FOOT;
        }
        break;
    }
    }
}
static void to_title(void)
{
    tone_stop();
    ui.tone_due = 0;
    ev_push(EV_ALLOFF, 0, 0);
    enter(GS_TITLE);
}
static void mode_step(int32_t d)
{
    ui.mode = (uint8_t)((ui.mode + MODE_N + (d < 0 ? -1 : 1)) % MODE_N);
    ui.dirty = 1;
}
static void replay(void)
{
    if (ui.gs == GS_ASK && ui.phase == PH_OUVIDO)
        tone(note_midi(ui.cur));
}

/* ------------------------------------------------------------- input --- */
static void pads_poll(void)
{
    int32_t e;
    while ((e = smk_pad_take()) >= 0)
        if ((uint32_t)e & 255u)
            replay();
}
static void keys_poll(void)
{
    uint64_t now = fm1_in.keys, ch = now ^ ui.keys;
    uint32_t k;
    ui.keys = now;
    if (fm1_in.frames < 100u)                      /* (the first readings settle) */
        return;
    for (k = 0; ch && k < 37u; k++)
        if ((ch >> k) & 1u) {
            uint32_t n = (uint32_t)(NR_KEY0 + (int32_t)k), vel = fm1_in.key_vel[k];
            vel = vel ? (vel > 127u ? 127u : vel) : 100u;
            if ((now >> k) & 1u) {
                if (ui.tone_off && ui.gs != GS_INTRO)
                    tone_stop();                   /* (the player's key over the heard note) */
                ev_push(EV_ON, n, vel);
                game_key(n);
            } else
                ev_push(EV_OFF, n, 0);
        }
}
static void buttons_poll(void)
{
    uint32_t rel, pr = fm1_input_edges(&rel), b;
    for (b = 0; b < FM1_NBTN; b++) {
        if ((rel >> b) & 1u)
            ui.btn_t[b] = 0;
        if (!((pr >> b) & 1u))
            continue;
        ui.btn_t[b] = fm1_ms | 1u;
        if (b == B_PATCH)
            to_title();
        else if (ui.gs == GS_TITLE && (b == B_LEFT || b == B_RIGHT))
            mode_step(b == B_LEFT ? -1 : 1);
        else if (b == B_PLAY) {
            if (ui.gs == GS_TITLE || (ui.gs == GS_OVER && fm1_ms - ui.t0 >= NR_OVER_LOCK_MS))
                game_start();
            else
                replay();
        }
    }
}
static void knobs_poll(void)
{
    uint32_t k;
    int32_t s;
    for (k = 0; k < 8u; k++)
        if ((s = fm1_enc_take(k)) != 0 && k == 0u && ui.gs == GS_TITLE)
            mode_step(s);
}

/* USB MIDI in: a MIDI keyboard answers as the keys do (any channel, or the one set) */
static void midi_poll(void)
{
    while (mi_r != mi_w) {
        uint32_t pkt, cin, d1, d2;
        RING_PUBLISH();
        pkt = midi_in_q[mi_r % MQ];
        RING_PUBLISH();
        mi_r++;
        cin = pkt & 15u, d1 = (pkt >> 16) & 127u, d2 = (pkt >> 24) & 127u;
        if (settings.midi_in && cin >= 8u && cin <= 14u && ((pkt >> 8) & 15u) != settings.midi_in - 1u)
            continue;
        if (cin == 9u && d2) {
            tone_stop();
            ev_push(EV_ON, d1, d2);
            game_key(d1);
        } else if (cin == 8u || cin == 9u)
            ev_push(EV_OFF, d1, 0);
        else if (cin == 0xBu && d1 == 123u)
            ev_push(EV_ALLOFF, 0, 0);
    }
}

/* ------------------------------------------------------------- LEDs --- */
/* the pads: the combo meter, column by column from the bottom left (each 16 a new colour); a right answer flashes
 * them green, game over red; the title: a wave */
static const uint8_t METER[3][3] = {{0, 240, 255}, {255, 40, 200}, {255, 230, 40}};
static void pad_col(uint32_t pad, const uint8_t *c, uint32_t k)   /* k of 256 */
{
    uint32_t sc = (4u + settings.bright * 12u) * k >> 8;
    padled_set(pad, (uint8_t)(c[0] * sc >> 8), (uint8_t)(c[1] * sc >> 8), (uint8_t)(c[2] * sc >> 8));
}
static void ui_leds(void)
{
    static const uint8_t GREEN[3] = {40, 255, 100}, RED[3] = {255, 20, 40};
    uint32_t i, now = fm1_ms, fl = ui.flash_t ? now - ui.flash_t : 1000000u;
    for (i = 0; i < 16u; i++) {
        uint32_t pad = (i & 1u ? 0u : 8u) + i / 2u;   /* (i: the meter's order) */
        if (ui.gs == GS_TITLE) {
            uint32_t ph = (now / 4u + (pad & 7u) * 40u + (pad >> 3) * 20u) & 255u;
            pad_col(pad, METER[(pad & 7u) >> 2 ? 1 : 0], ph < 128u ? 40u + ph * 2u : 40u + (255u - ph) * 2u);
        } else if (ui.gs == GS_OVER)
            pad_col(pad, RED, fl < 600u ? 256u : (now / 300u) & 1u ? 90u : 30u);
        else if (fl < 160u)
            pad_col(pad, GREEN, 256u);
        else {
            uint32_t lit = ui.combo % 16u, lap = ui.combo / 16u;
            if (i < lit)
                pad_col(pad, METER[lap % 3u], 256u);
            else if (lap)
                pad_col(pad, METER[(lap + 2u) % 3u], 40u);   /* (the last lap, dim) */
            else
                padled_set(pad, 0, 0, 0);
        }
    }
    fm1_led_key(B_LEFT, ui.gs == GS_TITLE);
    fm1_led_key(B_RIGHT, ui.gs == GS_TITLE);
    fm1_led_key(B_PLAY, ((ui.gs == GS_TITLE || ui.gs == GS_OVER) && (now / 500u) & 1u)
                        || (ui.gs == GS_ASK && ui.phase == PH_OUVIDO));
    fm1_led_key(B_PATCH, ui.gs != GS_TITLE);
    fm1_led_key(B_OCTDN, 0);
    fm1_led_key(B_OCTUP, 0);
}

static void preset_load(uint32_t n)                /* the game's sound: factory sound n (the USB console's "preset") */
{
    ev_push(EV_ALLOFF, 0, 0);
    preset_make(n % NFACT, &pt);
}

static void ui_init(void)
{
    preset_load(NR_SOUND);
    ui.best = (uint16_t)(settings.rsv[0] | settings.rsv[1] << 8);
    if (ui.best == 0xFFFFu)
        ui.best = 0;
    enter(GS_TITLE);
}

static void ui_input(void)
{
    pads_poll();
    keys_poll();
    buttons_poll();
    knobs_poll();
    game_tick();
}
