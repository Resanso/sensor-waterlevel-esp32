#include "WifiConnector.h"

#include <Preferences.h>

#include "config.h"

static String jsonEscape(const String &s) {
  String out;
  out.reserve(s.length() + 2);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if ((uint8_t)c < 0x20) {
      char buf[7];
      snprintf(buf, sizeof(buf), "\\u%04x", c);
      out += buf;
    } else {
      out += c;
    }
  }
  return out;
}

void WifiConnector::begin() {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);  // reconnect diatur sendiri supaya bisa pindah ke jaringan lain
  WiFi.mode(WIFI_STA);

  load();
  // Jaringan dari config.h dipakai sebagai bawaan saat flash masih kosong.
  if (savedCount == 0 && strlen(WIFI_SSID) > 0) {
    saved[0] = {WIFI_SSID, WIFI_PASSWORD};
    savedCount = 1;
    save();
  }

  Serial.printf("[WIFI] %d jaringan tersimpan\n", savedCount);
  offlineSinceMs = millis();
  lastAttemptMs = millis() - WIFI_RETRY_INTERVAL_MS;  // langsung coba saat boot
  if (savedCount == 0) startAP();
}

void WifiConnector::loop() {
  unsigned long now = millis();
  if (apActive) dns.processNextRequest();

  if (connected()) {
    if (state == IDLE || state == CONNECTING) {
      setState(ONLINE);
      Serial.printf("[WIFI] Terhubung ke %s, IP: %s\n", WiFi.SSID().c_str(),
                    WiFi.localIP().toString().c_str());
    }
    offlineSinceMs = now;
    // AP dibiarkan hidup sebentar supaya HP yang sedang di portal sempat melihat IP baru.
    if (apActive && state == ONLINE && now - stateSinceMs > WIFI_PORTAL_LINGER_MS &&
        (long)(now - portalUntilMs) > 0) {
      stopAP();
    }
  } else {
    if (state == ONLINE) {
      Serial.println("[WIFI] Koneksi terputus, mencari ulang...");
      setState(IDLE);
      lastAttemptMs = now - WIFI_RETRY_INTERVAL_MS;
    }
    if (!apActive && now - offlineSinceMs > WIFI_PORTAL_AFTER_MS) startAP();
  }

  switch (state) {
    case IDLE:
    case ONLINE:
      if (scanRequested ||
          (state == IDLE && savedCount > 0 && now - lastAttemptMs >= WIFI_RETRY_INTERVAL_MS)) {
        startScan();
      }
      break;

    case SCANNING: {
      int n = WiFi.scanComplete();
      if (n >= 0) {
        handleScanDone(n);
      } else if (n == WIFI_SCAN_FAILED || now - stateSinceMs > 15000) {
        WiFi.scanDelete();
        scanRequested = false;
        lastAttemptMs = now;
        setState(connected() ? ONLINE : IDLE);
      }
      break;
    }

    case CONNECTING:
      if (now - stateSinceMs > WIFI_CONNECT_TIMEOUT_MS) {
        Serial.printf("[WIFI] Gagal konek ke %s\n", connectingSsid.c_str());
        WiFi.disconnect();
        lastAttemptMs = now;
        setState(IDLE);
      }
      break;
  }
}

void WifiConnector::startScan() {
  scanRequested = false;
  if (WiFi.scanNetworks(true) == WIFI_SCAN_FAILED) {
    lastAttemptMs = millis();
    return;
  }
  setState(SCANNING);
}

void WifiConnector::handleScanDone(int n) {
  scanCount = 0;
  int best = -1;
  int bestRssi = -1000;

  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);
    if (ssid.isEmpty()) continue;

    int existing = -1;
    for (int j = 0; j < scanCount; j++) {
      if (scanResults[j].ssid == ssid) existing = j;
    }
    if (existing >= 0) {
      if (rssi > scanResults[existing].rssi) scanResults[existing].rssi = rssi;
    } else if (scanCount < 20) {
      scanResults[scanCount++] = {ssid, rssi, WiFi.encryptionType(i) != WIFI_AUTH_OPEN};
    }

    int k = find(ssid);
    if (k >= 0 && rssi > bestRssi) {
      best = k;
      bestRssi = rssi;
    }
  }
  WiFi.scanDelete();

  for (int i = 1; i < scanCount; i++) {
    ScanResult key = scanResults[i];
    int j = i - 1;
    while (j >= 0 && scanResults[j].rssi < key.rssi) {
      scanResults[j + 1] = scanResults[j];
      j--;
    }
    scanResults[j + 1] = key;
  }

  lastAttemptMs = millis();
  if (connected()) {
    setState(ONLINE);
  } else if (best >= 0) {
    connectTo(saved[best]);
  } else {
    if (savedCount > 0) Serial.println("[WIFI] Tidak ada jaringan tersimpan di sekitar");
    setState(IDLE);
  }
}

