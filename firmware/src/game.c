/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Wayne (NOTE RUSH 37), on Vector37 by M. Geisler (makkuro) */
/* NOTE RUSH 37: the game (in Vector37's ui.c's place; the board layer and the synth engine are Vector37's).
 * As the web version (NOTE RUSH CYBERFUNK):
 *   FASE 1 MEMORIZE  the new notes shown (name and staff) and heard
 *   FASE 2 NOME      the letter alone (C .. B): play it, any octave (+3)
 *   FASE 3 PAUTA     the staff alone (C4 .. C6): play that very key (+4)
 *   FASE 4 OUVIDO    the note heard: play it, any octave (+5)
 * A right answer: lightning from the key to the combo; N in a row (ACERTOS) move to the next phase; after OUVIDO a
 * new note (centre-out from B4). A wrong key, or the time out (TEMPO; SEM LIMITE: none): game over, a name (AAA) for
 * the local ranking (top 5, in flash).
 * Controls: the keys answer; title: < > or K1 the mode, a key or PLAY starts (that key silent); GLOBE: the settings
 * (TEMPO, SOM, NOTAS INICIAIS, ACERTOS/FASE: < > the row, K1 or OCT- OCT+ the value); PLAY or a pad: the OUVIDO note
 * again; the name: OCT+ OCT- (or K1) the letter, < > the place, PLAY saves, PATCH skips; PATCH: back to the title. */
#if !BOARD_SMK37
#error "NOTE RUSH 37 is for the SMK-37 (Pro / Elite / MKE-P37)"
#endif
enum { B_KNOBBANK, B_FADERBANK, B_LEFT, B_RIGHT, B_OCTDN, B_OCTUP, B_ARP, B_REPEAT, B_SCALE, B_CHORD, B_GLOBE,
       B_BT, B_PATCH, B_PARA, B_FX, B_SEQ, B_PLAY, B_REC };
#define PANEL_KEYS() fm1_in.keys

#define NR_NOTES 15u                               /* the white keys C4 .. C6 */
#define NR_TONE_MS 900u                            /* a note played for the player */
#define NR_OK_MS 340u                              /* a right answer shown (the lightning's length) */
#define NR_INTRO_MS 1500u                          /* a new note shown */
#define NR_OVER_MS 1600u                           /* game over shown, then the name or the ranking */
#define NR_BOARD_LOCK_MS 600u                      /* the ranking: keys ignored this long (a late key is no restart) */
#define NR_START_MS 350u                           /* the start: the pads' sweep before the first note sounds */
#define NR_KEY0 48                                 /* the bottom key: C3 */
#define NR_LB 5u                                   /* the ranking's places */

enum { GS_TITLE, GS_INTRO, GS_ASK, GS_OK, GS_OVER, GS_NAME, GS_BOARD, GS_SETUP };
enum { PH_NOME, PH_PAUTA, PH_OUVIDO, PH_N };
enum { MODE_ALL, MODE_NOME, MODE_PAUTA, MODE_OUVIDO, MODE_N };
enum { WHY_KEY = 1, WHY_TIME };
static const char *const MODE_NAME[MODE_N] = {"TODAS", "NOME", "PAUTA", "OUVIDO"};
static const uint8_t PHASE_BONUS[PH_N] = {3, 4, 5};
static const uint8_t ORDER[NR_NOTES] = {6, 7, 5, 8, 4, 9, 3, 10, 2, 11, 1, 12, 0, 13, 14};   /* centre-out from B4 */
static const uint8_t SEMI[7] = {0, 2, 4, 5, 7, 9, 11};
static const char *const LETTER[7] = {"C", "D", "E", "F", "G", "A", "B"};
static const char *const CHROMA[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};

