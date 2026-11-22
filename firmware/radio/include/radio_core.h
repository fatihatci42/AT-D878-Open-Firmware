/**
 * @file    radio_core.h
 * @brief   Radio Core State Machine & Management - AT-D878 Open Firmware
 * @version 0.0.1
 */

#ifndef RADIO_CORE_H
#define RADIO_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include "channel.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RADIO_STATE_STANDBY = 0,
    RADIO_STATE_RX,
    RADIO_STATE_TX,
    RADIO_STATE_SCANNING,
    RADIO_STATE_ERROR
} radio_state_t;

typedef enum {
    VFO_A = 0,
    VFO_B = 1
} vfo_index_t;

typedef struct {
    radio_state_t state;
    vfo_index_t   active_vfo;
    channel_t     vfo_channels[2];
    uint8_t       current_volume;
    uint8_t       current_squelch;
    bool          ptt_locked;
    int16_t       rssi_dbm;
    uint16_t      battery_mv;
    uint8_t       battery_percent;
    bool          is_charging;
    bool          scan_active;
    char          active_caller_callsign[16];
    char          active_caller_alias[24];
    uint32_t      active_tg;
} radio_core_status_t;

/**
 * @brief Radyo çekirdeğini başlat
 */
int radio_core_init(void);

/**
 * @brief Düzenli çalışma döngüsü (Task / Tick)
 */
void radio_core_process(void);

/**
 * @brief Aktif VFO'yu değiştir (VFO A / VFO B)
 */
int radio_core_set_active_vfo(vfo_index_t vfo);

/**
 * @brief Belirli bir kanala geçiş yap
 */
int radio_core_set_channel(vfo_index_t vfo, const channel_t *channel);

/**
 * @brief Manuel frekans ayarla (VFO modu)
 */
int radio_core_set_vfo_frequency(vfo_index_t vfo, uint32_t rx_hz, uint32_t tx_hz);

/**
 * @brief İletimi (TX) başlat
 */
int radio_core_start_tx(void);

/**
 * @brief İletimi (TX) durdur
 */
int radio_core_stop_tx(void);

/**
 * @brief VFO A/B arasında geçiş yap
 */
int radio_core_toggle_vfo(void);

/**
 * @brief Frekansı bir adım artır/azalt
 * @param up true=yukarı, false=aşağı
 */
int radio_core_step_frequency(bool up);

/**
 * @brief Radyo çekirdeği anlık durumunu oku
 */
void radio_core_get_status(radio_core_status_t *status);

/**
 * @brief APRS Konum Raporu Beacon'ı ilet (144.800 MHz AFSK 1200)
 */
int radio_core_send_aprs_beacon(void);

#ifdef __cplusplus
}
#endif

#endif /* RADIO_CORE_H */
