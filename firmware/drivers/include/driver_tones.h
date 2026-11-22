/**
 * @file    driver_tones.h
 * @brief   CTCSS / DCS Sub-Audible Tone Engine - AT-D878 Open Firmware
 * @version 0.1.0
 *
 * Standart 50 CTCSS Tonu ve 104 DCS Kodu desteği.
 * Röle açma, susturma ve Squelch Tail Elimination (STE) için kullanılır.
 */

#ifndef DRIVER_TONES_H
#define DRIVER_TONES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CTCSS_TONE_COUNT   50
#define DCS_CODE_COUNT     104

/* Standart 50 CTCSS Frekans Listesi (Hz x 10, örn: 67.0 Hz -> 670) */
extern const uint16_t CTCSS_TONE_TABLE[CTCSS_TONE_COUNT];

/* Standart 104 DCS Kod Listesi (Oktal, örn: 023 -> 23) */
extern const uint16_t DCS_CODE_TABLE[DCS_CODE_COUNT];

/**
 * @brief CTCSS frekansından liste indeksini bul
 * @param freq_x10 Frekans x 10 (örn: 885)
 * @return İndeks (0..49) veya -1
 */
int tones_find_ctcss_index(uint16_t freq_x10);

/**
 * @brief DCS kodunun 23-bit Golay kod kelimesini hesapla
 * @param octal_code 3 basamaklı oktal DCS kodu (örn: 023)
 * @param inverted   Ters polarite (DCS-I) mi?
 * @return 23-bit DCS kod kelimesi
 */
uint32_t tones_calc_dcs_word(uint16_t octal_code, bool inverted);

/**
 * @brief CTCSS frekansını biçimlendir (örn: 885 -> "88.5 Hz")
 */
void tones_format_ctcss(uint16_t freq_x10, char *out_str, size_t max_len);

/**
 * @brief DCS kodunu biçimlendir (örn: 23, inverted=false -> "D023N")
 */
void tones_format_dcs(uint16_t octal_code, bool inverted, char *out_str, size_t max_len);

typedef enum {
    STE_MODE_OFF = 0,
    STE_MODE_CTCSS_REVERSE_180,  /**< 180 derece CTCSS ters faz (Standart) */
    STE_MODE_CTCSS_REVERSE_120,  /**< 120 derece CTCSS ters faz (Motorola) */
    STE_MODE_DCS_134HZ,          /**< 134.4 Hz DCS Turn-off tonu */
    STE_MODE_55HZ                /**< 55.2 Hz Çin standardı */
} ste_mode_t;

/**
 * @brief TX bitişinde Squelch Tail Elimination (STE) dizisini yürüt
 * @param mode STE modu
 * @param ctcss_freq_x10 Aktif CTCSS frekansı (varsa)
 */
void tones_transmit_ste_tail(ste_mode_t mode, uint16_t ctcss_freq_x10);

/**
 * @brief RX sırasında STE algılandı mı kontrol et
 * @return true = Karşı taraf mandalı bıraktı, sesi hemen kes (mute)
 */
bool tones_detect_ste_tail(ste_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_TONES_H */
