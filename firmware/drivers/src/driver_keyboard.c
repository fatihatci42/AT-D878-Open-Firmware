/**
 * @file    driver_keyboard.c
 * @brief   Keypad, Side Keys & PTT Driver Implementation - AT-D878 Open Firmware
 * @version 0.0.1
 */

#include "driver_keyboard.h"
#include "hal_gpio.h"

int keyboard_init(void)
{
    /* PTT pinini dahili pull-up ile yapılandır */
    hal_gpio_configure(PIN_PTT, GPIO_MODE_INPUT_PULLUP, GPIO_SPEED_50MHZ);
    return 0;
}

bool keyboard_get_event(key_event_t *event)
{
    if (!event) return false;
    event->code = KEY_NONE;
    return false;
}

bool keyboard_is_ptt_pressed(void)
{
    /* PTT Active LOW: Basıldığında 0 (false), basılmadığında 1 (true) okunur */
    return !hal_gpio_read(PIN_PTT);
}
