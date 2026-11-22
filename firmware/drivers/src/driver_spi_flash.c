/**
 * @file    driver_spi_flash.c
 * @brief   External SPI Flash Driver — AT-D878 Open Firmware
 * @version 0.1.0
 *
 * Harici SPI NOR Flash sürücüsü implementasyonu.
 * SPI2 (PB3=SCK, PB4=MISO, PB5=MOSI) + PD8=CS üzerinden çalışır.
 *
 * Pin atamaları tahminidir ve doğrulanacaktır.
 */

#include "driver_spi_flash.h"
#include "hal_spi.h"
#include "hal_gpio.h"
#include "hal_systick.h"
#include <string.h>
#include <stdio.h>

/* ============================================================
 * Pin ve SPI Yapılandırması
 * ============================================================ */

/** Harici flash SPI bus'ı */
#define FLASH_SPI_BUS         HAL_SPI_2

/** Chip Select pini (Active LOW) */
#define PIN_FLASH_CS          ((hal_gpio_id_t){GPIO_PORT_D, 8})

/* CS kontrol makroları */
#define FLASH_CS_LOW()        hal_gpio_clear(PIN_FLASH_CS)
#define FLASH_CS_HIGH()       hal_gpio_set(PIN_FLASH_CS)

/* ============================================================
 * Statik Değişkenler
 * ============================================================ */

static spi_flash_info_t s_flash_info;
static bool s_initialized = false;

/* ============================================================
 * Düşük Seviye SPI İşlemleri
 * ============================================================ */

static void flash_send_cmd(uint8_t cmd)
{
    hal_spi_transmit(FLASH_SPI_BUS, &cmd, 1);
}

static void flash_send_addr(uint32_t addr)
{
    uint8_t buf[3];
    if (s_flash_info.addr_4byte) {
        uint8_t buf4[4] = {
            (uint8_t)(addr >> 24),
            (uint8_t)(addr >> 16),
            (uint8_t)(addr >> 8),
            (uint8_t)(addr)
        };
        hal_spi_transmit(FLASH_SPI_BUS, buf4, 4);
        return;
    }
    buf[0] = (uint8_t)(addr >> 16);
    buf[1] = (uint8_t)(addr >> 8);
    buf[2] = (uint8_t)(addr);
    hal_spi_transmit(FLASH_SPI_BUS, buf, 3);
}

static uint8_t flash_read_status1(void)
{
    uint8_t cmd = SPIFLASH_CMD_READ_STATUS1;
    uint8_t status;
    FLASH_CS_LOW();
    hal_spi_transmit(FLASH_SPI_BUS, &cmd, 1);
    hal_spi_receive(FLASH_SPI_BUS, &status, 1);
    FLASH_CS_HIGH();
    return status;
}

static void flash_write_enable(void)
{
    uint8_t cmd = SPIFLASH_CMD_WRITE_ENABLE;
    FLASH_CS_LOW();
    hal_spi_transmit(FLASH_SPI_BUS, &cmd, 1);
    FLASH_CS_HIGH();
}

/* ============================================================
 * Çip Tanılama
 * ============================================================ */

/** Bilinen flash çip tablosu */
typedef struct {
    uint8_t mfr;
    uint8_t type;
    uint8_t cap;
    const char *name;
    uint32_t size;
} flash_chip_entry_t;

static const flash_chip_entry_t known_chips[] = {
    /* Winbond */
    { 0xEF, 0x40, 0x19, "W25Q256JV",  0x02000000 },  /* 32 MB */
    { 0xEF, 0x40, 0x18, "W25Q128JV",  0x01000000 },  /* 16 MB */
    { 0xEF, 0x40, 0x17, "W25Q64JV",   0x00800000 },  /* 8 MB */
    { 0xEF, 0x40, 0x16, "W25Q32JV",   0x00400000 },  /* 4 MB */
    /* GigaDevice */
    { 0xC8, 0x40, 0x19, "GD25Q256D",  0x02000000 },
    { 0xC8, 0x40, 0x18, "GD25Q128E",  0x01000000 },
    { 0xC8, 0x40, 0x17, "GD25Q64E",   0x00800000 },
    /* Macronix */
    { 0xC2, 0x20, 0x19, "MX25L25645G",0x02000000 },
    { 0xC2, 0x20, 0x18, "MX25L12835F",0x01000000 },
    /* Sentinel */
    { 0x00, 0x00, 0x00, NULL,         0 },
};

