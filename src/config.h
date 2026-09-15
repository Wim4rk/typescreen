/**
 * @file config.h
 * @brief The configuration file: `key = value` lines, `#` comments.
 *        See typescreen.conf for the keys.
 */
#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    int vcom_mv;            /**< Panel VCOM in millivolts, positive. */
    int rotation;           /**< 0, 90, 180 or 270. */
    /** Margins in cells (columns for left/right, rows for top/bottom),
     *  line spacing as a fraction of the cell height. Converted to pixels
     *  once the font is known, so the layout follows the font size. */
    double margin_left, margin_right, margin_top, margin_bottom;
    double line_spacing;
    int keep_rows;          /**< Rows kept at the top when the page scrolls. */
    char font[512];
    char keyboard[512];
    char layout[512];
    char directory[512];    /**< Expanded: a leading ~ becomes $HOME. */
} config_t;

void config_defaults(config_t *c);

/**
 * @brief Read a file over the defaults and validate.
 *
 * `vcom` accepts volts as printed on the panel ("-2.14") or millivolts.
 * @return 0, or -1 after reporting every problem on stderr.
 */
int  config_load(config_t *c, const char *path);

#endif
