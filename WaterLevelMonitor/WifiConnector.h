#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>

// Menyimpan beberapa jaringan WiFi di flash, otomatis konek ke yang sinyalnya
// paling kuat, dan membuka portal setup (Access Point + captive portal) saat
// tidak ada jaringan tersimpan yang bisa dipakai.
class WifiConnector {
 public:
  static const int MAX_NETWORKS = 5;

  void begin();
  void loop();

  bool addNetwork(const String &ssid, const String &password);
  bool removeNetwork(const String &ssid);
  void requestScan() { scanRequested = true; }
  void openPortal(unsigned long durationMs);

  bool connected() const { return WiFi.status() == WL_CONNECTED; }
  bool portalActive() const { return apActive; }
  IPAddress ip() const;
  String statusJson() const;
  String scanJson() const;

 private:
  enum State { IDLE, SCANNING, CONNECTING, ONLINE };

  struct Network {
    String ssid;
    String password;
  };
  struct ScanResult {
    String ssid;
    int rssi;
    bool secure;
  };

  Network saved[MAX_NETWORKS];
  int savedCount = 0;
  ScanResult scanResults[20];
  int scanCount = 0;
  bool scanRequested = false;

  State state = IDLE;
  bool apActive = false;
  DNSServer dns;
  String connectingSsid;
  unsigned long stateSinceMs = 0;
  unsigned long lastAttemptMs = 0;
  unsigned long offlineSinceMs = 0;
  unsigned long portalUntilMs = 0;

  void load();
  void save();
  int find(const String &ssid) const;
  void startAP();
  void stopAP();
  void startScan();
  void handleScanDone(int n);
  void connectTo(const Network &net);
  void setState(State s);
};
