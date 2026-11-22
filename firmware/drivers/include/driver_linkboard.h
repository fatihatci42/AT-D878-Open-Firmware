/**
 * @file    driver_linkboard.h
 * @brief   BHT LinkBoard (APRS + Bluetooth) Sub-PCB Driver
 * @version 0.1.0
 *
 * AnyTone AT-D878UVII Plus LinkBoard alt kartı:
 *   - APRS TNC: 1200 baud AFSK modülasyon/demodülasyon
 *   - Bluetooth: SPP profili, kulaklık bağlantısı
 *   - GPS: NMEA cümle işleme (GPRMC, GPGGA)
 *
 * İletişim: UART (tahminen USART2 veya UART3)
 * Firmware sürümü: BHT V2.00 / BT V10046
 *
 * AT komut seti tahmini:
 *   AT+VERSION? → BHT V2.00
 *   AT+APRS=<data> → APRS paketi gönder
 *   AT+BT=ON/OFF → Bluetooth aç/kapat
 *   AT+GPS? → GPS koordinatları
 */

#ifndef DRIVER_LINKBOARD_H
#define DRIVER_LINKBOARD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * GPS Veri Yapıları
 * ============================================================ */

typedef struct {
    bool     valid;             /**< GPS fix geçerli mi */
    double   latitude;          /**< Enlem (derece, + = Kuzey) */
    double   longitude;         /**< Boylam (derece, + = Doğu) */
    float    altitude_m;        /**< Yükseklik (metre, MSL) */
    float    speed_knots;       /**< Hız (knot) */
    float    course_deg;        /**< Yön (derece) */
    uint8_t  satellites;        /**< Görülen uydu sayısı */
    uint8_t  hour, minute, second;
    uint8_t  day, month;
    uint16_t year;
} gps_data_t;

/* ============================================================
 * APRS Yapılandırması
 * ============================================================ */

typedef struct {
    char     callsign[10];      /**< Çağrı işareti (TA5THK) */
    uint8_t  ssid;              /**< SSID (0-15, telsiz=7) */
    char     dest[10];          /**< Hedef (APDR16, APD878) */
    char     path[32];          /**< Digi yolu (WIDE1-1,WIDE2-1) */
    char     symbol_table;      /**< Sembol tablosu ('/' veya '\\') */
    char     symbol_code;       /**< Sembol kodu ('>' = araba) */
    char     comment[64];       /**< Yorum */
    uint32_t freq_hz;           /**< APRS frekansı (144800000) */
    bool     smart_beacon;      /**< SmartBeaconing aktif */
    uint16_t beacon_interval_s; /**< Beacon periyodu (sn) */
} aprs_config_t;

/* ============================================================
 * Bluetooth Yapılandırması
 * ============================================================ */

typedef enum {
    BT_STATE_OFF = 0,
    BT_STATE_IDLE,
    BT_STATE_ADVERTISING,
    BT_STATE_CONNECTED,
} bt_state_t;

typedef struct {
    bt_state_t state;
    char       name[32];        /**< BT cihaz adı */
    char       addr[18];        /**< BT MAC adresi */
    int8_t     rssi;            /**< Bağlantı RSSI */
} bt_status_t;

/* ============================================================
 * API Fonksiyonları
 * ============================================================ */

/**
 * @brief LinkBoard sürücüsünü başlat
 *        UART bağlantısını kur ve firmware sürümünü doğrula
 * @return 0 başarılı, negatif hata
 */
int linkboard_init(void);

/**
 * @brief LinkBoard sürücüsünü kapat
 */
void linkboard_deinit(void);

/**
 * @brief LinkBoard firmware sürümünü oku
 * @param version Sürüm string'i (çıktı, min 32 byte)
 * @return 0 başarılı
 */
int linkboard_get_version(char *version, size_t max_len);

/* --- GPS --- */

/**
 * @brief GPS verilerini al
 * @param data GPS veri yapısı (çıktı)
 * @return 0 geçerli fix, -1 fix yok
 */
int linkboard_gps_get(gps_data_t *data);

/**
 * @brief GPS modülünü aç/kapat
 */
int linkboard_gps_power(bool on);

/* --- APRS --- */

/**
 * @brief APRS yapılandırmasını ayarla
 */
int linkboard_aprs_configure(const aprs_config_t *config);

/**
 * @brief APRS konum beacon'ı gönder
 *        GPS verisi otomatik eklenir
 * @return 0 başarılı
 */
int linkboard_aprs_send_position(void);

/**
 * @brief APRS mesaj paketi gönder
 * @param dest_call Hedef çağrı işareti
 * @param message   Mesaj içeriği
 * @return 0 başarılı
 */
int linkboard_aprs_send_message(const char *dest_call, const char *message);

/* --- Bluetooth --- */

/**
 * @brief Bluetooth aç/kapat
 */
int linkboard_bt_power(bool on);

/**
 * @brief Bluetooth durumunu oku
 */
int linkboard_bt_get_status(bt_status_t *status);

/* --- Periyodik İşlem --- */

/**
 * @brief LinkBoard veri işleme (ana döngüden çağrılır)
 *        UART'tan gelen verileri parse eder
 */
void linkboard_process(void);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_LINKBOARD_H */
