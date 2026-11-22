/**
 * @file    driver_audio.c
 * @brief   TLV320AIC3204 Audio Codec Driver Implementation - AT-D878 Open Firmware
 * @version 0.0.1
 */

#include "driver_audio.h"
#include "hal_i2c.h"
#include "hal_gpio.h"

int audio_codec_init(audio_sample_rate_t sample_rate)
{
    (void)sample_rate;
    /* Donanım Mute pinini aç */
    hal_gpio_set(PIN_AUDIO_MUTE);
    return 0;
}

int audio_codec_set_volume(uint8_t volume_pct)
{
    (void)volume_pct;
    return 0;
}

int audio_codec_set_mic_gain(uint8_t gain_pct)
{
    (void)gain_pct;
    return 0;
}

int audio_codec_set_path(audio_path_t path, bool enable)
{
    if (path == AUDIO_PATH_SPEAKER) {
        if (enable) {
            hal_gpio_set(PIN_AUDIO_MUTE);   /* Hoparlör amplifikatörü açık */
        } else {
            hal_gpio_clear(PIN_AUDIO_MUTE); /* Sessiz */
        }
    }
    return 0;
}

int audio_codec_mute(bool mute)
{
    return audio_codec_set_path(AUDIO_PATH_SPEAKER, !mute);
}
