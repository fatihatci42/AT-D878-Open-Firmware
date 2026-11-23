/**
 * @file    calibration.c
 * @brief   Factory RF Calibration & Power DAC Implementation
 * @version 0.1.0
 */

#include "calibration.h"
#include "driver_spi_flash.h"
#include "hal_gpio.h"
#include <string.h>

/* GD32F303 DAC0 Register Tanımları */
#define RCU_APB1EN_REG   (*(volatile uint32_t *)0x4002101CU)
#define RCU_APB1EN_DACEN (1U << 29)

#define DAC_CTL_REG      (*(volatile uint32_t *)0x40007400U)
#define DAC_CTL_DEN0     (1U << 0)
#define DAC0_R12DH_REG   (*(volatile uint32_t *)0x40007408U)

static calibration_data_t s_cal;

int calibration_init(void)
{
    memset(&s_cal, 0, sizeof(s_cal));

    /* DAC0 (PA4) donanım saatini etkinleştir */
    RCU_APB1EN_REG |= RCU_APB1EN_DACEN;

    /* PA4 analog modda yapılandır */
    hal_gpio_configure(PIN_TX_POWER_DAC, GPIO_MODE_INPUT_ANALOG, GPIO_SPEED_50MHZ);

    /* DAC0 kanalını etkinleştir */
    DAC_CTL_REG |= DAC_CTL_DEN0;
    DAC0_R12DH_REG = 0; /* Başlangıçta 0V / PA kapalı */

    /* SPI NAND'dan birincil kalibrasyonu oku */
    uint8_t buffer[CAL_BLOCK_SIZE];
    int rc = spi_flash_read(CAL_NAND_ADDR_PRIMARY, buffer, sizeof(buffer));

    bool valid = false;
    if (rc == 0) {
        /* Basit boş flash kontrolü (tümü 0xFF değilse) */
        for (size_t i = 0; i < sizeof(buffer); i++) {
            if (buffer[i] != 0xFF && buffer[i] != 0x00) {
                valid = true;
                break;
            }
        }
    }

    if (!valid) {
        /* İkincil yedek bloktan dene */
        rc = spi_flash_read(CAL_NAND_ADDR_BACKUP, buffer, sizeof(buffer));
        if (rc == 0) {
            for (size_t i = 0; i < sizeof(buffer); i++) {
                if (buffer[i] != 0xFF && buffer[i] != 0x00) {
                    valid = true;
                    break;
                }
            }
        }
    }

    if (valid) {
        /* VHF Tabloları (39 byte x 4 kademe = 156 byte) */
        memcpy(s_cal.vhf_power_table, &buffer[0], sizeof(s_cal.vhf_power_table));
        /* UHF Tabloları (81 byte x 4 kademe = 324 byte) */
        memcpy(s_cal.uhf_power_table, &buffer[156], sizeof(s_cal.uhf_power_table));
        s_cal.valid = true;
        return 0;
    }

    /* Kalibrasyon bloğu henüz okunmadıysa güvenli nominal değerler kullanılır */
    s_cal.valid = false;
    return -1;
}

uint16_t calibration_get_tx_dac(uint32_t freq_hz, tx_power_t power)
{
    uint32_t freq_mhz = freq_hz / 1000000;
    uint8_t pwr_idx = 0;

    switch (power) {
    case POWER_TURBO: pwr_idx = 0; break;
    case POWER_HIGH:  pwr_idx = 1; break;
    case POWER_MID:   pwr_idx = 2; break;
    case POWER_LOW:   pwr_idx = 3; break;
    default:          pwr_idx = 0; break;
    }

    /* Fabrika kalibrasyon tablosu geçerliyse */
    if (s_cal.valid) {
        if (freq_mhz >= CAL_VHF_START_MHZ && freq_mhz <= CAL_VHF_END_MHZ) {
            uint8_t step = (uint8_t)(freq_mhz - CAL_VHF_START_MHZ);
            uint8_t raw = s_cal.vhf_power_table[pwr_idx][step];
            /* 8-bit fabrika kalibrasyon değerini 12-bit DAC skalasına uyarla */
            return (uint16_t)(((uint32_t)raw * 4095) / 255);
        } else if (freq_mhz >= CAL_UHF_START_MHZ && freq_mhz <= CAL_UHF_END_MHZ) {
            uint8_t step = (uint8_t)(freq_mhz - CAL_UHF_START_MHZ);
            uint8_t raw = s_cal.uhf_power_table[pwr_idx][step];
            return (uint16_t)(((uint32_t)raw * 4095) / 255);
        }
    }

    /* Nominal güvenli varsayılanlar (12-bit DAC: 0..4095) */
    switch (power) {
    case POWER_TURBO: return 3600; /* ~7 Watt */
    case POWER_HIGH:  return 2800; /* ~5 Watt */
    case POWER_MID:   return 1800; /* ~2.5 Watt */
    case POWER_LOW:   return 900;  /* ~1 Watt */
    default:          return 2800;
    }
}

int calibration_apply_tx_power(uint32_t freq_hz, tx_power_t power)
{
    uint16_t dac_val = calibration_get_tx_dac(freq_hz, power);
    DAC0_R12DH_REG = (uint32_t)(dac_val & 0x0FFF);
    return 0;
}
