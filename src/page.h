/**
 * @file page.h
 * @brief The page: a grid of codepoints with a cursor, word wrap and
 *        scrolling.
 *
 * Text only goes in at the cursor, and the cursor only moves forward
 * (or back one cell on backspace). When the cursor runs off the last
 * row, the page scrolls so that keep_rows rows stay visible at the top.
 * Every operation reports what changed, so the caller redraws only that.
 *
 * The page knows nothing about pixels; canvas.c draws it.
 */
#ifndef PAGE_H
#define PAGE_H

#include <stdint.h>
#include <stdbool.h>

/** What a page operation changed. */
typedef enum {
    PAGE_NONE,   /**< Nothing visible. */
    PAGE_CELL,   /**< One cell: row, col. */
    PAGE_ROWS,   /**< Rows row0..row1 inclusive. */
    PAGE_ALL,    /**< The page scrolled. */
} page_change_kind;

typedef struct {
    page_change_kind kind;
    int row, col;
    int row0, row1;
} page_change_t;

typedef struct {
    int cols, rows;
    int keep_rows;
    uint32_t *cells;    /**< (rows + 1) * cols; 0 is empty. The extra row
                             holds a wrapped word until the page scrolls. */
    int cur_row, cur_col;
    bool after_wrap;    /**< A space typed right after an automatic wrap is dropped. */
} page_t;

/**
 * @brief Allocate an empty page.
 * @param p         The page to initialise.
 * @param cols      Columns.
 * @param rows      Rows.
 * @param keep_rows Rows kept at the top when the page scrolls.
 * @return 0, or -1 on allocation failure.
 */
int  page_init(page_t *p, int cols, int rows, int keep_rows);
void page_free(page_t *p);
void page_clear(page_t *p);

/**
 * @brief Put a codepoint at the cursor and advance.
 * @param p  The page.
 * @param cp A printable codepoint or '\n'.
 */
page_change_t page_put(page_t *p, uint32_t cp);

/**
 * @brief Clear the cell before the cursor.
 *
 * Does nothing at column 0 — the caller re-lays out the page from the
 * document in that case (see typewriter_relayout).
 */
page_change_t page_backspace(page_t *p);

/** @brief The codepoint in a cell, 0 if empty. */
static inline uint32_t page_cell(const page_t *p, int row, int col) {
    return p->cells[row * p->cols + col];
}

#endif