/* the settings (GLOBE): kept in the settings record (scenes.c): the sound in .preset, the rest in its spare bytes */
enum { SET_TIME, SET_SOUND, SET_START, SET_SFP, SET_N };
#define NR_SET_TAG 0xA7u                           /* rsv[0]: ours (else the defaults) */
static const char *const SET_NAME[SET_N] = {"TEMPO", "SOM", "NOTAS INICIAIS", "ACERTOS/FASE"};
static const char *const TIME_NAME[6] = {"SEM LIMITE", "TRANQUILO 6S", "F\xC1" "CIL 5S", "M\xC9" "DIO 4S",
                                         "DIF\xCD" "CIL 3S", "INSANO 2S"};   /* (the web's) */
static const uint8_t SND[] = {20, 22, 23, 21, 19, 16, 2, 12, 29};   /* factory sounds (presets.c) */
static const char *const SND_NAME[] = {"E.PIANO", "MARIMBA", "MUSIC BOX", "HARPA", "\xD3RG\xC3O", "CORDAS", "SINOS",
                                       "KALIMBA", "FLAUTA"};
#define NSND (sizeof SND / sizeof SND[0])
static const uint8_t SET_MIN[SET_N] = {0, 0, 1, 2}, SET_MAX[SET_N] = {5, NSND - 1u, 8, 12}, SET_DEF[SET_N] = {1, 0, 3, 3};

/* the ranking: a store object of its own (storage.c; scenes.c's sounds: OBJ_SCENE0) */
#define LB_OBJ (OBJ_SCENE0 + 1u)
#define LB_MAGIC 0x4E524C31u                       /* "NRL1" */
typedef struct { char n[3]; uint8_t rsv; uint16_t score; uint16_t rsv2; } lb_ent_t;
static struct { uint32_t magic; lb_ent_t e[NR_LB]; } lb;

#define D_HEAD 1u                                  /* the screen's strips (gdraw.c) */
#define D_BAR 2u
#define D_BODY 12u
#define D_FOOT 16u
#define D_ALL 31u

#define NBOLT 5u                                   /* the lightning (gdraw.c draws it) */
#define BOLT_PTS 22u
static struct {
    uint32_t t;                                    /* fired at (ms | 1), 0: none */
    uint8_t n, k;                                  /* bolts; intensity 0..255 */
    uint8_t np[NBOLT];
    int16_t p[NBOLT][BOLT_PTS][2];
} bolt;

static struct {
    uint8_t gs, mode, phase, unlocked, cur, last, streak, intro_n, why, played, rank, sel;
    uint8_t set[SET_N], set_was[SET_N];
    char name[3];
    uint16_t combo, gain;
    uint32_t t0;                                   /* the state entered at */
    uint32_t ask_t, limit;                         /* the answer's clock: started at, runs this long (0: no limit) */
    uint32_t tone_off, tone_due;                   /* the heard note: its note off at; a note due at */
    uint8_t tone_note, due_note;
    uint64_t keys, quiet;                          /* the keys held; ... the ones that started a game (silent) */
    uint32_t start_t, flash_t;                     /* the pads: the start's sweep, a right answer's flash, from */
    uint32_t rnd;
    uint8_t dirty;                                 /* everything to draw again (main.c, console.c set it) */
    uint8_t redraw;                                /* the strips to draw again (D_*) */
    char msg[28];
    uint32_t msg_until;
} ui = {.rnd = 0x9E3779B9u, .name = {'A', 'A', 'A'}};

static void ui_say(const char *a, const char *b)   /* a line over the card (the board layer: UPDATE MODE ..) */
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
static uint32_t since(uint32_t t)                  /* ms since t (a stamp: ms | 1, so up to 1 ahead) */
{
    int32_t d = (int32_t)(fm1_ms - t);
    return d < 0 ? 0u : (uint32_t)d;
}
static uint32_t note_midi(uint32_t d) { return 60u + SEMI[d % 7u] + 12u * (d / 7u); }   /* step 0 = C4 (60) */
static uint32_t phase_first(void) { return ui.mode == MODE_ALL ? PH_NOME : ui.mode - 1u; }
static uint32_t new_note(void) { return ORDER[ui.unlocked - ui.intro_n]; }   /* (the intro's note) */
static uint32_t best(void) { return lb.e[0].score; }

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
static void tone_later(uint32_t n, uint32_t ms)
{
    ui.due_note = (uint8_t)n;
    ui.tone_due = (fm1_ms + ms) | 1u;
}
static void preset_load(uint32_t n)                /* the sound: factory sound n (also the USB console's "preset") */
{
    ev_push(EV_ALLOFF, 0, 0);
    preset_make(n % NFACT, &pt);
}

