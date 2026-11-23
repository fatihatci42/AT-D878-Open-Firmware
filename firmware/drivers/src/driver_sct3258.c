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
#include "sct3258_boot_blob.h"
#include <string.h>

/* ============================================================
 * Pin Tanımları (RadioFoundry Doğrulanmış Harita)
 * ============================================================ */

/* Kontrol pinleri */
#define PIN_HPI_HCSN       PIN_SCT_HCSN       /* PD2, Active LOW */
#define PIN_HPI_HWRN       PIN_SCT_HWRN       /* PD3, Active LOW */
#define PIN_HPI_HRDN       PIN_SCT_HRDN       /* PD4, Active LOW */
#define PIN_HPI_HOBIB      PIN_SCT_HOBIB      /* PD5, Input, LOW=data ready */
#define PIN_HPI_RSTN       PIN_SCT_RSTN       /* PD6, Reset Active LOW */
#define PIN_HPI_STRAP      PIN_SCT_PIO1_STRAP /* PC12, Intel mode LOW */
#define PIN_HPI_EN1        PIN_SCT_PWR_EN1    /* PA8, HIGH */
#define PIN_HPI_EN2        PIN_SCT_PWR_EN2    /* PC6, HIGH */
#define PIN_HPI_SLOT       PIN_SCT_TDMA_SLOT  /* PD0, TDMA slot timing EXTI */

/* Veri bus pinleri (PD8-PD15) */
static const hal_gpio_id_t HPI_DATA_PINS[8] = {
    {GPIO_PORT_D,  8}, {GPIO_PORT_D,  9},
    {GPIO_PORT_D, 10}, {GPIO_PORT_D, 11},
    {GPIO_PORT_D, 12}, {GPIO_PORT_D, 13},
    {GPIO_PORT_D, 14}, {GPIO_PORT_D, 15},
};

/* ============================================================
 * HPI Register Adresleri & Paket Başlığı
 * ============================================================ */

#define HPI_REG_CHIP_ID       0x0000  /* Çip kimlik */
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

/* SCT3258 Paket Başlığı: 0x84, 0xA9, 0x61 */
static const uint8_t SCT_PACKET_HEADER[3] __attribute__((unused)) = {0x84, 0xA9, 0x61};

/* ============================================================
 * Statik Değişkenler
 * ============================================================ */

static sct3258_status_t s_status;
static bool s_initialized = false;
static bool s_boot_loaded = false;

/* ============================================================
 * HPI Bus İşlemleri (Intel Timing)
 * ============================================================ */

static void hpi_data_bus_output(void)
{
    for (int i = 0; i < 8; i++) {
        hal_gpio_configure(HPI_DATA_PINS[i], GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);
    }
}

static void hpi_data_bus_input(void)
{
    for (int i = 0; i < 8; i++) {
        hal_gpio_configure(HPI_DATA_PINS[i], GPIO_MODE_INPUT_PULLUP, GPIO_SPEED_50MHZ);
    }
}

static void hpi_data_bus_write(uint8_t value)
{
    for (int i = 0; i < 8; i++) {
        hal_gpio_write(HPI_DATA_PINS[i], (value >> i) & 1);
    }
}

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

static inline void hpi_delay(void)
{
    for (volatile int i = 0; i < 20; i++) {
        __asm__ volatile("nop");
    }
}

/* HOBIB (FIFO meşgul değil) bekle */
static inline void hpi_wait_ready(void)
{
    uint32_t timeout = 5000;
    while (!hal_gpio_read(PIN_HPI_HOBIB) && --timeout) {
        hpi_delay();
    }
}

/**
 * @brief HPI register'a 16-bit değer yaz (Intel mode: CS LOW -> WR strobe)
 */
int sct3258_hpi_write(uint16_t addr, uint16_t value)
{
    if (!s_initialized) return -1;

    hpi_wait_ready();
    hpi_data_bus_output();

    hal_gpio_clear(PIN_HPI_HCSN);     /* CS LOW */

    /* Adres baytları */
    hpi_data_bus_write((uint8_t)(addr >> 8));
    hal_gpio_clear(PIN_HPI_HWRN);
    hpi_delay();
    hal_gpio_set(PIN_HPI_HWRN);
    hpi_delay();

    hpi_data_bus_write((uint8_t)(addr & 0xFF));
    hal_gpio_clear(PIN_HPI_HWRN);
    hpi_delay();
    hal_gpio_set(PIN_HPI_HWRN);
    hpi_delay();

    /* Veri baytları */
    hpi_data_bus_write((uint8_t)(value >> 8));
    hal_gpio_clear(PIN_HPI_HWRN);
    hpi_delay();
    hal_gpio_set(PIN_HPI_HWRN);
    hpi_delay();

    hpi_data_bus_write((uint8_t)(value & 0xFF));
    hal_gpio_clear(PIN_HPI_HWRN);
    hpi_delay();
    hal_gpio_set(PIN_HPI_HWRN);
    hpi_delay();

    hal_gpio_set(PIN_HPI_HCSN);       /* CS HIGH */
    return 0;
}

