/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 ueine (NOTE RUSH 37), on Vector37 by M. Geisler (makkuro) */
/* NOTE RUSH 37: the game (in Vector37's ui.c's place; the board layer and the synth engine are Vector37's).
 * As the web version (NOTE RUSH CYBERFUNK):
 *   FASE 1 MEMORIZE  the new notes shown (name and staff) and heard
 *   FASE 2 NOME      the letter alone (C .. B): play it, any octave (+3)
 *   FASE 3 PAUTA     the staff alone (C3 .. C6): play that very key (+4)
 *   FASE 4 OUVIDO    the note heard: play it, any octave (+5)
 * A right answer: lightning in an arc from the bottom left corner to the combo, and across the pads; N in a row
 * (ACERTOS) move to the next phase; after OUVIDO a new note: the middle octave C4 .. B4 first, then the upper one
 * C5 .. C6, the lower one C3 .. B3 last. A wrong key, or the time out (TEMPO; SEM LIMITE: none): game over, a name
 * (AAA) for the local ranking (top 5, in flash).
 * The keys always play (the sound chosen in the settings); they answer only when asked. SEQ PLAY goes home: from any
 * screen to the title, on the title it starts a game (the name: ENVIAR); a pad plays the OUVIDO note again. Title:
 * < > the mode; GLOBE: the settings (TEMPO, SOM, NOTAS INICIAIS, ACERTOS/FASE: < > the row, K1..K4 or OCT- OCT+ the
 * value); PATCH: the sound (K1..K8 its parameters, drawn); PARA: the VETOR. The name: OCT+ OCT- the letter, < > the
 * place. The sound everywhere: K1..K8 (or the VETOR's), the faders VOLUME, REVERB, DELAY, CHORUS, the pitch wheel a
 * bend, the mod wheel lightning on the orbit (the sound's place jumps about as a bolt).
 * The VETOR (PARA: in, and out again), the same for every sound: a Lissajous running free round the four corners, each
 * corner a brightness; where its point is, the corners' brightness blended moves the sound's filter (that alone: the
 * sound's own path and corners stay as they are). On its screen the knobs are its: K1 K2 K3 the figure (FREQ X,
 * FREQ Y, FASE), K4..K7 each corner's brightness, K8 its speed. The mod wheel throws its point about as a bolt. */
#if !BOARD_SMK37
#error "NOTE RUSH 37 is for the SMK-37 (Pro / Elite / MKE-P37)"
#endif
enum { B_KNOBBANK, B_FADERBANK, B_LEFT, B_RIGHT, B_OCTDN, B_OCTUP, B_ARP, B_REPEAT, B_SCALE, B_CHORD, B_GLOBE,
       B_BT, B_PATCH, B_PARA, B_FX, B_SEQ, B_PLAY, B_REC };
#define PANEL_KEYS() fm1_in.keys

#define NR_NOTES 22u                               /* the white keys C3 .. C6 (index 0 = C3) */
#define NR_TONE_MS 900u                            /* a note played for the player */
#define NR_OK_MS 340u                              /* a right answer shown (the lightning's length) */
#define NR_INTRO_MS 1500u                          /* a new note shown */
#define NR_OVER_MS 1600u                           /* game over shown, then the name or the ranking */
#define NR_START_MS 350u                           /* the start: the pads' lightning before the first note sounds */
#define NR_KEY0 48                                 /* the bottom key: C3 */
#define NR_LB 5u                                   /* the ranking's places */

enum { GS_TITLE, GS_INTRO, GS_ASK, GS_OK, GS_OVER, GS_NAME, GS_BOARD, GS_SETUP, GS_VECTOR, GS_SOUND };
enum { PH_NOME, PH_PAUTA, PH_OUVIDO, PH_N };
enum { MODE_ALL, MODE_NOME, MODE_PAUTA, MODE_OUVIDO, MODE_N };
enum { WHY_KEY = 1, WHY_TIME };
static const char *const MODE_NAME[MODE_N] = {"TODAS", "NOME", "PAUTA", "OUVIDO"};
static const uint8_t PHASE_BONUS[PH_N] = {3, 4, 5};
static const uint8_t ORDER[NR_NOTES] = {7, 8, 9, 10, 11, 12, 13,            /* C4 .. B4 */
                                        14, 15, 16, 17, 18, 19, 20, 21,     /* C5 .. C6 */
                                        0, 1, 2, 3, 4, 5, 6};               /* C3 .. B3 */
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

/* the sound's controls: K1..K8 (a corner parameter: all four corners together) and the faders */
typedef struct { const char *name; uint8_t corner, id; } sctl_t;
static const sctl_t KNOB[8] = {{"BRILHO", 1, CP_CUT}, {"RESSON\xC2NCIA", 1, CP_RES}, {"ATAQUE", 0, G_AA},
                               {"RELEASE", 0, G_AR}, {"DRIVE", 0, G_DRIVE}, {"SALA", 0, G_RVSIZE},
                               {"ECO TEMPO", 0, G_DLTIME}, {"ECO REPETE", 0, G_DLFB}};
