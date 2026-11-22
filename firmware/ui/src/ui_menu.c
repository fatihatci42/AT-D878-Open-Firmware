/**
 * @file    ui_menu.c
 * @brief   Hierarchical Menu Navigation Engine Implementation - AT-D878 Open Firmware
 * @version 0.1.0
 */

#include "ui_menu.h"
#include "ui_screen.h"
#include "driver_display.h"
#include "driver_at1846s.h"
#include "driver_sct3258.h"
#include "driver_linkboard.h"
#include <stdio.h>
#include <string.h>

/* ============================================================
 * Menü Parametreleri ve Değişkenleri
 * ============================================================ */

static int32_t s_tx_power = 0;        /* 0=Turbo, 1=High, 2=Mid, 3=Low */
static int32_t s_squelch = 3;         /* 0-8 */
static int32_t s_beep = 1;            /* 0=Kapalı, 1=Açık */
static int32_t s_backlight_time = 10; /* 5, 10, 30, 0=Sürekli */
static int32_t s_brightness = 4;      /* 1-5 */
static int32_t s_lang_sel = 0;        /* 0=Türkçe, 1=English */

static int32_t s_dmr_cc = 1;          /* 1-15 */
static int32_t s_dmr_slot = 1;        /* 1, 2 */
static int32_t s_dmr_tg = 286;        /* Talkgroup: 286 Turkey */

static int32_t s_bt_power = 0;        /* 0=Kapalı, 1=Açık */
static int32_t s_gps_power = 1;       /* 0=Kapalı, 1=Açık */

static bool s_menu_active = false;
static menu_node_t *s_current_node = NULL;

/* ============================================================
 * Menü Aksiyon Fonksiyonları
 * ============================================================ */

static void action_send_aprs(void)
{
    linkboard_aprs_send_position();
}

static void action_apply_radio(void)
{
    at1846s_set_squelch((uint8_t)s_squelch);
}

static void action_apply_dmr(void)
{
    sct3258_set_dmr_params((uint8_t)s_dmr_cc,
                           (s_dmr_slot == 1) ? SCT3258_SLOT_1 : SCT3258_SLOT_2,
                           (uint32_t)s_dmr_tg,
                           SCT3258_CALL_GROUP);
}

static void action_toggle_bt(void)
{
    s_bt_power = !s_bt_power;
    linkboard_bt_power(s_bt_power != 0);
}

/* ============================================================
 * Menü Hiyerarşi Tanımları
 * ============================================================ */

/* İleri bildirimler */
static menu_node_t node_main;
static menu_node_t node_radio_set;
static menu_node_t node_dmr_set;
static menu_node_t node_aprs_set;
static menu_node_t node_bt_set;
static menu_node_t node_about;

/* 1. Telsiz Ayarları */
static const menu_item_t items_radio_set[] = {
    { "TX Gucu",       "TX Power",       MENU_ACTION_VALUE_INT,  NULL, action_apply_radio, &s_tx_power, 0, 3, 1 },
    { "Squelch",       "Squelch",        MENU_ACTION_VALUE_INT,  NULL, action_apply_radio, &s_squelch,  0, 8, 1 },
    { "Tus Sesi",      "Key Beep",       MENU_ACTION_TOGGLE,     NULL, NULL,               &s_beep,     0, 1, 1 },
    { "Ekran Isigi",   "Backlight (s)",  MENU_ACTION_VALUE_INT,  NULL, NULL,               &s_backlight_time, 0, 30, 5 },
    { "Parlaklik",     "Brightness",     MENU_ACTION_VALUE_INT,  NULL, NULL,               &s_brightness, 1, 5, 1 },
    { "Dil / Lang",    "Language",       MENU_ACTION_TOGGLE,     NULL, NULL,               &s_lang_sel, 0, 1, 1 },
};

/* 2. DMR Ayarları */
static const menu_item_t items_dmr_set[] = {
    { "Renk Kodu",     "Color Code",     MENU_ACTION_VALUE_INT,  NULL, action_apply_dmr,   &s_dmr_cc,   1, 15, 1 },
    { "Zaman Slotu",   "Time Slot",      MENU_ACTION_VALUE_INT,  NULL, action_apply_dmr,   &s_dmr_slot, 1, 2,  1 },
    { "Grup (TG)",     "TalkGroup",      MENU_ACTION_VALUE_INT,  NULL, action_apply_dmr,   &s_dmr_tg,   1, 999999, 1 },
};

