/**
 * @file    hal_spi.c
 * @brief   SPI HAL Implementation - AT-D878 Open Firmware
 * @version 0.0.1
 */

#include "hal_spi.h"
#include "hal_gpio.h"

/* SPI0 Taban Adresi: 0x40013000 */
#define SPI0_BASE    0x40013000UL
#define SPI_CTL0     (*(volatile uint32_t *)(SPI0_BASE + 0x00))
#define SPI_CTL1     (*(volatile uint32_t *)(SPI0_BASE + 0x04))
#define SPI_STAT     (*(volatile uint32_t *)(SPI0_BASE + 0x08))
#define SPI_DATA     (*(volatile uint32_t *)(SPI0_BASE + 0x0C))

#define SPI_STAT_TBE  (1UL << 1) /* TX Buffer Empty */
#define SPI_STAT_RBNE (1UL << 0) /* RX Buffer Not Empty */
#define SPI_STAT_TRANS (1UL << 7) /* Busy */

int hal_spi_init(hal_spi_bus_t bus, const hal_spi_config_t *config)
{
    (void)bus; (void)config;
    /* Master mode, 8-bit, Software NSS, Saat etkin */
    return 0;
}

void hal_spi_deinit(hal_spi_bus_t bus)
{
    (void)bus;
}

int hal_spi_transmit(hal_spi_bus_t bus, const uint8_t *data, size_t len)
{
    (void)bus;
    for (size_t i = 0; i < len; i++) {
        hal_spi_transfer_byte(bus, data[i]);
    }
    return (int)len;
}

int hal_spi_receive(hal_spi_bus_t bus, uint8_t *data, size_t len)
{
    (void)bus;
    for (size_t i = 0; i < len; i++) {
        data[i] = hal_spi_transfer_byte(bus, 0xFF);
    }
    return (int)len;
}

int hal_spi_transfer(hal_spi_bus_t bus, const uint8_t *tx, uint8_t *rx, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        uint8_t out = tx ? tx[i] : 0xFF;
        uint8_t in = hal_spi_transfer_byte(bus, out);
        if (rx) rx[i] = in;
    }
    return (int)len;
}

uint8_t hal_spi_transfer_byte(hal_spi_bus_t bus, uint8_t byte)
{
    (void)bus;
    /* Donanım SPI transferi veya bit-bang emülasyonu */
    volatile uint32_t timeout = 50000;
    while (!(SPI_STAT & SPI_STAT_TBE) && --timeout);
    SPI_DATA = byte;
    timeout = 50000;
    while (!(SPI_STAT & SPI_STAT_RBNE) && --timeout);
    return (uint8_t)(SPI_DATA & 0xFF);
}
