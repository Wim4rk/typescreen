#include "typewriter.h"
#include "compose.h"
#include <stdlib.h>
#include <string.h>

void typewriter_init(typewriter_t *tw, canvas_t *canvas, page_t *page, doc_t *doc) {
    memset(tw, 0, sizeof(*tw));
    tw->canvas = canvas;
    tw->page = page;
    tw->doc = doc;
    tw->span_row = -1;
}

void typewriter_flush(typewriter_t *tw) {
    if (tw->span_row < 0) return;
    canvas_draw_cells(tw->canvas, tw->page, tw->span_row, tw->span_col0, tw->span_col1);
    tw->span_row = -1;
}

static void apply(typewriter_t *tw, page_change_t ch) {
    switch (ch.kind) {
    case PAGE_NONE:
        break;
    case PAGE_CELL:
        if (tw->span_row == ch.row && ch.col == tw->span_col1 + 1) {
            tw->span_col1 = ch.col;
        } else {
            typewriter_flush(tw);
            tw->span_row = ch.row;
            tw->span_col0 = tw->span_col1 = ch.col;
        }
        break;
    case PAGE_ROWS:
        tw->span_row = -1;
        canvas_draw_rows(tw->canvas, tw->page, ch.row0, ch.row1);
        break;
    case PAGE_ALL:
        tw->span_row = -1;
        canvas_draw_page(tw->canvas, tw->page, DISPLAY_FAST);
        break;
    }
}

void typewriter_show_prompt(typewriter_t *tw) {
    page_t *p = tw->page;
    if (tw->prompt_visible || tw->span_row >= 0 || p->cur_row >= p->rows) return;

    /* The cursor cell is always empty; borrow it for one blit. */
    uint32_t *cell = &p->cells[p->cur_row * p->cols + p->cur_col];
    *cell = TYPEWRITER_PROMPT;
    canvas_draw_cells(tw->canvas, p, p->cur_row, p->cur_col, p->cur_col);
    *cell = 0;

    tw->prompt_visible = true;
    tw->prompt_row = p->cur_row;
    tw->prompt_col = p->cur_col;
}

void typewriter_hide_prompt(typewriter_t *tw) {
    if (!tw->prompt_visible) return;
    tw->prompt_visible = false;
    canvas_draw_cells(tw->canvas, tw->page, tw->prompt_row, tw->prompt_col, tw->prompt_col);
}

void typewriter_layout(typewriter_t *tw) {
    tw->prompt_visible = false;
    page_clear(tw->page);
    for (size_t i = 0; i < tw->doc->len; i++) page_put(tw->page, tw->doc->cp[i]);
    tw->hidden_char = false;
    tw->pending_accent = 0;
    tw->span_row = -1;
}

void typewriter_relayout(typewriter_t *tw) {
    page_t *p = tw->page;
    size_t n = (size_t)p->rows * p->cols;
    uint32_t *old = malloc(n * sizeof(uint32_t));
    if (old) memcpy(old, p->cells, n * sizeof(uint32_t));

    typewriter_layout(tw);

    if (!old) {
        canvas_draw_page(tw->canvas, p, DISPLAY_FAST);
        return;
    }

    int first = -1, last = -1;
    for (int r = 0; r < p->rows; r++) {
        if (memcmp(old + (size_t)r * p->cols, p->cells + (size_t)r * p->cols,
                   p->cols * sizeof(uint32_t)) != 0) {
            if (first < 0) first = r;
            last = r;
        }
    }
    free(old);
    if (first >= 0) canvas_draw_rows(tw->canvas, p, first, last);
}

static void put(typewriter_t *tw, uint32_t cp) {
    doc_push(tw->doc, cp);
    page_change_t ch = page_put(tw->page, cp);
    tw->hidden_char = (ch.kind == PAGE_NONE);
    apply(tw, ch);
}

void typewriter_type(typewriter_t *tw, uint32_t cp, bool more_waiting) {
    typewriter_hide_prompt(tw);

    if (tw->pending_accent) {
        uint32_t accent = tw->pending_accent;
        tw->pending_accent = 0;

        if (cp == ' ') return;   /* accent + space: the accent stands alone */

        uint32_t composed = compose(accent, cp);
        if (composed) {
            /* Replace the accent, already on the page, with the letter. */
            doc_pop(tw->doc);
            if (tw->page->cur_col > 0 && !tw->hidden_char) {
                page_backspace(tw->page);
                put(tw, composed);
            } else {
                doc_push(tw->doc, composed);
                typewriter_relayout(tw);
            }
            typewriter_flush(tw);
            return;
        }
    }

    put(tw, cp);

    if (!more_waiting || cp == ' ' || tw->span_col1 - tw->span_col0 + 1 >= TYPEWRITER_SPAN_MAX) {
        typewriter_flush(tw);
    }
}

void typewriter_dead(typewriter_t *tw, uint32_t accent) {
    typewriter_hide_prompt(tw);
    typewriter_flush(tw);
    tw->pending_accent = 0;
    put(tw, accent);
    typewriter_flush(tw);
    tw->pending_accent = accent;
}

void typewriter_newline(typewriter_t *tw) {
    typewriter_hide_prompt(tw);
    typewriter_flush(tw);
    tw->pending_accent = 0;
    put(tw, '\n');
}

void typewriter_backspace(typewriter_t *tw) {
    typewriter_hide_prompt(tw);
    typewriter_flush(tw);
    tw->pending_accent = 0;
    if (tw->doc->len == 0) return;

    doc_pop(tw->doc);
    if (tw->page->cur_col > 0 && !tw->hidden_char) {
        apply(tw, page_backspace(tw->page));
        typewriter_flush(tw);
    } else {
        typewriter_relayout(tw);
    }
}
