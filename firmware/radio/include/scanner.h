/**
 * @file    scanner.h
 * @brief   Frequency, Channel & Tone Scanner Engine - AT-D878 Open Firmware
 * @version 0.1.0
 */

#ifndef SCANNER_H
#define SCANNER_H

#include <stdint.h>
#include <stdbool.h>
#include "channel.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SCAN_STATE_IDLE = 0,
    SCAN_STATE_SCANNING,
    SCAN_STATE_PAUSED,
} scan_state_t;

typedef enum {
    SCAN_TYPE_CHANNEL = 0,      /**< Kanal tarama */
    SCAN_TYPE_VFO,              /**< VFO frekans aralığı tarama */
    SCAN_TYPE_TONE,             /**< CTCSS ton tarama */
} scan_type_t;

typedef enum {
    SCAN_RESUME_TIME = 0,       /**< Sinyal olsa bile 5 saniye sonra devam et */
    SCAN_RESUME_CARRIER,        /**< Sinyal kesilene kadar bekle */
    SCAN_RESUME_STOP            /**< İlk sinyalde dur */
} scan_resume_mode_t;

typedef struct {
    scan_state_t       state;
    scan_type_t        type;
    scan_resume_mode_t resume_mode;
    uint32_t           vfo_start_freq;
    uint32_t           vfo_end_freq;
    uint32_t           vfo_step_hz;
    uint16_t           current_channel_idx;
    uint32_t           current_freq_hz;
    uint16_t           found_ctcss_x10;
    bool               priority_watch_active;
    uint16_t           priority_channel_idx;
} scanner_status_t;

/**
 * @brief Tarayıcı motorunu başlat
 */
int scanner_init(void);

/**
 * @brief Kanal taramasını başlat
 */
int scanner_start_channel_scan(scan_resume_mode_t resume);

/**
 * @brief VFO frekans aralığı taraması başlat
 */
int scanner_start_vfo_scan(uint32_t start_hz, uint32_t end_hz, uint32_t step_hz, scan_resume_mode_t resume);

/**
 * @brief CTCSS ton taraması başlat
 */
int scanner_start_tone_scan(void);

/**
 * @brief Taramayı durdur
 */
void scanner_stop(void);

/**
 * @brief Tarama durumunu al
 */
void scanner_get_status(scanner_status_t *status);

/**
 * @brief Periyodik tarama adımı (Ana döngüden çağrılır)
 */
void scanner_process(void);

#ifdef __cplusplus
}
#endif

#endif /* SCANNER_H */
