/**
 * @file    driver_sct3258.c
 * @brief   SCT3258 DMR Baseband Processor Driver — AT-D878 Open Firmware
 * @version 0.1.0
 *
 * CML/Sicomm SCT3258TD DMR dijital baseband işlemci sürücüsü.
 *
 * HPI (Host Port Interface) 8-bit paralel bus:
 *   PE0  = SCT_RST    (Reset, Active LOW)
 *   PE1  = HPI_CTRL   (R/W kontrol)
 *   PE2  = HPI_ALE    (Address Latch Enable)
 *   PE3  = HPI_CS     (Chip Select, Active LOW)
 *   PE4  = HPI_INT    (Interrupt, MCU girişi)
 *   PD8-PD15 = HPI_D0-D7 (8-bit veri bus)
 *
 * KRİTİK UYARI:
 *   SCT3258 HPI protokolü tam olarak dokümante EDİLMEMİŞTİR.
 *   Bu implementasyon topluluk reverse-engineering çalışmalarına
 *   ve benzer IC'lerin (CMX991, SCT3258 ailesinin) davranışlarına
 *   dayanmaktadır.
 *
 *   AMBE+2 vocoder DVSI patent koruması altındadır.
 *   Bu sürücü yalnızca SCT3258'in kontrol ve durum register'larını
 *   yönetir; vocoder kodlaması donanım tarafından yapılır.
 */

#include "driver_sct3258.h"
#include "hal_gpio.h"
#include "hal_systick.h"
#include <string.h>

/* ============================================================
 * Pin Tanımları
 * ============================================================ */

/* Kontrol pinleri */
#define PIN_SCT_RST       ((hal_gpio_id_t){GPIO_PORT_E, 0})
#define PIN_HPI_CTRL      ((hal_gpio_id_t){GPIO_PORT_E, 1})
#define PIN_HPI_ALE       ((hal_gpio_id_t){GPIO_PORT_E, 2})
#define PIN_HPI_CS        ((hal_gpio_id_t){GPIO_PORT_E, 3})
#define PIN_HPI_INT       ((hal_gpio_id_t){GPIO_PORT_E, 4})

/* Veri bus pinleri (PD8-PD15) */
static const hal_gpio_id_t HPI_DATA_PINS[8] = {
    {GPIO_PORT_D,  8}, {GPIO_PORT_D,  9},
    {GPIO_PORT_D, 10}, {GPIO_PORT_D, 11},
    {GPIO_PORT_D, 12}, {GPIO_PORT_D, 13},
    {GPIO_PORT_D, 14}, {GPIO_PORT_D, 15},
};

/* ============================================================
 * HPI Register Adresleri (kısmen doğrulanmış)
 * ============================================================ */

#define HPI_REG_CHIP_ID       0x0000  /* Çip kimlik (salt okunur) */
#define HPI_REG_CONTROL       0x0001  /* Ana kontrol register */
#define HPI_REG_STATUS        0x0002  /* Durum register */
#define HPI_REG_MODE          0x0003  /* Çalışma modu */
#define HPI_REG_DMR_CC        0x0010  /* DMR Color Code */
#define HPI_REG_DMR_SLOT      0x0011  /* DMR Time Slot */
#define HPI_REG_DMR_SRC_ID_H  0x0012  /* Kaynak ID üst */
#define HPI_REG_DMR_SRC_ID_L  0x0013  /* Kaynak ID alt */
#define HPI_REG_DMR_DST_ID_H  0x0014  /* Hedef ID üst */
#define HPI_REG_DMR_DST_ID_L  0x0015  /* Hedef ID alt */
#define HPI_REG_DMR_CALL_TYPE 0x0016  /* Çağrı tipi */
#define HPI_REG_TX_CTRL       0x0020  /* TX kontrol */
#define HPI_REG_RX_STATUS     0x0021  /* RX durum */
#define HPI_REG_VOCODER_CFG   0x0030  /* Vocoder yapılandırma */
#define HPI_REG_INT_STATUS    0x0040  /* Interrupt durum */
#define HPI_REG_INT_ENABLE    0x0041  /* Interrupt etkinleştirme */

/* Kontrol register bitleri */
#define CTRL_BIT_ENABLE       (1U << 0)
#define CTRL_BIT_DMR_MODE     (1U << 1)
#define CTRL_BIT_ANALOG_MODE  (1U << 2)
#define CTRL_BIT_TX_ON        (1U << 4)
#define CTRL_BIT_RX_ON        (1U << 5)

/* Durum register bitleri */
#define STATUS_BIT_READY      (1U << 0)
#define STATUS_BIT_SYNCED     (1U << 1)
#define STATUS_BIT_VOICE      (1U << 2)
#define STATUS_BIT_DATA       (1U << 3)
#define STATUS_BIT_TX_ACTIVE  (1U << 4)
#define STATUS_BIT_RX_ACTIVE  (1U << 5)