/* 3. APRS Ayarları */
static const menu_item_t items_aprs_set[] = {
    { "Konum Gonder",  "Send Beacon",    MENU_ACTION_CALL_FUNC,  NULL, action_send_aprs,   NULL, 0, 0, 0 },
    { "GPS Gucu",      "GPS Power",      MENU_ACTION_TOGGLE,     NULL, NULL,               &s_gps_power, 0, 1, 1 },
    { "Frekans: 144.8","Freq: 144.800",  MENU_ACTION_NONE,       NULL, NULL,               NULL, 0, 0, 0 },
    { "Cagri: TA5THK", "Call: TA5THK",   MENU_ACTION_NONE,       NULL, NULL,               NULL, 0, 0, 0 },
};

/* 4. Bluetooth Ayarları */
static const menu_item_t items_bt_set[] = {
    { "BT Gucu",       "BT Power",       MENU_ACTION_TOGGLE,     NULL, action_toggle_bt,   &s_bt_power, 0, 1, 1 },
    { "Baglanti: Yok", "Status: Idle",   MENU_ACTION_NONE,       NULL, NULL,               NULL, 0, 0, 0 },
};

/* 5. Cihaz Bilgisi */
static const menu_item_t items_about[] = {
    { "Model: D878UV2","Model: D878UV2", MENU_ACTION_NONE,       NULL, NULL,               NULL, 0, 0, 0 },
    { "FW: Open v0.1", "FW: Open v0.1",  MENU_ACTION_NONE,       NULL, NULL,               NULL, 0, 0, 0 },
    { "MCU: GD32F303", "MCU: GD32F303",  MENU_ACTION_NONE,       NULL, NULL,               NULL, 0, 0, 0 },
    { "Saat: 120 MHz", "Clock: 120 MHz", MENU_ACTION_NONE,       NULL, NULL,               NULL, 0, 0, 0 },
    { "Cagri: TA5THK", "Call: TA5THK",   MENU_ACTION_NONE,       NULL, NULL,               NULL, 0, 0, 0 },
};

/* Ana Menü Öğeleri */
static const menu_item_t items_main[] = {
    { "1. Telsiz Ayarlari", "1. Radio Settings", MENU_ACTION_SUBMENU, &node_radio_set, NULL, NULL, 0, 0, 0 },
    { "2. DMR Ayarlari",    "2. DMR Settings",   MENU_ACTION_SUBMENU, &node_dmr_set,   NULL, NULL, 0, 0, 0 },
    { "3. APRS & GPS",      "3. APRS & GPS",     MENU_ACTION_SUBMENU, &node_aprs_set,  NULL, NULL, 0, 0, 0 },
    { "4. Bluetooth",       "4. Bluetooth",      MENU_ACTION_SUBMENU, &node_bt_set,    NULL, NULL, 0, 0, 0 },
    { "5. Cihaz Bilgisi",   "5. Device Info",    MENU_ACTION_SUBMENU, &node_about,     NULL, NULL, 0, 0, 0 },
};

/* Düğüm tanımları */
static menu_node_t node_radio_set = {
    .title_tr = "TELSIZ AYARLARI",
    .title_en = "RADIO SETTINGS",
    .parent = &node_main,
    .items = items_radio_set,
    .item_count = sizeof(items_radio_set) / sizeof(items_radio_set[0]),
    .selected_index = 0,
    .scroll_offset = 0,
};

static menu_node_t node_dmr_set = {
    .title_tr = "DMR AYARLARI",
    .title_en = "DMR SETTINGS",
    .parent = &node_main,
    .items = items_dmr_set,
    .item_count = sizeof(items_dmr_set) / sizeof(items_dmr_set[0]),
    .selected_index = 0,
    .scroll_offset = 0,
};

static menu_node_t node_aprs_set = {
    .title_tr = "APRS & GPS",
    .title_en = "APRS & GPS",
    .parent = &node_main,
    .items = items_aprs_set,
    .item_count = sizeof(items_aprs_set) / sizeof(items_aprs_set[0]),
    .selected_index = 0,
    .scroll_offset = 0,
};

