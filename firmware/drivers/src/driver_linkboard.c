/**
 * @file    driver_linkboard.c
 * @brief   BHT LinkBoard (APRS + BT + GPS) Driver — AT-D878 Open Firmware
 * @version 0.1.0
 *
 * LinkBoard UART iletişim protokolü:
 *   - USART2 (PB10/PB11) üzerinden 115200 baud, 8N1
 *   - AT komut tabanlı (tahmini, doğrulanacak)
 *   - GPS NMEA cümleleri LinkBoard tarafından iletilir
 */

#include "driver_linkboard.h"
#include "hal_uart.h"
#include "hal_systick.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ============================================================
 * Yapılandırma
 * ============================================================ */

/** LinkBoard'un bağlı olduğu UART portu */
#define LB_UART_PORT          HAL_UART_2

/** UART baud hızı */
#define LB_UART_BAUD          115200

/** Komut yanıt zaman aşımı (ms) */
#define LB_CMD_TIMEOUT_MS     1000

/** NMEA cümle tamponu */
#define NMEA_BUF_SIZE         128
#define GPS_FIX_MAX_AGE_MS    5000U

/* ============================================================
 * Statik Değişkenler
 * ============================================================ */

static gps_data_t   s_gps_data;
static aprs_config_t s_aprs_config;
static bt_status_t  s_bt_status;
static bool         s_initialized = false;
static tick_t       s_gps_last_fix_ms;

/* NMEA parse tamponu */
static char s_nmea_buf[NMEA_BUF_SIZE];
static uint8_t s_nmea_pos = 0;

/* ============================================================
 * Yardımcı Fonksiyonlar
 * ============================================================ */

/** LinkBoard'a AT komutu gönder ve yanıtı oku */
static int lb_send_command(const char *cmd, char *response, size_t resp_size)
{
    /* Komutu gönder */
    if (!cmd) return -1;
    size_t cmd_len = strlen(cmd);
    if (hal_uart_transmit(LB_UART_PORT, (const uint8_t *)cmd, cmd_len) != (int)cmd_len ||
        hal_uart_transmit(LB_UART_PORT, (const uint8_t *)"\r\n", 2) != 2) {
        return -1;
    }

    /* Yanıt bekle */
    if (response && resp_size > 0) {
        int n = hal_uart_receive(LB_UART_PORT, (uint8_t *)response,
                                 resp_size - 1, LB_CMD_TIMEOUT_MS);
        if (n > 0) {
            response[n] = '\0';
            return 0;
        }
        response[0] = '\0';
        return -1;
    }

    hal_delay_ms(50);  /* Komut işlenme süresi */
    return 0;
}

/* ============================================================
 * NMEA Parser
 * ============================================================ */

/**
 * @brief NMEA dereceyi ondalık dereceye dönüştür
 * "4056.7890" → 40 + 56.7890/60 = 40.9464833...
 */
static bool nmea_to_decimal(const char *field, char dir,
                            uint8_t degree_digits, double *decimal)
{
    if (!field || !decimal || field[0] == '\0') return false;

    char *end = NULL;
    double raw = strtod(field, &end);
    uint8_t max_degrees = (degree_digits == 2) ? 90 : 180;
    if (end == field || *end != '\0' || !(raw >= 0.0) ||
        raw > (double)max_degrees * 100.0 + 59.999999) {
        return false;
    }

    int degrees = (int)(raw / 100.0);
    double minutes = raw - (degrees * 100.0);
    if (minutes >= 60.0 ||
        (degrees == max_degrees && minutes > 0.0)) {
        return false;
    }

    if (dir == 'S' || dir == 'W') {
        *decimal = -(degrees + (minutes / 60.0));
    } else if (dir == 'N' || dir == 'E') {
        *decimal = degrees + (minutes / 60.0);
    } else {
        return false;
    }
    return true;
}

