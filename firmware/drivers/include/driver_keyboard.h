/**
 * @file    driver_keyboard.h
 * @brief   Keypad, Side Keys, and Rotary Encoder Driver - AT-D878 Open Firmware
 * @version 0.0.1
 */

#ifndef DRIVER_KEYBOARD_H
#define DRIVER_KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KEY_NONE = 0,
    KEY_0, KEY_1, KEY_2, KEY_3, KEY_4,
    KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
    KEY_MENU,
    KEY_EXIT,
    KEY_UP,
    KEY_DOWN,
    KEY_STAR,
    KEY_HASH,
    KEY_PTT,
    KEY_SIDE1,
    KEY_SIDE2,
    KEY_TOP,
    KEY_ROTARY_LEFT,
    KEY_ROTARY_RIGHT,
    KEY_ROTARY_PRESS
} key_code_t;

typedef enum {
    KEY_EVENT_PRESS = 0,
    KEY_EVENT_RELEASE,
    KEY_EVENT_HOLD,
    KEY_EVENT_LONG_HOLD
} key_event_type_t;

typedef struct {
    key_code_t       code;
    key_event_type_t event;
    uint32_t         timestamp_ms;
} key_event_t;

/**
 * @brief Tuş ve encoder sürücüsünü başlat
 */
int keyboard_init(void);

/**
 * @brief Tuş olay kuyruğundan bir olay oku (bloklamaz)
 */
bool keyboard_get_event(key_event_t *event);

/**
 * @brief PTT tuşu şu an basılı mı?
 */
bool keyboard_is_ptt_pressed(void);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_KEYBOARD_H */
