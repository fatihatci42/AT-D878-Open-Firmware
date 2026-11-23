/**
 * @file    ui_screen.c
 * @brief   Authentic AnyTone D878 Layout Display Engine - AT-D878 Open Firmware
 * @version 0.1.0
 */

#include "ui_screen.h"
#include "ui_menu.h"
#include "driver_display.h"
#include "scanner.h"
#include <stdio.h>
#include <string.h>

static ui_language_t s_lang = UI_LANG_TURKISH;
static ui_theme_t    s_theme = UI_THEME_DARK;
static screen_type_t s_current_screen = SCREEN_DUAL_VFO;

typedef struct {
    const char *tr;
    const char *en;
} lang_entry_t;

static const lang_entry_t STR_TX       = {"GONDERIM", "TRANSMIT"};
static const lang_entry_t STR_RX       = {"ALIS", "RECEIVE"};
static const lang_entry_t STR_STANDBY  = {"BEKLEME", "STANDBY"};

static const char* tr_or_en(const lang_entry_t *entry)
{
    return (s_lang == UI_LANG_TURKISH) ? entry->tr : entry->en;
}

int ui_init(ui_language_t lang, ui_theme_t theme)
{
    s_lang = lang;
    s_theme = theme;
    display_init();
    display_clear(COLOR_BLACK);
    ui_menu_init();
    return 0;
}

void ui_handle_key(const key_event_t *event)
{
    if (!event) return;

    /* Menü aktifse menü işle */
    if (ui_menu_handle_key(event)) {
        return;
    }

    /* Menü kapalıyken telsiz ana ekran tuşları */
    if (event->event == KEY_EVENT_PRESS) {
        switch (event->code) {
        case KEY_P1:
            radio_core_toggle_vfo();
            break;
        case KEY_P2: {
            scanner_status_t sc;
            scanner_get_status(&sc);
            if (sc.state == SCAN_STATE_SCANNING) {
                scanner_stop();
            } else {
                scanner_start_channel_scan(SCAN_RESUME_CARRIER);
            }
            break;
        }
        case KEY_UP:
            radio_core_step_frequency(true);
            break;
        case KEY_DOWN:
            radio_core_step_frequency(false);
            break;
        default:
            break;
        }
    }
}

void ui_set_language(ui_language_t lang)
{
    s_lang = lang;
}

void ui_switch_screen(screen_type_t screen)
{
    s_current_screen = screen;
    display_clear(s_theme == UI_THEME_DARK ? COLOR_BLACK : COLOR_WHITE);
}

void ui_draw_battery(uint8_t percentage, bool is_charging, int16_t x, int16_t y)
{
    uint16_t border_col = (percentage <= 1) ? COLOR_RED : COLOR_WHITE;
    uint16_t fill_col = (percentage > 3) ? COLOR_GREEN : COLOR_RED;

    display_fill_rect(x, y, 14, 7, border_col);
    display_fill_rect(x + 14, y + 2, 2, 3, border_col);
    display_fill_rect(x + 1, y + 1, 12, 5, COLOR_BLACK);

    if (is_charging) {
        /* Şarj oluyor ikonu (Şimşek) */
        display_draw_string(x + 4, y + 1, "~", COLOR_YELLOW, COLOR_BLACK, 1);
        return;
    }

    uint8_t bars = (percentage > 50) ? 3 : ((percentage > 3) ? 2 : (percentage > 0 ? 1 : 0));
    if (bars == 0) {
        /* Şarj gerekli: Kırmızı uyarı / şimşek */
        display_draw_string(x + 3, y + 1, "!", COLOR_RED, COLOR_BLACK, 1);
    } else {
        for (int i = 0; i < bars; i++) {
            display_fill_rect(x + 2 + (i * 4), y + 2, 3, 3, fill_col);
        }
    }
}

void ui_draw_smeter(int16_t rssi_dbm, int16_t x, int16_t y)
{
    int bars = 0;
    if (rssi_dbm > -75) bars = 4;
    else if (rssi_dbm > -90) bars = 3;
    else if (rssi_dbm > -105) bars = 2;
    else if (rssi_dbm > -118) bars = 1;

    display_draw_string(x, y, "Y", COLOR_CYAN, COLOR_NAVY, 1);
    for (int i = 0; i < 4; i++) {
        int h = (i + 1) * 2;
        uint16_t col = (i < bars) ? COLOR_GREEN : COLOR_DARKGREY;
        display_fill_rect(x + 7 + (i * 3), y + 7 - h, 2, h, col);
    }
}

