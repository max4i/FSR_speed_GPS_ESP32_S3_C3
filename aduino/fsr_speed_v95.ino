/*
 * ============================================================================
 * FSR SPEED TRACKER v9.4 - OPIS TECHNICZNY
 * ============================================================================
 * 
 * AUTOR:        Maxiii & Deepseek
 * WERSJA:       9.4 (final)
 * DATA:         2026-04-14
 * 
 * ============================================================================
 * OPIS OGÓLNY SYSTEMU
 * ============================================================================
 * 
 * System telemetrii dedykowany dla łodzi wyścigowych FSR (Formuła Silnikowa
 * Remote control). Urządzenie zbiera dane z GPS, czujnika temperatury silnika,
 * czujnika obrotów (RPM) oraz napięcia baterii. Dane są wyświetlane w czasie
 * rzeczywistym na mapie satelitarnej przez przeglądarkę internetową.
 * 
 * Główne funkcje:
 * - Śledzenie trasy na mapie satelitarnej Esri World Imagery (zoom 1-20)
 * - Wyświetlanie aktualnej prędkości, VMAX i maksymalnego przyspieszenia
 * - Trzy wykresy: prędkość (km/h), temperatura (°C), przyspieszenie (G)
 * - Automatyczne wykrywanie okrążeń z pomiarem czasów
 * - Zapisywanie trasy do pliku GPX z danymi: temp, RPM, przyspieszenie
 * - Regulacja mocy i trybu Wi-Fi (zasięg do 300m)
 * - Konfiguracja przez przeglądarkę internetową
 * - Tryb mobilny (automatyczne ukrywanie wykresów)
 * 
 * ============================================================================
 * BUDOWA SPRZĘTOWA
 * ============================================================================
 * 
 * GŁÓWNE ELEMENTY:
 * ----------------------------------------------------------------------------
 * | Element              | Model                      | Funkcja               |
 * |----------------------|----------------------------|-----------------------|
 * | Mikrokontroler       | ESP32-S3 SuperMini         | CPU, Wi-Fi AP, PSRAM  |
 * | Moduł GPS            | HT1818Z3G5L z AT6558R      | GPS+BDS Beidou, 10Hz  |
 * | Antena GPS           | GP 5/8 (złączka U.FL)      | Zwiększenie zasięgu   |
 * | Termistor            | NTC 100kΩ (Beta=3950)      | Pomiar temperatury    |
 * | Rezystor             | 100kΩ (0.25W)              | Dzielnik z termistorem|
 * | Czujnik Halla        | 3144                        | Pomiar obrotów (RPM)  |
 * | Rezystory dzielnika  | 2x 100kΩ (0.25W)           | Dzielnik napięcia 1:2 |
 * | Kondensator          | 1000µF/6.3V                | Stabilizacja 3.3V     |
 * 
 * ============================================================================
 * PODŁĄCZENIA (ESP32-S3 SuperMini)
 * ============================================================================
 * 
 * UWAGA: Unikamy pinów 19 i 20 (zajęte przez native USB ESP32-S3)
 * 
 * GPS (moduł HT1818Z3G5L):
 * ----------------------------------------------------------------------------
 * | GPIO ESP32 | Funkcja     | Podłączenie                               |
 * |------------|-------------|-------------------------------------------|
 * | 12         | RX (odbiór) | TX (nadajnik modułu GPS)                  |
 * | 13         | TX (nadajnik)| RX (odbiór modułu GPS)                   |
 * |            |             |                                           |
 * | Prędkość:  230400 baud (autodetekcja 4800-230400)                      |
 * | Częstotliwość: 10Hz (komendy PMTK220,100)                              |
 * | Komunikaty: tylko GGA i RMC (PMTK314)                                  |
 * | Zapis:     PMTK286,1 (zapis do pamięci modułu)                         |
 * 
 * Czujnik temperatury (NTC 100kΩ):
 * ----------------------------------------------------------------------------
 * | GPIO ESP32 | Funkcja              | Podłączenie                            |
 * |------------|----------------------|----------------------------------------|
 * | 11         | Analogowy (ADC1)     | Dzielnik napięcia:                     |
 * |            |                      | 3.3V ---[100kΩ]---[NTC]---GND         |
 * |            |                      |           |                            |
 * |            |                      |        GPIO11                          |
 * |            |                      |                                        |
 * | Uwaga: NTC 100kΩ (Beta=3950) do pomiaru temperatury silnika            |
 * |        Zakres pomiarowy: 0-260°C (typowo do 180°C)                     |
 * |        Filtr: średnia ruchoma z 10 próbek                              |
 * 
 * Czujnik obrotów (Hallotron 3144):
 * ----------------------------------------------------------------------------
 * | GPIO ESP32 | Funkcja              | Podłączenie                            |
 * |------------|----------------------|----------------------------------------|
 * | 10         | INPUT_PULLUP         | Sygnał z czujnika Halla                |
 * |            |                      | (zwiera do GND przy wykryciu magnesu)  |
 * |            |                      |                                        |
 * | Uwaga: Magnes zamontowany na wale silnika generuje 1 impuls/obrót      |
 * |        Przerwanie na zboczu opadającym (FALLING)                        |
 * |        Filtr: mediana z 5 próbek                                       |
 * 
 * Pomiar napięcia baterii (dzielnik napięcia):
 * ----------------------------------------------------------------------------
 * | GPIO ESP32 | Funkcja              | Podłączenie                            |
 * |------------|----------------------|----------------------------------------|
 * | 8          | Analogowy (ADC1)     | Dzielnik 1:2:                          |
 * |            |                      | Bateria(+) ---[100kΩ]---[100kΩ]---GND |
 * |            |                      |                    |                   |
 * |            |                      |                 GPIO8                  |
 * |            |                      |                                        |
 * | Uwaga: Zakres pomiarowy: 0-6.6V (dla LiPo 2S max 8.4V zmień dzielnik)  |
 * |        Wzór: V_batt = (V_adc / 4095) * 3.3 * 2.0                       |
 * 
 * Zasilanie:
 * ----------------------------------------------------------------------------
 * | Wersja           | Zasilanie              | Pobór prądu | Uwagi                |
 * |------------------|------------------------|-------------|-----------------------|
 * | Podstawowa (lekka)| Z odbiornika RC 5V     | ~200mA      | Brak pomiaru baterii  |
 * |                  |                        |             | i RPM (opcjonalnie)   |
 * |------------------|------------------------|-------------|-----------------------|
 * | Rozszerzona      | Bateria LiPo 2S 7.4V   | ~350mA      | Pełna funkcjonalność  |
 * | (pełna)          | + stabilizator 5V/1A   |             |                       |
 * |------------------|------------------------|-------------|-----------------------|
 * | Kondensator      | 1000µF/6.3V między     | -           | Stabilizacja napięcia |
 * |                  | 3.3V a GND             |             | przy skokach prądu    |
 * 
 * ============================================================================
 * OBLICZENIA MATEMATYCZNE
 * ============================================================================
 * 
 * 1. PRĘDKOŚĆ (km/h):
 * ----------------------------------------------------------------------------
 *    Odczyt z ramki NMEA RMC (gps.speed.kmph())
 *    Filtr: prędkość < 3 km/h jest ignorowana (antydryf)
 *    Jednostka: km/h
 * 
 * 2. PRZYSPIESZENIE (m/s² i G):
 * ----------------------------------------------------------------------------
 *    a = (V_current - V_last) / dt
 *    
 *    gdzie:
 *    - V_current: aktualna prędkość [m/s] = km/h / 3.6
 *    - V_last: poprzednia prędkość [m/s]
 *    - dt: czas między pomiarami [s] (max 1 sekunda)
 *    
 *    Przeliczenie na G:
 *    1 G = 9.80665 m/s²
 *    G = a / 9.80665
 *    
 *    Filtr: mediana z 5 ostatnich próbek
 *    Warunek: prędkość > 10 km/h (ACCEL_MIN_SPEED)
 * 
 * 3. TEMPERATURA (°C) dla NTC 100kΩ (Beta=3950):
 * ----------------------------------------------------------------------------
 *    R = (100000 * V_adc) / (3.3 - V_adc)
 *    
 *    gdzie:
 *    - R: rezystancja termistora [Ω]
 *    - V_adc: napięcie na GPIO11 [V] = (raw / 4095) * 3.3
 *    - raw: wartość ADC (0-4095)
 *    
 *    Równanie Steinharta-Harta:
 *    1/T = A + B*ln(R) + C*(ln(R))³
 *    
 *    gdzie:
 *    - A = 0.001129148
 *    - B = 0.000234125
 *    - C = 0.0000000876741
 *    - T: temperatura [K]
 *    
 *    Przeliczenie na °C:
 *    T[°C] = T[K] - 273.15
 *    
 *    Kalibracja:
 *    T_rzeczywista = T_odczytana * skala + offset
 *    - offset: domyślnie 38.0°C (korekta przesunięcia)
 *    - skala: domyślnie 1.0
 *    
 *    Filtr: średnia ruchoma z 10 próbek
 * 
 * 4. OBRÓTY (RPM):
 * ----------------------------------------------------------------------------
 *    RPM = (impulsy * 60000) / dt
 *    
 *    gdzie:
 *    - impulsy: liczba impulsów Hallotrona w ciągu dt
 *    - dt: czas pomiaru [ms] (minimum 1000ms)
 *    
 *    Filtr: mediana z 5 próbek
 *    Przerwanie: attachInterrupt(FALLING)
 * 
 * 5. NAPIĘCIE BATERII (V):
 * ----------------------------------------------------------------------------
 *    V_batt = (V_adc / 4095.0) * 3.3 * (R1+R2)/R2
 *    
 *    gdzie:
 *    - V_adc: napięcie na GPIO8 [V]
 *    - R1 = 100000Ω (górny rezystor)
 *    - R2 = 100000Ω (dolny rezystor)
 *    - Współczynnik dzielnika = 2.0
 *    
 *    Uproszczenie: V_batt = (raw / 4095.0) * 3.3 * 2.0
 * 
 * 6. DYSTANS (km):
 * ----------------------------------------------------------------------------
 *    D = R * acos(sin(lat1)*sin(lat2) + cos(lat1)*cos(lat2)*cos(dlon))
 *    
 *    gdzie:
 *    - R = 6371 km (promień Ziemi)
 *    - lat1, lon1: poprzednia pozycja [rad]
 *    - lat2, lon2: aktualna pozycja [rad]
 *    - dlon = lon2 - lon1 [rad]
 *    
 *    Minimalny dystans między punktami: 0.5 metra (DISTANCE_MIN)
 * 
 * 7. OKRĄŻENIA:
 * ----------------------------------------------------------------------------
 *    Odległość od punktu start/meta:
 *    D = R * acos(sin(lat1)*sin(lat2) + cos(lat1)*cos(lat2)*cos(dlon))
 *    
 *    gdzie R = 6371000 metrów
 *    
 *    Warunek: D < promień (domyślnie 15 metrów)
 *    Wykrycie: wejście w strefę (inside && !lastInside)
 *    Czas okrążenia: (czas_aktualny - czas_startu_okrążenia) / 1000 [s]
 * 
 * ============================================================================
 * FILTRY I ZABEZPIECZENIA
 * ============================================================================
 * 
 * | Filtr/Zabezpieczenie     | Wartość        | Opis                           |
 * |--------------------------|----------------|--------------------------------|
 * | Antydryf                 | 3 km/h         | Prędkość poniżej wyświetlana 0 |
 * | Mediana pozycji          | 5 próbek       | Bufor pozycji GPS              |
 * | HDOP                      | < 2.0          | Pozycja odrzucana przy słabej  |
 * |                           |                | dokładności                    |
 * | Minimalna liczba satelitów| ≥ 4            | Pozycja odrzucana              |
 * | Minimalny dystans punktów | 0.5 m          | Unikanie zapisu w miejscu      |
 * | Minimalna prędkość VMAX   | 5 km/h         | Tylko powyżej zapisujemy rekord|
 * | Minimalna prędkość ACCEL  | 10 km/h        | Tylko powyżej zapisujemy rekord|
 * 
 * ============================================================================
 * KONFIGURACJA WI-FI (OPTYMALIZACJA ZASIĘGU)
 * ============================================================================
 * 
 * Moc nadajnika (dBm) i zasięg:
 * ----------------------------------------------------------------------------
 * | Wartość | dBm    | Zasięg   | Kiedy używać                        |
 * |---------|--------|----------|-------------------------------------|
 * | 0       | 8.5    | ~30m     | Testy na stole, oszczędzanie baterii|
 * | 1       | 11     | ~50m     | Małe akweny                         |
 * | 2       | 13     | ~80m     | Standardowe warunki                 |
 * | 3       | 17     | ~150m    | Duże akweny                         |
 * | 4       | 19     | ~200m    | Wyścigi, duży zasięg                |
 * | 5       | 19.5   | ~300m    | Maksymalny zasięg (domyślny na stole)|
 * 
 * Tryb Wi-Fi:
 * ----------------------------------------------------------------------------
 * | Tryb    | Prędkość | Zasięg | Zakłócenia | Kiedy używać            |
 * |---------|----------|--------|------------|-------------------------|
 * | 802.11b | 11 Mbps  | Najlepszy | Najmniejsze | Maksymalny zasięg      |
 * | 802.11g | 54 Mbps  | Dobry  | Średnie    | Kompromis (zalecany)   |
 * | 802.11n | 150 Mbps | Średni | Największe | Dużo danych, bliski    |
 * 
 * Kanał: 1 (najlepsza penetracja, najmniej zakłóceń)
 * 
 * ============================================================================
 * KOMENDY GPS (wysyłane do modułu AT6558R)
 * ============================================================================
 * 
 * | Komenda                    | Opis                                      |
 * |----------------------------|-------------------------------------------|
 * | $PMTK220,100*2F            | Ustawienie odświeżania na 10Hz (100ms)   |
 * | $PMTK314,0,1,0,1,...*28    | Wyłączenie wszystkich NMEA oprócz GGA i  |
 * |                            | RMC                                       |
 * | $PMTK251,230400*...        | Zmiana prędkości transmisji na 230400    |
 * | $PMTK286,1*23              | Zapisz ustawienia w pamięci modułu GPS   |
 * 
 * ============================================================================
 * STRUKTURA DANYCH W PAMIĘCI PSRAM
 * ============================================================================
 * 
 * struct TrackPoint {
 *   float lat;              // Szerokość geograficzna (6 miejsc po przecinku)
 *   float lon;              // Długość geograficzna (6 miejsc po przecinku)
 *   float speed;            // Prędkość [km/h] (1 miejsce po przecinku)
 *   float temp;             // Temperatura silnika [°C] (1 miejsce)
 *   float rpm;              // Obroty silnika [RPM] (0 miejsc)
 *   float accel;            // Przyspieszenie [m/s²] (2 miejsca)
 *   unsigned long timestamp;// Znacznik czasu [ms] (millis())
 * };
 * 
 * Bufor: 6000 punktów = ok. 30-60 minut jazdy przy 3 punktach/sekundę
 * 
 * ============================================================================
 * DIAGNOSTYKA (Serial Monitor)
 * ============================================================================
 * 
 * Prędkość: 115200 baud
 * 
 * Co 5 sekund wyświetlane:
 * ----------------------------------------------------------------------------
 * 📍 52.591629,15.489385 | ⚡ 45.2 km/h | SAT:8 | VMAX:67.3 | ACCEL:2.45 m/s²
 * 🌡️ 51.2 C | 🔄 12500 RPM | 🔋 7.4 V | 📦 1250 pkt | Wi-Fi: OK
 * 🏁 Okrążenia: 5 | Najlepszy czas: 42.35 s
 * 
 * ============================================================================
 * KONFIGURACJA W ARDUINO IDE
 * ============================================================================
 * 
 * Dla ESP32-S3 SuperMini:
 * ----------------------------------------------------------------------------
 * | Ustawienie                | Wartość                                      |
 * |---------------------------|----------------------------------------------|
 * | Board                     | ESP32S3 Dev Module                           |
 * | USB Mode                  | CDC (Native USB)                             |
 * | USB CDC On Boot           | Enabled                                      |
 * | CPU Frequency             | 80 MHz (dla stabilności Wi-Fi)               |
 * | Flash Mode                | DIO                                          |
 * | Flash Size                | 4MB (32Mb)                                   |
 * | Partition Scheme          | Default 4MB with spiffs (1.2MB APP/1.5MB    |
 * |                           | SPIFFS)                                      |
 * | PSRAM                     | OPI PSRAM                                    |
 * | Upload Speed              | 921600                                       |
 * 
 * ============================================================================
 * PROBLEMY I ROZWIĄZANIA
 * ============================================================================
 * 
 * 1. Brak FIX GPS:
 *    - Sprawdź połączenia GPS (TX->GPIO12, RX->GPIO13)
 *    - Sprawdź zasilanie GPS (3.3V lub 5V)
 *    - Upewnij się, że antena GPS ma widok na niebo
 *    - Sprawdź czy moduł GPS ma podłączoną antenę
 * 
 * 2. Temperatura pokazuje 0°C:
 *    - Włącz czujnik w ustawieniach
 *    - Sprawdź połączenie termistora i rezystora 100kΩ
 *    - Sprawdź czy termistor nie jest uszkodzony
 * 
 * 3. Brak odczytu RPM:
 *    - Włącz czujnik w ustawieniach
 *    - Sprawdź odległość Hallotrona od magnesu (2-5mm)
 *    - Sprawdź czy magnes jest zamontowany prawidłowo
 *    - Sprawdź polaryzację magnesu (czujnik Halla reaguje na biegun N)
 * 
 * 4. ESP32 nie tworzy sieci Wi-Fi:
 *    - Naciśnij przycisk RESET na płycie
 *    - Zmień kanał Wi-Fi na 1 lub 6
 *    - Zmniejsz moc nadajnika (na stole wystarczy 8.5 dBm)
 *    - Sprawdź czy antena Wi-Fi jest podłączona
 * 
 * 5. Błąd "PSRAM not found":
 *    - Niektóre płyty ESP32-S3 nie mają PSRAM
 *    - System automatycznie użyje zwykłej pamięci RAM
 *    - Mniejsza pojemność bufora trasy (ale działa)
 * 
 * 6. Zasięg Wi-Fi za mały:
 *    - Zwiększ moc nadajnika w ustawieniach (19.5 dBm)
 *    - Ustaw tryb 802.11b (maksymalny zasięg)
 *    - Podnieś antenę wyżej
 *    - Użyj anteny z większym zyskiem (GP 5/8)
 * 
 * ============================================================================
 * WERSJE OPROGRAMOWANIA
 * ============================================================================
 * 
 * v9.0 - Podstawowa wersja z GPS, temp, RPM, mapą
 * v9.1 - Autodetekcja baudrate, konfiguracja 10Hz, filtry NMEA
 * v9.2 - Wykresy po lewej, tryb mobilny, temperatura do 260°C
 * v9.3 - Dymki VMAX i ACCEL (km/h i G), współrzędne startowe
 * v9.4 - Okrążenia, regulacja mocy Wi-Fi, poprawki wyświetlania
 * 
 
 */

