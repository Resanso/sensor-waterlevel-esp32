/*
 * Smart Water Level Monitoring System
 * ESP32 + Sensor Ultrasonik HC-SR04 / JSN-SR04T
 *
 * Fitur:
 *  - Ukur jarak dengan median filter (tahan noise)
 *  - Hitung level air (%, cm, liter)
 *  - Indikator LED hijau/kuning/merah + buzzer alarm
 *  - Kontrol pompa via relay (mode AUTO dengan histeresis, atau manual ON/OFF)
 *  - Dashboard web realtime + REST API (http://waterlevel.local)
 *  - Pengaturan tangki tersimpan di flash (Preferences)
 *  - Multi WiFi + portal setup: WiFi diatur dari dashboard, tanpa upload ulang
 *
 * Board: ESP32 Dev Module. Tidak butuh library tambahan (semua bawaan core ESP32).
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>

#include "config.h"
#include "web_page.h"
#include "WifiConnector.h"

enum PumpMode : uint8_t { PUMP_AUTO = 0, PUMP_MANUAL_ON = 1, PUMP_MANUAL_OFF = 2 };

struct Settings {
  float tankHeightCm;
  float fullDistanceCm;
  float capacityLiters;
  int lowPercent;
  int highPercent;
  bool buzzerEnabled;
  PumpMode pumpMode;
};

Settings settings;
Preferences prefs;
WebServer server(80);
WifiConnector wifi;

float distanceCm = -1;
float levelPercent = 0;
bool sensorOk = false;
bool pumpOn = false;
unsigned long lastMeasureMs = 0;
unsigned long bootPressedSinceMs = 0;

// ===================== Settings =====================

void loadSettings() {
  prefs.begin("waterlevel", true);
  settings.tankHeightCm   = prefs.getFloat("tankH", DEFAULT_TANK_HEIGHT_CM);
  settings.fullDistanceCm = prefs.getFloat("fullD", DEFAULT_FULL_DISTANCE_CM);
  settings.capacityLiters = prefs.getFloat("capL", DEFAULT_CAPACITY_LITERS);
  settings.lowPercent     = prefs.getInt("low", DEFAULT_LOW_PERCENT);
  settings.highPercent    = prefs.getInt("high", DEFAULT_HIGH_PERCENT);
  settings.buzzerEnabled  = prefs.getBool("buzzer", true);
  settings.pumpMode       = (PumpMode)prefs.getUChar("pump", PUMP_AUTO);
  prefs.end();
}

void saveSettings() {
  prefs.begin("waterlevel", false);
  prefs.putFloat("tankH", settings.tankHeightCm);
  prefs.putFloat("fullD", settings.fullDistanceCm);
  prefs.putFloat("capL", settings.capacityLiters);
  prefs.putInt("low", settings.lowPercent);
  prefs.putInt("high", settings.highPercent);
  prefs.putBool("buzzer", settings.buzzerEnabled);
  prefs.putUChar("pump", settings.pumpMode);
  prefs.end();
}

// ===================== Sensor =====================

float readDistanceOnce() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  unsigned long duration = pulseIn(PIN_ECHO, HIGH, ECHO_TIMEOUT_US);
  if (duration == 0) return -1;

  float d = duration * SOUND_SPEED_CM_PER_US / 2.0f;
  if (d < SENSOR_MIN_CM || d > SENSOR_MAX_CM) return -1;
  return d;
}

// Ambil beberapa sampel, buang yang gagal, kembalikan median.
float readDistanceMedian() {
  float samples[SAMPLES_PER_READING];
  int count = 0;

  for (int i = 0; i < SAMPLES_PER_READING; i++) {
    float d = readDistanceOnce();
    if (d > 0) samples[count++] = d;
    if (i < SAMPLES_PER_READING - 1) delay(SAMPLE_GAP_MS);
  }
  if (count < (SAMPLES_PER_READING + 1) / 2) return -1;

  for (int i = 1; i < count; i++) {
    float key = samples[i];
    int j = i - 1;
    while (j >= 0 && samples[j] > key) {
      samples[j + 1] = samples[j];
      j--;
    }
    samples[j + 1] = key;
  }
  return samples[count / 2];
}

void updateLevel() {
  float d = readDistanceMedian();
  sensorOk = d > 0;
  if (!sensorOk) return;

  distanceCm = d;
  float span = settings.tankHeightCm - settings.fullDistanceCm;
  float pct = (settings.tankHeightCm - d) / span * 100.0f;
  levelPercent = constrain(pct, 0.0f, 100.0f);
}

float levelCm() {
  return max(0.0f, settings.tankHeightCm - distanceCm);
}

float liters() {
  return settings.capacityLiters * levelPercent / 100.0f;
}

// ===================== Output =====================

void setRelay(bool on) {
  pumpOn = on;
  bool pinHigh = RELAY_ACTIVE_LOW ? !on : on;
  digitalWrite(PIN_RELAY, pinHigh ? HIGH : LOW);
}

void updatePump() {
  bool wanted = pumpOn;

  switch (settings.pumpMode) {
    case PUMP_MANUAL_ON:
      wanted = !(sensorOk && levelPercent >= OVERFLOW_PERCENT);
      break;
    case PUMP_MANUAL_OFF:
      wanted = false;
      break;
    case PUMP_AUTO:
      // Histeresis: ON saat <= low, OFF saat >= high, di antaranya pertahankan kondisi.
      if (!sensorOk) wanted = false;
      else if (levelPercent <= settings.lowPercent) wanted = true;
      else if (levelPercent >= settings.highPercent) wanted = false;
      break;
  }

  if (wanted != pumpOn) {
    setRelay(wanted);
    Serial.printf("[POMPA] %s\n", wanted ? "ON" : "OFF");
  }
}

const char *statusCode() {
  if (!sensorOk) return "SENSOR_ERROR";
  if (levelPercent <= settings.lowPercent) return "LOW";
  if (levelPercent >= settings.highPercent) return "FULL";
  return "MEDIUM";
}

void updateIndicators(unsigned long now) {
  bool blink = (now / 500) % 2;
  bool low = sensorOk && levelPercent <= settings.lowPercent;
  bool full = sensorOk && levelPercent >= settings.highPercent;

  digitalWrite(PIN_LED_RED, !sensorOk ? blink : low);
  digitalWrite(PIN_LED_YELLOW, sensorOk && !low && !full);
  digitalWrite(PIN_LED_GREEN, full);

  // Bunyi pendek tiap 2 detik saat air rendah atau sensor error.
  bool alarm = settings.buzzerEnabled && (!sensorOk || low);
  digitalWrite(PIN_BUZZER, alarm && (now % 2000) < 150);
}

const char *pumpModeName(PumpMode m) {
  switch (m) {
    case PUMP_MANUAL_ON:  return "on";
    case PUMP_MANUAL_OFF: return "off";
    default:              return "auto";
  }
}

// ===================== Web =====================

void sendJsonError(int code, const char *msg) {
  char buf[128];
  snprintf(buf, sizeof(buf), "{\"error\":\"%s\"}", msg);
  server.send(code, "application/json", buf);
}

void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleStatus() {
  char buf[400];
  snprintf(buf, sizeof(buf),
           "{\"distance_cm\":%.1f,\"level_cm\":%.1f,\"percent\":%.1f,\"liters\":%.0f,"
           "\"status\":\"%s\",\"sensor_ok\":%s,\"pump_on\":%s,\"pump_mode\":\"%s\","
           "\"rssi\":%d,\"ip\":\"%s\",\"uptime_s\":%lu}",
           sensorOk ? distanceCm : -1.0f, sensorOk ? levelCm() : 0.0f, levelPercent, liters(),
           statusCode(), sensorOk ? "true" : "false", pumpOn ? "true" : "false",
           pumpModeName(settings.pumpMode),
           wifi.connected() ? WiFi.RSSI() : 0, wifi.ip().toString().c_str(), millis() / 1000);
  server.send(200, "application/json", buf);
}

void handleGetConfig() {
  char buf[256];
  snprintf(buf, sizeof(buf),
           "{\"tank_height_cm\":%.1f,\"full_distance_cm\":%.1f,\"capacity_liters\":%.0f,"
           "\"low_percent\":%d,\"high_percent\":%d,\"buzzer\":%s}",
           settings.tankHeightCm, settings.fullDistanceCm, settings.capacityLiters,
           settings.lowPercent, settings.highPercent, settings.buzzerEnabled ? "true" : "false");
  server.send(200, "application/json", buf);
}

void handlePostConfig() {
  Settings next = settings;
  if (server.hasArg("tank_height_cm"))   next.tankHeightCm   = server.arg("tank_height_cm").toFloat();
  if (server.hasArg("full_distance_cm")) next.fullDistanceCm = server.arg("full_distance_cm").toFloat();
  if (server.hasArg("capacity_liters"))  next.capacityLiters = server.arg("capacity_liters").toFloat();
  if (server.hasArg("low_percent"))      next.lowPercent     = server.arg("low_percent").toInt();
  if (server.hasArg("high_percent"))     next.highPercent    = server.arg("high_percent").toInt();
  if (server.hasArg("buzzer"))           next.buzzerEnabled  = server.arg("buzzer") == "true";

  if (next.tankHeightCm < 10 || next.tankHeightCm > SENSOR_MAX_CM)
    return sendJsonError(400, "Tinggi tangki harus 10-400 cm");
  if (next.fullDistanceCm < SENSOR_MIN_CM || next.fullDistanceCm > next.tankHeightCm - 5)
    return sendJsonError(400, "Jarak penuh harus >= 2 cm dan minimal 5 cm di bawah tinggi tangki");
  if (next.capacityLiters < 0)
    return sendJsonError(400, "Kapasitas tidak valid");
  if (next.lowPercent < 0 || next.highPercent > 100 || next.lowPercent >= next.highPercent)
    return sendJsonError(400, "Batas rendah harus lebih kecil dari batas penuh (0-100%)");

  settings = next;
  saveSettings();
  Serial.println("[CONFIG] Pengaturan disimpan");
  handleGetConfig();
}

void handlePump() {
  String mode = server.arg("mode");
  if (mode == "auto")     settings.pumpMode = PUMP_AUTO;
  else if (mode == "on")  settings.pumpMode = PUMP_MANUAL_ON;
  else if (mode == "off") settings.pumpMode = PUMP_MANUAL_OFF;
  else return sendJsonError(400, "mode harus auto, on, atau off");

  saveSettings();
  updatePump();
  handleStatus();
}

void handleWifiStatus() {
  server.send(200, "application/json", wifi.statusJson());
}

void handleWifiScan() {
  if (server.method() == HTTP_POST) wifi.requestScan();
  server.send(200, "application/json", wifi.scanJson());
}

void handleWifiAdd() {
  if (!wifi.addNetwork(server.arg("ssid"), server.arg("password")))
    return sendJsonError(400, "SSID wajib diisi, password kosong atau 8-63 karakter");
  handleWifiStatus();
}

void handleWifiRemove() {
  if (!wifi.removeNetwork(server.arg("ssid"))) return sendJsonError(404, "Jaringan tidak ditemukan");
  handleWifiStatus();
}

void handleNotFound() {
  // Captive portal: HP mengecek internet ke domain lain, arahkan ke halaman setup.
  if (wifi.portalActive() && !server.uri().startsWith("/api/") &&
      server.hostHeader() != WiFi.softAPIP().toString()) {
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
    server.send(302, "text/plain", "");
    return;
  }
  sendJsonError(404, "not found");
}

void setupServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handlePostConfig);
  server.on("/api/pump", HTTP_POST, handlePump);
  server.on("/api/wifi", HTTP_GET, handleWifiStatus);
  server.on("/api/wifi/scan", HTTP_ANY, handleWifiScan);
  server.on("/api/wifi/add", HTTP_POST, handleWifiAdd);
  server.on("/api/wifi/remove", HTTP_POST, handleWifiRemove);
  server.onNotFound(handleNotFound);
  server.begin();
}

// Tahan tombol BOOT 3 detik untuk membuka portal setup (misal saat pindah lokasi).
void checkBootButton(unsigned long now) {
  if (digitalRead(PIN_BOOT_BUTTON) == HIGH) {
    bootPressedSinceMs = 0;
    return;
  }
  if (bootPressedSinceMs == 0) {
    bootPressedSinceMs = now;
  } else if (now - bootPressedSinceMs >= PORTAL_BUTTON_HOLD_MS) {
    wifi.openPortal(PORTAL_MANUAL_MS);
    bootPressedSinceMs = now;  // tidak terpicu berulang selama tombol masih ditahan
  }
}

// ===================== Main =====================

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Smart Water Level Monitor ===");

  // Set relay OFF sebelum pin jadi output, supaya pompa tidak sempat menyala saat boot.
  digitalWrite(PIN_RELAY, RELAY_ACTIVE_LOW ? HIGH : LOW);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);

  loadSettings();
  wifi.begin();
  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[WIFI] Dashboard: http://%s.local\n", MDNS_HOSTNAME);
  }
  setupServer();
}

void loop() {
  unsigned long now = millis();
  wifi.loop();
  server.handleClient();
  checkBootButton(now);

  if (now - lastMeasureMs >= MEASURE_INTERVAL_MS) {
    lastMeasureMs = now;
    updateLevel();
    updatePump();

    if (sensorOk) {
      Serial.printf("Jarak: %.1f cm | Level: %.1f cm (%.1f%%) | %.0f L | %s | Pompa: %s (%s)\n",
                    distanceCm, levelCm(), levelPercent, liters(), statusCode(),
                    pumpOn ? "ON" : "OFF", pumpModeName(settings.pumpMode));
    } else {
      Serial.println("[SENSOR] Tidak ada pantulan, cek kabel/posisi sensor");
    }
  }

  updateIndicators(now);
}
