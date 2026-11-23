/**
 * @file    radio_core.c
 * @brief   Radio Core State Machine Implementation - AT-D878 Open Firmware
 * @version 0.0.1
 */

#include "radio_core.h"
#include "driver_at1846s.h"
#include "driver_audio.h"
#include "driver_keyboard.h"
#include "driver_tones.h"
#include "driver_sct3258.h"
#include "driver_linkboard.h"
#include "calibration.h"
#include "codeplug_storage.h"
#include "dmr_protocol.h"
#include "scanner.h"
#include "hal_adc.h"
#include "hal_systick.h"
#include <string.h>

static radio_core_status_t s_radio;
static tick_t s_last_aprs_beacon_ms = 0;
static uint32_t s_aprs_beacon_interval_ms = 0;
static tick_t s_tx_started_ms = 0;
static uint32_t s_tx_timeout_ms = 120000U;

static bool radio_frequency_supported(uint32_t freq_hz)
{
    return (freq_hz >= 134000000U && freq_hz <= 174000000U) ||
           (freq_hz >= 400000000U && freq_hz <= 520000000U);
}

static int radio_apply_vfo_channel(vfo_index_t vfo)
{
    channel_t *ch = &s_radio.vfo_channels[vfo];
    if (!radio_frequency_supported(ch->rx_freq_hz)) return -1;

    if (ch->mode == CH_MODE_DMR) {
        if (sct3258_set_dmr_params(ch->color_code, (sct3258_slot_t)ch->slot,
                                   ch->contact_id, SCT3258_CALL_GROUP) != 0 ||
            sct3258_set_mode(SCT3258_MODE_DMR_RX) != 0) {
            return -1;
        }
    } else {
        (void)sct3258_set_mode(SCT3258_MODE_ANALOG_RX);
    }

    if (at1846s_set_frequency(ch->rx_freq_hz) != 0 ||
        at1846s_set_bandwidth(ch->bandwidth == BANDWIDTH_25KHZ ?
                              AT1846S_BW_25KHZ : AT1846S_BW_12_5KHZ) != 0 ||
        at1846s_set_squelch(s_radio.current_squelch) != 0 ||
        at1846s_rx_on() != 0) {
        return -1;
    }
    return 0;
}

