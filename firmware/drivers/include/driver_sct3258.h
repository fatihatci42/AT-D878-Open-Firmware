/**
 * @file    driver_sct3258.h
 * @brief   SCT3258 DMR Baseband Processor Driver - AT-D878 Open Firmware
 * @version 0.0.1
 *
 * CML/Sicomm SCT3258TD DMR dijital baseband işlemci sürücüsü.
 *
 * KRİTİK KISITLAMALAR:
 * 1. SCT3258'in tam teknik dokümantasyonu halka açık DEĞİLDİR.
 * 2. HPI (Host Port Interface) protokolü tam olarak bilinmemektedir.
 * 3. Bu başlık dosyası RadioFoundry araştırmaları ve topluluk bilgilerine
 *    dayanmaktadır. Doğrulanmamış bilgiler açıkça işaretlenmiştir.
 * 4. AMBE+2 vocoder DVSI patent koruması altındadır.
 *
 * BU SÜRÜCÜ TAMAMLANMAMIŞTIR.
 * Tam implementasyon için SCT3258 HPI protokolünün çözülmesi gerekmektedir.
 */

#ifndef DRIVER_SCT3258_H
#define DRIVER_SCT3258_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * SCT3258 Çalışma Modları
 * ============================================================ */

/**
 * @brief SCT3258 çalışma modu (doğrulanacak)
 */
typedef enum {
    SCT3258_MODE_IDLE     = 0,  /**< Bekleme modu */
    SCT3258_MODE_DMR_RX,        /**< DMR alım */
    SCT3258_MODE_DMR_TX,        /**< DMR iletim */
    SCT3258_MODE_DPMR_RX,       /**< dPMR alım */
    SCT3258_MODE_DPMR_TX,       /**< dPMR iletim */
    SCT3258_MODE_ANALOG_RX,     /**< Analog FM alım */
    SCT3258_MODE_ANALOG_TX,     /**< Analog FM iletim */
    SCT3258_MODE_PASSTHROUGH,   /**< Ses bypass (doğrulanacak) */
} sct3258_mode_t;

/**
 * @brief DMR slot numarası
 */
typedef enum {
    SCT3258_SLOT_1 = 1,
    SCT3258_SLOT_2 = 2,
} sct3258_slot_t;

/**
 * @brief DMR çağrı tipi
 */
typedef enum {
    SCT3258_CALL_GROUP    = 0,  /**< Grup çağrısı */
    SCT3258_CALL_PRIVATE  = 1,  /**< Özel çağrı */
    SCT3258_CALL_ALL      = 2,  /**< Genel çağrı */
} sct3258_call_type_t;

/**
 * @brief SCT3258 durum bilgisi
 */
typedef struct {
    sct3258_mode_t  mode;         /**< Aktif çalışma modu */
    sct3258_slot_t  active_slot;  /**< Aktif TDMA slotu */
    bool            synced;       /**< DMR senkronizasyon durumu */
    uint8_t         color_code;   /**< Aktif renk kodu */
    uint32_t        src_id;       /**< Kaynak DMR ID */
    uint32_t        dst_id;       /**< Hedef DMR ID/TG */
    sct3258_call_type_t call_type;/**< Çağrı tipi */
    bool            voice_active; /**< Ses aktif mi */
    bool            data_active;  /**< Veri aktif mi */
} sct3258_status_t;

/* ============================================================
 * API Fonksiyonları
 *
 * UYARI: Bu fonksiyonlar henüz IMPLEMENTASYON İÇERMEMEKTEDİR.
 * HPI protokolü çözülene kadar stub olarak kalacaktır.
 * ============================================================ */

/**
 * @brief SCT3258 sürücüsünü başlat
 * @return 0 başarılı, negatif hata kodu
 *
 * NOT: HPI bağlantı pinleri ve protokolü doğrulanmalıdır.
 */
int sct3258_init(void);

/**
 * @brief SCT3258 sürücüsünü kapat
 */
void sct3258_deinit(void);

/**
 * @brief SCT3258 çalışma durumunu kontrol et
 * @return true çalışıyor, false yanıt yok
 *
 * NOT: Bu fonksiyon HPI üzerinden basit bir iletişim testi yapar.
 */
bool sct3258_is_alive(void);

/**
 * @brief Çalışma modunu ayarla
 * @param mode Hedef mod
 * @return 0 başarılı, negatif hata kodu
 */
int sct3258_set_mode(sct3258_mode_t mode);

/**
 * @brief DMR parametrelerini ayarla
 * @param color_code Renk kodu (0-15)
 * @param slot       TDMA slot numarası
 * @param dst_id     Hedef DMR ID veya Talkgroup
 * @param call_type  Çağrı tipi
 * @return 0 başarılı, negatif hata kodu
 */
int sct3258_set_dmr_params(uint8_t color_code, sct3258_slot_t slot,
                           uint32_t dst_id, sct3258_call_type_t call_type);

/**
 * @brief DMR kaynak ID ayarla
 * @param src_id Yerel DMR ID
 * @return 0 başarılı, negatif hata kodu
 */
int sct3258_set_dmr_id(uint32_t src_id);

/**
 * @brief DMR TX başlat
 * @return 0 başarılı, negatif hata kodu
 */
int sct3258_dmr_tx_start(void);

/**
 * @brief DMR TX durdur
 * @return 0 başarılı, negatif hata kodu
 */
int sct3258_dmr_tx_stop(void);

/**
 * @brief SCT3258 durum bilgisini oku
 * @param status Durum yapısı (çıktı)
 * @return 0 başarılı, negatif hata kodu
 */
int sct3258_get_status(sct3258_status_t *status);

/**
 * @brief HPI register oku (araştırma/debug için)
 * @param addr HPI adresi
 * @return Register değeri, negatif hata kodu
 *
 * UYARI: HPI adresleri doğrulanmamıştır!
 */
int sct3258_hpi_read(uint16_t addr);

/**
 * @brief HPI register yaz (araştırma/debug için)
 * @param addr  HPI adresi
 * @param value Değer
 * @return 0 başarılı, negatif hata kodu
 *
 * UYARI: Yanlış HPI yazımı cihaza zarar verebilir!
 * BU FONKSİYONU YALNIZCA ARAŞTIRMA AMAÇLI KULLANIN.
 */
int sct3258_hpi_write(uint16_t addr, uint16_t value);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_SCT3258_H */