#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <HardwareSerial.h>
#include <TinyGPSPlus.h>
#include <ArduinoJson.h>
#include <esp_wifi.h>

// ==================== PINY DLA ESP32-S3 SUPERMINI ====================
#define GPS_RX_PIN 12
#define GPS_TX_PIN 13
#define GPS_BAUD 230400
#define TEMP_SENSOR_PIN 11
#define RPM_SENSOR_PIN 10
#define BATT_SENSOR_PIN 8

// ==================== DOMYŚLNA KONFIGURACJA ====================
struct Config {
  float temp_offset = 38.0;
  float temp_scale = 1.0;
  bool temp_enabled = true;
  bool rpm_enabled = true;
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
  // DOMYŚLNIE: niska moc (łatwiej znaleźć sieć) i szybki tryb
  int wifi_power = 0;        // 0=8.5dBm, 1=11dBm, 2=13dBm, 3=17dBm, 4=19dBm, 5=19.5dBm
  int wifi_mode = 2;         // 0=802.11b, 1=802.11g, 2=802.11n
};

Config config;

// ==================== WI-FI ====================
IPAddress local_ip(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);
TinyGPSPlus gps;
HardwareSerial GPS_Serial(1);

// ==================== FILTRY ====================
#define SPEED_THRESHOLD 3.0
#define POSITION_BUFFER 5
#define DISTANCE_MIN 0.5
#define MAX_TRACK_POINTS 6000
#define RPM_FILTER_WINDOW 5
#define TEMP_FILTER_WINDOW 10
#define ACCEL_FILTER_WINDOW 5
#define HDOP_GOOD_THRESHOLD 1.1
#define MIN_SATELLITES 6
#define VMAX_MIN_SPEED 5.0
#define ACCEL_MIN_SPEED 10.0
#define MS2_TO_G 0.101971621

