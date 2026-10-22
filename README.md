# Smart Water Level Monitoring System — ESP32 + Sensor Ultrasonik

Sistem pemantau ketinggian air tangki berbasis ESP32 dan sensor ultrasonik HC-SR04.
ESP32 mengukur jarak ke permukaan air, menghitung level (%, cm, liter), menyalakan
LED/buzzer sebagai indikator, mengontrol pompa lewat relay, dan menampilkan semuanya
di dashboard web yang bisa dibuka dari HP atau laptop.

![alur](https://img.shields.io/badge/ESP32-Arduino-blue) ![sensor](https://img.shields.io/badge/sensor-HC--SR04-green)

## Daftar Isi
1. [Fitur](#fitur)
2. [Komponen](#komponen)
3. [Rangkaian (Wiring)](#rangkaian-wiring)
4. [Upload Program ke ESP32](#upload-program-ke-esp32)
5. [Cara Menggunakan](#cara-menggunakan)
6. [Penjelasan Kode](#penjelasan-kode)
7. [REST API](#rest-api)
8. [Troubleshooting](#troubleshooting)

---

## Fitur
- Pengukuran jarak dengan **median filter** (5 sampel) supaya tahan noise.
- Level air dalam **persen, cm, dan liter**.
- Indikator **LED hijau/kuning/merah** dan **buzzer** alarm.
- Kontrol **pompa via relay**: mode **Auto** (dengan histeresis) atau manual **ON/OFF**.
- **Dashboard web realtime** + grafik riwayat 10 menit, bisa dibuka di `http://waterlevel.local`.
- **Pengaturan tangki** dari dashboard, tersimpan permanen di flash ESP32.
- **Multi WiFi + portal setup**: WiFi diatur dari HP tanpa upload ulang, otomatis
  konek ke jaringan terkuat.

---

## Komponen

| Komponen | Jumlah | Keterangan |
|---|---|---|
| ESP32 DevKit 30 pin (ESP32-D0WD, chip USB CH340) | 1 | Board utama |
| Sensor ultrasonik HC-SR04 | 1 | Bisa diganti JSN-SR04T (tahan air) |
| Resistor 1kΩ dan 2kΩ | 1 + 1 | Pembagi tegangan pin ECHO |
| LED hijau, kuning, merah + resistor 220Ω | 3 | Opsional, indikator |
| Buzzer aktif | 1 | Opsional, alarm |
| Modul relay 1 channel | 1 | Opsional, kontrol pompa |
| Breadboard + kabel jumper (male/female) | — | |
| Kabel USB Type-C (data, bukan hanya charging) | 1 | |

---

## Rangkaian (Wiring)

### Sensor HC-SR04 (wajib)

| HC-SR04 | ESP32 | Catatan |
|---|---|---|
| VCC | **VIN** (5V) | HC-SR04 butuh 5V. Di 3V3 sensor tidak memancarkan ultrasonik |
| GND | **GND** | Pastikan GND, bukan 3V3 (letaknya bersebelahan) |
| TRIG | **D5** (GPIO 5) | |
| ECHO | **D17** (GPIO 17) | **Lewat pembagi tegangan** |

**Pembagi tegangan ECHO.** HC-SR04 mengeluarkan sinyal 5V, sedangkan pin ESP32 maksimal 3.3V:

```
ECHO sensor ──[1kΩ]──●──[2kΩ]── GND
                     │
                    D17
```

Bisa juga memakai pasangan lain dengan rasio sekitar 1:2 (misal 10kΩ + 20kΩ, atau 4.7kΩ + 10kΩ).
Jika memakai **HC-SR04P** (versi 3.3V) dengan VCC di 3V3, pembagi tegangan tidak diperlukan.

### Indikator & relay (opsional)

| Komponen | ESP32 |
|---|---|
| LED hijau (PENUH) | D25 → resistor 220Ω → LED → GND |
| LED kuning (SEDANG) | D26 → resistor 220Ω → LED → GND |
| LED merah (RENDAH / error) | D27 → resistor 220Ω → LED → GND |
| Buzzer aktif (+) | D14, kaki (−) ke GND |
| Relay IN / VCC / GND | D23 / VIN / GND |

Semua pin bisa diubah di `WaterLevelMonitor/config.h`.

### Tips: ESP32 terlalu lebar untuk breadboard
ESP32 DevKit 30 pin menutupi semua lubang di satu sisi breadboard, sehingga pin VIN tidak
bisa dijangkau. Solusinya:
- **Satu sisi menggantung**: tancapkan hanya baris pin kanan (D23…3V3) di tepi breadboard,
  biarkan baris kiri (VIN, GND, D13, …) di luar, lalu pasang kabel **female** langsung ke pin VIN.
- **Dua breadboard berdampingan**: lepas satu power rail, letakkan ESP32 di tengah sambungan.
- **Tanpa breadboard**: kabel female-to-female langsung dari pin ESP32 ke pin sensor.

> Jangan letakkan board di atas permukaan logam (misalnya jaring besi), pin bisa korsleting.

### Pemasangan sensor di tangki
Pasang sensor di tutup tangki, **menghadap lurus ke bawah** ke permukaan air. Ukur:
- **Jarak sensor ke dasar tangki** (= tinggi tangki dari posisi sensor)
- **Jarak sensor ke permukaan air saat penuh** (minimal ±3 cm karena ada *blind zone*)

```
   ┌──[ sensor ]──┐   ─┬─            ─┬─
   │              │    │ jarak penuh   │
   │~~~~~~~~~~~~~~│   ─┴─ (air penuh)  │ jarak sensor
   │              │                    │ ke dasar tangki
   │     air      │                    │
   │              │                    │
   └──────────────┘                   ─┴─
```

---

## Upload Program ke ESP32

### Pakai arduino-cli (macOS)
```bash
brew install arduino-cli
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32

# (Opsional) WiFi bawaan
cp WaterLevelMonitor/secrets.example.h WaterLevelMonitor/secrets.h   # lalu isi SSID & password

# Cari port ESP32 (biasanya /dev/cu.usbserial-xxx)
arduino-cli board list

arduino-cli compile --fqbn esp32:esp32:esp32 WaterLevelMonitor
arduino-cli upload  --fqbn esp32:esp32:esp32:UploadSpeed=115200 -p /dev/cu.usbserial-110 WaterLevelMonitor
arduino-cli monitor -p /dev/cu.usbserial-110 -c baudrate=115200
```

> **Upload speed 115200 wajib** untuk board dengan chip CH340. Kecepatan bawaan (921600)
> sering gagal dengan error `Unable to verify flash chip connection`.

### Pakai Arduino IDE
1. Tambahkan URL board ESP32 di *File → Preferences → Additional boards manager URLs*:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
2. *Tools → Board → Boards Manager*, install **esp32 by Espressif**.
3. Buka `WaterLevelMonitor/WaterLevelMonitor.ino`.
4. Pilih board **ESP32 Dev Module**, *Upload Speed* **115200**, dan port `usbserial`.
5. Klik **Upload**, lalu buka Serial Monitor (115200 baud).

### Pakai PlatformIO
```bash
pio run -t upload
pio device monitor
```

Program tidak membutuhkan library tambahan, karena semua library sudah bawaan core ESP32.

---

## Cara Menggunakan

### 1. Hubungkan ke WiFi (pertama kali)
1. Nyalakan ESP32. Jika belum ada WiFi tersimpan (atau WiFi tersimpan tidak ada di sekitar
   selama 20 detik), ESP32 membuat WiFi sendiri: **`WaterLevel-ESP32`**, password **`12345678`**.
2. Sambungkan HP ke WiFi tersebut. Halaman setup biasanya muncul otomatis; jika tidak,
   buka **`http://192.168.4.1`**.
3. Di bagian **WiFi**, tekan **Cari WiFi di sekitar**, pilih jaringan, isi password, lalu
   **Simpan & Hubungkan**.
4. Setelah muncul "Berhasil", sambungkan HP kembali ke WiFi tersebut.

ESP32 hanya mendukung WiFi **2.4 GHz**. Jaringan 5 GHz tidak akan terdeteksi.

### 2. Buka dashboard
Buka **`http://waterlevel.local`** (atau IP yang tampil di dashboard/Serial Monitor) dari
perangkat yang terhubung ke WiFi yang sama.

| Bagian | Isi |
|---|---|
| Level Air | Gambar tangki, persentase, liter, status (RENDAH / SEDANG / PENUH / SENSOR ERROR) |
| Detail | Jarak sensor, tinggi air, status pompa, sinyal WiFi, IP, uptime, tombol pompa |
| Riwayat | Grafik level air 10 menit terakhir (selama halaman terbuka) |
| Pengaturan Tangki | Ukuran tangki, kapasitas, batas rendah/penuh, buzzer |
| WiFi | Status koneksi, jaringan tersimpan, cari & tambah WiFi |

### 3. Kalibrasi tangki
Di **Pengaturan Tangki**, isi lalu klik **Simpan**:
- **Jarak sensor ke dasar tangki (cm)**: ukur saat tangki kosong.
- **Jarak sensor ke air saat penuh (cm)**
- **Kapasitas tangki (liter)**
- **Batas rendah (%)**: di bawah ini status RENDAH dan pompa menyala (mode Auto).
- **Batas penuh (%)**: di atas ini status PENUH dan pompa mati (mode Auto).

Pengaturan tersimpan di flash dan tetap ada walau ESP32 dimatikan.

**Tes di meja tanpa tangki**: isi tinggi tangki 50 cm, jarak penuh 5 cm, lalu gerakkan
tangan mendekat/menjauh dari sensor. Persentase akan naik/turun seperti air.

### 4. Kontrol pompa
| Mode | Perilaku |
|---|---|
| **Auto** | Pompa ON saat level ≤ batas rendah, OFF saat ≥ batas penuh. Di antaranya kondisi dipertahankan (histeresis) supaya pompa tidak nyala-mati terus. Pompa OFF jika sensor error. |
| **ON** | Pompa menyala manual, tetapi tetap dimatikan otomatis jika level ≥ 98% (anti luber). |
| **OFF** | Pompa mati. |

Mode pompa tersimpan dan tetap berlaku setelah restart.

### 5. Indikator
| Level | Status | LED | Buzzer |
|---|---|---|---|
| ≤ batas rendah | RENDAH | merah | bunyi pendek tiap 2 detik |
| di antaranya | SEDANG | kuning | — |
| ≥ batas penuh | PENUH | hijau | — |
| sensor tidak membaca | SENSOR ERROR | merah berkedip | bunyi pendek tiap 2 detik |

Buzzer bisa dimatikan dari Pengaturan Tangki.

### 6. Pindah lokasi / ganti WiFi
- ESP32 menyimpan sampai **5 jaringan**, dan otomatis konek ke yang terkuat di sekitar.
- Saat offline, ESP32 mencari ulang setiap 30 detik. Begitu kembali ke lokasi lama, ia konek sendiri.
- Untuk menambah WiFi saat ESP32 masih terhubung ke jaringan lain, buka dashboard → **WiFi**,
  atau **tahan tombol BOOT 3 detik** untuk membuka portal setup selama 5 menit.

---

## Penjelasan Kode

### Struktur file
```
WaterLevelMonitor/
├── WaterLevelMonitor.ino   # program utama: sensor, pompa, LED/buzzer, web server
├── WifiConnector.h/.cpp    # manajemen WiFi: multi jaringan, scan, portal setup
├── web_page.h              # halaman dashboard (HTML + CSS + JS) disimpan di flash
├── config.h                # pin, default tangki, parameter sensor & WiFi
├── secrets.example.h       # template WiFi bawaan (salin jadi secrets.h)
└── secrets.h               # WiFi bawaan milikmu (tidak ikut git)
platformio.ini              # konfigurasi PlatformIO
```

### Alur program
```
setup()
 ├─ set relay OFF dulu (pompa tidak sempat nyala saat boot)
 ├─ set mode pin sensor, LED, buzzer, tombol BOOT
 ├─ loadSettings()      ← baca pengaturan tangki dari flash
 ├─ wifi.begin()        ← muat daftar WiFi, mulai mencari
 ├─ MDNS.begin()        ← nama waterlevel.local
 └─ setupServer()       ← daftarkan halaman & API

loop()  (berulang terus, tidak pernah berhenti/blocking lama)
 ├─ wifi.loop()         ← state machine WiFi + DNS portal
 ├─ server.handleClient() ← layani request dashboard
 ├─ checkBootButton()   ← tombol BOOT 3 detik → portal
 ├─ tiap 1 detik:
 │    ├─ updateLevel()  ← ukur jarak → hitung %
 │    ├─ updatePump()   ← logika pompa
 │    └─ log ke Serial
 └─ updateIndicators()  ← LED & buzzer
```

### 1. Membaca sensor: `readDistanceOnce()` & `readDistanceMedian()`
HC-SR04 bekerja seperti sonar:
1. ESP32 memberi pulsa **HIGH 10 µs** di pin TRIG.
2. Sensor memancarkan 8 gelombang ultrasonik 40 kHz.
3. Pin ECHO menjadi HIGH selama gelombang pergi dan kembali.
4. `pulseIn()` mengukur lama pulsa ECHO dalam mikrodetik.

```cpp
float d = duration * SOUND_SPEED_CM_PER_US / 2.0f;   // 0.0343 cm/µs, dibagi 2 (pergi-pulang)
```

Bacaan di luar rentang 2–400 cm atau timeout (tidak ada pantulan) dianggap gagal.
`readDistanceMedian()` mengambil **5 sampel** berjeda 60 ms, membuang yang gagal, lalu
mengambil **nilai tengah (median)**. Median lebih tahan terhadap bacaan liar daripada rata-rata.
Jika lebih dari separuh sampel gagal, hasilnya dianggap `SENSOR_ERROR`.

### 2. Menghitung level: `updateLevel()`
```cpp
span = tankHeight - fullDistance;                       // rentang jarak kosong→penuh
percent = (tankHeight - distance) / span * 100;         // dibatasi 0..100
level_cm = tankHeight - distance;
liters  = capacity * percent / 100;
```
Semakin dekat permukaan air ke sensor, jarak semakin kecil dan persentase semakin besar.

### 3. Logika pompa: `updatePump()`
Memakai **histeresis**: pompa baru menyala di batas rendah dan baru mati di batas penuh.
Tanpa histeresis, pompa akan menyala-mati berulang kali saat level di sekitar satu titik.
`setRelay()` membalik logika untuk relay **aktif LOW** (`RELAY_ACTIVE_LOW`). Di `setup()`,
pin relay di-set OFF **sebelum** dijadikan output supaya pompa tidak berkedip nyala saat boot.

### 4. LED & buzzer: `updateIndicators()`
Dipanggil setiap putaran `loop()`. Kedip dan bunyi buzzer dihitung dari `millis()`
(bukan `delay()`), jadi tidak menghambat web server.

### 5. Penyimpanan pengaturan: `loadSettings()` / `saveSettings()`
Memakai library **Preferences** (penyimpanan key-value di flash ESP32, namespace
`waterlevel`). Nilai default diambil dari `config.h` jika belum pernah disimpan.

### 6. Web server & dashboard
- `WebServer` di port 80 melayani halaman `/` dan endpoint `/api/*` (lihat [REST API](#rest-api)).
- Halaman dashboard ada di `web_page.h` sebagai string `PROGMEM` (disimpan di flash, bukan RAM).
- JavaScript di halaman memanggil `/api/status` **setiap 2 detik**, memperbarui tampilan,
  dan menggambar grafik di `<canvas>`.
- Pengaturan dikirim via `POST` form (`application/x-www-form-urlencoded`) dan divalidasi di
  ESP32 sebelum disimpan (misal batas rendah harus < batas penuh).
- **mDNS** membuat ESP32 bisa diakses dengan nama `waterlevel.local`, tanpa perlu tahu IP.

### 7. Manajemen WiFi: `WifiConnector`
State machine non-blocking dengan 4 status:

```
        ┌────────── gagal / timeout 15 s ──────────┐
        ▼                                          │
     [IDLE] ──tiap 30 s──► [SCANNING] ──ketemu──► [CONNECTING] ──berhasil──► [ONLINE]
        ▲                     │ tidak ada                                       │
        │                     └──────────────► IDLE                             │
        └──────────────────────── koneksi putus ────────────────────────────────┘
```

- **Scan async** (`WiFi.scanNetworks(true)`), sehingga dashboard tetap responsif saat mencari.
- Dari hasil scan, dipilih jaringan **tersimpan dengan sinyal terkuat**.
- Jika offline lebih dari 20 detik → **portal setup** dibuka: mode `WIFI_AP_STA` (ESP32 menjadi
  Access Point sambil tetap bisa mencari WiFi), plus **DNSServer** yang mengarahkan semua
  domain ke `192.168.4.1`. Inilah yang membuat halaman setup muncul otomatis di HP
  (*captive portal*). `handleNotFound()` me-redirect permintaan pengecekan internet dari HP.
- Setelah berhasil konek, portal dibiarkan hidup **30 detik** supaya HP sempat melihat IP baru,
  lalu ditutup.
- Daftar jaringan (maks 5) disimpan di Preferences namespace `wifi`. Jika penuh, jaringan
  paling lama dibuang.
- `WIFI_SSID` dari `secrets.h` hanya dipakai sebagai bawaan saat flash belum punya jaringan.

### 8. Konfigurasi: `config.h`
Semua angka penting ada di satu tempat:

| Konstanta | Default | Fungsi |
|---|---|---|
| `PIN_TRIG`, `PIN_ECHO` | 5, 17 | Pin sensor |
| `PIN_LED_*`, `PIN_BUZZER`, `PIN_RELAY` | 25/26/27, 14, 23 | Pin output |
| `RELAY_ACTIVE_LOW` | `true` | Logika modul relay |
| `DEFAULT_TANK_HEIGHT_CM` | 100 | Default jarak sensor ke dasar |
| `DEFAULT_FULL_DISTANCE_CM` | 5 | Default jarak saat penuh |
| `DEFAULT_LOW_PERCENT` / `HIGH` | 20 / 90 | Batas pompa ON / OFF |
| `OVERFLOW_PERCENT` | 98 | Pengaman mode manual ON |
| `MEASURE_INTERVAL_MS` | 1000 | Jeda antar pengukuran |
| `SAMPLES_PER_READING` | 5 | Jumlah sampel median |
| `AP_SSID` / `AP_PASSWORD` | WaterLevel-ESP32 / 12345678 | WiFi portal setup |
| `WIFI_RETRY_INTERVAL_MS` | 30000 | Jeda mencari ulang WiFi |
| `WIFI_PORTAL_AFTER_MS` | 20000 | Offline selama ini → portal dibuka |

---

## REST API
Semua respons dalam format JSON. Body `POST` berupa form (`key=value&...`).

| Method | Endpoint | Fungsi |
|---|---|---|
| GET | `/api/status` | Level, jarak, liter, status, pompa, WiFi, uptime |
| GET | `/api/config` | Pengaturan tangki |
| POST | `/api/config` | Ubah pengaturan: `tank_height_cm`, `full_distance_cm`, `capacity_liters`, `low_percent`, `high_percent`, `buzzer` |
| POST | `/api/pump` | `mode=auto` \| `on` \| `off` |
| GET | `/api/wifi` | Status koneksi + daftar WiFi tersimpan |
| POST → GET | `/api/wifi/scan` | Mulai scan, lalu ambil hasilnya |
| POST | `/api/wifi/add` | `ssid`, `password` (simpan & langsung konek) |
| POST | `/api/wifi/remove` | `ssid` |

Contoh:
```bash
curl http://waterlevel.local/api/status
# {"distance_cm":15.1,"level_cm":84.9,"percent":89.4,"liters":894,"status":"MEDIUM",
#  "sensor_ok":true,"pump_on":false,"pump_mode":"auto","rssi":-53,"ip":"192.168.18.69","uptime_s":150}

curl -X POST -d "mode=on" http://waterlevel.local/api/pump
curl -X POST -d "tank_height_cm=120&full_distance_cm=10" http://waterlevel.local/api/config
```

---

## Troubleshooting

| Gejala | Penyebab & solusi |
|---|---|
| Upload gagal `Unable to verify flash chip connection` | Turunkan upload speed ke **115200**. Gunakan kabel USB data. |
| Port tidak muncul di `arduino-cli board list` | Kabel hanya untuk charging, atau driver CH340 belum terpasang. |
| `SENSOR ERROR` terus | Cek: VCC sensor di **VIN** (bukan 3V3), GND di **GND** (bukan 3V3), TRIG di D5, ECHO di D17. Pastikan kabel tidak kendor. |
| Angka sensor sering hilang/acak | Arahkan sensor ke permukaan datar dan keras, jarak 5–200 cm. Tekan ulang kabel jumper. |
| ESP32 tidak bisa konek WiFi | Pastikan WiFi **2.4 GHz** dan password benar. Jaringan 5 GHz tidak didukung. |
| `waterlevel.local` tidak terbuka | Beberapa HP Android lama tidak mendukung mDNS. Gunakan IP dari dashboard atau router. |
| Portal setup tidak muncul otomatis | Buka manual `http://192.168.4.1` setelah tersambung ke `WaterLevel-ESP32`. |
| Pompa nyala saat ESP32 restart | Cek setting `RELAY_ACTIVE_LOW` sesuai jenis modul relay. |

Untuk melihat log, buka Serial Monitor (115200 baud):
```
[WIFI] Terhubung ke WiFiRumah, IP: 192.168.18.69
Jarak: 15.1 cm | Level: 84.9 cm (89.4%) | 894 L | MEDIUM | Pompa: OFF (auto)
```
