/**
 * @file canvas.h
 * @brief Maps the page's cells to panel pixels and draws them.
 *
 * The page lives in the user's orientation; `rotation` (0, 90, 180, 270)
 * is how many degrees clockwise the page is turned on the panel. Rotation
 * is a coordinate mapping done once per rectangle; glyphs arrive already
 * rotated from the font, so placing one is a memcpy per bitmap row.
 *
 * All drawing goes through one panel-sized scratch buffer allocated at
 * init. Nothing is allocated while drawing.
 */
#ifndef CANVAS_H
#define CANVAS_H

#include <stdint.h>
#include "display.h"
#include "font.h"
#include "page.h"

/** A canvas: a display, a font and a layout. */
typedef struct {
    const display_t *dev;
    const font_t *font;
    int rotation;
    int page_w, page_h;         /**< Panel size in the user's orientation. */
    int margin_left, margin_top; /**< In pixels, user orientation. */
    int row_h;                  /**< Cell height plus line spacing, pixels. */
    int cols, rows;             /**< The grid that fits inside the margins. */
    uint8_t *buf;               /**< Panel-sized scratch buffer. */
} canvas_t;

/**
 * @brief Compute the layout and allocate the scratch buffer.
 *
 * Margins are in cells (left/right in columns, top/bottom in rows) and
 * line spacing is a fraction of the cell height, so the same values work
 * with any font size.
 *
 * @param c        The canvas to initialise.
 * @param dev      An open display.
 * @param font     A font loaded with the same rotation.
 * @param rotation 0, 90, 180 or 270.
 * @param margin_left   Left margin in columns.
 * @param margin_right  Right margin in columns.
 * @param margin_top    Top margin in rows.
 * @param margin_bottom Bottom margin in rows.
 * @param line_spacing  Space between rows as a fraction of the cell height.
 * @return 0 on success, -1 if the margins leave no room for text.
 */
int canvas_init(canvas_t *c, const display_t *dev, const font_t *font, int rotation,
                double margin_left, double margin_right,
                double margin_top, double margin_bottom,
                double line_spacing);

/** @brief Free the scratch buffer. */
void canvas_free(canvas_t *c);

/**
 * @brief Draw cells col0..col1 (inclusive) of one row, as one fast update.
 *
 * This is the typing path: one keystroke is one call with col0 == col1,
 * a burst of keystrokes is one call spanning them.
 */
void canvas_draw_cells(canvas_t *c, const page_t *p, int row, int col0, int col1);

/**
 * @brief Draw rows row0..row1 (inclusive) across the full page width, as
 *        one fast update. Used after a word wrap or a backspace across a
 *        line.
 */
void canvas_draw_rows(canvas_t *c, const page_t *p, int row0, int row1);

/** @brief Draw the whole page. Use DISPLAY_CLEAN to clear ghosting. */
void canvas_draw_page(canvas_t *c, const page_t *p, display_mode mode);

/**
 * @brief Draw an arbitrary pixel buffer given in the user's orientation.
 *
 * For interfaces that need more than a grid of glyphs: the buffer is
 * rotated into the panel's orientation for you. (x, y, w, h) are in the
 * user's orientation, pixels are w*h bytes row-major, 0x00 black and
 * 0xF0 white.
 */
void canvas_draw_pixels(canvas_t *c, const uint8_t *pixels,
                        int x, int y, int w, int h, display_mode mode);

#endif
