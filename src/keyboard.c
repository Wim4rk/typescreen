#include "keyboard.h"
#include "utf8.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Key names accepted in layout files, without the KEY_ prefix. Any key
 * can also be given by its number. */
static const struct { const char *name; int code; } key_names[] = {
    {"A",KEY_A},{"B",KEY_B},{"C",KEY_C},{"D",KEY_D},{"E",KEY_E},{"F",KEY_F},{"G",KEY_G},
    {"H",KEY_H},{"I",KEY_I},{"J",KEY_J},{"K",KEY_K},{"L",KEY_L},{"M",KEY_M},{"N",KEY_N},
    {"O",KEY_O},{"P",KEY_P},{"Q",KEY_Q},{"R",KEY_R},{"S",KEY_S},{"T",KEY_T},{"U",KEY_U},
    {"V",KEY_V},{"W",KEY_W},{"X",KEY_X},{"Y",KEY_Y},{"Z",KEY_Z},
    {"1",KEY_1},{"2",KEY_2},{"3",KEY_3},{"4",KEY_4},{"5",KEY_5},
    {"6",KEY_6},{"7",KEY_7},{"8",KEY_8},{"9",KEY_9},{"0",KEY_0},
    {"MINUS",KEY_MINUS},{"EQUAL",KEY_EQUAL},{"LEFTBRACE",KEY_LEFTBRACE},
    {"RIGHTBRACE",KEY_RIGHTBRACE},{"SEMICOLON",KEY_SEMICOLON},{"APOSTROPHE",KEY_APOSTROPHE},
    {"GRAVE",KEY_GRAVE},{"BACKSLASH",KEY_BACKSLASH},{"COMMA",KEY_COMMA},{"DOT",KEY_DOT},
    {"SLASH",KEY_SLASH},{"SPACE",KEY_SPACE},{"TAB",KEY_TAB},{"102ND",KEY_102ND},
    {"KPASTERISK",KEY_KPASTERISK},{"KPMINUS",KEY_KPMINUS},{"KPPLUS",KEY_KPPLUS},
    {"KPDOT",KEY_KPDOT},{"KPSLASH",KEY_KPSLASH},
    {"KP0",KEY_KP0},{"KP1",KEY_KP1},{"KP2",KEY_KP2},{"KP3",KEY_KP3},{"KP4",KEY_KP4},
    {"KP5",KEY_KP5},{"KP6",KEY_KP6},{"KP7",KEY_KP7},{"KP8",KEY_KP8},{"KP9",KEY_KP9},
};

static int key_code(const char *name) {
    for (size_t i = 0; i < sizeof(key_names) / sizeof(key_names[0]); i++) {
        if (strcmp(key_names[i].name, name) == 0) return key_names[i].code;
    }
    char *end;
    long n = strtol(name, &end, 10);
    if (*end == '\0' && n > 0 && n <= KEY_MAX) return (int)n;
    return -1;
}

/* A token is a single character, U+XXXX, `space`, `none`, or any of
 * these prefixed with `dead:`. */
static int parse_token(const char *tok, uint32_t *cp, bool *dead) {
    *dead = false;
    if (strncmp(tok, "dead:", 5) == 0) {
        *dead = true;
        tok += 5;
    }
    if (strcmp(tok, "none") == 0 || strcmp(tok, "-") == 0) { *cp = 0; return 0; }
    if (strcmp(tok, "space") == 0) { *cp = ' '; return 0; }
    if ((tok[0] == 'U' || tok[0] == 'u') && tok[1] == '+') {
        char *end;
        *cp = (uint32_t)strtoul(tok + 2, &end, 16);
        return *end == '\0' ? 0 : -1;
    }
    const char *s = tok;
    *cp = utf8_next(&s);
    return *s == '\0' ? 0 : -1;
}

int keyboard_load_layout(layout_t *l, const char *path) {
    memset(l, 0, sizeof(*l));

    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "layout: cannot open %s\n", path);
        return -1;
    }

    char line[256];
    int lineno = 0;
    while (fgets(line, sizeof(line), fp)) {
        lineno++;
        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';

        char *tok = strtok(line, " \t\r\n");
        if (!tok) continue;

        int code = key_code(tok);
        if (code < 0) {
            fprintf(stderr, "layout: %s:%d: unknown key %s\n", path, lineno, tok);
            continue;
        }

        for (int level = 0; level < 4 && (tok = strtok(NULL, " \t\r\n")); level++) {
            if (parse_token(tok, &l->cp[code][level], &l->dead[code][level]) != 0) {
                fprintf(stderr, "layout: %s:%d: bad token %s\n", path, lineno, tok);
            }
        }
    }
    fclose(fp);
    return 0;
}

int keyboard_open(const char *device) {
    int fd = open(device, O_RDONLY | O_NONBLOCK);
    if (fd < 0) fprintf(stderr, "keyboard: cannot open %s\n", device);
    return fd;
}

static bool shift_l, shift_r, ctrl_l, ctrl_r, altgr, caps;

static bool is_letter(uint32_t cp) {
    return (cp >= 'a' && cp <= 'z') || cp >= 0xC0;
}

keypress_t keyboard_translate(const layout_t *l, const struct input_event *ev) {
    keypress_t k = { KP_NONE, 0, 0, false };
    if (ev->type != EV_KEY) return k;

    bool down = ev->value != 0;   /* press or repeat */

    switch (ev->code) {
    case KEY_LEFTSHIFT:  shift_l = down; return k;
    case KEY_RIGHTSHIFT: shift_r = down; return k;
    case KEY_LEFTCTRL:   ctrl_l = down;  return k;
    case KEY_RIGHTCTRL:  ctrl_r = down;  return k;
    case KEY_RIGHTALT:   altgr = down;   return k;
    case KEY_CAPSLOCK:   if (ev->value == 1) caps = !caps; return k;
    }

    if (!down) return k;
    k.ctrl = ctrl_l || ctrl_r;

    if (ev->code >= KEY_F1 && ev->code <= KEY_F10) {
        k.kind = KP_FUNCTION;
        k.fn = ev->code - KEY_F1 + 1;
        return k;
    }
    if (ev->code == KEY_F11 || ev->code == KEY_F12) {
        k.kind = KP_FUNCTION;
        k.fn = ev->code - KEY_F11 + 11;
        return k;
    }
    if (ev->code == KEY_ENTER || ev->code == KEY_KPENTER) { k.kind = KP_NEWLINE; return k; }
    if (ev->code == KEY_BACKSPACE) { k.kind = KP_BACKSPACE; return k; }
    if (ev->code == KEY_ESC) { k.kind = KP_ESCAPE; return k; }
    if (ev->code > KEY_MAX) return k;

    bool shift = shift_l || shift_r;
    if (caps && is_letter(l->cp[ev->code][0]) && l->cp[ev->code][1] != l->cp[ev->code][0]) {
        shift = !shift;
    }

    /* Fall back to the nearest defined level. */
    int level = altgr ? (shift ? 3 : 2) : (shift ? 1 : 0);
    while (level > 0 && l->cp[ev->code][level] == 0) {
        level = (level == 3) ? 2 : (level == 2) ? (shift ? 1 : 0) : 0;
    }

    uint32_t cp = l->cp[ev->code][level];
    if (cp == 0) return k;

    k.kind = l->dead[ev->code][level] ? KP_DEAD : KP_CHAR;
    k.cp = cp;
    return k;
}
