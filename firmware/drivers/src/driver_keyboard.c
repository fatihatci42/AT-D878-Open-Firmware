/**
 * @file    driver_keyboard.c
 * @brief   Full Keypad Matrix, Side Keys, PTT & Rotary Encoder Driver
 * @version 0.1.0
 *
 * AT-D878UVII Plus tuş taraması:
 *
 * Matrix Klavye (4×4 + ekstra satır):
 *   Satır çıkışları (ROW, Output Push-Pull):
 *     PA4, PA5, PA6, PA8, PA11
 *   Sütun girişleri (COL, Input Pull-Up):
 *     PD0, PD1, PD3, PD4
 *
 *   ROW\COL    PD0      PD1      PD3      PD4
 *   PA4        1        2        3        MENU
 *   PA5        4        5        6        UP
 *   PA6        7        8        9        DOWN
 *   PA8        *        0        #        EXIT
 *   PA11       PF1      PF2      PF3      (boş)
 *
 * Bağımsız tuşlar:
 *   PA0  = PTT          (Active LOW, dahili pull-up)
 *   PA15 = Rotary A     (Encoder Phase A)
 *   PB3  = Rotary B     (Encoder Phase B)
 *   PB0  = Rotary Press (Encoder buton)
 *
 * NOT: Bu pin atamaları stock firmware GPIO analizine dayalı
 *      tahminlerdir. Donanım teardown ile doğrulanacaktır.
 *
 * Debounce: 20 ms yazılım debounce + SysTick polling
 * Long press: 500 ms basılı tutma → KEY_EVENT_HOLD
 *             2000 ms → KEY_EVENT_LONG_HOLD
 */

#include "driver_keyboard.h"
#include "hal_gpio.h"
#include "hal_systick.h"
#include <string.h>

/* ============================================================
 * Matrix Tanımları
 * ============================================================ */

#define MATRIX_ROWS         5
#define MATRIX_COLS         4
#define TOTAL_KEYS          (MATRIX_ROWS * MATRIX_COLS + 4) /* matrix + PTT + RotA/B/Press */

/* Debounce parametreleri */
#define DEBOUNCE_MS         20
#define LONG_PRESS_MS       500
#define VERY_LONG_PRESS_MS  2000

/* Olay kuyruğu */
#define EVENT_QUEUE_SIZE    16

/* Satır (çıkış) pinleri */
static const hal_gpio_id_t ROW_PINS[MATRIX_ROWS] = {
    {GPIO_PORT_A,  4},  /* ROW0 */
    {GPIO_PORT_A,  5},  /* ROW1 */
    {GPIO_PORT_A,  6},  /* ROW2 */
    {GPIO_PORT_A,  8},  /* ROW3 */
    {GPIO_PORT_A, 11},  /* ROW4 */
};

/* Sütun (giriş) pinleri */
static const hal_gpio_id_t COL_PINS[MATRIX_COLS] = {
    {GPIO_PORT_D, 0},   /* COL0 */
    {GPIO_PORT_D, 1},   /* COL1 */
    {GPIO_PORT_D, 3},   /* COL2 */
    {GPIO_PORT_D, 4},   /* COL3 */
};

/* Matrix → tuş kodu eşleme tablosu */
static const key_code_t KEY_MAP[MATRIX_ROWS][MATRIX_COLS] = {
    /* COL0     COL1     COL2     COL3   */
    { KEY_1,   KEY_2,   KEY_3,   KEY_MENU },   /* ROW0 (PA4)  */
    { KEY_4,   KEY_5,   KEY_6,   KEY_UP   },   /* ROW1 (PA5)  */
    { KEY_7,   KEY_8,   KEY_9,   KEY_DOWN },   /* ROW2 (PA6)  */
    { KEY_STAR,KEY_0,   KEY_HASH,KEY_EXIT },   /* ROW3 (PA8)  */
    { KEY_SIDE1,KEY_SIDE2,KEY_TOP,KEY_NONE },  /* ROW4 (PA11) */
};

/* Bağımsız tuş pinleri */
#define PIN_ROTARY_A    ((hal_gpio_id_t){GPIO_PORT_A, 15})
#define PIN_ROTARY_B    ((hal_gpio_id_t){GPIO_PORT_B,  3})
#define PIN_ROTARY_BTN  ((hal_gpio_id_t){GPIO_PORT_B,  0})

