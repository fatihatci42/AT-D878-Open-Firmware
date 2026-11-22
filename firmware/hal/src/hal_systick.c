/**
 * @file    hal_systick.c
 * @brief   SysTick Timer & RCC Clock Setup — AT-D878 Open Firmware
 * @version 0.1.0
 *
 * GD32F303VGT6 saat ağacını yapılandırır:
 *   HSE 8 MHz → PLL ×15 → SYSCLK 120 MHz
 *   AHB /1 = 120 MHz, APB1 /2 = 60 MHz, APB2 /1 = 120 MHz
 * SysTick 1 ms periyotla çalışır.
 */

#include "hal_systick.h"
#include <string.h>

/* ============================================================
 * GD32F303 Register Adresleri
 * ============================================================ */

/* RCU (Reset and Clock Unit) */
#define RCU_BASE              0x40021000U
#define RCU_CTL               (*(volatile uint32_t *)(RCU_BASE + 0x00U))
#define RCU_CFG0              (*(volatile uint32_t *)(RCU_BASE + 0x04U))
#define RCU_INT               (*(volatile uint32_t *)(RCU_BASE + 0x08U))
#define RCU_APB2EN            (*(volatile uint32_t *)(RCU_BASE + 0x18U))
#define RCU_APB1EN            (*(volatile uint32_t *)(RCU_BASE + 0x1CU))
#define RCU_AHBEN             (*(volatile uint32_t *)(RCU_BASE + 0x14U))
#define RCU_CFG1              (*(volatile uint32_t *)(RCU_BASE + 0x2CU))

/* RCU_CTL bitleri */
#define RCU_CTL_HXTALEN       (1U << 16)   /* HSE osilatör etkinleştir */
#define RCU_CTL_HXTALSTB      (1U << 17)   /* HSE hazır bayrağı */
#define RCU_CTL_PLLEN         (1U << 24)   /* PLL etkinleştir */
#define RCU_CTL_PLLSTB        (1U << 25)   /* PLL kilit bayrağı */

/* RCU_CFG0 bitleri */
#define RCU_CFG0_SCS_PLL      (0x2U << 0)  /* Sistem saati: PLL */
#define RCU_CFG0_SCSS_PLL     (0x2U << 2)  /* SWS: PLL aktif */
#define RCU_CFG0_AHBPSC_1     (0x0U << 4)  /* AHB bölücü = /1 */
#define RCU_CFG0_APB1PSC_2    (0x4U << 8)  /* APB1 bölücü = /2 */
#define RCU_CFG0_APB2PSC_1    (0x0U << 11) /* APB2 bölücü = /1 */
#define RCU_CFG0_PLLSEL_HXTAL (1U << 16)   /* PLL kaynağı: HSE */
#define RCU_CFG0_PLLMF_SHIFT  18           /* PLL çarpanı bit pozisyonu */

/* FLASH (Wait States) */
#define FMC_BASE              0x40022000U
#define FMC_WS                (*(volatile uint32_t *)(FMC_BASE + 0x00U))
#define FMC_WS_WSCNT_3WS     0x03U        /* 3 wait state (96-120 MHz) */

/* SysTick (Cortex-M4 Core) */
#define SYST_CSR              (*(volatile uint32_t *)0xE000E010U)
#define SYST_RVR              (*(volatile uint32_t *)0xE000E014U)
#define SYST_CVR              (*(volatile uint32_t *)0xE000E018U)
#define SYST_CALIB            (*(volatile uint32_t *)0xE000E01CU)

#define SYST_CSR_ENABLE       (1U << 0)
#define SYST_CSR_TICKINT      (1U << 1)
#define SYST_CSR_CLKSOURCE    (1U << 2)   /* 1 = AHB, 0 = AHB/8 */

/* NVIC */
#define SCB_VTOR              (*(volatile uint32_t *)0xE000ED08U)

/* ============================================================
 * Statik Değişkenler
 * ============================================================ */

static volatile tick_t s_tick_counter = 0;
static soft_timer_t s_timers[SOFT_TIMER_MAX];
static bool s_systick_initialized = false;

