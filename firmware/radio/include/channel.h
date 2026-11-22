/**
 * @file    channel.h
 * @brief   Channel and VFO Data Structures - AT-D878 Open Firmware
 * @version 0.0.1
 */

#ifndef CHANNEL_H
#define CHANNEL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CHANNEL_NAME_LEN  16

typedef enum {
    CH_MODE_ANALOG_FM = 0,
    CH_MODE_DMR,
    CH_MODE_M17
} channel_mode_t;

typedef enum {
    POWER_LOW = 0,
    POWER_MID,
    POWER_HIGH,
    POWER_TURBO
} tx_power_t;

typedef enum {
    BANDWIDTH_12_5KHZ = 0,
    BANDWIDTH_25KHZ
} bandwidth_t;

typedef struct {
    uint16_t index;                       /**< Kanal indeksi 1-4000 */
    char     name[MAX_CHANNEL_NAME_LEN];  /**< Kanal adı */
    uint32_t rx_freq_hz;                  /**< RX frekansı (Hz) */
    uint32_t tx_freq_hz;                  /**< TX frekansı (Hz) */
    channel_mode_t mode;                  /**< Analog FM / DMR / M17 */
    tx_power_t     power;                 /**< Çıkış gücü */
    bandwidth_t    bandwidth;             /**< Kanal genişliği */
    
    /* Analog Parametreler */
    uint16_t ctcss_rx_x10;                /**< CTCSS RX (örn 88.5 -> 885, 0=kapalı) */
    uint16_t ctcss_tx_x10;                /**< CTCSS TX */
    uint16_t dcs_rx;                      /**< DCS RX kodu */
    uint16_t dcs_tx;                      /**< DCS TX kodu */
    uint8_t  squelch_level;               /**< 0-8 */

    /* DMR Parametreleri */
    uint8_t  color_code;                  /**< 0-15 */
    uint8_t  slot;                        /**< 1 veya 2 */
    uint32_t contact_id;                  /**< Varsayılan TalkGroup / Private ID */
    uint16_t rx_group_list_id;            /**< RX Grup listesi ID */

    /* Genel Bayraklar */
    bool rx_only;                         /**< Sadece alım */
    bool scan_skip;                       /**< Taramada atla */
    bool aprs_report;                     /**< APRS beacon açık */
} channel_t;

#ifdef __cplusplus
}
#endif

#endif /* CHANNEL_H */
