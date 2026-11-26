/**
 * @file    driver_at1846s.c
 * @brief   AT1846S RF Transceiver Driver — AT-D878 Open Firmware
 * @version 0.1.0
 *
 * BUS: Bit-banged 3-wire GPIO (PB3=SCLK, PB4=SDIO, PB5=SEN).
 * Stok firmware V4.00 RE ile doğrulandı: at1846_write_reg @ 0x0807A6FC.
 *
 * FREKANS: reg_val = freq_kHz * 16  (I2C/SPI bağımsız ölçekleme).
 * Stok firmware referans: at1846_set_freq @ 0x0805F504.
 *
 * Kaynaklar:
 *   reverse-engineering/OVERVIEW.md   — donanım/bus bölümü
 *   docs/REVERSE_ENGINEERING_BRIDGE.md — sürücü rehberi
 *   reverse-engineering/docs/patch.md  — VCO sınır ölçümleri
 */

#include "driver_at1846s.h"
#include "hal_gpio.h"
#include <string.h>

/* ============================================================
 * Bit-banged 3-wire GPIO yardımcı makroları
 *
 * SEN aktif-düşük; protokol:
 *   SEN low → 7-bit addr (MSB first) | 0 (yaz) / 1 (oku)
 *   → 16-bit değer (MSB first) → SEN high
 * SDIO yazma: MCU çıktı; okuma: MCU giriş (SEN düşükken yön değişir).
 * ============================================================ */

#ifndef HAL_GPIO_PIN_SET
/* HAL henüz tam implement edilmediyse placeholder makrolar */
#define HAL_GPIO_PIN_SET(port, bit)   ((void)0)
#define HAL_GPIO_PIN_CLR(port, bit)   ((void)0)
#define HAL_GPIO_PIN_READ(port, bit)  (0U)
#define HAL_GPIO_SET_OUTPUT(port, bit) ((void)0)
#define HAL_GPIO_SET_INPUT(port, bit)  ((void)0)
#define HAL_GPIO_DELAY_NS(ns)          ((void)0)
#endif

#define SCLK_HI()   HAL_GPIO_PIN_SET(AT1846S_PIN_SCLK_PORT, AT1846S_PIN_SCLK_BIT)
#define SCLK_LO()   HAL_GPIO_PIN_CLR(AT1846S_PIN_SCLK_PORT, AT1846S_PIN_SCLK_BIT)
#define SDIO_HI()   HAL_GPIO_PIN_SET(AT1846S_PIN_SDIO_PORT, AT1846S_PIN_SDIO_BIT)
#define SDIO_LO()   HAL_GPIO_PIN_CLR(AT1846S_PIN_SDIO_PORT, AT1846S_PIN_SDIO_BIT)
#define SEN_HI()    HAL_GPIO_PIN_SET(AT1846S_PIN_SEN_PORT,  AT1846S_PIN_SEN_BIT)
#define SEN_LO()    HAL_GPIO_PIN_CLR(AT1846S_PIN_SEN_PORT,  AT1846S_PIN_SEN_BIT)
#define SDIO_RD()   HAL_GPIO_PIN_READ(AT1846S_PIN_SDIO_PORT, AT1846S_PIN_SDIO_BIT)
#define SDIO_OUT()  HAL_GPIO_SET_OUTPUT(AT1846S_PIN_SDIO_PORT, AT1846S_PIN_SDIO_BIT)
#define SDIO_IN()   HAL_GPIO_SET_INPUT(AT1846S_PIN_SDIO_PORT,  AT1846S_PIN_SDIO_BIT)
/* En az 100 ns setup/hold süresi (stok firmware ~100 ns boşluk bırakıyor) */
#define WIRE_DELAY() HAL_GPIO_DELAY_NS(200)

/* ============================================================
 * Modül durumu
 * ============================================================ */

static at1846s_config_t  s_cfg;
static at1846s_status_t  s_st;
static bool              s_initialized = false;

