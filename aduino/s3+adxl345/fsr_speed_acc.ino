/*
 * ============================================================================
 * FSR SPEED ACC v0.9 - GPS + ADXL345
 * ============================================================================
 * 
 * DATA:        2026-04-23
 * AUTORZY:     Maxiii & Deepseek
 * 
 * NOWOŚCI W v0.9:
 * - Asynchroniczny serwer (AsyncWebServer) – stabilność Wi-Fi
 * - Kalibracja ADXL345 (endpoint /api/calibrate)
 * - Prędkość i VMAX z dwoma miejscami po przecinku
 * - ODTWARZANIE TRASY PO ODŚWIEŻENIU STRONY (/api/track)
 * - Eksport VMAX do CSV (/api/vmaxcsv)
 * - Zapamiętywanie numeru i CZASU najlepszego okrążenia
 * - Nagłówki cache dla buforowania strony (1 rok)
 * - Google Satellite mapa z wysoką rozdzielczością
 * 
 * ============================================================================
 */

#include <WiFi.h>
#include <SPIFFS.h>
#include <HardwareSerial.h>
#include <TinyGPSPlus.h>
#include <ArduinoJson.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// ==================== PINY ====================
#define GPS_RX_PIN 12
#define GPS_TX_PIN 13
#define GPS_BAUD 230400
#define TEMP_SENSOR_PIN 11
#define BATT_SENSOR_PIN 10
#define I2C_SDA 9
#define I2C_SCL 8

// ==================== PARAMETRY ====================
#define HDOP_GOOD_THRESHOLD 1.4
#define MIN_SATELLITES 6
#define SPEED_THRESHOLD 5.0
#define DISTANCE_MIN 0.5
#define MAX_TRACK_POINTS 6000
#define TEMP_FILTER_WINDOW 10
#define SPEED_FILTER_WINDOW 3
#define ACCEL_FILTER_WINDOW 5
#define VMAX_MIN_SPEED 5.0
#define MS2_TO_G 0.101971621
#define WIFI_TIMEOUT 10000

// ==================== SERWER ASYNCHRONICZNY ====================
AsyncWebServer server(80);

// ==================== STRUKTURY ====================
struct Config {
  float temp_offset = 38.0;
  float temp_scale = 1.0;
  bool temp_enabled = true;
  char wifi_ssid[32] = "FSR_ACC";
  char wifi_password[32] = "12345678";
  char sta_ssid[32] = "";
  char sta_password[32] = "";
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
  int best_lap_number = 0;
  unsigned long lap_start_time = 0;
  int wifi_power = 0;
  int wifi_mode = 2;
  int vmax_count = 0;
  float vmax_history[10] = {0};
  float vmax_lat[10] = {0};
  float vmax_lon[10] = {0};
  float accel_offset_x = 0.0;
  float accel_offset_y = 0.0;
  float accel_offset_z = 0.0;
  bool accel_calibrated = false;
};

struct TrackPoint {
    float lat;
    float lon;
    float speed;
    float temp;
    unsigned long timestamp;
};

// ==================== GLOBALNE ====================
Config config;
IPAddress local_ip(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);
TinyGPSPlus gps;
HardwareSerial GPS_Serial(1);
Adafruit_ADXL345_Unified adxl = Adafruit_ADXL345_Unified(12345);

// Zmienne
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
bool wifiConnected = false;
bool lastInsideStartLine = false;

// Filtry
float speedHistory[SPEED_FILTER_WINDOW];
int speedIndex = 0;
bool speedReady = false;
float fusedSpeed = 0;

// Bufor pozycji
float latBuffer[5];
float lonBuffer[5];
int bufferIndex = 0;
bool bufferFull = false;

// Temperatura
float tempEngine = 0.0f;
float tempBuffer[TEMP_FILTER_WINDOW];
int tempIndex = 0;

// Bateria
float batteryVoltage = 0.0f;

// Trasa
TrackPoint* trackBuffer = nullptr;
int trackCount = 0;
unsigned long lastDataTime = 0;

#include "html_acc.h"
#include "info.h"

// ==================== FUNKCJE POMOCNICZE ====================
float medianFilter(float *data, int size) {
  float sorted[size];
  memcpy(sorted, data, sizeof(float) * size);
  for (int i = 0; i < size - 1; i++) {
    for (int j = 0; j < size - i - 1; j++) {
      if (sorted[j] > sorted[j + 1]) {
        float t = sorted[j];
        sorted[j] = sorted[j + 1];
        sorted[j + 1] = t;
      }
    }
  }
  return sorted[size / 2];
}