// ==================== ZMIENNE ====================
float currentSpeed = 0;
float maxSpeed = 0;
float maxSpeedLat = 0;
float maxSpeedLon = 0;
float maxAccel = 0;
float maxAccelLat = 0;
float maxAccelLon = 0;
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

float tempEngine = 0.0f;
float tempBuffer[TEMP_FILTER_WINDOW];
int tempIndex = 0;

volatile unsigned long rpmPulseCount = 0;
unsigned long lastRpmTime = 0;
float currentRPM = 0.0f;
float rpmHistory[RPM_FILTER_WINDOW];
int rpmHistoryIndex = 0;
bool rpmHistoryReady = false;

float batteryVoltage = 0.0f;

float currentAccel = 0.0f;
float accelHistory[ACCEL_FILTER_WINDOW];
int accelIndex = 0;
bool accelReady = false;

struct TrackPoint {
    float lat;
    float lon;
    float speed;
    float temp;
    float rpm;
    float accel;
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
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) {
    Serial.println("[CONFIG] Błąd parsowania, używam domyślnych");
    return;
  }
  config.temp_offset = doc["temp_offset"] | 38.0;
  config.temp_scale = doc["temp_scale"] | 1.0;
  config.temp_enabled = doc["temp_enabled"] | true;
  config.rpm_enabled = doc["rpm_enabled"] | true;
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
  StaticJsonDocument<1024> doc;
  doc["temp_offset"] = config.temp_offset;
  doc["temp_scale"] = config.temp_scale;
  doc["temp_enabled"] = config.temp_enabled;
  doc["rpm_enabled"] = config.rpm_enabled;
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
  
  // ========== USTAWIENIA MOCY I TRYBU WEDŁUG KONFIGURACJI ==========
  
  // Ustawienie mocy nadajnika (poprawione stałe dla ESP32)
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
    case 0: Serial.println("8.5 dBm (niska - łatwiej znaleźć sieć)"); break;
    case 1: Serial.println("11 dBm (średnia)"); break;
    case 2: Serial.println("13 dBm (wysoka)"); break;
    case 3: Serial.println("17 dBm (bardzo wysoka)"); break;
    case 4: Serial.println("19 dBm (maksymalna)"); break;
    default: Serial.println("19.5 dBm (pełna moc)"); break;
  }
  
  // Ustawienie trybu Wi-Fi
  if (config.wifi_mode == 0) {
    esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B);
    Serial.println("[WiFi] Tryb: 802.11b (maksymalny zasięg, wolniejszy)");
  } else if (config.wifi_mode == 1) {
    esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11G);
    Serial.println("[WiFi] Tryb: 802.11g (kompromis zasięg/prędkość)");
  } else {
    esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11N);
    Serial.println("[WiFi] Tryb: 802.11n (maksymalna prędkość, łatwiej znaleźć sieć)");
  }
  
  delay(50);
  
  // Ustawienie kanału (zawsze 1 dla najlepszego zasięgu)
  if (WiFi.softAP(config.wifi_ssid, config.wifi_password, 1, 0, 4)) {
    Serial.printf("[WiFi] AP '%s' uruchomiony\n", config.wifi_ssid);
    Serial.printf("[WiFi] Hasło: %s\n", config.wifi_password);
    Serial.printf("[WiFi] IP: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("[WiFi] Kanał: 1\n");
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
      Serial.println("[LAP] START! Rozpoczęcie pomiaru okrążeń");
    } else {
      float lapTime = (currentTime - config.lap_start_time) / 1000.0;
      
      if (config.best_lap_time == 0 || lapTime < config.best_lap_time) {
        config.best_lap_time = lapTime;
        Serial.printf("[LAP] NOWY REKORD! %.2f sekund\n", lapTime);
      }
      
      Serial.printf("[LAP] Okrążenie #%d: %.2f sekund\n", config.laps + 1, lapTime);
      
      config.lap_start_time = currentTime;
      config.laps++;
      saveConfig();
    }
  }
  
  lastInsideStartLine = inside;
}

