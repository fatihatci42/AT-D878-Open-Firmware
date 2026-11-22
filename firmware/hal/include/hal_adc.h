/**
 * @file    hal_adc.h
 * @brief   ADC Hardware Abstraction & Battery Voltage Monitor - AT-D878 Open Firmware
 * @version 0.1.0
 */

#ifndef HAL_ADC_H
#define HAL_ADC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BATTERY_VOLTAGE_FULL_MV    8400  /**< Tam dolu (8.4V) */
#define BATTERY_VOLTAGE_NOMINAL_MV 7400  /**< Nominal (7.4V) */
#define BATTERY_VOLTAGE_LOW_MV     6800  /**< Düşük pil eşiği (6.8V) */
#define BATTERY_VOLTAGE_EMPTY_MV   6400  /**< Kritik boş eşiği (6.4V) */

/**
 * @brief GD32F303 dahili ADC0 alt sistemini başlat
 */
int hal_adc_init(void);

/**
 * @brief Ham ADC kanal değerini oku (12-bit: 0..4095)
 */
uint16_t hal_adc_read_channel(uint8_t channel);

/**
 * @brief Batarya gerilimini milivolt cinsinden oku
 * @return Gerilim (mV, örn: 7600 -> 7.60V)
 */
uint16_t hal_adc_read_battery_mv(void);

/**
 * @brief Batarya doluluk yüzdesini hesapla (0..100%)
 */
uint8_t hal_adc_get_battery_percentage(void);

/**
 * @brief Düşük pil uyarısı var mı?
 */
bool hal_adc_is_low_battery(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_ADC_H */
