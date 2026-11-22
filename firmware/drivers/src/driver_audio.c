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

#define I2C_BUS_AUDIO     HAL_I2C_0

/* ============================================================
 * Yardımcı I2C Fonksiyonları
 * ============================================================ */

static inline int aic_write(uint8_t page, uint8_t reg, uint8_t val)
{
    /* Sayfa seç */
    hal_i2c_write_reg8(I2C_BUS_AUDIO, TLV320AIC3204_I2C_ADDR, AIC3204_PAGE_SELECT_REG, page);
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
    /* 1. Mute pinini düşük yap (başlangıçta sessiz) */
    hal_gpio_configure(PIN_AUDIO_MUTE, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);
    hal_gpio_clear(PIN_AUDIO_MUTE);

    /* 2. Donanımsal Reset (Sayfa 0, Reg 0x01 = 0x01) */
    aic_write(0, AIC3204_SW_RESET_REG, 0x01);
    hal_delay_ms(10);

    /* 3. Güç Yapılandırması (Sayfa 1) */
    /* LDO etkinleştir, analog güç aç */
    aic_write(1, AIC3204_LDO_CTRL_REG, 0x08);     /* AVDD LDO açık */
    aic_write(1, AIC3204_PWR_CTRL_REG, 0x00);     /* D-LDO kapalı (harici 1.8V) */
    aic_write(1, 0x0A, 0x00);                     /* Ortak mod gerilimi 0.9V */

    /* 4. Saat Ağacı (Sayfa 0) — MCLK = 12 MHz (GD32 MCO veya kristal) */
    /* BCLK ve WCLK I2S slave modunda çalışır */
    aic_write(0, AIC3204_CLK_MUX_REG, 0x00);      /* PLL Clock = MCLK */
    aic_write(0, AIC3204_PLL_P_R_REG, 0x11);      /* P=1, R=1 */

    if (sample_rate == AUDIO_SAMPLE_RATE_8KHZ) {
        /* 8 kHz ses için: PLL J=8, D=1920 */
        aic_write(0, AIC3204_PLL_J_REG, 0x08);
        aic_write(0, AIC3204_PLL_D_MSB_REG, 0x07);
        aic_write(0, AIC3204_PLL_D_LSB_REG, 0x80);
        aic_write(0, AIC3204_NDAC_REG, 0x84);     /* NDAC = 4, açık */
        aic_write(0, AIC3204_MDAC_REG, 0x82);     /* MDAC = 2, açık */
        aic_write(0, AIC3204_DOSR_MSB_REG, 0x01); /* DOSR = 384 */
        aic_write(0, AIC3204_DOSR_LSB_REG, 0x80);
    } else {
        /* 16 kHz ses için varsayılan */
        aic_write(0, AIC3204_PLL_J_REG, 0x08);
        aic_write(0, AIC3204_PLL_D_MSB_REG, 0x07);
        aic_write(0, AIC3204_PLL_D_LSB_REG, 0x80);
        aic_write(0, AIC3204_NDAC_REG, 0x82);     /* NDAC = 2, açık */
        aic_write(0, AIC3204_MDAC_REG, 0x82);     /* MDAC = 2, açık */
        aic_write(0, AIC3204_DOSR_MSB_REG, 0x00); /* DOSR = 192 */
        aic_write(0, AIC3204_DOSR_LSB_REG, 0xC0);
    }

    /* 5. Dijital Ses Arayüzü (Sayfa 0) — I2S, 16-bit, Slave */
    aic_write(0, AIC3204_IFACE_CTRL1_REG, 0x00);  /* I2S, 16-bit, BCLK/WCLK giriş */
    aic_write(0, AIC3204_DAC_PRB_REG, 0x01);      /* İşleme Bloğu PRB_P1 */
    aic_write(0, AIC3204_ADC_PRB_REG, 0x01);      /* İşleme Bloğu PRB_R1 */

    /* 6. DAC Yönlendirme ve Güç (Sayfa 0) */
    aic_write(0, AIC3204_DAC_CHN_REG, 0xD4);      /* Sol/Sağ DAC açık, yumuşak geçiş */
    aic_write(0, AIC3204_DAC_VOL_L_REG, 0x00);    /* Sol ses seviyesi 0 dB */
    aic_write(0, AIC3204_DAC_VOL_R_REG, 0x00);    /* Sağ ses seviyesi 0 dB */

    /* 7. Analog Çıkış Yönlendirme (Sayfa 1) */
    aic_write(1, AIC3204_HP_DRIVER_REG, 0x3C);    /* HPL ve HPR sürücüleri açık */
    aic_write(1, 0x0E, 0x08);                     /* Sol DAC → HPL */
    aic_write(1, 0x0F, 0x08);                     /* Sağ DAC → HPR */
    aic_write(1, 0x10, 0x00);                     /* HPL kazancı 0 dB */
    aic_write(1, 0x11, 0x00);                     /* HPR kazancı 0 dB */

    /* 8. Mikrofon Giriş Yönlendirme (Sayfa 1) */
    aic_write(1, AIC3204_MICBIAS_REG, 0x40);      /* MicBias 2.5V açık */
    aic_write(1, AIC3204_LMIC_PGA_P_REG, 0x10);   /* IN1_L → Sol MICPGA (10k direnç) */
    aic_write(1, AIC3204_LMIC_PGA_M_REG, 0x10);   /* IN1_R → Sol MICPGA negatif */
    aic_write(1, AIC3204_ADC_PGA_L_REG, 0x28);    /* Sol ADC kazancı +20 dB */

    /* 9. Mute kaldır */
    hal_gpio_set(PIN_AUDIO_MUTE);

    return 0;
}

