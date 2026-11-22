/**
 * @file    main.c
 * @brief   Main Application Entry Point - AT-D878 Open Firmware
 * @version 0.1.0
 *
 * AnyTone AT-D878UVII Plus bağımsız açık kaynak firmware çekirdeği.
 *
 * Boot Sırası:
 *   1. SysTick & RCC: HSE→PLL→120MHz, SysTick 1ms
 *   2. GPIO: Tüm portların (A-E) saatlerini etkinleştir
 *   3. Keyboard: Matrix tuş tarama (5ms periyodik zamanlayıcı)
 *   4. Display: ST7735 1.77" TFT LCD başlat, boot ekranı göster
 *   5. Audio: TLV320AIC3204 codec başlat
 *   6. RF: AT1846S transceiver başlat, varsayılan frekans ayarla
 *   7. DMR: SCT3258 baseband başlat
 *   8. External Flash: Codeplug ve kişi listesini yükle
 *   9. LinkBoard: GPS + APRS + BT başlat
 *  10. Radio Core: Durum makinesi başlat
 *  11. UI: Kullanıcı arayüzü başlat
 *  12. Ana Döngü: Sürekli işleme
 *
 * Callsign: TA5THK / Fatih ATCI
 */

#include "hal_gpio.h"
#include "hal_systick.h"
#include "hal_uart.h"
#include "driver_display.h"
#include "driver_at1846s.h"
#include "driver_keyboard.h"
#include "driver_sct3258.h"
#include "driver_spi_flash.h"
#include "driver_linkboard.h"
#include "radio_core.h"
#include "cps_protocol.h"
#include "ui_screen.h"

/* ============================================================
 * Firmware Sürüm Bilgisi
 * ============================================================ */

#define FW_VERSION_MAJOR      0
#define FW_VERSION_MINOR      1
#define FW_VERSION_PATCH      0
#define FW_VERSION_STRING     "0.1.0-alpha"
#define FW_BUILD_DATE         __DATE__
#define FW_BUILD_TIME         __TIME__
#define FW_CALLSIGN           "TA5THK"

/* Sürüm bilgisini flash'a göm (CPS'ten okunabilir) */
__attribute__((section(".rodata"), used))
static const char fw_version_tag[] =
    "AT-D878-OPEN-FW|" FW_VERSION_STRING "|" FW_CALLSIGN "|" __DATE__;

/* ============================================================
 * Alt Sistem Başlatma Durumları
 * ============================================================ */

typedef struct {
    bool systick;
    bool gpio;
    bool keyboard;
    bool display;
    bool audio;
    bool rf;
    bool dmr;
    bool ext_flash;
    bool linkboard;
    bool radio_core;
    bool ui;
} boot_status_t;

static boot_status_t s_boot = {0};

/* ============================================================
 * Hata İşleme
 * ============================================================ */

/** Hata durumunda LED ile bildirim */
static void error_blink(int count)
{
    for (int i = 0; i < count; i++) {
        hal_gpio_set(PIN_LED_PWR);
        hal_delay_ms(200);
        hal_gpio_clear(PIN_LED_PWR);
        hal_delay_ms(200);
    }
    hal_delay_ms(1000);
}

/* ============================================================
 * Ana Giriş Noktası
 * ============================================================ */