/* ============================================================
 * Bit-banged 3-wire protokol
 *
 * Stok at1846_write_reg: SEN low → addr[6:0] | 0 (7 bit MSB önce)
 *                         → val[15:0] (16 bit MSB önce) → SEN high.
 * Stok at1846_read_reg:  SEN low → addr[6:0] | 1 (7 bit) → SDIO giriş
 *                         → val[15:0] oku → SEN high.
 * Her bit: SCLK low → data hazırla → SCLK high → 200ns → SCLK low.
 * ============================================================ */

static void _write_bit(uint8_t bit)
{
    SCLK_LO();
    if (bit) SDIO_HI(); else SDIO_LO();
    WIRE_DELAY();
    SCLK_HI();
    WIRE_DELAY();
}

static uint8_t _read_bit(void)
{
    SCLK_LO();
    WIRE_DELAY();
    SCLK_HI();
    uint8_t b = (SDIO_RD() != 0) ? 1u : 0u;
    WIRE_DELAY();
    return b;
}

static void _at1846s_write(uint8_t reg, uint16_t val)
{
    SEN_LO();
    SDIO_OUT();
    /* 7-bit adres | yazma biti (0) */
    for (int i = 6; i >= 0; --i)
        _write_bit((reg >> i) & 1u);
    _write_bit(0); /* R/W = 0 (yaz) */
    /* 16-bit değer */
    for (int i = 15; i >= 0; --i)
        _write_bit((val >> i) & 1u);
    SEN_HI();
}

static uint16_t _at1846s_read(uint8_t reg)
{
    SEN_LO();
    SDIO_OUT();
    /* 7-bit adres | okuma biti (1) */
    for (int i = 6; i >= 0; --i)
        _write_bit((reg >> i) & 1u);
    _write_bit(1); /* R/W = 1 (oku) */
    SDIO_IN();
    uint16_t val = 0;
    for (int i = 15; i >= 0; --i)
        val |= (uint16_t)(_read_bit() << i);
    SEN_HI();
    SDIO_OUT(); /* varsayılan yön: çıktı */
    return val;
}

/* ============================================================
 * Frekans yardımcıları
 *
 * Stok firmware at1846_set_freq @ 0x0805F504:
 *   reg_val = (freq_10hz / 100) * 16   /* 10Hz birim → kHz → ×16 */
 *
 * Bizim arg'ımız Hz; dönüşüm: reg_val = (freq_hz / 1000) * 16
 * Küsurat yuvarlama: (freq_hz * 16 + 500) / 1000
 * ============================================================ */

static uint32_t _freq_to_regval(uint32_t freq_hz)
{
    return (freq_hz / 1000U) * 16U;
}

static uint8_t _band_from_freq(uint32_t freq_hz)
{
    if (freq_hz <= AT1846S_FREQ_VHF_MAX_HZ) return AT1846S_BAND_VHF;
    if (freq_hz <= AT1846S_FREQ_220_MAX_HZ) return AT1846S_BAND_220;
    return AT1846S_BAND_UHF;
}

static bool _freq_in_range(uint32_t freq_hz)
{
    return (freq_hz >= AT1846S_FREQ_VHF_MIN_HZ && freq_hz <= AT1846S_FREQ_VHF_MAX_HZ)
        || (freq_hz >= AT1846S_FREQ_220_MIN_HZ && freq_hz <= AT1846S_FREQ_220_MAX_HZ)
        || (freq_hz >= AT1846S_FREQ_UHF_MIN_HZ && freq_hz <= AT1846S_FREQ_UHF_MAX_HZ);
}

/* Stok at1846_set_freq'ten çıkarılan spur ref değeri seçimi.
 * Belirli frekanslarda reg 0x05 farklı değer alır (144, 159, 410, 455,
 * 476–482 MHz civarı). Kaynaktan tam liste çıkarılana kadar NORMAL kullan. */
