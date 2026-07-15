#include <PZEM004Tv30.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WebServer.h>

// ─── WiFi ──────────────────────────────────────────────────
const char* ssid     = "TOPNET_5ED0";
const char* password = "191919982761998";

// ─── Broches ───────────────────────────────────────────────
#define RXD2       16
#define TXD2       17
#define RELAIS      4
#define BTN_RESET   0
#define LED_VERTE   2
#define LED_ROUGE  15
#define IMAX       0.4f

// ─── Matériel ──────────────────────────────────────────────
PZEM004Tv30       pzem(Serial2, RXD2, TXD2);
LiquidCrystal_I2C lcd(0x27, 16, 2);
WebServer         server(80);

// ─── Variables ─────────────────────────────────────────────
float tension, courant, puissance, energieWh, frequence, facteurP;
bool  surcharge  = false;
unsigned long chrono = 0;
String ipGlobale = "";

#define RELAIS_ON()   digitalWrite(RELAIS, LOW)
#define RELAIS_OFF()  digitalWrite(RELAIS, HIGH)

// ════════════════════════════════════════════════════════════
//  PAGE WEB AMÉLIORÉE (JAUGES + CHARTS + SEUILS)
// ════════════════════════════════════════════════════════════
const char PAGE_WEB[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>POWERWATCH - Surveillance Énergétique</title>
    <link href="https://fonts.googleapis.com/css2?family=Rajdhani:wght@400;600;700;800&family=Share+Tech+Mono&display=swap" rel="stylesheet">
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        :root {
            --bg: #0a0e1a;
            --panel: #111827;
            --border: #1e2d45;
            --accent: #00d4ff;
            --green: #00ff9d;
            --red: #ff3d5a;
            --yellow: #ffd166;
            --purple: #a78bfa;
            --orange: #fb923c;
            --text: #cdd6f4;
            --muted: #4a5568;
        }
        * { margin:0; padding:0; box-sizing:border-box; }
        body {
            background: var(--bg);
            font-family: 'Rajdhani', sans-serif;
            color: var(--text);
            min-height: 100vh;
            padding-bottom: 40px;
        }
        header {
            display: flex; align-items: center; justify-content: space-between;
            padding: 20px 30px;
            border-bottom: 1px solid var(--border);
            background: rgba(17,24,39,0.9);
            backdrop-filter: blur(10px);
            position: sticky; top: 0; z-index: 100;
        }
        .logo { display: flex; align-items: center; gap: 12px; }
        .logo-icon {
            width: 40px; height: 40px;
            background: linear-gradient(135deg, var(--accent), var(--green));
            border-radius: 10px;
            display: flex; align-items: center; justify-content: center;
            font-size: 20px;
        }
        .logo h1 { font-size: 24px; font-weight: 800; letter-spacing: 2px; color: white; }
        .logo span { color: var(--accent); }
        .status-badge {
            display: flex; align-items: center; gap: 10px;
            padding: 10px 20px;
            border-radius: 30px;
            font-size: 14px; font-weight: 700;
            border: 1px solid;
            transition: all 0.3s;
        }
        .status-badge.normal    { background: rgba(0,255,157,0.15); border-color: var(--green); color: var(--green); }
        .status-badge.surcharge { background: rgba(255,61,90,0.15); border-color: var(--red); color: var(--red); }
        .status-badge.erreur    { background: rgba(255,209,102,0.15); border-color: var(--yellow); color: var(--yellow); }
        .dot {
            width: 10px; height: 10px; border-radius: 50%;
            background: currentColor;
            animation: pulse 1.5s infinite;
        }
        @keyframes pulse { 0%,100%{opacity:1;transform:scale(1)} 50%{opacity:0.3;transform:scale(0.7)} }
        #alerte {
            display: none;
            margin: 20px 30px; padding: 16px 24px;
            background: rgba(255,61,90,0.15);
            border: 1px solid var(--red);
            border-radius: 12px;
            color: var(--red);
            font-size: 16px; font-weight: 700;
            text-align: center;
            animation: flash 1s infinite;
        }
        @keyframes flash { 0%,100%{opacity:1} 50%{opacity:0.5} }
        
        /* Grille des cartes */
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
            gap: 24px;
            padding: 20px 30px;
            max-width: 1400px;
            margin: 0 auto;
        }
        
        .card {
            background: rgba(17,24,39,0.8);
            backdrop-filter: blur(8px);
            border: 1px solid var(--border);
            border-radius: 20px;
            padding: 24px;
            position: relative;
            overflow: hidden;
            transition: transform 0.3s, border-color 0.3s;
        }
        .card:hover { transform: translateY(-5px); border-color: var(--accent); }
        .card::before {
            content: ''; position: absolute; top: 0; left: 0; right: 0; height: 3px;
            background: linear-gradient(90deg, transparent, var(--card-color), transparent);
        }
        .card-header {
            display: flex; align-items: center; justify-content: space-between;
            margin-bottom: 20px;
        }
        .card-label {
            font-size: 12px; font-weight: 700;
            letter-spacing: 2px;
            color: var(--muted);
            text-transform: uppercase;
        }
        .card-icon {
            width: 40px; height: 40px;
            border-radius: 10px;
            display: flex; align-items: center; justify-content: center;
            font-size: 18px;
            background: rgba(255,255,255,0.05);
            border: 1px solid var(--border);
        }
        .card-value {
            font-family: 'Share Tech Mono', monospace;
            font-size: 48px;
            color: white;
            line-height: 1;
            margin-bottom: 4px;
        }
        .card-unit {
            font-size: 14px;
            color: var(--muted);
            letter-spacing: 1px;
        }
        .card-bar {
            margin-top: 16px; height: 4px;
            background: var(--border);
            border-radius: 4px;
            overflow: hidden;
        }
        .card-bar-fill {
            height: 100%;
            border-radius: 4px;
            background: var(--card-color);
            transition: width 0.5s ease;
        }
        
        /* Couleurs des cartes */
        .c-tension   { --card-color: var(--accent);  }
        .c-courant   { --card-color: var(--yellow);  }
        .c-puissance { --card-color: #ff6b9d;        }
        .c-energie   { --card-color: var(--green);   }
        .c-frequence { --card-color: var(--purple);  }
        .c-facteur   { --card-color: var(--orange);  }
        
        /* Section graphique */
        .chart-section {
            padding: 20px 30px;
            max-width: 1400px;
            margin: 0 auto;
        }
        .chart-container {
            background: rgba(17,24,39,0.8);
            backdrop-filter: blur(8px);
            border: 1px solid var(--border);
            border-radius: 20px;
            padding: 24px;
            margin-top: 20px;
        }
        .chart-title {
            font-size: 18px;
            font-weight: 700;
            color: white;
            margin-bottom: 20px;
        }
        .chart-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 24px;
        }
        .chart-box {
            background: rgba(0,0,0,0.3);
            border-radius: 12px;
            padding: 16px;
        }
        .chart-box h3 {
            font-size: 14px; font-weight: 600;
            color: var(--muted);
            text-transform: uppercase;
            letter-spacing: 1px;
            margin-bottom: 12px;
        }
        canvas {
            max-height: 200px;
        }
        
        /* Seuils */
        .threshold-container {
            display: flex;
            align-items: center;
            gap: 16px;
            margin-top: 12px;
            padding: 10px 16px;
            background: rgba(0,0,0,0.3);
            border-radius: 8px;
        }
        .threshold-label {
            font-size: 12px;
            color: var(--muted);
        }
        .threshold-value {
            font-family: 'Share Tech Mono', monospace;
            font-size: 16px;
            color: white;
        }
        .threshold-bar {
            flex: 1;
            height: 4px;
            background: var(--border);
            border-radius: 4px;
            overflow: hidden;
        }
        .threshold-fill {
            height: 100%;
            border-radius: 4px;
            background: var(--card-color);
            transition: width 0.3s ease;
        }
        .threshold-warning {
            color: var(--yellow);
        }
        .threshold-danger {
            color: var(--red);
        }
        
        footer {
            text-align: center;
            padding: 20px;
            color: var(--muted);
            font-size: 13px;
            letter-spacing: 1px;
        }
        #refresh-info {
            color: var(--accent);
            font-family: 'Share Tech Mono', monospace;
        }
        
        @media (max-width: 768px) {
            .chart-grid {
                grid-template-columns: 1fr;
            }
            .grid {
                grid-template-columns: 1fr;
            }
        }
    </style>