/** NMEA checksum doğrula */
static bool nmea_verify_checksum(const char *sentence)
{
    if (sentence[0] != '$') return false;

    uint8_t calc = 0;
    int i = 1;
    while (sentence[i] && sentence[i] != '*') {
        calc ^= (uint8_t)sentence[i];
        i++;
    }

    if (sentence[i] != '*') return false;
    if (!isxdigit((unsigned char)sentence[i + 1]) ||
        !isxdigit((unsigned char)sentence[i + 2]) || sentence[i + 3] != '\0') {
        return false;
    }

    uint8_t given = (uint8_t)strtol(&sentence[i + 1], NULL, 16);
    return (calc == given);
}

/** Alan ayırıcıları ile NMEA cümlesini parse et */
static const char *nmea_field(const char *sentence, int field_num)
{
    int count = 0;
    const char *p = sentence;
    while (*p) {
        if (*p == ',') {
            count++;
            if (count == field_num) return p + 1;
        }
        p++;
    }
    return "";
}

/** Alandan string kopyala (bir sonraki ',' veya '*' ya kadar) */
static void nmea_field_copy(const char *sentence, int field_num,
                            char *out, size_t max_len)
{
    const char *f = nmea_field(sentence, field_num);
    size_t i = 0;
    while (f[i] && f[i] != ',' && f[i] != '*' && i < max_len - 1) {
        out[i] = f[i];
        i++;
    }
    out[i] = '\0';
}

/**
 * @brief $GPRMC cümlesini parse et
 * $GPRMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,ddmmyy,x.x,a*hh
 */
static void parse_gprmc(const char *sentence)
{
    char field[20];

    /* Alan 1: Zaman (hhmmss.ss) */
    nmea_field_copy(sentence, 1, field, sizeof(field));
    if (strlen(field) < 6 ||
        !isdigit((unsigned char)field[0]) || !isdigit((unsigned char)field[1]) ||
        !isdigit((unsigned char)field[2]) || !isdigit((unsigned char)field[3]) ||
        !isdigit((unsigned char)field[4]) || !isdigit((unsigned char)field[5])) {
        s_gps_data.valid = false;
        s_gps_last_fix_ms = 0;
        return;
    }
    s_gps_data.hour   = (uint8_t)((field[0] - '0') * 10 + (field[1] - '0'));
    s_gps_data.minute = (uint8_t)((field[2] - '0') * 10 + (field[3] - '0'));
    s_gps_data.second = (uint8_t)((field[4] - '0') * 10 + (field[5] - '0'));
    if (s_gps_data.hour > 23 || s_gps_data.minute > 59 ||
        s_gps_data.second > 60) {
        s_gps_data.valid = false;
        s_gps_last_fix_ms = 0;
        return;
    }

    /* Alan 2: Durum (A=aktif, V=void) */
    nmea_field_copy(sentence, 2, field, sizeof(field));
    s_gps_data.valid = (field[0] == 'A');
    if (!s_gps_data.valid) {
        s_gps_last_fix_ms = 0;
        return;
    }

    /* Alan 3,4: Enlem */
    char lat_field[16], lat_dir[4];
    nmea_field_copy(sentence, 3, lat_field, sizeof(lat_field));
    nmea_field_copy(sentence, 4, lat_dir, sizeof(lat_dir));

    /* Alan 5,6: Boylam */
    char lon_field[16], lon_dir[4];
    nmea_field_copy(sentence, 5, lon_field, sizeof(lon_field));
    nmea_field_copy(sentence, 6, lon_dir, sizeof(lon_dir));
    double latitude, longitude;
    if (!nmea_to_decimal(lat_field, lat_dir[0], 2, &latitude) ||
        !nmea_to_decimal(lon_field, lon_dir[0], 3, &longitude)) {
        s_gps_data.valid = false;
        s_gps_last_fix_ms = 0;
        return;
    }
    s_gps_data.latitude = latitude;
    s_gps_data.longitude = longitude;

    /* Alan 7: Hız (knot) */
    nmea_field_copy(sentence, 7, field, sizeof(field));
    if (field[0]) s_gps_data.speed_knots = (float)atof(field);

    /* Alan 8: Yön (derece) */
    nmea_field_copy(sentence, 8, field, sizeof(field));
    if (field[0]) s_gps_data.course_deg = (float)atof(field);

    /* Alan 9: Tarih (ddmmyy) */
    nmea_field_copy(sentence, 9, field, sizeof(field));
    if (strlen(field) >= 6 &&
        isdigit((unsigned char)field[0]) && isdigit((unsigned char)field[1]) &&
        isdigit((unsigned char)field[2]) && isdigit((unsigned char)field[3]) &&
        isdigit((unsigned char)field[4]) && isdigit((unsigned char)field[5])) {
        s_gps_data.day   = (uint8_t)((field[0] - '0') * 10 + (field[1] - '0'));
        s_gps_data.month = (uint8_t)((field[2] - '0') * 10 + (field[3] - '0'));
        s_gps_data.year  = (uint16_t)(2000 + (field[4] - '0') * 10 + (field[5] - '0'));
    }

    s_gps_last_fix_ms = hal_tick_get();
}