float getFilteredSpeed(float rawSpeed) {
  if (isnan(rawSpeed) || isinf(rawSpeed)) return 0;
  if (rawSpeed < SPEED_THRESHOLD) return 0;
  speedHistory[speedIndex % SPEED_FILTER_WINDOW] = rawSpeed;
  speedIndex++;
  if (speedIndex >= SPEED_FILTER_WINDOW) speedReady = true;
  if (speedReady) return medianFilter(speedHistory, SPEED_FILTER_WINDOW);
  return rawSpeed;
}

float fuseSpeed(float gpsSpeed, float accelX, float dt) {
  if (dt == 0 || dt > 500) return gpsSpeed;
  float accelMs2 = accelX * 9.80665;
  float deltaV = accelMs2 * (dt / 1000.0);
  float fused = 0.7 * gpsSpeed + 0.3 * (fusedSpeed + deltaV);
  if (fused < 0) fused = 0;
  if (fused > 200) fused = 200;
  return fused;
}

float calculateSpeedAccuracy(int satellites, float hdop) {
  float baseError = 0.36;
  float satFactor = 1.0 - ((satellites - 4) / 30.0);
  if (satFactor < 0.3) satFactor = 0.3;
  if (satFactor > 1.0) satFactor = 1.0;
  float hdopFactor = hdop / 1.0;
  if (hdopFactor < 0.5) hdopFactor = 0.5;
  float result = baseError * satFactor * hdopFactor;
  if (result < 0.05) result = 0.05;
  if (result > 1.0) result = 1.0;
  return result;
}

void addVMAXToHistory(float speed, float lat, float lon) {
  if (config.vmax_count < 10) {
    config.vmax_history[config.vmax_count] = speed;
    config.vmax_lat[config.vmax_count] = lat;
    config.vmax_lon[config.vmax_count] = lon;
    config.vmax_count++;
  } else {
    for (int i = 0; i < 9; i++) {
      config.vmax_history[i] = config.vmax_history[i + 1];
      config.vmax_lat[i] = config.vmax_lat[i + 1];
      config.vmax_lon[i] = config.vmax_lon[i + 1];
    }
    config.vmax_history[9] = speed;
    config.vmax_lat[9] = lat;
    config.vmax_lon[9] = lon;
  }
  saveConfig();
}

// ==================== KONFIGURACJA ====================
void loadConfig() {
  File file = SPIFFS.open("/config.json", "r");
  if (!file) {
    saveConfig();
    return;
  }
  StaticJsonDocument<2048> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) return;
  
  config.temp_offset = doc["temp_offset"] | 38.0;
  config.temp_scale = doc["temp_scale"] | 1.0;
  config.temp_enabled = doc["temp_enabled"] | true;
  strlcpy(config.wifi_ssid, doc["wifi_ssid"] | "FSR_ACC", sizeof(config.wifi_ssid));
  strlcpy(config.wifi_password, doc["wifi_password"] | "12345678", sizeof(config.wifi_password));
  strlcpy(config.sta_ssid, doc["sta_ssid"] | "", sizeof(config.sta_ssid));
  strlcpy(config.sta_password, doc["sta_password"] | "", sizeof(config.sta_password));
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
  config.best_lap_number = doc["best_lap_number"] | 0;
  config.lap_start_time = doc["lap_start_time"] | 0;
  config.wifi_power = doc["wifi_power"] | 0;
  config.wifi_mode = doc["wifi_mode"] | 2;
  config.vmax_count = doc["vmax_count"] | 0;
  for (int i = 0; i < 10; i++) {
    config.vmax_history[i] = doc["vmax_" + String(i)] | 0;
    config.vmax_lat[i] = doc["vmax_lat_" + String(i)] | 0;
    config.vmax_lon[i] = doc["vmax_lon_" + String(i)] | 0;
  }
  config.accel_offset_x = doc["accel_offset_x"] | 0.0;
  config.accel_offset_y = doc["accel_offset_y"] | 0.0;
  config.accel_offset_z = doc["accel_offset_z"] | 0.0;
  config.accel_calibrated = doc["accel_calibrated"] | false;
}

void saveConfig() {
  StaticJsonDocument<2048> doc;
  doc["temp_offset"] = config.temp_offset;
  doc["temp_scale"] = config.temp_scale;
  doc["temp_enabled"] = config.temp_enabled;
  doc["wifi_ssid"] = config.wifi_ssid;
  doc["wifi_password"] = config.wifi_password;
  doc["sta_ssid"] = config.sta_ssid;
  doc["sta_password"] = config.sta_password;
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
  doc["best_lap_number"] = config.best_lap_number;
  doc["lap_start_time"] = config.lap_start_time;
  doc["wifi_power"] = config.wifi_power;
  doc["wifi_mode"] = config.wifi_mode;
  doc["vmax_count"] = config.vmax_count;
  for (int i = 0; i < 10; i++) {
    doc["vmax_" + String(i)] = config.vmax_history[i];
    doc["vmax_lat_" + String(i)] = config.vmax_lat[i];
    doc["vmax_lon_" + String(i)] = config.vmax_lon[i];
  }
  doc["accel_offset_x"] = config.accel_offset_x;
  doc["accel_offset_y"] = config.accel_offset_y;
  doc["accel_offset_z"] = config.accel_offset_z;
  doc["accel_calibrated"] = config.accel_calibrated;
  
  File file = SPIFFS.open("/config.json", "w");
  if (file) {
    serializeJson(doc, file);
    file.close();
  }
}

