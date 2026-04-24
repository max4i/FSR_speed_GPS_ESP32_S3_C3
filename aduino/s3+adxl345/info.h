const char info_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>FSR_ACC - Informacje v0.9</title>
<style>
*{margin:0;padding:0;box-sizing:border-box;}
body{font-family:'Segoe UI',sans-serif;background:#0a0e1a;color:#fff;padding:20px;}
.container{max-width:900px;margin:0 auto;}
h1{color:#00d4ff;text-shadow:0 0 10px #00d4ff;margin-bottom:20px;text-align:center;}
h2{color:#ffcc00;margin:25px 0 15px 0;border-bottom:1px solid #333;padding-bottom:5px;}
h3{color:#00d4ff;margin:15px 0 10px 0;}
h4{color:#ffcc00;margin:10px 0 5px 0;font-size:14px;}
.card{background:rgba(0,0,0,0.7);border-radius:15px;padding:20px;margin-bottom:20px;border:1px solid rgba(0,212,255,0.3);}
table{width:100%;border-collapse:collapse;margin:10px 0;}
th,td{padding:8px;text-align:left;border-bottom:1px solid #333;}
th{color:#00d4ff;}
code{background:#1a2332;padding:2px 6px;border-radius:5px;font-family:monospace;color:#ffcc00;}
.formula{background:#1a2332;padding:15px;border-radius:10px;font-family:monospace;margin:10px 0;overflow-x:auto;}
.badge{display:inline-block;background:#10b981;color:#fff;padding:2px 8px;border-radius:10px;font-size:11px;margin-right:5px;}
.badge-orange{background:#ff7b00;color:#fff;padding:2px 8px;border-radius:10px;font-size:11px;margin-right:5px;}
.badge-blue{background:#00d4ff;color:#000;padding:2px 8px;border-radius:10px;font-size:11px;margin-right:5px;}
.warning{color:#ff7b00;}
.success{color:#10b981;}
.footer{text-align:center;margin-top:30px;padding:20px;color:#666;font-size:12px;}
.btn-back{background:#00d4ff;color:#000;border:none;padding:10px 20px;border-radius:25px;cursor:pointer;margin-top:20px;}
.btn-back:hover{opacity:0.8;}
</style>
</head>
<body>
<div class="container">
<h1>⚡ FSR SPEED ACC v0.9 ⚡</h1>
<p style="text-align:center;margin-bottom:20px;">System telemetrii dla łodzi wyścigowych FSR z zaawansowanymi filtrami</p>
<div class="card">
<h2>📡 Podłączenia</h2>
<table>
  <tr><th>Element</th><th>GPIO ESP32-S3</th><th>Uwagi</th></tr>
  <tr><td>GPS TX (z modułu)</th><td>12</th><td>podłącz do TX modułu GPS</th></tr>
  <tr><td>GPS RX (do modułu)</th><td>13</th><td>podłącz do RX modułu GPS</th></tr>
  <tr><td>ADXL345 SCL</th></table>8</th><td>I2C zegar</th></tr>
  </td><td>ADXL345 SDA</th></td>9</th><td>I2C dane</th></tr>
  <tr><td>Termistor NTC</th><td>11</th><td>przez rezystor 100kΩ do 3.3V</th></tr>
  <tr><td>Dzielnik baterii</th><td>10</th><td>2x100kΩ (środek do GPIO10)</th></tr>
</table>
<p class="warning">⚠️ Uwaga: Piny 19 i 20 są zajęte przez native USB ESP32-S3!</p>
</div>
<div class="card">
<h2>🔧 ZAAWANSOWANE FILTRY</h2>
<p>System wykorzystuje wieloetapowe przetwarzanie sygnału dla uzyskania maksymalnej dokładności pomiaru prędkości.</p>
<h3>1. Filtr antyaliasingowy Butterworth (20Hz)</h3>
<p>Filtr dolnoprzepustowy IIR rzędu 2, który odcina częstotliwości powyżej 20Hz. Eliminuje szumy wysokoczęstotliwościowe pochodzące z drgań silnika spalinowego (50-200Hz) oraz zakłóceń elektrycznych.</p>
<div class="formula">H(s) = 1 / (s² + 1.414s + 1)<br>Częstotliwość odcięcia: fc = 20Hz</div>
<h3>2. Filtr Hampela (odrzucanie outlierów)</h3>
<p>Detekcja i odrzucanie pojedynczych błędnych próbek (outlierów) – tzw. "szpilek". Dla każdej próbki obliczana jest mediana z okna 5 próbek oraz odchylenie absolutne (MAD). Próbka odbiegająca o więcej niż 3σ od mediany jest odrzucana i zastępowana medianą.</p>
<div class="formula">|xi - median| > 3 × MAD → outlier<br>gdzie: MAD = median(|xi - median|)</div>
<h3>3. Filtr Savitzky-Golay (wygładzanie)</h3>
<p>Wielomianowe wygładzanie oknem 7 próbek (stopień 2). Zachowuje kształt sygnału (szczyty prędkości nie są obcinane) przy jednoczesnej redukcji szumu. Daje lepsze wyniki niż klasyczna średnia ruchoma.</p>
<div class="formula">Współczynniki: [-2, 3, 6, 7, 6, 3, -2] / 21</div>
<h3>4. Adaptacyjna fuzja GPS/ACC (Mayonnais)</h3>
<p>Dynamiczne ważenie danych z GPS i akcelerometru ADXL345 w zależności od warunków:</p>
<ul><li>Przy małym przyspieszeniu i dobrym HDOP → <strong>90% GPS, 10% ACC</strong> (wygładzenie)</li><li>Przy dużym przyspieszeniu (zmiana prędkości) → <strong>30% GPS, 70% ACC</strong> (szybka reakcja)</li><li>Przy słabym sygnale GPS (HDOP > 1.4) → <strong>20% GPS, 80% ACC</strong></li><li>Przy braku sygnału GPS → <strong>100% ACC</strong> (krótkotrwale, do 5s)</li></ul>
<div class="formula">v_fused = α × v_gps + (1-α) × (v_acc + ∫a×dt)<br>α = dynamic (0.2 do 0.9 w zależności od warunków)</div>
<h3>5. Filtr Kalmana (adaptacyjny)</h3>
<p>Optymalny estymator stanu dla systemów liniowych z szumem. Wykorzystywany do fuzji danych z GPS i akcelerometru.</p>
<div class="formula">Predykcja: x̂ₖ = F·x̂ₖ₋₁ + B·uₖ<br>Korekcja: x̂ₖ = x̂ₖ + K·(zₖ - H·x̂ₖ)<br>Wzmocnienie Kalmana: K = Pₖ·Hᵀ·(H·Pₖ·Hᵀ + R)⁻¹</div>
<p><span class="badge-blue">Adaptacyjność</span> Macierz kowariancji szumu pomiarowego R jest dynamicznie zmieniana w zależności od HDOP i liczby satelitów. Im gorszy sygnał GPS, tym większe R (większe zaufanie do akcelerometru).</p>
<h3>6. Mediana końcowa (10 próbek)</h3>
<p>Ostateczne wygładzenie danych przed wyświetleniem. Mediana z 10 ostatnich próbek eliminuje pojedyncze zakłócenia, które mogły przejść przez poprzednie filtry.</p>
</div>
<div class="card">
<h2>📐 Obliczanie dokładności prędkości (Dok:)</h2>
<h3>Podstawowa dokładność GPS</h3>
<p>Moduł GPS AT6558R (GPS+BDS Beidou) ma deklarowaną dokładność pomiaru prędkości wynoszącą <strong>0,1 m/s = 0,36 km/h</strong>.</p>
<h3>Wpływ liczby satelitów (SAT)</h3>
<p>Więcej satelitów = lepsza geometria i wyższa dokładność. Współczynnik korygujący:</p>
<div class="formula">K_sat = 1 - (SAT - 4) / 30<br>Ograniczenie: 0,3 ≤ K_sat ≤ 1,0</div>
<h3>Wpływ HDOP (Horizontal Dilution of Precision)</h3>
<p>HDOP określa geometrię satelitów – im niższa wartość, tym lepsza dokładność.</p>
<div class="formula">K_hdop = HDOP / 1,0<br>Ograniczenie: 0,5 ≤ K_hdop ≤ 2,0</div>
<h3>Wpływ filtra medianowego (3 próbki)</h3>
<p>Filtr medianowy redukuje błędy losowe. Redukcja błędu jest proporcjonalna do pierwiastka z liczby próbek.</p>
<div class="formula">K_filtr = 1 / √n = 1 / √3 = 0,577</div>
<h3>Wzór końcowy na dokładność statyczną</h3>
<div class="formula">Dokładność [km/h] = 0,36 × K_sat × K_hdop × K_filtr</div>
<h3>Przykłady obliczeń:</h3>
</table>
  <tr><th>SAT</th><th>HDOP</th><th>K_sat</th><th>K_hdop</th><th>Dokładność [km/h]</th><th>Ocena</th></tr>
  <tr><td>26</th><td>0,5</th><td>0,30</th><td>0,50</th><td>0,36×0,30×0,50×0,577 = <strong>0,031</strong></th><td>doskonała</th></tr>
  <tr><td>20</th><td>0,7</th><td>0,47</th><td>0,70</th><td>0,36×0,47×0,70×0,577 = <strong>0,068</strong></th><td>bardzo dobra</th></tr>
  <tr><td>12</th><td>1,0</th><td>0,73</th><td>1,00</th><td>0,36×0,73×1,00×0,577 = <strong>0,152</strong></th><td>dobra</th></tr>
  <tr><td>8</th><td>1,3</th><td>0,87</th><td>1,30</th><td>0,36×0,87×1,30×0,577 = <strong>0,235</strong></th><td>przeciętna</th></tr>
</table>
<h3>Dokładność dynamiczna (przy zmianach prędkości)</h3>
<p>Dzięki fuzji z akcelerometrem ADXL345 i filtrowi Kalmana, błąd dynamiczny jest redukowany. Wartość szacowana na podstawie:</p>
<div class="formula">Dokładność_dyn [km/h] = 0,36 × K_sat × K_hdop × K_filtr × 2,5</div>
</div>
<div class="card">
<h2>🎯 Jak czytać dokładność (Dok:)</h2>
<p>Wartość w oknie <strong>Dok: ±X.XXX km/h</strong> to szacowany błąd pomiaru prędkości chwilowej.</p>
<ul><li><span class="badge">0,03 - 0,05</span> Doskonała</li><li><span class="badge">0,05 - 0,10</span> Bardzo dobra</li><li><span class="badge">0,10 - 0,20</span> Dobra</li><li><span class="badge">0,20 - 0,50</span> Przeciętna</li><li><span class="badge">0,50 - 1,00</span> Słaba (sprawdź antenę GPS)</li></ul>
</div>
<div class="card">
<h2>📊 Dokładność pomiarów w praktyce</h2>
表
  <tr><th>Warunki</th><th>SAT</th><th>HDOP</th><th>Dokładność statyczna</th><th>Dokładność dynamiczna</th><th>Błąd dystansu (40km)</th></tr>
  <tr><td>Optymalne (otwarta woda)</th><td>≥20</th><td>0,3-0,6</th><td>±0,03-0,07 km/h</th><td>±0,08-0,18 km/h</th><td>±12-28 m</th></tr>
  <tr><td>Dobre (małe fale)</th><td>10-19</th><td>0,6-1,0</th><td>±0,07-0,15 km/h</th><td>±0,18-0,38 km/h</th><td>±28-60 m</th></tr>
  <tr><td>Przeciętne (zasłonięcie)</th><td>6-9</th><td>1,0-1,4</th><td>±0,15-0,24 km/h</th><td>±0,38-0,60 km/h</th><td>±60-96 m</th></tr>
</table>
</div>
<div class="card">
<h2>🔧 Ustawienia w Arduino IDE</h2>
表
  <tr><th>Ustawienie</th><th>Wartość</th></tr>
  <tr><td>Board</th><td>ESP32S3 Dev Module</th></tr>
  <tr><td>USB Mode</th><td>CDC (Native USB)</th></tr>
  <tr><td>USB CDC On Boot</th><td>Enabled</th></tr>
  </tr><td>CPU Frequency</th><td>240 MHz</th></tr>
  <tr><td>Flash Mode</th><td>QIO</th></tr>
  <tr><td>Flash Size</th><td>4MB (32Mb)</th></tr>
  <tr><td>Flash Frequency</th><td>80 MHz</th></tr>
  <tr><td>Partition Scheme</th><td>No OTA (2MB APP/2MB SPIFFS)</th></tr>
  <tr><td>PSRAM</th><td>QSPI PSRAM</th></tr>
</table>
</div>
<div class="card">
<h2>⚠️ Rozwiązywanie problemów</h2>
表
  <tr><th>Problem</th><th>Rozwiązanie</th></tr>
  <tr><td>Brak sieci Wi-Fi</th><td>Naciśnij RESET, sprawdź czy wybrano ESP32S3 Dev Module</th></tr>
  <tr><td>GPS nie ma FIX</th><td>Zapewnij widok na niebo, sprawdź antenę</th></tr>
  <tr><td>Temperatura 0°C</th><tr>Włącz czujnik w ustawieniach, sprawdź połączenia</th></tr>
  <tr><td>VMAX nie zapisuje</th><td>HDOP > 1,4 lub SAT < 6 – popraw sygnał GPS</th></td>
  <tr><td>ADXL345 nie działa</th><td>Sprawdź I2C: SCL->GPIO8, SDA->GPIO9</th></tr>
  <tr><td>Bateria pokazuje 0V</th><td>Sprawdź dzielnik 2x100kΩ na GPIO10</th></tr>
</table>
</div>
<div class="footer">
<button class="btn-back" onclick="window.location.href='/'">← POWRÓT DO MAPY</button>
<p>FSR SPEED ACC v0.9 | Autorzy: Maxiii & Deepseek | 2026</p>
</div>
</div>
</body>
</html>
)rawliteral";