int radio_core_init(void)
{
    memset(&s_radio, 0, sizeof(radio_core_status_t));
    s_radio.state = RADIO_STATE_STANDBY;
    s_radio.active_vfo = VFO_A;
    s_radio.current_volume = 10;
    s_radio.current_squelch = 3;

    s_last_aprs_beacon_ms = hal_tick_get();
    s_aprs_beacon_interval_ms = 0;
    s_tx_started_ms = 0;
    s_tx_timeout_ms = 120000U;
    aprs_config_t aprs_config;
    if (linkboard_aprs_get_config(&aprs_config) == 0) {
        s_aprs_beacon_interval_ms = (uint32_t)aprs_config.beacon_interval_s * 1000U;
    }

    /* Donanım alt sistemlerini başlat */
    hal_adc_init();
    s_radio.battery_mv = hal_adc_read_battery_mv();
    s_radio.battery_percent = hal_adc_get_battery_percentage();

    if (codeplug_init() != 0) return -1;
    calibration_init();
    dmr_protocol_init();
    scanner_init();

    /* Flash'tan VFO ve ayarları oku */
    radio_settings_t settings;
    if (codeplug_load_settings(&settings) == 0 &&
        radio_frequency_supported(settings.vfo_a_freq_hz)) {
        if (settings.tot_seconds >= 30 && settings.tot_seconds <= 180) {
            s_tx_timeout_ms = (uint32_t)settings.tot_seconds * 1000U;
        }
        if (settings.default_squelch <= 8) {
            s_radio.current_squelch = settings.default_squelch;
        }
        s_radio.vfo_channels[VFO_A].index = 0;
        strncpy(s_radio.vfo_channels[VFO_A].name, "VFO-A", MAX_CHANNEL_NAME_LEN);
        s_radio.vfo_channels[VFO_A].rx_freq_hz = settings.vfo_a_freq_hz;
        s_radio.vfo_channels[VFO_A].tx_freq_hz = settings.vfo_a_freq_hz;
        s_radio.vfo_channels[VFO_A].mode = CH_MODE_ANALOG_FM;
        s_radio.vfo_channels[VFO_A].power = POWER_TURBO;
        s_radio.vfo_channels[VFO_A].bandwidth = BANDWIDTH_25KHZ;
        s_radio.vfo_channels[VFO_A].ctcss_tx_x10 = 885; /* 88.5 Hz */

        s_radio.vfo_channels[VFO_B].index = 1;
        strncpy(s_radio.vfo_channels[VFO_B].name, "VFO-B", MAX_CHANNEL_NAME_LEN);
        s_radio.vfo_channels[VFO_B].rx_freq_hz = settings.vfo_b_freq_hz > 0 ? settings.vfo_b_freq_hz : 433500000;
        s_radio.vfo_channels[VFO_B].tx_freq_hz = s_radio.vfo_channels[VFO_B].rx_freq_hz;
        s_radio.vfo_channels[VFO_B].mode = CH_MODE_DMR;
        s_radio.vfo_channels[VFO_B].color_code = 1;
        s_radio.vfo_channels[VFO_B].slot = 1;
        s_radio.vfo_channels[VFO_B].contact_id = 286; /* TG 286 TR */
        s_radio.vfo_channels[VFO_B].power = POWER_TURBO;
        s_radio.vfo_channels[VFO_B].bandwidth = BANDWIDTH_12_5KHZ;
        s_radio.active_vfo = (settings.active_vfo == 1) ? VFO_B : VFO_A;
    } else {
        /* Varsayılan VFO-A kanalı: 145.500 MHz FM (Türkiye VHF Röle/Simplex) */
        s_radio.vfo_channels[VFO_A].index = 0;
        strncpy(s_radio.vfo_channels[VFO_A].name, "VFO-A 2M", MAX_CHANNEL_NAME_LEN);
        s_radio.vfo_channels[VFO_A].rx_freq_hz = 145500000;
        s_radio.vfo_channels[VFO_A].tx_freq_hz = 145500000;
        s_radio.vfo_channels[VFO_A].mode = CH_MODE_ANALOG_FM;
        s_radio.vfo_channels[VFO_A].power = POWER_TURBO;
        s_radio.vfo_channels[VFO_A].bandwidth = BANDWIDTH_25KHZ;
        s_radio.vfo_channels[VFO_A].ctcss_tx_x10 = 885;

        /* Varsayılan VFO-B kanalı: 433.500 MHz DMR (Türkiye UHF DMR) */
        s_radio.vfo_channels[VFO_B].index = 1;
        strncpy(s_radio.vfo_channels[VFO_B].name, "VFO-B 70CM", MAX_CHANNEL_NAME_LEN);
        s_radio.vfo_channels[VFO_B].rx_freq_hz = 433500000;
        s_radio.vfo_channels[VFO_B].tx_freq_hz = 433500000;
        s_radio.vfo_channels[VFO_B].mode = CH_MODE_DMR;
        s_radio.vfo_channels[VFO_B].color_code = 1;
        s_radio.vfo_channels[VFO_B].slot = 1;
        s_radio.vfo_channels[VFO_B].contact_id = 286;
        s_radio.vfo_channels[VFO_B].power = POWER_TURBO;
        s_radio.vfo_channels[VFO_B].bandwidth = BANDWIDTH_12_5KHZ;
    }

    s_radio.vfo_channels[VFO_A].squelch_level = s_radio.current_squelch;
    s_radio.vfo_channels[VFO_B].squelch_level = s_radio.current_squelch;

    /* Donanımı başlat ve aktif VFO'ya ayarla */
    if (radio_apply_vfo_channel(s_radio.active_vfo) != 0) {
        if (s_radio.active_vfo == VFO_B) {
            s_radio.active_vfo = VFO_A;
            if (radio_apply_vfo_channel(VFO_A) == 0) return 0;
        }
        s_radio.state = RADIO_STATE_ERROR;
        return -1;
    }

    return 0;
}