String scanNetworks() {
  String json = "[";
  int n = WiFi.scanComplete();
  if (n == -2) {
    WiFi.scanNetworks(true);
    return "{\"status\":\"scanning\"}";
  } else if (n > 0) {
    for (int i = 0; i < n; ++i) {
      if (i > 0) json += ",";
      json += "\"" + WiFi.SSID(i) + "\"";
    }
    WiFi.scanDelete();
  }
  json += "]";
  return json;
}

void startWiFiHybrid() {
  Serial.println("[WiFi] Tryb hybrydowy: najpierw próba połączenia...");
  
  if (strlen(config.sta_ssid) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.sta_ssid, config.sta_password);
    
    unsigned long start = millis();
    while (millis() - start < WIFI_TIMEOUT) {
      if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.printf("[WiFi] Połączono z %s, IP: %s\n", config.sta_ssid, WiFi.localIP().toString().c_str());
        break;
      }
      delay(500);
      Serial.print(".");
    }
  }
  
  if (!wifiConnected) {
    Serial.println("\n[WiFi] Brak połączenia, uruchamiam Access Point...");
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(local_ip, gateway, subnet);
    
    wifi_power_t powerLevel;
    switch(config.wifi_power) {
      case 0: powerLevel = WIFI_POWER_8_5dBm; break;
      case 1: powerLevel = WIFI_POWER_11dBm; break;
      case 2: powerLevel = WIFI_POWER_13dBm; break;
      case 3: powerLevel = WIFI_POWER_17dBm; break;
      default: powerLevel = WIFI_POWER_19_5dBm; break;
    }
    WiFi.setTxPower(powerLevel);
    
    if (config.wifi_mode == 0) esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B);
    else if (config.wifi_mode == 1) esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11G);
    else esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11N);
    
    if (WiFi.softAP(config.wifi_ssid, config.wifi_password, 1, 0, 4)) {
      wifiStarted = true;
      Serial.printf("[WiFi] AP '%s' uruchomiony, IP: %s\n", config.wifi_ssid, WiFi.softAPIP().toString().c_str());
      WiFi.setSleep(false);
      Serial.println("[WiFi] Tryb uśpienia: WYŁĄCZONY");
    }
  } else {
    wifiStarted = true;
    WiFi.setSleep(false);
  }
  
  if (!MDNS.begin("fsr")) {
    Serial.println("[MDNS] Błąd uruchomienia mDNS");
  } else {
    Serial.println("[MDNS] Serwer nazw uruchomiony: http://fsr.local");
    MDNS.addService("http", "tcp", 80);
  }
}

// ==================== FUNKCJE GPS ====================
float calculateDistance(float lat1, float lon1, float lat2, float lon2) {
  float R = 6371;
  float dlat = (lat2 - lat1) * PI / 180;
  float dlon = (lon2 - lon1) * PI / 180;
  float a = sin(dlat/2) * sin(dlat/2) + cos(lat1*PI/180) * cos(lat2*PI/180) * sin(dlon/2) * sin(dlon/2);
  float c = 2 * atan2(sqrt(a), sqrt(1-a));
  return R * c;
}

float calculateDistanceToPoint(float lat1, float lon1, float lat2, float lon2) {
  float R = 6371000;
  float dlat = (lat2 - lat1) * PI / 180;
  float dlon = (lon2 - lon1) * PI / 180;
  float a = sin(dlat/2) * sin(dlat/2) + cos(lat1*PI/180) * cos(lat2*PI/180) * sin(dlon/2) * sin(dlon/2);
  float c = 2 * atan2(sqrt(a), sqrt(1-a));
  return R * c;
}

