/**
 * @file main.c
 * @brief The reference frontend: an event loop, key bindings, documents
 *        in one directory, and the F3 screen for choosing one.
 *
 * Everything about typing itself is in typewriter.c. A different
 * frontend replaces this file and keeps the rest.
 */
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "canvas.h"
#include "config.h"
#include "display.h"
#include "doc.h"
#include "files.h"
#include "font.h"
#include "keyboard.h"
#include "page.h"
#include "typewriter.h"
#include "utf8.h"

#define FN_SAVE    2
#define FN_OPEN    3
#define FN_REDRAW  5
#define FN_QUIT    10

#define PROMPT_MAX 128

/* Idle time before the prompt is shown. */
#define PROMPT_DELAY_MS 900

static volatile sig_atomic_t stop_requested = 0;

static void on_signal(int sig) {
    (void)sig;
    stop_requested = 1;
}

typedef enum { MODE_TYPE, MODE_OPEN } app_mode;

typedef struct {
    config_t cfg;
    display_t dev;
    font_t font;
    canvas_t canvas;
    page_t page;
    doc_t doc;
    typewriter_t tw;

    char name[FILES_NAME_MAX];   /* current document, within cfg.directory */

    app_mode mode;
    file_list_t list;
    uint32_t prompt[PROMPT_MAX];
    int prompt_len;
} app_t;

/* ---- documents ---- */

static void save(app_t *a) {
    char path[1024];
    if (files_path(path, sizeof(path), a->cfg.directory, a->name) != 0 ||
        doc_save(&a->doc, path) != 0) {
        fprintf(stderr, "save: cannot write %s/%s: %s\n", a->cfg.directory, a->name, strerror(errno));
    }
}

static int open_document(app_t *a, const char *name) {
    char path[1024];
    if (files_path(path, sizeof(path), a->cfg.directory, name) != 0) return -1;
    if (doc_load(&a->doc, path) != 0) {
        fprintf(stderr, "cannot read %s: %s\n", path, strerror(errno));
        return -1;
    }
    snprintf(a->name, sizeof(a->name), "%s", name);
    typewriter_layout(&a->tw);
    printf("document: %s (%zu characters)\n", path, a->doc.len);
    return 0;
}

/* ---- the open screen ---- */

static void page_puts(page_t *p, const char *s) {
    while (*s) page_put(p, utf8_next(&s));
}

static void open_begin(app_t *a) {
    typewriter_flush(&a->tw);
    save(a);

    if (files_list(&a->list, a->cfg.directory) != 0) {
        fprintf(stderr, "cannot list %s: %s\n", a->cfg.directory, strerror(errno));
        return;
    }

    page_t *p = &a->page;
    page_clear(p);
    page_puts(p, "Open a document: type its number, or a new name.\n");
    page_puts(p, "Enter to open, Esc to go back.\n\n");

    int shown = p->rows > 5 ? p->rows - 5 : 0;
    if (shown > a->list.count) shown = a->list.count;
    for (int i = 0; i < shown; i++) {
        char line[FILES_NAME_MAX + 8];
        snprintf(line, sizeof(line), "%2d  %s\n", i + 1, a->list.name[i]);
        page_puts(p, line);
    }
    page_puts(p, "\n> ");

    a->prompt_len = 0;
    a->mode = MODE_OPEN;
    canvas_draw_page(&a->canvas, p, DISPLAY_FAST);
}

static void open_end(app_t *a, const char *name) {
    a->mode = MODE_TYPE;
    if (!name || open_document(a, name) != 0) typewriter_layout(&a->tw);
    display_wait();
    canvas_draw_page(&a->canvas, &a->page, DISPLAY_CLEAN);
}

static void open_choose(app_t *a) {
    char name[FILES_NAME_MAX];
    if (utf8_encode(name, sizeof(name), a->prompt, (size_t)a->prompt_len) < 0) return;

    char *end;
    long n = strtol(name, &end, 10);
    if (name[0] && *end == '\0') {
        if (n >= 1 && n <= a->list.count) open_end(a, a->list.name[n - 1]);
        return;
    }
    if (files_valid_name(name)) open_end(a, name);
}

static void open_key(app_t *a, keypress_t k) {
    page_change_t ch;
    switch (k.kind) {
    case KP_CHAR:
    case KP_DEAD:
        if (a->prompt_len < PROMPT_MAX) {
            a->prompt[a->prompt_len++] = k.cp;
            ch = page_put(&a->page, k.cp);
            if (ch.kind == PAGE_CELL) canvas_draw_cells(&a->canvas, &a->page, ch.row, ch.col, ch.col);
            else if (ch.kind != PAGE_NONE) canvas_draw_page(&a->canvas, &a->page, DISPLAY_FAST);
        }
        break;
    case KP_BACKSPACE:
        if (a->prompt_len > 0) {
            a->prompt_len--;
            ch = page_backspace(&a->page);
            if (ch.kind == PAGE_CELL) canvas_draw_cells(&a->canvas, &a->page, ch.row, ch.col, ch.col);
        }
        break;
    case KP_NEWLINE:
        open_choose(a);
        break;
    case KP_ESCAPE:
        open_end(a, NULL);
        break;
    case KP_FUNCTION:
        if (k.fn == FN_OPEN) open_end(a, NULL);
        else if (k.fn == FN_QUIT) { open_end(a, NULL); stop_requested = 1; }
        break;
    case KP_NONE:
        break;
    }
}

