/**
 * @file    hal_gpio.h
 * @brief   GPIO Hardware Abstraction Layer - AT-D878 Open Firmware
 * @version 0.0.1
 *
 * GD32F303 GPIO donanım soyutlama katmanı.
 * Tüm GPIO erişimi bu arayüz üzerinden yapılır.
 *
 * NOT: Pin atamaları henüz doğrulanmamıştır. Fiziksel teardown
 * sonrası güncellenecektir.
 */

#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * GPIO Port ve Pin Tanımları
 * ============================================================ */

/**
 * @brief GPIO port numaralandırması
 */
typedef enum {
    GPIO_PORT_A = 0,
    GPIO_PORT_B,
    GPIO_PORT_C,
    GPIO_PORT_D,
    GPIO_PORT_E,
    GPIO_PORT_COUNT
} hal_gpio_port_t;

/**
 * @brief GPIO pin numarası (0-15)
 */
typedef uint8_t hal_gpio_pin_t;

/**
 * @brief GPIO pin tanımlayıcı (port + pin birleşimi)
 */
typedef struct {
    hal_gpio_port_t port;
    hal_gpio_pin_t  pin;
} hal_gpio_id_t;

/**
 * @brief GPIO pin modu
 */
typedef enum {
    GPIO_MODE_INPUT_ANALOG    = 0x00,  /**< Analog giriş (ADC) */
    GPIO_MODE_INPUT_FLOATING  = 0x04,  /**< Yüzen giriş */
    GPIO_MODE_INPUT_PULLDOWN  = 0x28,  /**< Pull-down giriş */
    GPIO_MODE_INPUT_PULLUP    = 0x48,  /**< Pull-up giriş */
    GPIO_MODE_OUTPUT_PP       = 0x10,  /**< Push-pull çıkış */
    GPIO_MODE_OUTPUT_OD       = 0x14,  /**< Open-drain çıkış */
    GPIO_MODE_AF_PP           = 0x18,  /**< Alternatif fonksiyon push-pull */
    GPIO_MODE_AF_OD           = 0x1C,  /**< Alternatif fonksiyon open-drain */
} hal_gpio_mode_t;

/**
 * @brief GPIO çıkış hızı
 */
typedef enum {
    GPIO_SPEED_2MHZ   = 0x02,  /**< 2 MHz */
    GPIO_SPEED_10MHZ  = 0x01,  /**< 10 MHz */
    GPIO_SPEED_50MHZ  = 0x03,  /**< 50 MHz */
} hal_gpio_speed_t;

/* ============================================================
 * Doğrulanmış Pin Atamaları (Kart: D878UV2A-BORD, FW: V4.00)
 * Kaynak: Orijinal binary 0x08020D14 - 0x08020EBC dökümü
 * ============================================================ */

/* PTT butonu - Active LOW (Dahili Pull-Up) */
#define PIN_PTT              ((hal_gpio_id_t){GPIO_PORT_A, 0})

/* Ses Amplifikatörü / Mute (1 = Ses Açık, 0 = Mute) */
#define PIN_AUDIO_MUTE       ((hal_gpio_id_t){GPIO_PORT_A, 7})

/* Hata/Durum LED & Güç Kontrol */
#define PIN_LED_PWR          ((hal_gpio_id_t){GPIO_PORT_A, 1})

/* 1.77" Renkli TFT LCD Kontrol Pinleri */
#define PIN_LCD_RST          ((hal_gpio_id_t){GPIO_PORT_C, 3})  /* Reset - Aktif HIGH */
#define PIN_LCD_DC           ((hal_gpio_id_t){GPIO_PORT_C, 4})  /* Data/Command (A0) */
#define PIN_LCD_CS           ((hal_gpio_id_t){GPIO_PORT_C, 5})  /* Chip Select - Aktif LOW */
#define PIN_LCD_BL           ((hal_gpio_id_t){GPIO_PORT_C, 10}) /* Arka Aydınlatma PWM/ON */