void checkLap(float currentLat, float currentLon, unsigned long currentTime) {
  if (config.start_line_lat == 0 && config.start_line_lon == 0) return;
  
  float distance = calculateDistanceToPoint(currentLat, currentLon, config.start_line_lat, config.start_line_lon);
  bool inside = (distance < config.start_line_radius);
  
  if (inside && !lastInsideStartLine) {
    if (config.laps == 0) {
      config.lap_start_time = currentTime;
      Serial.println("[LAP] Start pomiaru okrążeń");
    } else {
      float lapTime = (currentTime - config.lap_start_time) / 1000.0;
      if (config.best_lap_time == 0 || lapTime < config.best_lap_time) {
        config.best_lap_time = lapTime;
        config.best_lap_number = config.laps + 1;
        Serial.printf("[LAP] NOWY REKORD! %.2f s (okrążenie %d)\n", lapTime, config.best_lap_number);
      }
      config.lap_start_time = currentTime;
      config.laps++;
      saveConfig();
      Serial.printf("[LAP] Okrążenie #%d: %.2f s\n", config.laps, lapTime);
    }
  }
  lastInsideStartLine = inside;
}

void addPositionToBuffer(float lat, float lon) {
  latBuffer[bufferIndex] = lat;
  lonBuffer[bufferIndex] = lon;
  bufferIndex++;
  if (bufferIndex >= 5) { bufferIndex = 0; bufferFull = true; }
}

float getMedianLat() {
  if (!bufferFull && bufferIndex < 2) return gps.location.lat();
  float temp[5];
  int count = bufferFull ? 5 : bufferIndex;
  for (int i = 0; i < count; i++) temp[i] = latBuffer[i];
  for (int i = 0; i < count-1; i++)
    for (int j = 0; j < count-i-1; j++)
      if (temp[j] > temp[j+1]) { float t = temp[j]; temp[j] = temp[j+1]; temp[j+1] = t; }
  return temp[count/2];
}

float getMedianLon() {
  if (!bufferFull && bufferIndex < 2) return gps.location.lng();
  float temp[5];
  int count = bufferFull ? 5 : bufferIndex;
  for (int i = 0; i < count; i++) temp[i] = lonBuffer[i];
  for (int i = 0; i < count-1; i++)
    for (int j = 0; j < count-i-1; j++)
      if (temp[j] > temp[j+1]) { float t = temp[j]; temp[j] = temp[j+1]; temp[j+1] = t; }
  return temp[count/2];
}

void addTrackPoint(float lat, float lon, float speed, float temp) {
  if (trackBuffer && trackCount < MAX_TRACK_POINTS) {
    trackBuffer[trackCount] = {lat, lon, speed, temp, millis()};
    trackCount++;
  }
}

void initPSRAM() {
  if (psramFound()) {
    trackBuffer = (TrackPoint*)ps_malloc(sizeof(TrackPoint) * MAX_TRACK_POINTS);
  } else {
    trackBuffer = (TrackPoint*)malloc(sizeof(TrackPoint) * MAX_TRACK_POINTS);
  }
}

float readTemperature() {
  if (!config.temp_enabled) return 0;
  int raw = analogRead(TEMP_SENSOR_PIN);
  if (raw <= 0 || raw >= 4095) return 21.0f;
  
  float voltage = (raw / 4095.0) * 3.3;
  float resistance = (100000.0 * voltage) / (3.3 - voltage);
  float logR = log(resistance);
  float temp = 1.0 / (0.001129148 + 0.000234125 * logR + 0.0000000876741 * logR * logR * logR);
  temp = temp - 273.15;
  temp = temp * config.temp_scale + config.temp_offset;
  
  tempBuffer[tempIndex % TEMP_FILTER_WINDOW] = temp;
  tempIndex++;
  if (tempIndex >= TEMP_FILTER_WINDOW) {
    float sum = 0;
    for (int i = 0; i < TEMP_FILTER_WINDOW; i++) sum += tempBuffer[i];
    return sum / TEMP_FILTER_WINDOW;
  }
  return temp;
}

float readBattery() {
  int raw = analogRead(BATT_SENSOR_PIN);
  if (raw <= 0 || raw >= 4095) return 0.0f;
  return (raw / 4095.0) * 3.3 * 2.0;
}

// ==================== OBSŁUGA ENDPOINTÓW ====================
void handleRoot(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(200, "text/html", index_html);
  response->addHeader("Cache-Control", "public, max-age=31536000, immutable");
  request->send(response);
}

void handleInfo(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(200, "text/html", info_html);
  response->addHeader("Cache-Control", "public, max-age=31536000, immutable");
  request->send(response);
}

void handleScanWiFi(AsyncWebServerRequest *request) {
  String json = scanNetworks();
  request->send(200, "application/json", json);
}

