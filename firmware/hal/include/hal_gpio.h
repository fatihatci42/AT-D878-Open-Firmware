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
 * Doğrulanmış Pin Atamaları (AnyTone AT-D868 / D878 / D878UVII)
 * Kaynak: RadioFoundry Hardware Spec & D878 Teardown Analizleri
 * ============================================================ */

/* PTT butonu - Active LOW (Dahili Pull-Up) */
#define PIN_PTT              ((hal_gpio_id_t){GPIO_PORT_A, 0})

/* Ses Amplifikatörü / Mute (HM8872: 1 = Ses Açık, 0 = Mute) */
#define PIN_AUDIO_MUTE       ((hal_gpio_id_t){GPIO_PORT_A, 7})
#define PIN_AUDIO_SWITCH     ((hal_gpio_id_t){GPIO_PORT_E, 0})

/* Hata/Durum LED & Güç Kontrol */
#define PIN_LED_PWR          ((hal_gpio_id_t){GPIO_PORT_A, 1})

/* DAC0 12-bit TX Güç Seviyesi (Fabrika NAND kalibrasyon tablosundan beslenir) */
#define PIN_TX_POWER_DAC     ((hal_gpio_id_t){GPIO_PORT_A, 4})

/* 1.77" Renkli TFT LCD (ST7735S 8080 8-Bit Paralel Bus) */
#define PIN_LCD_RS_DC        ((hal_gpio_id_t){GPIO_PORT_B, 10}) /* Low=Komut, High=Veri */
#define PIN_LCD_DC           PIN_LCD_RS_DC
#define PIN_LCD_RST          ((hal_gpio_id_t){GPIO_PORT_B, 11}) /* Reset - Aktif LOW */
#define PIN_LCD_CS           ((hal_gpio_id_t){GPIO_PORT_B, 12}) /* Chip Select - Aktif LOW */
#define PIN_LCD_WR           ((hal_gpio_id_t){GPIO_PORT_B, 13}) /* Write Strobe - Aktif LOW */
#define PIN_LCD_RD           ((hal_gpio_id_t){GPIO_PORT_E, 15}) /* Read Strobe - Genelde HIGH */
#define PIN_LCD_BL           ((hal_gpio_id_t){GPIO_PORT_A, 6})  /* Arka Aydınlatma PWM (TIMER2_CH0) */

/* RF Transceiver (AT1846S / QA4818N) 3-Wire Bus */
#define PIN_RF_SCLK          ((hal_gpio_id_t){GPIO_PORT_B, 3})  /* 3-Wire Saat */
#define PIN_RF_SDIO          ((hal_gpio_id_t){GPIO_PORT_B, 4})  /* 3-Wire Çift Yönlü Veri */
#define PIN_RF_SEN           ((hal_gpio_id_t){GPIO_PORT_B, 5})  /* 3-Wire Chip Enable - Aktif LOW */
#define PIN_RF_CS            PIN_RF_SEN
#define PIN_RF_CLK           PIN_RF_SCLK

/* I2C Bus (RDA5802N FM Alıcı [0x10] & MMA8653FC Akselerometre [0x1D]) */
#define PIN_I2C_SCL          ((hal_gpio_id_t){GPIO_PORT_B, 6})
#define PIN_I2C_SDA          ((hal_gpio_id_t){GPIO_PORT_B, 7})

/* SPI NAND Flash (Toshiba TC58CVG0S3HRAIG / GigaDevice) */
#define PIN_NAND_CS          ((hal_gpio_id_t){GPIO_PORT_B, 8})
#define PIN_NAND_CLK         ((hal_gpio_id_t){GPIO_PORT_B, 14})
#define PIN_NAND_MOSI        ((hal_gpio_id_t){GPIO_PORT_B, 15})
#define PIN_NAND_MISO        ((hal_gpio_id_t){GPIO_PORT_A, 15}) /* Dual Output Okuma */

/* Döner Kanal Enkoderi (Quadrature Gray Code) */
#define PIN_ENC_A            ((hal_gpio_id_t){GPIO_PORT_C, 8})
#define PIN_ENC_B            ((hal_gpio_id_t){GPIO_PORT_C, 9})

/* Dahili ADC Girişleri */
#define PIN_ADC_BATT         ((hal_gpio_id_t){GPIO_PORT_C, 0})  /* ADC0_CH10 - Batarya gerilim bölücü */
#define PIN_ADC_RF_SENSE     ((hal_gpio_id_t){GPIO_PORT_C, 1})  /* ADC0_CH11 - RF güç detektörü */

/* Çevre Birim Güç Kontrolü (Bluetooth / GPS) */
#define PIN_PERIPH_PWR       ((hal_gpio_id_t){GPIO_PORT_E, 9})  /* 1 = GPS & BT Aktif */

/* DMR Baseband (SCT3258) Doğrulanmış HPI Kontrol Hatları */
#define PIN_SCT_PWR_EN1      ((hal_gpio_id_t){GPIO_PORT_A, 8})  /* Güç Enable 1 (HIGH) */
#define PIN_SCT_PWR_EN2      ((hal_gpio_id_t){GPIO_PORT_C, 6})  /* Güç Enable 2 (HIGH) */
#define PIN_SCT_PIO1_STRAP   ((hal_gpio_id_t){GPIO_PORT_C, 12}) /* Intel Mode Seçici (Reset'te LOW) */
#define PIN_SCT_HCSN         ((hal_gpio_id_t){GPIO_PORT_D, 2})  /* HPI Chip Select - Aktif LOW */
#define PIN_SCT_HWRN         ((hal_gpio_id_t){GPIO_PORT_D, 3})  /* HPI Write - Aktif LOW */
#define PIN_SCT_HRDN         ((hal_gpio_id_t){GPIO_PORT_D, 4})  /* HPI Read - Aktif LOW */
#define PIN_SCT_HOBIB        ((hal_gpio_id_t){GPIO_PORT_D, 5})  /* HPI FIFO Data Waiting (Giriş) */
#define PIN_SCT_RSTN         ((hal_gpio_id_t){GPIO_PORT_D, 6})  /* Reset - Aktif LOW */
#define PIN_SCT_TDMA_SLOT    ((hal_gpio_id_t){GPIO_PORT_D, 0})  /* PIO3 60ms TDMA Slot Sinyali (Kesme Girişi) */
#define PIN_SCT_RST          PIN_SCT_RSTN
#define PIN_SCT_HPI_CTRL     PIN_SCT_HWRN

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