/* ============================================================
 * RCC Saat Yapılandırması
 * ============================================================ */

/**
 * @brief GD32F303 saat ağacını HSE → PLL → 120 MHz olarak yapılandır
 */
static int rcc_clock_setup(void)
{
    uint32_t timeout;

    /* 1. Flash wait state ayarla (120 MHz için 3WS gerekli) */
    FMC_WS = (FMC_WS & ~0x07U) | FMC_WS_WSCNT_3WS;

    /* 2. HSE osilatörü etkinleştir */
    RCU_CTL |= RCU_CTL_HXTALEN;

    /* HSE stabilize olmasını bekle (timeout ~1 saniye) */
    timeout = 0x100000U;
    while (!(RCU_CTL & RCU_CTL_HXTALSTB)) {
        if (--timeout == 0) return -1;  /* HSE başlatılamadı */
    }

    /* 3. PLL yapılandır: HSE × 15 = 120 MHz */
    /* Önce PLL'i kapat */
    RCU_CTL &= ~RCU_CTL_PLLEN;

    /* PLL çarpanı ve kaynak ayarla */
    uint32_t cfg0 = RCU_CFG0;
    cfg0 &= ~(0xFU << RCU_CFG0_PLLMF_SHIFT);     /* PLLMF temizle */
    cfg0 &= ~(1U << 16);                           /* PLLSEL temizle */
    cfg0 &= ~(1U << 17);                           /* PLLPREDV temizle */
    cfg0 &= ~(0xFU << 4);                          /* AHB prescaler temizle */
    cfg0 &= ~(0x7U << 8);                          /* APB1 prescaler temizle */
    cfg0 &= ~(0x7U << 11);                         /* APB2 prescaler temizle */

    cfg0 |= ((PLL_MULTIPLIER - 2U) & 0xFU) << RCU_CFG0_PLLMF_SHIFT; /* ×15 → 0x0D */
    cfg0 |= RCU_CFG0_PLLSEL_HXTAL;                 /* PLL kaynağı: HSE */
    cfg0 |= RCU_CFG0_AHBPSC_1;                      /* AHB /1 */
    cfg0 |= RCU_CFG0_APB1PSC_2;                    /* APB1 /2 */
    cfg0 |= RCU_CFG0_APB2PSC_1;                    /* APB2 /1 */
    RCU_CFG0 = cfg0;

    /* PLL çarpanı bit 29 (GD32F303 genişletme) → 0 (×15 ≤ 16, gerek yok) */
    RCU_CFG1 &= ~(1U << 31);

    /* 4. PLL'i etkinleştir */
    RCU_CTL |= RCU_CTL_PLLEN;

    timeout = 0x100000U;
    while (!(RCU_CTL & RCU_CTL_PLLSTB)) {
        if (--timeout == 0) return -2;  /* PLL kilitlenemedi */
    }

    /* 5. Sistem saatini PLL'e geçir */
    cfg0 = RCU_CFG0;
    cfg0 &= ~(0x3U << 0);  /* SCS temizle */
    cfg0 |= RCU_CFG0_SCS_PLL;
    RCU_CFG0 = cfg0;

    /* PLL'in aktif olmasını doğrula */
    timeout = 0x100000U;
    while ((RCU_CFG0 & (0x3U << 2)) != RCU_CFG0_SCSS_PLL) {
        if (--timeout == 0) return -2;
    }

    /* 6. GPIO port saatlerini etkinleştir (GPIOA-E) */
    RCU_APB2EN |= (1U << 2)   /* GPIOA */
                | (1U << 3)   /* GPIOB */
                | (1U << 4)   /* GPIOC */
                | (1U << 5)   /* GPIOD */
                | (1U << 6)   /* GPIOE */
                | (1U << 0)   /* AFIO (Alternate Function) */
                | (1U << 14); /* USART0 (APB2) */

    /* APB1 çevre birim saatleri */
    RCU_APB1EN |= (1U << 17)  /* USART1 */
                | (1U << 18)  /* USART2 */
                | (1U << 19)  /* UART3 */
                | (1U << 20)  /* UART4 */
                | (1U << 14)  /* SPI1 */
                | (1U << 15)  /* SPI2 */
                | (1U << 21)  /* I2C0 */
                | (1U << 22)  /* I2C1 */
                | (1U << 1)   /* TIM1 (Timer1 / APB1) */
                | (1U << 2)   /* TIM2 */
                | (1U << 4);  /* TIM5 */

    /* AHB DMA saat */
    RCU_AHBEN |= (1U << 0)   /* DMA0 */
               | (1U << 1);  /* DMA1 */

    return 0;
}

