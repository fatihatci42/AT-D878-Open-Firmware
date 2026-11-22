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
#include "aprs_packet.h"
#include "codeplug_storage.h"
#include "dmr_protocol.h"
#include "scanner.h"
#include "hal_adc.h"
#include "hal_systick.h"
#include <string.h>

static radio_core_status_t s_radio;
static uint32_t s_tick_counter = 0;

int radio_core_init(void)
{
    memset(&s_radio, 0, sizeof(radio_core_status_t));
    s_radio.state = RADIO_STATE_STANDBY;
    s_radio.active_vfo = VFO_A;
    s_radio.current_volume = 10;
    s_radio.current_squelch = 3;

    /* Donanım alt sistemlerini başlat */
    hal_adc_init();
    s_radio.battery_mv = hal_adc_read_battery_mv();
    s_radio.battery_percent = hal_adc_get_battery_percentage();

    codeplug_init();
    dmr_protocol_init();
    scanner_init();

    /* Flash'tan VFO ve ayarları oku */
    radio_settings_t settings;
    if (codeplug_load_settings(&settings) == 0 && settings.vfo_a_freq_hz > 0) {
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

    /* Donanımı başlat ve aktif VFO'ya ayarla */
    at1846s_set_frequency(s_radio.vfo_channels[s_radio.active_vfo].rx_freq_hz);
    at1846s_set_bandwidth(s_radio.vfo_channels[s_radio.active_vfo].bandwidth == BANDWIDTH_25KHZ ? AT1846S_BW_25KHZ : AT1846S_BW_12_5KHZ);
    at1846s_set_squelch(s_radio.current_squelch);
    at1846s_rx_on();

    return 0;
}

void radio_core_process(void)
{
    s_tick_counter++;

    /* Periyodik batarya ölçümü (~500ms) */
    if ((s_tick_counter % 50) == 0) {
        s_radio.battery_mv = hal_adc_read_battery_mv();
        s_radio.battery_percent = hal_adc_get_battery_percentage();
    }

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

    /* Periyodik APRS Konum Beacon'ı (120 sn / 12000 tick) */
    if ((s_tick_counter % 12000) == 0 && s_radio.state == RADIO_STATE_STANDBY) {
        radio_core_send_aprs_beacon();
    }
}

int radio_core_set_active_vfo(vfo_index_t vfo)
{
    if (vfo > VFO_B) return -1;
    s_radio.active_vfo = vfo;

    /* Baseband modunu aktif VFO tipine göre yapılandır */
    if (s_radio.vfo_channels[vfo].mode == CH_MODE_DMR) {
        sct3258_set_mode(SCT3258_MODE_DMR_RX);
    } else {
        sct3258_set_mode(SCT3258_MODE_ANALOG_RX);
    }

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

    /* DMR modunda SCT3258 ve AMBE vocoder başlat */
    if (ch->mode == CH_MODE_DMR) {
        sct3258_set_dmr_params(ch->color_code, (sct3258_slot_t)ch->slot, ch->contact_id, SCT3258_CALL_GROUP);
        sct3258_dmr_tx_start();
    }

    audio_codec_set_path(AUDIO_PATH_MIC_INTERNAL, true);
    return 0;
}

int radio_core_stop_tx(void)
{
    channel_t *ch = &s_radio.vfo_channels[s_radio.active_vfo];

    /* DMR modunda SCT3258 iletimini durdur */
    if (ch->mode == CH_MODE_DMR) {
        sct3258_dmr_tx_stop();
        sct3258_set_mode(SCT3258_MODE_DMR_RX);
    }

    /* Analog FM ve CTCSS aktifse STE (Squelch Tail Elimination) gönder */
    if (ch->mode == CH_MODE_ANALOG_FM && ch->ctcss_tx_x10 > 0) {
        tones_transmit_ste_tail(STE_MODE_CTCSS_REVERSE_180, ch->ctcss_tx_x10);
    }

    at1846s_tx_off();
    audio_codec_set_path(AUDIO_PATH_MIC_INTERNAL, false);
    at1846s_set_frequency(ch->rx_freq_hz);
    at1846s_rx_on();
    s_radio.state = RADIO_STATE_STANDBY;
    return 0;
}

int radio_core_send_aprs_beacon(void)
{
    gps_data_t gps;
    double lat = 36.9914;  /* Adana, Türkiye (Varsayılan QTH) */
    double lon = 35.3308;
    float alt = 40.0f;

    if (linkboard_gps_get(&gps) == 0 && gps.valid) {
        lat = gps.latitude;
        lon = gps.longitude;
        alt = gps.altitude_m;
    }

    uint8_t pkt[APRS_MAX_PACKET_LEN];
    int len = aprs_build_position_packet(
        "TA5THK", 7, "APD878", "WIDE1-1,WIDE2-1",
        lat, lon, alt, '/', '>',
        "AT-D878 Open FW | 73 de TA5THK",
        pkt, sizeof(pkt)
    );

    /* LinkBoard alt modeme komut ilet */
    linkboard_aprs_send_position();

    /* 144.800 MHz RF frekansında doğrudan AFSK 1200 iletimi */
    if (len > 0 && s_radio.state == RADIO_STATE_STANDBY) {
        uint32_t saved_freq = s_radio.vfo_channels[s_radio.active_vfo].rx_freq_hz;
        s_radio.state = RADIO_STATE_TX;
        at1846s_rx_off();
        at1846s_set_frequency(144800000); /* 144.800 MHz APRS Standard */
        at1846s_set_bandwidth(AT1846S_BW_25KHZ);
        at1846s_tx_on();
        audio_codec_set_path(AUDIO_PATH_MIC_INTERNAL, false);
        hal_delay_ms(300); /* AFSK 1200 paket süresi */
        at1846s_tx_off();
        at1846s_set_frequency(saved_freq);
        at1846s_rx_on();
        s_radio.state = RADIO_STATE_STANDBY;
    }

    return len;
}

int radio_core_toggle_vfo(void)
{
    vfo_index_t next = (s_radio.active_vfo == VFO_A) ? VFO_B : VFO_A;
    return radio_core_set_active_vfo(next);
}

int radio_core_step_frequency(bool up)
{
    channel_t *ch = &s_radio.vfo_channels[s_radio.active_vfo];
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
    ch->tx_freq_hz = ch->rx_freq_hz;

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