/* ------------------------------------------------------------- settings, ranking --- */
static void set_load(void)
{
    uint32_t i;
    for (i = 0; i < SET_N; i++)
        ui.set[i] = SET_DEF[i];
    if (settings.rsv[0] == NR_SET_TAG) {
        ui.set[SET_TIME] = settings.rsv[1];
        ui.set[SET_START] = settings.rsv[2];
        ui.set[SET_SFP] = settings.rsv[3];
        for (i = 0; i < NSND; i++)
            if (SND[i] == settings.preset)
                ui.set[SET_SOUND] = (uint8_t)i;
    }
    for (i = 0; i < SET_N; i++)
        if (ui.set[i] < SET_MIN[i] || ui.set[i] > SET_MAX[i])
            ui.set[i] = SET_DEF[i];
}
static void set_save(void)
{
    settings.rsv[0] = NR_SET_TAG;
    settings.rsv[1] = ui.set[SET_TIME];
    settings.rsv[2] = ui.set[SET_START];
    settings.rsv[3] = ui.set[SET_SFP];
    settings.preset = SND[ui.set[SET_SOUND]];
    settings_save();
}
static uint32_t time_limit(void) { return ui.set[SET_TIME] ? 1000u * (7u - ui.set[SET_TIME]) : 0u; }   /* 6 s .. 2 s */

static void lb_load(void)
{
    if (!(flash_ok && store_on && st_load(LB_OBJ, &lb, sizeof lb) == (int)sizeof lb && lb.magic == LB_MAGIC))
        memset(&lb, 0, sizeof lb);
    lb.magic = LB_MAGIC;
}
static uint32_t lb_place(uint32_t score)           /* where this score goes (NR_LB: not in) */
{
    uint32_t i;
    for (i = 0; i < NR_LB; i++)
        if (score > lb.e[i].score)
            return i;
    return NR_LB;
}
static void lb_add(void)
{
    uint32_t i = lb_place(ui.combo), k;
    if (i >= NR_LB)
        return;
    for (k = NR_LB - 1u; k > i; k--)
        lb.e[k] = lb.e[k - 1u];
    lb.e[i].n[0] = ui.name[0], lb.e[i].n[1] = ui.name[1], lb.e[i].n[2] = ui.name[2];
    lb.e[i].score = ui.combo;
    ui.rank = (uint8_t)i;
    if (flash_ok && store_on)
        st_save(LB_OBJ, &lb, sizeof lb);
}

/* ------------------------------------------------------------- the lightning --- */
/* as the web's (lightning.js): from the key played to the combo; as many bolts as the phase's points; brighter,
 * longer and wider the quicker the answer (1 s or less: full, 5 s or more: none) */
