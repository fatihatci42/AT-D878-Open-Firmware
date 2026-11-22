/**
 * @file    hal_gpio.c
 * @brief   GPIO Hardware Abstraction Layer Implementation - AT-D878 Open Firmware
 * @version 0.0.1
 *
 * GD32F303VGT6 doğrudan register eşlemeli GPIO sürücü gerçekleştirmesi.
 */

#include "hal_gpio.h"

/* GD32F303 Port Taban Adresleri */
#define GPIOA_BASE   0x40010800UL
#define GPIOB_BASE   0x40010C00UL
#define GPIOC_BASE   0x40011000UL
#define GPIOD_BASE   0x40011400UL
#define GPIOE_BASE   0x40011800UL

/* Register Ofsetleri */
#define GPIO_CTL0_OFFSET  0x00UL  /* Pin 0-7 Konfigürasyon */
#define GPIO_CTL1_OFFSET  0x04UL  /* Pin 8-15 Konfigürasyon */
#define GPIO_ISTAT_OFFSET 0x08UL  /* Giriş Veri Durumu */
#define GPIO_OCTL_OFFSET  0x0CUL  /* Çıkış Veri Kontrol */
#define GPIO_BOP_OFFSET   0x10UL  /* Bit Operasyon (Set/Reset) */
#define GPIO_BC_OFFSET    0x14UL  /* Bit Temizle (Clear) */

/* RCU (Reset & Clock Unit) Register */
#define RCU_BASE          0x40021000UL
#define RCU_APB2EN        (*(volatile uint32_t *)(RCU_BASE + 0x18))
#define RCU_APB2EN_PAEN   (1UL << 2)
#define RCU_APB2EN_PBEN   (1UL << 3)
#define RCU_APB2EN_PCEN   (1UL << 4)
#define RCU_APB2EN_PDEN   (1UL << 5)
#define RCU_APB2EN_PEEN   (1UL << 6)
#define RCU_APB2EN_AFEN   (1UL << 0)

static uint32_t get_port_base(hal_gpio_port_t port)
{
    switch (port) {
        case GPIO_PORT_A: return GPIOA_BASE;
        case GPIO_PORT_B: return GPIOB_BASE;
        case GPIO_PORT_C: return GPIOC_BASE;
        case GPIO_PORT_D: return GPIOD_BASE;
        case GPIO_PORT_E: return GPIOE_BASE;
        default: return 0;
    }
}

int hal_gpio_init(void)
{
    /* Tüm GPIO port saatlerini (RCU) etkinleştir */
    RCU_APB2EN |= (RCU_APB2EN_PAEN | RCU_APB2EN_PBEN | RCU_APB2EN_PCEN |
                   RCU_APB2EN_PDEN | RCU_APB2EN_PEEN | RCU_APB2EN_AFEN);
    return 0;
}

int hal_gpio_configure(hal_gpio_id_t id, hal_gpio_mode_t mode, hal_gpio_speed_t speed)
{
    uint32_t base = get_port_base(id.port);
    if (!base || id.pin > 15) return -1;

    volatile uint32_t *ctl_reg = (id.pin < 8) ?
        (volatile uint32_t *)(base + GPIO_CTL0_OFFSET) :
        (volatile uint32_t *)(base + GPIO_CTL1_OFFSET);

    uint32_t pin_shift = (id.pin % 8) * 4;

    /* 4-bitlik konfigürasyon alanı: [MD1:MD0:CTL1:CTL0] */
    uint32_t conf_bits = 0;
    if ((mode & 0x10) != 0) {
        /* Çıkış modu: Hız bits [1:0] + Mode bits [3:2] */
        conf_bits = ((mode & 0x0C) | (speed & 0x03));
    } else {
        /* Giriş modu: Hız 00 + Mode bits */
        conf_bits = (mode & 0x0C);
    }

    /* Register'ı güncelle */
    uint32_t reg_val = *ctl_reg;
    reg_val &= ~(0xFUL << pin_shift);
    reg_val |= (conf_bits << pin_shift);
    *ctl_reg = reg_val;

    /* Pull-Up / Pull-Down ayarı (OCTL register üzerinden) */
    if (mode == GPIO_MODE_INPUT_PULLUP) {
        hal_gpio_set(id);
    } else if (mode == GPIO_MODE_INPUT_PULLDOWN) {
        hal_gpio_clear(id);
    }

    return 0;
}

void hal_gpio_set(hal_gpio_id_t id)
{
    uint32_t base = get_port_base(id.port);
    if (base && id.pin <= 15) {
        *(volatile uint32_t *)(base + GPIO_BOP_OFFSET) = (1UL << id.pin);
    }
}

void hal_gpio_clear(hal_gpio_id_t id)
{
    uint32_t base = get_port_base(id.port);
    if (base && id.pin <= 15) {
        *(volatile uint32_t *)(base + GPIO_BC_OFFSET) = (1UL << id.pin);
    }
}

void hal_gpio_toggle(hal_gpio_id_t id)
{
    uint32_t base = get_port_base(id.port);
    if (base && id.pin <= 15) {
        volatile uint32_t *octl = (volatile uint32_t *)(base + GPIO_OCTL_OFFSET);
        *octl ^= (1UL << id.pin);
    }
}

void hal_gpio_write(hal_gpio_id_t id, bool value)
{
    if (value) {
        hal_gpio_set(id);
    } else {
        hal_gpio_clear(id);
    }
}

bool hal_gpio_read(hal_gpio_id_t id)
{
    uint32_t base = get_port_base(id.port);
    if (!base || id.pin > 15) return false;
    uint32_t istat = *(volatile uint32_t *)(base + GPIO_ISTAT_OFFSET);
    return (istat & (1UL << id.pin)) != 0;
}
