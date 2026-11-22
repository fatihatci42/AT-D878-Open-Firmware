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
    s_scan.current_channel_idx = 1;
    s_last_step_time = hal_tick_get();
    return 0;
}

int scanner_start_vfo_scan(uint32_t start_hz, uint32_t end_hz, uint32_t step_hz, scan_resume_mode_t resume)
{
    s_scan.state = SCAN_STATE_SCANNING;
    s_scan.type = SCAN_TYPE_VFO;
    s_scan.resume_mode = resume;
    s_scan.vfo_start_freq = start_hz;
    s_scan.vfo_end_freq = end_hz;
    s_scan.vfo_step_hz = (step_hz == 0) ? 12500 : step_hz;
    s_scan.current_freq_hz = start_hz;
    s_last_step_time = hal_tick_get();
    at1846s_set_frequency(start_hz);
    return 0;
}

int scanner_start_tone_scan(void)
{
    s_scan.state = SCAN_STATE_SCANNING;
    s_scan.type = SCAN_TYPE_TONE;
    s_scan.found_ctcss_x10 = 0;
    s_last_step_time = hal_tick_get();
    return 0;
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
        s_scan.current_freq_hz += s_scan.vfo_step_hz;
        if (s_scan.current_freq_hz > s_scan.vfo_end_freq) {
            s_scan.current_freq_hz = s_scan.vfo_start_freq;
        }
        at1846s_set_frequency(s_scan.current_freq_hz);
    } else if (s_scan.type == SCAN_TYPE_CHANNEL) {
        channel_t ch;
        int next_idx = s_scan.current_channel_idx + 1;
        if (next_idx > 20) next_idx = 1;  /* Örnek döngü */
        s_scan.current_channel_idx = (uint16_t)next_idx;
        if (codeplug_load_channel((uint16_t)next_idx, &ch) == 0) {
            at1846s_set_frequency(ch.rx_freq_hz);
        }
    }
}
