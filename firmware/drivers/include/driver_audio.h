/**
 * @file    driver_audio.h
 * @brief   TLV320AIC3204 Audio Codec Driver - AT-D878 Open Firmware
 * @version 0.0.1
 *
 * Texas Instruments TLV320AIC3204 ultra-düşük güçlü stereo ses codec sürücüsü.
 * I2C kontrol arayüzü ve I2S dijital ses akışı desteği sağlar.
 */

#ifndef DRIVER_AUDIO_H
#define DRIVER_AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 7-bit standart I2C adresi */
#define TLV320AIC3204_I2C_ADDR       0x18

/* Sayfa seçim register'ı (Tüm sayfalarda ortaktır) */
#define AIC3204_PAGE_SELECT_REG      0x00

/* Sayfa 0 Register Tanımları */
#define AIC3204_SW_RESET_REG         0x01
#define AIC3204_CLK_MUX_REG          0x04
#define AIC3204_PLL_P_R_REG          0x05
#define AIC3204_PLL_J_REG            0x06
#define AIC3204_PLL_D_MSB_REG        0x07
#define AIC3204_PLL_D_LSB_REG        0x08
#define AIC3204_NDAC_REG             0x0B
#define AIC3204_MDAC_REG             0x0C
#define AIC3204_DOSR_MSB_REG         0x0D
#define AIC3204_DOSR_LSB_REG         0x0E
#define AIC3204_NADC_REG             0x12
#define AIC3204_MADC_REG             0x13
#define AIC3204_AOSR_REG             0x14
#define AIC3204_IFACE_CTRL1_REG      0x1B
#define AIC3204_DAC_PRB_REG          0x3C
#define AIC3204_ADC_PRB_REG          0x3D
#define AIC3204_DAC_CHN_REG          0x3F
#define AIC3204_DAC_VOL_L_REG        0x41
#define AIC3204_DAC_VOL_R_REG        0x42

/* Sayfa 1 Register Tanımları (Güç ve Analog Yönlendirme) */
#define AIC3204_PWR_CTRL_REG         0x01
#define AIC3204_LDO_CTRL_REG         0x02
#define AIC3204_HP_DRIVER_REG        0x09
#define AIC3204_MICBIAS_REG          0x33
#define AIC3204_LMIC_PGA_P_REG       0x34
#define AIC3204_LMIC_PGA_M_REG       0x36
#define AIC3204_RMIC_PGA_P_REG       0x37
#define AIC3204_RMIC_PGA_M_REG       0x39
#define AIC3204_ADC_PGA_L_REG        0x3B
#define AIC3204_ADC_PGA_R_REG        0x3C

/**
 * @brief Ses örnekleme frekansları
 */
typedef enum {
    AUDIO_SAMPLE_RATE_8KHZ  = 8000,
    AUDIO_SAMPLE_RATE_16KHZ = 16000,
    AUDIO_SAMPLE_RATE_48KHZ = 48000
} audio_sample_rate_t;

/**
 * @brief Ses yolu çalışma modu
 */
typedef enum {
    AUDIO_PATH_MUTE = 0,
    AUDIO_PATH_SPEAKER,
    AUDIO_PATH_EARPHONE,
    AUDIO_PATH_MIC_INTERNAL,
    AUDIO_PATH_MIC_EXTERNAL
} audio_path_t;

/**
 * @brief Audio codec başlatma
 */
int audio_codec_init(audio_sample_rate_t sample_rate);

/**
 * @brief Hoparlör/Kulaklık ses seviyesi (0-100%)
 */
int audio_codec_set_volume(uint8_t volume_pct);

/**
 * @brief Mikrofon kazancı (0-100%)
 */
int audio_codec_set_mic_gain(uint8_t gain_pct);

/**
 * @brief Ses yolunu aktif et / kapat
 */
int audio_codec_set_path(audio_path_t path, bool enable);

/**
 * @brief Donanımsal Mute
 */
int audio_codec_mute(bool mute);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_AUDIO_H */
