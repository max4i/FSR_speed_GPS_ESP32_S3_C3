/*
 * ============================================================================
 * FSR SPEED v9.4 - ESP32-C3 (wersja lekka)
 * ============================================================================
 * 
 * OPIS SYSTEMU:
 * ============================================================================
 * System telemetrii dedykowany dla łodzi wyścigowych FSR.
 * Wersja dla ESP32-C3 - zoptymalizowana pod kątem ograniczonej pamięci.
 * 
 * GŁÓWNE FUNKCJE:
 * ============================================================================
 * 1. Śledzenie trasy na mapie satelitarnej
 * 2. Wyświetlanie aktualnej prędkości i VMAX
 * 3. Automatyczne wykrywanie okrążeń z pomiarem czasów
 * 4. Zapisywanie trasy do pliku GPX
 * 5. Konfiguracja przez przeglądarkę internetową
 * 
 * PODŁĄCZENIA (ESP32-C3):
 * ============================================================================
 * | GPIO | Funkcja     | Podłączenie                               |
 * |------|-------------|-------------------------------------------|
 * | 4    | GPS RX      | TX modułu GPS                             |
 * | 5    | GPS TX      | RX modułu GPS                             |
 * |      |             |                                           |
 * | Uwaga: Unikamy pinów 18,19 (zajęte przez USB)                         |
 * 
 * ZASILANIE:
 * ============================================================================
 * - Przez USB-C (5V) lub pin 5V
 * - Pobór prądu: ~100mA (bez czujników)
 * ============================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <HardwareSerial.h>
#include <TinyGPSPlus.h>
#include <ArduinoJson.h>
#include <esp_wifi.h>

// ==================== PINY DLA ESP32-C3 ====================
#define GPS_RX_PIN 4
#define GPS_TX_PIN 5
#define GPS_BAUD 230400

// ==================== DOMYŚLNA KONFIGURACJA ====================
struct Config {
  char wifi_ssid[32] = "FSR_speed";
  char wifi_password[32] = "1234567890";
  uint32_t gps_baud = GPS_BAUD;
  int gps_rx_pin = GPS_RX_PIN;
  int gps_tx_pin = GPS_TX_PIN;
  float start_lat = 0.0;
  float start_lon = 0.0;
  bool start_set = false;
  float start_line_lat = 0.0;
  float start_line_lon = 0.0;
  float start_line_radius = 15.0;
  int laps = 0;
  float best_lap_time = 0;
  unsigned long lap_start_time = 0;
  int wifi_power = 0;
  int wifi_mode = 2;
};

Config config;

// ==================== WI-FI ====================
IPAddress local_ip(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);
TinyGPSPlus gps;
HardwareSerial GPS_Serial(0);  // UART0 dla ESP32-C3

// ==================== FILTRY ====================
#define SPEED_THRESHOLD 3.0
#define POSITION_BUFFER 5
#define DISTANCE_MIN 0.5
#define MAX_TRACK_POINTS 2000  // Zmniejszone dla ESP32-C3
#define HDOP_GOOD_THRESHOLD 1.1
#define MIN_SATELLITES 6
#define VMAX_MIN_SPEED 5.0
#define MS2_TO_G 0.101971621

// ==================== ZMIENNE ====================
float currentSpeed = 0;
float maxSpeed = 0;
float maxSpeedLat = 0;
float maxSpeedLon = 0;
float totalDistance = 0;
float lastLat = 0;
float lastLon = 0;
float lastSpeed = 0;
unsigned long lastSpeedTime = 0;
bool firstFix = true;
bool startPositionRecorded = false;
bool wifiStarted = false;
bool lastInsideStartLine = false;

float latBuffer[POSITION_BUFFER];
float lonBuffer[POSITION_BUFFER];
int bufferIndex = 0;
bool bufferFull = false;

struct TrackPoint {
    float lat;
    float lon;
    float speed;
    unsigned long timestamp;
};
TrackPoint* trackBuffer = nullptr;
int trackCount = 0;

unsigned long lastDataTime = 0;

// ==================== FUNKCJE KONFIGURACJI ====================
void loadConfig() {
  File file = SPIFFS.open("/config.json", "r");
  if (!file) {
    Serial.println("[CONFIG] Brak pliku, tworzę domyślny");
    saveConfig();
    return;
  }
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) {
    Serial.println("[CONFIG] Błąd parsowania, używam domyślnych");
    return;
  }
  strlcpy(config.wifi_ssid, doc["wifi_ssid"] | "FSR_speed", sizeof(config.wifi_ssid));
  strlcpy(config.wifi_password, doc["wifi_password"] | "1234567890", sizeof(config.wifi_password));
  config.gps_baud = doc["gps_baud"] | GPS_BAUD;
  config.gps_rx_pin = doc["gps_rx_pin"] | GPS_RX_PIN;
  config.gps_tx_pin = doc["gps_tx_pin"] | GPS_TX_PIN;
  config.start_lat = doc["start_lat"] | 0.0;
  config.start_lon = doc["start_lon"] | 0.0;
  config.start_set = doc["start_set"] | false;
  config.start_line_lat = doc["start_line_lat"] | 0.0;
  config.start_line_lon = doc["start_line_lon"] | 0.0;
  config.start_line_radius = doc["start_line_radius"] | 15.0;
  config.laps = doc["laps"] | 0;
  config.best_lap_time = doc["best_lap_time"] | 0;
  config.lap_start_time = doc["lap_start_time"] | 0;
  config.wifi_power = doc["wifi_power"] | 0;
  config.wifi_mode = doc["wifi_mode"] | 2;
  Serial.println("[CONFIG] Wczytano ustawienia");
}

void saveConfig() {
  StaticJsonDocument<512> doc;
  doc["wifi_ssid"] = config.wifi_ssid;
  doc["wifi_password"] = config.wifi_password;
  doc["gps_baud"] = config.gps_baud;
  doc["gps_rx_pin"] = config.gps_rx_pin;
  doc["gps_tx_pin"] = config.gps_tx_pin;
  doc["start_lat"] = config.start_lat;
  doc["start_lon"] = config.start_lon;
  doc["start_set"] = config.start_set;
  doc["start_line_lat"] = config.start_line_lat;
  doc["start_line_lon"] = config.start_line_lon;
  doc["start_line_radius"] = config.start_line_radius;
  doc["laps"] = config.laps;
  doc["best_lap_time"] = config.best_lap_time;
  doc["lap_start_time"] = config.lap_start_time;
  doc["wifi_power"] = config.wifi_power;
  doc["wifi_mode"] = config.wifi_mode;
  
  File file = SPIFFS.open("/config.json", "w");
  if (!file) {
    Serial.println("[CONFIG] Błąd zapisu");
    return;
  }
  serializeJson(doc, file);
  file.close();
  Serial.println("[CONFIG] Zapisano ustawienia");
}

void startWiFi() {
  Serial.println("[WiFi] Uruchamianie Access Point...");
  
  delay(200);
  WiFi.disconnect(true, true);
  delay(100);
  WiFi.mode(WIFI_AP);
  delay(100);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  delay(100);
  
  wifi_power_t powerLevel;
  switch(config.wifi_power) {
    case 0: powerLevel = WIFI_POWER_8_5dBm; break;
    case 1: powerLevel = WIFI_POWER_11dBm; break;
    case 2: powerLevel = WIFI_POWER_13dBm; break;
    case 3: powerLevel = WIFI_POWER_17dBm; break;
    case 4: powerLevel = WIFI_POWER_19dBm; break;
    default: powerLevel = WIFI_POWER_19_5dBm; break;
  }
  WiFi.setTxPower(powerLevel);
  Serial.printf("[WiFi] Moc nadajnika: ");
  switch(config.wifi_power) {
    case 0: Serial.println("8.5 dBm (niska)"); break;
    case 1: Serial.println("11 dBm (średnia)"); break;
    case 2: Serial.println("13 dBm (wysoka)"); break;
    case 3: Serial.println("17 dBm (bardzo wysoka)"); break;
    case 4: Serial.println("19 dBm (maksymalna)"); break;
    default: Serial.println("19.5 dBm (pełna moc)"); break;
  }
  
  if (config.wifi_mode == 0) {
    esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B);
    Serial.println("[WiFi] Tryb: 802.11b");
  } else if (config.wifi_mode == 1) {
    esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11G);
    Serial.println("[WiFi] Tryb: 802.11g");
  } else {
    esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11N);
    Serial.println("[WiFi] Tryb: 802.11n");
  }
  
  delay(50);
  
  if (WiFi.softAP(config.wifi_ssid, config.wifi_password, 1, 0, 4)) {
    Serial.printf("[WiFi] AP '%s' uruchomiony\n", config.wifi_ssid);
    Serial.printf("[WiFi] IP: %s\n", WiFi.softAPIP().toString().c_str());
    wifiStarted = true;
  } else {
    Serial.println("[WiFi] BŁĄD: Nie można uruchomić AP!");
    wifiStarted = false;
  }
}

void startServer() {
  if (wifiStarted) {
    server.on("/", handleRoot);
    server.on("/api/data", handleData);
    server.on("/api/resetmax", HTTP_POST, handleResetMax);
    server.on("/api/resettrack", HTTP_POST, handleResetTrack);
    server.on("/api/resetstart", HTTP_POST, handleResetStart);
    server.on("/api/resetlaps", HTTP_POST, handleResetLaps);
    server.on("/api/setstartline", HTTP_POST, handleSetStartLine);
    server.on("/api/save", HTTP_GET, handleSaveTrack);
    server.on("/api/config", HTTP_GET, handleConfigGet);
    server.on("/api/config", HTTP_POST, handleConfigPost);
    server.begin();
    Serial.println("[HTTP] Serwer WWW uruchomiony");
    Serial.println("\n→ http://192.168.4.1\n");
  }
}

// ==================== FUNKCJE OKRĄŻEŃ ====================
float calculateDistanceToPoint(float lat1, float lon1, float lat2, float lon2) {
  float R = 6371000;
  float dlat = (lat2 - lat1) * PI / 180;
  float dlon = (lon2 - lon1) * PI / 180;
  float a = sin(dlat/2) * sin(dlat/2) + 
            cos(lat1 * PI / 180) * cos(lat2 * PI / 180) * 
            sin(dlon/2) * sin(dlon/2);
  float c = 2 * atan2(sqrt(a), sqrt(1-a));
  return R * c;
}

void checkLap(float currentLat, float currentLon, unsigned long currentTime) {
  if (config.start_line_lat == 0 && config.start_line_lon == 0) return;
  
  float distance = calculateDistanceToPoint(currentLat, currentLon, 
                                             config.start_line_lat, 
                                             config.start_line_lon);
  
  bool inside = (distance < config.start_line_radius);
  
  if (inside && !lastInsideStartLine) {
    if (config.laps == 0) {
      config.lap_start_time = currentTime;
      Serial.println("[LAP] START!");
    } else {
      float lapTime = (currentTime - config.lap_start_time) / 1000.0;
      
      if (config.best_lap_time == 0 || lapTime < config.best_lap_time) {
        config.best_lap_time = lapTime;
        Serial.printf("[LAP] NOWY REKORD! %.2f s\n", lapTime);
      }
      
      Serial.printf("[LAP] Okrążenie #%d: %.2f s\n", config.laps + 1, lapTime);
      
      config.lap_start_time = currentTime;
      config.laps++;
      saveConfig();
    }
  }
  
  lastInsideStartLine = inside;
}

// ==================== FUNKCJE POMOCNICZE ====================
void initBuffer() {
  trackBuffer = (TrackPoint*)malloc(sizeof(TrackPoint) * MAX_TRACK_POINTS);
  if (trackBuffer) {
    Serial.printf("[BUFFER] Bufor trasy zaalokowany (%d punktow)\n", MAX_TRACK_POINTS);
  } else {
    Serial.println("[BUFFER] Błąd alokacji pamięci!");
  }
}

void addPositionToBuffer(float lat, float lon) {
  latBuffer[bufferIndex] = lat;
  lonBuffer[bufferIndex] = lon;
  bufferIndex++;
  if (bufferIndex >= POSITION_BUFFER) { bufferIndex=0; bufferFull=true; }
}

float getMedianLat() {
  if (!bufferFull && bufferIndex<2) return gps.location.lat();
  float temp[POSITION_BUFFER];
  int count = bufferFull ? POSITION_BUFFER : bufferIndex;
  for (int i=0; i<count; i++) temp[i] = latBuffer[i];
  for (int i=0; i<count-1; i++)
    for (int j=0; j<count-i-1; j++)
      if (temp[j] > temp[j+1]) { float t=temp[j]; temp[j]=temp[j+1]; temp[j+1]=t; }
  return temp[count/2];
}

float getMedianLon() {
  if (!bufferFull && bufferIndex<2) return gps.location.lng();
  float temp[POSITION_BUFFER];
  int count = bufferFull ? POSITION_BUFFER : bufferIndex;
  for (int i=0; i<count; i++) temp[i] = lonBuffer[i];
  for (int i=0; i<count-1; i++)
    for (int j=0; j<count-i-1; j++)
      if (temp[j] > temp[j+1]) { float t=temp[j]; temp[j]=temp[j+1]; temp[j+1]=t; }
  return temp[count/2];
}

float calculateDistance(float lat1, float lon1, float lat2, float lon2) {
  float R=6371;
  float dlat = (lat2-lat1)*PI/180;
  float dlon = (lon2-lon1)*PI/180;
  float a = sin(dlat/2)*sin(dlat/2) + cos(lat1*PI/180)*cos(lat2*PI/180)*sin(dlon/2)*sin(dlon/2);
  float c = 2*atan2(sqrt(a), sqrt(1-a));
  return R*c;
}

void addTrackPoint(float lat, float lon, float speed) {
  if (trackBuffer && trackCount < MAX_TRACK_POINTS) {
    trackBuffer[trackCount] = {lat, lon, speed, millis()};
    trackCount++;
  }
}

// ==================== KONFIGURACJA GPS ====================
bool autoDetectGPSBaud() {
  const unsigned long baudrates[] = {4800, 9600, 19200, 38400, 57600, 115200, 230400};
  const int numBauds = sizeof(baudrates) / sizeof(baudrates[0]);
  
  Serial.println("[GPS] Autodetekcja prędkości...");
  
  for (int i = 0; i < numBauds; i++) {
    unsigned long baud = baudrates[i];
    Serial.printf("[GPS] Próba %d baud\n", baud);
    GPS_Serial.begin(baud, SERIAL_8N1, config.gps_rx_pin, config.gps_tx_pin);
    GPS_Serial.setRxBufferSize(512);
    delay(500);
    
    unsigned long start = millis();
    bool sentenceFound = false;
    while (millis() - start < 2000) {
      while (GPS_Serial.available()) {
        char c = GPS_Serial.read();
        gps.encode(c);
        if (gps.location.isValid() || gps.speed.isValid()) {
          sentenceFound = true;
          break;
        }
      }
      if (sentenceFound) break;
    }
    
    if (sentenceFound) {
      Serial.printf("[GPS] Znaleziono ramkę przy %d baud\n", baud);
      return true;
    }
  }
  
  Serial.println("[GPS] Autodetekcja nie powiodła się");
  return false;
}

void configureGPS() {
  Serial.println("[GPS] Wysyłam komendy konfiguracyjne...");
  GPS_Serial.println("$PMTK220,100*2F");
  delay(200);
  GPS_Serial.println("$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28");
  delay(200);
  GPS_Serial.println("$PMTK286,1*23");
  delay(500);
  Serial.println("[GPS] Konfiguracja zakończona");
}

// ==================== OBSŁUGA SERWERA ====================
#include "html.h"

void handleRoot() { server.send(200, "text/html", index_html); }

void handleData() {
  float displaySpeed = (currentSpeed < SPEED_THRESHOLD) ? 0 : currentSpeed;
  float lat = getMedianLat();
  float lon = getMedianLon();
  int fix = gps.location.isValid() ? 1 : 0;
  float hdop = gps.hdop.isValid() ? gps.hdop.hdop() : 99.9;
  int satellites = gps.satellites.value();
  int fixQuality = (fix && hdop < HDOP_GOOD_THRESHOLD && satellites >= MIN_SATELLITES) ? 2 : (fix ? 1 : 0);
  
  float currentLapTime = 0;
  if (config.laps > 0 && config.lap_start_time > 0) {
    currentLapTime = (millis() - config.lap_start_time) / 1000.0;
  }
  
  String json = "{";
  json += "\"speed\":" + String(displaySpeed,1) + ",";
  json += "\"max_speed\":" + String(maxSpeed,1) + ",";
  json += "\"max_lat\":" + String(maxSpeedLat,6) + ",";
  json += "\"max_lon\":" + String(maxSpeedLon,6) + ",";
  json += "\"distance\":" + String(totalDistance,3) + ",";
  json += "\"lat\":" + String(lat,6) + ",";
  json += "\"lon\":" + String(lon,6) + ",";
  json += "\"sat\":" + String(satellites) + ",";
  json += "\"hdop\":" + String(hdop,1) + ",";
  json += "\"fix\":" + String(fix) + ",";
  json += "\"fix_quality\":" + String(fixQuality) + ",";
  json += "\"start_lat\":" + String(config.start_lat,6) + ",";
  json += "\"start_lon\":" + String(config.start_lon,6) + ",";
  json += "\"start_set\":" + String(config.start_set ? "true" : "false") + ",";
  json += "\"laps\":" + String(config.laps) + ",";
  json += "\"best_lap_time\":" + String(config.best_lap_time,2) + ",";
  json += "\"current_lap_time\":" + String(currentLapTime,2) + ",";
  json += "\"start_line_lat\":" + String(config.start_line_lat,6) + ",";
  json += "\"start_line_lon\":" + String(config.start_line_lon,6);
  json += "}";
  server.send(200, "application/json", json);
}

void handleResetMax() { 
  maxSpeed=0; maxSpeedLat=0; maxSpeedLon=0; 
  server.send(200, "text/plain", "OK"); 
}

void handleResetTrack() { trackCount=0; server.send(200, "text/plain", "OK"); }

void handleResetStart() {
  config.start_lat = 0;
  config.start_lon = 0;
  config.start_set = false;
  saveConfig();
  server.send(200, "text/plain", "OK");
}

void handleResetLaps() {
  config.laps = 0;
  config.best_lap_time = 0;
  config.lap_start_time = 0;
  saveConfig();
  Serial.println("[LAP] Reset okrążeń");
  server.send(200, "text/plain", "OK");
}

void handleSetStartLine() {
  if (!server.hasArg("plain")) { server.send(400, "text/plain", "Brak danych"); return; }
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  if (error) { server.send(400, "text/plain", "Błędny JSON"); return; }
  
  if (doc.containsKey("lat") && doc.containsKey("lon")) {
    config.start_line_lat = doc["lat"];
    config.start_line_lon = doc["lon"];
    config.start_line_radius = doc["radius"] | 15.0;
    saveConfig();
    Serial.printf("[LAP] Ustawiono linię start/meta: %.6f, %.6f\n", 
                  config.start_line_lat, config.start_line_lon);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Brak współrzędnych");
  }
}

void handleSaveTrack() {
  if (!trackBuffer || trackCount==0) { server.send(404, "text/plain", "Brak danych"); return; }
  char filename[32];
  snprintf(filename, sizeof(filename), "track_%lu.gpx", millis());
  server.sendHeader("Content-Type", "application/gpx+xml");
  server.sendHeader("Content-Disposition", "attachment; filename=" + String(filename));
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/gpx+xml", "");
  server.sendContent("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<gpx version=\"1.1\" creator=\"FSR Speed Tracker\">\n");
  server.sendContent("<metadata><desc>FSR Race Track - " + String(trackCount) + " points, Max Speed: " + String(maxSpeed,1) + " km/h, Laps: " + String(config.laps) + "</desc></metadata>\n");
  server.sendContent("<trk><name>FSR Race Track</name><trkseg>\n");
  for (int i=0; i<trackCount; i++) {
    char point[256];
    snprintf(point, sizeof(point),
      "<trkpt lat=\"%.6f\" lon=\"%.6f\"><speed>%.1f</speed><time>%lu</time></trkpt>\n",
      trackBuffer[i].lat, trackBuffer[i].lon, trackBuffer[i].speed, trackBuffer[i].timestamp);
    server.sendContent(point);
    if (i%100==0) delay(1);
  }
  server.sendContent("</trkseg></trk></gpx>\n");
  Serial.printf("[DOWNLOAD] Wyslano %d punktow\n", trackCount);
}

void handleConfigGet() {
  StaticJsonDocument<512> doc;
  doc["wifi_ssid"] = config.wifi_ssid;
  doc["wifi_password"] = config.wifi_password;
  doc["gps_baud"] = config.gps_baud;
  doc["gps_rx_pin"] = config.gps_rx_pin;
  doc["gps_tx_pin"] = config.gps_tx_pin;
  doc["start_lat"] = config.start_lat;
  doc["start_lon"] = config.start_lon;
  doc["start_set"] = config.start_set;
  doc["start_line_lat"] = config.start_line_lat;
  doc["start_line_lon"] = config.start_line_lon;
  doc["start_line_radius"] = config.start_line_radius;
  doc["wifi_power"] = config.wifi_power;
  doc["wifi_mode"] = config.wifi_mode;
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleConfigPost() {
  if (!server.hasArg("plain")) { server.send(400, "text/plain", "Brak danych"); return; }
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  if (error) { server.send(400, "text/plain", "Błędny JSON"); return; }
  bool restart = false;
  if (doc.containsKey("wifi_ssid")) { strlcpy(config.wifi_ssid, doc["wifi_ssid"], sizeof(config.wifi_ssid)); restart = true; }
  if (doc.containsKey("wifi_password")) { strlcpy(config.wifi_password, doc["wifi_password"], sizeof(config.wifi_password)); restart = true; }
  if (doc.containsKey("gps_baud")) { config.gps_baud = doc["gps_baud"]; restart = true; }
  if (doc.containsKey("gps_rx_pin")) { config.gps_rx_pin = doc["gps_rx_pin"]; restart = true; }
  if (doc.containsKey("gps_tx_pin")) { config.gps_tx_pin = doc["gps_tx_pin"]; restart = true; }
  if (doc.containsKey("start_lat")) { config.start_lat = doc["start_lat"]; config.start_set = true; }
  if (doc.containsKey("start_lon")) { config.start_lon = doc["start_lon"]; config.start_set = true; }
  if (doc.containsKey("start_line_lat")) { config.start_line_lat = doc["start_line_lat"]; }
  if (doc.containsKey("start_line_lon")) { config.start_line_lon = doc["start_line_lon"]; }
  if (doc.containsKey("start_line_radius")) { config.start_line_radius = doc["start_line_radius"]; }
  if (doc.containsKey("wifi_power")) { config.wifi_power = doc["wifi_power"]; restart = true; }
  if (doc.containsKey("wifi_mode")) { config.wifi_mode = doc["wifi_mode"]; restart = true; }
  saveConfig();
  if (restart) {
    server.send(200, "text/plain", "OK restart");
    delay(500);
    ESP.restart();
  } else {
    server.send(200, "text/plain", "OK");
  }
}

// ==================== SETUP ====================
void setup() {
  delay(500);
  
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔════════════════════════════════════════════════════════════╗");
  Serial.println("║      FSR SPEED v9.4 - ESP32-C3 (wersja lekka)             ║");
  Serial.println("║      Tylko GPS + okrążenia                                ║");
  Serial.println("╚════════════════════════════════════════════════════════════╝\n");
  
  // SPIFFS
  if (!SPIFFS.begin(false)) {
    Serial.println("[SPIFFS] Montowanie nieudane, formatowanie...");
    if (SPIFFS.format()) {
      Serial.println("[SPIFFS] Formatowanie udane");
      SPIFFS.begin(true);
    }
  } else {
    Serial.println("[SPIFFS] Gotowe");
  }
  
  loadConfig();
  initBuffer();
  
  // Wi-Fi
  startWiFi();
  startServer();
  
  // Bufor pozycji
  for (int i=0; i<POSITION_BUFFER; i++) { latBuffer[i]=0; lonBuffer[i]=0; }
  
  // GPS
  if (!autoDetectGPSBaud()) {
    Serial.println("[GPS] Autodetekcja nie zadziałała");
    GPS_Serial.begin(config.gps_baud, SERIAL_8N1, config.gps_rx_pin, config.gps_tx_pin);
    GPS_Serial.setRxBufferSize(512);
  }
  configureGPS();
  
  Serial.println("\n[SYSTEM] Gotowy!\n");
  Serial.printf("[INFO] Szukaj sieci: %s | Hasło: %s\n", config.wifi_ssid, config.wifi_password);
  Serial.println("[INFO] http://192.168.4.1\n");
}

// ==================== LOOP ====================
void loop() {
  if (wifiStarted) {
    server.handleClient();
  }
  
  while (GPS_Serial.available()) gps.encode(GPS_Serial.read());
  
  if (gps.speed.isValid()) {
    float newSpeed = gps.speed.kmph();
    unsigned long now = millis();
    unsigned long dt = now - lastSpeedTime;
    if (lastSpeedTime > 0 && dt > 0 && dt < 1000) {
      // tylko prędkość, bez przyspieszenia
    }
    lastSpeed = newSpeed;
    lastSpeedTime = now;
    currentSpeed = newSpeed;
    if (currentSpeed > maxSpeed && currentSpeed > VMAX_MIN_SPEED) {
      maxSpeed = currentSpeed;
      maxSpeedLat = getMedianLat();
      maxSpeedLon = getMedianLon();
    }
  }
  
  if (gps.location.isValid()) {
    float currentLat = gps.location.lat();
    float currentLon = gps.location.lng();
    
    addPositionToBuffer(currentLat, currentLon);
    float hdop = gps.hdop.isValid() ? gps.hdop.hdop() : 99.9;
    int sat = gps.satellites.value();
    
    checkLap(currentLat, currentLon, millis());
    
    if (!startPositionRecorded && !config.start_set && hdop < HDOP_GOOD_THRESHOLD && sat >= MIN_SATELLITES) {
      config.start_lat = currentLat;
      config.start_lon = currentLon;
      config.start_set = true;
      startPositionRecorded = true;
      saveConfig();
      Serial.printf("[START] Zapisano start: %.6f, %.6f\n", config.start_lat, config.start_lon);
    }
    
    if (hdop < 2.0 && sat >= 4) {
      if (!firstFix) {
        float dist = calculateDistance(lastLat, lastLon, currentLat, currentLon);
        if (dist*1000 > DISTANCE_MIN) totalDistance += dist;
      } else firstFix = false;
      lastLat = currentLat;
      lastLon = currentLon;
    }
  }
  
  if (millis() - lastDataTime >= 100) {
    lastDataTime = millis();
    if (gps.location.isValid()) {
      float hdop = gps.hdop.isValid() ? gps.hdop.hdop() : 99.9;
      int sat = gps.satellites.value();
      if (hdop < 2.0 && sat >= 4) {
        float lat = getMedianLat();
        float lon = getMedianLon();
        float dispSpeed = (currentSpeed < SPEED_THRESHOLD) ? 0 : currentSpeed;
        addTrackPoint(lat, lon, dispSpeed);
      }
    }
  }
  
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 5000) {
    lastDebug = millis();
    if (gps.location.isValid()) {
      Serial.printf("📍 %.6f,%.6f | ⚡ %.1f km/h | SAT:%d | VMAX:%.1f\n", 
        getMedianLat(), getMedianLon(), currentSpeed, gps.satellites.value(), maxSpeed);
      Serial.printf("🏁 Okrążenia: %d | Najlepszy: %.2f s | 📦 %d pkt\n", 
        config.laps, config.best_lap_time, trackCount);
    } else {
      Serial.println("⏳ Czekam na GPS...");
    }
  }
  delay(10);
}