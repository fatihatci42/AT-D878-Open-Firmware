/**
 * @file    main.c
 * @brief   Main Application Entry Point - AT-D878 Open Firmware
 * @version 0.0.1
 *
 * AnyTone AT-D878UVII Plus bağımsız açık kaynak firmware çekirdeği.
 */

#include "hal_gpio.h"
#include "driver_display.h"
#include "driver_at1846s.h"
#include "radio_core.h"
#include "ui_screen.h"

int main(void)
{
    /* 1. Donanım Soyutlama Katmanını (HAL) Başlat */
    hal_gpio_init();

    /* 2. Kullanıcı Girişlerini Yapılandır (PTT Active LOW) */
    hal_gpio_configure(PIN_PTT, GPIO_MODE_INPUT_PULLUP, GPIO_SPEED_50MHZ);

    /* 3. Çevre Birim Güçlerini Etkinleştir (GPS / Bluetooth / Audio) */
    hal_gpio_configure(PIN_PERIPH_PWR, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);
    hal_gpio_set(PIN_PERIPH_PWR);

    hal_gpio_configure(PIN_AUDIO_MUTE, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);
    hal_gpio_set(PIN_AUDIO_MUTE); /* Ses amplifikatörünü aç */

    /* 4. RF Donanımını ve Radyo Çekirdeğini Başlat */
    radio_core_init();

    /* 5. Kullanıcı Arayüzü ve 1.77" Renkli TFT LCD Ekranı Başlat */
    ui_init(UI_LANG_TURKISH, UI_THEME_DARK);

    /* 6. Ana Döngü (Super-Loop / RTOS Öncesi Durum Makinesi) */
    radio_core_status_t status;
    while (1) {
        /* Radyo durumunu işle (PTT, Squelch, RSSI) */
        radio_core_process();

        /* Durumu oku ve ekrana çiz */
        radio_core_get_status(&status);
        ui_render(&status);
    }

    return 0;
}