/* ============================================================
 * Statik Değişkenler
 * ============================================================ */

static sct3258_status_t s_status;
static bool s_initialized = false;

/* ============================================================
 * HPI Bus İşlemleri
 * ============================================================ */

/** Veri bus'ını çıkış moduna ayarla */
static void hpi_data_bus_output(void)
{
    for (int i = 0; i < 8; i++) {
        hal_gpio_configure(HPI_DATA_PINS[i], GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);
    }
}

/** Veri bus'ını giriş moduna ayarla */
static void hpi_data_bus_input(void)
{
    for (int i = 0; i < 8; i++) {
        hal_gpio_configure(HPI_DATA_PINS[i], GPIO_MODE_INPUT_PULLUP, GPIO_SPEED_50MHZ);
    }
}

/** 8-bit veriyi bus'a yaz */
static void hpi_data_bus_write(uint8_t value)
{
    for (int i = 0; i < 8; i++) {
        hal_gpio_write(HPI_DATA_PINS[i], (value >> i) & 1);
    }
}

/** 8-bit veriyi bus'tan oku */
static uint8_t hpi_data_bus_read(void)
{
    uint8_t value = 0;
    for (int i = 0; i < 8; i++) {
        if (hal_gpio_read(HPI_DATA_PINS[i])) {
            value |= (1U << i);
        }
    }
    return value;
}

/** Kısa gecikme (bus timing) */
static inline void hpi_delay(void)
{
    for (volatile int i = 0; i < 20; i++) {
        __asm__ volatile("nop");
    }
}

/**
 * @brief HPI register'a 16-bit değer yaz
 *
 * HPI Yazma Protokolü (tahmini):
 *   1. CS LOW
 *   2. ALE HIGH → Adres bus'a yaz → ALE LOW (adres kilitle)
 *   3. CTRL = WRITE
 *   4. Veri bus'a yaz (önce yüksek byte, sonra düşük byte)
 *   5. CS HIGH
 */
int sct3258_hpi_write(uint16_t addr, uint16_t value)
{
    if (!s_initialized) return -1;

    hpi_data_bus_output();

    /* Adres fazı */
    hal_gpio_clear(PIN_HPI_CS);       /* CS LOW */
    hal_gpio_set(PIN_HPI_ALE);        /* ALE HIGH */
    hal_gpio_clear(PIN_HPI_CTRL);     /* CTRL = WRITE */

    /* Adres yüksek byte */
    hpi_data_bus_write((uint8_t)(addr >> 8));
    hpi_delay();
    /* Adres düşük byte */
    hpi_data_bus_write((uint8_t)(addr & 0xFF));
    hpi_delay();

    hal_gpio_clear(PIN_HPI_ALE);      /* ALE LOW (adres kilitlendi) */
    hpi_delay();

    /* Veri fazı */
    hpi_data_bus_write((uint8_t)(value >> 8));    /* Yüksek byte */
    hpi_delay();
    hpi_data_bus_write((uint8_t)(value & 0xFF));  /* Düşük byte */
    hpi_delay();

    hal_gpio_set(PIN_HPI_CS);         /* CS HIGH */

    return 0;
}

/**
 * @brief HPI register'dan 16-bit değer oku
 */
int sct3258_hpi_read(uint16_t addr)
{
    if (!s_initialized) return -1;

    /* Adres fazı (çıkış) */
    hpi_data_bus_output();

    hal_gpio_clear(PIN_HPI_CS);
    hal_gpio_set(PIN_HPI_ALE);
    hal_gpio_set(PIN_HPI_CTRL);       /* CTRL = READ */

    hpi_data_bus_write((uint8_t)(addr >> 8));
    hpi_delay();
    hpi_data_bus_write((uint8_t)(addr & 0xFF));
    hpi_delay();

    hal_gpio_clear(PIN_HPI_ALE);
    hpi_delay();

    /* Veri fazı (giriş) */
    hpi_data_bus_input();
    hpi_delay();

    uint8_t hi = hpi_data_bus_read();
    hpi_delay();
    uint8_t lo = hpi_data_bus_read();

    hal_gpio_set(PIN_HPI_CS);

    return (int)((uint16_t)(hi << 8) | lo);
}

/* ============================================================
 * API İmplementasyonu
 * ============================================================ */

