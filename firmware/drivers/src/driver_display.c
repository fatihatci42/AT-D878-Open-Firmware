/**
 * @file    driver_display.c
 * @brief   1.77" TFT LCD (160x128) Driver Implementation - AT-D878 Open Firmware
 * @version 0.0.1
 *
 * Doğrulanan Pinler (D878UV2A-BORD):
 * - RST:       GPIOC Pin 3
 * - DC (A0):   GPIOC Pin 4
 * - CS:        GPIOC Pin 5
 * - Backlight: GPIOC Pin 10
 */

#include "driver_display.h"
#include "hal_gpio.h"
#include "hal_spi.h"
#include <string.h>

#define ST7735_SWRESET  0x01
#define ST7735_SLPOUT   0x11
#define ST7735_NORON    0x13
#define ST7735_INVOFF   0x20
#define ST7735_DISPON   0x29
#define ST7735_CASET    0x2A
#define ST7735_RASET    0x2B
#define ST7735_RAMWR    0x2C
#define ST7735_MADCTL   0x36
#define ST7735_COLMOD   0x3A

/* Dahili Framebuffer (160x128 piksel, 16-bit RGB565) = 40 KB SRAM */
static uint16_t s_framebuffer[LCD_WIDTH * LCD_HEIGHT];

/* Basit 5x7 ASCII Font Veritabanı (Küçük boyut, yüksek verim) */
static const uint8_t s_font5x7[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, /* Boşluk (Space - 32) */
    0x00, 0x00, 0x5F, 0x00, 0x00, /* ! */
    0x00, 0x07, 0x00, 0x07, 0x00, /* " */
    0x14, 0x7F, 0x14, 0x7F, 0x14, /* # */
    0x24, 0x2A, 0x7F, 0x2A, 0x12, /* $ */
    0x23, 0x13, 0x08, 0x64, 0x62, /* % */
    0x36, 0x49, 0x55, 0x22, 0x50, /* & */
    0x00, 0x05, 0x03, 0x00, 0x00, /* ' */
    0x00, 0x1C, 0x22, 0x41, 0x00, /* ( */
    0x00, 0x41, 0x22, 0x1C, 0x00, /* ) */
    0x14, 0x08, 0x3E, 0x08, 0x14, /* * */
    0x08, 0x08, 0x3E, 0x08, 0x08, /* + */
    0x00, 0x50, 0x30, 0x00, 0x00, /* , */
    0x08, 0x08, 0x08, 0x08, 0x08, /* - */
    0x00, 0x60, 0x60, 0x00, 0x00, /* . */
    0x20, 0x10, 0x08, 0x04, 0x02, /* / */
    0x3E, 0x51, 0x49, 0x45, 0x3E, /* 0 */
    0x00, 0x42, 0x7F, 0x40, 0x00, /* 1 */
    0x42, 0x61, 0x51, 0x49, 0x46, /* 2 */
    0x21, 0x41, 0x45, 0x4B, 0x31, /* 3 */
    0x18, 0x14, 0x12, 0x7F, 0x10, /* 4 */
    0x27, 0x45, 0x45, 0x45, 0x39, /* 5 */
    0x3C, 0x4A, 0x49, 0x49, 0x30, /* 6 */
    0x01, 0x71, 0x09, 0x05, 0x03, /* 7 */
    0x36, 0x49, 0x49, 0x49, 0x36, /* 8 */
    0x06, 0x49, 0x49, 0x29, 0x1E, /* 9 */
    0x00, 0x36, 0x36, 0x00, 0x00, /* : */
    0x00, 0x56, 0x36, 0x00, 0x00, /* ; */
    0x08, 0x14, 0x22, 0x41, 0x00, /* < */
    0x14, 0x14, 0x14, 0x14, 0x14, /* = */
    0x00, 0x41, 0x22, 0x14, 0x08, /* > */
    0x02, 0x01, 0x51, 0x09, 0x06, /* ? */
    0x32, 0x49, 0x79, 0x41, 0x3E, /* @ */
    0x7E, 0x11, 0x11, 0x11, 0x7E, /* A */
    0x7F, 0x49, 0x49, 0x49, 0x36, /* B */
    0x3E, 0x41, 0x41, 0x41, 0x22, /* C */
    0x7F, 0x41, 0x41, 0x22, 0x1C, /* D */
    0x7F, 0x49, 0x49, 0x49, 0x41, /* E */
    0x7F, 0x09, 0x09, 0x09, 0x01, /* F */
    0x3E, 0x41, 0x49, 0x49, 0x7A, /* G */
    0x7F, 0x08, 0x08, 0x08, 0x7F, /* H */
    0x00, 0x41, 0x7F, 0x41, 0x00, /* I */
    0x20, 0x40, 0x41, 0x3F, 0x01, /* J */
    0x7F, 0x08, 0x14, 0x22, 0x41, /* K */
    0x7F, 0x40, 0x40, 0x40, 0x40, /* L */
    0x7F, 0x02, 0x0C, 0x02, 0x7F, /* M */
    0x7F, 0x04, 0x08, 0x10, 0x7F, /* N */
    0x3E, 0x41, 0x41, 0x41, 0x3E, /* O */
    0x7F, 0x09, 0x09, 0x09, 0x06, /* P */
    0x3E, 0x41, 0x51, 0x21, 0x5E, /* Q */
    0x7F, 0x09, 0x19, 0x29, 0x46, /* R */
    0x46, 0x49, 0x49, 0x49, 0x31, /* S */
    0x01, 0x01, 0x7F, 0x01, 0x01, /* T */
    0x3F, 0x40, 0x40, 0x40, 0x3F, /* U */
    0x1F, 0x20, 0x40, 0x20, 0x1F, /* V */
    0x7F, 0x20, 0x18, 0x20, 0x7F, /* W */
    0x63, 0x14, 0x08, 0x14, 0x63, /* X */
    0x07, 0x08, 0x70, 0x08, 0x07, /* Y */
    0x61, 0x51, 0x49, 0x45, 0x43  /* Z */
};