int main(void)
{
    int rc;

    /* ============================================
     * 1. Sistem Saati & SysTick (KRİTİK)
     * ============================================ */
    rc = hal_systick_init();
    s_boot.systick = (rc == 0);
    if (!s_boot.systick) {
        /* Saat başlatılamazsa başka bir şey yapılamaz.
         * Varsayılan ~8MHz HSI ile devam et. */
    }

    /* ============================================
     * 2. GPIO Alt Sistemi
     * ============================================ */
    rc = hal_gpio_init();
    s_boot.gpio = (rc == 0);

    /* LED ve güç kontrol pinlerini yapılandır */
    hal_gpio_configure(PIN_LED_PWR, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);
    hal_gpio_set(PIN_LED_PWR);  /* LED yak — boot başladı */

    /* Çevre birim güç kontrolü */
    hal_gpio_configure(PIN_PERIPH_PWR, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);
    hal_gpio_set(PIN_PERIPH_PWR);  /* GPS & BT güç aç */

    /* Ses amplifikatörü mute (başlangıçta sessiz) */
    hal_gpio_configure(PIN_AUDIO_MUTE, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);
    hal_gpio_clear(PIN_AUDIO_MUTE);  /* Mute ON */

    /* ============================================
     * 3. Tuş Takımı & Encoder
     * ============================================ */
    rc = keyboard_init();
    s_boot.keyboard = (rc == 0);

    /* ============================================
     * 4. LCD Ekran
     * ============================================ */
    rc = display_init();
    s_boot.display = (rc == 0);

    if (s_boot.display) {
        /* Boot ekranı göster */
        display_clear(0x0000);  /* Siyah arka plan */
        display_draw_string(20, 30, "AT-D878 OPEN FW", 0xFFFF, 0x0000, 1);
        display_draw_string(35, 55, FW_VERSION_STRING, 0x07E0, 0x0000, 1);
        display_draw_string(40, 80, FW_CALLSIGN, 0xFFE0, 0x0000, 1);
        display_draw_string(15, 105, "Booting...", 0x07FF, 0x0000, 1);
    }

    /* ============================================
     * 5. RF Transceiver (AT1846S)
     * ============================================ */
    hal_delay_ms(50);  /* Güç stabilizasyonu bekle */

    at1846s_config_t rf_cfg = {
        .xtal_freq_hz = 26000000,  /* 26 MHz kristal */
        .use_i2c      = true,
        .i2c_addr     = 0x35,
    };
    rc = at1846s_init(&rf_cfg);
    s_boot.rf = (rc == 0);

    if (s_boot.rf) {
        /* Varsayılan frekans: 438.500 MHz (Türkiye DMR) */
        at1846s_set_frequency(438500000);
        at1846s_set_bandwidth(AT1846S_BW_12_5KHZ);
        at1846s_set_squelch(3);
        at1846s_rx_on();
    }

    /* ============================================
     * 6. DMR Baseband (SCT3258)
     * ============================================ */
    rc = sct3258_init();
    s_boot.dmr = (rc == 0);

    if (s_boot.dmr) {
        /* TA5THK DMR ID: varsayılan ayarla */
        sct3258_set_dmr_id(2860501);  /* Örnek TR DMR ID */
        sct3258_set_dmr_params(1, SCT3258_SLOT_1, 91, SCT3258_CALL_GROUP);
    }

    /* ============================================
     * 7. Harici SPI Flash
     * ============================================ */
    rc = spi_flash_init();
    s_boot.ext_flash = (rc == 0);

    if (s_boot.ext_flash && s_boot.display) {
        spi_flash_info_t flash_info;
        spi_flash_get_info(&flash_info);
        /* LCD'de flash bilgisi göster (debug) */
        /* Flash bilgisi gelecekte LCD'de gösterilecek */
    }

    /* ============================================
     * 8. LinkBoard (GPS + APRS + BT)
     * ============================================ */
    rc = linkboard_init();
    s_boot.linkboard = (rc == 0);

    if (s_boot.linkboard) {
        /* APRS yapılandırması */
        aprs_config_t aprs_cfg = {
            .callsign        = "TA5THK",
            .ssid            = 7,
            .dest            = "APD878",
            .path            = "WIDE1-1,WIDE2-1",
            .symbol_table    = '/',
            .symbol_code     = '>',
            .comment         = "AT-D878 Open FW",
            .freq_hz         = 144800000,
            .smart_beacon    = true,
            .beacon_interval_s = 120,
        };
        linkboard_aprs_configure(&aprs_cfg);
        linkboard_gps_power(true);
    }

    /* ============================================
     * 9. Radyo Çekirdeği & CPS İletişimi
     * ============================================ */
    cps_protocol_init();
    radio_core_init();
    s_boot.radio_core = true;

    /* ============================================
     * 10. Kullanıcı Arayüzü
     * ============================================ */
    ui_init(UI_LANG_TURKISH, UI_THEME_DARK);
    s_boot.ui = true;

    /* Ses amplifikatörünü aç (boot tamamlandı) */
    hal_gpio_set(PIN_AUDIO_MUTE);

    /* Boot tamamlandı — LED söndür */
    hal_delay_ms(500);

    /* Hata olan alt sistemleri LED ile bildir */
    if (!s_boot.rf)        error_blink(2);
    if (!s_boot.dmr)       error_blink(3);
    if (!s_boot.ext_flash) error_blink(4);
    if (!s_boot.linkboard) error_blink(5);

    hal_gpio_clear(PIN_LED_PWR);  /* LED kapat */

    if (s_boot.display) {
        display_draw_string(15, 105, "Ready! 73 de TA5THK", 0x07E0, 0x0000, 1);
        hal_delay_ms(1000);
    }

    /* ============================================
     * 11. Ana Döngü (Super-Loop)
     * ============================================ */
    radio_core_status_t status;
    key_event_t key_event;

    while (1) {
        /* A. Yazılım zamanlayıcılarını işle (keyboard scan dahil) */
        hal_timers_process();

        /* B. Tuş olaylarını işle */
        while (keyboard_get_event(&key_event)) {
            /* PTT tuşu → TX/RX geçişi */
            if (key_event.code == KEY_PTT) {
                if (key_event.event == KEY_EVENT_PRESS) {
                    radio_core_start_tx();
                } else if (key_event.event == KEY_EVENT_RELEASE) {
                    radio_core_stop_tx();
                }
            }

            /* VFO değiştirme: Long press on ROTARY_PRESS */
            if (key_event.code == KEY_ROTARY_PRESS &&
                key_event.event == KEY_EVENT_HOLD) {
                radio_core_toggle_vfo();
            }

            /* Frekans ayarı: Rotary encoder */
            if (key_event.code == KEY_ROTARY_RIGHT) {
                radio_core_step_frequency(true);  /* Yukarı */
            } else if (key_event.code == KEY_ROTARY_LEFT) {
                radio_core_step_frequency(false); /* Aşağı */
            }

            /* UI menü navigasyonu ve tuş işleme */
            ui_handle_key(&key_event);
        }

        /* C. Radyo durumunu ve CPS seri iletişimini işle */
        radio_core_process();
        cps_protocol_process();

        /* D. LinkBoard verilerini işle (GPS/APRS/BT) */
        if (s_boot.linkboard) {
            linkboard_process();
        }

        /* E. Durumu oku ve ekrana çiz */
        radio_core_get_status(&status);
        ui_render(&status);

        /* F. Düşük güç modu — kısa bekleme (CPU rahatlatma) */
        __asm__ volatile ("wfi");
    }

    return 0;
}
