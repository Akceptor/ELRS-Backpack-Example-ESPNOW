
#if ESP32
#pragma message "ESP32 stuff happening!"
#else
#pragma message "ESP8266 stuff happening!"
#endif

#ifdef ESP32
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <WebServer.h>
#include <Preferences.h>
#include <MD5Builder.h>
#else
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Hash.h>
#endif

#include "crsf.h"  // Include crsf.h

uint8_t UID[6] = {51,18,164,89,216,116}; // this is my UID. You have to change it to your once (UID Input: 12345678)

const char* ssid = "Backpack_ELRS_Crsf";           // Access point SSID
const char* password = "12345678";                  // Access point password

static const uint32_t UID_MAGIC = 0x55494431; // "UID1"
static const size_t UID_LEN = 6;

#ifdef ESP32
WebServer server(80);
Preferences prefs;
#else
ESP8266WebServer server(80);
#endif

static String uidToString(const uint8_t *uid) {
  String out;
  for (size_t i = 0; i < UID_LEN; ++i) {
    if (i) {
      out += ",";
    }
    out += String(uid[i]);
  }
  return out;
}

static void computeUidFromPhrase(const String &phrase, uint8_t out[UID_LEN]) {
  String bindingPhraseFull = "-DMY_BINDING_PHRASE=\"" + phrase + "\"";
  MD5Builder md5;
  md5.begin();
  md5.add(bindingPhraseFull);
  md5.calculate();
  uint8_t digest[16];
  md5.getBytes(digest);
  for (size_t i = 0; i < UID_LEN; ++i) {
    out[i] = digest[i];
  }
}

#ifdef ESP32
static bool loadUidFromStorage() {
  prefs.begin("elrs", true);
  uint32_t magic = prefs.getUInt("magic", 0);
  if (magic != UID_MAGIC) {
    prefs.end();
    return false;
  }
  size_t got = prefs.getBytes("uid", UID, UID_LEN);
  prefs.end();
  return got == UID_LEN;
}

static void saveUidToStorage() {
  prefs.begin("elrs", false);
  prefs.putUInt("magic", UID_MAGIC);
  prefs.putBytes("uid", UID, UID_LEN);
  prefs.end();
}
#else
static void eepromWriteU32(int addr, uint32_t value) {
  EEPROM.write(addr + 0, (value >> 24) & 0xFF);
  EEPROM.write(addr + 1, (value >> 16) & 0xFF);
  EEPROM.write(addr + 2, (value >> 8) & 0xFF);
  EEPROM.write(addr + 3, value & 0xFF);
}

static uint32_t eepromReadU32(int addr) {
  uint32_t value = 0;
  value |= (uint32_t)EEPROM.read(addr + 0) << 24;
  value |= (uint32_t)EEPROM.read(addr + 1) << 16;
  value |= (uint32_t)EEPROM.read(addr + 2) << 8;
  value |= (uint32_t)EEPROM.read(addr + 3);
  return value;
}

static bool loadUidFromStorage() {
  EEPROM.begin(64);
  uint32_t magic = eepromReadU32(0);
  if (magic != UID_MAGIC) {
    EEPROM.end();
    return false;
  }
  for (size_t i = 0; i < UID_LEN; ++i) {
    UID[i] = EEPROM.read(4 + i);
  }
  EEPROM.end();
  return true;
}

static void saveUidToStorage() {
  EEPROM.begin(64);
  eepromWriteU32(0, UID_MAGIC);
  for (size_t i = 0; i < UID_LEN; ++i) {
    EEPROM.write(4 + i, UID[i]);
  }
  EEPROM.commit();
  EEPROM.end();
}
#endif

