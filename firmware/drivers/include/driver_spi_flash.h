/**
 * @file    driver_spi_flash.h
 * @brief   External SPI Flash Driver - AT-D878 Open Firmware
 * @version 0.1.0
 *
 * Harici SPI NOR/NAND Flash sürücüsü.
 * Codeplug (kanallar, kişiler, ayarlar) ve ikon verileri
 * bu flash'ta saklanır.
 *
 * Desteklenen çip aileleri:
 *   - Winbond W25Qxx (W25Q256, W25Q128)
 *   - GigaDevice GD25Qxx
 *   - Macronix MX25Lxx
 *
 * Bellek Haritası (tahmini, doğrulanacak):
 *   0x000000 - 0x0FFFFF  : Codeplug verileri (~1 MB)
 *   0x100000 - 0x1FFFFF  : DMR kişi listesi (~1 MB, 500K kişi)
 *   0x200000 - 0x3FFFFF  : Boot ekranları ve ikonlar (~2 MB)
 *   0x400000 - 0x7FFFFF  : Ses kayıtları / RX log (~4 MB)
 *   0x800000 - 0x1FFFFFF : Boş/genişleme (~24 MB)
 */

#ifndef DRIVER_SPI_FLASH_H
#define DRIVER_SPI_FLASH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * SPI Flash Komutları (Standart JEDEC)
 * ============================================================ */

#define SPIFLASH_CMD_READ             0x03  /* Normal Read */
#define SPIFLASH_CMD_FAST_READ        0x0B  /* Fast Read (dummy byte) */
#define SPIFLASH_CMD_READ_ID          0x9F  /* JEDEC ID */
#define SPIFLASH_CMD_READ_UID         0x4B  /* Unique ID */
#define SPIFLASH_CMD_WRITE_ENABLE     0x06  /* Write Enable */
#define SPIFLASH_CMD_WRITE_DISABLE    0x04  /* Write Disable */
#define SPIFLASH_CMD_PAGE_PROGRAM     0x02  /* Page Program (256 byte) */
#define SPIFLASH_CMD_SECTOR_ERASE     0x20  /* Sector Erase (4 KB) */
#define SPIFLASH_CMD_BLOCK_ERASE_32K  0x52  /* Block Erase (32 KB) */
#define SPIFLASH_CMD_BLOCK_ERASE_64K  0xD8  /* Block Erase (64 KB) */
#define SPIFLASH_CMD_CHIP_ERASE       0xC7  /* Chip Erase */
#define SPIFLASH_CMD_READ_STATUS1     0x05  /* Status Register 1 */
#define SPIFLASH_CMD_READ_STATUS2     0x35  /* Status Register 2 */
#define SPIFLASH_CMD_WRITE_STATUS     0x01  /* Write Status Register */
#define SPIFLASH_CMD_POWER_DOWN       0xB9  /* Power Down */
#define SPIFLASH_CMD_RELEASE_PD       0xAB  /* Release Power Down */
#define SPIFLASH_CMD_EN4B_ADDR        0xB7  /* Enter 4-byte address mode */
#define SPIFLASH_CMD_EX4B_ADDR        0xE9  /* Exit 4-byte address mode */

/* Status Register 1 bitleri */
#define SPIFLASH_SR1_BUSY             (1U << 0)
#define SPIFLASH_SR1_WEL              (1U << 1)

/* ============================================================
 * Veri Yapıları
 * ============================================================ */

/** Flash çip bilgisi */
typedef struct {
    uint8_t  manufacturer_id;    /**< Üretici ID (0xEF=Winbond, 0xC8=GD) */
    uint8_t  memory_type;        /**< Bellek tipi */
    uint8_t  capacity_id;        /**< Kapasite kodu */
    uint32_t total_size;         /**< Toplam boyut (byte) */
    uint32_t page_size;          /**< Sayfa boyutu (byte, genellikle 256) */
    uint32_t sector_size;        /**< Sektör boyutu (byte, genellikle 4096) */
    uint32_t block_size;         /**< Blok boyutu (byte, genellikle 65536) */
    bool     addr_4byte;         /**< 4-byte adresleme gerekli mi? (>16MB) */
    char     name[24];           /**< Çip adı */
} spi_flash_info_t;

/* ============================================================
 * API Fonksiyonları
 * ============================================================ */

/**
 * @brief SPI Flash sürücüsünü başlat ve çip tanıla
 * @return 0 başarılı, -1 iletişim hatası, -2 bilinmeyen çip
 */
int spi_flash_init(void);

/**
 * @brief SPI Flash sürücüsünü kapat
 */
void spi_flash_deinit(void);

/**
 * @brief Flash çip bilgisini al
 * @param info Çip bilgi yapısı (çıktı)
 * @return 0 başarılı, negatif hata
 */
int spi_flash_get_info(spi_flash_info_t *info);

/**
 * @brief Flash'tan veri oku
 * @param addr   Başlangıç adresi
 * @param data   Veri tamponu (çıktı)
 * @param len    Okunacak byte sayısı
 * @return Okunan byte sayısı, negatif hata
 */
int spi_flash_read(uint32_t addr, uint8_t *data, size_t len);

/**
 * @brief Flash'a veri yaz (sayfa sınırları otomatik yönetilir)
 * @param addr   Başlangıç adresi
 * @param data   Yazılacak veri
 * @param len    Veri uzunluğu (byte)
 * @return Yazılan byte sayısı, negatif hata
 *
 * NOT: Yazılacak alan önceden silinmiş olmalıdır!
 */
int spi_flash_write(uint32_t addr, const uint8_t *data, size_t len);

/**
 * @brief 4 KB sektör sil
 * @param addr Sektör başlangıç adresi (4K hizalı)
 * @return 0 başarılı, negatif hata
 */
int spi_flash_erase_sector(uint32_t addr);

/**
 * @brief 64 KB blok sil
 * @param addr Blok başlangıç adresi (64K hizalı)
 * @return 0 başarılı, negatif hata
 */
int spi_flash_erase_block(uint32_t addr);

/**
 * @brief Tüm flash'ı sil
 * @return 0 başarılı, negatif hata
 *
 * DİKKAT: Bu işlem tüm codeplug ve kullanıcı verilerini siler!
 * Yalnızca fabrika sıfırlama için kullanın.
 */
int spi_flash_erase_chip(void);

/**
 * @brief Flash meşgul mü?
 * @return true meşgul, false hazır
 */
bool spi_flash_is_busy(void);

/**
 * @brief Flash işleminin tamamlanmasını bekle
 * @param timeout_ms Zaman aşımı (ms)
 * @return 0 başarılı, -1 zaman aşımı
 */
int spi_flash_wait_ready(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_SPI_FLASH_H */