void handleCalibrate(AsyncWebServerRequest *request) {
  Serial.println("[CALIB] Rozpoczynam kalibrację ADXL345...");
  float sumX = 0, sumY = 0, sumZ = 0;
  const int samples = 100;
  for (int i = 0; i < samples; i++) {
    sensors_event_t event;
    adxl.getEvent(&event);
    sumX += event.acceleration.x;
    sumY += event.acceleration.y;
    sumZ += event.acceleration.z;
    delay(10);
  }
  config.accel_offset_x = sumX / samples;
  config.accel_offset_y = sumY / samples;
  config.accel_offset_z = sumZ / samples - 1.0;
  config.accel_calibrated = true;
  saveConfig();
  Serial.printf("[CALIB] Offsety: X=%.3f, Y=%.3f, Z=%.3f\n", 
                config.accel_offset_x, config.accel_offset_y, config.accel_offset_z);
  request->send(200, "text/plain", "Kalibracja zakończona");
}

void handleSetStartLine(AsyncWebServerRequest *request) {
  if (!request->hasParam("plain", true)) {
    request->send(400, "text/plain", "Brak danych");
    return;
  }
  String body = request->getParam("plain", true)->value();
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, body);
  if (error) {
    request->send(400, "text/plain", "Błędny JSON");
    return;
  }
  
  if (doc.containsKey("latA") && doc.containsKey("lonA") && doc.containsKey("latB") && doc.containsKey("lonB")) {
    config.start_line_lat = doc["latA"];
    config.start_line_lon = doc["lonA"];
    config.start_line_radius = doc["radius"] | 15.0;
    saveConfig();
    request->send(200, "text/plain", "OK");
  } else if (doc.containsKey("lat") && doc.containsKey("lon")) {
    config.start_line_lat = doc["lat"];
    config.start_line_lon = doc["lon"];
    config.start_line_radius = doc["radius"] | 15.0;
    saveConfig();
    request->send(200, "text/plain", "OK");
  } else {
    request->send(400, "text/plain", "Brak współrzędnych");
  }
}

void handleData(AsyncWebServerRequest *request) {
  float displaySpeed = (currentSpeed < SPEED_THRESHOLD) ? 0 : currentSpeed;
  float lat = getMedianLat();
  float lon = getMedianLon();
  int fix = gps.location.isValid() ? 1 : 0;
  float hdop = gps.hdop.isValid() ? gps.hdop.hdop() : 99.9;
  int satellites = gps.satellites.value();
  int fixQuality = (fix && hdop < HDOP_GOOD_THRESHOLD && satellites >= MIN_SATELLITES) ? 2 : (fix ? 1 : 0);
  
  float accuracy = calculateSpeedAccuracy(satellites, hdop);
  
  float currentLapTime = 0;
  if (config.laps > 0 && config.lap_start_time > 0) {
    currentLapTime = (millis() - config.lap_start_time) / 1000.0;
  }
  
  String json = "{";
  json += "\"speed\":" + String(displaySpeed,2) + ",";
  json += "\"max_speed\":" + String(maxSpeed,2) + ",";
  json += "\"max_lat\":" + String(maxSpeedLat,6) + ",";
  json += "\"max_lon\":" + String(maxSpeedLon,6) + ",";
  json += "\"distance\":" + String(totalDistance,2) + ",";
  json += "\"lat\":" + String(lat,6) + ",";
  json += "\"lon\":" + String(lon,6) + ",";
  json += "\"sat\":" + String(satellites) + ",";
  json += "\"hdop\":" + String(hdop,1) + ",";
  json += "\"fix\":" + String(fix) + ",";
  json += "\"fix_quality\":" + String(fixQuality) + ",";
  json += "\"temp_engine\":" + String(tempEngine,0) + ",";
  json += "\"temp_enabled\":" + String(config.temp_enabled ? "true" : "false") + ",";
  json += "\"battery\":" + String(batteryVoltage,1) + ",";
  json += "\"laps\":" + String(config.laps) + ",";
  json += "\"best_lap_time\":" + String(config.best_lap_time,2) + ",";
  json += "\"best_lap_number\":" + String(config.best_lap_number) + ",";
  json += "\"current_lap_time\":" + String(currentLapTime,2) + ",";
  json += "\"accuracy\":" + String(accuracy,3) + ",";
  json += "\"wifi_connected\":" + String(wifiConnected ? "true" : "false") + ",";
  json += "\"vmax_count\":" + String(config.vmax_count);
  for (int i = 0; i < config.vmax_count && i < 10; i++) {
    json += ",\"vmax_" + String(i) + "\":" + String(config.vmax_history[i],2);
    json += ",\"vmax_lat_" + String(i) + "\":" + String(config.vmax_lat[i],6);
    json += ",\"vmax_lon_" + String(i) + "\":" + String(config.vmax_lon[i],6);
  }
  json += "}";
  
  request->send(200, "application/json", json);
}

void handleResetMax(AsyncWebServerRequest *request) {
  maxSpeed = 0; maxSpeedLat = 0; maxSpeedLon = 0;
  request->send(200, "text/plain", "OK");
}