</head>
<body>
<header>
    <div class="logo">
        <div class="logo-icon">⚡</div>
        <h1>POWER<span>WATCH</span></h1>
    </div>
    <div id="status-badge" class="status-badge normal">
        <div class="dot"></div>
        <span id="status-text">NORMAL</span>
    </div>
</header>

<div id="alerte">⚠ SURCHARGE DÉTECTÉE — CHARGE COUPÉE — Appuyer sur BOOT pour réarmer</div>

<!-- Cartes de mesures -->
<div class="grid">
    <!-- Tension -->
    <div class="card c-tension">
        <div class="card-header">
            <div class="card-label">Tension</div>
            <div class="card-icon">🔌</div>
        </div>
        <div class="card-value" id="val-tension">---</div>
        <div class="card-unit">Volts (V)</div>
        <div class="card-bar"><div class="card-bar-fill" id="bar-tension" style="width:0%"></div></div>
        <div class="threshold-container">
            <span class="threshold-label">Seuil :</span>
            <span class="threshold-value" id="th-tension">260 V</span>
            <div class="threshold-bar"><div class="threshold-fill" id="thbar-tension" style="width:0%"></div></div>
        </div>
    </div>
    
    <!-- Courant -->
    <div class="card c-courant">
        <div class="card-header">
            <div class="card-label">Courant</div>
            <div class="card-icon">⚡</div>
        </div>
        <div class="card-value" id="val-courant">---</div>
        <div class="card-unit">Ampères (A)</div>
        <div class="card-bar"><div class="card-bar-fill" id="bar-courant" style="width:0%"></div></div>
        <div class="threshold-container">
            <span class="threshold-label">Seuil max :</span>
            <span class="threshold-value" id="th-courant">0.4 A</span>
            <div class="threshold-bar"><div class="threshold-fill" id="thbar-courant" style="width:0%"></div></div>
        </div>
    </div>
    
    <!-- Puissance -->
    <div class="card c-puissance">
        <div class="card-header">
            <div class="card-label">Puissance</div>
            <div class="card-icon">💡</div>
        </div>
        <div class="card-value" id="val-puissance">---</div>
        <div class="card-unit">Watts (W)</div>
        <div class="card-bar"><div class="card-bar-fill" id="bar-puissance" style="width:0%"></div></div>
    </div>
    
    <!-- Énergie -->
    <div class="card c-energie">
        <div class="card-header">
            <div class="card-label">Énergie</div>
            <div class="card-icon">📊</div>
        </div>
        <div class="card-value" id="val-energie">---</div>
        <div class="card-unit">Watt-heures (Wh)</div>
        <div class="card-bar"><div class="card-bar-fill" id="bar-energie" style="width:0%"></div></div>
    </div>
    
    <!-- Fréquence -->
    <div class="card c-frequence">
        <div class="card-header">
            <div class="card-label">Fréquence</div>
            <div class="card-icon">〰️</div>
        </div>
        <div class="card-value" id="val-frequence">---</div>
        <div class="card-unit">Hertz (Hz)</div>
        <div class="card-bar"><div class="card-bar-fill" id="bar-frequence" style="width:0%"></div></div>
    </div>
    
    <!-- Facteur de puissance -->
    <div class="card c-facteur">
        <div class="card-header">
            <div class="card-label">Facteur de puissance</div>
            <div class="card-icon">📐</div>
        </div>
        <div class="card-value" id="val-facteur">---</div>
        <div class="card-unit">cos φ</div>
        <div class="card-bar"><div class="card-bar-fill" id="bar-facteur" style="width:0%"></div></div>
    </div>