/* ---- key bindings ---- */

static void handle(app_t *a, keypress_t k, bool more_waiting) {
    if (a->mode == MODE_OPEN) {
        open_key(a, k);
        return;
    }

    switch (k.kind) {
    case KP_NONE:
    case KP_ESCAPE:
        break;
    case KP_CHAR:
        typewriter_type(&a->tw, k.cp, more_waiting);
        break;
    case KP_DEAD:
        typewriter_dead(&a->tw, k.cp);
        break;
    case KP_NEWLINE:
        typewriter_newline(&a->tw);
        save(a);
        break;
    case KP_BACKSPACE:
        typewriter_backspace(&a->tw);
        break;
    case KP_FUNCTION:
        typewriter_hide_prompt(&a->tw);
        typewriter_flush(&a->tw);
        if (k.fn == FN_SAVE) {
            save(a);
        } else if (k.fn == FN_OPEN) {
            open_begin(a);
        } else if (k.fn == FN_REDRAW) {
            display_wait();
            canvas_draw_page(&a->canvas, &a->page, DISPLAY_CLEAN);
        } else if (k.fn == FN_QUIT) {
            stop_requested = 1;
        }
        break;
    }
}

int main(int argc, char **argv) {
    const char *conf = argc > 1 ? argv[1] : "typescreen.conf";
    app_t *a = calloc(1, sizeof(app_t));
    if (!a) return 1;

    config_defaults(&a->cfg);
    if (config_load(&a->cfg, conf) != 0) return 1;

    layout_t *layout = malloc(sizeof(layout_t));
    if (!layout || keyboard_load_layout(layout, a->cfg.layout) != 0) return 1;

    int kb = keyboard_open(a->cfg.keyboard);
    if (kb < 0) return 1;

    if (display_open(&a->dev, (uint16_t)a->cfg.vcom_mv) != 0) return 1;
    if (font_load(&a->font, a->cfg.font, a->cfg.rotation) != 0) return 1;
    if (canvas_init(&a->canvas, &a->dev, &a->font, a->cfg.rotation,
                    a->cfg.margin_left, a->cfg.margin_right,
                    a->cfg.margin_top, a->cfg.margin_bottom,
                    a->cfg.line_spacing) != 0) return 1;
    if (page_init(&a->page, a->canvas.cols, a->canvas.rows, a->cfg.keep_rows) != 0) return 1;
    doc_init(&a->doc);
    typewriter_init(&a->tw, &a->canvas, &a->page, &a->doc);

    /* Continue with the most recently written document, or start a new one. */
    char name[FILES_NAME_MAX];
    if (files_newest(name, sizeof(name), a->cfg.directory) != 0) {
        fprintf(stderr, "cannot list %s: %s\n", a->cfg.directory, strerror(errno));
        return 1;
    }
    if (name[0] == '\0') files_date_name(name, sizeof(name));
    if (open_document(a, name) != 0) return 1;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    display_clear(&a->dev, DISPLAY_INIT);
    canvas_draw_page(&a->canvas, &a->page, DISPLAY_CLEAN);

    struct pollfd pfd = { kb, POLLIN, 0 };
    struct input_event evs[64];

    while (!stop_requested) {
        int ready = poll(&pfd, 1, PROMPT_DELAY_MS);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }
        if (ready == 0) {
            if (a->mode == MODE_TYPE) typewriter_show_prompt(&a->tw);
            continue;
        }
        ssize_t n = read(kb, evs, sizeof(evs));
        if (n < 0) {
            if (errno == EAGAIN || errno == EINTR) continue;
            perror("keyboard");
            break;
        }
        int count = (int)(n / sizeof(struct input_event));
        for (int i = 0; i < count && !stop_requested; i++) {
            keypress_t k = keyboard_translate(layout, &evs[i]);
            if (k.kind == KP_NONE) continue;
            bool more = (i < count - 1) || poll(&pfd, 1, 0) > 0;
            handle(a, k, more);
        }
    }

    if (a->mode == MODE_OPEN) typewriter_layout(&a->tw);
    typewriter_flush(&a->tw);
    save(a);

    close(kb);
    display_close();
    page_free(&a->page);
    canvas_free(&a->canvas);
    font_free(&a->font);
    doc_free(&a->doc);
    free(layout);
    free(a);
    return 0;
}