// ==================== FUNKCJE POMOCNICZE ====================
void initPSRAM() {
  if (psramFound()) {
    Serial.printf("[PSRAM] Znaleziono PSRAM: %d bajtów\n", ESP.getPsramSize());
    trackBuffer = (TrackPoint*)ps_malloc(sizeof(TrackPoint) * MAX_TRACK_POINTS);
    if (trackBuffer) {
      Serial.printf("[PSRAM] Bufor trasy zaalokowany (%d punktow)\n", MAX_TRACK_POINTS);
    } else {
      Serial.println("[PSRAM] Brak pamięci PSRAM, używam zwykłej RAM");
      trackBuffer = (TrackPoint*)malloc(sizeof(TrackPoint) * MAX_TRACK_POINTS);
    }
  } else {
    Serial.println("[PSRAM] Brak PSRAM, używam zwykłej RAM");
    trackBuffer = (TrackPoint*)malloc(sizeof(TrackPoint) * MAX_TRACK_POINTS);
  }
}

void IRAM_ATTR rpmISR() { rpmPulseCount++; }

float readRPM() {
  if (!config.rpm_enabled) return 0;
  if (rpmPulseCount == 0) return 0;
  unsigned long now = millis();
  unsigned long dt = now - lastRpmTime;
  if (dt >= 1000) {
    float rawRPM = (rpmPulseCount * 60000.0) / dt;
    rpmPulseCount = 0;
    lastRpmTime = now;
    rpmHistory[rpmHistoryIndex % RPM_FILTER_WINDOW] = rawRPM;
    rpmHistoryIndex++;
    if (rpmHistoryIndex >= RPM_FILTER_WINDOW) rpmHistoryReady = true;
    if (rpmHistoryReady) {
      float sorted[RPM_FILTER_WINDOW];
      memcpy(sorted, rpmHistory, sizeof(rpmHistory));
      for (int i = 0; i < RPM_FILTER_WINDOW-1; i++)
        for (int j = 0; j < RPM_FILTER_WINDOW-i-1; j++)
          if (sorted[j] > sorted[j+1]) { float t=sorted[j]; sorted[j]=sorted[j+1]; sorted[j+1]=t; }
      return sorted[RPM_FILTER_WINDOW/2];
    }
    return rawRPM;
  }
  return currentRPM;
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
    for (int i=0; i<TEMP_FILTER_WINDOW; i++) sum += tempBuffer[i];
    return sum / TEMP_FILTER_WINDOW;
  }
  return temp;
}