</div>

<!-- Section Graphiques -->
<div class="chart-section">
    <div class="chart-container">
        <div class="chart-title">📈 Évolution en temps réel</div>
        <div class="chart-grid">
            <div class="chart-box">
                <h3>Courant (A)</h3>
                <canvas id="chart-courant"></canvas>
            </div>
            <div class="chart-box">
                <h3>Puissance (W)</h3>
                <canvas id="chart-puissance"></canvas>
            </div>
        </div>
    </div>
</div>

<footer>
    Mise à jour toutes les 2 secondes &nbsp;|&nbsp; <span id="refresh-info">--:--:--</span>
</footer>

<script>
// ── Historique des données ──────────────────────────────
const MAX_POINTS = 30;
let historyCourant = [];
let historyPuissance = [];

// ── Initialisation des graphiques ──────────────────────
const ctxCourant = document.getElementById('chart-courant').getContext('2d');
const ctxPuissance = document.getElementById('chart-puissance').getContext('2d');

const chartCourant = new Chart(ctxCourant, {
    type: 'line',
    data: {
        labels: Array(MAX_POINTS).fill(''),
        datasets: [{
            label: 'Courant (A)',
            data: Array(MAX_POINTS).fill(0),
            borderColor: '#ffd166',
            backgroundColor: 'rgba(255,209,102,0.1)',
            borderWidth: 2,
            fill: true,
            tension: 0.3,
            pointRadius: 0
        }]
    },
    options: {
        responsive: true,
        maintainAspectRatio: false,
        plugins: { legend: { display: false } },
        scales: {
            x: { display: false },
            y: { min: 0, grid: { color: 'rgba(255,255,255,0.05)' } }
        },
        animation: { duration: 300 }
    }
});