/**
 * @brief HPI register'dan 16-bit değer oku
 */
int sct3258_hpi_read(uint16_t addr)
{
    if (!s_initialized) return -1;

    hpi_wait_ready();
    hpi_data_bus_output();

    hal_gpio_clear(PIN_HPI_HCSN);

    /* Adres baytları yaz */
    hpi_data_bus_write((uint8_t)(addr >> 8));
    hal_gpio_clear(PIN_HPI_HWRN);
    hpi_delay();
    hal_gpio_set(PIN_HPI_HWRN);
    hpi_delay();

    hpi_data_bus_write((uint8_t)(addr & 0xFF));
    hal_gpio_clear(PIN_HPI_HWRN);
    hpi_delay();
    hal_gpio_set(PIN_HPI_HWRN);
    hpi_delay();

    /* Veri baytları oku */
    hpi_data_bus_input();

    hal_gpio_clear(PIN_HPI_HRDN);
    hpi_delay();
    uint8_t hi = hpi_data_bus_read();
    hal_gpio_set(PIN_HPI_HRDN);
    hpi_delay();

    hal_gpio_clear(PIN_HPI_HRDN);
    hpi_delay();
    uint8_t lo = hpi_data_bus_read();
    hal_gpio_set(PIN_HPI_HRDN);
    hpi_delay();

    hal_gpio_set(PIN_HPI_HCSN);
    return (int)((uint16_t)(hi << 8) | lo);
}

/* ============================================================
 * API İmplementasyonu
 * ============================================================ */

int sct3258_init(void)
{
    memset(&s_status, 0, sizeof(s_status));

    /* Güç ve kontrol pinlerini yapılandır */
    hal_gpio_configure(PIN_HPI_EN1, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);
    hal_gpio_configure(PIN_HPI_EN2, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);
    hal_gpio_configure(PIN_HPI_STRAP, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);
    hal_gpio_configure(PIN_HPI_RSTN, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);
    hal_gpio_configure(PIN_HPI_HCSN, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);
    hal_gpio_configure(PIN_HPI_HWRN, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);
    hal_gpio_configure(PIN_HPI_HRDN, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);
    hal_gpio_configure(PIN_HPI_HOBIB, GPIO_MODE_INPUT_PULLUP, GPIO_SPEED_50MHZ);
    hal_gpio_configure(PIN_HPI_SLOT, GPIO_MODE_INPUT_FLOATING, GPIO_SPEED_50MHZ);

    /* Başlangıç durumları */
    hal_gpio_set(PIN_HPI_HCSN);
    hal_gpio_set(PIN_HPI_HWRN);
    hal_gpio_set(PIN_HPI_HRDN);

    /* RadioFoundry Doğrulanmış Reset Sırası:
     * 1. PA8 ve PC6 HIGH (SCT3258 Güç Besleme)
     * 2. PC12 (PIO1 strap) LOW (Intel HPI Mode)
     * 3. RSTN 100ms HIGH, 500ms LOW, ardından HIGH
     * 4. PC12 pull-down input olarak serbest bırakılır
     */
    hal_gpio_set(PIN_HPI_EN1);
    hal_gpio_set(PIN_HPI_EN2);
    hal_gpio_clear(PIN_HPI_STRAP); /* Intel mode */

    hal_gpio_set(PIN_HPI_RSTN);
    hal_delay_ms(100);
    hal_gpio_clear(PIN_HPI_RSTN);
    hal_delay_ms(500);
    hal_gpio_set(PIN_HPI_RSTN);
    hal_delay_ms(50);

    hal_gpio_configure(PIN_HPI_STRAP, GPIO_MODE_INPUT_PULLDOWN, GPIO_SPEED_50MHZ);

    s_initialized = true;

    /* 1024 16-bit word bootloader bloğunu HPI üzerinden yükle */
    hpi_data_bus_output();
    hal_gpio_clear(PIN_HPI_HCSN);
    for (size_t i = 0; i < sizeof(SCT3258_BOOT_BLOB); i += 2) {
        hpi_data_bus_write(SCT3258_BOOT_BLOB[i]);
        hal_gpio_clear(PIN_HPI_HWRN);
        hpi_delay();
        hal_gpio_set(PIN_HPI_HWRN);
        hpi_delay();

        hpi_data_bus_write(SCT3258_BOOT_BLOB[i + 1]);
        hal_gpio_clear(PIN_HPI_HWRN);
        hpi_delay();
        hal_gpio_set(PIN_HPI_HWRN);
        hpi_delay();
    }
    hal_gpio_set(PIN_HPI_HCSN);
    s_boot_loaded = true;

    hal_delay_ms(50);

    /* Varsayılan yapılandırma */
    sct3258_hpi_write(HPI_REG_INT_ENABLE, 0x003F);
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