static int flash_identify(void)
{
    uint8_t cmd = SPIFLASH_CMD_READ_ID;
    uint8_t id[3];

    FLASH_CS_LOW();
    hal_spi_transmit(FLASH_SPI_BUS, &cmd, 1);
    hal_spi_receive(FLASH_SPI_BUS, id, 3);
    FLASH_CS_HIGH();

    s_flash_info.manufacturer_id = id[0];
    s_flash_info.memory_type     = id[1];
    s_flash_info.capacity_id     = id[2];
    s_flash_info.page_size       = 256;
    s_flash_info.sector_size     = 4096;
    s_flash_info.block_size      = 65536;

    /* Bilinen çip tablosundan eşle */
    for (int i = 0; known_chips[i].name != NULL; i++) {
        if (known_chips[i].mfr  == id[0] &&
            known_chips[i].type == id[1] &&
            known_chips[i].cap  == id[2]) {
            s_flash_info.total_size = known_chips[i].size;
            s_flash_info.addr_4byte = (known_chips[i].size > 0x01000000);
            strncpy(s_flash_info.name, known_chips[i].name,
                    sizeof(s_flash_info.name) - 1);
            return 0;
        }
    }

    /* Bilinmeyen çip — kapasite ID'den tahmin et */
    if (id[0] != 0x00 && id[0] != 0xFF) {
        s_flash_info.total_size = (1U << id[2]);
        s_flash_info.addr_4byte = (s_flash_info.total_size > 0x01000000);
        snprintf(s_flash_info.name, sizeof(s_flash_info.name),
                 "Unknown(%02X-%02X-%02X)", id[0], id[1], id[2]);
        return 0;  /* Yine de çalışmayı dene */
    }

    return -2;  /* Çip yanıt yok */
}

/* ============================================================
 * API İmplementasyonu
 * ============================================================ */

int spi_flash_init(void)
{
    /* CS pinini yapılandır (çıkış, HIGH = deselect) */
    hal_gpio_configure(PIN_FLASH_CS, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);
    FLASH_CS_HIGH();

    /* SPI bus'ı yapılandır */
    hal_spi_config_t spi_cfg = {
        .mode     = HAL_SPI_MODE_0,
        .datasize = HAL_SPI_DATASIZE_8BIT,
        .bitorder = HAL_SPI_MSB_FIRST,
        .clock_hz = 20000000,  /* 20 MHz */
        .use_dma  = false,
    };
    hal_spi_init(FLASH_SPI_BUS, &spi_cfg);

    /* Power-Down'dan çık */
    uint8_t cmd = SPIFLASH_CMD_RELEASE_PD;
    FLASH_CS_LOW();
    hal_spi_transmit(FLASH_SPI_BUS, &cmd, 1);
    FLASH_CS_HIGH();
    hal_delay_us(50);

    /* Çip tanıla */
    memset(&s_flash_info, 0, sizeof(s_flash_info));
    int rc = flash_identify();
    if (rc != 0) return rc;

    /* 4-byte adresleme moduna gir (>16MB çipler) */
    if (s_flash_info.addr_4byte) {
        cmd = SPIFLASH_CMD_EN4B_ADDR;
        FLASH_CS_LOW();
        hal_spi_transmit(FLASH_SPI_BUS, &cmd, 1);
        FLASH_CS_HIGH();
    }

    s_initialized = true;
    return 0;
}