void radio_core_process(void)
{
    /* Periyodik batarya ölçümü (~500ms) */
    static tick_t last_battery_update_ms = 0;
    tick_t now = hal_tick_get();
    if (hal_tick_elapsed(last_battery_update_ms, now) >= 500U) {
        last_battery_update_ms = now;
        s_radio.battery_mv = hal_adc_read_battery_mv();
        s_radio.battery_percent = hal_adc_get_battery_percentage();
    }

    if (s_radio.state == RADIO_STATE_TX &&
        hal_tick_elapsed(s_tx_started_ms, now) >= s_tx_timeout_ms) {
        radio_core_stop_tx();
        s_radio.ptt_locked = true;
    }

    /* PTT kontrolü */
    bool ptt_pressed = keyboard_is_ptt_pressed();
    if (ptt_pressed) {
        if (s_radio.state != RADIO_STATE_TX && !s_radio.ptt_locked) {
            radio_core_start_tx();
        }
    } else {
        if (s_radio.state == RADIO_STATE_TX) radio_core_stop_tx();
        s_radio.ptt_locked = false;
    }

    /* RX durumunda squelch ve RSSI kontrolü */
    if (s_radio.state != RADIO_STATE_TX && s_radio.state != RADIO_STATE_ERROR) {
        s_radio.rssi_dbm = at1846s_get_rssi();
        bool squelch_open = at1846s_get_squelch_status();
        if (squelch_open) {
            s_radio.state = RADIO_STATE_RX;
            audio_codec_set_path(AUDIO_PATH_SPEAKER, true);
        } else {
            s_radio.state = RADIO_STATE_STANDBY;
            audio_codec_set_path(AUDIO_PATH_SPEAKER, false);
        }

        /* DMR aktif çağrı ve Talker Alias güncelleme */
        dmr_call_info_t active_call;
        if (dmr_protocol_get_active_call(&active_call) == 0 && active_call.is_active) {
            memcpy(s_radio.active_caller_alias, active_call.talker_alias, sizeof(s_radio.active_caller_alias) - 1);
            s_radio.active_caller_alias[sizeof(s_radio.active_caller_alias) - 1] = '\0';
            s_radio.active_tg = active_call.dst_id;
        }
    }

    /* Tarayıcı durum makinesi tick */
    scanner_process();
    scanner_status_t sc_stat;
    scanner_get_status(&sc_stat);
    s_radio.scan_active = (sc_stat.state == SCAN_STATE_SCANNING);

    /* APRS aralığı yapılandırmadaki saniye değerinden alınır. */
    if (s_aprs_beacon_interval_ms > 0 &&
        s_radio.state == RADIO_STATE_STANDBY &&
        hal_tick_elapsed(s_last_aprs_beacon_ms, now) >= s_aprs_beacon_interval_ms) {
        s_last_aprs_beacon_ms = now;
        radio_core_send_aprs_beacon();
    }
}

int radio_core_set_active_vfo(vfo_index_t vfo)
{
    if (vfo > VFO_B || s_radio.state == RADIO_STATE_TX ||
        s_radio.state == RADIO_STATE_ERROR) return -1;
    uint8_t previous_squelch = s_radio.current_squelch;
    s_radio.current_squelch = s_radio.vfo_channels[vfo].squelch_level;
    if (radio_apply_vfo_channel(vfo) != 0) {
        s_radio.current_squelch = previous_squelch;
        s_radio.state = RADIO_STATE_ERROR;
        return -1;
    }
    s_radio.active_vfo = vfo;
    return 0;
}

