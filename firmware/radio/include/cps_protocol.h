/**
 * @file    cps_protocol.h
 * @brief   AnyTone USB/UART CPS Communication Protocol Engine - AT-D878 Open Firmware
 * @version 0.1.0
 *
 * PC CPS (Customer Programming Software) ve AT-D878 Studio ile çift yönlü iletişim:
 * - Bootloader / Application model kimlik doğrulaması (b"PROGRAM" -> b"QX\x06", b"\x02" -> b"ID878UV2...")
 * - Harici SPI Flash Codeplug Okuma / Yazma
 * - Telsiz Durumu (Batarya gerilimi, RSSI, aktif kanal) sorgulama
 * - Uzaktan Tuş Basımı Enjeksiyonu (Desktop simülatör entegrasyonu)
 */

#ifndef CPS_PROTOCOL_H
#define CPS_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CPS_UART_PORT           0  /* HAL_UART_0 (PA9/PA10) */
#define CPS_BAUDRATE_DEFAULT    921600
#define CPS_BAUDRATE_FALLBACK   115200

#define CPS_CMD_ACK             0x06
#define CPS_CMD_NACK            0x15
#define CPS_CMD_QUERY_ID        0x02
#define CPS_CMD_READ_BLOCK      0x01
#define CPS_CMD_WRITE_BLOCK     0x03
#define CPS_CMD_GET_STATUS      0x05
#define CPS_CMD_REMOTE_KEY      0x08
#define CPS_CMD_SCREEN_DUMP     0x0A

/**
 * @brief CPS protokol motorunu başlat (UART0 yapılandır)
 */
int cps_protocol_init(void);

/**
 * @brief Periyodik CPS protokol dinleme ve paket işleme
 */
void cps_protocol_process(void);

/**
 * @brief CPS bağlantısı aktif mi?
 */
bool cps_protocol_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* CPS_PROTOCOL_H */
