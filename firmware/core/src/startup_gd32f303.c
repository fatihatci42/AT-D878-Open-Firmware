/**
 * @file    startup_gd32f303.c
 * @brief   Cortex-M4 Startup, Full Vector Table & Minimal Syscalls
 * @version 0.1.0
 *
 * AnyTone AT-D878UVII Plus — GD32F303VGT6
 *
 * Vektör tablosu 0x08004000 adresine yerleşir (bootloader sonrası).
 * GD32F303 tüm 68 IRQ kanalı için handler tanımları içerir.
 * Kullanılmayan handler'lar Default_Handler'a yönlendirilir (weak alias).
 */

#include <stdint.h>
#include <sys/stat.h>

/* ============================================================
 * Linker Sembolları
 * ============================================================ */

extern uint8_t _estack[];
extern uint8_t _sidata[];
extern uint8_t _sdata[];
extern uint8_t _edata[];
extern uint8_t _sbss[];
extern uint8_t _ebss[];

extern int main(void);

/* ============================================================
 * Handler Prototipleri
 * ============================================================ */

void Reset_Handler(void);
void Default_Handler(void);

/* Cortex-M4 Çekirdek Kesmeleri */
void NMI_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void);  /* Özel handler — aşağıda tanımlı */
void MemManage_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)     __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)    __attribute__((weak, alias("Default_Handler")));

/* GD32F303 Çevre Birim Kesmeleri (68 IRQ) */
void WWDGT_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void LVD_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void TAMPER_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void RTC_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void FMC_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void RCU_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void EXTI0_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void EXTI1_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void EXTI2_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void EXTI3_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void EXTI4_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void DMA0_Channel0_IRQHandler(void)__attribute__((weak, alias("Default_Handler")));
void DMA0_Channel1_IRQHandler(void)__attribute__((weak, alias("Default_Handler")));
void DMA0_Channel2_IRQHandler(void)__attribute__((weak, alias("Default_Handler")));
void DMA0_Channel3_IRQHandler(void)__attribute__((weak, alias("Default_Handler")));
void DMA0_Channel4_IRQHandler(void)__attribute__((weak, alias("Default_Handler")));
void DMA0_Channel5_IRQHandler(void)__attribute__((weak, alias("Default_Handler")));
void DMA0_Channel6_IRQHandler(void)__attribute__((weak, alias("Default_Handler")));
void ADC0_1_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void CAN0_TX_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void CAN0_RX0_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void CAN0_RX1_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void CAN0_EWMC_IRQHandler(void)    __attribute__((weak, alias("Default_Handler")));
void EXTI5_9_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void TIMER0_BRK_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));
void TIMER0_UP_IRQHandler(void)    __attribute__((weak, alias("Default_Handler")));
void TIMER0_TRG_CMT_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void TIMER0_Channel_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
void TIMER1_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void TIMER2_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void TIMER3_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void I2C0_EV_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void I2C0_ER_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void I2C1_EV_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void I2C1_ER_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void SPI0_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void SPI1_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void USART0_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void USART1_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void USART2_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void EXTI10_15_IRQHandler(void)    __attribute__((weak, alias("Default_Handler")));
void RTC_Alarm_IRQHandler(void)    __attribute__((weak, alias("Default_Handler")));
void USBFS_WKUP_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));
void TIMER7_BRK_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));
void TIMER7_UP_IRQHandler(void)    __attribute__((weak, alias("Default_Handler")));
void TIMER7_TRG_CMT_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void TIMER7_Channel_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
void ADC2_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void EXMC_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void SDIO_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void TIMER4_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void SPI2_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void UART3_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void UART4_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void TIMER5_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void TIMER6_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void DMA1_Channel0_IRQHandler(void)__attribute__((weak, alias("Default_Handler")));
void DMA1_Channel1_IRQHandler(void)__attribute__((weak, alias("Default_Handler")));
void DMA1_Channel2_IRQHandler(void)__attribute__((weak, alias("Default_Handler")));
void DMA1_Channel3_IRQHandler(void)__attribute__((weak, alias("Default_Handler")));
void DMA1_Channel4_IRQHandler(void)__attribute__((weak, alias("Default_Handler")));
void ENET_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void ENET_WKUP_IRQHandler(void)    __attribute__((weak, alias("Default_Handler")));
void CAN1_TX_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void CAN1_RX0_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void CAN1_RX1_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void CAN1_EWMC_IRQHandler(void)    __attribute__((weak, alias("Default_Handler")));
void USBFS_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));

