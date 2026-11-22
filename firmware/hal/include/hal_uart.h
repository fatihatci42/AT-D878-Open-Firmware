/**
 * @file    hal_uart.h
 * @brief   UART Hardware Abstraction Layer - AT-D878 Open Firmware
 * @version 0.0.1
 *
 * GD32F303 UART/USART donanım soyutlama katmanı.
 * GPS, Bluetooth ve debug seri port bu arayüzü kullanır.
 */

#ifndef HAL_UART_H
#define HAL_UART_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief UART kanal numaralandırması
 * GD32F303VGT6: USART0, USART1, USART2, UART3, UART4
 */
typedef enum {
    HAL_UART_0 = 0,  /**< USART0 (APB2) - Debug? */
    HAL_UART_1,      /**< USART1 (APB1) - GPS? */
    HAL_UART_2,      /**< USART2 (APB1) - Bluetooth? */
    HAL_UART_3,      /**< UART3  (APB1) */
    HAL_UART_4,      /**< UART4  (APB1) */
    HAL_UART_COUNT
} hal_uart_port_t;

/**
 * @brief UART parity
 */
typedef enum {
    HAL_UART_PARITY_NONE = 0,
    HAL_UART_PARITY_EVEN,
    HAL_UART_PARITY_ODD,
} hal_uart_parity_t;

/**
 * @brief UART stop bit
 */
typedef enum {
    HAL_UART_STOP_1   = 0,
    HAL_UART_STOP_0_5 = 1,
    HAL_UART_STOP_2   = 2,
    HAL_UART_STOP_1_5 = 3,
} hal_uart_stopbits_t;

/**
 * @brief UART yapılandırma yapısı
 */
typedef struct {
    uint32_t             baudrate;   /**< Baud hızı (bps) */
    hal_uart_parity_t    parity;     /**< Parity tipi */
    hal_uart_stopbits_t  stopbits;   /**< Stop bit sayısı */
    bool                 use_dma;    /**< DMA kullanımı */
    size_t               rx_buf_size;/**< RX ring buffer boyutu */
} hal_uart_config_t;

/**
 * @brief UART alma callback
 */
typedef void (*hal_uart_rx_callback_t)(hal_uart_port_t port,
                                       const uint8_t *data,
                                       size_t len,
                                       void *context);

/**
 * @brief UART alt sistemini başlat
 */
int hal_uart_init(hal_uart_port_t port, const hal_uart_config_t *config);

/**
 * @brief UART kanalını kapat
 */
void hal_uart_deinit(hal_uart_port_t port);

/**
 * @brief UART veri gönder (bloklayıcı)
 */
int hal_uart_transmit(hal_uart_port_t port, const uint8_t *data, size_t len);

/**
 * @brief UART formatlı string gönder
 */
int hal_uart_printf(hal_uart_port_t port, const char *fmt, ...);

/**
 * @brief UART veri al (bloklayıcı, timeout ile)
 */
int hal_uart_receive(hal_uart_port_t port, uint8_t *data,
                     size_t len, uint32_t timeout_ms);

/**
 * @brief UART alma callback ayarla (kesme ile)
 */
int hal_uart_set_rx_callback(hal_uart_port_t port,
                             hal_uart_rx_callback_t callback,
                             void *context);

/**
 * @brief UART RX buffer'da bekleyen byte sayısı
 */
size_t hal_uart_available(hal_uart_port_t port);

/**
 * @brief UART RX buffer'ı temizle
 */
void hal_uart_flush(hal_uart_port_t port);

#ifdef __cplusplus
}
#endif

#endif /* HAL_UART_H */
