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

/* ============================================================
 * Batarya Gerilim Eşikleri (Fluke Multimetre Kalibrasyon Verileri)
 * Kaynak: Jason Reilly VK7ZJA (878techmods.html - #BatteryMeter)
 * ============================================================ */
#define BATTERY_VOLTAGE_FULL_MV    8400  /**< Tam dolu şarj cihazı kesim (8.40V) */
#define BATTERY_VOLTAGE_3BAR_MV    7460  /**< 3 blok eşiği (> 7.46V: %50-%100) */
#define BATTERY_VOLTAGE_2BAR_MV    6850  /**< 2 blok eşiği (6.85V - 7.46V: %3-%50) */
#define BATTERY_VOLTAGE_1BAR_MV    6430  /**< 1 blok eşiği (6.43V - 6.85V: %1-%3) */
#define BATTERY_VOLTAGE_RECHARGE_MV 6000 /**< Şimşek / Şarj ikonu (6.00V - 6.43V: kritik boş-%1) */
#define BATTERY_VOLTAGE_CRITICAL_MV 6000 /**< Aşırı deşarj uyarısı (< 6.00V) */
#define BATTERY_VOLTAGE_CUTOFF_MV  5920  /**< Otomatik koruma kapanma gerilimi (5.92V) */

#define BATTERY_VOLTAGE_NOMINAL_MV 7400  /**< Nominal çalışma (7.4V) */
#define BATTERY_VOLTAGE_LOW_MV     BATTERY_VOLTAGE_2BAR_MV
#define BATTERY_VOLTAGE_EMPTY_MV   BATTERY_VOLTAGE_CUTOFF_MV

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
 * @brief Kalibre edilmiş batarya blok sayısını döndür (0..3)
 * @return 3: >7.46V, 2: 6.85-7.46V, 1: 6.43-6.85V, 0: <=6.43V
 */
uint8_t hal_adc_get_battery_bars(void);

/**
 * @brief Düşük pil uyarısı var mı?
 */
bool hal_adc_is_low_battery(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_ADC_H */
