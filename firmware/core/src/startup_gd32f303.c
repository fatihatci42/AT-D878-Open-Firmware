/**
 * @file    startup_gd32f303.c
 * @brief   Cortex-M4 Startup & Vector Table - AT-D878 Open Firmware
 * @version 0.0.1
 */

#include <stdint.h>
#include <sys/stat.h>

extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

extern int main(void);

void Reset_Handler(void);
void Default_Handler(void);

void NMI_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)     __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)    __attribute__((weak, alias("Default_Handler")));

/* Vector Table - 0x08004000 adresine yerleşir */
__attribute__((section(".isr_vector"), used))
const uint32_t g_pfnVectors[] = {
    (uint32_t)&_estack,               /* Initial Stack Pointer (MSP) */
    (uint32_t)&Reset_Handler,         /* Reset Handler */
    (uint32_t)&NMI_Handler,           /* NMI */
    (uint32_t)&HardFault_Handler,     /* HardFault */
    (uint32_t)&MemManage_Handler,     /* MPU Fault */
    (uint32_t)&BusFault_Handler,      /* Bus Fault */
    (uint32_t)&UsageFault_Handler,    /* Usage Fault */
    0, 0, 0, 0,                       /* Reserved */
    (uint32_t)&SVC_Handler,           /* SVCall */
    0, 0,                             /* Debug / Reserved */
    (uint32_t)&PendSV_Handler,        /* PendSV */
    (uint32_t)&SysTick_Handler,       /* SysTick */
};

void Reset_Handler(void)
{
    /* .data bölümünü Flash'tan SRAM'e kopyala */
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    /* .bss bölümünü sıfırla */
    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0;
    }

    /* main() fonksiyonuna dallan */
    main();

    /* Sonsuz döngü (main dönerse) */
    while (1) {
        __asm__ volatile ("wfi");
    }
}

void Default_Handler(void)
{
    while (1) {
        __asm__ volatile ("wfi");
    }
}

/* Newlib Minimal Syscalls */
int _close(int file) { (void)file; return -1; }
int _fstat(int file, struct stat *st) { (void)file; st->st_mode = S_IFCHR; return 0; }
int _isatty(int file) { (void)file; return 1; }
int _lseek(int file, int ptr, int dir) { (void)file; (void)ptr; (void)dir; return 0; }
int _read(int file, char *ptr, int len) { (void)file; (void)ptr; (void)len; return 0; }
int _write(int file, char *ptr, int len) { (void)file; (void)ptr; (void)len; return len; }
