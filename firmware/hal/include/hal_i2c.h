/**
 * @file    hal_i2c.h
 * @brief   I2C Hardware Abstraction Layer - AT-D878 Open Firmware
 * @version 0.0.1
 *
 * GD32F303 I2C donanım soyutlama katmanı.
 * Audio codec (TLV320AIC3204) ve AT1846S (I2C modunda) bu arayüzü kullanır.
 */

#ifndef HAL_I2C_H
#define HAL_I2C_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief I2C kanal numaralandırması
 * GD32F303VGT6: I2C0, I2C1 (2 adet)
 */
typedef enum {
    HAL_I2C_0 = 0,  /**< I2C0 */
    HAL_I2C_1,      /**< I2C1 */
    HAL_I2C_COUNT
} hal_i2c_bus_t;

/**
 * @brief I2C hız modu
 */
typedef enum {
    HAL_I2C_SPEED_STANDARD = 100000,  /**< 100 kHz Standard mode */
    HAL_I2C_SPEED_FAST     = 400000,  /**< 400 kHz Fast mode */
} hal_i2c_speed_t;

/**
 * @brief I2C yapılandırma yapısı
 */
typedef struct {
    hal_i2c_speed_t speed;       /**< I2C hızı */
    bool            use_dma;     /**< DMA kullanımı */
} hal_i2c_config_t;

/**
 * @brief I2C alt sistemini başlat
 * @param bus    I2C kanal numarası
 * @param config Yapılandırma
 * @return 0 başarılı, negatif hata kodu
 */
int hal_i2c_init(hal_i2c_bus_t bus, const hal_i2c_config_t *config);

/**
 * @brief I2C kanalını kapat
 * @param bus I2C kanal numarası
 */
void hal_i2c_deinit(hal_i2c_bus_t bus);

/**
 * @brief I2C register'a yaz
 * @param bus      I2C kanal numarası
 * @param dev_addr 7-bit cihaz adresi
 * @param reg_addr Register adresi
 * @param data     Yazılacak veri
 * @param len      Veri uzunluğu (byte)
 * @return 0 başarılı, negatif hata kodu
 */
int hal_i2c_write_reg(hal_i2c_bus_t bus, uint8_t dev_addr,
                      uint8_t reg_addr, const uint8_t *data, size_t len);

/**
 * @brief I2C register'dan oku
 * @param bus      I2C kanal numarası
 * @param dev_addr 7-bit cihaz adresi
 * @param reg_addr Register adresi
 * @param data     Okunan veri tamponu
 * @param len      Okunacak byte sayısı
 * @return 0 başarılı, negatif hata kodu
 */
int hal_i2c_read_reg(hal_i2c_bus_t bus, uint8_t dev_addr,
                     uint8_t reg_addr, uint8_t *data, size_t len);

/**
 * @brief I2C tek byte register yaz
 * @param bus      I2C kanal numarası
 * @param dev_addr 7-bit cihaz adresi
 * @param reg_addr Register adresi
 * @param value    Yazılacak değer
 * @return 0 başarılı, negatif hata kodu
 */
int hal_i2c_write_reg8(hal_i2c_bus_t bus, uint8_t dev_addr,
                       uint8_t reg_addr, uint8_t value);

/**
 * @brief I2C tek byte register oku
 * @param bus      I2C kanal numarası
 * @param dev_addr 7-bit cihaz adresi
 * @param reg_addr Register adresi
 * @return Okunan değer (negatif hata kodu)
 */
int hal_i2c_read_reg8(hal_i2c_bus_t bus, uint8_t dev_addr,
                      uint8_t reg_addr);

/**
 * @brief I2C 16-bit register yaz (big-endian)
 * @param bus      I2C kanal numarası
 * @param dev_addr 7-bit cihaz adresi
 * @param reg_addr Register adresi
 * @param value    Yazılacak 16-bit değer
 * @return 0 başarılı, negatif hata kodu
 */
int hal_i2c_write_reg16(hal_i2c_bus_t bus, uint8_t dev_addr,
                        uint8_t reg_addr, uint16_t value);

/**
 * @brief I2C 16-bit register oku (big-endian)
 * @param bus      I2C kanal numarası
 * @param dev_addr 7-bit cihaz adresi
 * @param reg_addr Register adresi
 * @return Okunan 16-bit değer (negatif hata kodu)
 */
int hal_i2c_read_reg16(hal_i2c_bus_t bus, uint8_t dev_addr,
                       uint8_t reg_addr);

/**
 * @brief I2C adresinde cihaz var mı kontrol et
 * @param bus      I2C kanal numarası
 * @param dev_addr 7-bit cihaz adresi
 * @return true cihaz bulundu, false bulunamadı
 */
bool hal_i2c_probe(hal_i2c_bus_t bus, uint8_t dev_addr);

/**
 * @brief I2C bus üzerindeki tüm cihazları tara
 * @param bus       I2C kanal numarası
 * @param addrs     Bulunan adresler dizisi
 * @param max_count Dizi boyutu
 * @return Bulunan cihaz sayısı
 */
int hal_i2c_scan(hal_i2c_bus_t bus, uint8_t *addrs, size_t max_count);

#ifdef __cplusplus
}
#endif

#endif /* HAL_I2C_H */
