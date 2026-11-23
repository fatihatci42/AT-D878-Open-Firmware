/**
 * @file    hal_adc.c
 * @brief   ADC & Battery Voltage Measurement Implementation
 * @version 0.1.0
 */

#include "hal_adc.h"
#include "hal_systick.h"

/* GD32F303 ADC0 Register Base */
#define ADC0_BASE           0x40012400U
#define ADC_STAT            (*(volatile uint32_t *)(ADC0_BASE + 0x00U))
#define ADC_CTL0            (*(volatile uint32_t *)(ADC0_BASE + 0x04U))
#define ADC_CTL1            (*(volatile uint32_t *)(ADC0_BASE + 0x08U))
#define ADC_SAMPT0          (*(volatile uint32_t *)(ADC0_BASE + 0x0CU))
#define ADC_SAMPT1          (*(volatile uint32_t *)(ADC0_BASE + 0x10U))
#define ADC_RSQ0            (*(volatile uint32_t *)(ADC0_BASE + 0x2CU))
#define ADC_RSQ2            (*(volatile uint32_t *)(ADC0_BASE + 0x34U))
#define ADC_RDATA           (*(volatile uint32_t *)(ADC0_BASE + 0x4CU))

/* CTL1 bitleri */
#define ADC_CTL1_ADCON      (1U << 0)
#define ADC_CTL1_CLB        (1U << 2)
#define ADC_CTL1_RSTCLB     (1U << 3)
#define ADC_CTL1_SWRCST     (1U << 22)

#define ADC_STAT_EOC        (1U << 1)

/* Gerilim bölücü katsayısı: R1=100k, R2=33k -> Çarpan ~ 4.03 */
/* V_bat = (ADC / 4095) * 3300 mV * (133 / 33) */
#define DIVIDER_NUMERATOR   133
#define DIVIDER_DENOMINATOR 33

static bool s_adc_initialized = false;

int hal_adc_init(void)
{
    /* 1. ADC aç */
    ADC_CTL1 |= ADC_CTL1_ADCON;
    hal_delay_us(50);

    /* 2. Kalibrasyon sıfırla ve başlat */
    ADC_CTL1 |= ADC_CTL1_RSTCLB;
    while (ADC_CTL1 & ADC_CTL1_RSTCLB) {}

    ADC_CTL1 |= ADC_CTL1_CLB;
    while (ADC_CTL1 & ADC_CTL1_CLB) {}

    s_adc_initialized = true;
    return 0;
}

uint16_t hal_adc_read_channel(uint8_t channel)
{
    if (!s_adc_initialized) return 0;

    /* Tek kanal dönüşümü yapılandır */
    ADC_RSQ0 = 0;              /* 1 dönüşüm */
    ADC_RSQ2 = channel & 0x1F; /* Kanal seç */

    /* Dönüşümü başlat */
    ADC_CTL1 |= ADC_CTL1_SWRCST;

    /* Tamamlanmasını bekle */
    uint32_t timeout = 10000;
    while (!(ADC_STAT & ADC_STAT_EOC)) {
        if (--timeout == 0) return 0;
    }

    return (uint16_t)(ADC_RDATA & 0x0FFFU);
}

uint16_t hal_adc_read_battery_mv(void)
{
    /* Kanal 10 (PC0 batarya gerilim bölücü) oku */
    uint16_t raw = hal_adc_read_channel(10);
    if (raw == 0) {
        /* Simülasyonda veya sensör yoksa varsayılan 7.6V döndür */
        return 7600;
    }

    /* V_pin_mv = (raw * 3300) / 4095 */
    uint32_t v_pin_mv = ((uint32_t)raw * 3300U) / 4095U;
    uint32_t v_bat_mv = (v_pin_mv * DIVIDER_NUMERATOR) / DIVIDER_DENOMINATOR;

    return (uint16_t)v_bat_mv;
}

uint8_t hal_adc_get_battery_bars(void)
{
    uint16_t mv = hal_adc_read_battery_mv();
    if (mv >= BATTERY_VOLTAGE_3BAR_MV) return 3; /* > 7.46V: 3 blok */
    if (mv >= BATTERY_VOLTAGE_2BAR_MV) return 2; /* 6.85V - 7.46V: 2 blok */
    if (mv >= BATTERY_VOLTAGE_1BAR_MV) return 1; /* 6.43V - 6.85V: 1 blok */
    return 0;                                    /* <= 6.43V: Şarj uyarısı */
}

uint8_t hal_adc_get_battery_percentage(void)
{
    uint16_t mv = hal_adc_read_battery_mv();
    if (mv >= BATTERY_VOLTAGE_FULL_MV) return 100;
    if (mv <= BATTERY_VOLTAGE_CUTOFF_MV) return 0;

    /* Fluke tablosuna göre kademeli kapasite hesaplaması */
    if (mv >= BATTERY_VOLTAGE_3BAR_MV) {
        /* 7460 - 8400 mV -> %50 - %100 */
        return 50 + (uint8_t)(((uint32_t)(mv - BATTERY_VOLTAGE_3BAR_MV) * 50U) / (BATTERY_VOLTAGE_FULL_MV - BATTERY_VOLTAGE_3BAR_MV));
    } else if (mv >= BATTERY_VOLTAGE_2BAR_MV) {
        /* 6850 - 7460 mV -> %3 - %50 */
        return 3 + (uint8_t)(((uint32_t)(mv - BATTERY_VOLTAGE_2BAR_MV) * 47U) / (BATTERY_VOLTAGE_3BAR_MV - BATTERY_VOLTAGE_2BAR_MV));
    } else if (mv >= BATTERY_VOLTAGE_1BAR_MV) {
        /* 6430 - 6850 mV -> %1 - %3 */
        return 1 + (uint8_t)(((uint32_t)(mv - BATTERY_VOLTAGE_1BAR_MV) * 2U) / (BATTERY_VOLTAGE_2BAR_MV - BATTERY_VOLTAGE_1BAR_MV));
    } else {
        return 0;
    }
}

bool hal_adc_is_low_battery(void)
{
    return hal_adc_read_battery_mv() < BATTERY_VOLTAGE_LOW_MV;
}