float readBattery() {
  int raw = analogRead(BATT_SENSOR_PIN);
  if (raw <= 0 || raw >= 4095) return 0.0f;
  float voltage = (raw / 4095.0) * 3.3 * 2.0;
  return voltage;
}

float calculateAcceleration(float currentSpd, float lastSpd, unsigned long dt) {
  if (dt == 0) return 0;
  if (currentSpd < ACCEL_MIN_SPEED && lastSpd < ACCEL_MIN_SPEED) return 0;
  float currentMs = currentSpd / 3.6;
  float lastMs = lastSpd / 3.6;
  float dtSec = dt / 1000.0;
  float accel = (currentMs - lastMs) / dtSec;
  
  accelHistory[accelIndex % ACCEL_FILTER_WINDOW] = accel;
  accelIndex++;
  if (accelIndex >= ACCEL_FILTER_WINDOW) accelReady = true;
  
  if (accelReady) {
    float sorted[ACCEL_FILTER_WINDOW];
    memcpy(sorted, accelHistory, sizeof(accelHistory));
    for (int i = 0; i < ACCEL_FILTER_WINDOW-1; i++)
      for (int j = 0; j < ACCEL_FILTER_WINDOW-i-1; j++)
        if (sorted[j] > sorted[j+1]) { float t=sorted[j]; sorted[j]=sorted[j+1]; sorted[j+1]=t; }
    return sorted[ACCEL_FILTER_WINDOW/2];
  }
  return accel;
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

void addTrackPoint(float lat, float lon, float speed, float temp, float rpm, float accel) {
  if (trackBuffer && trackCount < MAX_TRACK_POINTS) {
    trackBuffer[trackCount] = {lat, lon, speed, temp, rpm, accel, millis()};
    trackCount++;
  }
}

// ==================== KONFIGURACJA GPS ====================
bool autoDetectGPSBaud() {
  const unsigned long baudrates[] = {4800, 9600, 19200, 38400, 57600, 115200, 230400};
  const int numBauds = sizeof(baudrates) / sizeof(baudrates[0]);
  
  Serial.println("[GPS] Rozpoczynam autodetekcję prędkości...");
  
  for (int i = 0; i < numBauds; i++) {
    unsigned long baud = baudrates[i];
    Serial.printf("[GPS] Próba %d baud\n", baud);
    GPS_Serial.begin(baud, SERIAL_8N1, config.gps_rx_pin, config.gps_tx_pin);
    GPS_Serial.setRxBufferSize(1024);
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
      Serial.printf("[GPS] Znaleziono poprawną ramkę przy %d baud\n", baud);
      return true;
    }
  }
  
  Serial.println("[GPS] Autodetekcja nie powiodła się");
  return false;
}