/* ============================================================
 * İç Durum Yapıları
 * ============================================================ */

typedef struct {
    bool     pressed;           /* Şu an basılı mı */
    bool     debounce_active;   /* Debounce bekleniyor */
    tick_t   debounce_start;    /* Debounce başlangıcı */
    tick_t   press_start;       /* Basılma zamanı */
    bool     hold_sent;         /* HOLD olayı gönderildi mi */
    bool     long_hold_sent;    /* LONG_HOLD olayı gönderildi mi */
} key_state_t;

/* Her matrix pozisyonu + 4 bağımsız tuş için durum */
static key_state_t s_key_state[TOTAL_KEYS];

/* Olay kuyruğu */
static key_event_t s_event_queue[EVENT_QUEUE_SIZE];
static volatile uint8_t s_queue_head = 0;
static volatile uint8_t s_queue_tail = 0;

/* Encoder son durum */
static uint8_t s_encoder_last_ab = 0;

/* Zamanlayıcı handle */
static timer_handle_t s_scan_timer = TIMER_INVALID;

/* ============================================================
 * Olay Kuyruğu Yardımcıları
 * ============================================================ */

static void enqueue_event(key_code_t code, key_event_type_t event)
{
    uint8_t next = (s_queue_head + 1) % EVENT_QUEUE_SIZE;
    if (next == s_queue_tail) return; /* Kuyruk dolu */

    s_event_queue[s_queue_head].code         = code;
    s_event_queue[s_queue_head].event        = event;
    s_event_queue[s_queue_head].timestamp_ms = hal_tick_get();
    s_queue_head = next;
}

/* ============================================================
 * Tuş Durumu Güncelle
 * ============================================================ */

static void update_key(uint8_t idx, key_code_t code, bool raw_pressed)
{
    key_state_t *ks = &s_key_state[idx];
    tick_t now = hal_tick_get();

    if (raw_pressed != ks->pressed) {
        /* Durum değişikliği — debounce başlat */
        if (!ks->debounce_active) {
            ks->debounce_active = true;
            ks->debounce_start = now;
        } else if (hal_tick_elapsed(ks->debounce_start, now) >= DEBOUNCE_MS) {
            /* Debounce süresi geçti → durumu güncelle */
            ks->debounce_active = false;
            ks->pressed = raw_pressed;

            if (raw_pressed) {
                ks->press_start = now;
                ks->hold_sent = false;
                ks->long_hold_sent = false;
                enqueue_event(code, KEY_EVENT_PRESS);
            } else {
                enqueue_event(code, KEY_EVENT_RELEASE);
            }
        }
    } else {
        ks->debounce_active = false;

        /* Basılı tutma kontrolü */
        if (ks->pressed) {
            uint32_t held = hal_tick_elapsed(ks->press_start, now);
            if (held >= VERY_LONG_PRESS_MS && !ks->long_hold_sent) {
                ks->long_hold_sent = true;
                enqueue_event(code, KEY_EVENT_LONG_HOLD);
            } else if (held >= LONG_PRESS_MS && !ks->hold_sent) {
                ks->hold_sent = true;
                enqueue_event(code, KEY_EVENT_HOLD);
            }
        }
    }
}

/* ============================================================
 * Rotary Encoder İşleme (Gray Code)
 * ============================================================ */

static void process_encoder(void)
{
    uint8_t a = hal_gpio_read(PIN_ROTARY_A) ? 1 : 0;
    uint8_t b = hal_gpio_read(PIN_ROTARY_B) ? 1 : 0;
    uint8_t ab = (a << 1) | b;

    if (ab != s_encoder_last_ab) {
        /* Gray code döndürme yönü tespiti */
        /* AB geçişleri: 00→01→11→10 = saat yönü (sağ)
         *               00→10→11→01 = saat tersi (sol)  */
        static const int8_t enc_table[4][4] = {
            /*          00  01  10  11  */
            /* 00 */ {  0, +1, -1,  0 },
            /* 01 */ { -1,  0,  0, +1 },
            /* 10 */ { +1,  0,  0, -1 },
            /* 11 */ {  0, -1, +1,  0 },
        };

        int8_t dir = enc_table[s_encoder_last_ab][ab];
        if (dir > 0) {
            enqueue_event(KEY_ROTARY_RIGHT, KEY_EVENT_PRESS);
        } else if (dir < 0) {
            enqueue_event(KEY_ROTARY_LEFT, KEY_EVENT_PRESS);
        }

        s_encoder_last_ab = ab;
    }

    /* Encoder buton */
    bool btn = !hal_gpio_read(PIN_ROTARY_BTN);
    update_key(MATRIX_ROWS * MATRIX_COLS + 3, KEY_ROTARY_PRESS, btn);
}

