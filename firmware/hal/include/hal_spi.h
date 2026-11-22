/**
 * @file    hal_spi.h
 * @brief   SPI Hardware Abstraction Layer - AT-D878 Open Firmware
 * @version 0.0.1
 *
 * GD32F303 SPI donanım soyutlama katmanı.
 * AT1846S, LCD ve NAND Flash gibi SPI bileşenler bu arayüzü kullanır.
 */

#ifndef HAL_SPI_H
#define HAL_SPI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SPI kanal numaralandırması
 * GD32F303VGT6: SPI0, SPI1, SPI2 (3 adet)
 */
typedef enum {
    HAL_SPI_0 = 0,  /**< SPI0 (APB2) */
    HAL_SPI_1,      /**< SPI1 (APB1) */
    HAL_SPI_2,      /**< SPI2 (APB1) */
    HAL_SPI_COUNT
} hal_spi_bus_t;

/**
 * @brief SPI çalışma modu
 */
typedef enum {
    HAL_SPI_MODE_0 = 0,  /**< CPOL=0, CPHA=0 */
    HAL_SPI_MODE_1,      /**< CPOL=0, CPHA=1 */
    HAL_SPI_MODE_2,      /**< CPOL=1, CPHA=0 */
    HAL_SPI_MODE_3,      /**< CPOL=1, CPHA=1 */
} hal_spi_mode_t;

/**
 * @brief SPI veri genişliği
 */
typedef enum {
    HAL_SPI_DATASIZE_8BIT  = 0,
    HAL_SPI_DATASIZE_16BIT = 1,
} hal_spi_datasize_t;

/**
 * @brief SPI bit sırası
 */
typedef enum {
    HAL_SPI_MSB_FIRST = 0,
    HAL_SPI_LSB_FIRST = 1,
} hal_spi_bitorder_t;

/**
 * @brief SPI yapılandırma yapısı
 */
typedef struct {
    hal_spi_mode_t     mode;       /**< SPI modu (0-3) */
    hal_spi_datasize_t datasize;   /**< Veri genişliği */
    hal_spi_bitorder_t bitorder;   /**< Bit sırası */
    uint32_t           clock_hz;   /**< Saat frekansı (Hz) */
    bool               use_dma;    /**< DMA kullanımı */
} hal_spi_config_t;

/**
 * @brief SPI alt sistemini başlat
 * @param bus    SPI kanal numarası
 * @param config Yapılandırma
 * @return 0 başarılı, negatif hata kodu
 */
int hal_spi_init(hal_spi_bus_t bus, const hal_spi_config_t *config);

/**
 * @brief SPI kanalını kapat
 * @param bus SPI kanal numarası
 */
void hal_spi_deinit(hal_spi_bus_t bus);

/**
 * @brief SPI veri gönder
 * @param bus  SPI kanal numarası
 * @param data Gönderilecek veri tamponu
 * @param len  Veri uzunluğu (byte)
 * @return Gönderilen byte sayısı, negatif hata kodu
 */
int hal_spi_transmit(hal_spi_bus_t bus, const uint8_t *data, size_t len);

/**
 * @brief SPI veri al
 * @param bus  SPI kanal numarası
 * @param data Alınan veri tamponu
 * @param len  Okunacak byte sayısı
 * @return Okunan byte sayısı, negatif hata kodu
 */
int hal_spi_receive(hal_spi_bus_t bus, uint8_t *data, size_t len);

/**
 * @brief SPI eşzamanlı gönder ve al (full-duplex)
 * @param bus    SPI kanal numarası
 * @param tx     Gönderilecek veri
 * @param rx     Alınan veri tamponu
 * @param len    Veri uzunluğu (byte)
 * @return Transfer edilen byte sayısı, negatif hata kodu
 */
int hal_spi_transfer(hal_spi_bus_t bus, const uint8_t *tx,
                     uint8_t *rx, size_t len);

/**
 * @brief Tek byte gönder ve al
 * @param bus  SPI kanal numarası
 * @param byte Gönderilecek byte
 * @return Alınan byte
 */
uint8_t hal_spi_transfer_byte(hal_spi_bus_t bus, uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif /* HAL_SPI_H */
