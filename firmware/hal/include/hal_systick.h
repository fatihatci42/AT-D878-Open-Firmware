/**
 * @file    hal_systick.h
 * @brief   SysTick Timer & System Tick - AT-D878 Open Firmware
 * @version 0.1.0
 *
 * GD32F303 Cortex-M4 SysTick zamanlayıcı.
 * 1 ms çözünürlükte sistem saati sağlar.
 * Debounce, timeout, periyodik görevler ve RTOS-öncesi
 * zamanlama bu modül üzerinden yapılır.
 */

#ifndef HAL_SYSTICK_H
#define HAL_SYSTICK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * GD32F303 Saat Yapılandırma Sabitleri
 * ============================================================ */

/** Harici Yüksek Hızlı Osilatör (HSE) = 8 MHz */
#define HSE_FREQ_HZ           8000000U

/** PLL çarpanı: 8 MHz × 15 = 120 MHz */
#define PLL_MULTIPLIER        15U

/** Sistem saat frekansı (HCLK) = 120 MHz */
#define SYSTEM_CLOCK_HZ       (HSE_FREQ_HZ * PLL_MULTIPLIER)

/** APB1 bölücü = /2 → 60 MHz */
#define APB1_CLOCK_HZ         (SYSTEM_CLOCK_HZ / 2)

/** APB2 bölücü = /1 → 120 MHz */
#define APB2_CLOCK_HZ         SYSTEM_CLOCK_HZ

/** SysTick kesme periyodu (ms) */
#define SYSTICK_PERIOD_MS     1U

/* ============================================================
 * Zaman Tipi
 * ============================================================ */

/** Sistem zaman damgası (ms, uint32_t → ~49 gün taşma) */
typedef uint32_t tick_t;

/* ============================================================
 * Yazılım Zamanlayıcı (Software Timer)
 * ============================================================ */

/** Maksimum eşzamanlı yazılım zamanlayıcı sayısı */
#define SOFT_TIMER_MAX        16

/** Zamanlayıcı callback tipi */
typedef void (*soft_timer_callback_t)(void *context);

/** Yazılım zamanlayıcı yapısı */
typedef struct {
    tick_t                 period_ms;    /**< Periyot (ms) */
    tick_t                 next_fire;    /**< Sonraki ateşleme zamanı */
    soft_timer_callback_t  callback;     /**< Callback fonksiyonu */
    void                  *context;      /**< Kullanıcı bağlamı */
    bool                   repeat;       /**< Tekrarlı mı? */
    bool                   active;       /**< Aktif mi? */
} soft_timer_t;

/** Zamanlayıcı tanıtıcı (handle) */
typedef int8_t timer_handle_t;

#define TIMER_INVALID         ((timer_handle_t)-1)

/* ============================================================
 * API Fonksiyonları
 * ============================================================ */

/**
 * @brief Sistem saatini yapılandır (HSE → PLL → 120 MHz)
 *        ve SysTick'i 1 ms periyotla başlat.
 * @return 0 başarılı, -1 HSE başlatma hatası, -2 PLL kilitlenemedi
 */
int hal_systick_init(void);

/**
 * @brief Sistem başlangıcından bu yana geçen ms sayısı
 * @return Tick sayısı (monoton artan, taşma güvenli)
 */
tick_t hal_tick_get(void);

/**
 * @brief Bloklayıcı gecikme (ms)
 * @param ms Gecikme süresi (milisaniye)
 */
void hal_delay_ms(uint32_t ms);

/**
 * @brief Bloklayıcı gecikme (µs) — döngü tabanlı, kesin değil
 * @param us Gecikme süresi (mikrosaniye)
 */
void hal_delay_us(uint32_t us);

/**
 * @brief İki tick arasındaki farkı hesapla (taşma güvenli)
 * @param start Başlangıç tick
 * @param end   Bitiş tick
 * @return Fark (ms)
 */
static inline uint32_t hal_tick_elapsed(tick_t start, tick_t end)
{
    return (uint32_t)(end - start);
}

/**
 * @brief Belirli bir süre geçti mi?
 */
static inline bool hal_tick_expired(tick_t start, uint32_t timeout_ms)
{
    return hal_tick_elapsed(start, hal_tick_get()) >= timeout_ms;
}

/* ============================================================
 * Yazılım Zamanlayıcı API
 * ============================================================ */

/**
 * @brief Yeni bir yazılım zamanlayıcı oluştur
 * @param period_ms Periyot (ms)
 * @param repeat    Tekrarlı mı?
 * @param callback  Callback fonksiyonu
 * @param context   Kullanıcı bağlamı
 * @return Zamanlayıcı tanıtıcı veya TIMER_INVALID
 */
timer_handle_t hal_timer_create(uint32_t period_ms, bool repeat,
                                soft_timer_callback_t callback,
                                void *context);

/**
 * @brief Zamanlayıcıyı başlat
 */
int hal_timer_start(timer_handle_t handle);

/**
 * @brief Zamanlayıcıyı durdur
 */
int hal_timer_stop(timer_handle_t handle);

/**
 * @brief Zamanlayıcıyı sil
 */
int hal_timer_delete(timer_handle_t handle);

/**
 * @brief Tüm aktif zamanlayıcıları işle
 *        (Ana döngüden her iterasyonda çağrılmalıdır)
 */
void hal_timers_process(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_SYSTICK_H */