/* ============================================================
 * SysTick Yapılandırması
 * ============================================================ */

int hal_systick_init(void)
{
    int rc;

    /* Vektör tablosu offset: 0x08004000 (bootloader sonrası) */
    SCB_VTOR = 0x08004000U;

    /* Saat ağacını yapılandır */
    rc = rcc_clock_setup();
    if (rc != 0) return rc;

    /* Yazılım zamanlayıcıları sıfırla */
    memset(s_timers, 0, sizeof(s_timers));

    /* SysTick: AHB saati / (SYSTEM_CLOCK_HZ / 1000) = 1 ms periyot */
    SYST_RVR = (SYSTEM_CLOCK_HZ / 1000U) - 1U;  /* 119999 */
    SYST_CVR = 0U;
    SYST_CSR = SYST_CSR_ENABLE | SYST_CSR_TICKINT | SYST_CSR_CLKSOURCE;

    s_systick_initialized = true;
    return 0;
}

tick_t hal_tick_get(void)
{
    return s_tick_counter;
}

void hal_delay_ms(uint32_t ms)
{
    tick_t start = hal_tick_get();
    while (hal_tick_elapsed(start, hal_tick_get()) < ms) {
        __asm__ volatile ("nop");
    }
}

void hal_delay_us(uint32_t us)
{
    /* 120 MHz → yaklaşık 30 döngü/µs (4 instruction/loop) */
    volatile uint32_t count = us * 30U;
    while (count--) {
        __asm__ volatile ("nop");
    }
}

/* ============================================================
 * SysTick Kesme İşleyicisi
 * ============================================================ */

void SysTick_Handler(void)
{
    s_tick_counter++;
}

/* ============================================================
 * Yazılım Zamanlayıcı Implementasyonu
 * ============================================================ */

timer_handle_t hal_timer_create(uint32_t period_ms, bool repeat,
                                soft_timer_callback_t callback,
                                void *context)
{
    for (int i = 0; i < SOFT_TIMER_MAX; i++) {
        if (!s_timers[i].active && !s_timers[i].callback) {
            s_timers[i].period_ms = period_ms;
            s_timers[i].repeat    = repeat;
            s_timers[i].callback  = callback;
            s_timers[i].context   = context;
            s_timers[i].active    = false;
            s_timers[i].next_fire = 0;
            return (timer_handle_t)i;
        }
    }
    return TIMER_INVALID;
}

int hal_timer_start(timer_handle_t handle)
{
    if (handle < 0 || handle >= SOFT_TIMER_MAX) return -1;
    s_timers[handle].next_fire = hal_tick_get() + s_timers[handle].period_ms;
    s_timers[handle].active = true;
    return 0;
}

int hal_timer_stop(timer_handle_t handle)
{
    if (handle < 0 || handle >= SOFT_TIMER_MAX) return -1;
    s_timers[handle].active = false;
    return 0;
}

int hal_timer_delete(timer_handle_t handle)
{
    if (handle < 0 || handle >= SOFT_TIMER_MAX) return -1;
    memset(&s_timers[handle], 0, sizeof(soft_timer_t));
    return 0;
}

void hal_timers_process(void)
{
    tick_t now = hal_tick_get();
    for (int i = 0; i < SOFT_TIMER_MAX; i++) {
        if (s_timers[i].active && s_timers[i].callback) {
            if ((int32_t)(now - s_timers[i].next_fire) >= 0) {
                s_timers[i].callback(s_timers[i].context);
                if (s_timers[i].repeat) {
                    s_timers[i].next_fire += s_timers[i].period_ms;
                } else {
                    s_timers[i].active = false;
                }
            }
        }
    }
}
