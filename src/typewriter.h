/**
 * @file typewriter.h
 * @brief The typewriter: keystrokes in, cells on the panel out.
 *
 * Owns none of its parts. The caller provides a canvas, a page and a
 * document, and may replace the document (to open another file) as long
 * as it calls typewriter_layout() afterwards.
 *
 * What happens here, and nowhere else:
 *  - a character goes into the document and the page, and the changed
 *    cells go to the panel;
 *  - bursts of keystrokes are collected into one update per row
 *    (see typewriter_type's more_waiting);
 *  - dead keys: an accent is placed, and the next letter replaces it with
 *    the accented letter;
 *  - backspace across a line boundary rebuilds the page from the document,
 *    so the page always shows exactly what the document contains;
 *  - the prompt: a mark in the cell where the next character will land,
 *    shown when the frontend says the typist has paused.
 *
 * A frontend that wants different keys, screens or behaviour around the
 * typing keeps this and writes its own main.c.
 */
#ifndef TYPEWRITER_H
#define TYPEWRITER_H

#include <stdbool.h>
#include <stdint.h>
#include "canvas.h"
#include "doc.h"
#include "page.h"

/** When keys arrive faster than the panel draws, up to this many cells on
 *  the current row are collected and sent as one update. */
#define TYPEWRITER_SPAN_MAX 8

typedef struct {
    canvas_t *canvas;
    page_t *page;
    doc_t *doc;

    uint32_t pending_accent;   /**< A dead key waiting for its letter, or 0. */
    bool hidden_char;          /**< The last character went into the document but not the page. */
    int span_row, span_col0, span_col1;   /**< Cells placed but not yet sent; row -1 = none. */

    bool prompt_visible;
    int prompt_row, prompt_col;
} typewriter_t;

/** The prompt: what is drawn where the next character will land. */
#define TYPEWRITER_PROMPT '_'


/** @brief Bind a typewriter to its canvas, page and document. */
void typewriter_init(typewriter_t *tw, canvas_t *canvas, page_t *page, doc_t *doc);

/**
 * @brief Type a printable character.
 * @param tw The typewriter.
 * @param cp A printable codepoint.
 * @param more_waiting True if more keystrokes are already queued. The
 *        cell is then held back and sent together with the following
 *        ones, at a space, after TYPEWRITER_SPAN_MAX cells, or when the
 *        queue is empty.
 */
void typewriter_type(typewriter_t *tw, uint32_t cp, bool more_waiting);

/** @brief Type a dead key: the accent is shown and combines with the next letter. */
void typewriter_dead(typewriter_t *tw, uint32_t accent);

/** @brief End the line. */
void typewriter_newline(typewriter_t *tw);

/** @brief Remove the last character. */
void typewriter_backspace(typewriter_t *tw);

/** @brief Send any held-back cells to the panel. Call before drawing
 *         anything else on the page, and before saving on exit. */
void typewriter_flush(typewriter_t *tw);

/** @brief Rebuild the page from the document without drawing. Call after
 *         replacing the document, then draw the page yourself. */
void typewriter_layout(typewriter_t *tw);

/** @brief Rebuild the page from the document and redraw the rows that changed. */
void typewriter_relayout(typewriter_t *tw);

/**
 * @brief Show the prompt in the cursor cell.
 *
 * Call when no key has arrived for a while (wimwriter used 900 ms). Every
 * typing operation removes it again by itself, so the prompt costs
 * nothing while text is flowing.
 */
void typewriter_show_prompt(typewriter_t *tw);

/** @brief Remove the prompt, if shown. Call before drawing anything else
 *         on the page. */
void typewriter_hide_prompt(typewriter_t *tw);

#endif