static menu_node_t node_bt_set = {
    .title_tr = "BLUETOOTH",
    .title_en = "BLUETOOTH",
    .parent = &node_main,
    .items = items_bt_set,
    .item_count = sizeof(items_bt_set) / sizeof(items_bt_set[0]),
    .selected_index = 0,
    .scroll_offset = 0,
};

static menu_node_t node_about = {
    .title_tr = "CIHAZ BILGISI",
    .title_en = "DEVICE INFO",
    .parent = &node_main,
    .items = items_about,
    .item_count = sizeof(items_about) / sizeof(items_about[0]),
    .selected_index = 0,
    .scroll_offset = 0,
};

static menu_node_t node_main = {
    .title_tr = "ANA MENU",
    .title_en = "MAIN MENU",
    .parent = NULL,
    .items = items_main,
    .item_count = sizeof(items_main) / sizeof(items_main[0]),
    .selected_index = 0,
    .scroll_offset = 0,
};

/* ============================================================
 * API İmplementasyonu
 * ============================================================ */

void ui_menu_init(void)
{
    s_menu_active = false;
    s_current_node = &node_main;
    node_main.selected_index = 0;
    node_main.scroll_offset = 0;
}

bool ui_menu_is_active(void)
{
    return s_menu_active;
}

void ui_menu_open(void)
{
    s_menu_active = true;
    s_current_node = &node_main;
    display_clear(COLOR_BLACK);
}

void ui_menu_close(void)
{
    s_menu_active = false;
    display_clear(COLOR_BLACK);
}

bool ui_menu_handle_key(const key_event_t *event)
{
    if (!event || event->event != KEY_EVENT_PRESS) return false;

    /* Menü kapalıyken MENU veya ROTARY_PRESS tuşu menüyü açar */
    if (!s_menu_active) {
        if (event->code == KEY_MENU || event->code == KEY_ROTARY_PRESS) {
            ui_menu_open();
            return true;
        }
        return false;
    }

    if (!s_current_node) {
        s_menu_active = false;
        return false;
    }

    menu_node_t *node = s_current_node;

    switch (event->code) {
    case KEY_UP:
    case KEY_ROTARY_LEFT:
        if (node->selected_index > 0) {
            node->selected_index--;
            if (node->selected_index < node->scroll_offset) {
                node->scroll_offset = node->selected_index;
            }
        } else {
            /* Başa sar */
            node->selected_index = node->item_count - 1;
            if (node->item_count > MENU_MAX_ITEMS_PER_PAGE) {
                node->scroll_offset = node->item_count - MENU_MAX_ITEMS_PER_PAGE;
            }
        }
        return true;

    case KEY_DOWN:
    case KEY_ROTARY_RIGHT:
        if (node->selected_index < node->item_count - 1) {
            node->selected_index++;
            if (node->selected_index >= node->scroll_offset + MENU_MAX_ITEMS_PER_PAGE) {
                node->scroll_offset = node->selected_index - MENU_MAX_ITEMS_PER_PAGE + 1;
            }
        } else {
            /* Başa dön */
            node->selected_index = 0;
            node->scroll_offset = 0;
        }
        return true;

    case KEY_MENU:
    case KEY_ROTARY_PRESS: {
        const menu_item_t *item = &node->items[node->selected_index];
        switch (item->type) {
        case MENU_ACTION_SUBMENU:
            if (item->submenu) {
                item->submenu->parent = node;
                item->submenu->selected_index = 0;
                item->submenu->scroll_offset = 0;
                s_current_node = item->submenu;
                display_clear(COLOR_BLACK);
            }
            break;

        case MENU_ACTION_TOGGLE:
            if (item->val_ptr) {
                *item->val_ptr = !(*item->val_ptr);
                if (item->action_func) item->action_func();
            }
            break;

        case MENU_ACTION_VALUE_INT:
            if (item->val_ptr) {
                *item->val_ptr += item->val_step;
                if (*item->val_ptr > item->val_max) {
                    *item->val_ptr = item->val_min;
                }
                if (item->action_func) item->action_func();
            }
            break;

        case MENU_ACTION_CALL_FUNC:
            if (item->action_func) {
                item->action_func();
            }
            break;

        case MENU_ACTION_BACK:
        default:
            break;
        }
        return true;
    }

    case KEY_EXIT:
        if (node->parent) {
            s_current_node = node->parent;
            display_clear(COLOR_BLACK);
        } else {
            ui_menu_close();
        }
        return true;

    default:
        break;
    }

    return true;
}