/* Yardımcı: Basit Gecikme */
static void delay_ms(volatile uint32_t ms)
{
    for (volatile uint32_t i = 0; i < ms * 12000; i++) {
        __asm__("nop");
    }
}

static void lcd_write_cmd(uint8_t cmd)
{
    hal_gpio_clear(PIN_LCD_DC); /* DC = 0 (Komut) */
    hal_gpio_clear(PIN_LCD_CS); /* CS = 0 */
    hal_spi_transmit(HAL_SPI_0, &cmd, 1);
    hal_gpio_set(PIN_LCD_CS);   /* CS = 1 */
}

static void lcd_write_data(uint8_t data)
{
    hal_gpio_set(PIN_LCD_DC);   /* DC = 1 (Veri) */
    hal_gpio_clear(PIN_LCD_CS); /* CS = 0 */
    hal_spi_transmit(HAL_SPI_0, &data, 1);
    hal_gpio_set(PIN_LCD_CS);   /* CS = 1 */
}

int display_init(void)
{
    /* 1. Pinleri yapılandır */
    hal_gpio_configure(PIN_LCD_RST, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);
    hal_gpio_configure(PIN_LCD_DC,  GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);
    hal_gpio_configure(PIN_LCD_CS,  GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);
    hal_gpio_configure(PIN_LCD_BL,  GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);

    /* 2. Donanımsal Reset Dizilimi */
    hal_gpio_set(PIN_LCD_RST);
    delay_ms(10);
    hal_gpio_clear(PIN_LCD_RST);
    delay_ms(20);
    hal_gpio_set(PIN_LCD_RST);
    delay_ms(50);

    /* 3. ST7735 / ILI9163 Komut Dizisi */
    lcd_write_cmd(ST7735_SWRESET);
    delay_ms(120);

    lcd_write_cmd(ST7735_SLPOUT);
    delay_ms(120);

    lcd_write_cmd(ST7735_COLMOD);
    lcd_write_data(0x05); /* 16-bit/piksel (RGB565) */

    lcd_write_cmd(ST7735_MADCTL);
    lcd_write_data(0xA0); /* Yatay mod (Landscape), RGB sırası */

    lcd_write_cmd(ST7735_DISPON);
    delay_ms(20);

    /* Arka aydınlatmayı aç */
    display_set_backlight(100);

    /* Ekranı temizle */
    display_clear(COLOR_BLACK);
    return 0;
}

void display_set_backlight(uint8_t brightness_pct)
{
    hal_gpio_write(PIN_LCD_BL, brightness_pct > 0);
}

void display_clear(uint16_t color)
{
    for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
        s_framebuffer[i] = color;
    }
}

void display_draw_pixel(int16_t x, int16_t y, uint16_t color)
{
    if (x >= 0 && x < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT) {
        s_framebuffer[y * LCD_WIDTH + x] = color;
    }
}

void display_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    int16_t x2 = x + w;
    int16_t y2 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 > LCD_WIDTH)  x2 = LCD_WIDTH;
    if (y2 > LCD_HEIGHT) y2 = LCD_HEIGHT;

    for (int16_t cy = y; cy < y2; cy++) {
        for (int16_t cx = x; cx < x2; cx++) {
            s_framebuffer[cy * LCD_WIDTH + cx] = color;
        }
    }
}

void display_draw_string(int16_t x, int16_t y, const char *str, uint16_t fg, uint16_t bg, uint8_t size)
{
    int16_t cur_x = x;
    while (*str) {
        char c = *str++;
        if (c >= 'a' && c <= 'z') c -= 32; /* Büyük harfe çevir */

        if (c >= 32 && c <= 90) {
            uint16_t glyph_idx = (c - 32) * 5;
            for (uint8_t col = 0; col < 5; col++) {
                uint8_t line = s_font5x7[glyph_idx + col];
                for (uint8_t row = 0; row < 7; row++) {
                    uint16_t color = (line & (1 << row)) ? fg : bg;
                    if (size == 1) {
                        display_draw_pixel(cur_x + col, y + row, color);
                    } else {
                        display_fill_rect(cur_x + col * size, y + row * size, size, size, color);
                    }
                }
            }
        }
        cur_x += (6 * size);
    }
}

void display_flush(void)
{
    /* Adres penceresi tanımla: (0,0) ile (159,127) */
    lcd_write_cmd(ST7735_CASET);
    lcd_write_data(0x00);
    lcd_write_data(0x00);
    lcd_write_data(0x00);
    lcd_write_data(LCD_WIDTH - 1);

    lcd_write_cmd(ST7735_RASET);
    lcd_write_data(0x00);
    lcd_write_data(0x00);
    lcd_write_data(0x00);
    lcd_write_data(LCD_HEIGHT - 1);

    /* Framebuffer'ı LCD RAM'ine yaz */
    lcd_write_cmd(ST7735_RAMWR);

    hal_gpio_set(PIN_LCD_DC);   /* DC = 1 (Veri) */
    hal_gpio_clear(PIN_LCD_CS); /* CS = 0 */

    /* SPI ile tüm framebuffer'ı aktar */
    hal_spi_transmit(HAL_SPI_0, (const uint8_t *)s_framebuffer, sizeof(s_framebuffer));

    hal_gpio_set(PIN_LCD_CS);   /* CS = 1 */
}
