/**
 * @file    ui_screen.h
 * @brief   User Interface & Display Rendering Engine - AT-D878 Open Firmware
 * @version 0.0.1
 *
 * 1.77" Renkli TFT LCD (160x128) için VFO, Kanal, S-Meter ve Menü ekranları.
 * Türkçe ve İngilizce dil desteği içerir.
 */

#ifndef UI_SCREEN_H
#define UI_SCREEN_H

#include <stdint.h>
#include <stdbool.h>
#include "radio_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_LANG_TURKISH = 0,
    UI_LANG_ENGLISH = 1
} ui_language_t;

typedef enum {
    UI_THEME_DARK = 0,
    UI_THEME_LIGHT = 1
} ui_theme_t;

typedef enum {
    SCREEN_DUAL_VFO = 0,
    SCREEN_CHANNEL_INFO,
    SCREEN_DMR_MONITOR,
    SCREEN_GPS_APRS,
    SCREEN_MENU_MAIN,
    SCREEN_ABOUT
} screen_type_t;

/**
 * @brief Kullanıcı arayüzü motorunu başlat
 */
int ui_init(ui_language_t lang, ui_theme_t theme);

/**
 * @brief Aktif ekranı çiz / tazele
 */
void ui_render(const radio_core_status_t *radio_status);

/**
 * @brief Ekrana geçiş yap
 */
void ui_switch_screen(screen_type_t screen);

/**
 * @brief Dil değiştir (Türkçe / İngilizce)
 */
void ui_set_language(ui_language_t lang);

/**
 * @brief S-Metre (Alış sinyal gücü barı) çizimi
 * @param rssi_dbm -120 dBm ile -50 dBm arası
 */
void ui_draw_smeter(int16_t rssi_dbm, int16_t x, int16_t y);

/**
 * @brief Pil durumu ikonunu çiz (Yüzde 0-100)
 */
void ui_draw_battery(uint8_t percentage, bool is_charging, int16_t x, int16_t y);

#ifdef __cplusplus
}
#endif

#endif /* UI_SCREEN_H */