/* ============================================================
 * Matrix Tarama (5ms periyotla çağrılır)
 * ============================================================ */

static void keyboard_scan(void *context)
{
    (void)context;

    /* 1. Matrix tarama */
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        /* Tüm satırları HIGH yap */
        for (uint8_t r = 0; r < MATRIX_ROWS; r++) {
            hal_gpio_set(ROW_PINS[r]);
        }

        /* Aktif satırı LOW yap */
        hal_gpio_clear(ROW_PINS[row]);

        /* Kısa bekleme (sinyalin oturması için) */
        for (volatile int d = 0; d < 100; d++) { __asm__ volatile("nop"); }

        /* Sütunları oku */
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            key_code_t code = KEY_MAP[row][col];
            if (code == KEY_NONE) continue;

            bool pressed = !hal_gpio_read(COL_PINS[col]); /* Active LOW */
            uint8_t idx = row * MATRIX_COLS + col;
            update_key(idx, code, pressed);
        }
    }

    /* Tüm satırları LOW'a çek (enerji tasarrufu) */
    for (uint8_t r = 0; r < MATRIX_ROWS; r++) {
        hal_gpio_clear(ROW_PINS[r]);
    }

    /* 2. PTT tarama */
    bool ptt = !hal_gpio_read(PIN_PTT);
    update_key(MATRIX_ROWS * MATRIX_COLS, KEY_PTT, ptt);

    /* 3. Rotary encoder */
    process_encoder();
}

/* ============================================================
 * API İmplementasyonu
 * ============================================================ */

int keyboard_init(void)
{
    /* Durum yapılarını sıfırla */
    memset(s_key_state, 0, sizeof(s_key_state));
    s_queue_head = 0;
    s_queue_tail = 0;

    /* Satır pinlerini çıkış olarak yapılandır */
    for (uint8_t r = 0; r < MATRIX_ROWS; r++) {
        hal_gpio_configure(ROW_PINS[r], GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHZ);
        hal_gpio_clear(ROW_PINS[r]);
    }

    /* Sütun pinlerini dahili pull-up giriş olarak yapılandır */
    for (uint8_t c = 0; c < MATRIX_COLS; c++) {
        hal_gpio_configure(COL_PINS[c], GPIO_MODE_INPUT_PULLUP, GPIO_SPEED_50MHZ);
    }

    /* PTT pini (dahili pull-up) */
    hal_gpio_configure(PIN_PTT, GPIO_MODE_INPUT_PULLUP, GPIO_SPEED_50MHZ);

    /* Rotary Encoder pinleri (dahili pull-up) */
    hal_gpio_configure(PIN_ROTARY_A,   GPIO_MODE_INPUT_PULLUP, GPIO_SPEED_50MHZ);
    hal_gpio_configure(PIN_ROTARY_B,   GPIO_MODE_INPUT_PULLUP, GPIO_SPEED_50MHZ);
    hal_gpio_configure(PIN_ROTARY_BTN, GPIO_MODE_INPUT_PULLUP, GPIO_SPEED_50MHZ);

    /* Encoder başlangıç durumu */
    s_encoder_last_ab = (hal_gpio_read(PIN_ROTARY_A) ? 2 : 0)
                      | (hal_gpio_read(PIN_ROTARY_B) ? 1 : 0);

    /* 5ms periyodik tarama zamanlayıcısı oluştur */
    s_scan_timer = hal_timer_create(5, true, keyboard_scan, NULL);
    if (s_scan_timer != TIMER_INVALID) {
        hal_timer_start(s_scan_timer);
    }

    return 0;
}

bool keyboard_get_event(key_event_t *event)
{
    if (!event) return false;

    if (s_queue_head == s_queue_tail) {
        event->code = KEY_NONE;
        return false;
    }

    *event = s_event_queue[s_queue_tail];
    s_queue_tail = (s_queue_tail + 1) % EVENT_QUEUE_SIZE;
    return true;
}

bool keyboard_is_ptt_pressed(void)
{
    return !hal_gpio_read(PIN_PTT);
}
