/**
 * @file    aprs_packet.c
 * @brief   AX.25 Packet Protocol & APRS Frame Encoder Implementation
 * @version 0.1.0
 */

#include "aprs_packet.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

uint16_t aprs_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0x8408;  /* CCITT reversed polynomial */
            } else {
                crc >>= 1;
            }
        }
    }
    return crc ^ 0xFFFF;
}

/** AX.25 çağrı işareti kodlayıcı (7 byte, her karakter sola 1 bit kaydırılır) */
static void encode_callsign(const char *call, uint8_t ssid, bool is_last, uint8_t *out)
{
    char upper[7] = "      ";
    int len = (int)strlen(call);
    if (len > 6) len = 6;
    for (int i = 0; i < len; i++) {
        upper[i] = (char)toupper((unsigned char)call[i]);
    }
    for (int i = 0; i < 6; i++) {
        out[i] = (uint8_t)(upper[i] << 1);
    }
    /* SSID byte: bit 0 = last address flag, bits 1..4 = SSID, bits 5..6 = reserved */
    out[6] = (uint8_t)(0x60 | ((ssid & 0x0F) << 1) | (is_last ? 0x01 : 0x00));
}

int aprs_build_position_packet(const char *src_call, uint8_t src_ssid,
                               const char *dest_call, const char *path,
                               double lat_deg, double lon_deg, float alt_meters,
                               char sym_table, char sym_code,
                               const char *comment,
                               uint8_t *out_buf, size_t max_len)
{
    if (!src_call || !dest_call || !out_buf || max_len < 64) return -1;

    size_t pos = 0;

    /* 1. Başlangıç Bayrağı (Flag) */
    out_buf[pos++] = AX25_FLAG;

    /* 2. Hedef Adresi (7 byte) */
    encode_callsign(dest_call, 0, false, &out_buf[pos]);
    pos += 7;

    /* 3. Kaynak Adresi (7 byte) */
    encode_callsign(src_call, src_ssid, (path == NULL || path[0] == '\0'), &out_buf[pos]);
    pos += 7;

    /* 4. Repeater / Digi Yolu (örn: WIDE1-1, WIDE2-1) */
    if (path && path[0] != '\0') {
        encode_callsign("WIDE1", 1, false, &out_buf[pos]);
        pos += 7;
        encode_callsign("WIDE2", 1, true, &out_buf[pos]);
        pos += 7;
    }

    /* 5. Kontrol & PID Baytları */
    out_buf[pos++] = AX25_CONTROL_UI;
    out_buf[pos++] = AX25_PID_NO_LAYER3;

    /* 6. APRS Bilgi Alanı (Payload) */
    /* Enlem: DDMM.hhN/S */
    char lat_dir = (lat_deg >= 0) ? 'N' : 'S';
    double lat_abs = (lat_deg >= 0) ? lat_deg : -lat_deg;
    int lat_d = (int)lat_abs;
    double lat_m = (lat_abs - lat_d) * 60.0;

    /* Boylam: DDDMM.hhE/W */
    char lon_dir = (lon_deg >= 0) ? 'E' : 'W';
    double lon_abs = (lon_deg >= 0) ? lon_deg : -lon_deg;
    int lon_d = (int)lon_abs;
    double lon_m = (lon_abs - lon_d) * 60.0;

    char info[160];
    snprintf(info, sizeof(info), "!%02d%05.2f%c%c%03d%05.2f%c%c/A=%06d %s",
             lat_d, lat_m, lat_dir,
             sym_table,
             lon_d, lon_m, lon_dir,
             sym_code,
             (int)(alt_meters * 3.28084f), /* fit cinsinden */
             comment ? comment : "AnyTone Open FW");

    size_t info_len = strlen(info);
    if (pos + info_len + 3 > max_len) return -2;

    memcpy(&out_buf[pos], info, info_len);
    pos += info_len;

    /* 7. CRC-16 (Flag hariç hesaplanır) */
    uint16_t crc = aprs_crc16(&out_buf[1], pos - 1);
    out_buf[pos++] = (uint8_t)(crc & 0xFF);
    out_buf[pos++] = (uint8_t)(crc >> 8);

    /* 8. Bitiş Bayrağı (Flag) */
    out_buf[pos++] = AX25_FLAG;

    return (int)pos;
}

int aprs_build_message_packet(const char *src_call, uint8_t src_ssid,
                              const char *target_call, uint8_t target_ssid,
                              const char *message, uint16_t msg_id,
                              uint8_t *out_buf, size_t max_len)
{
    if (!src_call || !target_call || !message || !out_buf || max_len < 64) return -1;

    size_t pos = 0;
    out_buf[pos++] = AX25_FLAG;

    encode_callsign("APDR16", 0, false, &out_buf[pos]);
    pos += 7;
    encode_callsign(src_call, src_ssid, false, &out_buf[pos]);
    pos += 7;
    encode_callsign("WIDE1", 1, true, &out_buf[pos]);
    pos += 7;

    out_buf[pos++] = AX25_CONTROL_UI;
    out_buf[pos++] = AX25_PID_NO_LAYER3;

    /* APRS Mesaj Formatı: :CALLSIGN-S:Mesaj içeriği{MsgID */
    char call_padded[10] = "         ";
    char tgt[12];
    if (target_ssid > 0) {
        snprintf(tgt, sizeof(tgt), "%s-%d", target_call, target_ssid);
    } else {
        snprintf(tgt, sizeof(tgt), "%s", target_call);
    }
    int tlen = (int)strlen(tgt);
    if (tlen > 9) tlen = 9;
    memcpy(call_padded, tgt, tlen);

    char info[160];
    snprintf(info, sizeof(info), ":%s:%s{%02u", call_padded, message, msg_id % 100);

    size_t info_len = strlen(info);
    memcpy(&out_buf[pos], info, info_len);
    pos += info_len;

    uint16_t crc = aprs_crc16(&out_buf[1], pos - 1);
    out_buf[pos++] = (uint8_t)(crc & 0xFF);
    out_buf[pos++] = (uint8_t)(crc >> 8);
    out_buf[pos++] = AX25_FLAG;

    return (int)pos;
}
