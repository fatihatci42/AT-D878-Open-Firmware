/**
 * @file    hal_uart.c
 * @brief   UART Hardware Abstraction Layer — AT-D878 Open Firmware
 * @version 0.1.0
 *
 * GD32F303VGT6 USART/UART register-seviye sürücüsü.
 *
 * Kanal Atamaları (tahmini, doğrulanacak):
 *   USART0 (PA9/PA10)  → USB/Debug seri
 *   USART1 (PA2/PA3)   → GPS modülü (9600 baud, NMEA)
 *   USART2 (PB10/PB11) → Bluetooth LinkBoard (115200 baud)
 *   UART3  (PC10/PC11) → APRS TNC / Genişleme
 *   UART4  (PC12/PD2)  → Yedek
 */

#include "hal_uart.h"
#include "hal_systick.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/* ============================================================
 * GD32F303 USART Register Tanımları
 * ============================================================ */

/* USART base adresleri */
static const uint32_t USART_BASE[] = {
    0x40013800U,  /* USART0 (APB2) */
    0x40004400U,  /* USART1 (APB1) */
    0x40004800U,  /* USART2 (APB1) */
    0x40004C00U,  /* UART3  (APB1) */
    0x40005000U,  /* UART4  (APB1) */
};

/* Register offset'leri */
#define USART_STAT0       0x00U   /* Status register 0 */
#define USART_DATA        0x04U   /* Data register */
#define USART_BAUD        0x08U   /* Baud rate register */
#define USART_CTL0        0x0CU   /* Control register 0 */
#define USART_CTL1        0x10U   /* Control register 1 */
#define USART_CTL2        0x14U   /* Control register 2 */
#define USART_GP          0x18U   /* Guard time / prescaler */

/* STAT0 bitleri */
#define USART_STAT0_RBNE  (1U << 5)   /* RX buffer not empty */
#define USART_STAT0_TC    (1U << 6)   /* Transmission complete */
#define USART_STAT0_TBE   (1U << 7)   /* TX buffer empty */
#define USART_STAT0_ORE   (1U << 3)   /* Overrun error */

/* CTL0 bitleri */
#define USART_CTL0_UEN    (1U << 13)  /* USART enable */
#define USART_CTL0_TEN    (1U << 3)   /* Transmit enable */
#define USART_CTL0_REN    (1U << 2)   /* Receive enable */
#define USART_CTL0_RBNEIE (1U << 5)   /* RBNE interrupt enable */
#define USART_CTL0_WL     (1U << 12)  /* Word length (0=8bit, 1=9bit) */
#define USART_CTL0_PCEN   (1U << 10)  /* Parity enable */
#define USART_CTL0_PM     (1U << 9)   /* Parity mode (0=even, 1=odd) */

/* CTL1 bitleri */
#define USART_CTL1_STB_SHIFT  12      /* Stop bits */

/* Register erişim makroları */
#define USART_REG(port, off)  (*(volatile uint32_t *)(USART_BASE[port] + (off)))

/* ============================================================
 * Ring Buffer (Kesme tabanlı RX)
 * ============================================================ */

#define RX_RING_SIZE   256   /* Her kanal için RX tamponu */