const chartPuissance = new Chart(ctxPuissance, {
    type: 'line',
    data: {
        labels: Array(MAX_POINTS).fill(''),
        datasets: [{
            label: 'Puissance (W)',
            data: Array(MAX_POINTS).fill(0),
            borderColor: '#ff6b9d',
            backgroundColor: 'rgba(255,107,157,0.1)',
            borderWidth: 2,
            fill: true,
            tension: 0.3,
            pointRadius: 0
        }]
    },
    options: {
        responsive: true,
        maintainAspectRatio: false,
        plugins: { legend: { display: false } },
        scales: {
            x: { display: false },
            y: { min: 0, grid: { color: 'rgba(255,255,255,0.05)' } }
        },
        animation: { duration: 300 }
    }
});

// ── Mise à jour principale ──────────────────────────────
function maj() {
    fetch('/data')
        .then(r => r.json())
        .then(d => {
            // Mise à jour des valeurs
            document.getElementById('val-tension').textContent   = d.tension.toFixed(1);
            document.getElementById('val-courant').textContent   = d.courant.toFixed(3);
            document.getElementById('val-puissance').textContent = d.puissance.toFixed(1);
            document.getElementById('val-energie').textContent   = d.energieWh.toFixed(1);
            document.getElementById('val-frequence').textContent = d.frequence.toFixed(1);
            document.getElementById('val-facteur').textContent   = d.facteurP.toFixed(2);
            
            // Barres de progression
            const pctTension = Math.min(d.tension / 260 * 100, 100);
            const pctCourant = Math.min(d.courant / 0.4 * 100, 100);
            const pctPuissance = Math.min(d.puissance / 500 * 100, 100);
            const pctFrequence = Math.min(d.frequence / 60 * 100, 100);
            const pctFacteur = Math.min(d.facteurP * 100, 100);
            
            document.getElementById('bar-tension').style.width   = pctTension + '%';
            document.getElementById('bar-courant').style.width   = pctCourant + '%';
            document.getElementById('bar-puissance').style.width = pctPuissance + '%';
            document.getElementById('bar-energie').style.width   = Math.min(d.energieWh / 1000 * 100, 100) + '%';
            document.getElementById('bar-frequence').style.width = pctFrequence + '%';
            document.getElementById('bar-facteur').style.width   = pctFacteur + '%';
            
            // Barres de seuil
            document.getElementById('thbar-tension').style.width   = pctTension + '%';
            document.getElementById('thbar-courant').style.width   = pctCourant + '%';
            
            // Couleur seuil courant
            const thbarCourant = document.getElementById('thbar-courant');
            if (pctCourant > 90) {
                thbarCourant.style.background = 'var(--red)';
            } else if (pctCourant > 70) {
                thbarCourant.style.background = 'var(--yellow)';
            } else {
                thbarCourant.style.background = 'var(--card-color)';
            }
            
            // Historique des graphiques
            historyCourant.push(d.courant);
            historyPuissance.push(d.puissance);
            if (historyCourant.length > MAX_POINTS) historyCourant.shift();
            if (historyPuissance.length > MAX_POINTS) historyPuissance.shift();
            
            // Mise à jour des graphiques
            chartCourant.data.datasets[0].data = historyCourant;
            chartPuissance.data.datasets[0].data = historyPuissance;
            chartCourant.update();
            chartPuissance.update();
            
            // Badge de statut
            const badge = document.getElementById('status-badge');
            const txt = document.getElementById('status-text');
            const alerte = document.getElementById('alerte');
            
            if (d.surcharge) {
                badge.className = 'status-badge surcharge';
                txt.textContent = 'SURCHARGE';
                alerte.style.display = 'block';
            } else if (d.erreur) {
                badge.className = 'status-badge erreur';
                txt.textContent = 'ERREUR CAPTEUR';
                alerte.style.display = 'none';
            } else {
                badge.className = 'status-badge normal';
                txt.textContent = 'NORMAL';
                alerte.style.display = 'none';
            }
            
            // Heure de mise à jour
            document.getElementById('refresh-info').textContent =
                new Date().toLocaleTimeString('fr-FR');
        })
        .catch(() => {
            document.getElementById('status-badge').className = 'status-badge erreur';
            document.getElementById('status-text').textContent = 'HORS LIGNE';
        });
}

