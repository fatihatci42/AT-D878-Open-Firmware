/**
 * @file    driver_at1846s.h
 * @brief   AT1846S RF Transceiver Driver - AT-D878 Open Firmware
 * @version 0.1.0
 *
 * Auctus AT1846S tek çipli VHF/UHF RF alıcı-verici sürücüsü.
 *
 * BUS UYARISI: Stok firmware V4.00 tersine mühendisliği ile kesinlikle
 * doğrulandı — AT1846S, I2C veya SPI ile değil, BIT-BANGED 3-WIRE GPIO
 * üzerinden kontrol edilir.
 *
 *   PB3 = SCLK   (at1846_sclk)
 *   PB4 = SDIO   (at1846_sdio, çift yönlü)
 *   PB5 = SEN    (at1846_sen, chip select aktif-düşük)
 *
 * Protokol: 7-bit register adresi | 0x80 (okuma) + 16-bit değer.
 * Stok referans: at1846_write_reg @ 0x0807A6FC (324 çağrı),
 *               at1846_read_reg  @ 0x0807A748 (yaklaşık).
 *
 * FREKANS HESAPLAMA (doğrulanmış):
 *   reg_val = freq_kHz * 16
 *   Reg 0x29 = (reg_val >> 16) & 0x3FFF
 *   Reg 0x2A = (reg_val) & 0xFFFF
 *   NOT: (freq_hz << 16) / xtal formülü YANLIŞ — xtal-bağımsız ölçekleme.
 *
 * BANT SINIRLARI (stok radyo ölçümü, bir cihaz üzerinde):
 *   VHF  (÷6): 131–178 MHz
 *   220  (÷4): 200–286 MHz
 *   UHF  (÷2): 393–572 MHz
 *   Boşluklar: 178–200 MHz ve 286–393 MHz erişilemez.
 *
 * PLL KİLİT:
 *   Reg 0x0D[15] = lock flag
 *   Reg 0x13[8:0] = VCO cap-bank (511 = sınır dışı)
 *   Reg 0x0F[7:6] = bant durumu (11=VHF÷6, 10=220÷4, 00=UHF÷2) — SALT OKUNUR
 *
 * FRONT-END GPIO (AT1846S reg 0x1F):
 *   [9:8] = VHF frontend: 11=açık, 10=kapalı
 *   [1:0] = UHF frontend: 11=açık, 10=kapalı
 *
 * RX YENİDEN TUNE SIRASI:
 *   1. Reg 0x29/0x2A frekans yaz
 *   2. Reg 0x30 bit5 (RX) OFF → ON (at1846_rx_restart eşdeğeri)
 *   Hızlı yol ~2.5 ms; tam RF komut yolu ~175 ms.
 *
 * İzleme filtresi: TIM3 CCR1 PWM — her retune'da ayarlanmalı,
 * aksi takdirde hassasiyet ~25 dB düşer.
 *
 * Kaynaklar: reverse-engineering/OVERVIEW.md, docs/REVERSE_ENGINEERING_BRIDGE.md
 */

#ifndef DRIVER_AT1846S_H
#define DRIVER_AT1846S_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * GPIO Pin Tanımları (RE doğrulaması)
 * ============================================================ */

/** SCLK: PB3 */
#define AT1846S_PIN_SCLK_PORT   GPIOB
#define AT1846S_PIN_SCLK_BIT    3

/** SDIO: PB4 (çift yönlü, okuma/yazma için yön değiştirilir) */
#define AT1846S_PIN_SDIO_PORT   GPIOB
#define AT1846S_PIN_SDIO_BIT    4

/** SEN: PB5 (aktif-düşük chip select) */
#define AT1846S_PIN_SEN_PORT    GPIOB
#define AT1846S_PIN_SEN_BIT     5

/* ============================================================
 * AT1846S Register Adresleri
 * ============================================================ */

