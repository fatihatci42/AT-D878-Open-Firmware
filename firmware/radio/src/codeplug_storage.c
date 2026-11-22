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

#define CODEPLUG_SECTOR_MAX_SIZE 4096U

/* Sector read/modify/write is shared by codeplug writers and is not reentrant. */
static uint8_t s_sector_buffer[CODEPLUG_SECTOR_MAX_SIZE];
static uint8_t s_record_verify[sizeof(zone_t) > sizeof(channel_t) ?
                               sizeof(zone_t) : sizeof(channel_t)];

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

static int codeplug_write_preserving_sector(uint32_t address,
                                             const uint8_t *data,
                                             size_t length)
{
    spi_flash_info_t flash;
    if (!data || length == 0 || spi_flash_get_info(&flash) != 0 ||
        flash.sector_size == 0 || flash.sector_size > sizeof(s_sector_buffer) ||
        address >= flash.total_size || length > (size_t)(flash.total_size - address)) {
        return -1;
    }

    size_t written = 0;
    while (written < length) {
        uint32_t current_address = address + (uint32_t)written;
        uint32_t sector_start = current_address - (current_address % flash.sector_size);
        size_t sector_offset = current_address - sector_start;
        size_t chunk = flash.sector_size - sector_offset;
        if (chunk > length - written) chunk = length - written;

        if (spi_flash_read(sector_start, s_sector_buffer, flash.sector_size) !=
            (int)flash.sector_size) {
            return -1;
        }
        memcpy(s_sector_buffer + sector_offset, data + written, chunk);
        if (spi_flash_erase_sector(sector_start) != 0 ||
            spi_flash_write(sector_start, s_sector_buffer, flash.sector_size) !=
            (int)flash.sector_size) {
            return -1;
        }
        written += chunk;
    }

    if (length > sizeof(s_record_verify) ||
        spi_flash_read(address, s_record_verify, length) != (int)length ||
        memcmp(s_record_verify, data, length) != 0) {
        return -1;
    }
    return (int)length;
}

static int codeplug_store_header(void)
{
    return codeplug_write_preserving_sector(FLASH_ADDR_HEADER,
                                             (const uint8_t *)&s_header,
                                             sizeof(s_header));
}

int codeplug_init(void)
{
    spi_flash_info_t flash;
    if (spi_flash_get_info(&flash) != 0 || flash.sector_size == 0) {
        s_initialized = false;
        return -1;
    }

    /* SPI Flash sürücüsü üzerinden başlığı oku */
    int rc = spi_flash_read(FLASH_ADDR_HEADER, (uint8_t *)&s_header, sizeof(s_header));
    if (rc != (int)sizeof(s_header)) {
        s_initialized = false;
        return -1;
    }
    if (s_header.magic != CODEPLUG_MAGIC) {
        /* Geçersiz veya boş flash -> Fabrika ayarlarıyla formatla */
        return codeplug_factory_reset();
    }
    if (s_header.version != CODEPLUG_VERSION) {
        s_initialized = false;
        return -2;
    }

    s_initialized = true;
    return 0;
}