setInterval(maj, 2000);
window.onload = maj;
</script>
</body>
</html>
)rawliteral";

// ════════════════════════════════════════════════════════════
//  LEDs
// ════════════════════════════════════════════════════════════
void led_normal()    { digitalWrite(LED_VERTE, HIGH); digitalWrite(LED_ROUGE, LOW);  }
void led_surcharge() { digitalWrite(LED_VERTE, LOW);  digitalWrite(LED_ROUGE, HIGH); }
void led_erreur()    {
    digitalWrite(LED_VERTE, HIGH); digitalWrite(LED_ROUGE, HIGH); delay(200);
    digitalWrite(LED_VERTE, LOW);  digitalWrite(LED_ROUGE, LOW);  delay(200);
}

// ════════════════════════════════════════════════════════════
//  ROUTES WEB
// ════════════════════════════════════════════════════════════
void route_accueil() { server.send(200, "text/html", PAGE_WEB); }

void route_data() {
    bool erreur = isnan(tension) || isnan(courant);
    String json = "{";
    json += "\"tension\":"   + String(erreur ? 0 : tension,   1) + ",";
    json += "\"courant\":"   + String(erreur ? 0 : courant,   3) + ",";
    json += "\"puissance\":" + String(erreur ? 0 : puissance, 1) + ",";
    json += "\"energieWh\":" + String(erreur ? 0 : energieWh, 1) + ",";
    json += "\"frequence\":" + String(erreur ? 0 : frequence, 1) + ",";
    json += "\"facteurP\":"  + String(erreur ? 0 : facteurP,  2) + ",";
    json += "\"surcharge\":" + String(surcharge ? "true" : "false") + ",";
    json += "\"erreur\":"    + String(erreur    ? "true" : "false");
    json += "}";
    server.send(200, "application/json", json);
}

