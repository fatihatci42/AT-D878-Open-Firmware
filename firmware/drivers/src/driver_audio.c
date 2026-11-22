/**
 * @file    driver_audio.c
 * @brief   TLV320AIC3204 Audio Codec Driver Implementation - AT-D878 Open Firmware
 * @version 0.1.0
 *
 * Texas Instruments TLV320AIC3204 stereo ses codec tam donanım sürücüsü.
 * I2C0 (PB6=SCL, PB7=SDA) üzerinden 400 kHz hızında kontrol edilir.
 *
 * Ses yolları:
 *   - RX (Hoparlör): DAC L/R → LOL/LOR → Dahili ses amfisi (PA7 Mute ile kontrol edilir)
 *   - TX (Mikrofon): Dahili/Harici mikrofon → IN1_L/IN1_R → MicBias 2.5V → PGA → ADC
 */

#include "driver_audio.h"
#include "hal_i2c.h"
#include "hal_gpio.h"
#include "hal_systick.h"
#include <stdbool.h>

#define I2C_BUS_AUDIO     HAL_I2C_0

static bool s_initialized = false;

/* ============================================================
 * Yardımcı I2C Fonksiyonları
 * ============================================================ */

static inline int aic_write(uint8_t page, uint8_t reg, uint8_t val)
{
    /* Sayfa seç */
    int rc = hal_i2c_write_reg8(I2C_BUS_AUDIO, TLV320AIC3204_I2C_ADDR,
                                AIC3204_PAGE_SELECT_REG, page);
    if (rc != 0) return rc;
    /* Register yaz */
    return hal_i2c_write_reg8(I2C_BUS_AUDIO, TLV320AIC3204_I2C_ADDR, reg, val);
}

static inline uint8_t aic_read(uint8_t page, uint8_t reg)
{
    hal_i2c_write_reg8(I2C_BUS_AUDIO, TLV320AIC3204_I2C_ADDR, AIC3204_PAGE_SELECT_REG, page);
    return (uint8_t)hal_i2c_read_reg8(I2C_BUS_AUDIO, TLV320AIC3204_I2C_ADDR, reg);
}

/* ============================================================
 * API İmplementasyonu
 * ============================================================ */