static uint16_t _ref_for_freq(uint32_t freq_hz)
{
    (void)freq_hz;
    return AT1846S_REF_NORMAL; /* TODO: spur listesi RE tamamlandığında genişlet */
}

/* ============================================================
 * Init register sırası
 *
 * Stok at1846_init_regs @ 0x0805E8CE çağrısının özeti.
 * Gerçek proje derlemesi için tüm sabitler RE'den tamamlanacak.
 * ============================================================ */

static int _init_regs(void)
{
    /* Soft reset kaldır */
    _at1846s_write(0x30, 0x0004);
    /* LDO / referans */
    _at1846s_write(AT1846S_REG_LDO_VOLTAGE, 0x0380);
    /* Varsayılan bant genişliği: 12.5 kHz */
    uint16_t ch = _at1846s_read(AT1846S_REG_CH_MODE);
    ch &= ~AT1846S_CH_MODE_25KHZ;
    _at1846s_write(AT1846S_REG_CH_MODE, ch);
    /* Ref frekans: normal */
    _at1846s_write(AT1846S_REG_REF_FREQ, AT1846S_REF_NORMAL);
    return 0;
}

/* ============================================================
 * Genel API
 * ============================================================ */

int at1846s_init(const at1846s_config_t *config)
{
    if (!config) return -1;
    memcpy(&s_cfg, config, sizeof(at1846s_config_t));
    if (s_cfg.xtal_freq_hz == 0)
        s_cfg.xtal_freq_hz = 26000000U; /* varsayılan kristal */
    memset(&s_st, 0, sizeof(at1846s_status_t));
    s_initialized = false;

    /* GPIO pinlerini başlat */
    HAL_GPIO_SET_OUTPUT(AT1846S_PIN_SCLK_PORT, AT1846S_PIN_SCLK_BIT);
    HAL_GPIO_SET_OUTPUT(AT1846S_PIN_SDIO_PORT, AT1846S_PIN_SDIO_BIT);
    HAL_GPIO_SET_OUTPUT(AT1846S_PIN_SEN_PORT,  AT1846S_PIN_SEN_BIT);
    SEN_HI();
    SCLK_LO();

    /* Çip kimliğini doğrula (reg 0x00 = 0x1846) */
    uint16_t id = _at1846s_read(AT1846S_REG_CHIP_ID);
    if (id != 0x1846) return -2; /* Çip bulunamadı veya bus bağlantısı yok */

    int rc = _init_regs();
    if (rc != 0) return rc;

    s_initialized = true;
    return 0;
}

void at1846s_deinit(void)
{
    if (!s_initialized) return;
    at1846s_rx_off();
    at1846s_tx_off();
    SEN_HI();
    s_initialized = false;
}

int at1846s_set_frequency(uint32_t freq_hz)
{
    if (!s_initialized) return -1;
    if (!_freq_in_range(freq_hz)) return -2;

    /* Ref/spur seçimi */
    _at1846s_write(AT1846S_REG_REF_FREQ, _ref_for_freq(freq_hz));

    /* Frekans kayıtları: reg_val = freq_kHz * 16 */
    uint32_t rv = _freq_to_regval(freq_hz);
    _at1846s_write(AT1846S_REG_FREQ_HI, (uint16_t)((rv >> 16) & 0x3FFF));
    _at1846s_write(AT1846S_REG_FREQ_LO, (uint16_t)(rv & 0xFFFF));

    /* Bant durumu reg 0x0F'ten otomatik güncellenir (salt okunur) */
    s_st.band        = _band_from_freq(freq_hz);
    s_st.frequency_hz = freq_hz;

    /* Front-end GPIO: aktif bant açık, diğeri kapalı.
     * Stok: at1846_frontends_set sadece aktif VFO bandını açar. */
    uint16_t fe = _at1846s_read(AT1846S_REG_FRONTEND_GPIO);
    if (s_st.band == AT1846S_BAND_VHF || s_st.band == AT1846S_BAND_220) {
        fe = (fe & ~(AT1846S_FE_VHF_ON | AT1846S_FE_UHF_ON))
           | AT1846S_FE_VHF_ON | AT1846S_FE_UHF_OFF;
    } else {
        fe = (fe & ~(AT1846S_FE_VHF_ON | AT1846S_FE_UHF_ON))
           | AT1846S_FE_UHF_ON | AT1846S_FE_VHF_OFF;
    }
    _at1846s_write(AT1846S_REG_FRONTEND_GPIO, fe);

    /* TODO: TIM3 CCR1 tracking-filter PWM — frekansa göre ayarlanacak.
     * Stok: rf_frontend_tune_from_tables kalibrasyon tablosundan değer
     * alır ve apc_pwm_write ile TIM3 CC1'e yazar. Bu olmadan ~25 dB kayıp. */

    return 0;
}

