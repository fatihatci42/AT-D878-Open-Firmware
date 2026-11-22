/**
 * @file    dmr_protocol.h
 * @brief   DMR Protocol Layer, Talker Alias & Digital Monitor Engine
 * @version 0.1.0
 *
 * AnyTone D878 Dijital Monitör, Talker Alias ve Çağrı Tanıma Katmanı.
 */

#ifndef DMR_PROTOCOL_H
#define DMR_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "driver_sct3258.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DMR_MAX_ALIAS_LEN          32
#define DMR_LAST_HEARD_COUNT       16

typedef enum {
    DIGITAL_MONITOR_OFF = 0,
    DIGITAL_MONITOR_SINGLE_SLOT, /**< Sadece seçili slotu dinle */
    DIGITAL_MONITOR_DOUBLE_SLOT  /**< Her iki slotu da dinle (Promiscuous) */
} digital_monitor_mode_t;

typedef struct {
    uint32_t            src_id;
    uint32_t            dst_id;
    sct3258_slot_t      slot;
    uint8_t             color_code;
    sct3258_call_type_t call_type;
    char                talker_alias[DMR_MAX_ALIAS_LEN];
    int16_t             rssi_dbm;
    uint32_t            start_time_ms;
    uint32_t            duration_ms;
    bool                is_active;
} dmr_call_info_t;

/**
 * @brief DMR protokol motorunu başlat
 */
int dmr_protocol_init(void);

/**
 * @brief Dijital Monitör modunu ayarla
 */
void dmr_protocol_set_monitor_mode(digital_monitor_mode_t mode);

/**
 * @brief Aktif çağrı bilgisini oku
 */
int dmr_protocol_get_active_call(dmr_call_info_t *call);

/**
 * @brief Son duyulan istasyonlar listesini oku (Last Heard)
 * @param list Dizi çıktısı
 * @param max_count Maksimum eleman
 * @return Alınan istasyon sayısı
 */
int dmr_protocol_get_last_heard(dmr_call_info_t *list, size_t max_count);

/**
 * @brief Gelen DMR çerçevesini işle (SCT3258'den gelen veri)
 */
void dmr_protocol_process_frame(const uint8_t *frame_data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* DMR_PROTOCOL_H */