/**
 * @brief $GPGGA cümlesini parse et (uydu sayısı ve yükseklik)
 */
static void parse_gpgga(const char *sentence)
{
    char field[20];

    /* Alan 7: Uydu sayısı */
    nmea_field_copy(sentence, 7, field, sizeof(field));
    if (field[0]) s_gps_data.satellites = (uint8_t)atoi(field);

    /* Alan 9: Yükseklik (metre) */
    nmea_field_copy(sentence, 9, field, sizeof(field));
    if (field[0]) s_gps_data.altitude_m = (float)atof(field);
}

/** NMEA cümlesini işle */
static void process_nmea(const char *sentence)
{
    if (!nmea_verify_checksum(sentence)) return;

    if (strncmp(sentence, "$GPRMC", 6) == 0 ||
        strncmp(sentence, "$GNRMC", 6) == 0) {
        parse_gprmc(sentence);
    } else if (strncmp(sentence, "$GPGGA", 6) == 0 ||
               strncmp(sentence, "$GNGGA", 6) == 0) {
        parse_gpgga(sentence);
    }
}

/* ============================================================
 * API İmplementasyonu
 * ============================================================ */

int linkboard_init(void)
{
    memset(&s_gps_data, 0, sizeof(s_gps_data));
    memset(&s_aprs_config, 0, sizeof(s_aprs_config));
    memset(&s_bt_status, 0, sizeof(s_bt_status));
    s_nmea_pos = 0;
    s_gps_last_fix_ms = 0;

    /* UART başlat */
    hal_uart_config_t uart_cfg = {
        .baudrate    = LB_UART_BAUD,
        .parity      = HAL_UART_PARITY_NONE,
        .stopbits    = HAL_UART_STOP_1,
        .use_dma     = false,
        .rx_buf_size = 256,
    };

    int rc = hal_uart_init(LB_UART_PORT, &uart_cfg);
    if (rc != 0) return rc;

    hal_delay_ms(100);  /* LinkBoard boot süresi */

    s_initialized = true;

    /* Firmware sürümünü sorgula */
    char version[32];
    linkboard_get_version(version, sizeof(version));

    return 0;
}

void linkboard_deinit(void)
{
    if (s_initialized) {
        hal_uart_deinit(LB_UART_PORT);
        s_initialized = false;
    }
}

int linkboard_get_version(char *version, size_t max_len)
{
    if (!s_initialized || !version) return -1;
    return lb_send_command("AT+VERSION?", version, max_len);
}

int linkboard_gps_get(gps_data_t *data)
{
    if (!data) return -1;
    memcpy(data, &s_gps_data, sizeof(gps_data_t));
    if (!data->valid ||
        hal_tick_elapsed(s_gps_last_fix_ms, hal_tick_get()) > GPS_FIX_MAX_AGE_MS) {
        data->valid = false;
        return -1;
    }
    return 0;
}

int linkboard_gps_power(bool on)
{
    return lb_send_command(on ? "AT+GPS=ON" : "AT+GPS=OFF", NULL, 0);
}

