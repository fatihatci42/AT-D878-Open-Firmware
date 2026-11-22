/**
 * @file    driver_display.h
 * @brief   1.77" TFT LCD Driver - AT-D878 Open Firmware
 * @version 0.0.1
 *
 * AnyTone 1.77" Renkli TFT LCD (128x160 veya 160x128 RGB565) donanım sürücüsü.
 * SPI arayüzü ve DMA destekli framebuffer aktarımı sağlar.
 */

#ifndef DRIVER_DISPLAY_H
#define DRIVER_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LCD_WIDTH   160
#define LCD_HEIGHT  128

/* Standart 16-bit RGB565 Renk Tanımları */
#define COLOR_BLACK       0x0000
#define COLOR_NAVY        0x000F
#define COLOR_DARKGREEN   0x03E0
#define COLOR_DARKCYAN    0x03EF
#define COLOR_MAROON      0x7800
#define COLOR_PURPLE      0x780F
#define COLOR_OLIVE       0x7BE0
#define COLOR_LIGHTGREY   0xC618
#define COLOR_DARKGREY    0x7BEF
#define COLOR_BLUE        0x001F
#define COLOR_GREEN       0x07E0
#define COLOR_CYAN        0x07FF
#define COLOR_RED         0xF800
#define COLOR_MAGENTA     0xF81F
#define COLOR_YELLOW      0xFFE0
#define COLOR_WHITE       0xFFFF
#define COLOR_ORANGE      0xFD20

/**
 * @brief Ekran sürücüsünü başlat
 */
int display_init(void);

/**
 * @brief Ekran aydınlatmasını ayarla (0-100% PWM)
 */
void display_set_backlight(uint8_t brightness_pct);

/**
 * @brief Tüm ekranı belirtilen renk ile temizle
 */
void display_clear(uint16_t color);

/**
 * @brief Belirli bir alana piksel verisi yaz
 */
void display_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

/**
 * @brief Piksel çiz
 */
void display_draw_pixel(int16_t x, int16_t y, uint16_t color);

/**
 * @brief Metin yazdır
 */
void display_draw_string(int16_t x, int16_t y, const char *str, uint16_t fg_color, uint16_t bg_color, uint8_t size);

/**
 * @brief Framebuffer'ı LCD'ye gönder (DMA asenkron veya senkron)
 */
void display_flush(void);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_DISPLAY_H */