int codeplug_factory_reset(void)
{
    spi_flash_info_t flash;
    if (spi_flash_get_info(&flash) != 0 || flash.sector_size == 0 ||
        spi_flash_erase_sector(FLASH_ADDR_HEADER) != 0 ||
        spi_flash_erase_sector(FLASH_ADDR_SETTINGS) != 0) {
        s_initialized = false;
        return -1;
    }

    s_initialized = false;
    memset(&s_header, 0, sizeof(s_header));
    s_header.magic = CODEPLUG_MAGIC;
    s_header.version = CODEPLUG_VERSION;
    s_header.channel_count = 2;
    s_header.zone_count = 1;
    s_header.local_dmr_id = 2860501;
    strncpy(s_header.callsign, "TA5THK", sizeof(s_header.callsign) - 1);
    strncpy(s_header.radio_name, "D878 OPEN FW", sizeof(s_header.radio_name) - 1);

    if (spi_flash_write(FLASH_ADDR_HEADER, (const uint8_t *)&s_header,
                        sizeof(s_header)) != (int)sizeof(s_header)) {
        return -1;
    }

    /* 2. Varsayılan ayarları kaydet */
    if (codeplug_save_settings(&DEFAULT_SETTINGS) != (int)sizeof(DEFAULT_SETTINGS)) {
        return -1;
    }

    /* 3. Varsayılan 1. Bölgeyi kaydet */
    zone_t zone1;
    memset(&zone1, 0, sizeof(zone1));
    strncpy(zone1.name, "ZONE 1 TURKIYE", sizeof(zone1.name) - 1);
    zone1.channel_count = 2;
    zone1.channel_indices[0] = 1;
    zone1.channel_indices[1] = 2;
    if (codeplug_save_zone(0, &zone1) != (int)sizeof(zone1)) return -1;

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
    if (codeplug_save_channel(1, &ch1) != (int)sizeof(ch1)) return -1;

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
    if (codeplug_save_channel(2, &ch2) != (int)sizeof(ch2)) return -1;

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
    if (!settings || spi_flash_erase_sector(FLASH_ADDR_SETTINGS) != 0) return -1;
    if (spi_flash_write(FLASH_ADDR_SETTINGS, (const uint8_t *)settings,
                        sizeof(radio_settings_t)) != (int)sizeof(radio_settings_t)) {
        return -1;
    }
    radio_settings_t verify;
    if (spi_flash_read(FLASH_ADDR_SETTINGS, (uint8_t *)&verify, sizeof(verify)) !=
        (int)sizeof(verify) || memcmp(&verify, settings, sizeof(verify)) != 0) {
        return -1;
    }
    return (int)sizeof(radio_settings_t);
}

int codeplug_load_channel(uint16_t index, channel_t *ch)
{
    if (!s_initialized || !ch || index == 0 || index > MAX_CHANNELS ||
        index > s_header.channel_count) return -1;
    uint32_t addr = FLASH_ADDR_CHANNELS + ((uint32_t)(index - 1) * sizeof(channel_t));
    int rc = spi_flash_read(addr, (uint8_t *)ch, sizeof(channel_t));
    if (rc != (int)sizeof(channel_t) || ch->index != index ||
        ch->rx_freq_hz == UINT32_MAX) return -1;
    return rc;
}

int codeplug_save_channel(uint16_t index, const channel_t *ch)
{
    if (!ch || index == 0 || index > MAX_CHANNELS || ch->index != index) return -1;
    uint32_t addr = FLASH_ADDR_CHANNELS + ((uint32_t)(index - 1) * sizeof(channel_t));
    int rc = codeplug_write_preserving_sector(addr, (const uint8_t *)ch, sizeof(channel_t));
    if (rc != (int)sizeof(channel_t)) return -1;
    if (index > s_header.channel_count) {
        s_header.channel_count = index;
        if (codeplug_store_header() != (int)sizeof(s_header)) return -1;
    }
    return rc;
}

int codeplug_load_zone(uint8_t index, zone_t *zone)
{
    if (!s_initialized || !zone || index >= MAX_ZONES || index >= s_header.zone_count) return -1;
    uint32_t addr = FLASH_ADDR_ZONES + ((uint32_t)index * sizeof(zone_t));
    int rc = spi_flash_read(addr, (uint8_t *)zone, sizeof(zone_t));
    if (rc != (int)sizeof(zone_t)) return -1;
    if (zone->channel_count > MAX_CHANNELS_PER_ZONE) return -1;
    return rc;
}

int codeplug_save_zone(uint8_t index, const zone_t *zone)
{
    if (!zone || index >= MAX_ZONES || zone->channel_count > MAX_CHANNELS_PER_ZONE) return -1;
    for (uint16_t i = 0; i < zone->channel_count; i++) {
        if (zone->channel_indices[i] == 0 || zone->channel_indices[i] > MAX_CHANNELS) return -1;
    }
    uint32_t addr = FLASH_ADDR_ZONES + ((uint32_t)index * sizeof(zone_t));
    int rc = codeplug_write_preserving_sector(addr, (const uint8_t *)zone, sizeof(zone_t));
    if (rc != (int)sizeof(zone_t)) return -1;
    if (index >= s_header.zone_count) {
        s_header.zone_count = (uint16_t)index + 1U;
        if (codeplug_store_header() != (int)sizeof(s_header)) return -1;
    }
    return rc;
}