int linkboard_aprs_configure(const aprs_config_t *config)
{
    if (!s_initialized || !config) return -1;
    memcpy(&s_aprs_config, config, sizeof(aprs_config_t));
    s_aprs_config.callsign[sizeof(s_aprs_config.callsign) - 1] = '\0';
    s_aprs_config.dest[sizeof(s_aprs_config.dest) - 1] = '\0';
    s_aprs_config.path[sizeof(s_aprs_config.path) - 1] = '\0';
    s_aprs_config.comment[sizeof(s_aprs_config.comment) - 1] = '\0';
    if (s_aprs_config.callsign[0] == '\0' || s_aprs_config.ssid > 15) return -1;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+APRS_CALL=%s-%d",
             s_aprs_config.callsign, s_aprs_config.ssid);
    if (lb_send_command(cmd, NULL, 0) != 0) return -1;

    snprintf(cmd, sizeof(cmd), "AT+APRS_PATH=%s", s_aprs_config.path);
    if (lb_send_command(cmd, NULL, 0) != 0) return -1;

    snprintf(cmd, sizeof(cmd), "AT+APRS_SYMBOL=%c%c",
             s_aprs_config.symbol_table, s_aprs_config.symbol_code);
    if (lb_send_command(cmd, NULL, 0) != 0) return -1;

    return 0;
}

int linkboard_aprs_get_config(aprs_config_t *config)
{
    if (!s_initialized || !config) return -1;
    memcpy(config, &s_aprs_config, sizeof(aprs_config_t));
    return 0;
}

int linkboard_aprs_send_position(void)
{
    gps_data_t gps;
    if (!s_initialized || linkboard_gps_get(&gps) != 0 || !gps.valid) return -1;

    char cmd[160];
    int cmd_len = snprintf(cmd, sizeof(cmd), "AT+APRS_POS=%.4f,%.4f,%.0f,%s",
                           gps.latitude, gps.longitude,
                           (double)gps.altitude_m, s_aprs_config.comment);
    if (cmd_len < 0 || (size_t)cmd_len >= sizeof(cmd)) return -1;
    return lb_send_command(cmd, NULL, 0);
}

int linkboard_aprs_send_message(const char *dest_call, const char *message)
{
    if (!s_initialized || !dest_call || !message) return -1;

    char cmd[160];
    snprintf(cmd, sizeof(cmd), "AT+APRS_MSG=%s,%s", dest_call, message);
    return lb_send_command(cmd, NULL, 0);
}

int linkboard_bt_power(bool on)
{
    if (!s_initialized) return -1;
    int rc = lb_send_command(on ? "AT+BT=ON" : "AT+BT=OFF", NULL, 0);
    if (rc == 0) {
        s_bt_status.state = on ? BT_STATE_ADVERTISING : BT_STATE_OFF;
    }
    return rc;
}

int linkboard_bt_get_status(bt_status_t *status)
{
    if (!status) return -1;
    memcpy(status, &s_bt_status, sizeof(bt_status_t));
    return 0;
}

void linkboard_process(void)
{
    if (!s_initialized) return;

    /* UART'tan gelen baytları oku ve NMEA cümlelerini parse et */
    uint8_t byte;
    while (hal_uart_available(LB_UART_PORT) > 0) {
        if (hal_uart_receive(LB_UART_PORT, &byte, 1, 0) != 1) break;

        if (byte == '$') {
            /* Yeni NMEA cümlesi başlangıcı */
            s_nmea_pos = 0;
            s_nmea_buf[s_nmea_pos++] = (char)byte;
        } else if (byte == '\n' || byte == '\r') {
            /* Cümle sonu */
            if (s_nmea_pos > 6) {
                s_nmea_buf[s_nmea_pos] = '\0';
                process_nmea(s_nmea_buf);
            }
            s_nmea_pos = 0;
        } else {
            if (s_nmea_pos < NMEA_BUF_SIZE - 1) {
                s_nmea_buf[s_nmea_pos++] = (char)byte;
            }
        }
    }
}