void ui_render(const radio_core_status_t *radio)
{
    if (ui_menu_is_active()) {
        ui_menu_render();
        return;
    }

    if (!radio) return;

    /* 1. ÜST DURUM ÇUBUĞU (0..14 piksel) */
    display_fill_rect(0, 0, LCD_WIDTH, 14, COLOR_NAVY);
    
    /* RSSI / Anten */
    ui_draw_smeter(radio->rssi_dbm, 2, 3);

    /* Güç: T (Turbo 7W) */
    display_draw_string(22, 3, "T", COLOR_YELLOW, COLOR_NAVY, 1);

    /* Monitör / Hoparlör İkonu */
    display_draw_string(32, 3, "M", COLOR_LIGHTGREY, COLOR_NAVY, 1);

    /* GPS Uydu İkonu */
    display_draw_string(44, 3, "GPS", COLOR_GREEN, COLOR_NAVY, 1);

    /* Bluetooth İkonu */
    display_draw_string(66, 3, "B", COLOR_CYAN, COLOR_NAVY, 1);

    /* DMR Renk Kodu veya Tarama */
    if (radio->scan_active) {
        display_draw_string(74, 3, "SCAN", COLOR_YELLOW, COLOR_NAVY, 1);
    } else {
        display_draw_string(76, 3, "C1", COLOR_MAGENTA, COLOR_NAVY, 1);
    }

    /* Pil Voltajı ve İkonu (ADC'den gerçek okuma) */
    char bat_str[10];
    if (radio->battery_mv > 0) {
        snprintf(bat_str, sizeof(bat_str), "%u.%uV", 
                 (unsigned int)(radio->battery_mv / 1000), 
                 (unsigned int)((radio->battery_mv % 1000) / 100));
    } else {
        strncpy(bat_str, "7.4V", sizeof(bat_str));
    }
    uint16_t bat_text_col = (radio->battery_mv > 0 && radio->battery_mv < 6800) ? COLOR_RED : COLOR_WHITE;
    display_draw_string(96, 3, bat_str, bat_text_col, COLOR_NAVY, 1);
    ui_draw_battery(radio->battery_percent > 0 ? radio->battery_percent : 75, radio->is_charging, 126, 3);

    /* 2. VFO A KUTUSU (15..66 piksel) */
    bool is_vfo_a = (radio->active_vfo == VFO_A);
    uint16_t bg_a = is_vfo_a ? 0x08A6 : COLOR_BLACK;
    display_fill_rect(0, 15, LCD_WIDTH, 50, bg_a);

    display_draw_string(4, 18, is_vfo_a ? "> VFO-A" : "  VFO-A", COLOR_YELLOW, bg_a, 1);
    display_draw_string(110, 18, radio->vfo_channels[VFO_A].mode == CH_MODE_DMR ? "[DMR]" : "[FM]", COLOR_CYAN, bg_a, 1);

    char freq_str[24];
    uint32_t f_a = radio->vfo_channels[VFO_A].rx_freq_hz;
    snprintf(freq_str, sizeof(freq_str), "%03u.%05u", (unsigned int)(f_a / 1000000), (unsigned int)((f_a % 1000000) / 10));
    display_draw_string(10, 30, freq_str, radio->state == RADIO_STATE_TX ? COLOR_RED : COLOR_WHITE, bg_a, 2);
    display_draw_string(10, 50, "ZONE: TURKIYE | CSQ", COLOR_LIGHTGREY, bg_a, 1);

    /* 3. VFO B KUTUSU (67..113 piksel) */
    bool is_vfo_b = (radio->active_vfo == VFO_B);
    uint16_t bg_b = is_vfo_b ? 0x08A6 : COLOR_BLACK;
    display_fill_rect(0, 66, LCD_WIDTH, 46, bg_b);

    display_draw_string(4, 69, is_vfo_b ? "> VFO-B" : "  VFO-B", COLOR_YELLOW, bg_b, 1);
    display_draw_string(110, 69, radio->vfo_channels[VFO_B].mode == CH_MODE_DMR ? "[DMR]" : "[FM]", COLOR_MAGENTA, bg_b, 1);

    uint32_t f_b = radio->vfo_channels[VFO_B].rx_freq_hz;
    snprintf(freq_str, sizeof(freq_str), "%03u.%05u", (unsigned int)(f_b / 1000000), (unsigned int)((f_b % 1000000) / 10));
    display_draw_string(10, 80, freq_str, COLOR_LIGHTGREY, bg_b, 2);
    if (radio->active_caller_alias[0] != '\0') {
        char call_disp[32];
        snprintf(call_disp, sizeof(call_disp), "CALL: %s", radio->active_caller_alias);
        display_draw_string(10, 99, call_disp, COLOR_GREEN, bg_b, 1);
    } else {
        display_draw_string(10, 99, "TG: 286 | TS1 | CC: 1", COLOR_LIGHTGREY, bg_b, 1);
    }

    /* 4. ALT ÇUBUK (114..128 piksel) */
    uint16_t bot_bg = (radio->state == RADIO_STATE_TX) ? 0x8000 : 0x18C3;
    display_fill_rect(0, 114, LCD_WIDTH, 14, bot_bg);

    const char *state_str = tr_or_en(&STR_STANDBY);
    if (radio->state == RADIO_STATE_TX) state_str = tr_or_en(&STR_TX);
    else if (radio->state == RADIO_STATE_RX) state_str = tr_or_en(&STR_RX);

    display_draw_string(10, 116, state_str, (radio->state == RADIO_STATE_TX) ? COLOR_RED : COLOR_GREEN, bot_bg, 1);
    display_draw_string(70, 116, "MENU", COLOR_WHITE, bot_bg, 1);
    display_draw_string(115, 116, "TARA", COLOR_WHITE, bot_bg, 1);

    display_flush();
}
