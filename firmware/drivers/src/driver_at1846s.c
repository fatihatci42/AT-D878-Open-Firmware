/**
 * @file    driver_at1846s.c
 * @brief   AT1846S RF Transceiver Driver Implementation - AT-D878 Open Firmware
 * @version 0.0.1
 *
 * Auctus AT1846S tek çipli VHF/UHF RF alıcı-verici sürücüsü.
 */

#include "driver_at1846s.h"
#include "hal_i2c.h"
#include "hal_gpio.h"
#include <string.h>

#define AT1846S_DEFAULT_I2C_ADDR  0x35

static at1846s_config_t s_config;
static at1846s_status_t s_status;
static bool s_initialized = false;

/* Yardımcı: 16-bit register yaz */
static int at1846s_write_internal(uint8_t reg, uint16_t val)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val & 0xFF);
    return hal_i2c_write_reg(HAL_I2C_0, s_config.i2c_addr, reg, buf, 2);
}

/* Yardımcı: 16-bit register oku */
static int at1846s_read_internal(uint8_t reg, uint16_t *val)
{
    uint8_t buf[2];
    int res = hal_i2c_read_reg(HAL_I2C_0, s_config.i2c_addr, reg, buf, 2);
    if (res == 0 && val) {
        *val = ((uint16_t)buf[0] << 8) | buf[1];
    }
    return res;
}

int at1846s_init(const at1846s_config_t *config)
{
    if (!config || !config->use_i2c) return -1;

    memcpy(&s_config, config, sizeof(at1846s_config_t));
    if (s_config.i2c_addr == 0) {
        s_config.i2c_addr = AT1846S_DEFAULT_I2C_ADDR;
    }
    if (s_config.i2c_addr > 0x7F) return -1;

    hal_i2c_config_t i2c_cfg = {
        .speed = HAL_I2C_SPEED_FAST,
        .use_dma = false,
    };
    if (hal_i2c_init(HAL_I2C_0, &i2c_cfg) != 0 ||
        !hal_i2c_probe(HAL_I2C_0, s_config.i2c_addr)) {
        s_initialized = false;
        return -2;
    }

    memset(&s_status, 0, sizeof(at1846s_status_t));
    s_initialized = false;

    /* Çip başlatma register sıralaması */
    if (at1846s_write_internal(AT1846S_REG_CLK_MODE, 0x0000) != 0 ||
        at1846s_write_internal(AT1846S_REG_LDO_VOLTAGE, 0x0380) != 0 ||
        at1846s_write_internal(AT1846S_REG_CH_MODE, AT1846S_BW_12_5KHZ) != 0) {
        return -3;
    }

    s_status.bandwidth = AT1846S_BW_12_5KHZ;
    s_initialized = true;
    return 0;
}

void at1846s_deinit(void)
{
    if (s_initialized) {
        at1846s_rx_off();
        at1846s_tx_off();
        s_initialized = false;
    }
}

int at1846s_set_frequency(uint32_t freq_hz)
{
    if (!s_initialized ||
        !((freq_hz >= 134000000U && freq_hz <= 174000000U) ||
          (freq_hz >= 400000000U && freq_hz <= 520000000U))) return -1;

    /* 
     * AT1846S frekans hesaplama:
     * F_step = F_xtal / (2^16) = 26000000 / 65536 = 396.7285 Hz
     * Reg_Val = Freq / F_step
     */
    uint32_t xtal = s_config.xtal_freq_hz ? s_config.xtal_freq_hz : 26000000;
    uint64_t reg_val = ((uint64_t)freq_hz << 16) / xtal;

    uint16_t hi = (uint16_t)((reg_val >> 16) & 0x3FFF);
    uint16_t lo = (uint16_t)(reg_val & 0xFFFF);

    /* Bant seçimi otomatik ayarla */
    uint8_t band = (freq_hz >= 400000000U) ?
                   AT1846S_BAND_UHF_400MHZ : AT1846S_BAND_VHF_134MHZ;
    if (at1846s_set_band(band) != 0 ||
        at1846s_write_internal(AT1846S_REG_FREQ_HI, hi) != 0 ||
        at1846s_write_internal(AT1846S_REG_FREQ_LO, lo) != 0) {
        return -2;
    }

    s_status.frequency_hz = freq_hz;
    return 0;
}

int64_t at1846s_get_frequency(void)
{
    return s_status.frequency_hz;
}

int at1846s_set_band(uint8_t band)
{
    if (!s_initialized || (band != AT1846S_BAND_UHF_400MHZ &&
        band != AT1846S_BAND_VHF_200MHZ && band != AT1846S_BAND_VHF_134MHZ)) return -1;
    int rc = at1846s_write_internal(AT1846S_REG_BAND_SEL, (uint16_t)band);
    if (rc == 0) s_status.band = band;
    return rc;
}

int at1846s_set_bandwidth(uint8_t bw)
{
    if (!s_initialized || (bw != AT1846S_BW_12_5KHZ && bw != AT1846S_BW_25KHZ)) return -1;
    int rc = at1846s_write_internal(AT1846S_REG_CH_MODE, (uint16_t)bw);
    if (rc == 0) s_status.bandwidth = bw;
    return rc;
}

int at1846s_tx_on(void)
{
    if (!s_initialized) return -1;
    /* TXON bitini aktif et */
    int rc = at1846s_write_internal(0x3C, 0x0020);
    if (rc == 0) {
        s_status.tx_active = true;
        s_status.rx_active = false;
    }
    return rc;
}

int at1846s_tx_off(void)
{
    if (!s_initialized) return -1;
    int rc = at1846s_write_internal(0x3C, 0x0000);
    if (rc == 0) s_status.tx_active = false;
    return rc;
}

int at1846s_rx_on(void)
{
    if (!s_initialized) return -1;
    /* RXON bitini aktif et */
    int rc = at1846s_write_internal(0x3C, 0x0010);
    if (rc == 0) {
        s_status.rx_active = true;
        s_status.tx_active = false;
    }
    return rc;
}

int at1846s_rx_off(void)
{
    if (!s_initialized) return -1;
    int rc = at1846s_write_internal(0x3C, 0x0000);
    if (rc == 0) s_status.rx_active = false;
    return rc;
}

int16_t at1846s_get_rssi(void)
{
    uint16_t raw_rssi = 0;
    at1846s_read_internal(0x1B, &raw_rssi);
    /* dBm dönüşüm formülü */
    int16_t rssi_dbm = -120 + (int16_t)(raw_rssi & 0xFF) / 2;
    s_status.rssi_dbm = rssi_dbm;
    return rssi_dbm;
}

int at1846s_set_squelch(uint8_t level)
{
    if (!s_initialized || level > 8) return -1;
    /* Squelch eşik register ayarı */
    uint16_t th = (uint16_t)level * 12;
    return at1846s_write_internal(0x48, th);
}

bool at1846s_get_squelch_status(void)
{
    uint16_t status_reg = 0;
    at1846s_read_internal(0x1C, &status_reg);
    s_status.squelch_open = (status_reg & 0x0001) != 0;
    return s_status.squelch_open;
}

int at1846s_get_status(at1846s_status_t *status)
{
    if (!status) return -1;
    memcpy(status, &s_status, sizeof(at1846s_status_t));
    return 0;
}
