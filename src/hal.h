/**
 * @file hal.h
 * @brief GPIO and SPI access for the IT8951 board.
 *
 * One backend is selected at build time:
 *  - `-DBCM`   bcm2835 library (Pi Zero .. Pi 4, needs root)
 *  - `-DLGPIO` lgpio library (Pi 5 and newer kernels)
 */
#ifndef HAL_H
#define HAL_H

#include <stdint.h>

#define PIN_RST  17
#define PIN_CS   8
#define PIN_BUSY 24

int  hal_init(void);
void hal_close(void);

void    hal_gpio_write(int pin, int value);
int     hal_gpio_read(int pin);
void    hal_delay_ms(uint32_t ms);

void    hal_spi_write_byte(uint8_t value);
uint8_t hal_spi_read_byte(void);
/** @brief Bulk write; the fast path for pixel data. */
void    hal_spi_write(const uint8_t *data, uint32_t len);

#endif