void configureGPS() {
  Serial.printf("[GPS] Konfiguracja - docelowy baud: %d\n", config.gps_baud);
  
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
  json += "\"max_accel\":" + String(maxAccel,2) + ",";
  json += "\"max_accel_g\":" + String(maxAccel * MS2_TO_G,2) + ",";
  json += "\"max_accel_lat\":" + String(maxAccelLat,6) + ",";
  json += "\"max_accel_lon\":" + String(maxAccelLon,6) + ",";
  json += "\"distance\":" + String(totalDistance,3) + ",";
  json += "\"lat\":" + String(lat,6) + ",";
  json += "\"lon\":" + String(lon,6) + ",";
  json += "\"sat\":" + String(satellites) + ",";
  json += "\"hdop\":" + String(hdop,1) + ",";
  json += "\"fix\":" + String(fix) + ",";
  json += "\"fix_quality\":" + String(fixQuality) + ",";
  json += "\"temp_engine\":" + String(tempEngine,1) + ",";
  json += "\"rpm\":" + String(currentRPM,0) + ",";
  json += "\"battery\":" + String(batteryVoltage,2) + ",";
  json += "\"accel\":" + String(currentAccel,2) + ",";
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
  maxAccel=0; maxAccelLat=0; maxAccelLon=0;
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
  Serial.println("[LAP] Resetowano licznik okrążeń");
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
    Serial.printf("[LAP] Ustawiono linię start/meta: %.6f, %.6f (promień %.1f m)\n", 
                  config.start_line_lat, config.start_line_lon, config.start_line_radius);
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
  server.sendContent("<metadata><desc>FSR Race Track - " + String(trackCount) + " points, Max Speed: " + String(maxSpeed,1) + " km/h, Max Accel: " + String(maxAccel * MS2_TO_G,2) + " G, Laps: " + String(config.laps) + "</desc></metadata>\n");
  server.sendContent("<trk><name>FSR Race Track</name><trkseg>\n");
  for (int i=0; i<trackCount; i++) {
    char point[512];
    snprintf(point, sizeof(point),
      "<trkpt lat=\"%.6f\" lon=\"%.6f\"><speed>%.1f</speed><extensions><temp>%.1f</temp><rpm>%.0f</rpm><accel>%.2f</accel></extensions><time>%lu</time></trkpt>\n",
      trackBuffer[i].lat, trackBuffer[i].lon, trackBuffer[i].speed, trackBuffer[i].temp, trackBuffer[i].rpm, trackBuffer[i].accel, trackBuffer[i].timestamp);
    server.sendContent(point);
    if (i%100==0) delay(1);
  }
  server.sendContent("</trkseg></trk></gpx>\n");
  Serial.printf("[DOWNLOAD] Wyslano %d punktow\n", trackCount);
}