int audio_codec_set_volume(uint8_t volume_pct)
{
    if (volume_pct > 100) volume_pct = 100;

    /* %0 → -63.5 dB (0x7F = sessiz), %100 → 0 dB (0x00) */
    int8_t gain_db;
    if (volume_pct == 0) {
        gain_db = 0x7F;  /* Mute */
    } else {
        /* Lineer orantılama: 100% -> 0x00, 1% -> 0x3F (-31.5 dB) */
        gain_db = (int8_t)((100 - volume_pct) * 63 / 100);
    }

    aic_write(0, AIC3204_DAC_VOL_L_REG, (uint8_t)gain_db);
    aic_write(0, AIC3204_DAC_VOL_R_REG, (uint8_t)gain_db);
    return 0;
}

int audio_codec_set_mic_gain(uint8_t gain_pct)
{
    if (gain_pct > 100) gain_pct = 100;

    /* 0..47.5 dB PGA kazancı (0.5 dB adımlarla, 0..0x5F) */
    uint8_t pga = (uint8_t)(gain_pct * 95 / 100);
    aic_write(1, AIC3204_ADC_PGA_L_REG, pga);
    aic_write(1, AIC3204_ADC_PGA_R_REG, pga);
    return 0;
}

int audio_codec_set_path(audio_path_t path, bool enable)
{
    switch (path) {
    case AUDIO_PATH_SPEAKER:
        if (enable) {
            hal_gpio_set(PIN_AUDIO_MUTE);   /* Amfiyi aç */
            aic_write(1, AIC3204_HP_DRIVER_REG, 0x3C);
        } else {
            hal_gpio_clear(PIN_AUDIO_MUTE); /* Amfiyi kapat */
        }
        break;

    case AUDIO_PATH_MIC_INTERNAL:
    case AUDIO_PATH_MIC_EXTERNAL:
        if (enable) {
            aic_write(1, AIC3204_MICBIAS_REG, 0x40); /* MicBias Aç */
            aic_write(0, 0x51, 0xC0);                /* Sol ve Sağ ADC Açık */
        } else {
            aic_write(0, 0x51, 0x00);                /* ADC Kapalı */
            aic_write(1, AIC3204_MICBIAS_REG, 0x00); /* MicBias Kapat */
        }
        break;

    case AUDIO_PATH_MUTE:
    default:
        hal_gpio_clear(PIN_AUDIO_MUTE);
        break;
    }

    return 0;
}

int audio_codec_mute(bool mute)
{
    return audio_codec_set_path(AUDIO_PATH_SPEAKER, !mute);
}