int sct3258_init(void)
{
    memset(&s_status, 0, sizeof(s_status));

    /* Kontrol pinlerini yapılandır */
    hal_gpio_configure(PIN_SCT_RST,  GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);
    hal_gpio_configure(PIN_HPI_CTRL, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);
    hal_gpio_configure(PIN_HPI_ALE,  GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);
    hal_gpio_configure(PIN_HPI_CS,   GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);
    hal_gpio_configure(PIN_HPI_INT,  GPIO_MODE_INPUT_PULLUP, GPIO_SPEED_50MHZ);

    /* Başlangıç durumları */
    hal_gpio_set(PIN_HPI_CS);       /* CS HIGH (deselect) */
    hal_gpio_clear(PIN_HPI_ALE);    /* ALE LOW */

    /* Reset dizisi: RST LOW → 10ms bekle → RST HIGH → 50ms bekle */
    hal_gpio_clear(PIN_SCT_RST);    /* Reset aktif */
    hal_delay_ms(10);
    hal_gpio_set(PIN_SCT_RST);      /* Reset bırak */
    hal_delay_ms(50);               /* Boot bekle */

    s_initialized = true;

    /* Çip hazır mı kontrol et */
    if (!sct3258_is_alive()) {
        s_initialized = false;
        return -1;
    }

    /* Varsayılan yapılandırma */
    sct3258_hpi_write(HPI_REG_INT_ENABLE, 0x003F);  /* Tüm interrupt'ları etkinleştir */
    sct3258_hpi_write(HPI_REG_CONTROL, CTRL_BIT_ENABLE);

    return 0;
}

void sct3258_deinit(void)
{
    if (s_initialized) {
        sct3258_hpi_write(HPI_REG_CONTROL, 0x0000); /* Devre dışı */
        hal_gpio_clear(PIN_SCT_RST);                /* Reset */
        s_initialized = false;
    }
}

bool sct3258_is_alive(void)
{
    if (!s_initialized) return false;

    int chip_id = sct3258_hpi_read(HPI_REG_CHIP_ID);
    /* SCT3258 beklenen chip ID: 0x3258 veya benzeri */
    return (chip_id > 0);
}

int sct3258_set_mode(sct3258_mode_t mode)
{
    if (!s_initialized) return -1;

    uint16_t ctrl = CTRL_BIT_ENABLE;

    switch (mode) {
    case SCT3258_MODE_DMR_RX:
        ctrl |= CTRL_BIT_DMR_MODE | CTRL_BIT_RX_ON;
        break;
    case SCT3258_MODE_DMR_TX:
        ctrl |= CTRL_BIT_DMR_MODE | CTRL_BIT_TX_ON;
        break;
    case SCT3258_MODE_ANALOG_RX:
        ctrl |= CTRL_BIT_ANALOG_MODE | CTRL_BIT_RX_ON;
        break;
    case SCT3258_MODE_ANALOG_TX:
        ctrl |= CTRL_BIT_ANALOG_MODE | CTRL_BIT_TX_ON;
        break;
    case SCT3258_MODE_IDLE:
    default:
        break;
    }

    int rc = sct3258_hpi_write(HPI_REG_CONTROL, ctrl);
    if (rc == 0) {
        s_status.mode = mode;
    }
    return rc;
}

int sct3258_set_dmr_params(uint8_t color_code, sct3258_slot_t slot,
                           uint32_t dst_id, sct3258_call_type_t call_type)
{
    if (!s_initialized) return -1;

    sct3258_hpi_write(HPI_REG_DMR_CC, (uint16_t)(color_code & 0x0F));
    sct3258_hpi_write(HPI_REG_DMR_SLOT, (uint16_t)slot);
    sct3258_hpi_write(HPI_REG_DMR_DST_ID_H, (uint16_t)(dst_id >> 16));
    sct3258_hpi_write(HPI_REG_DMR_DST_ID_L, (uint16_t)(dst_id & 0xFFFF));
    sct3258_hpi_write(HPI_REG_DMR_CALL_TYPE, (uint16_t)call_type);

    s_status.color_code = color_code;
    s_status.active_slot = slot;
    s_status.dst_id = dst_id;
    s_status.call_type = call_type;

    return 0;
}

int sct3258_set_dmr_id(uint32_t src_id)
{
    if (!s_initialized) return -1;

    sct3258_hpi_write(HPI_REG_DMR_SRC_ID_H, (uint16_t)(src_id >> 16));
    sct3258_hpi_write(HPI_REG_DMR_SRC_ID_L, (uint16_t)(src_id & 0xFFFF));
    s_status.src_id = src_id;

    return 0;
}

int sct3258_dmr_tx_start(void)
{
    return sct3258_set_mode(SCT3258_MODE_DMR_TX);
}

int sct3258_dmr_tx_stop(void)
{
    return sct3258_set_mode(SCT3258_MODE_IDLE);
}

int sct3258_get_status(sct3258_status_t *status)
{
    if (!status || !s_initialized) return -1;

    /* Donanım durum register'ını oku */
    int raw = sct3258_hpi_read(HPI_REG_STATUS);
    if (raw >= 0) {
        uint16_t sr = (uint16_t)raw;
        s_status.synced       = (sr & STATUS_BIT_SYNCED) != 0;
        s_status.voice_active = (sr & STATUS_BIT_VOICE)  != 0;
        s_status.data_active  = (sr & STATUS_BIT_DATA)   != 0;
    }

    memcpy(status, &s_status, sizeof(sct3258_status_t));
    return 0;
}
