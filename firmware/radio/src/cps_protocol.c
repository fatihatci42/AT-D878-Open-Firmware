/**
 * @file    cps_protocol.c
 * @brief   AnyTone USB/UART CPS Communication Protocol Implementation
 * @version 0.1.0
 */

#include "cps_protocol.h"
#include "hal_uart.h"
#include "driver_spi_flash.h"
#include "driver_keyboard.h"
#include "radio_core.h"
#include "hal_adc.h"
#include <string.h>

static bool s_connected = false;
static uint8_t s_rx_buffer[2048];

int cps_protocol_init(void)
{
    hal_uart_config_t cfg = {
        .baudrate    = CPS_BAUDRATE_DEFAULT,
        .parity      = HAL_UART_PARITY_NONE,
        .stopbits    = HAL_UART_STOP_1,
        .use_dma     = false,
        .rx_buf_size = 2048
    };

    int rc = hal_uart_init(HAL_UART_0, &cfg);
    s_connected = false;
    return rc;
}

bool cps_protocol_is_connected(void)
{
    return s_connected;
}

void cps_protocol_process(void)
{
    size_t avail = hal_uart_available(HAL_UART_0);
    if (avail == 0) return;

    /* Handshake kontrolü: "PROGRAM" el sıkışması */
    if (avail >= 7) {
        uint8_t peek[8];
        int n = hal_uart_receive(HAL_UART_0, peek, 7, 5);
        if (n == 7 && memcmp(peek, "PROGRAM", 7) == 0) {
            static const uint8_t resp_qx[] = {'Q', 'X', CPS_CMD_ACK};
            hal_uart_transmit(HAL_UART_0, resp_qx, sizeof(resp_qx));
            s_connected = true;
            return;
        }
    }

    uint8_t cmd = 0;
    if (hal_uart_receive(HAL_UART_0, &cmd, 1, 5) != 1) {
        return;
    }

    switch (cmd) {
    case CPS_CMD_QUERY_ID: {
        /* AnyTone AT-D878UVII Plus Model Kimliği */
        static const uint8_t id_payload[] = {
            'I', 'D', '8', '7', '8', 'U', 'V', '2', 0x00,
            'V', '1', '0', '1', 0x00, 0x00, CPS_CMD_ACK
        };
        hal_uart_transmit(HAL_UART_0, id_payload, sizeof(id_payload));
        s_connected = true;
        break;
    }

    case CPS_CMD_ACK: {
        uint8_t ack = CPS_CMD_ACK;
        hal_uart_transmit(HAL_UART_0, &ack, 1);
        break;
    }

    case CPS_CMD_GET_STATUS: {
        /* Telsiz Durum Paketi */
        radio_core_status_t status;
        radio_core_get_status(&status);

        uint8_t status_pkt[12];
        status_pkt[0] = (uint8_t)(status.battery_mv >> 8);
        status_pkt[1] = (uint8_t)(status.battery_mv & 0xFF);
        status_pkt[2] = status.battery_percent;
        status_pkt[3] = (uint8_t)status.state;
        status_pkt[4] = (uint8_t)status.active_vfo;
        status_pkt[5] = (uint8_t)(status.rssi_dbm >> 8);
        status_pkt[6] = (uint8_t)(status.rssi_dbm & 0xFF);
        status_pkt[7] = status.scan_active ? 1 : 0;
        status_pkt[8] = CPS_CMD_ACK;

        hal_uart_transmit(HAL_UART_0, status_pkt, 9);
        break;
    }

    case CPS_CMD_REMOTE_KEY: {
        /* Uzaktan tuş basımı enjeksiyonu [key_code, event_type] */
        uint8_t key_data[2];
        if (hal_uart_receive(HAL_UART_0, key_data, 2, 20) == 2) {
            keyboard_inject_event((key_code_t)key_data[0], (key_event_type_t)key_data[1]);
            uint8_t ack = CPS_CMD_ACK;
            hal_uart_transmit(HAL_UART_0, &ack, 1);
        }
        break;
    }

    case CPS_CMD_READ_BLOCK: {
        /* Harici SPI Flash Codeplug Okuma: [Addr: 4B big-endian, Len: 2B big-endian] */
        uint8_t req[6];
        if (hal_uart_receive(HAL_UART_0, req, 6, 50) == 6) {
            uint32_t addr = ((uint32_t)req[0] << 24) | ((uint32_t)req[1] << 16) |
                            ((uint32_t)req[2] << 8)  | (uint32_t)req[3];
            uint16_t len  = ((uint16_t)req[4] << 8)  | (uint16_t)req[5];

            if (len > sizeof(s_rx_buffer)) len = sizeof(s_rx_buffer);

            spi_flash_read(addr, s_rx_buffer, len);

            /* Checksum hesapla ve ilet */
            uint8_t checksum = 0;
            for (uint16_t i = 0; i < len; i++) {
                checksum += s_rx_buffer[i];
            }

            hal_uart_transmit(HAL_UART_0, s_rx_buffer, len);
            hal_uart_transmit(HAL_UART_0, &checksum, 1);
        }
        break;
    }

    case CPS_CMD_WRITE_BLOCK: {
        /* Harici SPI Flash Codeplug Yazma: [Addr: 4B, Len: 2B, Data..., Checksum: 1B] */
        uint8_t header[6];
        if (hal_uart_receive(HAL_UART_0, header, 6, 50) == 6) {
            uint32_t addr = ((uint32_t)header[0] << 24) | ((uint32_t)header[1] << 16) |
                            ((uint32_t)header[2] << 8)  | (uint32_t)header[3];
            uint16_t len  = ((uint16_t)header[4] << 8)  | (uint16_t)header[5];

            if (len <= sizeof(s_rx_buffer)) {
                if (hal_uart_receive(HAL_UART_0, s_rx_buffer, len, 500) == (int)len) {
                    uint8_t expected_cs = 0;
                    hal_uart_receive(HAL_UART_0, &expected_cs, 1, 50);

                    /* Yaz ve ACK döndür */
                    spi_flash_write(addr, s_rx_buffer, len);
                    uint8_t ack = CPS_CMD_ACK;
                    hal_uart_transmit(HAL_UART_0, &ack, 1);
                }
            }
        }
        break;
    }

    default:
        break;
    }
}
