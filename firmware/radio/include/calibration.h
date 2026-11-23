/**
 * @file    calibration.h
 * @brief   Factory RF Calibration & Power DAC Engine - AT-D878 Open Firmware
 * @version 0.1.0
 *
 * AnyTone D878 / D868 / D578 Fabrika Kalibrasyon Motoru.
 * Kaynak: RadioFoundry Hardware Specification (docs/spec/hardware-anytone.md)
 *
 * SPI NAND Bellek Haritası:
 * - Birincil Kalibrasyon Bloğu: 0x02F60000 (~1.5 KB)
 * - İkincil Yedek Blok:        0x02F80000
 *
 * Güç Kalibrasyon Tabloları (DAC0 / PA4):
 * - UHF: 400 - 480 MHz (81 frekans adımı / MHz başına 1 byte) x 4 Güç Seviyesi (Turbo, High, Mid, Low)
 * - VHF: 136 - 174 MHz (39 frekans adımı / MHz başına 1 byte) x 4 Güç Seviyesi
 */

#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "channel.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAL_NAND_ADDR_PRIMARY      0x02F60000U
#define CAL_NAND_ADDR_BACKUP       0x02F80000U
#define CAL_BLOCK_SIZE             1536

#define CAL_VHF_START_MHZ          136
#define CAL_VHF_END_MHZ            174
#define CAL_VHF_STEPS              39

#define CAL_UHF_START_MHZ          400
#define CAL_UHF_END_MHZ            480
#define CAL_UHF_STEPS              81

typedef struct {
    uint8_t vhf_power_table[4][CAL_VHF_STEPS]; /* Turbo, High, Mid, Low */
    uint8_t uhf_power_table[4][CAL_UHF_STEPS]; /* Turbo, High, Mid, Low */
    bool    valid;
} calibration_data_t;

/**
 * @brief Kalibrasyon alt sistemini başlat ve SPI NAND'dan fabrika verilerini yükle
 * @return 0 başarılı, negatif ise varsayılan nominal değerler devrede
 */
int calibration_init(void);

/**
 * @brief Belirli bir frekans ve güç kademesi için 12-bit DAC0 değerini hesapla
 * @param freq_hz  TX frekansı (Hz)
 * @param power    Güç kademesi (POWER_TURBO, POWER_HIGH, POWER_MID, POWER_LOW)
 * @return 12-bit DAC değeri (0..4095)
 */
uint16_t calibration_get_tx_dac(uint32_t freq_hz, tx_power_t power);

/**
 * @brief TX güç seviyesini doğrudan PA4 (DAC0) donanımına uygula
 */
int calibration_apply_tx_power(uint32_t freq_hz, tx_power_t power);

#ifdef __cplusplus
}
#endif

#endif /* CALIBRATION_H */