int radio_core_set_channel(vfo_index_t vfo, const channel_t *channel)
{
    if (vfo > VFO_B || !channel || s_radio.state == RADIO_STATE_TX ||
        s_radio.state == RADIO_STATE_ERROR ||
        !radio_frequency_supported(channel->rx_freq_hz) ||
        (!channel->rx_only && !radio_frequency_supported(channel->tx_freq_hz)) ||
        (channel->mode != CH_MODE_ANALOG_FM && channel->mode != CH_MODE_DMR) ||
        (channel->bandwidth != BANDWIDTH_12_5KHZ &&
         channel->bandwidth != BANDWIDTH_25KHZ) || channel->squelch_level > 8 ||
        (channel->mode == CH_MODE_DMR &&
         (channel->color_code > 15 || (channel->slot != 1 && channel->slot != 2)))) {
        return -1;
    }

    channel_t previous = s_radio.vfo_channels[vfo];
    uint8_t previous_squelch = s_radio.current_squelch;
    s_radio.vfo_channels[vfo] = *channel;
    s_radio.vfo_channels[vfo].name[MAX_CHANNEL_NAME_LEN - 1] = '\0';
    if (vfo == s_radio.active_vfo) {
        s_radio.current_squelch = channel->squelch_level;
    }
    if (vfo == s_radio.active_vfo && radio_apply_vfo_channel(vfo) != 0) {
        s_radio.vfo_channels[vfo] = previous;
        s_radio.current_squelch = previous_squelch;
        s_radio.state = RADIO_STATE_ERROR;
        return -1;
    }
    return 0;
}

int radio_core_set_vfo_frequency(vfo_index_t vfo, uint32_t rx_hz, uint32_t tx_hz)
{
    if (vfo > VFO_B || s_radio.state == RADIO_STATE_TX ||
        s_radio.state == RADIO_STATE_ERROR || !radio_frequency_supported(rx_hz) ||
        (!s_radio.vfo_channels[vfo].rx_only && !radio_frequency_supported(tx_hz))) {
        return -1;
    }

    channel_t previous = s_radio.vfo_channels[vfo];
    s_radio.vfo_channels[vfo].rx_freq_hz = rx_hz;
    s_radio.vfo_channels[vfo].tx_freq_hz = tx_hz;
    if (vfo == s_radio.active_vfo && radio_apply_vfo_channel(vfo) != 0) {
        s_radio.vfo_channels[vfo] = previous;
        s_radio.state = RADIO_STATE_ERROR;
        return -1;
    }
    return 0;
}

int radio_core_start_tx(void)
{
    channel_t *ch = &s_radio.vfo_channels[s_radio.active_vfo];
    if (s_radio.state == RADIO_STATE_TX) return 0;
    if (s_radio.state == RADIO_STATE_ERROR) return -1;
    if (ch->rx_only || !radio_frequency_supported(ch->rx_freq_hz) ||
        !radio_frequency_supported(ch->tx_freq_hz)) {
        return -1;
    }

    if (audio_codec_set_path(AUDIO_PATH_SPEAKER, false) != 0) return -4;

    if (at1846s_rx_off() != 0) return -2;
    if (at1846s_set_frequency(ch->tx_freq_hz) != 0) {
        at1846s_set_frequency(ch->rx_freq_hz);
        at1846s_rx_on();
        return -2;
    }

    /* DMR modunda SCT3258 ve AMBE vocoder başlat */
    if (ch->mode == CH_MODE_DMR) {
        if (sct3258_set_dmr_params(ch->color_code, (sct3258_slot_t)ch->slot,
                                   ch->contact_id, SCT3258_CALL_GROUP) != 0 ||
            sct3258_dmr_tx_start() != 0) {
            sct3258_set_mode(SCT3258_MODE_DMR_RX);
            at1846s_set_frequency(ch->rx_freq_hz);
            at1846s_rx_on();
            return -3;
        }
    }

    /* RF Güç Kalibrasyonunu PA4 (DAC0) üzerinden uygula */
    calibration_apply_tx_power(ch->tx_freq_hz, ch->power);

    /* Transmit Permit Tone (TPT) cıvıltısını yürüt */
    tones_play_tpt(TPT_MODE_MOTOTRBO_CHIRP);

    if (at1846s_tx_on() != 0) {
        if (ch->mode == CH_MODE_DMR) {
            sct3258_dmr_tx_stop();
            sct3258_set_mode(SCT3258_MODE_DMR_RX);
        }
        at1846s_set_frequency(ch->rx_freq_hz);
        at1846s_rx_on();
        return -2;
    }

    if (audio_codec_set_path(AUDIO_PATH_MIC_INTERNAL, true) != 0) {
        at1846s_tx_off();
        if (ch->mode == CH_MODE_DMR) {
            sct3258_dmr_tx_stop();
            sct3258_set_mode(SCT3258_MODE_DMR_RX);
        }
        at1846s_set_frequency(ch->rx_freq_hz);
        at1846s_rx_on();
        return -2;
    }

    s_tx_started_ms = hal_tick_get();
    s_radio.state = RADIO_STATE_TX;
    return 0;
}

