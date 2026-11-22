/**
 * @file    codeplug_storage.c
 * @brief   Non-Volatile SPI Flash Codeplug Storage Implementation
 * @version 0.1.0
 */

#include "codeplug_storage.h"
#include "driver_spi_flash.h"
#include <string.h>
#include <stdio.h>

static codeplug_header_t s_header;
static bool s_initialized = false;

/* Varsayılan fabrika ayarları */
static const radio_settings_t DEFAULT_SETTINGS = {
    .beep_enabled     = 1,
    .backlight_time_s = 10,
    .brightness_level = 4,
    .tot_seconds      = 120,
    .default_squelch  = 3,
    .language         = 0,      /* Türkçe */
    .analog_mic_gain  = 3,
    .dmr_mic_gain     = 3,
    .ste_mode         = 1,      /* 180° CTCSS STE */
    .active_vfo       = 0,      /* VFO-A */
    .vfo_a_freq_hz    = 145500000,
    .vfo_b_freq_hz    = 433500000,
};

int codeplug_init(void)
{
    /* SPI Flash sürücüsü üzerinden başlığı oku */
    int rc = spi_flash_read(FLASH_ADDR_HEADER, (uint8_t *)&s_header, sizeof(s_header));
    if (rc <= 0 || s_header.magic != CODEPLUG_MAGIC) {
        /* Geçersiz veya boş flash -> Fabrika ayarlarıyla formatla */
        return codeplug_factory_reset();
    }

    s_initialized = true;
    return 0;
}

int codeplug_factory_reset(void)
{
    /* 1. Başlık alanını sil ve yaz */
    spi_flash_erase_sector(FLASH_ADDR_HEADER);

    memset(&s_header, 0, sizeof(s_header));
    s_header.magic = CODEPLUG_MAGIC;
    s_header.version = CODEPLUG_VERSION;
    s_header.channel_count = 2;
    s_header.zone_count = 1;
    s_header.local_dmr_id = 2860501;
    strncpy(s_header.callsign, "TA5THK", sizeof(s_header.callsign) - 1);
    strncpy(s_header.radio_name, "D878 OPEN FW", sizeof(s_header.radio_name) - 1);

    spi_flash_write(FLASH_ADDR_HEADER, (const uint8_t *)&s_header, sizeof(s_header));

    /* 2. Varsayılan ayarları kaydet */
    codeplug_save_settings(&DEFAULT_SETTINGS);

    /* 3. Varsayılan 1. Bölgeyi kaydet */
    zone_t zone1;
    memset(&zone1, 0, sizeof(zone1));
    strncpy(zone1.name, "ZONE 1 TURKIYE", sizeof(zone1.name) - 1);
    zone1.channel_count = 2;
    zone1.channel_indices[0] = 1;
    zone1.channel_indices[1] = 2;
    codeplug_save_zone(0, &zone1);

    /* 4. Varsayılan 2 Kanalı kaydet */
    channel_t ch1;
    memset(&ch1, 0, sizeof(ch1));
    ch1.index = 1;
    strncpy(ch1.name, "VHF 145.500 FM", sizeof(ch1.name) - 1);
    ch1.rx_freq_hz = 145500000;
    ch1.tx_freq_hz = 145500000;
    ch1.mode = CH_MODE_ANALOG_FM;
    ch1.power = POWER_HIGH;
    ch1.bandwidth = BANDWIDTH_25KHZ;
    ch1.squelch_level = 3;
    codeplug_save_channel(1, &ch1);

    channel_t ch2;
    memset(&ch2, 0, sizeof(ch2));
    ch2.index = 2;
    snprintf(ch2.name, sizeof(ch2.name), "UHF 433 DMR");
    ch2.rx_freq_hz = 433500000;
    ch2.tx_freq_hz = 433500000;
    ch2.mode = CH_MODE_DMR;
    ch2.power = POWER_HIGH;
    ch2.bandwidth = BANDWIDTH_12_5KHZ;
    ch2.color_code = 1;
    ch2.slot = 1;
    ch2.contact_id = 286;  /* TG 286 */
    codeplug_save_channel(2, &ch2);

    s_initialized = true;
    return 0;
}

int codeplug_load_settings(radio_settings_t *settings)
{
    if (!settings) return -1;
    return spi_flash_read(FLASH_ADDR_SETTINGS, (uint8_t *)settings, sizeof(radio_settings_t));
}

int codeplug_save_settings(const radio_settings_t *settings)
{
    if (!settings) return -1;
    spi_flash_erase_sector(FLASH_ADDR_SETTINGS);
    return spi_flash_write(FLASH_ADDR_SETTINGS, (const uint8_t *)settings, sizeof(radio_settings_t));
}

int codeplug_load_channel(uint16_t index, channel_t *ch)
{
    if (!ch || index == 0 || index > MAX_CHANNELS) return -1;
    uint32_t addr = FLASH_ADDR_CHANNELS + ((uint32_t)(index - 1) * sizeof(channel_t));
    return spi_flash_read(addr, (uint8_t *)ch, sizeof(channel_t));
}

int codeplug_save_channel(uint16_t index, const channel_t *ch)
{
    if (!ch || index == 0 || index > MAX_CHANNELS) return -1;
    uint32_t addr = FLASH_ADDR_CHANNELS + ((uint32_t)(index - 1) * sizeof(channel_t));
    return spi_flash_write(addr, (const uint8_t *)ch, sizeof(channel_t));
}

int codeplug_load_zone(uint8_t index, zone_t *zone)
{
    if (!zone || index >= MAX_ZONES) return -1;
    uint32_t addr = FLASH_ADDR_ZONES + ((uint32_t)index * sizeof(zone_t));
    return spi_flash_read(addr, (uint8_t *)zone, sizeof(zone_t));
}

int codeplug_save_zone(uint8_t index, const zone_t *zone)
{
    if (!zone || index >= MAX_ZONES) return -1;
    uint32_t addr = FLASH_ADDR_ZONES + ((uint32_t)index * sizeof(zone_t));
    return spi_flash_write(addr, (const uint8_t *)zone, sizeof(zone_t));
}