#define AT1846S_REG_CHIP_ID         0x00  /**< Çip kimliği (okuma: 0x1846) */
#define AT1846S_REG_CLK_MODE        0x04  /**< Saat modu */
#define AT1846S_REG_REF_FREQ        0x05  /**< Ref/spur seçimi (8763/8543/8643) */
#define AT1846S_REG_LDO_BYPASS      0x08  /**< LDO bypass */
#define AT1846S_REG_LDO_VOLTAGE     0x09  /**< LDO çıkış gerilimi */
#define AT1846S_REG_PA_BIAS         0x0A  /**< PA bias */
#define AT1846S_REG_LOCK_STATUS     0x0D  /**< [15]=PLL lock flag */
#define AT1846S_REG_BAND_STATUS     0x0F  /**< [7:6] bant durumu — SALT OKUNUR */
#define AT1846S_REG_VCO_CAPBANK     0x13  /**< [8:0] VCO cap-bank (511=sınır dışı) */
#define AT1846S_REG_RSSI            0x1B  /**< RSSI (8-bit) */
#define AT1846S_REG_FRONTEND_GPIO   0x1F  /**< [9:8]=VHF FE, [1:0]=UHF FE */
#define AT1846S_REG_FREQ_HI         0x29  /**< Frekans üst: f_kHz*16 >> 16 */
#define AT1846S_REG_FREQ_LO         0x2A  /**< Frekans alt: f_kHz*16 & 0xFFFF */
#define AT1846S_REG_CH_MODE         0x30  /**< [13:12]=BW, [6]=TX, [5]=RX */
#define AT1846S_REG_SQUELCH_TH      0x49  /**< Squelch eşik */
#define AT1846S_REG_RX_VOL          0x44  /**< RX ses seviyesi */
#define AT1846S_REG_EXT_PAGE        0x7F  /**< Genişletilmiş sayfa (1=AGC tablo) */

/* Reg 0x30 bit maskeleri */
#define AT1846S_CH_MODE_25KHZ       (0x3 << 12)
#define AT1846S_CH_MODE_12_5KHZ     (0x0 << 12)
#define AT1846S_CH_MODE_TX_ON       (1 << 6)
#define AT1846S_CH_MODE_RX_ON       (1 << 5)

/* Reg 0x05 referans seçimi sabitleri (stok firmware değerleri) */
#define AT1846S_REF_NORMAL          0x8763  /**< Normal frekanslar */
#define AT1846S_REF_SPUR_A          0x8543  /**< Spur kaçınma A (144, 410 MHz...) */
#define AT1846S_REF_SPUR_B          0x8643  /**< Spur kaçınma B (159, 455 MHz...) */

/* Reg 0x1F front-end sabitleri */
#define AT1846S_FE_VHF_ON           (0x3 << 8)
#define AT1846S_FE_VHF_OFF          (0x2 << 8)
#define AT1846S_FE_UHF_ON           (0x3 << 0)
#define AT1846S_FE_UHF_OFF          (0x2 << 0)

/* ============================================================
 * Bant Seçimi (reg 0x0F[7:6] durum — yazılamaz; bant freq'ten çıkar)
 * ============================================================ */

#define AT1846S_BAND_VHF            0  /**< 131–178 MHz, bölücü ÷6 */
#define AT1846S_BAND_220            1  /**< 200–286 MHz, bölücü ÷4 */
#define AT1846S_BAND_UHF            2  /**< 393–572 MHz, bölücü ÷2 */

/* Frekans sınırları (ölçülen kilit aralığı; kenar bandlar doğrusal değil) */
#define AT1846S_FREQ_VHF_MIN_HZ     131000000U
#define AT1846S_FREQ_VHF_MAX_HZ     178000000U
#define AT1846S_FREQ_220_MIN_HZ     200000000U
#define AT1846S_FREQ_220_MAX_HZ     286000000U
#define AT1846S_FREQ_UHF_MIN_HZ     393000000U
#define AT1846S_FREQ_UHF_MAX_HZ     572000000U

/* ============================================================
 * Veri Yapıları
 * ============================================================ */

/**
 * @brief AT1846S başlatma yapılandırması.
 *
 * Bus türü sabit: bit-banged 3-wire GPIO (PB3/PB4/PB5).
 * Yapılandırma değerleri gelecekte genişletilebilir.
 */
typedef struct {
    uint32_t xtal_freq_hz;  /**< Referans kristal Hz (0 = 26 MHz varsayılan) */
} at1846s_config_t;