void WifiConnector::connectTo(const Network &net) {
  Serial.printf("[WIFI] Menghubungkan ke %s...\n", net.ssid.c_str());
  connectingSsid = net.ssid;
  WiFi.disconnect();
  WiFi.begin(net.ssid.c_str(), net.password.c_str());
  setState(CONNECTING);
}

bool WifiConnector::addNetwork(const String &ssid, const String &password) {
  if (ssid.isEmpty() || ssid.length() > 32) return false;
  if (password.length() > 0 && (password.length() < 8 || password.length() > 63)) return false;

  int k = find(ssid);
  if (k < 0) {
    if (savedCount == MAX_NETWORKS) {
      // Penuh: buang jaringan paling lama.
      for (int i = 1; i < savedCount; i++) saved[i - 1] = saved[i];
      savedCount--;
    }
    k = savedCount++;
  }
  saved[k] = {ssid, password};
  save();

  // Langsung coba jaringan yang baru disimpan.
  connectTo(saved[k]);
  return true;
}

bool WifiConnector::removeNetwork(const String &ssid) {
  int k = find(ssid);
  if (k < 0) return false;
  for (int i = k + 1; i < savedCount; i++) saved[i - 1] = saved[i];
  savedCount--;
  save();
  return true;
}

void WifiConnector::openPortal(unsigned long durationMs) {
  portalUntilMs = millis() + durationMs;
  startAP();
}

IPAddress WifiConnector::ip() const {
  if (connected()) return WiFi.localIP();
  if (apActive) return WiFi.softAPIP();
  return IPAddress();
}

String WifiConnector::statusJson() const {
  static const char *stateNames[] = {"idle", "scanning", "connecting", "online"};
  String json = "{\"connected\":";
  json += connected() ? "true" : "false";
  json += ",\"ssid\":\"" + jsonEscape(connected() ? WiFi.SSID() : connectingSsid) + "\"";
  json += ",\"ip\":\"" + (connected() ? WiFi.localIP().toString() : String("")) + "\"";
  json += ",\"rssi\":" + String(connected() ? WiFi.RSSI() : 0);
  json += ",\"state\":\"" + String(stateNames[state]) + "\"";
  json += ",\"portal\":";
  json += apActive ? "true" : "false";
  json += ",\"ap_ssid\":\"" + jsonEscape(AP_SSID) + "\"";
  json += ",\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\"";
  json += ",\"saved\":[";
  for (int i = 0; i < savedCount; i++) {
    if (i) json += ",";
    json += "\"" + jsonEscape(saved[i].ssid) + "\"";
  }
  json += "]}";
  return json;
}

String WifiConnector::scanJson() const {
  String json = "{\"scanning\":";
  json += (scanRequested || state == SCANNING) ? "true" : "false";
  json += ",\"networks\":[";
  for (int i = 0; i < scanCount; i++) {
    if (i) json += ",";
    json += "{\"ssid\":\"" + jsonEscape(scanResults[i].ssid) + "\",\"rssi\":" +
            String(scanResults[i].rssi) + ",\"secure\":" +
            (scanResults[i].secure ? "true" : "false") + ",\"saved\":" +
            (find(scanResults[i].ssid) >= 0 ? "true" : "false") + "}";
  }
  json += "]}";
  return json;
}

void WifiConnector::startAP() {
  if (apActive) return;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  dns.start(53, "*", WiFi.softAPIP());  // semua domain diarahkan ke ESP32 (captive portal)
  apActive = true;
  Serial.printf("[WIFI] Portal setup aktif: SSID=%s, password=%s, buka http://%s\n", AP_SSID,
                AP_PASSWORD, WiFi.softAPIP().toString().c_str());
}

void WifiConnector::stopAP() {
  if (!apActive) return;
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  apActive = false;
  Serial.println("[WIFI] Portal setup ditutup");
}

void WifiConnector::setState(State s) {
  state = s;
  stateSinceMs = millis();
}

int WifiConnector::find(const String &ssid) const {
  for (int i = 0; i < savedCount; i++) {
    if (saved[i].ssid == ssid) return i;
  }
  return -1;
}

void WifiConnector::load() {
  Preferences prefs;
  prefs.begin("wifi", true);
  savedCount = min((int)prefs.getInt("n", 0), MAX_NETWORKS);
  for (int i = 0; i < savedCount; i++) {
    saved[i].ssid = prefs.getString(("s" + String(i)).c_str(), "");
    saved[i].password = prefs.getString(("p" + String(i)).c_str(), "");
  }
  prefs.end();
}

void WifiConnector::save() {
  Preferences prefs;
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.putInt("n", savedCount);
  for (int i = 0; i < savedCount; i++) {
    prefs.putString(("s" + String(i)).c_str(), saved[i].ssid);
    prefs.putString(("p" + String(i)).c_str(), saved[i].password);
  }
  prefs.end();
}