typedef struct {
    uint8_t  buf[RX_RING_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} ring_buf_t;

/* ============================================================
 * Statik Değişkenler
 * ============================================================ */

static ring_buf_t s_rx_ring[HAL_UART_COUNT];
static hal_uart_rx_callback_t s_rx_callback[HAL_UART_COUNT];
static void *s_rx_context[HAL_UART_COUNT];
static bool s_initialized[HAL_UART_COUNT];

/* ============================================================
 * Ring Buffer Yardımcıları
 * ============================================================ */

static inline void ring_put(ring_buf_t *r, uint8_t byte)
{
    uint16_t next = (r->head + 1) % RX_RING_SIZE;
    if (next != r->tail) {
        r->buf[r->head] = byte;
        r->head = next;
    }
    /* Taşma durumunda en eski veri kaybedilir */
}

static inline bool ring_get(ring_buf_t *r, uint8_t *byte)
{
    if (r->head == r->tail) return false;
    *byte = r->buf[r->tail];
    r->tail = (r->tail + 1) % RX_RING_SIZE;
    return true;
}

static inline size_t ring_available(const ring_buf_t *r)
{
    return (RX_RING_SIZE + r->head - r->tail) % RX_RING_SIZE;
}

/* ============================================================
 * NVIC Yardımcıları
 * ============================================================ */

/* GD32F303 USART IRQ numaraları */
static const uint8_t USART_IRQ_NUM[] = {
    37,  /* USART0_IRQn */
    38,  /* USART1_IRQn */
    39,  /* USART2_IRQn */
    52,  /* UART3_IRQn */
    53,  /* UART4_IRQn */
};

#define NVIC_ISER(n)  (*(volatile uint32_t *)(0xE000E100U + 4U * ((n) / 32U)))
#define NVIC_ICER(n)  (*(volatile uint32_t *)(0xE000E180U + 4U * ((n) / 32U)))

static inline void nvic_enable_irq(uint8_t irqn)
{
    NVIC_ISER(irqn) = 1U << (irqn % 32U);
}

/* ============================================================
 * API İmplementasyonu
 * ============================================================ */

int hal_uart_init(hal_uart_port_t port, const hal_uart_config_t *config)
{
    if (port >= HAL_UART_COUNT || !config) return -1;

    /* Ring buffer sıfırla */
    memset(&s_rx_ring[port], 0, sizeof(ring_buf_t));
    s_rx_callback[port] = NULL;
    s_rx_context[port]  = NULL;

    /* USART'ı devre dışı bırak */
    USART_REG(port, USART_CTL0) &= ~USART_CTL0_UEN;

    /* Baud rate hesapla */
    uint32_t apb_clk;
    if (port == HAL_UART_0) {
        apb_clk = APB2_CLOCK_HZ;  /* USART0: APB2 */
    } else {
        apb_clk = APB1_CLOCK_HZ;  /* Diğerleri: APB1 */
    }

    /* USARTDIV = APB_CLK / (16 × BAUD)
     * Register = mantissa << 4 | fraction */
    uint32_t divider = (apb_clk + config->baudrate / 2U) / config->baudrate;
    USART_REG(port, USART_BAUD) = divider;

    /* CTL0: Word length, parity */
    uint32_t ctl0 = USART_CTL0_TEN | USART_CTL0_REN | USART_CTL0_RBNEIE;
    if (config->parity != HAL_UART_PARITY_NONE) {
        ctl0 |= USART_CTL0_PCEN;
        ctl0 |= USART_CTL0_WL;  /* 9-bit (8 data + parity) */
        if (config->parity == HAL_UART_PARITY_ODD) {
            ctl0 |= USART_CTL0_PM;
        }
    }
    USART_REG(port, USART_CTL0) = ctl0;

    /* CTL1: Stop bits */
    uint32_t ctl1 = USART_REG(port, USART_CTL1);
    ctl1 &= ~(0x3U << USART_CTL1_STB_SHIFT);
    ctl1 |= ((uint32_t)config->stopbits & 0x3U) << USART_CTL1_STB_SHIFT;
    USART_REG(port, USART_CTL1) = ctl1;

    /* NVIC: USART kesmesini etkinleştir */
    nvic_enable_irq(USART_IRQ_NUM[port]);

    /* USART etkinleştir */
    USART_REG(port, USART_CTL0) |= USART_CTL0_UEN;

    s_initialized[port] = true;
    return 0;
}

void hal_uart_deinit(hal_uart_port_t port)
{
    if (port >= HAL_UART_COUNT) return;
    USART_REG(port, USART_CTL0) &= ~USART_CTL0_UEN;
    s_initialized[port] = false;
}

int hal_uart_transmit(hal_uart_port_t port, const uint8_t *data, size_t len)
{
    if (port >= HAL_UART_COUNT || !s_initialized[port]) return -1;

    for (size_t i = 0; i < len; i++) {
        /* TBE bayrağını bekle */
        uint32_t timeout = 0x10000U;
        while (!(USART_REG(port, USART_STAT0) & USART_STAT0_TBE)) {
            if (--timeout == 0) return -2;
        }
        USART_REG(port, USART_DATA) = data[i];
    }

    /* Son byte'ın tamamlanmasını bekle */
    uint32_t timeout = 0x10000U;
    while (!(USART_REG(port, USART_STAT0) & USART_STAT0_TC)) {
        if (--timeout == 0) return -2;
    }

    return (int)len;
}

int hal_uart_printf(hal_uart_port_t port, const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n > 0) {
        hal_uart_transmit(port, (const uint8_t *)buf, (size_t)n);
    }
    return n;
}

int hal_uart_receive(hal_uart_port_t port, uint8_t *data,
                     size_t len, uint32_t timeout_ms)
{
    if (port >= HAL_UART_COUNT || !s_initialized[port]) return -1;

    tick_t start = hal_tick_get();
    size_t received = 0;

    while (received < len) {
        uint8_t byte;
        if (ring_get(&s_rx_ring[port], &byte)) {
            data[received++] = byte;
        } else {
            if (hal_tick_expired(start, timeout_ms)) break;
        }
    }

    return (int)received;
}

int hal_uart_set_rx_callback(hal_uart_port_t port,
                             hal_uart_rx_callback_t callback,
                             void *context)
{
    if (port >= HAL_UART_COUNT) return -1;
    s_rx_callback[port] = callback;
    s_rx_context[port]  = context;
    return 0;
}

size_t hal_uart_available(hal_uart_port_t port)
{
    if (port >= HAL_UART_COUNT) return 0;
    return ring_available(&s_rx_ring[port]);
}

void hal_uart_flush(hal_uart_port_t port)
{
    if (port >= HAL_UART_COUNT) return;
    s_rx_ring[port].head = 0;
    s_rx_ring[port].tail = 0;
}

/* ============================================================
 * USART Kesme İşleyicileri
 * ============================================================ */

static void usart_irq_handler(hal_uart_port_t port)
{
    uint32_t stat = USART_REG(port, USART_STAT0);

    if (stat & USART_STAT0_RBNE) {
        uint8_t byte = (uint8_t)(USART_REG(port, USART_DATA) & 0xFFU);
        ring_put(&s_rx_ring[port], byte);

        /* Callback varsa çağır */
        if (s_rx_callback[port]) {
            s_rx_callback[port](port, &byte, 1, s_rx_context[port]);
        }
    }

    /* Overrun hatasını temizle */
    if (stat & USART_STAT0_ORE) {
        (void)USART_REG(port, USART_DATA);
    }
}

void USART0_IRQHandler(void) { usart_irq_handler(HAL_UART_0); }
void USART1_IRQHandler(void) { usart_irq_handler(HAL_UART_1); }
void USART2_IRQHandler(void) { usart_irq_handler(HAL_UART_2); }
void UART3_IRQHandler(void)  { usart_irq_handler(HAL_UART_3); }
void UART4_IRQHandler(void)  { usart_irq_handler(HAL_UART_4); }