void handleConfigGet() {
  StaticJsonDocument<1024> doc;
  doc["temp_offset"] = config.temp_offset;
  doc["temp_scale"] = config.temp_scale;
  doc["temp_enabled"] = config.temp_enabled;
  doc["rpm_enabled"] = config.rpm_enabled;
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
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  if (error) { server.send(400, "text/plain", "Błędny JSON"); return; }
  bool restart = false;
  if (doc.containsKey("temp_offset")) config.temp_offset = doc["temp_offset"];
  if (doc.containsKey("temp_scale")) config.temp_scale = doc["temp_scale"];
  if (doc.containsKey("temp_enabled")) config.temp_enabled = doc["temp_enabled"];
  if (doc.containsKey("rpm_enabled")) config.rpm_enabled = doc["rpm_enabled"];
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
  Serial.println("║      FSR SPEED v9.4 - ESP32-S3 SUPERMINI                  ║");
  Serial.println("║      OKRĄŻENIA + REGULACJA MOCY WI-FI                      ║");
  Serial.println("╚════════════════════════════════════════════════════════════╝\n");
  
  // Inicjalizacja SPIFFS
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
  initPSRAM();
  
  // Wi-Fi i serwer (NAJPIERW!)
  startWiFi();
  startServer();
  
  // Inicjalizacja czujników
  for (int i=0; i<POSITION_BUFFER; i++) { latBuffer[i]=0; lonBuffer[i]=0; }
  for (int i=0; i<TEMP_FILTER_WINDOW; i++) tempBuffer[i]=21.0;
  for (int i=0; i<ACCEL_FILTER_WINDOW; i++) accelHistory[i]=0;
  
  pinMode(RPM_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RPM_SENSOR_PIN), rpmISR, FALLING);
  lastRpmTime = millis();
  Serial.println("[RPM] Hallotron aktywny (GPIO 10)");
  
  pinMode(BATT_SENSOR_PIN, INPUT);
  Serial.println("[BATT] Pomiar baterii aktywny (GPIO 8)");
  
  // GPS (w tle)
  if (!autoDetectGPSBaud()) {
    Serial.println("[GPS] Autodetekcja nie zadziałała, używam baud z config");
    GPS_Serial.begin(config.gps_baud, SERIAL_8N1, config.gps_rx_pin, config.gps_tx_pin);
    GPS_Serial.setRxBufferSize(1024);
  }
  configureGPS();
  
  Serial.println("\n[SYSTEM] Gotowy! Serwer WWW działa, GPS w tle...\n");
  Serial.println("[INFO] Na pierwsze uruchomienie: niska moc (8.5dBm) i tryb 802.11n");
  Serial.println("[INFO] Szukaj sieci: FSR_speed | Hasło: 1234567890\n");
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
      currentAccel = calculateAcceleration(newSpeed, lastSpeed, dt);
      if (currentAccel > maxAccel && newSpeed > ACCEL_MIN_SPEED) {
        maxAccel = currentAccel;
        maxAccelLat = getMedianLat();
        maxAccelLon = getMedianLon();
      }
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
      Serial.printf("[START] Zapisano współrzędne startowe: %.6f, %.6f\n", config.start_lat, config.start_lon);
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
  
  static unsigned long lastTemp = 0;
  if (millis() - lastTemp >= 500) { lastTemp = millis(); tempEngine = readTemperature(); }
  
  static unsigned long lastRpm = 0;
  if (millis() - lastRpm >= 500) { lastRpm = millis(); currentRPM = readRPM(); }
  
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
        addTrackPoint(lat, lon, dispSpeed, tempEngine, currentRPM, currentAccel);
      }
    }
  }
  
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 5000) {
    lastDebug = millis();
    if (gps.location.isValid()) {
      Serial.printf("📍 %.6f,%.6f | ⚡ %.1f km/h | SAT:%d | VMAX:%.1f | ACCEL:%.2f m/s² (%.2f G)\n", 
        getMedianLat(), getMedianLon(), currentSpeed, gps.satellites.value(), maxSpeed, maxAccel, maxAccel * MS2_TO_G);
      Serial.printf("🌡️ %.1f C | 🔄 %.0f RPM | 🔋 %.2f V | 📦 %d pkt | Wi-Fi: %s\n", 
        tempEngine, currentRPM, batteryVoltage, trackCount, wifiStarted ? "OK" : "BRAK");
      Serial.printf("🏁 Okrążenia: %d | Najlepszy czas: %.2f s\n", config.laps, config.best_lap_time);
    } else {
      Serial.println("⏳ Czekam na GPS...");
    }
  }
  delay(10);
}