/* ============================================================
 * Vektör Tablosu — 0x08004000 adresine yerleşir
 * ============================================================ */

__attribute__((section(".isr_vector"), used))
const uint32_t g_pfnVectors[] = {
    /* Cortex-M4 Çekirdek */
    (uint32_t)&_estack,                    /*  0: Initial Stack Pointer */
    (uint32_t)&Reset_Handler,              /*  1: Reset Handler */
    (uint32_t)&NMI_Handler,                /*  2: NMI */
    (uint32_t)&HardFault_Handler,          /*  3: Hard Fault */
    (uint32_t)&MemManage_Handler,          /*  4: MPU Fault */
    (uint32_t)&BusFault_Handler,           /*  5: Bus Fault */
    (uint32_t)&UsageFault_Handler,         /*  6: Usage Fault */
    0, 0, 0, 0,                            /*  7-10: Reserved */
    (uint32_t)&SVC_Handler,                /* 11: SVCall */
    (uint32_t)&DebugMon_Handler,           /* 12: Debug Monitor */
    0,                                     /* 13: Reserved */
    (uint32_t)&PendSV_Handler,             /* 14: PendSV */
    (uint32_t)&SysTick_Handler,            /* 15: SysTick */

    /* GD32F303 Çevre Birim Kesmeleri */
    (uint32_t)&WWDGT_IRQHandler,           /* 16: Window Watchdog */
    (uint32_t)&LVD_IRQHandler,             /* 17: Low Voltage Detector */
    (uint32_t)&TAMPER_IRQHandler,          /* 18: Tamper */
    (uint32_t)&RTC_IRQHandler,             /* 19: RTC */
    (uint32_t)&FMC_IRQHandler,             /* 20: Flash Memory Controller */
    (uint32_t)&RCU_IRQHandler,             /* 21: Reset and Clock Unit */
    (uint32_t)&EXTI0_IRQHandler,           /* 22: EXTI Line 0 */
    (uint32_t)&EXTI1_IRQHandler,           /* 23: EXTI Line 1 */
    (uint32_t)&EXTI2_IRQHandler,           /* 24: EXTI Line 2 */
    (uint32_t)&EXTI3_IRQHandler,           /* 25: EXTI Line 3 */
    (uint32_t)&EXTI4_IRQHandler,           /* 26: EXTI Line 4 */
    (uint32_t)&DMA0_Channel0_IRQHandler,   /* 27: DMA0 Channel 0 */
    (uint32_t)&DMA0_Channel1_IRQHandler,   /* 28: DMA0 Channel 1 */
    (uint32_t)&DMA0_Channel2_IRQHandler,   /* 29: DMA0 Channel 2 */
    (uint32_t)&DMA0_Channel3_IRQHandler,   /* 30: DMA0 Channel 3 */
    (uint32_t)&DMA0_Channel4_IRQHandler,   /* 31: DMA0 Channel 4 */
    (uint32_t)&DMA0_Channel5_IRQHandler,   /* 32: DMA0 Channel 5 */
    (uint32_t)&DMA0_Channel6_IRQHandler,   /* 33: DMA0 Channel 6 */
    (uint32_t)&ADC0_1_IRQHandler,          /* 34: ADC0 and ADC1 */
    (uint32_t)&CAN0_TX_IRQHandler,         /* 35: CAN0 TX */
    (uint32_t)&CAN0_RX0_IRQHandler,        /* 36: CAN0 RX0 */
    (uint32_t)&CAN0_RX1_IRQHandler,        /* 37: CAN0 RX1 */
    (uint32_t)&CAN0_EWMC_IRQHandler,       /* 38: CAN0 EWMC */
    (uint32_t)&EXTI5_9_IRQHandler,         /* 39: EXTI Lines 5-9 */
    (uint32_t)&TIMER0_BRK_IRQHandler,      /* 40: TIMER0 Break */
    (uint32_t)&TIMER0_UP_IRQHandler,       /* 41: TIMER0 Update */
    (uint32_t)&TIMER0_TRG_CMT_IRQHandler,  /* 42: TIMER0 Trigger/Commutation */
    (uint32_t)&TIMER0_Channel_IRQHandler,  /* 43: TIMER0 Channel */
    (uint32_t)&TIMER1_IRQHandler,          /* 44: TIMER1 */
    (uint32_t)&TIMER2_IRQHandler,          /* 45: TIMER2 */
    (uint32_t)&TIMER3_IRQHandler,          /* 46: TIMER3 */
    (uint32_t)&I2C0_EV_IRQHandler,         /* 47: I2C0 Event */
    (uint32_t)&I2C0_ER_IRQHandler,         /* 48: I2C0 Error */
    (uint32_t)&I2C1_EV_IRQHandler,         /* 49: I2C1 Event */
    (uint32_t)&I2C1_ER_IRQHandler,         /* 50: I2C1 Error */
    (uint32_t)&SPI0_IRQHandler,            /* 51: SPI0 */
    (uint32_t)&SPI1_IRQHandler,            /* 52: SPI1 */
    (uint32_t)&USART0_IRQHandler,          /* 53: USART0 */
    (uint32_t)&USART1_IRQHandler,          /* 54: USART1 */
    (uint32_t)&USART2_IRQHandler,          /* 55: USART2 */
    (uint32_t)&EXTI10_15_IRQHandler,       /* 56: EXTI Lines 10-15 */
    (uint32_t)&RTC_Alarm_IRQHandler,       /* 57: RTC Alarm */
    (uint32_t)&USBFS_WKUP_IRQHandler,     /* 58: USB Full-Speed Wakeup */
    (uint32_t)&TIMER7_BRK_IRQHandler,      /* 59: TIMER7 Break */
    (uint32_t)&TIMER7_UP_IRQHandler,       /* 60: TIMER7 Update */
    (uint32_t)&TIMER7_TRG_CMT_IRQHandler,  /* 61: TIMER7 Trigger/Commutation */
    (uint32_t)&TIMER7_Channel_IRQHandler,  /* 62: TIMER7 Channel */
    (uint32_t)&ADC2_IRQHandler,            /* 63: ADC2 */
    (uint32_t)&EXMC_IRQHandler,            /* 64: External Memory Controller */
    (uint32_t)&SDIO_IRQHandler,            /* 65: SDIO */
    (uint32_t)&TIMER4_IRQHandler,          /* 66: TIMER4 */
    (uint32_t)&SPI2_IRQHandler,            /* 67: SPI2 */
    (uint32_t)&UART3_IRQHandler,           /* 68: UART3 */
    (uint32_t)&UART4_IRQHandler,           /* 69: UART4 */
    (uint32_t)&TIMER5_IRQHandler,          /* 70: TIMER5 */
    (uint32_t)&TIMER6_IRQHandler,          /* 71: TIMER6 */
    (uint32_t)&DMA1_Channel0_IRQHandler,   /* 72: DMA1 Channel 0 */
    (uint32_t)&DMA1_Channel1_IRQHandler,   /* 73: DMA1 Channel 1 */
    (uint32_t)&DMA1_Channel2_IRQHandler,   /* 74: DMA1 Channel 2 */
    (uint32_t)&DMA1_Channel3_IRQHandler,   /* 75: DMA1 Channel 3 */
    (uint32_t)&DMA1_Channel4_IRQHandler,   /* 76: DMA1 Channel 4 */
    (uint32_t)&ENET_IRQHandler,            /* 77: Ethernet */
    (uint32_t)&ENET_WKUP_IRQHandler,       /* 78: Ethernet Wakeup */
    (uint32_t)&CAN1_TX_IRQHandler,         /* 79: CAN1 TX */
    (uint32_t)&CAN1_RX0_IRQHandler,        /* 80: CAN1 RX0 */
    (uint32_t)&CAN1_RX1_IRQHandler,        /* 81: CAN1 RX1 */
    (uint32_t)&CAN1_EWMC_IRQHandler,       /* 82: CAN1 EWMC */
    (uint32_t)&USBFS_IRQHandler,           /* 83: USB Full-Speed */
};

