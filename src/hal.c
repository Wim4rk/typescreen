/* Derived from Waveshare's DEV_Config.c (MIT). See LICENSE.md. */
#include "hal.h"
#include <stdio.h>

#if defined(BCM)
#include <bcm2835.h>

int hal_init(void) {
    if (!bcm2835_init()) {
        fprintf(stderr, "bcm2835_init failed (run as root?)\n");
        return -1;
    }
    bcm2835_spi_begin();
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_32);
    bcm2835_gpio_fsel(PIN_RST, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(PIN_CS, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(PIN_BUSY, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_write(PIN_CS, HIGH);
    return 0;
}

void hal_close(void) {
    bcm2835_gpio_write(PIN_CS, LOW);
    bcm2835_gpio_write(PIN_RST, LOW);
    bcm2835_spi_end();
    bcm2835_close();
}

void hal_gpio_write(int pin, int value) { bcm2835_gpio_write(pin, value); }
int  hal_gpio_read(int pin)             { return bcm2835_gpio_lev(pin); }
void hal_delay_ms(uint32_t ms)          { bcm2835_delay(ms); }

void    hal_spi_write_byte(uint8_t v)   { bcm2835_spi_transfer(v); }
uint8_t hal_spi_read_byte(void)         { return bcm2835_spi_transfer(0x00); }

void hal_spi_write(const uint8_t *data, uint32_t len) {
    bcm2835_spi_writenb((const char *)data, len);
}

#elif defined(LGPIO)
#include <lgpio.h>

static int gpio_handle = -1;
static int spi_handle = -1;

int hal_init(void) {
    /* Pi 5 exposes the header on gpiochip4, older boards on gpiochip0. */
    gpio_handle = lgGpiochipOpen(4);
    if (gpio_handle < 0) gpio_handle = lgGpiochipOpen(0);
    if (gpio_handle < 0) {
        fprintf(stderr, "lgGpiochipOpen failed\n");
        return -1;
    }
    spi_handle = lgSpiOpen(0, 0, 12500000, 0);
    if (spi_handle < 0) {
        fprintf(stderr, "lgSpiOpen failed\n");
        return -1;
    }
    lgGpioClaimInput(gpio_handle, 0, PIN_BUSY);
    lgGpioClaimOutput(gpio_handle, 0, PIN_RST, 0);
    lgGpioClaimOutput(gpio_handle, 0, PIN_CS, 1);
    return 0;
}

void hal_close(void) {
    lgGpioWrite(gpio_handle, PIN_CS, 0);
    lgGpioWrite(gpio_handle, PIN_RST, 0);
    lgSpiClose(spi_handle);
    lgGpiochipClose(gpio_handle);
}

void hal_gpio_write(int pin, int value) { lgGpioWrite(gpio_handle, pin, value); }
int  hal_gpio_read(int pin)             { return lgGpioRead(gpio_handle, pin); }
void hal_delay_ms(uint32_t ms)          { lguSleep(ms / 1000.0); }

void hal_spi_write_byte(uint8_t v) { lgSpiWrite(spi_handle, (const char *)&v, 1); }

uint8_t hal_spi_read_byte(void) {
    char v = 0;
    lgSpiRead(spi_handle, &v, 1);
    return (uint8_t)v;
}

void hal_spi_write(const uint8_t *data, uint32_t len) {
    /* spidev rejects transfers larger than its buffer (4096 by default). */
    const uint32_t chunk = 4096;
    while (len > 0) {
        uint32_t n = len < chunk ? len : chunk;
        lgSpiWrite(spi_handle, (const char *)data, n);
        data += n;
        len -= n;
    }
}

#else
#error "Select a backend: -DBCM or -DLGPIO"
#endif
