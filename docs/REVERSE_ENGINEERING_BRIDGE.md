# Tersine Mühendislik Köprüsü

**Kaynak:** `reverse-engineering/` (V4.00 stok firmware tersine mühendisliği)  
**Hedef:** `firmware/` — AT-D878 Open Firmware geliştirme

Bu belge, stok firmware'den elde edilen doğrulanmış donanım ve yazılım
bulgularını Open Firmware'in sürücü ve mimari kararlarına nasıl yansıtması
gerektiğini açıklar. `docs/HARDWARE_ANALYSIS.md`'deki "⚠️ Doğrulanacak"
maddelerinin büyük bölümü burada yanıtlanmıştır.

---

## 1. AT1846S RF Transceiver — Gerçek Bus Arayüzü

**HARDWARE_ANALYSIS.md hatası:** "I2C veya 3/4-wire SPI (doğrulanacak)"  
**RE bulgusu (OVERVIEW.md, stok firmware ölçümleri):** AT1846S, I2C veya standart SPI
üzerinden değil, **bit-banged 3-wire GPIO** üzerinden kontrol edilir.

| Sinyal | MCU Pini | Stok firmware sabiti |
|--------|----------|----------------------|
| SCLK   | PB3      | `at1846_sclk`        |
| SDIO   | PB4      | `at1846_sdio`        |
| SEN    | PB5      | `at1846_sen`         |

Protokol: 7-bit register adresi + 16-bit değer, `0x80` = okuma biti.  
Stok fonksiyonlar: `at1846_write_reg` @ `0x0807A6FC` (324 çağrı), `at1846_read_reg`.

**Mevcut `driver_at1846s.c` sorunu:** Sürücü `hal_i2c_write_reg` / `hal_i2c_read_reg`
kullanıyor. Bu **yanlış**. `hal_gpio` ile bit-banging olarak yeniden yazılmalı.

**Frekans yazma protokolü (stok firmware, doğrulanmış):**

```c
// Arg: 10 Hz biriminde frekans
// Reg 0x29 / 0x2A = f_kHz * 16  (AT1846S ölçekleme; NOT: f_Hz / xtal değil)
// VHF/UHF split: 300 MHz'de, reg 0x05 spura göre seçilir
void at1846_set_freq(uint32_t freq_10hz);  // @ 0x0805F504 (stok)
```

> `driver_at1846s.c`'deki `((uint64_t)freq_hz << 16) / xtal` hesabı yanlış.
> Doğrusu: `reg_val = (freq_hz / 10000) * 16` — yani `freq_kHz * 16`.

**VCO sınırları (stok firmware ölçümü, bir radyo üzerinde):**

| Bant | Bölücü | Kilit aralığı | Kullanılabilir |
|------|--------|---------------|----------------|
| VHF  | ÷6     | 129.84–178.17 MHz | 131–178 MHz |
| 220  | ÷4     | 194.75–288.73 MHz | 200–286 MHz |
| UHF  | ÷2     | 389.48–~577.6 MHz | 393–572 MHz |

Stok firmware bant sınırını klipler; 300 MHz üstü = UHF, altı = VHF (BCD byte).