int radio_core_stop_tx(void)
{
    if (s_radio.state != RADIO_STATE_TX) return 0;
    channel_t *ch = &s_radio.vfo_channels[s_radio.active_vfo];
    int dmr_stop_rc = 0;

    /* DMR modunda SCT3258 iletimini durdur */
    if (ch->mode == CH_MODE_DMR) {
        dmr_stop_rc = sct3258_dmr_tx_stop();
        if (sct3258_set_mode(SCT3258_MODE_DMR_RX) != 0) dmr_stop_rc = -1;
    }

    /* Analog FM ve CTCSS aktifse STE (Squelch Tail Elimination) gönder */
    if (ch->mode == CH_MODE_ANALOG_FM && ch->ctcss_tx_x10 > 0) {
        tones_transmit_ste_tail(STE_MODE_CTCSS_REVERSE_180, ch->ctcss_tx_x10);
    }

    int tx_off_rc = at1846s_tx_off();
    calibration_apply_tx_power(ch->rx_freq_hz, POWER_LOW); /* PA bias kapat */
    int mic_off_rc = audio_codec_set_path(AUDIO_PATH_MIC_INTERNAL, false);
    if (tx_off_rc != 0 || mic_off_rc != 0 || dmr_stop_rc != 0 ||
        at1846s_set_frequency(ch->rx_freq_hz) != 0 ||
        at1846s_rx_on() != 0) {
        s_radio.state = RADIO_STATE_ERROR;
        return -1;
    }
    s_radio.state = RADIO_STATE_STANDBY;
    return 0;
}

int radio_core_send_aprs_beacon(void)
{
    gps_data_t gps;
    if (s_radio.state != RADIO_STATE_STANDBY ||
        linkboard_gps_get(&gps) != 0 || !gps.valid) {
        return -1;
    }

    /* LinkBoard/TNC transmits the position; do not key the RF carrier directly. */
    return linkboard_aprs_send_position();
}

int radio_core_toggle_vfo(void)
{
    vfo_index_t next = (s_radio.active_vfo == VFO_A) ? VFO_B : VFO_A;
    return radio_core_set_active_vfo(next);
}

int radio_core_step_frequency(bool up)
{
    if (s_radio.state == RADIO_STATE_TX || s_radio.state == RADIO_STATE_ERROR) return -1;
    channel_t *ch = &s_radio.vfo_channels[s_radio.active_vfo];
    uint32_t step = (ch->bandwidth == BANDWIDTH_25KHZ) ? 25000 : 12500;
    uint32_t band_min, band_max;
    if (ch->rx_freq_hz >= 134000000U && ch->rx_freq_hz <= 174000000U) {
        band_min = 134000000U;
        band_max = 174000000U;
    } else if (ch->rx_freq_hz >= 400000000U && ch->rx_freq_hz <= 520000000U) {
        band_min = 400000000U;
        band_max = 520000000U;
    } else {
        return -1;
    }

    uint32_t next = ch->rx_freq_hz;
    if (up) {
        next = (band_max - next < step) ? band_max : next + step;
    } else {
        next = (next - band_min < step) ? band_min : next - step;
    }
    return radio_core_set_vfo_frequency(s_radio.active_vfo, next, next);
}

void radio_core_get_status(radio_core_status_t *status)
{
    if (status) {
        memcpy(status, &s_radio, sizeof(radio_core_status_t));
    }
}