/* ============================================================
 * Reset Handler — Çalışma Zamanı Başlatma
 * ============================================================ */

void Reset_Handler(void)
{
    /* .data bölümünü Flash'tan SRAM'e kopyala */
    uint32_t *src = (uint32_t *)_sidata;
    uint32_t *dst = (uint32_t *)_sdata;
    while (dst < (uint32_t *)_edata) {
        *dst++ = *src++;
    }

    /* .bss bölümünü sıfırla */
    dst = (uint32_t *)_sbss;
    while (dst < (uint32_t *)_ebss) {
        *dst++ = 0;
    }

    /* FPU etkinleştir (Cortex-M4 opsiyonel) */
    /* SCB->CPACR |= ((3 << 10*2) | (3 << 11*2)); */
    *(volatile uint32_t *)0xE000ED88U |= (0xFU << 20);

    /* main() fonksiyonuna dallan */
    main();

    /* main() dönerse sonsuz döngü */
    while (1) {
        __asm__ volatile ("wfi");
    }
}

/* ============================================================
 * Hard Fault Handler — Hata Teşhisi
 * ============================================================ */

void HardFault_Handler(void)
{
    /* HardFault durumunda LED yakarak kullanıcıyı uyar */
    /* PA1 = LED_PWR → hızlı yanıp sönme */

    /* GPIOA base = 0x40010800, ODR = offset 0x0C, Pin 1 */
    volatile uint32_t *gpioa_odr = (volatile uint32_t *)0x4001080CU;

    while (1) {
        *gpioa_odr ^= (1U << 1);  /* PA1 toggle */
        for (volatile uint32_t i = 0; i < 500000; i++) {
            __asm__ volatile("nop");
        }
    }
}

