/**
 * @file    codeplug_storage.h
 * @brief   Non-Volatile SPI Flash Codeplug Storage Engine - AT-D878 Open Firmware
 * @version 0.1.0
 *
 * 256 MB Harici SPI Flash üzerinde 4000 Kanal, 250 Bölge (Zone),
 * Telsiz Ayarları ve 500k DMR Kişi Listesinin yönetimi.
 */

#ifndef CODEPLUG_STORAGE_H
#define CODEPLUG_STORAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "channel.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CODEPLUG_MAGIC             0x44383738  /* 'D878' */
#define CODEPLUG_VERSION           0x0100      /* v1.00 */

#define MAX_CHANNELS               4000
#define MAX_ZONES                  250
#define MAX_CHANNELS_PER_ZONE      64
#define MAX_ZONE_NAME_LEN          16

/* ============================================================
 * Toshiba TC58CVG0S3HRAIG 1Gbit (128 MB) SPI NAND Flash Bellek Haritası
 * Kaynak: Jason Reilly VK7ZJA (878techmods.html - #FlashMem)
 * ============================================================ */
#define NAND_ADDR_FONTS_START       0x00000000U /* Alternatif dil fontları (0x00000000 - 0x0014FFFF) */
#define NAND_ADDR_BOOT_PICTURE      0x00150000U /* Varsayılan açılış logosu (0x00150000 - 0x00159FFF) */
#define NAND_ADDR_ICONS_START       0x0015A000U /* Çok renkli ikon bitmapleri (0x0015A000 - 0x001FFFFF) */
#define NAND_ADDR_ICONS_EXT         0x00420000U /* Ek ikon bitmapleri (0x00420000 - 0x0043BFFF) */
#define NAND_ADDR_CODEPLUG_START    0x00800000U /* Orijinal CPS Codeplug alanı (0x00800000 - 0x02EFFFFF) */
#define NAND_ADDR_RF_ALIGN_START    0x02F00000U /* Aktif RF 'soft' kalibrasyon verisi (0x02F00000 - 0x02F8061F) */
#define NAND_ADDR_LOCAL_INFO        0x02FA0000U /* Yerel telsiz konfigürasyon bloğu (0x02FA0000 - 0x02FDFFFF) */
#define NAND_ADDR_CUSTOM_BOOT_PIC   0x042C0000U /* Özel kullanıcı açılış resmi (0x042C0000 - 0x042C9FFF) */
#define NAND_ADDR_BACKGROUND_PIC    0x04300000U /* Özel arka plan resmi (0x04300000 - 0x04309FFF) */
#define NAND_ADDR_DMR_ID_DATABASE   0x04500000U /* DMR ID dijital kişi veritabanı (0x04500000 - 0x06BFFFFF) */
#define NAND_ADDR_DMR_RECORDINGS    0x06D00000U /* DMR ses kayıtları (0x06D00000 - 0x07FFFFFF) */

/* Open Firmware Dahili Codeplug Adresleri */
#define FLASH_ADDR_HEADER          0x000000U  /* 4 KB Başlık & Tanımlayıcı */
#define FLASH_ADDR_SETTINGS        0x001000U  /* 4 KB Genel Telsiz Ayarları */
#define FLASH_ADDR_ZONES           0x004000U  /* 32 KB Bölge (Zone) Tablosu */
#define FLASH_ADDR_CHANNELS        0x020000U  /* 256 KB Kanal Tablosu (4000 x 64B) */
#define FLASH_ADDR_DMR_CONTACTS    0x100000U  /* 4 MB DMR Kişi Listesi (500k) */

/* ============================================================
 * Veri Yapıları
 * ============================================================ */

typedef struct {
    uint32_t magic;                    /**< 'D878' */
    uint16_t version;                  /**< 0x0100 */
    uint16_t channel_count;            /**< Kayıtlı kanal sayısı */
    uint16_t zone_count;               /**< Kayıtlı bölge sayısı */
    uint32_t local_dmr_id;             /**< Yerel DMR ID */
    char     callsign[12];             /**< Çağrı işareti (TA5THK) */
    char     radio_name[16];           /**< Telsiz adı */
    uint32_t crc32;                    /**< Bütünlük kontrolü */
} __attribute__((packed)) codeplug_header_t;

typedef struct {
    char     name[MAX_ZONE_NAME_LEN];  /**< Bölge adı */
    uint16_t channel_count;            /**< Bu bölgedeki kanal sayısı */
    uint16_t channel_indices[MAX_CHANNELS_PER_ZONE]; /**< Kanal indeksleri (1-4000) */
} __attribute__((packed)) zone_t;

typedef struct {
    uint8_t  beep_enabled;             /**< Tuş sesi */
    uint8_t  backlight_time_s;         /**< Ekran ışığı süresi */
    uint8_t  brightness_level;         /**< 1-5 */
    uint8_t  tot_seconds;              /**< Gönderme zaman aşımı (30-180s) */
    uint8_t  default_squelch;          /**< 0-8 */
    uint8_t  language;                 /**< 0=TR, 1=EN */
    uint8_t  analog_mic_gain;          /**< 1-5 */
    uint8_t  dmr_mic_gain;             /**< 1-5 */
    uint8_t  ste_mode;                 /**< Squelch Tail Elimination */
    uint8_t  active_vfo;               /**< 0=VFO-A, 1=VFO-B */
    uint32_t vfo_a_freq_hz;
    uint32_t vfo_b_freq_hz;
} __attribute__((packed)) radio_settings_t;

/* ============================================================
 * API Fonksiyonları
 * ============================================================ */

/**
 * @brief Codeplug alt sistemini başlat
 *        Flash'ı kontrol eder, geçersizse varsayılanları yazar
 */
int codeplug_init(void);

/**
 * @brief Telsiz ayarlarını oku
 */
int codeplug_load_settings(radio_settings_t *settings);

/**
 * @brief Telsiz ayarlarını kaydet
 */
int codeplug_save_settings(const radio_settings_t *settings);

/**
 * @brief Kanal oku
 * @param index 1 - 4000 arası
 * @param ch Çıktı kanal yapısı
 */
int codeplug_load_channel(uint16_t index, channel_t *ch);

/**
 * @brief Kanal kaydet
 * @param index 1 - 4000 arası
 * @param ch Kaydedilecek kanal
 */
int codeplug_save_channel(uint16_t index, const channel_t *ch);

/**
 * @brief Bölge oku
 * @param index 0 - 249 arası
 */
int codeplug_load_zone(uint8_t index, zone_t *zone);

/**
 * @brief Bölge kaydet
 * @param index 0 - 249 arası
 */
int codeplug_save_zone(uint8_t index, const zone_t *zone);

/**
 * @brief Codeplug'ı fabrika ayarlarına sıfırla
 */
int codeplug_factory_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* CODEPLUG_STORAGE_H */