void handleResetTrack(AsyncWebServerRequest *request) {
  trackCount = 0;
  request->send(200, "text/plain", "OK");
}

void handleResetLaps(AsyncWebServerRequest *request) {
  config.laps = 0;
  config.best_lap_time = 0;
  config.best_lap_number = 0;
  config.lap_start_time = 0;
  saveConfig();
  request->send(200, "text/plain", "OK");
}

void handleResetVMAXHistory(AsyncWebServerRequest *request) {
  config.vmax_count = 0;
  for (int i = 0; i < 10; i++) {
    config.vmax_history[i] = 0;
    config.vmax_lat[i] = 0;
    config.vmax_lon[i] = 0;
  }
  saveConfig();
  request->send(200, "text/plain", "OK");
}

void handleSaveTrack(AsyncWebServerRequest *request) {
  if (!trackBuffer || trackCount == 0) {
    request->send(404, "text/plain", "Brak danych");
    return;
  }
  request->send(200, "application/gpx+xml", "");
}

void handleGetTrack(AsyncWebServerRequest *request) {
  String json = "{\"track\":[";
  for (int i = 0; i < trackCount; i++) {
    if (i > 0) json += ",";
    json += "{\"lat\":" + String(trackBuffer[i].lat, 6) + ",\"lon\":" + String(trackBuffer[i].lon, 6) + "}";
  }
  json += "],\"vmax\":[";
  for (int i = 0; i < config.vmax_count && i < 10; i++) {
    if (i > 0) json += ",";
    json += "{\"num\":" + String(i+1) + ",\"speed\":" + String(config.vmax_history[i], 2) + 
            ",\"lat\":" + String(config.vmax_lat[i], 6) + ",\"lon\":" + String(config.vmax_lon[i], 6) + "}";
  }
  json += "],\"laps\":" + String(config.laps) + 
         ",\"best_lap_time\":" + String(config.best_lap_time, 2) +
         ",\"best_lap_number\":" + String(config.best_lap_number) +
         ",\"start_line_A_lat\":" + String(config.start_line_lat, 6) +
         ",\"start_line_A_lon\":" + String(config.start_line_lon, 6) +
         ",\"start_line_B_lat\":" + String(config.start_line_lat + 0.00009, 6) +
         ",\"start_line_B_lon\":" + String(config.start_line_lon, 6) + "}";
  request->send(200, "application/json", json);
}

void handleExportVmaxCSV(AsyncWebServerRequest *request) {
  String csv = "numer, predkosc_kmh, szerokosc, dlugosc\n";
  for (int i = 0; i < config.vmax_count && i < 10; i++) {
    csv += String(i+1) + "," + String(config.vmax_history[i], 2) + "," + 
           String(config.vmax_lat[i], 6) + "," + String(config.vmax_lon[i], 6) + "\n";
  }
  request->send(200, "text/csv", csv);
}