/* ============================================================
 * Default Handler — Beklenmeyen Kesme
 * ============================================================ */

void Default_Handler(void)
{
    while (1) {
        __asm__ volatile ("wfi");
    }
}

/* ============================================================
 * Newlib Minimal Syscalls
 * ============================================================ */

int _close(int file) { (void)file; return -1; }
int _fstat(int file, struct stat *st) { (void)file; st->st_mode = S_IFCHR; return 0; }
int _isatty(int file) { (void)file; return 1; }
int _lseek(int file, int ptr, int dir) { (void)file; (void)ptr; (void)dir; return 0; }
int _read(int file, char *ptr, int len) { (void)file; (void)ptr; (void)len; return 0; }
int _write(int file, char *ptr, int len) { (void)file; (void)ptr; (void)len; return len; }
int _getpid(void) { return 1; }
int _kill(int pid, int sig) { (void)pid; (void)sig; return -1; }

/* _sbrk: Heap yönetimi */
static uint8_t *heap_end = NULL;

void *_sbrk(int incr)
{
    uint8_t *prev_heap;

    if (heap_end == NULL) {
        heap_end = (uint8_t *)_sbss;
    }
    prev_heap = heap_end;

    /* Stack ile çakışma kontrolü */
    uintptr_t stack_limit = (uintptr_t)_estack - 1024;
    if ((uintptr_t)(heap_end + incr) > stack_limit) {
        return (void *)-1;  /* Out of memory */
    }

    heap_end += incr;
    return (void *)prev_heap;
}
