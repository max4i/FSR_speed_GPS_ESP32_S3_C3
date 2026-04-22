const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
<title>FSR_ACC v0.8</title>
<link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css" />
<script src="https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/leaflet.js"></script>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
<style>
*{margin:0;padding:0;box-sizing:border-box;}
body{font-family:'Segoe UI',sans-serif;background:#0a0e1a;}
#map{height:100vh;width:100%;background:#0a0e1a;}

/* Napis tytułowy */
.neon-title {
    position: absolute;
    top: 15px;
    left: 50%;
    transform: translateX(-50%);
    z-index: 2000;
    font-size: 14px;
    font-weight: bold;
    color: #ffcc00;
    text-shadow: 0 0 5px #ffcc00, 0 0 10px #ffcc00;
    background: rgba(0,0,0,0.5);
    padding: 6px 12px;
    border-radius: 20px;
    backdrop-filter: blur(4px);
    white-space: nowrap;
    border: 1px solid rgba(255,204,0,0.3);
    pointer-events: none;
}

/* Wszystkie okna mają tę samą przezroczystość */
.data-panel, .laps-panel, .vmax-history, .panel {
    background: rgba(0,0,0,0.35);
    backdrop-filter: blur(8px);
}

/* Panel SAT/HDOP/Dokładność */
.data-panel {
    position: absolute;
    top: 80px;
    right: 20px;
    border-radius: 12px;
    padding: 10px 15px;
    text-align: right;
    font-family: 'Courier New', monospace;
    z-index: 2000;
    border: 1px solid rgba(255,255,255,0.2);
    min-width: 160px;
}
.data-line {
    color: #ffcc00;
    font-size: 13px;
    font-weight: bold;
    margin-bottom: 4px;
}

/* Panel okrążeń */
.laps-panel {
    position: absolute;
    top: 160px;
    right: 20px;
    border-radius: 12px;
    padding: 10px 15px;
    text-align: right;
    font-family: 'Courier New', monospace;
    z-index: 2000;
    border: 1px solid rgba(255,255,255,0.2);
    min-width: 180px;
}
.lap-number {
    color: #ff3333;
    font-size: 18px;
    font-weight: bold;
    text-shadow: 0 0 5px #ff0000;
}
.lap-time { color: #ff9999; font-size: 13px; }
.best-lap { color: #ffcc00; font-size: 11px; margin-top: 5px; }
.lap-record { color: #ffcc00; font-size: 11px; margin-top: 3px; }

/* Panel VMAX historii – tylko 4 unikalne wartości */
.vmax-history {
    position: absolute;
    top: 300px;
    right: 20px;
    border-radius: 12px;
    padding: 10px 15px;
    text-align: right;
    font-family: 'Courier New', monospace;
    z-index: 2000;
    border: 1px solid rgba(255,255,255,0.2);
    max-height: 180px;
    overflow-y: auto;
    min-width: 140px;
}
.vmax-title { color: #ffcc00; font-size: 11px; margin-bottom: 5px; opacity: 0.8; }
.vmax-item { color: #ff3333; font-size: 12px; font-weight: bold; margin-bottom: 3px; }

.leaflet-control-zoom {
    position: absolute;
    right: 20px !important;
    left: auto !important;
    top: 520px !important;
    z-index: 1000;
}

.charts-container {
    position: absolute;
    top: 20px;
    left: 20px;
    width: 280px;
    z-index: 1500;
    display: flex;
    flex-direction: column;
    gap: 15px;
}
.chart-card {
    background: rgba(0,0,0,0.75);
    backdrop-filter: blur(8px);
    border-radius: 16px;
    padding: 10px;
    border: 1px solid rgba(255,204,0,0.4);
}
.chart-card h4 { color: #ffcc00; font-size: 12px; text-align: center; margin-bottom: 5px; }
canvas { width: 100% !important; height: auto !important; max-height: 140px; }

/* Panel dolny */
.panel {
    position: absolute;
    bottom: 15px;
    left: 15px;
    right: 15px;
    border-radius: 25px;
    padding: 10px 15px;
    z-index: 1000;
    border: 2px solid rgba(255,204,0,0.5);
}
.speed-row { display: flex; gap: 12px; margin-bottom: 12px; }
.speed-card { flex: 1; text-align: center; background: rgba(26,35,50,0.8); border-radius: 20px; padding: 10px; }
.speed-value { font-size: 3rem; font-weight: 900; }
.speed-label { font-size: 0.65rem; color: #9ca3af; margin-top: 5px; }
.current { color: #00d4ff; text-shadow: 0 0 8px #00d4ff; }
.max { color: #ff7b00; text-shadow: 0 0 8px #ff7b00; }

/* Rząd danych - 4 karty (dystans, bateria, temperatura, drgania) */
.data-row {
    display: flex;
    gap: 12px;
    margin-bottom: 12px;
    justify-content: center;
}
.data-card {
    flex: 1;
    text-align: center;
    background: rgba(26,35,50,0.8);
    border-radius: 15px;
    padding: 8px;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
}
.data-value { font-size: 1.1rem; font-weight: bold; color: #ffcc00; white-space: nowrap; }
.data-label { font-size: 0.55rem; color: #9ca3af; margin-top: 2px; }
.vibration-good { color: #10b981; }
.vibration-warning { color: #ffcc00; }
.vibration-danger { color: #ff3333; }

.buttons { display: flex; gap: 8px; justify-content: center; flex-wrap: wrap; }
.btn { padding: 8px 16px; border-radius: 30px; border: none; font-weight: bold; font-size: 0.75rem; cursor: pointer; }
.btn:active { transform: scale(0.96); }
.btn-center { background: #00d4ff; color: #000; }
.btn-startline { background: #ff3333; color: #fff; }
.btn-reset { background: #ff7b00; color: #000; }
.btn-track { background: #10b981; color: #000; }
.btn-save { background: #8b5cf6; color: #000; }
.btn-config { background: #6b7280; color: #fff; }

.modal {
    display: none;
    position: fixed;
    top: 0; left: 0;
    width: 100%; height: 100%;
    background: rgba(0,0,0,0.8);
    z-index: 3000;
    justify-content: center;
    align-items: center;
}
.modal-content {
    background: #1a2332;
    border-radius: 25px;
    padding: 20px;
    width: 90%;
    max-width: 500px;
    border: 2px solid #ffcc00;
    color: #fff;
    max-height: 80vh;
    overflow-y: auto;
}
.modal-content h3 { margin-bottom: 15px; text-align: center; color: #ffcc00; }
.modal-content label { display: block; margin: 10px 0 5px; }
.modal-content input, .modal-content select {
    width: 100%;
    padding: 8px;
    border-radius: 10px;
    border: none;
    background: #0f1722;
    color: #fff;
}
.modal-content button { margin-top: 15px; width: 100%; }
.close { float: right; font-size: 28px; cursor: pointer; color: #ffcc00; }
.scan-btn { background: #ffcc00; color: #000; border: none; padding: 5px 10px; border-radius: 10px; cursor: pointer; margin-top: 5px; }
.network-list { max-height: 150px; overflow-y: auto; margin-top: 5px; }
.network-item { padding: 5px; background: #0f1722; margin: 2px; border-radius: 5px; cursor: pointer; }
.network-item:hover { background: #ffcc00; color: #000; }

/* Wersja mobilna */
@media (max-width: 768px) {
    .neon-title { font-size: 9px; top: 8px; }
    .data-panel { top: 60px; right: 10px; padding: 6px 10px; min-width: 120px; }
    .data-line { font-size: 10px; }
    .laps-panel { top: 140px; right: 10px; padding: 6px 10px; min-width: 150px; }
    .lap-number { font-size: 14px; }
    .vmax-history { top: 270px; right: 10px; padding: 6px 10px; max-height: 130px; }
    .leaflet-control-zoom { top: 460px !important; }
    .charts-container { display: none !important; }
    .panel { padding: 8px; }
    .speed-value { font-size: 2rem !important; }
    .data-value { font-size: 0.8rem; }
    .data-label { font-size: 0.5rem; }
    .btn { padding: 6px 10px; font-size: 0.65rem; }
}
</style>
</head>
<body>
<div id="map"></div>

<div class="neon-title" id="neonTitle">⚡ FSR SPEED ACC by MAXIII v0.9 ⚡</div>

<div class="data-panel" id="dataPanel">
    <div class="data-line" id="satLine">SAT: --</div>
    <div class="data-line" id="hdopLine">HDOP: --</div>
    <div class="data-line" id="accuracyLine">Dok: ±-- km/h</div>
</div>

<div class="laps-panel" id="lapsPanel">
    <div class="lap-number">Pętla: 0</div>
    <div class="lap-time">Czas: --</div>
    <div class="best-lap">🏆 Rekord: --</div>
    <div class="lap-record">⚡ Najszybsza pętla: --</div>
</div>

<div class="vmax-history" id="vmaxHistory">
    <div class="vmax-title">🏆 VMAX</div>
</div>

<div class="charts-container" id="chartsContainer">
    <div class="chart-card"><h4>Prędkość (km/h)</h4><canvas id="speedChart"></canvas></div>
</div>

<div class="panel">
<div class="speed-row">
<div class="speed-card"><div class="speed-value current" id="speed">0.0</div><div class="speed-label">PRĘDKOŚĆ</div></div>
<div class="speed-card"><div class="speed-value max" id="maxspeed">0.0</div><div class="speed-label">VMAX</div></div>
</div>

<div class="data-row">
<div class="data-card"><div class="data-value" id="distance">0.00 km</div><div class="data-label">DYSTANS</div></div>
<div class="data-card"><div class="data-value" id="battery">0.0 V</div><div class="data-label">BATERIA</div></div>
<div class="data-card" id="tempCard"><div class="data-value" id="tempEngine">0°C</div><div class="data-label">TEMPERATURA</div></div>
<div class="data-card"><div class="data-value" id="vibrationValue">0.00</div><div class="data-label">DRGANIA m/s²</div></div>
</div>

<div class="buttons">
<button class="btn btn-center" onclick="centerMap()">🎯 CENTRUJ</button>
<button class="btn btn-startline" onclick="setStartLine()">🏁 USTAW START</button>
<button class="btn btn-reset" onclick="resetMax()">🏆 RESET VMAX</button>
<button class="btn btn-reset" onclick="resetVMAXHistory()">📋 RESET HISTORII</button>
<button class="btn btn-track" onclick="resetTrack()">🗑️ RESET TRASY</button>
<button class="btn btn-save" onclick="saveTrack()">💾 ZAPISZ TRASĘ</button>
<button class="btn btn-config" onclick="openConfig()">⚙️ USTAWIENIA</button>
</div>
</div>

<div id="configModal" class="modal">
<div class="modal-content">
<span class="close" onclick="closeConfig()">&times;</span>
<h3>⚙️ USTAWIENIA SYSTEMU</h3>

<h3 style="font-size:14px; margin-top:10px;">🌡️ CZUJNIK TEMPERATURY</h3>
<label>Offset temperatury</label>
<input type="number" step="0.5" id="temp_offset">
<label>Skala temperatury</label>
<input type="number" step="0.01" id="temp_scale">
<label><input type="checkbox" id="temp_enabled"> Włącz czujnik temperatury</label>

<h3 style="font-size:14px; margin-top:15px;">📡 WI-FI</h3>
<label>Sieć do połączenia (STA)</label>
<input type="text" id="sta_ssid" readonly>
<button class="scan-btn" onclick="scanWiFi()">🔍 SZUKAJ SIECI</button>
<div id="networkList" class="network-list"></div>
<label>Hasło sieci</label>
<input type="text" id="sta_password">
<label>Nazwa AP (gdy brak sieci)</label>
<input type="text" id="wifi_ssid">
<label>Hasło AP</label>
<input type="text" id="wifi_password">
<label>Moc Wi-Fi (tryb AP)</label>
<select id="wifi_power">
    <option value="0">8.5 dBm - Niska</option>
    <option value="1">11 dBm - Średnia</option>
    <option value="2">13 dBm - Wysoka</option>
    <option value="3">17 dBm - Bardzo wysoka</option>
    <option value="4">19 dBm - Maksymalna</option>
    <option value="5">19.5 dBm - Pełna moc</option>
</select>
<label>Tryb Wi-Fi (AP)</label>
<select id="wifi_mode">
    <option value="0">802.11b - Max zasięg</option>
    <option value="1">802.11g - Kompromis</option>
    <option value="2">802.11n - Max prędkość</option>
</select>

<h3 style="font-size:14px; margin-top:15px;">🔧 FILTRY ANTYWIBRACYJNE</h3>
<label>Współczynnik LPF (kąt pochylenia)</label>
<select id="lpf_alpha">
    <option value="0.98">0.98 - Bardzo wolna reakcja (silne drgania)</option>
    <option value="0.95">0.95 - Standard (zalecane)</option>
    <option value="0.90">0.90 - Szybsza reakcja</option>
    <option value="0.85">0.85 - Minimalne tłumienie</option>
</select>

<h3 style="font-size:14px; margin-top:15px;">📊 EKSPORT DANYCH</h3>
<button class="btn btn-save" onclick="exportVmaxCSV()">📊 EKSPORT VMAX DO CSV</button>

<h3 style="font-size:14px; margin-top:15px;">🔧 KALIBRACJA</h3>
<button class="btn btn-save" onclick="calibrateADXL()">🔧 KALIBRUJ CZUJNIK ADXL345</button>
<p style="font-size:10px; color:#999; margin-top:5px;">⚠️ Kalibracja tylko przy wyłączonym silniku i nieruchomej łodzi! Wykonywana jednorazowo.</p>

<h3 style="font-size:14px; margin-top:15px;">ℹ️ INFORMACJE</h3>
<button class="btn btn-info" onclick="window.location.href='/info'" style="background:#00d4ff; color:#000;">📖 STRONA INFORMACYJNA</button>

<button class="btn btn-save" onclick="saveConfig()" style="margin-top:20px; background:#ffcc00; color:#000;">💾 ZAPISZ I RESTARTUJ</button>
</div>
</div>

<script>
let map = null, marker = null, trackLine = null, trackPoints = [], startLineMarker = null;
let speedChart = null;
let speedData = [];
let firstFixReceived = false;
let tempEnabled = true;
let isMobile = /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(navigator.userAgent) || window.innerWidth <= 768;

function initMap() {
    if(map !== null) return true;
    if(typeof L === 'undefined') return false;
    map = L.map('map', { zoomControl: true, zoomSnap: 0.5 }).setView([52.0, 15.0], 20);
    L.tileLayer('https://mt1.google.com/vt/lyrs=s,h&x={x}&y={y}&z={z}', {
        attribution: '&copy; Google Maps | FSR Speed Tracker',
        maxZoom: 20, subdomains: ['mt0', 'mt1', 'mt2', 'mt3']
    }).addTo(map);
    return true;
}

function initCharts() {
    if(isMobile) return;
    const speedCtx = document.getElementById('speedChart').getContext('2d');
    speedChart = new Chart(speedCtx, {
        type: 'line',
        data: { labels: [], datasets: [{ label: 'km/h', data: [], borderColor: '#ffcc00', borderWidth: 2, pointRadius: 0, fill: true, backgroundColor: 'rgba(255,204,0,0.1)' }] },
        options: { responsive: true, maintainAspectRatio: true, plugins: { legend: { labels: { color: '#ffcc00', font: { size: 9 } } } }, scales: { y: { beginAtZero: true, grid: { color: '#2d3a4e' } } } }
    });
}

function updateCharts(speed) {
    if(isMobile) return;
    speedData.push(speed);
    if(speedData.length > 60) speedData.shift();
    if(speedChart) {
        speedChart.data.labels = Array.from({length: speedData.length}, (_,i) => i+1);
        speedChart.data.datasets[0].data = [...speedData];
        speedChart.update('none');
    }
}

// ==================== POPRAWA: VMAX – tylko 4 ostatnie unikalne wartości ====================
function updateVMAXHistory(vmaxList) {
    const container = document.getElementById('vmaxHistory');
    if(!container) return;
    
    // 1. Usuń duplikaty, zachowując kolejność (ostatnia wartość to najnowsza)
    const unique = [];
    for(let i = vmaxList.length - 1; i >= 0; i--) {
        const val = vmaxList[i];
        if(val > 0 && !unique.includes(val)) {
            unique.unshift(val); // wstaw na początek, aby zachować chronologię
        }
    }
    // 2. Weź ostatnie 4 unikalne wartości (czyli 4 najnowsze)
    const last4 = unique.slice(-4);
    
    let html = '<div class="vmax-title">🏆 VMAX</div>';
    for(let i = 0; i < last4.length; i++) {
        html += `<div class="vmax-item">#${i+1} ${last4[i].toFixed(2)} km/h</div>`;
    }
    container.innerHTML = html;
}
// =====================================================================

function updateLapsPanel(laps, currentTime, bestTime, bestLapNumber) {
    const panel = document.getElementById('lapsPanel');
    if(!panel) return;
    let html = `<div class="lap-number">Pętla: ${laps}</div>`;
    html += `<div class="lap-time">Czas: ${formatTime(currentTime)}</div>`;
    if (bestTime > 0) html += `<div class="best-lap">🏆 Rekord: ${formatTime(bestTime)}</div>`;
    if (bestLapNumber > 0 && bestTime > 0) html += `<div class="lap-record">⚡ Najszybsza pętla: ${bestLapNumber}/${formatTime(bestTime)}</div>`;
    panel.innerHTML = html;
}

function updateDataPanel(sat, hdop, accuracy, fixQuality) {
    document.getElementById('satLine').innerHTML = `SAT: ${sat}`;
    if (hdop > 0 && hdop < 99) {
        document.getElementById('hdopLine').innerHTML = `HDOP: ${hdop.toFixed(1)}`;
    } else {
        document.getElementById('hdopLine').innerHTML = `HDOP: --`;
    }
    document.getElementById('accuracyLine').innerHTML = `Dok: ±${accuracy.toFixed(3)} km/h`;
}

function updateVibrationPanel() {
    fetch('/api/vibration').then(r=>r.json()).then(d=>{
        const vibElem = document.getElementById('vibrationValue');
        vibElem.innerText = d.vibration_level.toFixed(2);
        vibElem.className = d.vibration_level > 8 ? 'vibration-danger' : (d.vibration_level > 3 ? 'vibration-warning' : 'vibration-good');
    }).catch(e=>console.log(e));
}

function formatTime(sec) {
    if(sec <= 0) return '00:00.0';
    let mins = Math.floor(sec / 60);
    let secs = (sec % 60).toFixed(1);
    return `${mins.toString().padStart(2,'0')}:${secs.toString().padStart(4,'0')}`;
}

function centerMap() {
    if(!map) return;
    fetch('/api/data').then(r=>r.json()).then(d=>{ if(d.fix==1 && d.lat) map.setView([d.lat, d.lon], 20); });
}

function setStartLine() {
    fetch('/api/data').then(r=>r.json()).then(d=>{
        if(d.fix==1 && d.lat && d.lat != 0) {
            const latA = d.lat;
            const lonA = d.lon;
            const latB = latA + 0.00009;
            const lonB = lonA;
            fetch('/api/setstartline', { method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({latA: latA, lonA: lonA, latB: latB, lonB: lonB, radius: 5}) })
                .then(() => {
                    alert('🏁 Linia start/meta ustawiona!');
                    if(startLineMarker) startLineMarker.remove();
                    const linePoints = [[latA, lonA], [latB, lonB]];
                    startLineMarker = L.polyline(linePoints, {color: '#ff3333', weight: 4, opacity: 0.8}).addTo(map);
                });
        } else alert('Brak sygnału GPS!');
    });
}

function resetMax() { fetch('/api/resetmax', {method:'POST'}); }
function resetTrack() { trackPoints = []; if(trackLine) trackLine.remove(); trackLine = null; fetch('/api/resettrack', {method:'POST'}); }
function resetVMAXHistory() { fetch('/api/resetvmaxhistory', {method:'POST'}); }
function saveTrack() { window.location.href = '/api/save'; }
function exportVmaxCSV() { window.location.href = '/api/vmaxcsv'; }
function calibrateADXL() { if(confirm('⚠️ UWAGA! Wyłącz silnik! Łódź musi stać nieruchomo na płaskiej wodzie. Czy kontynuować kalibrację?')) fetch('/api/calibrate', {method:'POST'}).then(r=>r.text()).then(alert); }

function scanWiFi() {
    const listDiv = document.getElementById('networkList');
    listDiv.innerHTML = '<div>Skanowanie...</div>';
    fetch('/api/scanwifi').then(r=>r.json()).then(networks => {
        if(networks.status === 'scanning') { setTimeout(scanWiFi, 2000); return; }
        let html = '';
        for(let ssid of networks) { if(ssid) html += `<div class="network-item" onclick="selectNetwork('${ssid.replace(/'/g, "\\'")}')">${ssid}</div>`; }
        if(html === '') html = '<div>Brak sieci</div>';
        listDiv.innerHTML = html;
    });
}
function selectNetwork(ssid) { document.getElementById('sta_ssid').value = ssid; document.getElementById('networkList').innerHTML = ''; }

async function openConfig() {
    const res = await fetch('/api/config');
    const cfg = await res.json();
    document.getElementById('temp_offset').value = cfg.temp_offset;
    document.getElementById('temp_scale').value = cfg.temp_scale;
    document.getElementById('temp_enabled').checked = cfg.temp_enabled;
    document.getElementById('wifi_ssid').value = cfg.wifi_ssid;
    document.getElementById('wifi_password').value = cfg.wifi_password;
    document.getElementById('sta_ssid').value = cfg.sta_ssid || '';
    document.getElementById('sta_password').value = cfg.sta_password || '';
    document.getElementById('wifi_power').value = cfg.wifi_power || 0;
    document.getElementById('wifi_mode').value = cfg.wifi_mode || 2;
    document.getElementById('lpf_alpha').value = cfg.lpf_alpha || 0.95;
    document.getElementById('configModal').style.display = 'flex';
}
function closeConfig() { document.getElementById('configModal').style.display = 'none'; }
async function saveConfig() {
    const payload = {
        temp_offset: parseFloat(document.getElementById('temp_offset').value),
        temp_scale: parseFloat(document.getElementById('temp_scale').value),
        temp_enabled: document.getElementById('temp_enabled').checked,
        wifi_ssid: document.getElementById('wifi_ssid').value,
        wifi_password: document.getElementById('wifi_password').value,
        sta_ssid: document.getElementById('sta_ssid').value,
        sta_password: document.getElementById('sta_password').value,
        wifi_power: parseInt(document.getElementById('wifi_power').value),
        wifi_mode: parseInt(document.getElementById('wifi_mode').value),
        lpf_alpha: parseFloat(document.getElementById('lpf_alpha').value)
    };
    const res = await fetch('/api/config', { method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(payload) });
    alert(await res.text());
    closeConfig();
}

async function loadTrack() {
    try {
        const res = await fetch('/api/track');
        const data = await res.json();
        if (data.track && data.track.length) {
            trackPoints = data.track.map(p => [p.lat, p.lon]);
            if (trackLine) trackLine.remove();
            trackLine = L.polyline(trackPoints, {color: '#ffcc00', weight: 3, opacity: 0.9}).addTo(map);
            if (trackPoints.length > 0) map.fitBounds(L.latLngBounds(trackPoints));
        }
        if (data.vmax && data.vmax.length) {
            data.vmax.forEach(v => {
                const numberIcon = L.divIcon({ html: `<div style="background: #ff3333; color: white; border-radius: 50%; width: 24px; height: 24px; display: flex; align-items: center; justify-content: center; font-weight: bold; border: 2px solid #ffcc00;">${v.num}</div>`, iconSize: [24, 24], className: 'vmax-marker' });
                L.marker([v.lat, v.lon], {icon: numberIcon}).bindTooltip(`VMAX ${v.speed.toFixed(2)} km/h`).addTo(map);
            });
        }
        updateLapsPanel(data.laps || 0, 0, data.best_lap_time || 0, data.best_lap_number || 0);
        if (data.start_line_A_lat && data.start_line_A_lon && data.start_line_B_lat && data.start_line_B_lon && data.start_line_A_lat != 0 && !startLineMarker) {
            const linePoints = [[data.start_line_A_lat, data.start_line_A_lon], [data.start_line_B_lat, data.start_line_B_lon]];
            startLineMarker = L.polyline(linePoints, {color: '#ff3333', weight: 4, opacity: 0.8}).addTo(map);
        }
    } catch(e) { console.log("loadTrack error", e); }
}

function fetchData() {
    fetch('/api/data').then(r=>r.json()).then(d=>{
        document.getElementById('speed').innerText = d.speed.toFixed(2);
        document.getElementById('maxspeed').innerText = d.max_speed.toFixed(2);
        document.getElementById('distance').innerHTML = d.distance.toFixed(2) + ' km';
        document.getElementById('battery').innerHTML = d.battery.toFixed(1) + ' V';
        tempEnabled = d.temp_enabled;
        const tempCard = document.getElementById('tempCard');
        if(tempEnabled && d.temp_engine > 0) { tempCard.style.display = 'flex'; document.getElementById('tempEngine').innerHTML = d.temp_engine.toFixed(0) + '°C'; }
        else { tempCard.style.display = 'none'; }
        updateLapsPanel(d.laps || 0, d.current_lap_time || 0, d.best_lap_time || 0, d.best_lap_number || 0);
        updateCharts(d.speed);
        updateDataPanel(d.sat || 0, d.hdop || 0, d.accuracy || 0.1, d.fix_quality || 0);
        let vmaxList = [];
        for(let i = 0; i < (d.vmax_count || 0); i++) { if(d['vmax_' + i]) vmaxList.push(d['vmax_' + i]); }
        updateVMAXHistory(vmaxList);
        if(map && d.fix==1 && d.lat) {
            if(!firstFixReceived && d.lat) { map.setView([d.lat, d.lon], 20); firstFixReceived = true; }
            if(marker) marker.remove();
            marker = L.circleMarker([d.lat, d.lon], { radius:6, fillColor:'#ffcc00', color:'#fff', weight:2, fillOpacity:0.9 }).addTo(map);
            trackPoints.push([d.lat, d.lon]);
            if(trackPoints.length > 1) { if(trackLine) trackLine.remove(); trackLine = L.polyline(trackPoints, { color:'#ffcc00', weight:3, opacity:0.9 }).addTo(map); }
            if(d.start_line_A_lat && d.start_line_A_lon && d.start_line_B_lat && d.start_line_B_lon && d.start_line_A_lat != 0 && !startLineMarker) {
                const linePoints = [[d.start_line_A_lat, d.start_line_A_lon], [d.start_line_B_lat, d.start_line_B_lon]];
                startLineMarker = L.polyline(linePoints, {color: '#ff3333', weight: 4, opacity: 0.8}).addTo(map);
            }
        }
    });
}

if(isMobile) { 
    document.getElementById('chartsContainer').style.display = 'none';
}

let initInterval = setInterval(() => { 
    if (initMap()) { 
        clearInterval(initInterval); 
        initCharts(); 
        loadTrack();
        fetchData(); 
        setInterval(fetchData, 200);
        setInterval(updateVibrationPanel, 1000);
        setTimeout(() => centerMap(), 2000); 
    } 
}, 200);
</script>
</body>
</html>
)rawliteral";