/** @brief RF durum anlık görüntüsü */
typedef struct {
    uint32_t frequency_hz;  /**< Ayarlı frekans (Hz) */
    int16_t  rssi_dbm;      /**< RSSI (dBm) */
    bool     pll_locked;    /**< reg 0x0D[15] = true ise PLL kilitli */
    bool     squelch_open;  /**< Squelch açık mı (sinyal var) */
    bool     tx_active;     /**< TX modu aktif */
    bool     rx_active;     /**< RX modu aktif */
    uint8_t  band;          /**< AT1846S_BAND_* */
    uint8_t  bandwidth;     /**< 0=12.5kHz, 1=25kHz */
    uint16_t vco_capbank;   /**< reg 0x13[8:0] — 511 = sınır dışı */
} at1846s_status_t;

/** @brief CTCSS/DCS yapılandırması */
typedef struct {
    bool     enabled;
    bool     is_dcs;
    uint16_t ctcss_freq_x10; /**< CTCSS Hz×10 (67.0 Hz → 670) */
    uint16_t dcs_code;
    bool     dcs_inverted;
} at1846s_tone_config_t;

/* ============================================================
 * API
 * ============================================================ */

/** @brief Bit-banged 3-wire GPIO ve çip reset/init sırasını çalıştır. */
int  at1846s_init(const at1846s_config_t *config);

/** @brief TX/RX kapat, GPIO yönlerini sıfırla. */
void at1846s_deinit(void);

/**
 * @brief RF frekans ayarla.
 * @param freq_hz  Hz cinsinden (145000000 = 145.000 MHz)
 * @return 0 başarılı; -1 başlatılmamış; -2 bant sınırı dışı
 *
 * İç formül: reg_val = (freq_hz / 1000) * 16
 * Front-end GPIO, tracking-filter PWM otomatik ayarlanır.
 */
int  at1846s_set_frequency(uint32_t freq_hz);

/** @brief Son ayarlanan frekansı döndür (Hz). */
int64_t at1846s_get_frequency(void);

/**
 * @brief RX modunu başlat (reg 0x30 RX bit OFF→ON).
 * PLL kilit bekleme süresi ~2 ms.
 */
int  at1846s_rx_on(void);
int  at1846s_rx_off(void);

/** @brief TX modunu başlat/kapat. */
int  at1846s_tx_on(void);
int  at1846s_tx_off(void);

/** @brief VHF/UHF front-end GPIO aç/kapat. */
int  at1846s_frontend_set(uint8_t band, bool enable);

/** @brief RSSI oku (reg 0x1B). Geçersiz ise negatif. */
int16_t at1846s_get_rssi(void);

/** @brief PLL kilit durumu (reg 0x0D[15]). */
bool at1846s_is_locked(void);

/** @brief Squelch eşiği ayarla (0–8). */
int  at1846s_set_squelch(uint8_t level);

/** @brief Squelch durumunu oku. */
bool at1846s_get_squelch_status(void);

/** @brief Ses seviyesi (0–15). */
int  at1846s_set_rx_volume(uint8_t volume);

/** @brief CTCSS/DCS tonu yapılandır. */
int  at1846s_set_tone(const at1846s_tone_config_t *config);

/** @brief Anlık durum al. */
int  at1846s_get_status(at1846s_status_t *status);

/** @brief Ham register oku (debug). Negatif = hata. */
int  at1846s_read_reg(uint8_t reg);

/** @brief Ham register yaz (debug). UYARI: hatalı değer donanımı bozar. */
int  at1846s_write_reg(uint8_t reg, uint16_t value);

/**
 * @brief Çip sıfırlama + init sırası (wedge'den kurtarma).
 *
 * PLL wedge durumu (reg 0x13 = 0xa1ff, hiçbir şey kitlenemiyor):
 *   1. Soft reset: reg 0x30 = 0x0001
 *   2. reg 0x0A / 0x04 pulse (at1846_reset_wait_id eşdeğeri)
 *   3. Init register sırası yeniden çalıştır
 *   4. Frekans yeniden yükle
 * Stok referans: `at1846_reset_wait_id` @ 0x0805E870
 */
int  at1846s_reset_and_reinit(void);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_AT1846S_H */