**PLL kilit durumu:**  
- Reg `0x0D[15]` = lock flag; `0x13[8:0]` = VCO cap-bank (511 = sınır dışı)  
- Reg `0x29/0x2A` geri okuma: VCO frekansını ÷6 olarak döndürür (reg 05 = `8763`'te)

**RX yeniden tune sırası (stok firmware, 175 ms normal yol):**
```
rx_freq_a/b yaz → rf_freq_select_active() → rf_cmd_post(1) → ~175 ms bekle
```
Hızlı tarama için (scope.py'de uygulanan): reg `0x29/0x2A` yaz → reg `0x30`
RX bit off/on → ~2.5 ms, kilit <2 ms'de.

**RX izleme filtresi:** TIM3 CCR1 PWM. Her frekans değişiminde
`rf_frontend_tune_from_tables` çağrılmalı, aksi takdirde hassasiyet ~25 dB düşer.

**RX front-end GPIO'ları:** AT1846S reg `0x1F`:
- `[9:8]` = VHF front-end: `11` açık / `10` kapalı
- `[1:0]` = UHF front-end: `11` açık / `10` kapalı

---

## 2. LCD — Gerçek Arayüz

**HARDWARE_ANALYSIS.md hatası:** "SPI (muhtemelen), 128×160 (doğrulanacak)"  
**RE bulgusu:** ST7735S, **8080 paralel** arayüz, **160×128** piksel.

| Sinyal | MCU Pini |
|--------|----------|
| Data[7:0] | PE7–PE14 |
| D/CX | PB10 |
| RST  | PB11 |
| CS   | PB12 |
| WR   | PB13 |

Stok fonksiyonlar: `lcd_init_st7735`, `lcd_draw_text` (ASCII + GB2312).  
Renk: RGB565. `driver_display.c` SPI varsayımıyla yazılmışsa EXMC/GPIO paralel'e taşınmalı.

---

## 3. SCT3258 DMR Baseband — HPI Protokolü

**RE bulgusu:** 8-bit HPI (paralel), adres satırı yok.

| Sinyal | MCU Pini |
|--------|----------|
| Data[7:0] | PD8–PD15 |
| HCSN | PD2 |
| HWRN | PD3 |
| HRDN | PD4 |

Stok erişim: `hpi_write_byte/buf`, `hpi_read_byte` @ `0x080823BA`.  
Wrapper'lar: `0x0808711A / 32 / 3E`.

Op-code dispatcher'lar: `sct_query` @ `0x0808B91A`, `sct_command` @ `0x0808C7D2`.  
SCT3258 komut kuyruğu 40 girişli (`dmr_bb_cmd_enqueue(op,a,b,c)`).

---

## 4. BK1080 FM Alıcı

**RE bulgusu:** Bağımsız IC, I2C üzerinde.

| Özellik | Değer |
|---------|-------|
| Arayüz | I2C: PB6 SDA / PB7 SCL |
| I2C Adresi | 0x20 |
| Register protokolü | Si470x-style sıralı yazma, reg 0x02'den başlar |

Stok fonksiyonlar: `fm_chip_init`, `fm_set_volume`.  
AT1846S FM alıcı ile karıştırılmamalı; bunlar ayrı devreler.

---

## 5. GPS

**RE bulgusu:** `0x40004400` = ST USART2 (GD "USART1"), **9600 baud**, NMEA.  
Stok: `nmea_rx_char`, `gps_module_config` (MTK/CASIC/$CFG komutları; UBX yok).

---

## 6. Bluetooth / APRS LinkBoard (BHT V2.00)

**RE bulgusu:** UART4 (PC10/PC11), iki mod:
- Binary çerçeve: `7E len cmd payload sum EF` (`bt_send_frame`)
- ASCII AT: `AT+VER?`, `+APRS:` satırları RX frame taşır (`bt_send_at`)

Güç pini: PE15. Probe sırası: 115200 baud `AT+VER?` → `"APRS…"` cevabı =
`aprs_board_present`; sonra 9600'e düşer `bt_probe`.

---

## 7. SPI NAND Flash

**RE bulgusu:** CS = PB8, GigaDevice (ID 0xC8).  
Stok `R` komutu: 128 KB blok bazında A/B-bank seçimiyle mantıksal okuma.  
`W` sadece ≥ 0x800000 adreslerine yazar; `H` = factory block 0x2FA0000.  
SPI NAND 1 Gbit (D878UV); UVII Plus 2 Gbit.

---

## 8. USB CDC

**RE bulgusu:** GD32 VID `28e9:018a`, OUT EP3 64 B, IN EP `0x81`.  
Stok fonksiyonlar: `usb_cdc_rx_arm`, `usb_cdc_tx`.  
Programlama protokolü: `R` + BE32 addr + len (1..255) = SPI NAND oku (~150 KB/s);
`W` = yaz; `END` = reboot. Debug patch ile ek: peek/poke/call/key/screen komutları.

---

## 9. Ses Yolu ve APRS RX

**RE bulgusu:** ADC1 ch10 = **PC0** = pre-squelch RX audio (12-bit, DMA-dairesel,
ch11=PC1 pil, ch12=PC2 bilinmiyor → `0x20001048..4D`).

TIM6_IRQHandler ses modları:
- Mod 1: Tone sampling, 4× tone frekansı
- Mod 2: 9.02 kHz 8-bit capture
- Mod 3: 11.2 kHz zero-crossing periyotları

**Önemli:** TIM6 sahipsiz — `rf_cmd_apply_lo` her RX girişinde yeniden yapılandırır;
herhangi bir TIM6 kullanıcısı retune/TX sonrası yeniden arm etmeli.

**MCU üzerinde APRS RX mümkün:** ADC PC0 + TIM6 ile yazılımsal AFSK1200 demodülasyonu
çalışıyor (dış board gerekmez). Stok firmware `aprs_rx` modu bunu doğruladı.

**TX audio yolu:** PB9 = TIM4 CH4 PWM DAC (76.8 kHz, periyot 1562), DMA1 ch7.

---

## 10. Görev Mimarisi (Stok Firmware, Open Firmware İçin Referans)

Stok firmware **uC/OS-II** (200 Hz tick, LOWEST_PRIO=63, 34 TCB) kullanır.
Open Firmware FreeRTOS veya Zephyr kullanabilir ama öncelik yapısı referans alınabilir:

| Öncelik | Görev | Açıklama |
|---------|-------|----------|
| 0 | OSTmr_Task | 100 Hz timer (en yüksek!) |
| 3 | usb_pc_link | USB CDC CPS/debug |
| 4 | bt_uart4 | BT/APRS daughterboard |
| 5 | gps | NMEA parse |
| 6 | sig_encoder | DTMF/2-tone/AFSK TX |
| 7 | rf_cmd | RF komut yürütücü |
| 8 | dmr_bb | DMR baseband servisi |
| 9 | tone | Bip/ton |
| 10 | keys_ui | Tuş/menü |
| 11 | event_handler | Bildirimler, popup |
| 12 | display | Ekran renderer |
| 14 | housekeeping | 10 ms periyodik |

**Kritik kesme öncelikleri:** USB (IRQ 20) ve TIM6 **0x10** önceliğinde — OS
kritik bölgelerini (BASEPRI=0x20) aşarlar. Bu ISR'lardan hiçbir zaman OS çağrısı yapma.
IRQ 25 @ 0x30 önceliği güvenli proxy olarak kullanılabilir.

---

## 11. Flash Bellek Haritası (Stok Firmware, Referans)

| Adres | İçerik |
|-------|--------|
| `0x08000000–0x08003FFF` | Bootloader (16 KB, güncelleme dosyasında YOK) |
| `0x08004000` | Uygulama başlangıcı (SP, Reset Vector) |
| `0x08004000–0x080A0000` | Kod |
| `0x080A0000+` | String/menü/init tabloları |
| `0x080E7600–0x080F57FF` | **Kullanılmayan boş alan (56.5 KB)** |
| `0x080F5800–0x080FFFFF` | Firmware'in kendi flash yazma bölgesi |

Bootloader jump koşulu: `[0x08004000]` geçerli bir RAM SP gibi görünüyorsa.
Checksum yok; `.CDI`/`.spi` host-side uzunluk bilgisi içerir, bootloader doğrulamaz.

---

## 12. RE Araçlarını Studio'dan Kullanmak

Studio'nun *Radio I/O* sekmesi `tools/radio_io.py` aracılığıyla şu RE
araçlarına erişebilir:

```python
from tools.radio_io import RadioIO

rio = RadioIO(port="COM3", baud=921600)
rio.dump("backup.bin")          # SPI NAND dump
rio.flash("firmware.CDD")       # Flash yaz
rio.peek(0x200003C0, 4)         # RAM oku (debug patch gerekli)
rio.poke(0x200003C0, b"\x00\x00\x20\x0A")  # RAM yaz
```

Bkz: `../tools/radio_io.py` ve `reverse-engineering/tools/radio.py`.

---

## 13. Hızlı Referans: Önemli RAM Adresleri

| Adres | İsim | Tip | Açıklama |
|-------|------|-----|----------|
| `0x2000130A` | `OSRunning` | u8 | uC/OS-II koşuyor mu |
| `0x20000951` | `screen_id` | u8 | Aktif ekran ID'si |
| `0x20007FF8` | `radio_settings` | u8[256] | Codeplug mirror |
| `0x2000A33C` | `region_info` | u8[8] | +3 = bölge indexi |
| `0x200003C0` | `rx_freq_10hz` | u32 | RX frekansı (10 Hz birim) |
| `0x200003B4` | `tx_freq_10hz` | u32 | TX frekansı (10 Hz birim) |
| `0x200003E4` | `rf_chan_rec` | ptr | RF kanal kaydı ptr |
| `0x20000BF4` | RF cmd mailbox | — | rf_cmd_post hedefi |
| `0x20000BED` | RF cmd byte | u8 | 1=RX re-enter, 0x0E=TX |

---

*Bu belge `reverse-engineering/OVERVIEW.md`, `docs/patch.md`, `docs/pc_link.md`,
`docs/debug_channel.md` ve stok firmware ölçümlerinden derlenerek oluşturulmuştur.*  
*Stok firmware kodu burada bulunmaz; yalnızca davranış ve adres bilgileri içerir.*