static int32_t isqrt(int32_t v)
{
    int32_t r = 0, b = 1 << 14;
    while (b > v)
        b >>= 2;
    while (b) {
        if (v >= r + b)
            v -= r + b, r = (r >> 1) + b;
        else
            r >>= 1;
        b >>= 2;
    }
    return r;
}
static int32_t key_x(uint32_t n)                   /* the key's place along the screen's width */
{
    static const uint8_t WI[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};   /* (a black key: by its white one) */
    int32_t k = (int32_t)n - NR_KEY0, w;
    if (k < 0 || k > 36)
        return 120;
    w = (k / 12) * 7 + WI[k % 12];
    return 8 + w * 224 / 21;
}
static void lightning(uint32_t n, uint32_t react_ms)
{
    int32_t k = react_ms <= 1000u ? 256 : react_ms >= 5000u ? 0 : 256 - (int32_t)(react_ms - 1000u) * 256 / 4000;
    int32_t base = 7 + (41 * k >> 8), steps = 5 + (15 * k >> 8);
    uint32_t b;
    if (k <= 0)
        return;
    bolt.t = fm1_ms | 1u;
    bolt.k = (uint8_t)(k > 255 ? 255 : k);
    bolt.n = ui.gain > NBOLT ? NBOLT : ui.gain;
    for (b = 0; b < bolt.n; b++) {
        int32_t cx = key_x(n), cy = 238, tx = 210 + (int32_t)(rnd() % 17u) - 8, ty = 17 + (int32_t)(rnd() % 9u) - 4;
        int32_t dx = 0, dy = -256, i;
        uint32_t np = 0;
        bolt.p[b][np][0] = (int16_t)cx, bolt.p[b][np++][1] = (int16_t)cy;
        for (i = 0; i < steps && np < BOLT_PTS - 1u; i++) {
            int32_t ex = tx - cx, ey = ty - cy, d = isqrt(ex * ex + ey * ey), j, len;
            if (d < base * 3 / 2)
                break;
            ex = ex * 256 / d, ey = ey * 256 / d;  /* toward the combo, half turned from the last way, a wobble */
            dx = (dx + ex) / 2, dy = (dy + ey) / 2;
            j = (int32_t)(rnd() % 205u) - 102;
            ex = dx - dy * j / 256, ey = dy + dx * j / 256;
            d = isqrt(ex * ex + ey * ey) | 1;
            dx = ex * 256 / d, dy = ey * 256 / d;
            len = base * (128 + (int32_t)(rnd() % 230u)) / 256;
            cx = clamp(cx + dx * len / 256, 0, 239), cy = clamp(cy + dy * len / 256, 0, 239);
            bolt.p[b][np][0] = (int16_t)cx, bolt.p[b][np++][1] = (int16_t)cy;
        }
        bolt.p[b][np][0] = (int16_t)tx, bolt.p[b][np++][1] = (int16_t)ty;
        bolt.np[b] = (uint8_t)np;
    }
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
    ui.limit = time_limit();
    if (ui.phase == PH_OUVIDO) {
        tone(note_midi(d));
        if (ui.limit)
            ui.limit += NR_TONE_MS;                /* (the clock as for the others once the note has sounded) */
    }
}
static void intro(uint32_t wait)
{
    enter(GS_INTRO);
    tone_later(note_midi(new_note()), wait);
}
static void game_start(void)
{
    ui.rnd ^= fm1_ms * 2654435761u;
    ui.combo = 0;
    ui.streak = 0;
    ui.unlocked = ui.set[SET_START];
    ui.intro_n = ui.unlocked;                      /* the first notes shown, one by one */
    ui.phase = (uint8_t)phase_first();
    ui.last = 0xFF;
    ui.start_t = fm1_ms | 1u;
    bolt.t = 0;
    intro(NR_START_MS);
}
static void next(void)                             /* after a right answer */
{
    if (ui.streak >= ui.set[SET_SFP]) {
        ui.streak = 0;
        if (ui.mode == MODE_ALL && ui.phase + 1u < PH_N)
            ui.phase++;
        else {
            ui.phase = (uint8_t)phase_first();
            if (ui.unlocked < NR_NOTES) {          /* all done for these notes: one more */
                ui.unlocked++;
                ui.intro_n = 1;
                intro(0);
                return;
            }
        }
    }
    ask();
}
static void hit(uint32_t n)
{
    ui.gain = PHASE_BONUS[ui.phase];
    ui.combo = (uint16_t)(ui.combo + ui.gain);
    ui.streak++;
    ui.flash_t = fm1_ms | 1u;
    lightning(n, fm1_ms - ui.ask_t);
    enter(GS_OK);
}
static void miss(uint32_t why, uint32_t played)
{
    ui.why = (uint8_t)why;
    ui.played = (uint8_t)played;
    ui.flash_t = fm1_ms | 1u;
    tone_later(note_midi(ui.cur), 450);            /* the right note, heard after the wrong one */
    enter(GS_OVER);
}
static void board(void)
{
    ui.rank = 0xFF;
    enter(GS_BOARD);
}
static int answer_ok(uint32_t n)
{
    uint32_t t = note_midi(ui.cur);
    return ui.phase == PH_PAUTA ? n == t : n % 12u == t % 12u;
}
static int starts(void)                            /* a key now starts a game (and stays silent) */
{
    return ui.gs == GS_TITLE || (ui.gs == GS_BOARD && fm1_ms - ui.t0 >= NR_BOARD_LOCK_MS);
}
static void game_key(uint32_t n)                   /* a key pressed (the keybed, or USB MIDI), MIDI note n */
{
    ui.rnd ^= fm1_ms;
    if (starts())
        game_start();
    else if (ui.gs == GS_ASK) {
        if (answer_ok(n))
            hit(n);
        else
            miss(WHY_KEY, n);
    }
}
static void to_title(void)
{
    tone_stop();
    ui.tone_due = 0;
    ev_push(EV_ALLOFF, 0, 0);
    enter(GS_TITLE);
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
        ui.dirty = 1;
    }
    if (bolt.t && since(bolt.t) >= NR_OK_MS) {
        bolt.t = 0;
        ui.dirty = 1;                              /* (the last frame: without it) */
    }
    switch (ui.gs) {
    case GS_INTRO:
        if (!ui.tone_due && now - ui.t0 >= NR_INTRO_MS) {
            if (--ui.intro_n)
                intro(0);
            else
                ask();
        }
        break;
    case GS_ASK:
        if (ui.limit && now - ui.ask_t >= ui.limit)
            miss(WHY_TIME, 0);
        break;
    case GS_OK:
        if (now - ui.t0 >= NR_OK_MS)
            next();
        break;
    case GS_OVER:
        if (now - ui.t0 >= NR_OVER_MS) {
            if (ui.combo && lb_place(ui.combo) < NR_LB) {
                ui.sel = 0;
                enter(GS_NAME);
            } else
                board();
        }
        break;
    }
}