int audio_codec_init(audio_sample_rate_t sample_rate)
{
    if (sample_rate != AUDIO_SAMPLE_RATE_8KHZ &&
        sample_rate != AUDIO_SAMPLE_RATE_16KHZ) return -1;

    s_initialized = false;

    hal_i2c_config_t i2c_cfg = {
        .speed = HAL_I2C_SPEED_FAST,
        .use_dma = false,
    };
    if (hal_i2c_init(I2C_BUS_AUDIO, &i2c_cfg) != 0 ||
        !hal_i2c_probe(I2C_BUS_AUDIO, TLV320AIC3204_I2C_ADDR)) {
        return -1;
    }

    /* 1. Mute pinini düşük yap (başlangıçta sessiz) */
    hal_gpio_configure(PIN_AUDIO_MUTE, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);
    hal_gpio_clear(PIN_AUDIO_MUTE);

#define AIC_WRITE_OR_FAIL(page, reg, value) \
    do { if (aic_write((page), (reg), (value)) != 0) goto init_failed; } while (0)

    /* 2. Donanımsal Reset (Sayfa 0, Reg 0x01 = 0x01) */
    AIC_WRITE_OR_FAIL(0, AIC3204_SW_RESET_REG, 0x01);
    hal_delay_ms(10);

    /* 3. Güç Yapılandırması (Sayfa 1) */
    /* LDO etkinleştir, analog güç aç */
    AIC_WRITE_OR_FAIL(1, AIC3204_LDO_CTRL_REG, 0x08); /* AVDD LDO açık */
    AIC_WRITE_OR_FAIL(1, AIC3204_PWR_CTRL_REG, 0x00); /* D-LDO kapalı (harici 1.8V) */
    AIC_WRITE_OR_FAIL(1, 0x0A, 0x00);                 /* Ortak mod gerilimi 0.9V */

    /* 4. Saat Ağacı (Sayfa 0) — MCLK = 12 MHz (GD32 MCO veya kristal) */
    /* BCLK ve WCLK I2S slave modunda çalışır */
    AIC_WRITE_OR_FAIL(0, AIC3204_CLK_MUX_REG, 0x00); /* PLL Clock = MCLK */
    AIC_WRITE_OR_FAIL(0, AIC3204_PLL_P_R_REG, 0x11); /* P=1, R=1 */

    if (sample_rate == AUDIO_SAMPLE_RATE_8KHZ) {
        /* 8 kHz ses için: PLL J=8, D=1920 */
        AIC_WRITE_OR_FAIL(0, AIC3204_PLL_J_REG, 0x08);
        AIC_WRITE_OR_FAIL(0, AIC3204_PLL_D_MSB_REG, 0x07);
        AIC_WRITE_OR_FAIL(0, AIC3204_PLL_D_LSB_REG, 0x80);
        AIC_WRITE_OR_FAIL(0, AIC3204_NDAC_REG, 0x84); /* NDAC = 4, açık */
        AIC_WRITE_OR_FAIL(0, AIC3204_MDAC_REG, 0x82); /* MDAC = 2, açık */
        AIC_WRITE_OR_FAIL(0, AIC3204_DOSR_MSB_REG, 0x01); /* DOSR = 384 */
        AIC_WRITE_OR_FAIL(0, AIC3204_DOSR_LSB_REG, 0x80);
    } else {
        /* 16 kHz ses için varsayılan */
        AIC_WRITE_OR_FAIL(0, AIC3204_PLL_J_REG, 0x08);
        AIC_WRITE_OR_FAIL(0, AIC3204_PLL_D_MSB_REG, 0x07);
        AIC_WRITE_OR_FAIL(0, AIC3204_PLL_D_LSB_REG, 0x80);
        AIC_WRITE_OR_FAIL(0, AIC3204_NDAC_REG, 0x82); /* NDAC = 2, açık */
        AIC_WRITE_OR_FAIL(0, AIC3204_MDAC_REG, 0x82); /* MDAC = 2, açık */
        AIC_WRITE_OR_FAIL(0, AIC3204_DOSR_MSB_REG, 0x00); /* DOSR = 192 */
        AIC_WRITE_OR_FAIL(0, AIC3204_DOSR_LSB_REG, 0xC0);
    }

    /* 5. Dijital Ses Arayüzü (Sayfa 0) — I2S, 16-bit, Slave */
    AIC_WRITE_OR_FAIL(0, AIC3204_IFACE_CTRL1_REG, 0x00); /* I2S, 16-bit, BCLK/WCLK giriş */
    AIC_WRITE_OR_FAIL(0, AIC3204_DAC_PRB_REG, 0x01); /* İşleme Bloğu PRB_P1 */
    AIC_WRITE_OR_FAIL(0, AIC3204_ADC_PRB_REG, 0x01); /* İşleme Bloğu PRB_R1 */

    /* 6. DAC Yönlendirme ve Güç (Sayfa 0) */
    AIC_WRITE_OR_FAIL(0, AIC3204_DAC_CHN_REG, 0xD4); /* Sol/Sağ DAC açık, yumuşak geçiş */
    AIC_WRITE_OR_FAIL(0, AIC3204_DAC_VOL_L_REG, 0x00); /* Sol ses seviyesi 0 dB */
    AIC_WRITE_OR_FAIL(0, AIC3204_DAC_VOL_R_REG, 0x00); /* Sağ ses seviyesi 0 dB */

    /* 7. Analog Çıkış Yönlendirme (Sayfa 1) */
    AIC_WRITE_OR_FAIL(1, AIC3204_HP_DRIVER_REG, 0x3C); /* HPL ve HPR sürücüleri açık */
    AIC_WRITE_OR_FAIL(1, 0x0E, 0x08); /* Sol DAC → HPL */
    AIC_WRITE_OR_FAIL(1, 0x0F, 0x08); /* Sağ DAC → HPR */
    AIC_WRITE_OR_FAIL(1, 0x10, 0x00); /* HPL kazancı 0 dB */
    AIC_WRITE_OR_FAIL(1, 0x11, 0x00); /* HPR kazancı 0 dB */

    /* 8. Mikrofon Giriş Yönlendirme (Sayfa 1) */
    AIC_WRITE_OR_FAIL(1, AIC3204_MICBIAS_REG, 0x40); /* MicBias 2.5V açık */
    AIC_WRITE_OR_FAIL(1, AIC3204_LMIC_PGA_P_REG, 0x10); /* IN1_L → Sol MICPGA */
    AIC_WRITE_OR_FAIL(1, AIC3204_LMIC_PGA_M_REG, 0x10); /* IN1_R → Sol MICPGA negatif */
    AIC_WRITE_OR_FAIL(1, AIC3204_ADC_PGA_L_REG, 0x28); /* Sol ADC kazancı +20 dB */

    s_initialized = true;
#undef AIC_WRITE_OR_FAIL
    return 0;

init_failed:
#undef AIC_WRITE_OR_FAIL
    hal_gpio_clear(PIN_AUDIO_MUTE);
    return -2;
}

