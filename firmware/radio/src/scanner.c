/**
 * @file    scanner.c
 * @brief   Frequency, Channel & Tone Scanner Engine Implementation
 * @version 0.1.0
 */

#include "scanner.h"
#include "driver_at1846s.h"
#include "driver_tones.h"
#include "codeplug_storage.h"
#include "hal_systick.h"
#include <string.h>

static scanner_status_t s_scan;
static tick_t s_last_step_time = 0;
static tick_t s_pause_start_time = 0;

#define SCAN_STEP_INTERVAL_MS      50   /* Her frekans adımında bekleme (50ms) */
#define SCAN_PAUSE_TIME_MS         5000 /* TIME modunda duraklama süresi (5 sn) */

static bool scan_frequency_supported(uint32_t freq_hz)
{
    return (freq_hz >= 134000000U && freq_hz <= 174000000U) ||
           (freq_hz >= 400000000U && freq_hz <= 520000000U);
}

int scanner_init(void)
{
    memset(&s_scan, 0, sizeof(s_scan));
    s_scan.state = SCAN_STATE_IDLE;
    s_scan.resume_mode = SCAN_RESUME_TIME;
    return 0;
}

int scanner_start_channel_scan(scan_resume_mode_t resume)
{
    s_scan.state = SCAN_STATE_SCANNING;
    s_scan.type = SCAN_TYPE_CHANNEL;
    s_scan.resume_mode = resume;
    s_scan.current_channel_idx = 0;
    s_last_step_time = hal_tick_get();
    return 0;
}

int scanner_start_vfo_scan(uint32_t start_hz, uint32_t end_hz, uint32_t step_hz, scan_resume_mode_t resume)
{
    bool same_vhf_band = start_hz >= 134000000U && start_hz <= 174000000U &&
                         end_hz >= 134000000U && end_hz <= 174000000U;
    bool same_uhf_band = start_hz >= 400000000U && start_hz <= 520000000U &&
                         end_hz >= 400000000U && end_hz <= 520000000U;
    if (start_hz > end_hz || (!same_vhf_band && !same_uhf_band)) return -1;

    s_scan.state = SCAN_STATE_SCANNING;
    s_scan.type = SCAN_TYPE_VFO;
    s_scan.resume_mode = resume;
    s_scan.vfo_start_freq = start_hz;
    s_scan.vfo_end_freq = end_hz;
    s_scan.vfo_step_hz = (step_hz == 0) ? 12500 : step_hz;
    s_scan.current_freq_hz = start_hz;
    s_last_step_time = hal_tick_get();
    if (at1846s_set_frequency(start_hz) != 0) {
        s_scan.state = SCAN_STATE_IDLE;
        return -1;
    }
    return 0;
}

int scanner_start_tone_scan(void)
{
    /* A CTCSS demodulation/sample API is not available in the current driver. */
    return -2;
}

void scanner_stop(void)
{
    s_scan.state = SCAN_STATE_IDLE;
}

void scanner_get_status(scanner_status_t *status)
{
    if (status) {
        memcpy(status, &s_scan, sizeof(s_scan));
    }
}

void scanner_process(void)
{
    if (s_scan.state == SCAN_STATE_IDLE) return;

    tick_t now = hal_tick_get();

    /* 1. Sinyal algılandığında duraklama yönetimi */
    if (s_scan.state == SCAN_STATE_PAUSED) {
        bool signal_present = at1846s_get_squelch_status();

        if (s_scan.resume_mode == SCAN_RESUME_CARRIER) {
            if (!signal_present) {
                s_scan.state = SCAN_STATE_SCANNING;
                s_last_step_time = now;
            }
            return;
        } else if (s_scan.resume_mode == SCAN_RESUME_TIME) {
            if (hal_tick_elapsed(s_pause_start_time, now) >= SCAN_PAUSE_TIME_MS) {
                s_scan.state = SCAN_STATE_SCANNING;
                s_last_step_time = now;
            }
            return;
        } else if (s_scan.resume_mode == SCAN_RESUME_STOP) {
            scanner_stop();
            return;
        }
    }

    /* 2. Tarama adımı zamanı geldi mi? */
    if (hal_tick_elapsed(s_last_step_time, now) < SCAN_STEP_INTERVAL_MS) {
        return;
    }
    s_last_step_time = now;

    /* Sinyal var mı kontrol et */
    if (at1846s_get_squelch_status()) {
        s_scan.state = SCAN_STATE_PAUSED;
        s_pause_start_time = now;
        return;
    }

    /* 3. Sonraki frekansa / kanala adımla */
    if (s_scan.type == SCAN_TYPE_VFO) {
        if (s_scan.vfo_step_hz == 0 ||
            s_scan.current_freq_hz > s_scan.vfo_end_freq ||
            s_scan.vfo_end_freq - s_scan.current_freq_hz < s_scan.vfo_step_hz) {
            s_scan.current_freq_hz = s_scan.vfo_start_freq;
        } else {
            s_scan.current_freq_hz += s_scan.vfo_step_hz;
        }
        if (at1846s_set_frequency(s_scan.current_freq_hz) != 0) scanner_stop();
    } else if (s_scan.type == SCAN_TYPE_CHANNEL) {
        bool tuned = false;
        uint16_t next_idx = s_scan.current_channel_idx;
        for (uint16_t attempt = 0; attempt < MAX_CHANNELS; attempt++) {
            next_idx++;
            if (next_idx > MAX_CHANNELS) next_idx = 1;

            channel_t ch;
            if (codeplug_load_channel(next_idx, &ch) <= 0 || ch.scan_skip ||
                !scan_frequency_supported(ch.rx_freq_hz)) {
                continue;
            }

            if (at1846s_set_frequency(ch.rx_freq_hz) != 0) {
                scanner_stop();
                return;
            }
            s_scan.current_channel_idx = next_idx;
            s_scan.current_freq_hz = ch.rx_freq_hz;
            tuned = true;
            break;
        }
        if (!tuned) scanner_stop();
    }
}