/* ------------------------------------------------------------- the menus --- */
static void mode_step(int32_t d)
{
    ui.mode = (uint8_t)((ui.mode + MODE_N + (d < 0 ? -1 : 1)) % MODE_N);
    ui.dirty = 1;
}
static void name_letter(int32_t d)
{
    char *c = &ui.name[ui.sel % 3u];
    *c = (char)('A' + ((*c - 'A') + 26 + (d < 0 ? -1 : 1)) % 26);
    ui.redraw |= D_BODY;
}
static void name_done(int save)
{
    if (save)
        lb_add();
    ui.gs = GS_BOARD;                              /* (the place just taken stays lit) */
    ui.t0 = fm1_ms;
    ui.dirty = 1;
    if (!save)
        ui.rank = 0xFF;
}
static void setup_open(void)
{
    uint32_t i;
    for (i = 0; i < SET_N; i++)
        ui.set_was[i] = ui.set[i];
    ui.sel = 0;
    enter(GS_SETUP);
}
static void setup_close(void)
{
    uint32_t i;
    for (i = 0; i < SET_N; i++)
        if (ui.set[i] != ui.set_was[i]) {
            set_save();
            break;
        }
    to_title();
}
static void setup_turn(uint32_t row, int32_t d)
{
    int32_t v = (int32_t)ui.set[row] + (d < 0 ? -1 : 1);
    if (v < SET_MIN[row] || v > SET_MAX[row])
        return;
    ui.set[row] = (uint8_t)v;
    ui.sel = (uint8_t)row;
    if (row == SET_SOUND) {                        /* (heard at once) */
        preset_load(SND[v]);
        tone_later(72, 30);
    }
    ui.redraw |= D_BODY;
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
            uint64_t bit = (uint64_t)1u << k;
            vel = vel ? (vel > 127u ? 127u : vel) : 100u;
            if (now & bit) {
                if (starts() || ui.gs == GS_SETUP || ui.gs == GS_NAME) {
                    ui.quiet |= bit;               /* (a key that starts, or in a menu: silent) */
                    game_key(n);
                    continue;
                }
                if (ui.tone_off && ui.gs != GS_INTRO)
                    tone_stop();                   /* (the player's key over the heard note) */
                ev_push(EV_ON, n, vel);
                game_key(n);
            } else if (ui.quiet & bit)
                ui.quiet &= ~bit;
            else
                ev_push(EV_OFF, n, 0);
        }
}
static void buttons_poll(void)
{
    uint32_t rel, pr = fm1_input_edges(&rel), b;
    for (b = 0; b < FM1_NBTN; b++) {
        if (!((pr >> b) & 1u))
            continue;
        switch (ui.gs) {
        case GS_SETUP:
            if (b == B_GLOBE || b == B_PATCH)
                setup_close();
            else if (b == B_LEFT || b == B_RIGHT)
                ui.sel = (uint8_t)((ui.sel + SET_N + (b == B_LEFT ? -1 : 1)) % SET_N), ui.redraw |= D_BODY;
            else if (b == B_OCTDN || b == B_OCTUP)
                setup_turn(ui.sel, b == B_OCTDN ? -1 : 1);
            break;
        case GS_NAME:
            if (b == B_OCTUP || b == B_OCTDN)
                name_letter(b == B_OCTUP ? 1 : -1);
            else if (b == B_LEFT || b == B_RIGHT)
                ui.sel = (uint8_t)((ui.sel + 3u + (b == B_LEFT ? -1 : 1)) % 3u), ui.redraw |= D_BODY;
            else if (b == B_PLAY)
                name_done(1);
            else if (b == B_PATCH)
                name_done(0);
            break;
        default:
            if (b == B_PATCH)
                to_title();
            else if (b == B_GLOBE && (ui.gs == GS_TITLE || ui.gs == GS_BOARD))
                setup_open();
            else if (ui.gs == GS_TITLE && (b == B_LEFT || b == B_RIGHT))
                mode_step(b == B_LEFT ? -1 : 1);
            else if (b == B_PLAY) {
                if (starts())
                    game_start();
                else
                    replay();
            }
            break;
        }
    }
}
static void knobs_poll(void)
{
    uint32_t k;
    int32_t s;
    for (k = 0; k < 8u; k++) {
        if ((s = fm1_enc_take(k)) == 0)
            continue;
        if (ui.gs == GS_SETUP && k < SET_N)
            setup_turn(k, s);                      /* (K1..K4: the rows) */
        else if (ui.gs == GS_NAME && k == 0u)
            name_letter(s);
        else if (ui.gs == GS_TITLE && k == 0u)
            mode_step(s);
    }
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
            if (!starts()) {
                tone_stop();
                ev_push(EV_ON, d1, d2);
            }
            game_key(d1);
        } else if (cin == 8u || cin == 9u)
            ev_push(EV_OFF, d1, 0);
        else if (cin == 0xBu && d1 == 123u)
            ev_push(EV_ALLOFF, 0, 0);
    }
}