int audio_codec_set_volume(uint8_t volume_pct)
{
    if (!s_initialized) return -1;
    if (volume_pct > 100) volume_pct = 100;

    /* %0 → -63.5 dB (0x7F = sessiz), %100 → 0 dB (0x00) */
    int8_t gain_db;
    if (volume_pct == 0) {
        gain_db = 0x7F;  /* Mute */
    } else {
        /* Lineer orantılama: 100% -> 0x00, 1% -> 0x3F (-31.5 dB) */
        gain_db = (int8_t)((100 - volume_pct) * 63 / 100);
    }

    if (aic_write(0, AIC3204_DAC_VOL_L_REG, (uint8_t)gain_db) != 0 ||
        aic_write(0, AIC3204_DAC_VOL_R_REG, (uint8_t)gain_db) != 0) return -1;
    return 0;
}

int audio_codec_set_mic_gain(uint8_t gain_pct)
{
    if (!s_initialized) return -1;
    if (gain_pct > 100) gain_pct = 100;

    /* 0..47.5 dB PGA kazancı (0.5 dB adımlarla, 0..0x5F) */
    uint8_t pga = (uint8_t)(gain_pct * 95 / 100);
    if (aic_write(1, AIC3204_ADC_PGA_L_REG, pga) != 0 ||
        aic_write(1, AIC3204_ADC_PGA_R_REG, pga) != 0) return -1;
    return 0;
}

int audio_codec_set_path(audio_path_t path, bool enable)
{
    if (!s_initialized) return -1;
    switch (path) {
    case AUDIO_PATH_SPEAKER:
        if (enable) {
            if (aic_write(1, AIC3204_HP_DRIVER_REG, 0x3C) != 0) return -1;
            hal_gpio_set(PIN_AUDIO_MUTE);   /* Amfiyi aç */
        } else {
            hal_gpio_clear(PIN_AUDIO_MUTE); /* Amfiyi kapat */
        }
        break;

    case AUDIO_PATH_MIC_INTERNAL:
    case AUDIO_PATH_MIC_EXTERNAL:
        if (enable) {
            if (aic_write(1, AIC3204_MICBIAS_REG, 0x40) != 0 ||
                aic_write(0, 0x51, 0xC0) != 0) return -1;
        } else {
            if (aic_write(0, 0x51, 0x00) != 0 ||
                aic_write(1, AIC3204_MICBIAS_REG, 0x00) != 0) return -1;
        }
        break;

    case AUDIO_PATH_EARPHONE:
        return -2;

    case AUDIO_PATH_MUTE:
        hal_gpio_clear(PIN_AUDIO_MUTE);
        break;

    default:
        return -1;
    }

    return 0;
}

int audio_codec_mute(bool mute)
{
    return audio_codec_set_path(AUDIO_PATH_SPEAKER, !mute);
}