static const sctl_t FADER[4] = {{"VOLUME", 0, G_VOLUME}, {"REVERB", 0, G_RVMIX}, {"DELAY", 0, G_DLMIX},
                                {"CHORUS", 0, G_CHMIX}};

/* the VETOR (PARA): a store object of its own; the corners' brightness: added to the sound's own */
#define VEC_OBJ (OBJ_SCENE0 + 2u)
#define VEC_MAGIC 0x4E525632u                      /* "NRV2" */
typedef struct { uint32_t magic; uint8_t rsv, speed; int8_t fx, fy, ph, cut[NC]; uint8_t rsv2; } vec_t;
#define VEC_DEFAULT {VEC_MAGIC, 0, 48, -64, -39, 0, {0, 0, 0, 0}, 0}   /* (a figure eight; the corners at 0: it */
static vec_t vec = VEC_DEFAULT, vec_was;                            /*  changes nothing until they are turned) */
static const char *const VEC_NAME[8] = {"FREQ X", "FREQ Y", "FASE", "BRILHO CANTO 1", "BRILHO CANTO 2",
                                        "BRILHO CANTO 3", "BRILHO CANTO 4", "VELOCIDADE"};
#define VEC_SPEED_MAX 112
static int16_t base_cut[NC];                       /* the sound's own corner cutoffs (K1 BRILHO moves them) */
static const int8_t DIR16[16][2] = {{64, 0}, {59, 24}, {45, 45}, {24, 59}, {0, 64}, {-24, 59}, {-45, 45}, {-59, 24},
                                    {-64, 0}, {-59, -24}, {-45, -45}, {-24, -59}, {0, -64}, {24, -59}, {45, -45}, {59, -24}};

/* the rack: what was set on the PATCH screen and with the faders, kept from sound to sound (and in flash); a knob or
 * a fader never touched: each sound's own. BRILHO and RESSONANCIA as a change from the sound's own (each sound starts
 * elsewhere), the others as values */
#define RACK_OBJ (OBJ_SCENE0 + 3u)
#define RACK_MAGIC 0x4E525231u                     /* "NRR1" */
#define RACK_UNSET (-128)
typedef struct { uint32_t magic; int8_t k[8], f[4]; } rack_t;
static rack_t rack;
static uint8_t rack_dirty;
static patch_t factory;                            /* the sound as it comes */

/* the ranking: a store object of its own (storage.c; scenes.c's sounds: OBJ_SCENE0) */
#define LB_OBJ (OBJ_SCENE0 + 1u)
#define LB_MAGIC 0x4E524C31u                       /* "NRL1" */
typedef struct { char n[3]; uint8_t rsv; uint16_t score; uint16_t rsv2; } lb_ent_t;
static struct { uint32_t magic; lb_ent_t e[NR_LB]; } lb;

#define D_HEAD 1u                                  /* the screen's strips (gdraw.c) */
#define D_BAR 2u
#define D_MID 4u                                   /* (the card's upper half) */
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
    uint64_t keys;
    uint32_t flash_t;                              /* game over's red, from */
    uint8_t kturn;                                 /* the sound's knob turned last (+1: the PATCH screen lights it) */
    uint32_t kturn_t;
    int32_t bend, mod;                             /* the pitch wheel (Q15), the mod wheel (0 .. 32767) */
    uint8_t fad[4], fad_ready;                     /* the faders as last taken */
    uint32_t rnd;
    uint8_t dirty;                                 /* everything to draw again (main.c, console.c set it) */
    uint8_t redraw;                                /* the strips to draw again (D_*) */
    char msg[28];
    uint32_t msg_until;
} ui = {.rnd = 0x9E3779B9u, .name = {'A', 'A', 'A'}};

