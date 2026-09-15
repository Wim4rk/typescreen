/**
 * @file display.h
 * @brief The e-paper panel: a frame buffer of 8-bit pixels and a way to
 *        refresh part of it.
 *
 * This is the only layer that knows the controller. To support another
 * controller, implement these functions (see it8951.c). Everything above
 * uses panel coordinates as the controller reports them; rotation is
 * handled in canvas.c.
 *
 * Pixels are one byte each, 0x00 black .. 0xF0 white. Only the upper
 * nibble is significant to the IT8951, and glyph tables use exactly
 * these two values.
 */
#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

/** Refresh modes, in order of speed. */
typedef enum {
    DISPLAY_FAST,   /**< Black and white, no flash. For typing. */
    DISPLAY_CLEAN,  /**< Grey levels, slow, removes most ghosting. */
    DISPLAY_INIT,   /**< Full clear with flashing. Removes all ghosting. */
} display_mode;

/** An open panel. */
typedef struct {
    uint16_t width;      /**< Panel width in pixels, as reported by the controller. */
    uint16_t height;     /**< Panel height in pixels. */
    uint32_t mem_addr;   /**< Controller frame buffer address (controller-specific). */
} display_t;

/**
 * @brief Initialise the hardware and read the panel's size.
 * @param d      Filled in on success.
 * @param vcom_mv Panel VCOM in millivolts (the value printed on the cable, without sign).
 * @return 0 on success, -1 on failure (message on stderr).
 */
int display_open(display_t *d, uint16_t vcom_mv);

/** @brief Release the hardware. */
void display_close(void);

/**
 * @brief Write pixels into the frame buffer without refreshing the panel.
 * @param d      The display.
 * @param pixels w*h bytes, row-major.
 * @param x,y    Top-left corner, panel coordinates.
 * @param w,h    Size in pixels.
 */
void display_load(const display_t *d, const uint8_t *pixels, int x, int y, int w, int h);

/** @brief Refresh a rectangle of the panel from the frame buffer. */
void display_refresh(int x, int y, int w, int h, display_mode mode);

/** @brief display_load followed by display_refresh. */
void display_blit(const display_t *d, const uint8_t *pixels,
                  int x, int y, int w, int h, display_mode mode);

/**
 * @brief Block until the previous refresh has finished.
 *
 * Fast refreshes of different areas may overlap; call this before a
 * refresh that covers an area still being drawn, and before any
 * DISPLAY_CLEAN or DISPLAY_INIT refresh.
 */
void display_wait(void);

/** @brief Fill the whole panel with white. */
void display_clear(const display_t *d, display_mode mode);

#endif
