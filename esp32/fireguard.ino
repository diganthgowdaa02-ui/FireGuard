/**
 * FireGuard ESP32 — Self-hosted Dashboard
 *
 * The ESP32 serves its own web dashboard at http://<IP>/
 * Open that URL in any browser on the same WiFi network.
 *
 * Pins:
 *   GPIO 5  → IR Flame Sensor DO  (LOW = fire)
 *   GPIO 23 → Relay IN            (LOW = ON, HIGH = OFF)
 *   GPIO 18 → Buzzer
 *   GPIO 2  → Onboard LED
 */

#include <WiFi.h>
#include <WebServer.h>

#define WIFI_SSID      "Diganth's A36"
#define WIFI_PASSWORD  "diganth@098"

const int flamePin  = 5;
const int relayPin  = 23;
const int buzzerPin = 18;
const int ledPin    = 2;

#define COOLDOWN_SEC 10

WebServer server(80);

bool          prevFlame       = false;
bool          inCooldown      = false;
unsigned long cooldownStart   = 0;
bool          motorRunning    = false;
int           flameEventCount = 0;
unsigned long lastFlameTime   = 0;
unsigned long startTime       = 0;

// ── Dashboard HTML (served at GET /) ─────────────────────────────────────────
const char DASHBOARD[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>FireGuard Live</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:#0c0c0e;color:#f1f1f3;min-height:100vh}
.nav{background:#141418;border-bottom:1px solid #222;padding:0 1.5rem;height:56px;display:flex;align-items:center;justify-content:space-between;position:sticky;top:0;z-index:10}
.logo{font-weight:800;font-size:1.1rem;display:flex;align-items:center;gap:8px}
.pill{display:flex;align-items:center;gap:7px;background:#0c0c0e;border:1px solid #222;border-radius:99px;padding:5px 14px;font-size:.78rem;font-weight:600}
.dot{width:8px;height:8px;border-radius:50%;background:#22c55e;box-shadow:0 0 8px #22c55e;animation:blink 1.5s infinite}
@keyframes blink{0%,100%{opacity:1}50%{opacity:.4}}
.live-bar{padding:8px 1.5rem;font-size:.78rem;font-weight:500;border-bottom:1px solid #1a2a1a;background:rgba(34,197,94,.07);color:#86efac;display:flex;align-items:center;gap:.6rem;flex-wrap:wrap}
.live-bar .ldot{width:8px;height:8px;border-radius:50%;background:#22c55e;animation:blink 1.5s infinite;flex-shrink:0}
.sep{color:rgba(255,255,255,.15)}
.hero{padding:2rem 1.5rem 1.5rem;max-width:1000px;margin:0 auto}
.status-card{border:2px solid #222;border-radius:16px;padding:1.75rem 2rem;margin-bottom:1.5rem;display:flex;align-items:center;gap:1.5rem;transition:.3s}
.status-card.safe{border-color:#22c55e;background:rgba(34,197,94,.06)}
.status-card.danger{border-color:#ef4444;background:rgba(239,68,68,.08);animation:pdanger 1.5s infinite}
.status-card.warn{border-color:#eab308;background:rgba(234,179,8,.06)}
@keyframes pdanger{0%,100%{box-shadow:0 0 0 0 rgba(239,68,68,.3)}50%{box-shadow:0 0 0 12px rgba(239,68,68,0)}}
.s-icon{width:64px;height:64px;border-radius:50%;background:#1a1a1e;display:flex;align-items:center;justify-content:center;font-size:1.8rem;flex-shrink:0}
.s-title{font-size:2rem;font-weight:800;line-height:1.1}
.s-sub{font-size:.9rem;color:#6b6b80;margin-top:.3rem}
.s-time{margin-left:auto;font-size:.75rem;color:#444;font-family:monospace;text-align:right;white-space:nowrap}
.stats{display:grid;grid-template-columns:repeat(4,1fr);gap:1rem;margin-bottom:1.5rem}
.stat{background:#141418;border:1px solid #222;border-radius:12px;padding:1rem;text-align:center}
.stat-l{font-size:.68rem;color:#6b6b80;text-transform:uppercase;letter-spacing:.06em;margin-bottom:.4rem;font-weight:600}
.stat-v{font-size:1.5rem;font-weight:700;font-family:monospace}
.green{color:#22c55e}.red{color:#ef4444}.yellow{color:#eab308}
.cards{display:grid;grid-template-columns:1fr 1fr;gap:1rem}
.card{background:#141418;border:1px solid #222;border-radius:14px;padding:1.5rem}
.card.danger{border-color:#ef4444;background:rgba(239,68,68,.07)}
.card.active{border-color:#22c55e;background:rgba(34,197,94,.05)}
.card.warn{border-color:#eab308;background:rgba(234,179,8,.05)}
.card-top{display:flex;justify-content:space-between;align-items:center;margin-bottom:.75rem}
.c-icon{font-size:1.5rem}
.badge{font-size:.65rem;font-weight:700;padding:3px 10px;border-radius:99px;text-transform:uppercase}
.badge.safe{background:rgba(34,197,94,.15);color:#22c55e}
.badge.danger{background:rgba(239,68,68,.2);color:#ef4444}
.badge.warn{background:rgba(234,179,8,.15);color:#eab308}
.badge.live{background:rgba(34,197,94,.15);color:#22c55e}
.c-name{font-size:.72rem;color:#6b6b80;text-transform:uppercase;letter-spacing:.05em;margin-bottom:.4rem;font-weight:600}
.c-big{font-size:2.4rem;font-weight:800;font-family:monospace;line-height:1;margin-bottom:.4rem}
.c-desc{font-size:.8rem;color:#6b6b80}
.footer{text-align:center;padding:2rem;color:#444;font-size:.75rem;border-top:1px solid #1a1a1e;margin-top:2rem}
@media(max-width:600px){.stats{grid-template-columns:1fr 1fr}.cards{grid-template-columns:1fr}.status-card{flex-direction:column;text-align:center}.s-time{margin:0}}
</style>
</head>
<body>
<nav class="nav">
  <div class="logo">🔥 FireGuard</div>
  <div class="pill"><span class="dot"></span><span id="pillTxt">Live</span></div>
</nav>
<div class="live-bar">
  <span class="ldot"></span>
  <strong>ESP32 Connected & Live</strong>
  <span class="sep">·</span>
  <span id="liveDetail">Loading...</span>
</div>
<div class="hero">
  <div class="status-card safe" id="sc">
    <div class="s-icon" id="si">🛡️</div>
    <div>
      <div class="s-title" id="st">All Clear</div>
      <div class="s-sub" id="ss">No flame detected. System monitoring.</div>
    </div>
    <div class="s-time">Updated<br><span id="upd">--:--:--</span></div>
  </div>
  <div class="stats">
    <div class="stat"><div class="stat-l">Flame</div><div class="stat-v green" id="vFlame">NO</div></div>
    <div class="stat"><div class="stat-l">Pump</div><div class="stat-v" id="vPump">OFF</div></div>
    <div class="stat"><div class="stat-l">Events</div><div class="stat-v" id="vEvents">0</div></div>
    <div class="stat"><div class="stat-l">Uptime</div><div class="stat-v" id="vUp">--</div></div>
  </div>
  <div class="cards">
    <div class="card" id="cFlame">
      <div class="card-top"><span class="c-icon">🔥</span><span class="badge safe" id="bFlame">SAFE</span></div>
      <div class="c-name">IR Flame Sensor · GPIO 5</div>
      <div class="c-big green" id="bigFlame">NO FLAME</div>
      <div class="c-desc" id="dFlame">No infrared radiation detected</div>
    </div>
    <div class="card" id="cPump">
      <div class="card-top"><span class="c-icon">💧</span><span class="badge safe" id="bPump">STANDBY</span></div>
      <div class="c-name">Relay · Water Pump · GPIO 23</div>
      <div class="c-big green" id="bigPump">OFF</div>
      <div class="c-desc" id="dPump">Pump off. No fire detected.</div>
    </div>
  </div>
</div>
<div class="footer">FireGuard ESP32 · Self-hosted Dashboard · Auto-refreshes every 2s</div>
<script>
async function poll(){
  try{
    const r=await fetch('/api/sensors');
    const d=await r.json();
    setOnline(true);
    update(d);
  }catch(e){
    setOnline(false);
    console.warn('fetch failed',e);
  }
}

function setOnline(on){
  const bar=document.querySelector('.live-bar');
  const dot=document.querySelector('.live-bar .ldot');
  const pill=document.getElementById('pillTxt');
  if(on){
    bar.style.background='rgba(34,197,94,.07)';
    bar.style.borderBottomColor='rgba(34,197,94,.25)';
    bar.style.color='#86efac';
    dot.style.background='#22c55e';
    dot.style.boxShadow='0 0 8px #22c55e';
    pill.textContent='ESP32 Live';
  } else {
    bar.style.background='rgba(239,68,68,.07)';
    bar.style.borderBottomColor='rgba(239,68,68,.25)';
    bar.style.color='#fca5a5';
    dot.style.background='#ef4444';
    dot.style.boxShadow='0 0 8px #ef4444';
    dot.style.animation='none';
    document.getElementById('liveDetail').textContent='Connection lost — ESP32 not responding';
    pill.textContent='Offline';
    document.getElementById('sc').className='status-card';
    document.getElementById('si').textContent='⚠️';
    document.getElementById('st').textContent='Connection Lost';
    document.getElementById('ss').textContent='ESP32 stopped responding. Check power and WiFi.';
  }
}

function update(d){
  const flame=d.flame, pump=d.pump_active, cool=d.cooldown_left||0;
  document.getElementById('upd').textContent=new Date().toLocaleTimeString();
  document.getElementById('liveDetail').textContent=
    'Uptime: '+fmt(d.uptime||0)+' · WiFi: '+(d.rssi||'--')+' dBm · Heap: '+(d.heap||'--')+' KB';
  document.getElementById('vEvents').textContent=d.flame_events||0;
  document.getElementById('vUp').textContent=fmt(d.uptime||0);

  // Status card
  const sc=document.getElementById('sc');
  if(flame){
    sc.className='status-card danger';
    document.getElementById('si').textContent='🔥';
    document.getElementById('st').textContent='FIRE DETECTED';
    document.getElementById('ss').textContent='IR sensor triggered — water pump activated!';
  } else if(pump){
    sc.className='status-card warn';
    document.getElementById('si').textContent='💧';
    document.getElementById('st').textContent='Extinguishing...';
    document.getElementById('ss').textContent='Flame gone. Pump cooldown: '+cool+'s remaining.';
  } else {
    sc.className='status-card safe';
    document.getElementById('si').textContent='🛡️';
    document.getElementById('st').textContent='All Clear';
    document.getElementById('ss').textContent='No flame detected. System monitoring normally.';
  }

  // Stats
  const vf=document.getElementById('vFlame');
  vf.textContent=flame?'🔥 YES':'NO';
  vf.className='stat-v '+(flame?'red':'green');
  const vp=document.getElementById('vPump');
  vp.textContent=pump?'ON':'OFF';
  vp.className='stat-v '+(pump?'yellow':'green');

  // Flame card
  document.getElementById('cFlame').className='card'+(flame?' danger':'');
  document.getElementById('bigFlame').textContent=flame?'FLAME!':'NO FLAME';
  document.getElementById('bigFlame').className='c-big '+(flame?'red':'green');
  document.getElementById('bFlame').textContent=flame?'ALERT':'SAFE';
  document.getElementById('bFlame').className='badge '+(flame?'danger':'safe');
  document.getElementById('dFlame').textContent=flame?'⚠️ Flame detected!':'No radiation detected.';

  // Pump card
  document.getElementById('cPump').className='card'+(flame?' danger':pump?' warn':'');
  document.getElementById('bigPump').textContent=flame?'PUMPING':pump?'COOLDOWN':'OFF';
  document.getElementById('bigPump').className='c-big '+(flame?'red':pump?'yellow':'green');
  document.getElementById('bPump').textContent=flame?'ACTIVE':pump?'COOLDOWN':'STANDBY';
  document.getElementById('bPump').className='badge '+(flame?'danger':pump?'warn':'safe');
  document.getElementById('dPump').textContent=flame?'💧 Dispensing water!':pump?'Pump runs '+cool+'s more.':'Pump off. No fire.';
}

function fmt(s){return Math.floor(s/3600)+'h '+Math.floor((s%3600)/60)+'m '+s%60+'s'}

poll();
setInterval(poll,2000);
</script>
</body>
</html>
)rawhtml";

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(flamePin,  INPUT);
  pinMode(relayPin,  OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(ledPin,    OUTPUT);

  digitalWrite(relayPin,  HIGH);
  digitalWrite(buzzerPin, LOW);
  digitalWrite(ledPin,    LOW);

  // WiFi
  Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.printf("\n[WiFi] Connected!\n");
  Serial.printf("[Dashboard] Open: http://%s\n", WiFi.localIP().toString().c_str());
  Serial.printf("[API]       Open: http://%s/api/sensors\n", WiFi.localIP().toString().c_str());

  // Routes
  server.on("/", []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send_P(200, "text/html", DASHBOARD);
  });

  server.on("/api/sensors", handleSensors);
  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });

  server.begin();
  startTime = millis();
  Serial.println("[FireGuard] Ready.");
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();

  bool fireNow = (digitalRead(flamePin) == LOW);

  if (fireNow) {
    digitalWrite(relayPin,  LOW);
    digitalWrite(buzzerPin, HIGH);
    digitalWrite(ledPin,    HIGH);
    motorRunning = true;
    if (!prevFlame) {
      flameEventCount++;
      lastFlameTime = millis();
      inCooldown    = false;
      Serial.printf("[ALERT] FIRE #%d — Pump ON\n", flameEventCount);
    }
    prevFlame = true;
  } else {
    if (prevFlame) {
      inCooldown    = true;
      cooldownStart = millis();
      Serial.printf("[INFO] Flame gone. Cooldown %ds.\n", COOLDOWN_SEC);
    }
    prevFlame = false;

    if (inCooldown) {
      if ((millis() - cooldownStart) / 1000 >= COOLDOWN_SEC) {
        inCooldown   = false;
        motorRunning = false;
        digitalWrite(relayPin,  HIGH);
        digitalWrite(buzzerPin, LOW);
        digitalWrite(ledPin,    LOW);
        Serial.println("[INFO] Cooldown done. Pump OFF.");
      }
    } else {
      digitalWrite(relayPin,  HIGH);
      digitalWrite(buzzerPin, LOW);
      digitalWrite(ledPin,    HIGH);
      motorRunning = false;
    }
  }

  delay(200);

  // ── Print to Serial Monitor every 2 seconds ──
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 2000) {
    lastPrint = millis();
    unsigned long uptime = (millis() - startTime) / 1000;
    Serial.println("─────────────────────────────");
    Serial.printf("  Flame    : %s\n",   fireNow      ? "🔥 DETECTED" : "✅ None");
    Serial.printf("  Pump     : %s\n",   motorRunning ? "ON (active)" : "OFF");
    Serial.printf("  Cooldown : %ds\n",  inCooldown   ? (int)(COOLDOWN_SEC - (millis()-cooldownStart)/1000) : 0);
    Serial.printf("  Events   : %d\n",   flameEventCount);
    Serial.printf("  Uptime   : %luh %lum %lus\n",
                  uptime/3600, (uptime%3600)/60, uptime%60);
    Serial.printf("  WiFi     : %d dBm\n", WiFi.RSSI());
    Serial.printf("  Heap     : %d KB\n",  ESP.getFreeHeap()/1024);
    Serial.printf("  IP       : %s\n",   WiFi.localIP().toString().c_str());
    Serial.println("─────────────────────────────");
  }
}

// ─────────────────────────────────────────────────────────────────────────────
void handleSensors() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-cache");

  bool fireNow = (digitalRead(flamePin) == LOW);
  int  cooldownLeft = 0;
  if (inCooldown) {
    unsigned long e = (millis() - cooldownStart) / 1000;
    cooldownLeft = max(0, COOLDOWN_SEC - (int)e);
  }

  String json = "{";
  json += "\"flame\":"         + String(fireNow ? "true" : "false") + ",";
  json += "\"pump_active\":"   + String(motorRunning ? "true" : "false") + ",";
  json += "\"cooldown_left\":" + String(cooldownLeft) + ",";
  json += "\"flame_events\":"  + String(flameEventCount) + ",";
  json += "\"alert_level\":"   + String(fireNow ? 2 : inCooldown ? 1 : 0) + ",";
  json += "\"uptime\":"        + String((millis() - startTime) / 1000) + ",";
  json += "\"heap\":"          + String(ESP.getFreeHeap() / 1024) + ",";
  json += "\"rssi\":"          + String(WiFi.RSSI());
  json += "}";

  server.send(200, "application/json", json);
}
