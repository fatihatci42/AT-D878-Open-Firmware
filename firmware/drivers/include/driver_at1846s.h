/**
 * @file    driver_at1846s.h
 * @brief   AT1846S RF Transceiver Driver - AT-D878 Open Firmware
 * @version 0.0.1
 *
 * Auctus AT1846S tek çipli VHF/UHF RF transceiver sürücüsü.
 *
 * Desteklenen özellikler:
 * - Çift bant (VHF 134-174 MHz, UHF 400-520 MHz)
 * - FM modülasyon/demodülasyon
 * - Frekans sentezi
 * - RSSI ölçümü
 * - Squelch kontrolü
 * - TX güç kontrolü
 * - CTCSS/DCS tonu üretimi ve algılama
 *
 * Register bilgileri OpenRTX ve topluluk araştırmalarından alınmıştır.
 *
 * UYARI: MCU ile AT1846S arasındaki fiziksel bağlantı (I2C vs SPI)
 * ve pin atamaları henüz doğrulanmamıştır.
 */

#ifndef DRIVER_AT1846S_H
#define DRIVER_AT1846S_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * AT1846S Register Adresleri (Doğrulanmış)
 * ============================================================ */

#define AT1846S_REG_CLK_MODE        0x04  /**< Saat modu (frekans aralığı) */
#define AT1846S_REG_LDO_BYPASS      0x08  /**< LDO bypass kontrolü */
#define AT1846S_REG_LDO_VOLTAGE     0x09  /**< LDO çıkış gerilimi */
#define AT1846S_REG_PA_BIAS         0x0A  /**< PA bias gerilimi */
#define AT1846S_REG_BAND_SEL        0x0F  /**< Bant seçimi */
#define AT1846S_REG_GPIO_MODE       0x1F  /**< GPIO modları */
#define AT1846S_REG_FREQ_HI         0x29  /**< Frekans üst 14 bit */
#define AT1846S_REG_FREQ_LO         0x2A  /**< Frekans alt 16 bit */
#define AT1846S_REG_XTAL_FREQ       0x2B  /**< Referans kristal frekansı */
#define AT1846S_REG_ADC_CLK         0x2C  /**< ADC saat frekansı */
#define AT1846S_REG_INT_EN          0x2D  /**< Interrupt enable mask */
#define AT1846S_REG_CH_MODE         0x30  /**< Kanal modu (12.5/25 kHz) */
#define AT1846S_REG_RX_VOL          0x44  /**< RX ses seviyesi */
#define AT1846S_REG_EXT_PAGE        0x7F  /**< Genişletilmiş adres erişimi */

/* ============================================================
 * AT1846S Bant Seçimi
 * ============================================================ */

#define AT1846S_BAND_UHF_400MHZ     0x00  /**< 400-520 MHz */
#define AT1846S_BAND_VHF_200MHZ     0x02  /**< 200-260 MHz */
#define AT1846S_BAND_VHF_134MHZ     0x03  /**< 134-174 MHz */

/* ============================================================
 * AT1846S Kanal Genişliği
 * ============================================================ */

#define AT1846S_BW_12_5KHZ          0x00  /**< 12.5 kHz dar bant */
#define AT1846S_BW_25KHZ            0x03  /**< 25 kHz geniş bant */

/* ============================================================
 * Veri Yapıları
 * ============================================================ */

/**
 * @brief AT1846S başlatma yapılandırması
 */
typedef struct {
    uint32_t xtal_freq_hz;    /**< Referans kristal frekansı (Hz) */
    bool     use_i2c;         /**< true=I2C, false=SPI */
    uint8_t  i2c_addr;        /**< I2C adresi (I2C modunda) */
} at1846s_config_t;

/**
 * @brief RF durum bilgisi
 */
typedef struct {
    uint32_t frequency_hz;    /**< Ayarlı frekans (Hz) */
    int16_t  rssi_dbm;        /**< RSSI değeri (dBm) */
    bool     squelch_open;    /**< Squelch durumu */
    bool     tx_active;       /**< TX aktif mi */
    bool     rx_active;       /**< RX aktif mi */
    uint8_t  band;            /**< Aktif bant */
    uint8_t  bandwidth;       /**< Kanal genişliği */
    uint8_t  tx_power;        /**< TX güç seviyesi */
} at1846s_status_t;

/**
 * @brief CTCSS/DCS yapılandırması
 */