int64_t at1846s_get_frequency(void)
{
    return (int64_t)s_st.frequency_hz;
}

int at1846s_rx_on(void)
{
    if (!s_initialized) return -1;
    /* reg 0x30: RX bit OFF → ON (at1846_rx_restart eşdeğeri) */
    uint16_t ch = _at1846s_read(AT1846S_REG_CH_MODE);
    ch &= ~AT1846S_CH_MODE_TX_ON;
    _at1846s_write(AT1846S_REG_CH_MODE, ch & ~AT1846S_CH_MODE_RX_ON); /* önce OFF */
    _at1846s_write(AT1846S_REG_CH_MODE, ch | AT1846S_CH_MODE_RX_ON);  /* sonra ON */
    s_st.rx_active = true;
    s_st.tx_active = false;
    return 0;
}

int at1846s_rx_off(void)
{
    if (!s_initialized) return -1;
    uint16_t ch = _at1846s_read(AT1846S_REG_CH_MODE);
    _at1846s_write(AT1846S_REG_CH_MODE, ch & ~AT1846S_CH_MODE_RX_ON);
    s_st.rx_active = false;
    return 0;
}

int at1846s_tx_on(void)
{
    if (!s_initialized) return -1;
    uint16_t ch = _at1846s_read(AT1846S_REG_CH_MODE);
    ch &= ~AT1846S_CH_MODE_RX_ON;
    _at1846s_write(AT1846S_REG_CH_MODE, ch | AT1846S_CH_MODE_TX_ON);
    s_st.tx_active = true;
    s_st.rx_active = false;
    return 0;
}

int at1846s_tx_off(void)
{
    if (!s_initialized) return -1;
    uint16_t ch = _at1846s_read(AT1846S_REG_CH_MODE);
    _at1846s_write(AT1846S_REG_CH_MODE, ch & ~AT1846S_CH_MODE_TX_ON);
    s_st.tx_active = false;
    return 0;
}

int at1846s_frontend_set(uint8_t band, bool enable)
{
    if (!s_initialized) return -1;
    uint16_t fe = _at1846s_read(AT1846S_REG_FRONTEND_GPIO);
    if (band == AT1846S_BAND_VHF || band == AT1846S_BAND_220) {
        fe = (fe & ~AT1846S_FE_VHF_ON)
           | (enable ? AT1846S_FE_VHF_ON : AT1846S_FE_VHF_OFF);
    } else {
        fe = (fe & ~AT1846S_FE_UHF_ON)
           | (enable ? AT1846S_FE_UHF_ON : AT1846S_FE_UHF_OFF);
    }
    _at1846s_write(AT1846S_REG_FRONTEND_GPIO, fe);
    return 0;
}

