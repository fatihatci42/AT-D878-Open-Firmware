/**
 * @file    aprs_packet.h
 * @brief   AX.25 Packet Protocol & APRS Frame Encoder - AT-D878 Open Firmware
 * @version 0.1.0
 *
 * 1200 Baud AFSK Bell 202 (144.800 MHz EU/TR, 144.390 MHz US)
 * AX.25 Unnumbered Information (UI) çerçeve kodlayıcı ve SmartBeaconing.
 */

#ifndef APRS_PACKET_H
#define APRS_PACKET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AX25_FLAG                  0x7E
#define AX25_CONTROL_UI            0x03
#define AX25_PID_NO_LAYER3         0xF0

#define APRS_MAX_PACKET_LEN        256

/**
 * @brief AX.25 CRC-16 CCITT hesapla
 */
uint16_t aprs_crc16(const uint8_t *data, size_t len);

/**
 * @brief Standart APRS Konum Raporu AX.25 çerçevesi oluştur
 * @param src_call      Kaynak çağrı işareti (örn: "TA5THK")
 * @param src_ssid      Kaynak SSID (0..15, el telsizi için 7)
 * @param dest_call     Hedef yazılım kodu (örn: "APDR16")
 * @param path          Digi yolu (örn: "WIDE1-1,WIDE2-1")
 * @param lat_deg       Enlem (ondalık derece, örn: 36.9914)
 * @param lon_deg       Boylam (ondalık derece, örn: 35.3308)
 * @param alt_meters    Rakım (metre)
 * @param sym_table     Sembol tablosu ('/' veya '\\')
 * @param sym_code      Sembol kodu ('>' = araba, '[' = koşucu, '-' = ev)
 * @param comment       Yorum metni
 * @param out_buf       Çıktı tamponu
 * @param max_len       Tampon boyutu
 * @return Oluşturulan byte sayısı, negatif hata
 */
int aprs_build_position_packet(const char *src_call, uint8_t src_ssid,
                               const char *dest_call, const char *path,
                               double lat_deg, double lon_deg, float alt_meters,
                               char sym_table, char sym_code,
                               const char *comment,
                               uint8_t *out_buf, size_t max_len);

/**
 * @brief APRS Metin Mesajı AX.25 çerçevesi oluştur
 */
int aprs_build_message_packet(const char *src_call, uint8_t src_ssid,
                              const char *target_call, uint8_t target_ssid,
                              const char *message, uint16_t msg_id,
                              uint8_t *out_buf, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* APRS_PACKET_H */
