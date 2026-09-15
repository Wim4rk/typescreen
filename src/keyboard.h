/**
 * @file keyboard.h
 * @brief An evdev keyboard with a loadable layout (see layouts/).
 *
 * A layout file has one line per key: the key's name (its Linux
 * KEY_ name without the prefix, or its number) followed by up to four
 * tokens for the plain, shift, AltGr and AltGr+shift levels. A token is
 * a single character, U+XXXX, `space` or `none`; prefix an accent with
 * `dead:` to make it a dead key. Missing levels fall back to the nearest
 * defined one. Modifiers, Enter, Backspace, Escape and F1-F12 are fixed.
 */
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>
#include <linux/input.h>

typedef enum {
    KP_NONE,       /**< Modifier, key release, or unmapped key. */
    KP_CHAR,       /**< cp is a printable codepoint. */
    KP_DEAD,       /**< cp is an accent to combine with the next letter. */
    KP_NEWLINE,
    KP_BACKSPACE,
    KP_ESCAPE,
    KP_FUNCTION,   /**< fn is 1..12. */
} keypress_kind;

/** One translated key press. */
typedef struct {
    keypress_kind kind;
    uint32_t cp;
    int fn;
    bool ctrl;     /**< A Ctrl key was held. */
} keypress_t;

/** Levels: 0 plain, 1 shift, 2 altgr, 3 altgr+shift. */
typedef struct {
    uint32_t cp[KEY_MAX + 1][4];
    bool dead[KEY_MAX + 1][4];
} layout_t;

/** @return 0, or -1 if the file cannot be opened. Bad lines are reported and skipped. */
int keyboard_load_layout(layout_t *l, const char *path);

/** @brief Open the device non-blocking. @return The fd, or -1. */
int keyboard_open(const char *device);

/** @brief Translate one event. Modifier state is kept between calls. */
keypress_t keyboard_translate(const layout_t *l, const struct input_event *ev);

#endif
