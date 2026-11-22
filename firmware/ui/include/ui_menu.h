/**
 * @file    ui_menu.h
 * @brief   Hierarchical Menu Navigation Engine - AT-D878 Open Firmware
 * @version 0.1.0
 *
 * AnyTone D878 tarzı menü sistemi:
 * - Ayarlar (Ses, Işık, Güç, Squelch, Dil)
 * - Kanal & Zone
 * - DMR / Talkgroup / Kişiler
 * - APRS & GPS
 * - Bluetooth
 * - Cihaz Bilgisi & Hakkında
 */

#ifndef UI_MENU_H
#define UI_MENU_H

#include <stdint.h>
#include <stdbool.h>
#include "driver_keyboard.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MENU_MAX_ITEMS_PER_PAGE  5
#define MENU_MAX_TITLE_LEN       24
#define MENU_MAX_ITEM_TEXT_LEN   24

typedef enum {
    MENU_ACTION_NONE = 0,
    MENU_ACTION_SUBMENU,
    MENU_ACTION_TOGGLE,
    MENU_ACTION_VALUE_INT,
    MENU_ACTION_CALL_FUNC,
    MENU_ACTION_BACK
} menu_item_type_t;

typedef struct menu_node menu_node_t;

typedef void (*menu_func_t)(void);

typedef struct {
    const char       *text_tr;
    const char       *text_en;
    menu_item_type_t  type;
    menu_node_t      *submenu;
    menu_func_t       action_func;
    int32_t          *val_ptr;
    int32_t           val_min;
    int32_t           val_max;
    int32_t           val_step;
} menu_item_t;

struct menu_node {
    const char       *title_tr;
    const char       *title_en;
    menu_node_t      *parent;
    const menu_item_t *items;
    uint8_t           item_count;
    uint8_t           selected_index;
    uint8_t           scroll_offset;
};

/**
 * @brief Menü motorunu başlat
 */
void ui_menu_init(void);

/**
 * @brief Menü tuş olayını işle
 * @return true tuş tüketildi, false menü kapalı
 */
bool ui_menu_handle_key(const key_event_t *event);

/**
 * @brief Aktif menüyü LCD'ye çiz
 */
void ui_menu_render(void);

/**
 * @brief Menü açık mı?
 */
bool ui_menu_is_active(void);

/**
 * @brief Menüyü aç
 */
void ui_menu_open(void);

/**
 * @brief Menüyü kapat ve ana VFO ekranına dön
 */
void ui_menu_close(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_MENU_H */
