/**
 * @file it8951.c
 * @brief display.h implemented for the IT8951 controller over SPI.
 *
 * Derived from Waveshare's EPD_IT8951.c (MIT). See LICENSE.md.
 */
#include "display.h"
#include "hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CMD_SYS_RUN      0x0001
#define CMD_REG_RD       0x0010
#define CMD_REG_WR       0x0011
#define CMD_LD_IMG_AREA  0x0021
#define CMD_LD_IMG_END   0x0022
#define CMD_DPY_AREA     0x0034
#define CMD_GET_DEV_INFO 0x0302
#define CMD_VCOM         0x0039

#define REG_I80CPCR  0x0004   /* packed pixel write enable */
#define REG_LISAR    0x0208   /* load image start address, low word */
#define REG_LUTAFSR  0x1224   /* non-zero while a refresh is running */

#define PREAMBLE_CMD   0x6000
#define PREAMBLE_WRITE 0x0000
#define PREAMBLE_READ  0x1000

#define ENDIAN_BIG  1
#define FORMAT_8BPP 3
#define ROTATE_0    0

/* Waveshare's mode numbers. A2's number depends on the firmware's LUT;
 * 6 is what the boards ship with. */
#define MODE_INIT 0
#define MODE_GC16 2
#define MODE_A2   6

static uint16_t mode_number(display_mode mode) {
    switch (mode) {
    case DISPLAY_INIT:  return MODE_INIT;
    case DISPLAY_CLEAN: return MODE_GC16;
    default:            return MODE_A2;
    }
}

static void wait_busy(void) {
    while (hal_gpio_read(PIN_BUSY) == 0) {}
}

static void write_word(uint16_t v) {
    hal_spi_write_byte(v >> 8);
    hal_spi_write_byte(v & 0xFF);
}

static uint16_t read_word(void) {
    uint16_t v = (uint16_t)hal_spi_read_byte() << 8;
    v |= hal_spi_read_byte();
    return v;
}

static void write_command(uint16_t cmd) {
    wait_busy();
    hal_gpio_write(PIN_CS, 0);
    write_word(PREAMBLE_CMD);
    wait_busy();
    write_word(cmd);
    hal_gpio_write(PIN_CS, 1);
}

static void write_data(uint16_t data) {
    wait_busy();
    hal_gpio_write(PIN_CS, 0);
    write_word(PREAMBLE_WRITE);
    wait_busy();
    write_word(data);
    hal_gpio_write(PIN_CS, 1);
}

static void read_data(uint16_t *buf, int count) {
    wait_busy();
    hal_gpio_write(PIN_CS, 0);
    write_word(PREAMBLE_READ);
    wait_busy();
    read_word();   /* dummy word */
    wait_busy();
    for (int i = 0; i < count; i++) buf[i] = read_word();
    hal_gpio_write(PIN_CS, 1);
}

static void write_args(uint16_t cmd, const uint16_t *args, int count) {
    write_command(cmd);
    for (int i = 0; i < count; i++) write_data(args[i]);
}

static uint16_t read_reg(uint16_t addr) {
    uint16_t v;
    write_command(CMD_REG_RD);
    write_data(addr);
    read_data(&v, 1);
    return v;
}

static void write_reg(uint16_t addr, uint16_t value) {
    write_command(CMD_REG_WR);
    write_data(addr);
    write_data(value);
}

static uint16_t get_vcom(void) {
    uint16_t v;
    write_command(CMD_VCOM);
    write_data(0x0000);
    read_data(&v, 1);
    return v;
}

static void set_vcom(uint16_t vcom) {
    write_command(CMD_VCOM);
    write_data(0x0001);
    write_data(vcom);
}

static void reset(void) {
    hal_gpio_write(PIN_RST, 1);
    hal_delay_ms(200);
    hal_gpio_write(PIN_RST, 0);
    hal_delay_ms(10);
    hal_gpio_write(PIN_RST, 1);
    hal_delay_ms(200);
}

int display_open(display_t *dev, uint16_t vcom_mv) {
    if (hal_init() != 0) return -1;

    reset();
    write_command(CMD_SYS_RUN);

    /* Device info: width, height, memory address (low, high), then
     * firmware and LUT version strings, 16 bytes each. */
    uint16_t info[20];
    write_command(CMD_GET_DEV_INFO);
    read_data(info, 20);

    dev->width = info[0];
    dev->height = info[1];
    dev->mem_addr = ((uint32_t)info[3] << 16) | info[2];

    if (dev->width == 0 || dev->height == 0 || dev->width > 4096 || dev->height > 4096) {
        fprintf(stderr, "IT8951: no valid device info (panel %ux%u)\n", dev->width, dev->height);
        return -1;
    }

    write_reg(REG_I80CPCR, 0x0001);

    if (vcom_mv != get_vcom()) set_vcom(vcom_mv);

    printf("IT8951: panel %ux%u, firmware %.16s, LUT %.16s, VCOM -%.2f V\n",
           dev->width, dev->height, (const char *)&info[4], (const char *)&info[12],
           get_vcom() / 1000.0);
    return 0;
}

void display_close(void) {
    hal_close();
}

void display_load(const display_t *dev, const uint8_t *pixels, int x, int y, int w, int h) {
    write_reg(REG_LISAR + 2, (uint16_t)(dev->mem_addr >> 16));
    write_reg(REG_LISAR, (uint16_t)(dev->mem_addr & 0xFFFF));

    uint16_t args[5] = {
        (ENDIAN_BIG << 8) | (FORMAT_8BPP << 4) | ROTATE_0,
        (uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h,
    };
    write_args(CMD_LD_IMG_AREA, args, 5);

    wait_busy();
    hal_gpio_write(PIN_CS, 0);
    write_word(PREAMBLE_WRITE);
    wait_busy();
    hal_spi_write(pixels, (uint32_t)w * (uint32_t)h);
    hal_gpio_write(PIN_CS, 1);

    write_command(CMD_LD_IMG_END);
}

void display_refresh(int x, int y, int w, int h, display_mode mode) {
    uint16_t args[5] = { (uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h, mode_number(mode) };
    write_args(CMD_DPY_AREA, args, 5);
}

void display_blit(const display_t *dev, const uint8_t *pixels,
                  int x, int y, int w, int h, display_mode mode) {
    display_load(dev, pixels, x, y, w, h);
    display_refresh(x, y, w, h, mode);
}

void display_wait(void) {
    while (read_reg(REG_LUTAFSR) != 0) {}
}

void display_clear(const display_t *dev, display_mode mode) {
    size_t n = (size_t)dev->width * dev->height;
    uint8_t *white = malloc(n);
    if (!white) return;
    memset(white, 0xF0, n);
    display_wait();
    display_blit(dev, white, 0, 0, dev->width, dev->height, mode);
    free(white);
}
