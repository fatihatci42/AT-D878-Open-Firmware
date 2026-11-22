/**
 * @file    radio_core.c
 * @brief   Radio Core State Machine Implementation - AT-D878 Open Firmware
 * @version 0.0.1
 */

#include "radio_core.h"
#include "driver_at1846s.h"
#include "driver_audio.h"
#include "driver_keyboard.h"
#include <string.h>

static radio_core_status_t s_radio;

int radio_core_init(void)
{
    memset(&s_radio, 0, sizeof(radio_core_status_t));
    s_radio.state = RADIO_STATE_STANDBY;
    s_radio.active_vfo = VFO_A;
    s_radio.current_volume = 10;
    s_radio.current_squelch = 3;

    /* Varsayılan VFO-A kanalı: 145.500 MHz FM */
    s_radio.vfo_channels[VFO_A].index = 0;
    strncpy(s_radio.vfo_channels[VFO_A].name, "VFO-A 2M", MAX_CHANNEL_NAME_LEN);
    s_radio.vfo_channels[VFO_A].rx_freq_hz = 145500000;
    s_radio.vfo_channels[VFO_A].tx_freq_hz = 145500000;
    s_radio.vfo_channels[VFO_A].mode = CH_MODE_ANALOG_FM;
    s_radio.vfo_channels[VFO_A].power = POWER_MID;
    s_radio.vfo_channels[VFO_A].bandwidth = BANDWIDTH_25KHZ;

    /* Varsayılan VFO-B kanalı: 433.500 MHz FM */
    s_radio.vfo_channels[VFO_B].index = 1;
    strncpy(s_radio.vfo_channels[VFO_B].name, "VFO-B 70CM", MAX_CHANNEL_NAME_LEN);
    s_radio.vfo_channels[VFO_B].rx_freq_hz = 433500000;
    s_radio.vfo_channels[VFO_B].tx_freq_hz = 433500000;
    s_radio.vfo_channels[VFO_B].mode = CH_MODE_ANALOG_FM;
    s_radio.vfo_channels[VFO_B].power = POWER_MID;
    s_radio.vfo_channels[VFO_B].bandwidth = BANDWIDTH_25KHZ;

    /* Donanımı başlat ve aktif VFO'ya ayarla */
    at1846s_set_frequency(s_radio.vfo_channels[VFO_A].rx_freq_hz);
    at1846s_set_bandwidth(s_radio.vfo_channels[VFO_A].bandwidth == BANDWIDTH_25KHZ ? AT1846S_BW_25KHZ : AT1846S_BW_12_5KHZ);
    at1846s_set_squelch(s_radio.current_squelch);
    at1846s_rx_on();

    return 0;
}

void radio_core_process(void)
{
    /* PTT kontrolü */
    bool ptt_pressed = keyboard_is_ptt_pressed();
    if (ptt_pressed && s_radio.state != RADIO_STATE_TX && !s_radio.ptt_locked) {
        radio_core_start_tx();
    } else if (!ptt_pressed && s_radio.state == RADIO_STATE_TX) {
        radio_core_stop_tx();
    }

    /* RX durumunda squelch ve RSSI kontrolü */
    if (s_radio.state != RADIO_STATE_TX) {
        s_radio.rssi_dbm = at1846s_get_rssi();
        bool squelch_open = at1846s_get_squelch_status();
        if (squelch_open) {
            s_radio.state = RADIO_STATE_RX;
            audio_codec_set_path(AUDIO_PATH_SPEAKER, true);
        } else {
            s_radio.state = RADIO_STATE_STANDBY;
            audio_codec_set_path(AUDIO_PATH_SPEAKER, false);
        }
    }
}

int radio_core_set_active_vfo(vfo_index_t vfo)
{
    if (vfo > VFO_B) return -1;
    s_radio.active_vfo = vfo;
    return at1846s_set_frequency(s_radio.vfo_channels[vfo].rx_freq_hz);
}

int radio_core_start_tx(void)
{
    channel_t *ch = &s_radio.vfo_channels[s_radio.active_vfo];
    if (ch->rx_only) return -1;

    s_radio.state = RADIO_STATE_TX;
    at1846s_rx_off();
    at1846s_set_frequency(ch->tx_freq_hz);
    at1846s_tx_on();
    audio_codec_set_path(AUDIO_PATH_MIC_INTERNAL, true);
    return 0;
}

int radio_core_stop_tx(void)
{
    channel_t *ch = &s_radio.vfo_channels[s_radio.active_vfo];
    at1846s_tx_off();
    audio_codec_set_path(AUDIO_PATH_MIC_INTERNAL, false);
    at1846s_set_frequency(ch->rx_freq_hz);
    at1846s_rx_on();
    s_radio.state = RADIO_STATE_STANDBY;
    return 0;
}

int radio_core_toggle_vfo(void)
{
    vfo_index_t next = (s_radio.active_vfo == VFO_A) ? VFO_B : VFO_A;
    return radio_core_set_active_vfo(next);
}

int radio_core_step_frequency(bool up)
{
    channel_t *ch = &s_radio.vfo_channels[s_radio.active_vfo];
    /* Varsayılan adım: 12.5 kHz (dar bant) veya 25 kHz (geniş bant) */
    uint32_t step = (ch->bandwidth == BANDWIDTH_25KHZ) ? 25000 : 12500;

    if (up) {
        ch->rx_freq_hz += step;
        ch->tx_freq_hz += step;
    } else {
        ch->rx_freq_hz -= step;
        ch->tx_freq_hz -= step;
    }

    /* Bant sınır kontrolü */
    if (ch->rx_freq_hz < 134000000) ch->rx_freq_hz = 134000000;
    if (ch->rx_freq_hz > 520000000) ch->rx_freq_hz = 520000000;
    ch->tx_freq_hz = ch->rx_freq_hz;  /* Simplex varsayılan */

    if (s_radio.state != RADIO_STATE_TX) {
        at1846s_set_frequency(ch->rx_freq_hz);
    }

    return 0;
}

void radio_core_get_status(radio_core_status_t *status)
{
    if (status) {
        memcpy(status, &s_radio, sizeof(radio_core_status_t));
    }
}