static void handleRoot() {
  String uidStr = uidToString(UID);
  static const char kIndexHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <title>ELRS Binding</title>
    <style>
      body { font-family: Arial, sans-serif; margin: 24px; color: #222; }
      .card { max-width: 460px; padding: 16px; border: 1px solid #ccc; border-radius: 8px; }
      label { display: block; margin-bottom: 8px; font-weight: 600; }
      input { width: 100%; padding: 8px; margin-bottom: 12px; }
      button { padding: 8px 12px; }
      .hint { font-size: 12px; color: #555; }
    </style>
  </head>
  <body>
    <div class="card">
      <h2>Binding Phrase</h2>
      <form action="/set" method="post">
        <label for="phrase">Phrase</label>
        <input id="phrase" name="phrase" type="text" maxlength="64" required>
        <button type="submit">Save UID</button>
      </form>
      <p class="hint">Current UID: %UID%</p>
      <div class="hint">UID will be derived like ExpressLRS: md5("-DMY_BINDING_PHRASE=\"phrase\""), first 6 bytes.</div>
    </div>
  </body>
</html>
)HTML";
  String page = String(kIndexHtml);
  page.replace("%UID%", uidStr);
  server.send(200, "text/html", page);
}

static void handleSet() {
  String phrase = server.arg("phrase");
  phrase.trim();
  if (phrase.length() == 0) {
    server.send(400, "text/plain", "Missing binding phrase.");
    return;
  }
  uint8_t newUid[UID_LEN];
  computeUidFromPhrase(phrase, newUid);
  memcpy(UID, newUid, UID_LEN);
  saveUidToStorage();
  String body = "<!doctype html><html><body><h3>Saved UID</h3><p>" +
                uidToString(UID) +
                "</p><p>Rebooting...</p></body></html>";
  server.send(200, "text/html", body);
  Serial.print("Saved UID from phrase: ");
  Serial.println(uidToString(UID));
  delay(200);
  ESP.restart();
}
    
// Callback when data is received
#ifdef ESP32
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
#else
void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
#endif
  // Keep ESP-NOW RX silent; only print specific CRSF frames.
  // For CRSF protocol from here
  espnow_len = len;
  crsf_len = len - 8;
  espnow_received = true;
  crsfStoreEspnowPacket(incomingData + 8, crsf_len);
}

void setup() {
  // Init Serial Monitor
  Serial.begin(115200);
  Serial.println("Start");
  crsfBegin();
  if (loadUidFromStorage()) {
    Serial.print("Loaded UID from storage: ");
    Serial.println(uidToString(UID));
  }
  uint8_t macUid[UID_LEN];
  memcpy(macUid, UID, UID_LEN);
  // MAC address can only be set with unicast, so first byte must be even, not odd --> important for BACKPACK
  macUid[0] &= ~0x01;
  WiFi.mode(WIFI_STA);
  // Set device as a Wi-Fi Station
  //WiFi.mode(WIFI_AP_STA); // Enables AP and station at the same time
  WiFi.disconnect();
  // Set a custom MAC address for the device in station mode (important for BACKPACK compatibility)
  #ifdef ESP32
  esp_wifi_set_mac(WIFI_IF_STA, macUid);
  #else
  wifi_set_macaddr(STATION_IF, macUid); //--> important for BACKPACK
  #endif
  IPAddress apIp(10, 0, 0, 1);
  IPAddress apGw(10, 0, 0, 1);
  IPAddress apMask(255, 255, 255, 0);
  WiFi.softAPConfig(apIp, apGw, apMask);
  // Start the device in Access Point mode with the specified SSID and password
  WiFi.softAP(ssid, password);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/set", HTTP_POST, handleSet);
  server.on("/set", HTTP_GET, handleSet);
  server.begin();
  // Init ESP-NOW
  #ifdef ESP32
  if (esp_now_init() != ESP_OK) {
  #else
  if (esp_now_init() != 0) {
  #endif
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  #ifdef ESP32
  esp_now_register_recv_cb(OnDataRecv);
  #else
  // Set ESP-NOW Role
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  // Register peer
  esp_now_add_peer(macUid, ESP_NOW_ROLE_COMBO, 0, NULL, 0);
  // Register a callback function to handle received ESP-NOW data
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  #endif
}

void loop() {
  server.handleClient();
  // Only for CRSF output
  crsfReceive();
}