void handleConfigGet(AsyncWebServerRequest *request) {
  StaticJsonDocument<2048> doc;
  doc["temp_offset"] = config.temp_offset;
  doc["temp_scale"] = config.temp_scale;
  doc["temp_enabled"] = config.temp_enabled;
  doc["wifi_ssid"] = config.wifi_ssid;
  doc["wifi_password"] = config.wifi_password;
  doc["sta_ssid"] = config.sta_ssid;
  doc["sta_password"] = config.sta_password;
  doc["gps_baud"] = config.gps_baud;
  doc["gps_rx_pin"] = config.gps_rx_pin;
  doc["gps_tx_pin"] = config.gps_tx_pin;
  doc["wifi_power"] = config.wifi_power;
  doc["wifi_mode"] = config.wifi_mode;
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void handleConfigPost(AsyncWebServerRequest *request) {
  if (!request->hasParam("plain", true)) {
    request->send(400, "text/plain", "Brak danych");
    return;
  }
  String body = request->getParam("plain", true)->value();
  StaticJsonDocument<2048> doc;
  DeserializationError error = deserializeJson(doc, body);
  if (error) {
    request->send(400, "text/plain", "Błędny JSON");
    return;
  }
  bool restart = false;
  if (doc.containsKey("temp_offset")) config.temp_offset = doc["temp_offset"];
  if (doc.containsKey("temp_scale")) config.temp_scale = doc["temp_scale"];
  if (doc.containsKey("temp_enabled")) config.temp_enabled = doc["temp_enabled"];
  if (doc.containsKey("wifi_ssid")) { strlcpy(config.wifi_ssid, doc["wifi_ssid"], sizeof(config.wifi_ssid)); restart = true; }
  if (doc.containsKey("wifi_password")) { strlcpy(config.wifi_password, doc["wifi_password"], sizeof(config.wifi_password)); restart = true; }
  if (doc.containsKey("sta_ssid")) { strlcpy(config.sta_ssid, doc["sta_ssid"], sizeof(config.sta_ssid)); restart = true; }
  if (doc.containsKey("sta_password")) { strlcpy(config.sta_password, doc["sta_password"], sizeof(config.sta_password)); restart = true; }
  if (doc.containsKey("gps_baud")) { config.gps_baud = doc["gps_baud"]; restart = true; }
  if (doc.containsKey("gps_rx_pin")) { config.gps_rx_pin = doc["gps_rx_pin"]; restart = true; }
  if (doc.containsKey("gps_tx_pin")) { config.gps_tx_pin = doc["gps_tx_pin"]; restart = true; }
  if (doc.containsKey("wifi_power")) { config.wifi_power = doc["wifi_power"]; restart = true; }
  if (doc.containsKey("wifi_mode")) { config.wifi_mode = doc["wifi_mode"]; restart = true; }
  saveConfig();
  if (restart) {
    request->send(200, "text/plain", "OK restart");
    delay(500);
    ESP.restart();
  } else {
    request->send(200, "text/plain", "OK");
  }
}

// ==================== SETUP ====================
void setup() {
  delay(500);
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔════════════════════════════════════════════════════════════╗");
  Serial.println("║      FSR SPEED ACC v0.9 - GPS + ADXL345                    ║");
  Serial.println("║      AsyncWebServer | Odtwarzanie trasy | CSV              ║");
  Serial.println("╚════════════════════════════════════════════════════════════╝\n");
  
  if (!SPIFFS.begin(false)) {
    SPIFFS.format();
    SPIFFS.begin(true);
  }
  
  loadConfig();
  initPSRAM();
  
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  if (!adxl.begin()) {
    Serial.println("[ADXL345] Brak czujnika!");
  } else {
    adxl.setRange(ADXL345_RANGE_16_G);
    adxl.setDataRate(ADXL345_DATARATE_200_HZ);
    Serial.println("[ADXL345] OK");
    if (config.accel_calibrated) {
      Serial.println("[ADXL345] Wczytano kalibrację z pamięci");
    } else {
      Serial.println("[ADXL345] Brak kalibracji – użyj przycisku w ustawieniach");
    }
  }
  
  GPS_Serial.begin(config.gps_baud, SERIAL_8N1, config.gps_rx_pin, config.gps_tx_pin);
  GPS_Serial.setRxBufferSize(1024);
  delay(500);
  GPS_Serial.println("$PMTK220,100*2F");
  delay(200);
  GPS_Serial.println("$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28");
  delay(200);
  Serial.println("[GPS] OK - 10Hz, GGA+RMC");
  
  startWiFiHybrid();
  
  // ========== REJESTRACJA ENDPOINTÓW ==========
  server.on("/", HTTP_GET, handleRoot);
  server.on("/info", HTTP_GET, handleInfo);
  server.on("/api/data", HTTP_GET, handleData);
  server.on("/api/resetmax", HTTP_POST, handleResetMax);
  server.on("/api/resettrack", HTTP_POST, handleResetTrack);
  server.on("/api/resetlaps", HTTP_POST, handleResetLaps);
  server.on("/api/resetvmaxhistory", HTTP_POST, handleResetVMAXHistory);
  server.on("/api/save", HTTP_GET, handleSaveTrack);
  server.on("/api/track", HTTP_GET, handleGetTrack);
  server.on("/api/vmaxcsv", HTTP_GET, handleExportVmaxCSV);
  server.on("/api/config", HTTP_GET, handleConfigGet);
  server.on("/api/config", HTTP_POST, handleConfigPost);
  server.on("/api/scanwifi", HTTP_GET, handleScanWiFi);
  server.on("/api/calibrate", HTTP_POST, handleCalibrate);
  server.on("/api/setstartline", HTTP_POST, handleSetStartLine);
  // ============================================
  
  server.begin();
  Serial.println("[HTTP] Asynchroniczny serwer start (AsyncWebServer)");
  Serial.println("[HTTP] Nagłówki cache włączone (ważne 1 rok)");
  
  if (wifiConnected) {
    Serial.printf("[INFO] Połączono z Wi-Fi, IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[INFO] Strona: http://%s lub http://fsr.local\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.printf("[INFO] Tryb AP, IP: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("[INFO] Strona: http://%s lub http://fsr.local\n", WiFi.softAPIP().toString().c_str());
  }
  
  for (int i = 0; i < SPEED_FILTER_WINDOW; i++) speedHistory[i] = 0;
  for (int i = 0; i < 5; i++) { latBuffer[i] = 0; lonBuffer[i] = 0; }
  for (int i = 0; i < TEMP_FILTER_WINDOW; i++) tempBuffer[i] = 21.0;
  
  Serial.println("\n[SYSTEM] Gotowy! Po odświeżeniu strony trasa zostanie odtworzona.\n");
  Serial.println("[CACHE] Strona główna i info będą buforowane przez 1 rok na urządzeniu.\n");
}

// ==================== LOOP ====================
void loop() {
  while (GPS_Serial.available()) {
    gps.encode(GPS_Serial.read());
  }
  
  sensors_event_t event;
  adxl.getEvent(&event);
  float accelX = event.acceleration.x;
  float accelY = event.acceleration.y;
  float accelZ = event.acceleration.z;
  if (config.accel_calibrated) {
    accelX -= config.accel_offset_x;
    accelY -= config.accel_offset_y;
    accelZ -= config.accel_offset_z;
  }
  
  if (gps.speed.isValid() && gps.location.isValid()) {
    float rawSpeed = gps.speed.kmph();
    float filteredSpeed = getFilteredSpeed(rawSpeed);
    unsigned long now = millis();
    unsigned long dt = now - lastSpeedTime;
    
    if (lastSpeedTime > 0 && dt > 0 && dt < 500) {
      fusedSpeed = fuseSpeed(filteredSpeed, accelX, dt);
    } else {
      fusedSpeed = filteredSpeed;
    }
    
    float hdop = gps.hdop.isValid() ? gps.hdop.hdop() : 99.9;
    int sat = gps.satellites.value();
    
    if (hdop < HDOP_GOOD_THRESHOLD && sat >= MIN_SATELLITES) {
      if (fusedSpeed > maxSpeed && fusedSpeed > VMAX_MIN_SPEED) {
        maxSpeed = fusedSpeed;
        maxSpeedLat = getMedianLat();
        maxSpeedLon = getMedianLon();
        addVMAXToHistory(maxSpeed, maxSpeedLat, maxSpeedLon);
        Serial.printf("[VMAX] Nowy rekord: %.2f km/h\n", maxSpeed);
      }
    }
    
    lastSpeed = fusedSpeed;
    lastSpeedTime = now;
    currentSpeed = fusedSpeed;
  }
  
  if (gps.location.isValid()) {
    float lat = gps.location.lat();
    float lon = gps.location.lng();
    addPositionToBuffer(lat, lon);
    
    float hdop = gps.hdop.isValid() ? gps.hdop.hdop() : 99.9;
    int sat = gps.satellites.value();
    
    checkLap(lat, lon, millis());
    
    if (!startPositionRecorded && !config.start_set && hdop < HDOP_GOOD_THRESHOLD && sat >= MIN_SATELLITES) {
      config.start_lat = lat;
      config.start_lon = lon;
      config.start_set = true;
      startPositionRecorded = true;
      saveConfig();
      Serial.printf("[START] Zapisano pozycję startową: %.6f, %.6f\n", lat, lon);
    }
    
    if (hdop < 2.0 && sat >= 4) {
      if (!firstFix) {
        float dist = calculateDistance(lastLat, lastLon, lat, lon);
        if (dist * 1000 > DISTANCE_MIN) totalDistance += dist;
      } else firstFix = false;
      lastLat = lat;
      lastLon = lon;
    }
  }
  
  static unsigned long lastTemp = 0;
  if (millis() - lastTemp >= 500) { lastTemp = millis(); tempEngine = readTemperature(); }
  
  static unsigned long lastBatt = 0;
  if (millis() - lastBatt >= 2000) { lastBatt = millis(); batteryVoltage = readBattery(); }
  
  if (millis() - lastDataTime >= 100) {
    lastDataTime = millis();
    if (gps.location.isValid()) {
      float hdop = gps.hdop.isValid() ? gps.hdop.hdop() : 99.9;
      int sat = gps.satellites.value();
      if (hdop < 2.0 && sat >= 4) {
        float lat = getMedianLat();
        float lon = getMedianLon();
        float dispSpeed = (currentSpeed < SPEED_THRESHOLD) ? 0 : currentSpeed;
        addTrackPoint(lat, lon, dispSpeed, tempEngine);
      }
    }
  }
  
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 5000) {
    lastDebug = millis();
    if (gps.location.isValid()) {
      float hdop = gps.hdop.isValid() ? gps.hdop.hdop() : 99.9;
      int clients = WiFi.softAPgetStationNum();
      Serial.printf("📍 %.6f,%.6f | ⚡ %.2f km/h | VMAX:%.2f | SAT:%d | HDOP:%.1f | 🏁 %d | Klienci: %d\n",
        getMedianLat(), getMedianLon(), currentSpeed, maxSpeed, gps.satellites.value(), hdop, config.laps, clients);
    }
  }
  
  delay(1);
}