static void ui_say(const char *a, const char *b)   /* a short line in the header (title: its foot) for 2 s */
{
    str_cpy(ui.msg, a, sizeof ui.msg);
    if (b)
        str_cat(ui.msg, b, sizeof ui.msg);
    ui.msg_until = fm1_ms + 2000u;
    ui.redraw |= D_HEAD | D_FOOT;
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
static uint32_t note_midi(uint32_t i) { return 48u + SEMI[i % 7u] + 12u * (i / 7u); }   /* index 0 = C3 (48) */
static int32_t note_step(uint32_t i) { return (int32_t)i - 7; }   /* its staff step: 0 = C4 (gdraw.c) */
static uint32_t phase_first(void) { return ui.mode == MODE_ALL ? PH_NOME : ui.mode - 1u; }
static uint32_t new_note(void) { return ORDER[ui.unlocked - ui.intro_n]; }   /* (the intro's note) */

/* ------------------------------------------------------------- sound --- */
static void note_play(uint32_t n, uint32_t vel)    /* a note on, the same each time: the chorus's sweep starts anew */
{                                                  /* (its free-running LFO made each strike louder or softer) */
    chs.ph = 0;
    ev_push(EV_ON, n, vel);
}
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
    note_play(n, 100);
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
    if (pt.g[G_BEND] < 2)
        pt.g[G_BEND] = 2;                          /* (the wheels need a range: 2 semitones) */
    pt.g[G_RETRIG] = 1;                            /* the same key, the same sound: each note starts its orbit anew */
    pt.g[G_SPIN] = 0;                              /* (and the path does not turn on with the time) */
    if (pt.g[G_POLY] == PM_POLY)                   /* (Vector37's), no random place in the stereo, no random drift */
        pt.g[G_STEREO] = 0, pt.g[G_DETUNE] = 0;
    factory = pt;
    {                                              /* the rack on it */
        uint32_t i, c;
        for (i = 0; i < 8u; i++) {
            const sctl_t *k = &KNOB[i];
            if (rack.k[i] == RACK_UNSET)
                continue;
            if (k->corner)
                for (c = 0; c < NC; c++)
                    pt.c[c][k->id] = (int16_t)clamp(factory.c[c][k->id] + rack.k[i], CDESC[k->id].min, CDESC[k->id].max);
            else
                pt.g[k->id] = (int16_t)clamp(rack.k[i], GDESC[k->id].min, GDESC[k->id].max);
        }
        for (i = 0; i < 4u; i++)
            if (rack.f[i] != RACK_UNSET)
                pt.g[FADER[i].id] = rack.f[i];
        for (c = 0; c < NC; c++)
            base_cut[c] = pt.c[c][CP_CUT];
    }
}
static void rack_clear(void) { memset(rack.k, RACK_UNSET & 0xFF, sizeof rack.k), memset(rack.f, RACK_UNSET & 0xFF, sizeof rack.f); }
static void rack_load(void)
{
    if (!(flash_ok && store_on && st_load(RACK_OBJ, &rack, sizeof rack) == (int)sizeof rack && rack.magic == RACK_MAGIC))
        rack_clear();
    rack.magic = RACK_MAGIC;
}
static void rack_save(void)                        /* (out of a game: a save silences the sound a moment) */
{
    if (rack_dirty && flash_ok && store_on)
        st_save(RACK_OBJ, &rack, sizeof rack);
    rack_dirty = 0;
}
static void say_num(const char *name, int32_t v)   /* "BRILHO 80" in the header */
{
    char b[8];
    uint32_t n = 0;
    if (v < 0)
        b[n++] = '-', v = -v;
    if (v >= 100)
        b[n++] = (char)('0' + v / 100);
    if (v >= 10)
        b[n++] = (char)('0' + v / 10 % 10);
    b[n++] = (char)('0' + v % 10);
    b[n] = 0;
    ui_say(name, " ");
    str_cat(ui.msg, b, sizeof ui.msg);
}
static void sctl_say(const sctl_t *c) { say_num(c->name, c->corner ? (c->id == CP_CUT ? base_cut[0] : pt.c[0][c->id]) : pt.g[c->id]); }
static void sctl_set(const sctl_t *c, int32_t v, int rel)   /* rel: v is a change */
{
    uint32_t k;
    if (c->corner && c->id == CP_CUT)              /* (the brightness: the sound's own; the VETOR adds to it) */
        for (k = 0; k < NC; k++)
            base_cut[k] = (int16_t)clamp(rel ? base_cut[k] + v : v, 0, 127), pt.c[k][CP_CUT] = base_cut[k];
    else if (c->corner)
        for (k = 0; k < NC; k++)
            pt.c[k][c->id] = (int16_t)clamp(rel ? pt.c[k][c->id] + v : v, CDESC[c->id].min, CDESC[c->id].max);
    else
        pt.g[c->id] = (int16_t)clamp(rel ? pt.g[c->id] + v : v, GDESC[c->id].min, GDESC[c->id].max);
    sctl_say(c);
    if (c >= KNOB && c < KNOB + 8) {               /* (a knob: the PATCH screen lights it, draws it anew) */
        uint32_t i = (uint32_t)(c - KNOB);
        int32_t now = c->corner ? (c->id == CP_CUT ? base_cut[0] : pt.c[0][c->id]) : pt.g[c->id];
        ui.kturn = (uint8_t)(i + 1u), ui.kturn_t = fm1_ms | 1u, ui.redraw |= D_BODY;
        rack.k[i] = (int8_t)clamp(c->corner ? now - factory.c[0][c->id] : now, -127, 127);
    } else
        rack.f[c - FADER] = (int8_t)clamp(pt.g[c->id], 0, 127);
    rack_dirty = 1;
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

static void vec_load(void)
{
    if (!(flash_ok && store_on && st_load(VEC_OBJ, &vec, sizeof vec) == (int)sizeof vec && vec.magic == VEC_MAGIC)) {
        vec_t d = VEC_DEFAULT;
        vec = d;
    }
}
static void vec_knob(uint32_t k, int32_t s)        /* the VETOR's knobs: the figure, the corners, the speed */
{
    int8_t *p = k == 0u ? &vec.fx : k == 1u ? &vec.fy : k == 2u ? &vec.ph : k < 7u ? &vec.cut[k - 3u] : 0;
    if (p) {
        *p = (int8_t)clamp(*p + s * 2, -64, 63);
        if (k < 2u) {                              /* (a frequency: as the path takes it, 1.0 .. 6.0) */
            int32_t f = clamp(256 + (*p + 64) * 256 / 25, 256, 6 * 256);
            char b[8] = {(char)('0' + (f >> 8)), '.', (char)('0' + ((f & 255) * 10 >> 8)), 0};
            ui_say(VEC_NAME[k], " "), str_cat(ui.msg, b, sizeof ui.msg);
        } else
            say_num(VEC_NAME[k], *p);
    } else {
        vec.speed = (uint8_t)clamp((int32_t)vec.speed + s * 2, 0, VEC_SPEED_MAX);
        say_num(VEC_NAME[7], vec.speed);
    }
    ui.redraw |= D_BODY;
}

/* the mod wheel's lightning on the orbit: the sound's place jumps out from the path as a bolt does, piece by piece,
 * each bent off the straight way, then back to the path and out again, somewhere else; farther and quicker the
 * further the wheel is up (gdraw.c draws its trail on the VETOR) */
#define VB_TRAIL 12u
static struct { int32_t x, y, tx, ty; uint32_t t; uint8_t n, tn; int16_t tr[VB_TRAIL][2]; } vb;
static void vbolt_tick(void)
{
    int32_t a = ui.mod;
    if (since(vb.t) < 32u - (uint32_t)a * 22u / 32767u)
        return;
    vb.t = fm1_ms | 1u;
    if (a < 400) {                                 /* (the wheel down: back onto the path) */
        vb.x = vb.x * 3 / 4, vb.y = vb.y * 3 / 4, vb.n = 0, vb.tn = 0;
    } else {
        int32_t ex, ey, j;
        if (!vb.n) {                               /* a new bolt: from the path, out toward somewhere */
            const int8_t *d = DIR16[rnd() % 16u];
            int32_t r = a / 100 * (40 + (int32_t)(rnd() % 60u)) * 3 / 4;
            vb.tx = d[0] * r / 64, vb.ty = d[1] * r / 64;
            vb.x = vb.y = 0, vb.n = (uint8_t)(3u + rnd() % 5u), vb.tn = 0;
        }
        ex = (vb.tx - vb.x) / vb.n, ey = (vb.ty - vb.y) / vb.n, j = (int32_t)(rnd() % 201u) - 100;
        vb.x = clamp(vb.x + ex - ey * j / 150, -32767, 32767), vb.y = clamp(vb.y + ey + ex * j / 150, -32767, 32767);
        vb.n--;
        if (vb.tn < VB_TRAIL)
            vb.tr[vb.tn][0] = (int16_t)vb.x, vb.tr[vb.tn++][1] = (int16_t)vb.y;
    }
}

/* the VETOR running: its point on the figure (as the engine draws a Lissajous: orbit.c), thrown about by the mod
 * wheel's bolt; the corners' brightness blended there moves the sound's filter. Speed: 0.05 .. 6 rounds a second */
static struct { uint32_t ph, t; int32_t x, y; } vp;
static void vec_tick(void)
{
    uint32_t c, v = vec.speed > VEC_SPEED_MAX ? VEC_SPEED_MAX : vec.speed, dt = fm1_ms - vp.t;
    uint32_t q8 = (51u << (v >> 4)) * (256u + (v & 15u) * 16u) >> 8;   /* x 0.2 .. x 25 of a round in 4 s */
    int32_t x, y, w[NC], off = 0;
    opar_t o;
    if (dt == 0u)
        return;
    vp.t = fm1_ms;
    vp.ph += (uint32_t)((uint64_t)(dt > 50u ? 50u : dt) * 1073741u * q8 >> 8);
    memset(&o, 0, sizeof o);
    o.wx = vec.ph * 256, o.wy = vec.fx * 256, o.mo = vec.fy * 256;
    fam_xy(SH_LISSA, &o, vp.ph, &x, &y);
    vp.x = clamp(x * 7 / 8 + vb.x, -32767, 32767), vp.y = clamp(y * 7 / 8 + vb.y, -32767, 32767);
    orbit_weights(vp.x, vp.y, w, 2);
    for (c = 0; c < NC; c++)
        off += w[c] * vec.cut[c];
    off >>= 15;
    for (c = 0; c < NC; c++)
        pt.c[c][CP_CUT] = (int16_t)clamp(base_cut[c] + off, 0, 127);
}

/* ------------------------------------------------------------- the lightning --- */
/* the screen: as many bolts as the phase's points, each an arc from the bottom left corner up to the combo (a
 * quadratic curve bent toward the top left), jagged off it; more pieces and wilder the quicker the answer (1 s or
 * less: full, 5 s or more: none) */
static void lightning(uint32_t react_ms)
{
    int32_t k = react_ms <= 1000u ? 256 : react_ms >= 5000u ? 0 : 256 - (int32_t)(react_ms - 1000u) * 256 / 4000;
    int32_t steps = 8 + (13 * k >> 8), jit = 3 + (8 * k >> 8), i;
    uint32_t b;
    if (k <= 0)
        return;
    bolt.t = fm1_ms | 1u;
    bolt.k = (uint8_t)(k > 255 ? 255 : k);
    bolt.n = ui.gain > NBOLT ? NBOLT : ui.gain;
    for (b = 0; b < bolt.n; b++) {
        int32_t x0 = (int32_t)(rnd() % 12u), y0 = 239 - (int32_t)(rnd() % 12u);
        int32_t x1 = 210 + (int32_t)(rnd() % 17u) - 8, y1 = 17 + (int32_t)(rnd() % 9u) - 4;
        int32_t cx = 4 + (int32_t)(rnd() % 70u), cy = 8 + (int32_t)(rnd() % 70u), s2 = steps * steps;
        for (i = 0; i <= steps; i++) {
            int32_t u = steps - i, x = (u * u * x0 + 2 * u * i * cx + i * i * x1) / s2;
            int32_t y = (u * u * y0 + 2 * u * i * cy + i * i * y1) / s2;
            if (i && i < steps)
                x += (int32_t)(rnd() % (2u * (uint32_t)jit + 1u)) - jit, y += (int32_t)(rnd() % (2u * (uint32_t)jit + 1u)) - jit;
            bolt.p[b][i][0] = (int16_t)clamp(x, 0, 239), bolt.p[b][i][1] = (int16_t)clamp(y, 0, 239);
        }
        bolt.np[b] = (uint8_t)(steps + 1);
    }
}

/* the pads: the same idea on the 2 x 8 grid: a bolt walks from one end to the other, pad by pad, now on along the
 * row, now across, now both, with a branch here and there; each pad struck flashes white, then glows yellow away */
#define PB_N 4u
#define PB_MS 180u                                 /* a bolt's run across */
#define PH_MS 420u                                 /* a pad's glow after the strike */
static struct { uint32_t t; uint8_t n, done, p[28]; } pb[PB_N];
static uint32_t pheat[16];                         /* each pad struck at (ms | 1) */
static uint32_t pad_at(int32_t col, uint32_t row) { return (row ? 8u : 0u) + (uint32_t)col; }   /* row 1: bottom */
static void pad_bolt(int left, uint32_t row, int up, uint32_t delay)   /* from the left (or right) end, row; up: end
                                                                          on the top row; delay: its start, ms on */
{
    uint32_t s, o = 0, n = 0;
    int32_t col = left ? 0 : 7, dir = left ? 1 : -1, end = left ? 7 : 0;
    for (s = 1; s < PB_N; s++)                     /* (the oldest slot) */
        if (!pb[s].t || (pb[o].t && since(pb[s].t) > since(pb[o].t)))
            o = s;
    pb[o].p[n++] = (uint8_t)pad_at(col, row);
    while (n < sizeof pb[o].p - 2u && col != end) {
        uint32_t r = rnd() % 100u;
        if (r < 22u)
            row ^= 1u;                             /* across */
        else {
            col += dir;                            /* on, sometimes across too */
            if (r < 48u)
                row ^= 1u;
        }
        pb[o].p[n++] = (uint8_t)pad_at(col, row);
        if (rnd() % 100u < 18u)
            pb[o].p[n++] = (uint8_t)pad_at(col, row ^ 1u);   /* a branch */
    }
    if (up && row)
        pb[o].p[n++] = (uint8_t)pad_at(end, 0u);
    pb[o].n = (uint8_t)n;
    pb[o].done = 0;
    pb[o].t = (fm1_ms + delay) | 1u;
}

/* the screen's bands (the pads go on from the screen's right edge, as on the keyboard): a bolt runs a band of the
 * screen from left to right (the header while a combo runs, the score while it is named), jagged, a branch here and
 * there; out at the right edge it goes on across the pads */
#define SB_N 3u
#define SB_PTS 24u
#define SB_BR 10u                                  /* branches at most */
#define SB_RUN 130u                                /* its run across the band */
#define SB_LIFE 300u                               /* ... then it draws back into the right edge */
static struct { uint32_t t; uint8_t n, nb, strip; uint8_t at[SB_BR], bl[SB_BR]; int16_t p[SB_PTS][2], b[SB_BR][3][2]; } sb[SB_N];
static void band_bolt(int32_t y0, int32_t y1, uint32_t strip, int wild)   /* wild: more branches, longer, and */
{                                                                          /* only on the screen (not on the pads) */
    uint32_t s, o = 0, n = 0;
    int32_t x = 0, y = y0 + (int32_t)(rnd() % (uint32_t)(y1 - y0 + 1));
    for (s = 1; s < SB_N; s++)
        if (!sb[s].t || (sb[o].t && since(sb[s].t) > since(sb[o].t)))
            o = s;
    sb[o].nb = 0;
    sb[o].p[n][0] = (int16_t)x, sb[o].p[n++][1] = (int16_t)y;
    while (x < 239 && n < SB_PTS) {
        x += 8 + (int32_t)(rnd() % 13u);
        x = x > 239 ? 239 : x;
        y = clamp(y + (int32_t)(rnd() % 21u) - 10, y0, y1);
        sb[o].p[n][0] = (int16_t)x, sb[o].p[n++][1] = (int16_t)y;
        if (sb[o].nb < (wild ? SB_BR : 4u) && rnd() % 100u < (wild ? 60u : 25u)) {   /* a branch, up or down */
            int32_t d = rnd() & 1u ? 1 : -1, k, bl = wild ? 3 : 2, bx = x, by = y;
            for (k = 0; k < bl; k++) {             /* (each piece further off, and bent) */
                bx += (wild ? 3 : 4) + (int32_t)(rnd() % (wild ? 9u : 5u)) - (wild ? 2 : 0);
                by = clamp(by + d * (4 + (int32_t)(rnd() % (wild ? 10u : 6u))), y0, y1);
                sb[o].b[sb[o].nb][k][0] = (int16_t)bx, sb[o].b[sb[o].nb][k][1] = (int16_t)by;
            }
            sb[o].bl[sb[o].nb] = (uint8_t)bl;
            sb[o].at[sb[o].nb++] = (uint8_t)(n - 1u);
        }
    }
    sb[o].n = (uint8_t)n;
    sb[o].strip = (uint8_t)strip;
    sb[o].t = fm1_ms | 1u;
    if (!wild)
        pad_bolt(1, (uint32_t)(y - y0) * 2u > (uint32_t)(y1 - y0) ? 1u : 0u, 0, SB_RUN);   /* (on, from its height) */
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
    bolt.t = 0;
    pad_bolt(1, 1u, 0, 0);                         /* (the start: two bolts across the pads) */
    pad_bolt(1, 0u, 0, 0);
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
static void hit(void)
{
    ui.gain = PHASE_BONUS[ui.phase];
    ui.combo = (uint16_t)(ui.combo + ui.gain);
    ui.streak++;
    lightning(fm1_ms - ui.ask_t);
    pad_bolt(1, 0u, 0, 200);                       /* at the combo (the screen's top right): on, across the pads */
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
static void game_key(uint32_t n)                   /* a key pressed (the keybed, or USB MIDI), MIDI note n */
{
    ui.rnd ^= fm1_ms;
    if (ui.gs == GS_ASK) {
        if (answer_ok(n))
            hit();
        else
            miss(WHY_KEY, n);
    }
}
static void to_title(void)
{
    rack_save();
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
        ui.redraw |= D_HEAD | D_FOOT;
    }
    {                                              /* the bands' lightning: the score being named, the combo */
        static uint32_t due;                       /* running (more often the higher), on into the pads */
        uint32_t i;
        if (ui.gs == GS_NAME || (ui.combo && ui.gs >= GS_INTRO && ui.gs <= GS_OK)) {
            if ((int32_t)(now - due) >= 0) {
                if (ui.gs == GS_NAME)
                    band_bolt(46, 104, D_MID, 1), due = now + 140u + rnd() % 220u;
                else
                    band_bolt(3, 30, D_HEAD, 0), due = now + 1500u - (ui.combo > 60u ? 60u : ui.combo) * 20u + rnd() % 200u;
            }
        } else
            due = now + 300u;
        for (i = 0; i < SB_N; i++)
            if (sb[i].t) {
                ui.redraw |= sb[i].strip;          /* (on screen: drawn every frame, and once more when gone) */
                if (since(sb[i].t) >= SB_LIFE)
                    sb[i].t = 0;
            }
    }
    if (ui.gs == GS_VECTOR) {                      /* (the VETOR: its dot moves) */
        static uint32_t vt;
        if (now / 33u != vt)
            vt = now / 33u, ui.redraw |= D_BODY;
    }
    vbolt_tick();
    vec_tick();
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
static void vec_open(void)
{
    vec_was = vec;
    enter(GS_VECTOR);
}
static void vec_close(void)
{
    if (memcmp(&vec, &vec_was, sizeof vec) && flash_ok && store_on)
        st_save(VEC_OBJ, &vec, sizeof vec);
    to_title();
}
static void sound_open(void) { ui.kturn = 0; enter(GS_SOUND); }
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
static void keys_poll(void)                        /* the keys always play; asked, they answer */
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
                note_play(n, vel);
                game_key(n);
            } else
                ev_push(EV_OFF, n, 0);
        }
}
static void buttons_poll(void)                     /* PLAY: home (the title), there: a game; PATCH, GLOBE, PARA: */
{                                                  /* their screens (outside a game) */
    uint32_t rel, pr = fm1_input_edges(&rel), b;
    for (b = 0; b < FM1_NBTN; b++) {
        if (!((pr >> b) & 1u))
            continue;
        switch (ui.gs) {
        case GS_SETUP:
            if (b == B_GLOBE || b == B_PLAY)
                setup_close();
            else if (b == B_LEFT || b == B_RIGHT)
                ui.sel = (uint8_t)((ui.sel + SET_N + (b == B_LEFT ? -1 : 1)) % SET_N), ui.redraw |= D_BODY;
            else if (b == B_OCTDN || b == B_OCTUP)
                setup_turn(ui.sel, b == B_OCTDN ? -1 : 1);
            break;
        case GS_VECTOR:
            if (b == B_PARA || b == B_PLAY)
                vec_close();
            break;
        case GS_SOUND:
            if (b == B_PATCH || b == B_PLAY)
                to_title();
            else if (b == B_GLOBE) {               /* GLOBE there: every sound as it comes */
                rack_clear();
                rack_dirty = 1;
                preset_load(SND[ui.set[SET_SOUND]]);
                ui_say("DE F\xC1" "BRICA", 0);
                ui.redraw |= D_BODY;
            }
            break;
        case GS_NAME:
            if (b == B_OCTUP || b == B_OCTDN)
                name_letter(b == B_OCTUP ? 1 : -1);
            else if (b == B_LEFT || b == B_RIGHT)
                ui.sel = (uint8_t)((ui.sel + 3u + (b == B_LEFT ? -1 : 1)) % 3u), ui.redraw |= D_BODY;
            else if (b == B_PLAY)
                name_done(1);
            break;
        default:
            if (b == B_PLAY) {
                if (ui.gs == GS_TITLE)
                    game_start();
                else if (ui.gs != GS_OVER)
                    to_title();                    /* (the ranking, or a game left) */
            } else if (ui.gs != GS_TITLE && ui.gs != GS_BOARD)
                break;                             /* (in a game: the keys, the pads) */
            else if (b == B_GLOBE)
                setup_open();
            else if (b == B_PARA)
                vec_open();
            else if (b == B_PATCH)
                sound_open();
            else if (ui.gs == GS_TITLE && (b == B_LEFT || b == B_RIGHT))
                mode_step(b == B_LEFT ? -1 : 1);
            break;
        }
    }
}
static void knobs_poll(void)                       /* the settings: K1..K4 its rows; else the sound */
{
    uint32_t k;
    int32_t s;
    for (k = 0; k < 8u; k++) {
        if ((s = fm1_enc_take(k)) == 0)
            continue;
        if (ui.gs == GS_SETUP && k < SET_N)
            setup_turn(k, s);
        else if (ui.gs == GS_VECTOR)
            vec_knob(k, s);                        /* (its screen: the knobs are the VETOR's) */
        else
            sctl_set(&KNOB[k], s * 2, 1);
    }
}
static void faders_poll(void)                      /* VOLUME, REVERB, DELAY, CHORUS: a fader moved sets its value */
{
    uint32_t i;
    if (fm1_in.frames < 300u)
        return;
    for (i = 0; i < 4u; i++) {
        int32_t v = fm1_in.fader[i], d = v - ui.fad[i];
        if (!ui.fad_ready)
            ui.fad[i] = (uint8_t)v;                /* (where they are at the start: the sound's own values stay) */
        else if (d > 1 || d < -1 || (d && (v == 0 || v == 127))) {
            ui.fad[i] = (uint8_t)v;
            sctl_set(&FADER[i], v, 0);
        }
    }
    ui.fad_ready = 1;
}
/* the wheels (as Vector37's ui.c reads them): pitch a bend (its centre where it rests); mod the lightning on the orbit
 * (vbolt_tick) */