void spi_flash_deinit(void)
{
    if (s_initialized) {
        /* Power-Down moduna gir */
        uint8_t cmd = SPIFLASH_CMD_POWER_DOWN;
        FLASH_CS_LOW();
        hal_spi_transmit(FLASH_SPI_BUS, &cmd, 1);
        FLASH_CS_HIGH();
        s_initialized = false;
    }
}

int spi_flash_get_info(spi_flash_info_t *info)
{
    if (!info || !s_initialized) return -1;
    memcpy(info, &s_flash_info, sizeof(spi_flash_info_t));
    return 0;
}

int spi_flash_read(uint32_t addr, uint8_t *data, size_t len)
{
    if (!s_initialized || !data || len == 0) return -1;

    FLASH_CS_LOW();
    flash_send_cmd(SPIFLASH_CMD_FAST_READ);
    flash_send_addr(addr);
    /* Dummy byte (Fast Read gerekliliği) */
    uint8_t dummy = 0;
    hal_spi_transmit(FLASH_SPI_BUS, &dummy, 1);
    /* Veriyi oku */
    hal_spi_receive(FLASH_SPI_BUS, data, len);
    FLASH_CS_HIGH();

    return (int)len;
}

int spi_flash_write(uint32_t addr, const uint8_t *data, size_t len)
{
    if (!s_initialized || !data || len == 0) return -1;

    size_t written = 0;

    while (written < len) {
        /* Sayfa sınırına kadar yazılabilecek byte sayısı */
        size_t page_offset = addr % s_flash_info.page_size;
        size_t page_remain = s_flash_info.page_size - page_offset;
        size_t chunk = (len - written);
        if (chunk > page_remain) chunk = page_remain;

        /* Write Enable */
        flash_write_enable();

        /* Page Program */
        FLASH_CS_LOW();
        flash_send_cmd(SPIFLASH_CMD_PAGE_PROGRAM);
        flash_send_addr(addr);
        hal_spi_transmit(FLASH_SPI_BUS, data + written, chunk);
        FLASH_CS_HIGH();

        /* Yazımın tamamlanmasını bekle */
        if (spi_flash_wait_ready(100) != 0) return -2;

        written += chunk;
        addr    += chunk;
    }

    return (int)written;
}

int spi_flash_erase_sector(uint32_t addr)
{
    if (!s_initialized) return -1;

    flash_write_enable();

    FLASH_CS_LOW();
    flash_send_cmd(SPIFLASH_CMD_SECTOR_ERASE);
    flash_send_addr(addr & ~(s_flash_info.sector_size - 1));
    FLASH_CS_HIGH();

    return spi_flash_wait_ready(500);
}

int spi_flash_erase_block(uint32_t addr)
{
    if (!s_initialized) return -1;

    flash_write_enable();

    FLASH_CS_LOW();
    flash_send_cmd(SPIFLASH_CMD_BLOCK_ERASE_64K);
    flash_send_addr(addr & ~(s_flash_info.block_size - 1));
    FLASH_CS_HIGH();

    return spi_flash_wait_ready(2000);
}

int spi_flash_erase_chip(void)
{
    if (!s_initialized) return -1;

    flash_write_enable();

    FLASH_CS_LOW();
    flash_send_cmd(SPIFLASH_CMD_CHIP_ERASE);
    FLASH_CS_HIGH();

    /* Chip erase 60-200 saniye sürebilir */
    return spi_flash_wait_ready(300000);
}

bool spi_flash_is_busy(void)
{
    return (flash_read_status1() & SPIFLASH_SR1_BUSY) != 0;
}

int spi_flash_wait_ready(uint32_t timeout_ms)
{
    tick_t start = hal_tick_get();
    while (spi_flash_is_busy()) {
        if (hal_tick_expired(start, timeout_ms)) return -1;
    }
    return 0;
}
