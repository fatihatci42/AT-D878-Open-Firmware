/**
 * @file    driver_rda5802.h
 * @brief   RDA5802N Commercial FM Broadcast Receiver Driver - AT-D878 Open Firmware
 * @version 0.1.0
 *
 * AnyTone AT-D878 / D868 / D578 dahili ticari FM radyo çipi (65.0 - 108.0 MHz).
 * I2C Adresi: 0x10 (PB6 SCL, PB7 SDA)
 * Kaynak: RadioFoundry Hardware Specification (hardware-anytone.md Bölüm 4.1)
 */

#ifndef DRIVER_RDA5802_H
#define DRIVER_RDA5802_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RDA5802_I2C_ADDR           0x10

#define RDA5802_FREQ_MIN_KHZ       65000   /**< 65.0 MHz */
#define RDA5802_FREQ_MAX_KHZ       108000  /**< 108.0 MHz */

/**
 * @brief RDA5802N FM alıcı durum yapısı
 */
typedef struct {
    bool     power_on;
    uint32_t freq_khz;     /**< Aktif frekans (kHz cinsinden, örn: 100000 -> 100.0 MHz) */
    uint8_t  volume;       /**< 0..15 */
    uint8_t  rssi;         /**< 0..127 sinyal gücü */
    bool     stereo;       /**< Stereo alım aktif */
    bool     muted;
} rda5802_status_t;

/**
 * @brief RDA5802N FM çipini başlat
 */
int rda5802_init(void);

/**
 * @brief FM radyoyu aç / kapat (güç koruma)
 */
int rda5802_power(bool on);

/**
 * @brief FM istasyon frekansını ayarla
 * @param freq_khz Frekans (örn: 98000 = 98.0 MHz)
 */
int rda5802_set_frequency(uint32_t freq_khz);

/**
 * @brief Ses seviyesini ayarla (0..15)
 */
int rda5802_set_volume(uint8_t volume);

/**
 * @brief Sustur (Mute) aç / kapat
 */
int rda5802_set_mute(bool mute);

/**
 * @brief Bir sonraki güçlü istasyonu otomatik ara (Seek)
 * @param up true=yukarı, false=aşağı
 */
int rda5802_seek(bool up);

/**
 * @brief Anlık durum ve sinyal gücünü oku
 */
int rda5802_get_status(rda5802_status_t *status);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_RDA5802_H */