// ════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    delay(2000);

    pinMode(RELAIS, OUTPUT);
    RELAIS_ON();
    Serial.println("Relais : ON (lampe allumee)");

    pinMode(BTN_RESET, INPUT_PULLUP);
    pinMode(LED_VERTE, OUTPUT);
    pinMode(LED_ROUGE, OUTPUT);

    led_surcharge(); delay(600);
    led_normal();    delay(600);

    lcd.init(); lcd.backlight();
    lcd.setCursor(0, 0); lcd.print(" MESURE ENERGIE ");
    lcd.setCursor(0, 1); lcd.print("  Demarrage...  ");
    delay(1500);

    WiFi.begin(ssid, password);
    Serial.print("Connexion WiFi");
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Connexion WiFi  ");
    lcd.setCursor(0, 1); lcd.print("Patientez...    ");
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }

    ipGlobale = WiFi.localIP().toString();
    Serial.println();
    Serial.println("============================");
    Serial.println("      WiFi connecte !       ");
    Serial.println("Ouvre dans ton navigateur : ");
    Serial.println("http://" + ipGlobale);
    Serial.println("============================");

    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("WiFi connecte ! ");
    lcd.setCursor(0, 1); lcd.print("Voir Serial...  ");
    delay(2000);
    lcd.clear();

    pzem.resetEnergy();
    Serial.println("Energie PZEM remise a zero !");

    server.on("/",     route_accueil);
    server.on("/data", route_data);
    server.begin();
    Serial.println("Serveur web demarre !");
}

// ════════════════════════════════════════════════════════════
//  LOOP
// ════════════════════════════════════════════════════════════
void loop() {
    server.handleClient();

    if (surcharge && digitalRead(BTN_RESET) == LOW) {
        surcharge = false;
        RELAIS_ON();
        pzem.resetEnergy();
        led_normal();
        lcd.clear(); lcd.setCursor(0, 0); lcd.print("  Reset OK...   ");
        Serial.println("Reset OK - Systeme rearm!");
        delay(1000); lcd.clear();
    }

    if (millis() - chrono < 2000) return;
    chrono = millis();

    tension   = pzem.voltage();
    courant   = pzem.current();
    puissance = pzem.power();
    energieWh = pzem.energy() * 1000.0;
    frequence = pzem.frequency();
    facteurP  = pzem.pf();

    if (isnan(tension) || isnan(courant)) {
        led_erreur();
        lcd.clear(); lcd.setCursor(0, 0); lcd.print("Erreur capteur !");
        Serial.println("ERREUR : capteur non detecte !");
        return;
    }

    if (courant > IMAX && !surcharge) {
        surcharge = true;
        RELAIS_OFF();
        led_surcharge();
        lcd.clear();
        lcd.setCursor(0, 0); lcd.print("!! SURCHARGE !!");
        lcd.setCursor(0, 1); lcd.print("Charge coupee   ");
        Serial.println("!!! SURCHARGE - Charge coupee !!!");
    }

    if (!surcharge) {
        RELAIS_ON();
        led_normal();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("U:"); lcd.print(tension, 0);
        lcd.print("V I:"); lcd.print(courant, 3); lcd.print("A");
        lcd.setCursor(0, 1);
        lcd.print("P:"); lcd.print(puissance, 1);
        lcd.print("W E:"); lcd.print(energieWh, 1); lcd.print("Wh");
    }

    Serial.println("--------------------");
    Serial.print("Tension   : "); Serial.print(tension,   1); Serial.println(" V");
    Serial.print("Courant   : "); Serial.print(courant,   3); Serial.println(" A");
    Serial.print("Puissance : "); Serial.print(puissance, 1); Serial.println(" W");
    Serial.print("Energie   : "); Serial.print(energieWh, 2); Serial.println(" Wh");
    Serial.print("Frequence : "); Serial.print(frequence, 1); Serial.println(" Hz");
    Serial.print("Facteur P : "); Serial.println(facteurP, 2);
    Serial.println(surcharge ? "ETAT : SURCHARGE" : "ETAT : Normal");
    Serial.print("IP        : http://"); Serial.println(ipGlobale);
    Serial.println("--------------------");
}