typedef struct {
    bool     enabled;         /**< CTCSS/DCS etkin mi */
    bool     is_dcs;          /**< true=DCS, false=CTCSS */
    uint16_t tone_freq_x10;   /**< CTCSS frekansı × 10 (67.0 → 670) */
    uint16_t dcs_code;        /**< DCS kodu */
    bool     dcs_inverted;    /**< DCS ters polarite */
} at1846s_tone_config_t;

/* ============================================================
 * API Fonksiyonları
 * ============================================================ */

/**
 * @brief AT1846S sürücüsünü başlat
 * @param config Yapılandırma
 * @return 0 başarılı, negatif hata kodu
 */
int at1846s_init(const at1846s_config_t *config);

/**
 * @brief AT1846S sürücüsünü kapat
 */
void at1846s_deinit(void);

/**
 * @brief Çip kimlik register'ını oku (doğrulama için)
 * @return Çip ID değeri, negatif hata kodu
 */
int at1846s_read_chip_id(void);

/**
 * @brief RF frekans ayarla
 * @param freq_hz Frekans (Hz cinsinden, örn: 145000000 = 145.000 MHz)
 * @return 0 başarılı, negatif hata kodu
 */
int at1846s_set_frequency(uint32_t freq_hz);

/**
 * @brief Aktif frekansı oku
 * @return Frekans (Hz), negatif hata kodu
 */
int64_t at1846s_get_frequency(void);

/**
 * @brief Bant seçimi
 * @param band AT1846S_BAND_* sabitlerinden biri
 * @return 0 başarılı, negatif hata kodu
 */
int at1846s_set_band(uint8_t band);

/**
 * @brief Kanal genişliği ayarla
 * @param bw AT1846S_BW_* sabitlerinden biri
 * @return 0 başarılı, negatif hata kodu
 */
int at1846s_set_bandwidth(uint8_t bw);

/**
 * @brief TX modunu etkinleştir
 * @return 0 başarılı, negatif hata kodu
 */
int at1846s_tx_on(void);

/**
 * @brief TX modunu kapat
 * @return 0 başarılı, negatif hata kodu
 */
int at1846s_tx_off(void);

/**
 * @brief RX modunu etkinleştir
 * @return 0 başarılı, negatif hata kodu
 */
int at1846s_rx_on(void);

/**
 * @brief RX modunu kapat (standby)
 * @return 0 başarılı, negatif hata kodu
 */
int at1846s_rx_off(void);

/**
 * @brief TX güç seviyesi ayarla
 * @param power_level 0-15 arası güç seviyesi
 * @return 0 başarılı, negatif hata kodu
 */
int at1846s_set_tx_power(uint8_t power_level);

/**
 * @brief RSSI değerini oku
 * @return RSSI değeri (dBm), negatif geçersiz
 */
int16_t at1846s_get_rssi(void);

/**
 * @brief Squelch seviyesi ayarla
 * @param level 0-8 arası squelch seviyesi (0=kapalı)
 * @return 0 başarılı, negatif hata kodu
 */
int at1846s_set_squelch(uint8_t level);

/**
 * @brief Squelch durumunu oku
 * @return true=açık (sinyal var), false=kapalı
 */
bool at1846s_get_squelch_status(void);

/**
 * @brief RX ses seviyesi ayarla
 * @param volume 0-15 arası ses seviyesi
 * @return 0 başarılı, negatif hata kodu
 */
int at1846s_set_rx_volume(uint8_t volume);

/**
 * @brief CTCSS/DCS tonu ayarla
 * @param config Ton yapılandırması
 * @return 0 başarılı, negatif hata kodu
 */
int at1846s_set_tone(const at1846s_tone_config_t *config);

/**
 * @brief Durum bilgisini oku
 * @param status Durum yapısı (çıktı)
 * @return 0 başarılı, negatif hata kodu
 */
int at1846s_get_status(at1846s_status_t *status);

/**
 * @brief Ham register oku (debug/araştırma için)
 * @param reg_addr Register adresi
 * @return 16-bit register değeri, negatif hata kodu
 */
int at1846s_read_reg(uint8_t reg_addr);

/**
 * @brief Ham register yaz (debug/araştırma için)
 * @param reg_addr Register adresi
 * @param value    16-bit değer
 * @return 0 başarılı, negatif hata kodu
 *
 * UYARI: Yanlış register değerleri donanıma zarar verebilir!
 */
int at1846s_write_reg(uint8_t reg_addr, uint16_t value);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_AT1846S_H */