static void wheels_poll(void)
{
    enum { DEAD = 12, SPAN0 = 80, MOD_LO = 485, MOD_HI = 615 };
    static int32_t pc, plo, phi, mlo, mhi, rest, mf, shown = -1;
    int32_t p = fm1_in.wheel[0], m = fm1_in.wheel[1], d, x;
    if (fm1_in.frames < 300u)
        return;
    if (!pc) {
        pc = p, plo = p - SPAN0, phi = p + SPAN0;
        mlo = m < MOD_LO ? m : MOD_LO, mhi = m > MOD_HI ? m : MOD_HI;
    }
    plo = p < plo ? p : plo, phi = p > phi ? p : phi;
    mlo = m < mlo ? m : mlo, mhi = m > mhi ? m : mhi;
    d = p - pc;
    if (d > -DEAD && d < DEAD) {
        x = 0;
        if (++rest > 200) {
            pc += d > 0 ? 1 : d < 0 ? -1 : 0;
            rest = 0;
        }
    } else {
        rest = 0;
        x = d > 0 ? (d - DEAD) * 32767 / (phi - pc - DEAD > 1 ? phi - pc - DEAD : 1)
                  : (d + DEAD) * 32767 / (pc - plo - DEAD > 1 ? pc - plo - DEAD : 1);
    }
    ui.bend = clamp(x, -32767, 32767);
    x = mhi - m <= DEAD ? 0 : clamp((mhi - m - DEAD) * 32767 / (mhi - mlo - DEAD > 1 ? mhi - mlo - DEAD : 1), 0, 32767);
    mf += (x * 8 - mf) >> 3;                       /* (smoothed: the reading is noisy) */
    ui.mod = (mf + 4) >> 3;
    x = ui.mod * 100 / 32767;
    if (x != shown && (x - shown > 2 || shown - x > 2 || x == 0)) {
        if (shown >= 0) {
            char b[6];
            b[0] = (char)('0' + x / 100), b[1] = (char)('0' + x / 10 % 10), b[2] = (char)('0' + x % 10), b[3] = '%', b[4] = 0;
            ui_say("RAIO ", x >= 100 ? b : x >= 10 ? b + 1 : b + 2);
        }
        shown = x;
    }
    joy_x = ui.bend;
}

