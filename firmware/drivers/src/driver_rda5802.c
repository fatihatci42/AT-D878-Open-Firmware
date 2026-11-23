/**
 * @file    driver_rda5802.c
 * @brief   RDA5802N Commercial FM Broadcast Receiver Implementation
 * @version 0.1.0
 */

#include "driver_rda5802.h"
#include "hal_i2c.h"
#include "hal_systick.h"
#include <string.h>

#define RDA5802_REG_CHIP_ID       0x00
#define RDA5802_REG_CTRL          0x02
#define RDA5802_REG_CHANNEL       0x03
#define RDA5802_REG_SYS_CTRL      0x04
#define RDA5802_REG_VOLUME        0x05
#define RDA5802_REG_STATUS        0x0A
#define RDA5802_REG_RSSI          0x0B

/* Register 0x02 Bitleri */
#define RDA_CTRL_DHIZ             (1U << 15) /* Audio output high-z disable (1 = audio on) */
#define RDA_CTRL_DMUTE            (1U << 14) /* Mute disable (1 = normal audio, 0 = mute) */
#define RDA_CTRL_BASS             (1U << 12) /* Bass boost */
#define RDA_CTRL_SEEKUP           (1U << 9)  /* Seek up */
#define RDA_CTRL_SEEK             (1U << 8)  /* Start seek */
#define RDA_CTRL_SKMODE           (1U << 7)  /* Seek wrap */
#define RDA_CTRL_CLK_MODE         (0U << 4)  /* 32.768 kHz kristal */
#define RDA_CTRL_SOFTRESET        (1U << 1)
#define RDA_CTRL_ENABLE           (1U << 0)  /* Power on */

/* Register 0x03 Bitleri */
#define RDA_CHAN_TUNE             (1U << 4)  /* Start tune */
#define RDA_CHAN_BAND_WORLD       (0U << 2)  /* 87-108 MHz */
#define RDA_CHAN_BAND_EAST_EU     (1U << 2)  /* 76-91 MHz */
#define RDA_CHAN_SPACE_100KHZ     (0U << 0)  /* 100 kHz kanal adımı */

static rda5802_status_t s_fm_status = {0};

int rda5802_init(void)
{
    memset(&s_fm_status, 0, sizeof(s_fm_status));
    s_fm_status.volume = 12;
    s_fm_status.freq_khz = 100000; /* 100.0 MHz */

    /* I2C üzerinden çipi uyandır */
    uint16_t ctrl = RDA_CTRL_DHIZ | RDA_CTRL_DMUTE | RDA_CTRL_BASS | RDA_CTRL_ENABLE;
    int rc = hal_i2c_write_reg16(HAL_I2C_0, RDA5802_I2C_ADDR, RDA5802_REG_CTRL, ctrl);
    if (rc != 0) {
        return -1;
    }

    hal_delay_ms(50);

    /* Ses seviyesi ayarla (Volume bits 3:0) */
    uint16_t vol_reg = 0x8880 | (s_fm_status.volume & 0x0F);
    hal_i2c_write_reg16(HAL_I2C_0, RDA5802_I2C_ADDR, RDA5802_REG_VOLUME, vol_reg);

    s_fm_status.power_on = true;
    s_fm_status.muted = false;

    return rda5802_set_frequency(s_fm_status.freq_khz);
}

int rda5802_power(bool on)
{
    s_fm_status.power_on = on;
    uint16_t ctrl = on ? (RDA_CTRL_DHIZ | RDA_CTRL_DMUTE | RDA_CTRL_ENABLE) : 0x0000;
    return hal_i2c_write_reg16(HAL_I2C_0, RDA5802_I2C_ADDR, RDA5802_REG_CTRL, ctrl);
}

int rda5802_set_frequency(uint32_t freq_khz)
{
    if (freq_khz < RDA5802_FREQ_MIN_KHZ) freq_khz = RDA5802_FREQ_MIN_KHZ;
    if (freq_khz > RDA5802_FREQ_MAX_KHZ) freq_khz = RDA5802_FREQ_MAX_KHZ;

    s_fm_status.freq_khz = freq_khz;

    /* Kanal indeksi: (freq - 87.0 MHz) / 100 kHz */
    uint16_t chan_idx = 0;
    uint16_t band = RDA_CHAN_BAND_WORLD;

    if (freq_khz >= 87000) {
        chan_idx = (uint16_t)((freq_khz - 87000) / 100);
        band = RDA_CHAN_BAND_WORLD;
    } else {
        chan_idx = (uint16_t)((freq_khz - 65000) / 100);
        band = RDA_CHAN_BAND_EAST_EU;
    }

    uint16_t chan_reg = (chan_idx << 6) | RDA_CHAN_TUNE | band | RDA_CHAN_SPACE_100KHZ;
    return hal_i2c_write_reg16(HAL_I2C_0, RDA5802_I2C_ADDR, RDA5802_REG_CHANNEL, chan_reg);
}

int rda5802_set_volume(uint8_t volume)
{
    if (volume > 15) volume = 15;
    s_fm_status.volume = volume;

    uint16_t vol_reg = 0x8880 | (volume & 0x0F);
    return hal_i2c_write_reg16(HAL_I2C_0, RDA5802_I2C_ADDR, RDA5802_REG_VOLUME, vol_reg);
}

int rda5802_set_mute(bool mute)
{
    s_fm_status.muted = mute;
    uint16_t ctrl = RDA_CTRL_DHIZ | RDA_CTRL_ENABLE;
    if (!mute) {
        ctrl |= RDA_CTRL_DMUTE;
    }
    return hal_i2c_write_reg16(HAL_I2C_0, RDA5802_I2C_ADDR, RDA5802_REG_CTRL, ctrl);
}

int rda5802_seek(bool up)
{
    uint16_t ctrl = RDA_CTRL_DHIZ | RDA_CTRL_DMUTE | RDA_CTRL_SEEK | RDA_CTRL_SKMODE | RDA_CTRL_ENABLE;
    if (up) {
        ctrl |= RDA_CTRL_SEEKUP;
    }
    return hal_i2c_write_reg16(HAL_I2C_0, RDA5802_I2C_ADDR, RDA5802_REG_CTRL, ctrl);
}

int rda5802_get_status(rda5802_status_t *status)
{
    if (!status) return -1;

    int rssi_raw = hal_i2c_read_reg16(HAL_I2C_0, RDA5802_I2C_ADDR, RDA5802_REG_RSSI);
    if (rssi_raw >= 0) {
        s_fm_status.rssi = (uint8_t)((rssi_raw >> 9) & 0x7F);
        s_fm_status.stereo = (rssi_raw & (1U << 10)) != 0;
    }

    int stat_raw = hal_i2c_read_reg16(HAL_I2C_0, RDA5802_I2C_ADDR, RDA5802_REG_STATUS);
    if (stat_raw >= 0) {
        uint16_t chan = (uint16_t)(stat_raw & 0x03FF);
        s_fm_status.freq_khz = 87000 + (chan * 100);
    }

    memcpy(status, &s_fm_status, sizeof(rda5802_status_t));
    return 0;
}
