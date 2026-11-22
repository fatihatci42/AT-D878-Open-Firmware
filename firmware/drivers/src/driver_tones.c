/**
 * @file    driver_tones.c
 * @brief   CTCSS / DCS Sub-Audible Tone Engine Implementation
 * @version 0.1.0
 */

#include "driver_tones.h"
#include <stdio.h>

/* Standart 50 CTCSS Tonu (Hz x 10) */
const uint16_t CTCSS_TONE_TABLE[CTCSS_TONE_COUNT] = {
     670,  693,  719,  744,  770,  797,  825,  854,  885,  915,
     948,  974, 1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273,
    1318, 1365, 1413, 1462, 1514, 1567, 1598, 1622, 1655, 1679,
    1713, 1738, 1773, 1799, 1835, 1862, 1899, 1928, 1966, 1995,
    2035, 2065, 2107, 2181, 2257, 2291, 2336, 2418, 2503, 2541
};

/* Standart 104 DCS Kodu (Oktal) */
const uint16_t DCS_CODE_TABLE[DCS_CODE_COUNT] = {
     23,  25,  26,  31,  32,  36,  43,  47,  51,  53,  54,  65,  71,  72,  73,  74,
    114, 115, 116, 122, 125, 131, 132, 134, 143, 145, 152, 155, 156, 162, 165, 172,
    174, 205, 212, 223, 225, 226, 243, 244, 245, 246, 251, 252, 255, 261, 263, 265,
    266, 271, 274, 306, 311, 315, 325, 331, 332, 343, 346, 351, 356, 364, 365, 371,
    411, 412, 413, 423, 431, 432, 445, 446, 452, 454, 455, 462, 464, 465, 466, 503,
    506, 516, 523, 526, 532, 546, 565, 606, 612, 624, 627, 631, 632, 654, 662, 664,
    703, 712, 723, 731, 732, 734, 743, 754
};

int tones_find_ctcss_index(uint16_t freq_x10)
{
    for (int i = 0; i < CTCSS_TONE_COUNT; i++) {
        if (CTCSS_TONE_TABLE[i] == freq_x10) return i;
    }
    return -1;
}

uint32_t tones_calc_dcs_word(uint16_t octal_code, bool inverted)
{
    /* 9-bit DCS veri kelimesi: 3 oktal basamağın binary hali */
    /* Örn: 023 (octal) -> 000 010 011 */
    uint32_t d0 = octal_code % 10;
    uint32_t d1 = (octal_code / 10) % 10;
    uint32_t d2 = (octal_code / 100) % 10;
    uint32_t data9 = (d2 << 6) | (d1 << 3) | d0;

    /* Sabit başlangıç bitleri: 100b (4) + 9 bit veri -> 12 bit */
    uint32_t word12 = (0x04U << 9) | (data9 & 0x1FFU);

    /* Golay (23,12) parite hesaplama polinomu: x^11 + x^9 + x^7 + x^6 + x^5 + x + 1 (0xC75) */
    uint32_t poly = 0x0C75U;
    uint32_t reg = word12 << 11;
    for (int i = 22; i >= 11; i--) {
        if (reg & (1U << i)) {
            reg ^= (poly << (i - 11));
        }
    }
    uint32_t parity11 = reg & 0x7FFU;
    uint32_t code23 = (word12 << 11) | parity11;

    if (inverted) {
        code23 ^= 0x7FFFFFU;  /* 23-bit tersle */
    }

    return code23;
}

void tones_format_ctcss(uint16_t freq_x10, char *out_str, size_t max_len)
{
    if (!out_str || max_len == 0) return;
    if (freq_x10 == 0) {
        snprintf(out_str, max_len, "Kapali");
    } else {
        snprintf(out_str, max_len, "%u.%u Hz", freq_x10 / 10, freq_x10 % 10);
    }
}

void tones_format_dcs(uint16_t octal_code, bool inverted, char *out_str, size_t max_len)
{
    if (!out_str || max_len == 0) return;
    if (octal_code == 0) {
        snprintf(out_str, max_len, "Kapali");
    } else {
        snprintf(out_str, max_len, "D%03u%c", octal_code, inverted ? 'I' : 'N');
    }
}

void tones_transmit_ste_tail(ste_mode_t mode, uint16_t ctcss_freq_x10)
{
    if (mode == STE_MODE_OFF) return;
    (void)ctcss_freq_x10;

    switch (mode) {
    case STE_MODE_CTCSS_REVERSE_180:
    case STE_MODE_CTCSS_REVERSE_120:
    case STE_MODE_DCS_134HZ:
    case STE_MODE_55HZ:
    default:
        break;
    }
}

bool tones_detect_ste_tail(ste_mode_t mode)
{
    if (mode == STE_MODE_OFF) return false;
    return false;
}

