#include "page.h"
#include <stdlib.h>
#include <string.h>

static const page_change_t NONE = { PAGE_NONE, 0, 0, 0, 0 };

int page_init(page_t *p, int cols, int rows, int keep_rows) {
    memset(p, 0, sizeof(*p));
    p->cols = cols;
    p->rows = rows;
    p->keep_rows = keep_rows < rows ? keep_rows : rows - 1;
    p->cells = calloc((size_t)(rows + 1) * cols, sizeof(uint32_t));
    return p->cells ? 0 : -1;
}

void page_free(page_t *p) {
    free(p->cells);
    memset(p, 0, sizeof(*p));
}

void page_clear(page_t *p) {
    memset(p->cells, 0, (size_t)(p->rows + 1) * p->cols * sizeof(uint32_t));
    p->cur_row = 0;
    p->cur_col = 0;
    p->after_wrap = false;
}

static uint32_t *row_ptr(page_t *p, int row) {
    return p->cells + row * p->cols;
}

/* Move rows up so the cursor lands on keep_rows. */
static page_change_t scroll(page_t *p) {
    int shift = p->cur_row - p->keep_rows;
    int rows_kept = p->keep_rows + 1;   /* including the cursor row */
    memmove(row_ptr(p, 0), row_ptr(p, shift), (size_t)rows_kept * p->cols * sizeof(uint32_t));
    memset(row_ptr(p, rows_kept), 0, (size_t)(p->rows + 1 - rows_kept) * p->cols * sizeof(uint32_t));
    p->cur_row = p->keep_rows;
    page_change_t c = { PAGE_ALL, 0, 0, 0, 0 };
    return c;
}

static page_change_t wrap(page_t *p) {
    int old_row = p->cur_row;
    uint32_t *row = row_ptr(p, old_row);

    int brk = p->cols - 1;
    while (brk > 0 && row[brk] != ' ' && row[brk] != '-') brk--;

    p->cur_row++;
    p->after_wrap = true;

    if (brk > 0) {
        /* Move the word after the break to the next row. */
        int word_len = p->cols - 1 - brk;
        uint32_t *next = row_ptr(p, p->cur_row);
        memcpy(next, row + brk + 1, (size_t)word_len * sizeof(uint32_t));
        memset(row + brk + 1, 0, (size_t)word_len * sizeof(uint32_t));
        p->cur_col = word_len;
    } else {
        p->cur_col = 0;
    }

    if (p->cur_row >= p->rows) return scroll(p);

    page_change_t c = { PAGE_ROWS, 0, 0, old_row, p->cur_row };
    return c;
}

page_change_t page_put(page_t *p, uint32_t cp) {
    if (cp == '\n') {
        p->after_wrap = false;
        p->cur_row++;
        p->cur_col = 0;
        if (p->cur_row >= p->rows) return scroll(p);
        return NONE;
    }

    if (cp == ' ' && p->after_wrap && p->cur_col == 0) {
        p->after_wrap = false;
        return NONE;
    }
    p->after_wrap = false;

    p->cells[p->cur_row * p->cols + p->cur_col] = cp;
    page_change_t c = { PAGE_CELL, p->cur_row, p->cur_col, 0, 0 };
    p->cur_col++;

    if (p->cur_col >= p->cols) return wrap(p);
    return c;
}

page_change_t page_backspace(page_t *p) {
    p->after_wrap = false;
    if (p->cur_col == 0) return NONE;
    p->cur_col--;
    p->cells[p->cur_row * p->cols + p->cur_col] = 0;
    page_change_t c = { PAGE_CELL, p->cur_row, p->cur_col, 0, 0 };
    return c;
}