/* ------------------------------------------------------------- LEDs --- */
/* the identity's colours: yellow on blue. The combo meter column by column from the bottom left (each 16 a lap:
 * yellow on blue, then white on yellow); a right answer flashes green then flickers with the lightning; the start
 * sweeps yellow left to right; game over red; the title and the menus breathe blue with a yellow column */
static const uint8_t C_YEL_LED[3] = {255, 230, 40}, C_BLU_LED[3] = {20, 50, 255}, C_VIO_LED[3] = {50, 0, 210};
static const uint8_t C_OK_LED[3] = {0, 255, 110}, C_ERR_LED[3] = {255, 20, 50}, C_WHT_LED[3] = {255, 255, 255};
static void pad_col(uint32_t pad, const uint8_t *c, uint32_t k)   /* k of 256 */
{
    uint32_t sc = (4u + settings.bright * 12u) * k >> 8;
    padled_set(pad, (uint8_t)(c[0] * sc >> 8), (uint8_t)(c[1] * sc >> 8), (uint8_t)(c[2] * sc >> 8));
}
static void ui_leds(void)
{
    uint32_t i, now = fm1_ms, fl = ui.flash_t ? since(ui.flash_t) : 1000000u;
    uint32_t st = ui.start_t ? since(ui.start_t) : 1000000u;
    for (i = 0; i < 16u; i++) {
        uint32_t pad = (i & 1u ? 0u : 8u) + i / 2u, col = pad & 7u;   /* (i: the meter's order) */
        if (st < 560u) {                           /* the start: a yellow sweep */
            int32_t d = (int32_t)(st / 60u) - (int32_t)col;
            if (d >= 0 && d < 3)
                pad_col(pad, d ? C_BLU_LED : C_YEL_LED, d == 2 ? 120u : 256u);
            else
                pad_col(pad, C_VIO_LED, 30u);
        } else if (ui.gs == GS_OVER)
            pad_col(pad, C_ERR_LED, fl < 500u ? 256u : (now / 300u) & 1u ? 110u : 40u);
        else if (ui.gs == GS_TITLE || ui.gs >= GS_NAME) {
            uint32_t ph = (now / 6u) & 255u, br = ph < 128u ? 40u + ph : 40u + (255u - ph);
            if (col == (now / 180u) % 8u)
                pad_col(pad, C_YEL_LED, 220u);
            else
                pad_col(pad, C_BLU_LED, br);
        } else if (fl < 120u)
            pad_col(pad, C_OK_LED, 256u);
        else if (bolt.t && since(bolt.t) < NR_OK_MS)
            pad_col(pad, (rnd() & 3u) ? C_YEL_LED : C_WHT_LED, 120u + (rnd() & 127u));
        else {
            uint32_t lit = ui.combo % 16u, lap = (ui.combo / 16u) & 1u;
            if (i < lit)
                pad_col(pad, lap ? C_WHT_LED : C_YEL_LED, 256u);
            else
                pad_col(pad, lap ? C_YEL_LED : C_BLU_LED, 40u);
        }
    }
    fm1_led_key(B_LEFT, ui.gs == GS_TITLE || ui.gs == GS_SETUP || ui.gs == GS_NAME);
    fm1_led_key(B_RIGHT, ui.gs == GS_TITLE || ui.gs == GS_SETUP || ui.gs == GS_NAME);
    fm1_led_key(B_PLAY, ((ui.gs == GS_TITLE || ui.gs == GS_BOARD || ui.gs == GS_NAME) && (now / 500u) & 1u)
                        || (ui.gs == GS_ASK && ui.phase == PH_OUVIDO));
    fm1_led_key(B_GLOBE, ui.gs == GS_SETUP || ((ui.gs == GS_TITLE || ui.gs == GS_BOARD) && !((now / 500u) & 1u)));
    fm1_led_key(B_PATCH, ui.gs != GS_TITLE);
    fm1_led_key(B_OCTDN, ui.gs == GS_SETUP || ui.gs == GS_NAME);
    fm1_led_key(B_OCTUP, ui.gs == GS_SETUP || ui.gs == GS_NAME);
}

static void ui_init(void)
{
    set_load();
    lb_load();
    preset_load(SND[ui.set[SET_SOUND]]);
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