/* ============================================================
 * Menü Çizimi (160x128 LCD)
 * ============================================================ */

void ui_menu_render(void)
{
    if (!s_menu_active || !s_current_node) return;

    menu_node_t *node = s_current_node;
    bool is_tr = (s_lang_sel == 0);

    /* 1. ÜST BAŞLIK ÇUBUĞU (0..16 piksel) */
    display_fill_rect(0, 0, LCD_WIDTH, 16, COLOR_NAVY);
    const char *title = is_tr ? node->title_tr : node->title_en;
    display_draw_string(6, 4, title, COLOR_YELLOW, COLOR_NAVY, 1);

    /* Sayfa indeksi (örn: 1/5) */
    char idx_str[10];
    snprintf(idx_str, sizeof(idx_str), "%d/%d",
             node->selected_index + 1, node->item_count);
    display_draw_string(130, 4, idx_str, COLOR_WHITE, COLOR_NAVY, 1);

    /* 2. MENÜ ÖĞELERİ (18..112 piksel) */
    uint8_t visible = node->item_count - node->scroll_offset;
    if (visible > MENU_MAX_ITEMS_PER_PAGE) {
        visible = MENU_MAX_ITEMS_PER_PAGE;
    }

    for (uint8_t i = 0; i < visible; i++) {
        uint8_t item_idx = node->scroll_offset + i;
        const menu_item_t *item = &node->items[item_idx];
        bool selected = (item_idx == node->selected_index);

        int16_t row_y = 18 + (i * 19);
        uint16_t row_bg = selected ? 0x08A6 : COLOR_BLACK;   /* Seçili: Koyu camgöbeği */
        uint16_t text_fg = selected ? COLOR_WHITE : COLOR_LIGHTGREY;

        display_fill_rect(0, row_y, LCD_WIDTH, 18, row_bg);

        /* İmleç */
        if (selected) {
            display_draw_string(4, row_y + 4, ">", COLOR_YELLOW, row_bg, 1);
        }

        /* Metin */
        const char *txt = is_tr ? item->text_tr : item->text_en;
        display_draw_string(14, row_y + 4, txt, text_fg, row_bg, 1);

        /* Değer / Durum Göstergesi */
        char val_str[16];
        val_str[0] = '\0';

        if (item->type == MENU_ACTION_TOGGLE && item->val_ptr) {
            snprintf(val_str, sizeof(val_str), "[%s]",
                     *item->val_ptr ? (is_tr ? "ACIK" : "ON") : (is_tr ? "KPLI" : "OFF"));
        } else if (item->type == MENU_ACTION_VALUE_INT && item->val_ptr) {
            if (item->val_ptr == &s_tx_power) {
                const char *pwr_str[] = {"TURBO", "HIGH", "MID", "LOW"};
                snprintf(val_str, sizeof(val_str), "%s", pwr_str[*item->val_ptr]);
            } else {
                snprintf(val_str, sizeof(val_str), "< %ld >", (long)*item->val_ptr);
            }
        } else if (item->type == MENU_ACTION_SUBMENU) {
            snprintf(val_str, sizeof(val_str), ">>");
        }

        if (val_str[0] != '\0') {
            int16_t val_x = LCD_WIDTH - (int16_t)(strlen(val_str) * 6) - 6;
            display_draw_string(val_x, row_y + 4, val_str,
                                selected ? COLOR_YELLOW : COLOR_CYAN, row_bg, 1);
        }
    }

    /* 3. ALT ÇUBUK (114..128 piksel) */
    display_fill_rect(0, 114, LCD_WIDTH, 14, 0x18C3);
    display_draw_string(8, 116, is_tr ? "[SEC]" : "[SELECT]", COLOR_GREEN, 0x18C3, 1);
    display_draw_string(116, 116, is_tr ? "[GERI]" : "[BACK]", COLOR_RED, 0x18C3, 1);

    display_flush();
}