/* RF Transceiver (AT1846S) Kontrol Hatları */
#define PIN_RF_CS            ((hal_gpio_id_t){GPIO_PORT_B, 1})  /* Chip Select */
#define PIN_RF_CLK           ((hal_gpio_id_t){GPIO_PORT_B, 2})  /* Seri Saat / Veri */

/* RF Güç ve Bant Seçici Switch */
#define PIN_RF_BAND_SWITCH   ((hal_gpio_id_t){GPIO_PORT_E, 6})  /* 1=UHF (400-520MHz), 0=VHF (134-174MHz) */
#define PIN_RF_PA_BIAS       ((hal_gpio_id_t){GPIO_PORT_E, 7})  /* PA Bias / TX Enable */

/* Çevre Birim Güç Kontrolü (Bluetooth / GPS) */
#define PIN_PERIPH_PWR       ((hal_gpio_id_t){GPIO_PORT_E, 9})  /* 1 = GPS & BT Aktif */

/* DMR Baseband (SCT3258) Kontrol */
#define PIN_SCT_RST          ((hal_gpio_id_t){GPIO_PORT_E, 0})  /* 1 = Aktif, 0 = Reset */
#define PIN_SCT_HPI_CTRL     ((hal_gpio_id_t){GPIO_PORT_E, 1})  /* HPI Kontrol Hattı */

/* ============================================================
 * API Fonksiyonları
 * ============================================================ */

/**
 * @brief GPIO alt sistemini başlat
 * @return 0 başarılı, negatif hata kodu
 */
int hal_gpio_init(void);

/**
 * @brief Tek bir GPIO pinini yapılandır
 * @param id    Pin tanımlayıcı (port + pin)
 * @param mode  Pin modu
 * @param speed Çıkış hızı (çıkış modlarında)
 * @return 0 başarılı, negatif hata kodu
 */
int hal_gpio_configure(hal_gpio_id_t id, hal_gpio_mode_t mode,
                       hal_gpio_speed_t speed);

/**
 * @brief GPIO pinini HIGH yap
 * @param id Pin tanımlayıcı
 */
void hal_gpio_set(hal_gpio_id_t id);

/**
 * @brief GPIO pinini LOW yap
 * @param id Pin tanımlayıcı
 */
void hal_gpio_clear(hal_gpio_id_t id);

/**
 * @brief GPIO pin durumunu değiştir (toggle)
 * @param id Pin tanımlayıcı
 */
void hal_gpio_toggle(hal_gpio_id_t id);

/**
 * @brief GPIO pin değerini yaz
 * @param id    Pin tanımlayıcı
 * @param value true=HIGH, false=LOW
 */
void hal_gpio_write(hal_gpio_id_t id, bool value);

/**
 * @brief GPIO pin değerini oku
 * @param id Pin tanımlayıcı
 * @return true=HIGH, false=LOW
 */
bool hal_gpio_read(hal_gpio_id_t id);

/**
 * @brief GPIO harici kesme yapılandır
 * @param id       Pin tanımlayıcı
 * @param trigger  Tetikleme tipi (yükselen/düşen/her ikisi)
 * @param callback Kesme çağrı fonksiyonu
 * @param context  Kullanıcı bağlamı
 * @return 0 başarılı, negatif hata kodu
 */
typedef void (*hal_gpio_irq_callback_t)(hal_gpio_id_t id, void *context);

typedef enum {
    GPIO_IRQ_RISING  = 0x01,
    GPIO_IRQ_FALLING = 0x02,
    GPIO_IRQ_BOTH    = 0x03,
} hal_gpio_irq_trigger_t;

int hal_gpio_irq_configure(hal_gpio_id_t id,
                           hal_gpio_irq_trigger_t trigger,
                           hal_gpio_irq_callback_t callback,
                           void *context);

/**
 * @brief GPIO harici kesmeyi etkinleştir
 * @param id Pin tanımlayıcı
 */
void hal_gpio_irq_enable(hal_gpio_id_t id);

/**
 * @brief GPIO harici kesmeyi devre dışı bırak
 * @param id Pin tanımlayıcı
 */
void hal_gpio_irq_disable(hal_gpio_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* HAL_GPIO_H */
