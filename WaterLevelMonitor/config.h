#pragma once

// ===================== WiFi =====================
// Jaringan WiFi diatur dari dashboard (bagian "WiFi") dan tersimpan di flash, maks 5 jaringan.
// ESP32 otomatis konek ke jaringan tersimpan yang sinyalnya paling kuat.
// WIFI_SSID di bawah hanya dipakai sebagai bawaan saat flash belum punya jaringan tersimpan.
// Kosongkan ("") jika ingin langsung masuk portal setup.
// Isi di secrets.h (salin dari secrets.example.h), file itu tidak ikut ke git.
#if __has_include("secrets.h")
#include "secrets.h"
#else
#define WIFI_SSID      ""
#define WIFI_PASSWORD  ""
#endif

// Portal setup: jika tidak ada WiFi yang bisa dipakai, ESP32 membuat WiFi sendiri.
// Sambungkan HP ke AP_SSID, halaman setup akan muncul otomatis (atau buka http://192.168.4.1).
#define AP_SSID        "WaterLevel-ESP32"
#define AP_PASSWORD    "12345678"      // minimal 8 karakter
#define MDNS_HOSTNAME  "waterlevel"    // dashboard: http://waterlevel.local

#define WIFI_CONNECT_TIMEOUT_MS 15000
#define WIFI_RETRY_INTERVAL_MS  30000   // jeda mencari ulang jaringan tersimpan saat offline
#define WIFI_PORTAL_AFTER_MS    20000   // offline selama ini -> portal setup dibuka
#define WIFI_PORTAL_LINGER_MS   30000   // portal tetap hidup sebentar setelah berhasil konek
#define PORTAL_BUTTON_HOLD_MS   3000    // tahan tombol BOOT selama ini untuk membuka portal
#define PORTAL_MANUAL_MS        300000  // portal yang dibuka manual bertahan 5 menit

// ===================== Pin =====================
#define PIN_TRIG        5
#define PIN_ECHO        17   // D17/TX2. HC-SR04 5V: WAJIB pakai pembagi tegangan (lihat README)
#define PIN_LED_GREEN   25
#define PIN_LED_YELLOW  26
#define PIN_LED_RED     27
#define PIN_BUZZER      14   // buzzer aktif
#define PIN_RELAY       23   // relay pompa (opsional)
#define RELAY_ACTIVE_LOW true // kebanyakan modul relay aktif LOW
#define PIN_BOOT_BUTTON 0    // tombol BOOT bawaan board

// ===================== Default tangki =====================
// Bisa diubah lewat dashboard, tersimpan permanen di flash.
#define DEFAULT_TANK_HEIGHT_CM    100.0f // jarak sensor ke DASAR tangki (tangki kosong)
#define DEFAULT_FULL_DISTANCE_CM  5.0f   // jarak sensor ke permukaan air saat PENUH
#define DEFAULT_CAPACITY_LITERS   1000.0f
#define DEFAULT_LOW_PERCENT       20     // di bawah ini: status RENDAH, pompa ON (mode auto)
#define DEFAULT_HIGH_PERCENT      90     // di atas ini: status PENUH, pompa OFF (mode auto)
#define OVERFLOW_PERCENT          98     // pengaman: pompa manual ON tetap dimatikan di atas ini

// ===================== Sensor =====================
#define MEASURE_INTERVAL_MS     1000
#define SAMPLES_PER_READING     5
#define SAMPLE_GAP_MS           60      // HC-SR04 butuh jeda ~60 ms antar ping
#define ECHO_TIMEOUT_US         30000UL // ~5 m
#define SENSOR_MIN_CM           2.0f
#define SENSOR_MAX_CM           400.0f
#define SOUND_SPEED_CM_PER_US   0.0343f // ~20°C
