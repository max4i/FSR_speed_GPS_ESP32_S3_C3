# FSR Speed Tracker v9.5

## Telemetria dla łodzi wyścigowych FSR

## Spis treści

1. [Opis ogólny](#opis-ogólny)
2. [Wersja ESP32-S3 (pełna)](#wersja-esp32-s3-pełna)
3. [Wersja ESP32-C3 (lekka)](#wersja-esp32-c3-lekka)
4. [Porównanie wersji](#porównanie-wersji)
5. [Budowa i podłączenia](#budowa-i-podłączenia)
6. [Instalacja oprogramowania](#instalacja-oprogramowania)
7. [Konfiguracja](#konfiguracja)
8. [Obsługa systemu](#obsługa-systemu)
9. [Funkcje zaawansowane](#funkcje-zaawansowane)
10. [Rozwiązywanie problemów](#rozwiązywanie-problemów)
11. [Dane techniczne](#dane-techniczne)

---

## Opis ogólny

System telemetrii dla łodzi wyścigowych **FSR** (Formuła Silnikowa Remote control) umożliwiający śledzenie parametrów pracy jednostki w czasie rzeczywistym. Urządzenie tworzy własną sieć Wi-Fi, do której można podłączyć telefon, tablet lub laptop, a następnie w przeglądarce internetowej obserwować:

- Pozycję łodzi na mapie satelitarnej
- Prędkość chwilową i maksymalną (VMAX)
- Przyspieszenie w jednostkach **G**
- Temperaturę silnika
- Obroty silnika (RPM)
- Napięcie baterii pokładowej
- Liczbę okrążeń z pomiarem czasu
- Przebyty dystans
- Trasę do zapisu w formacie **GPX**

System dostępny jest w dwóch wersjach sprzętowych.

---

## Wersja ESP32-S3 (pełna)

Pełna wersja oparta na **ESP32-S3 SuperMini** z PSRAM 2MB.

| Parametr | Wartość |
|----------|---------|
| Mikrokontroler | ESP32-S3 SuperMini (QFN56) |
| Pamięć Flash | 4 MB |
| Pamięć PSRAM | 2 MB (QSPI) |
| CPU | 240 MHz |
| Moduł GPS | HT1818Z3G5L z AT6558R (GPS+BDS, 10Hz) |
| Czujnik temp. | NTC 100kΩ (Beta=3950) |
| Czujnik obrotów | Hallotron 3144 |
| Pomiar napięcia | Dzielnik 2×100kΩ |
| Wi-Fi | 802.11b/g/n, kanał 1, moc 8.5–19.5 dBm |
| Zasięg Wi-Fi | do 300 m |
| Zasilanie | 5V lub 7.4V LiPo |
| Pobór prądu | ~350 mA |

**Funkcje:** wszystko (wykresy, regulacja mocy, zapis 6000 punktów).

---

## Wersja ESP32-C3 (lekka)

Uproszczona wersja na **ESP32-C3 SuperMini** (RISC-V).

| Parametr | Wartość |
|----------|---------|
| Mikrokontroler | ESP32-C3 SuperMini |
| Pamięć Flash | 4 MB |
| Pamięć RAM | 400 KB |
| CPU | 160 MHz |
| Moduł GPS | HT1818Z3G5L (10Hz) |
| Czujnik temp. | NTC 100kΩ (opcjonalnie) |
| Czujnik obrotów | brak (opcjonalnie) |
| Pomiar napięcia | brak |
| Wi-Fi | 802.11b/g/n, moc stała |
| Zasięg Wi-Fi | do 150 m |
| Zasilanie | 5V |
| Pobór prądu | ~200 mA |

**Funkcje:** podstawowe (prędkość, trasa, okrążenia, GPX). Brak wykresów i regulacji mocy.

---

## Porównanie wersji

| Cecha | ESP32-S3 | ESP32-C3 |
|-------|----------|----------|
| Cena zestawu | ~93 zł | ~50 zł |
| Pobór prądu | 350 mA | 200 mA |
| Zasięg Wi-Fi | do 300 m | do 150 m |
| Bufor trasy | 6000 pkt | 1000 pkt |
| Pamięć PSRAM | 2 MB | brak |
| Wykresy w UI | tak | nie |
| Regulacja mocy Wi-Fi | tak | nie |
| Pomiar temperatury | tak | opcjonalnie |
| Pomiar obrotów | tak | nie |
| Pomiar baterii | tak | nie |
| Czas zapisu trasy | ~60 min | ~10 min |

---

## Budowa i podłączenia

### Wspólne elementy

| Element | Model |
|---------|-------|
| ESP | ESP32-S3 lub ESP32-C3 SuperMini |
| GPS | HT1818Z3G5L z AT6558R |
| Antena GPS | GP 5/8 |
| Kondensator | 1000µF/6.3V |

### Podłączenia dla ESP32-S3

| Element | GPIO |
|---------|------|
| GPS TX → ESP RX | 12 |
| GPS RX ← ESP TX | 13 |
| Termistor NTC | 11 |
| Hallotron 3144 | 10 |
| Dzielnik baterii | 8 |

### Podłączenia dla ESP32-C3

| Element | GPIO |
|---------|------|
| GPS TX → ESP RX | 5 |
| GPS RX ← ESP TX | 6 |
| Termistor NTC | 2 (opcjonalnie) |
| Hallotron | 3 (opcjonalnie) |

### Schemat (ESP32-S3)