/* USB MIDI in: a MIDI keyboard plays and answers as the keys do (any channel, or the one set) */
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
            note_play(d1, d2);
            game_key(d1);
        } else if (cin == 8u || cin == 9u)
            ev_push(EV_OFF, d1, 0);
        else if (cin == 0xBu && d1 == 123u)
            ev_push(EV_ALLOFF, 0, 0);
    }
}

/* ------------------------------------------------------------- LEDs --- */
/* the identity's colours, blue and yellow: a slow blue breath under it all (game over: red), over it the pads'
 * lightning (pad_bolt) in yellow: the screen's bolts going on (a right answer, a combo running), the start, and in the
 * menus now and then on its own */
static const uint8_t C_YEL_LED[3] = {255, 230, 40}, C_BLU_LED[3] = {20, 50, 255};
static const uint8_t C_ERR_LED[3] = {255, 20, 50};
static void pad_col(uint32_t pad, const uint8_t *c, uint32_t k)   /* k of 256 */
{
    uint32_t sc = (4u + settings.bright * 12u) * k >> 8;
    padled_set(pad, (uint8_t)(c[0] * sc >> 8), (uint8_t)(c[1] * sc >> 8), (uint8_t)(c[2] * sc >> 8));
}
static void ui_leds(void)
{
    static uint32_t idle_next;
    uint32_t i, now = fm1_ms, fl = ui.flash_t ? since(ui.flash_t) : 1000000u;
    int menu = ui.gs == GS_TITLE || ui.gs >= GS_NAME;
    if (menu && (int32_t)(now - idle_next) >= 0) {   /* (the menus: a bolt now and then, either way, either row) */ /* (the menus: a bolt now and then, either way, either row) */
        pad_bolt((int)(rnd() & 1u), rnd() & 1u, 0, 0);
        idle_next = now + 500u + rnd() % 1100u;
    }
    for (i = 0; i < PB_N; i++)                     /* the bolts on their way: their pads struck as they get there */
        if (pb[i].t && (int32_t)(now - pb[i].t) >= 0) {
            uint32_t r = pb[i].n * since(pb[i].t) / PB_MS + 1u;
            for (r = r > pb[i].n ? pb[i].n : r; pb[i].done < r; pb[i].done++)
                pheat[pb[i].p[pb[i].done] & 15u] = now | 1u;
            if (pb[i].done >= pb[i].n)
                pb[i].t = 0;
        }
    for (i = 0; i < 16u; i++) {
        uint32_t pad = (i & 1u ? 0u : 8u) + i / 2u;   /* (i: the meter's order) */
        uint32_t age = pheat[pad] ? since(pheat[pad]) : PH_MS;
        if (age < 70u)                             /* struck: yellow, flickering */
            pad_col(pad, C_YEL_LED, (rnd() & 1u) ? 256u : 150u);
        else if (age < PH_MS)                      /* its glow */
            pad_col(pad, C_YEL_LED, 256u - (age - 70u) * 216u / (PH_MS - 70u));
        else if (ui.gs == GS_OVER)
            pad_col(pad, C_ERR_LED, fl < 500u ? 256u : (now / 300u) & 1u ? 110u : 40u);
        else {
            uint32_t ph = (now / 8u) & 255u;
            pad_col(pad, C_BLU_LED, ph < 128u ? 30u + ph / 2u : 30u + (255u - ph) / 2u);
        }
    }
    fm1_led_key(B_LEFT, ui.gs == GS_TITLE || ui.gs == GS_SETUP || ui.gs == GS_NAME);
    fm1_led_key(B_RIGHT, ui.gs == GS_TITLE || ui.gs == GS_SETUP || ui.gs == GS_NAME);
    fm1_led_key(B_PLAY, ui.gs == GS_TITLE ? (now / 500u) & 1u : menu);   /* (the title: blinking; elsewhere: home) */
    fm1_led_key(B_GLOBE, ui.gs == GS_SETUP);
    fm1_led_key(B_PATCH, ui.gs == GS_SOUND);
    fm1_led_key(B_PARA, ui.gs == GS_VECTOR);
    fm1_led_key(B_OCTDN, ui.gs == GS_SETUP || ui.gs == GS_NAME);
    fm1_led_key(B_OCTUP, ui.gs == GS_SETUP || ui.gs == GS_NAME);
}

static void ui_init(void)
{
    set_load();
    lb_load();
    vec_load();
    rack_load();
    ui.fad_ready = 0;                              /* (the faders where they are at power-on: the rack's values stay) */
    preset_load(SND[ui.set[SET_SOUND]]);
    enter(GS_TITLE);
}

static void ui_input(void)
{
    pads_poll();
    keys_poll();
    buttons_poll();
    knobs_poll();
    faders_poll();
    wheels_poll();
    game_tick();
}