int16_t at1846s_get_rssi(void)
{
    if (!s_initialized) return -128;
    /* Reg 0x1B: 7-bit değer. Stok firmware RSSI ortalaması ≥25 ms bekliyor.
     * Ölçüm: averaging mode 3 (reg 0x5A[11:10]=3) + ≥25 ms dwell gerekli. */
    uint16_t raw = _at1846s_read(AT1846S_REG_RSSI);
    /* dBm dönüşüm: stok firmware -120 + raw/2 yaklaşımını kullanıyor */
    int16_t dbm = (int16_t)(-120 + (int16_t)((raw & 0x7F) / 2));
    s_st.rssi_dbm = dbm;
    return dbm;
}

bool at1846s_is_locked(void)
{
    if (!s_initialized) return false;
    uint16_t status = _at1846s_read(AT1846S_REG_LOCK_STATUS);
    bool locked = (status & 0x8000) != 0;
    s_st.pll_locked = locked;
    s_st.vco_capbank = (uint16_t)(_at1846s_read(AT1846S_REG_VCO_CAPBANK) & 0x01FF);
    return locked;
}

int at1846s_set_squelch(uint8_t level)
{
    if (!s_initialized || level > 8) return -1;
    /* Stok rf_squelch_cal_apply kalibrasyon tablosundan değer alır;
     * kalibrasyon verisi olmadan basit lineer yaklaşım: */
    uint16_t th = (uint16_t)(level * 12);
    _at1846s_write(AT1846S_REG_SQUELCH_TH, th);
    return 0;
}

bool at1846s_get_squelch_status(void)
{
    if (!s_initialized) return false;
    /* Stok: reg 0x1C[0] squelch detect */
    uint16_t st = _at1846s_read(0x1C);
    s_st.squelch_open = (st & 0x0001) != 0;
    return s_st.squelch_open;
}

int at1846s_set_rx_volume(uint8_t volume)
{
    if (!s_initialized || volume > 15) return -1;
    _at1846s_write(AT1846S_REG_RX_VOL, (uint16_t)(volume & 0x0F));
    return 0;
}

int at1846s_set_tone(const at1846s_tone_config_t *config)
{
    if (!s_initialized || !config) return -1;
    /* TODO: CTCSS/DCS register haritası RE tamamlandığında implemente edilecek. */
    (void)config;
    return 0;
}

int at1846s_get_status(at1846s_status_t *out)
{
    if (!out) return -1;
    /* Anlık PLL ve squelch durumunu da güncelle */
    s_st.pll_locked = at1846s_is_locked();
    s_st.squelch_open = at1846s_get_squelch_status();
    memcpy(out, &s_st, sizeof(at1846s_status_t));
    return 0;
}

int at1846s_read_reg(uint8_t reg)
{
    if (!s_initialized) return -1;
    return (int)_at1846s_read(reg);
}

int at1846s_write_reg(uint8_t reg, uint16_t value)
{
    if (!s_initialized) return -1;
    _at1846s_write(reg, value);
    return 0;
}

int at1846s_reset_and_reinit(void)
{
    /*
     * Stok at1846_reset_wait_id @ 0x0805E870:
     *   1. reg 0x30 = 0x0001  (soft reset aktif)
     *   2. reg 0x0A pulse     (PA bias sıfırla)
     *   3. reg 0x04 set       (clk mode)
     *   4. at1846_init_regs   (tüm kayıt sırası)
     *
     * PLL wedge: reg 0x27 puls sırası gerektiriyor
     * (0x74a4 → 0x7ca4 → 0x6ca4); şimdilik init_regs yeterli.
     */
    _at1846s_write(0x30, 0x0001); /* soft reset */
    for (volatile int i = 0; i < 1000; ++i); /* ~kısa bekleme */
    _at1846s_write(0x30, 0x0004); /* reset kaldır */

    int rc = _init_regs();
    if (rc != 0) return rc;

    /* Mevcut frekansı yeniden yükle */
    if (s_st.frequency_hz > 0) {
        rc = at1846s_set_frequency(s_st.frequency_hz);
    }
    return rc;
}
