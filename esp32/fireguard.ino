/**
 * FireGuard ESP32 Firmware
 * 
 * Components:
 *   - IR Flame Sensor (DO) → GPIO 27
 *   - Relay Module (IN)    → GPIO 26  (controls water pump)
 *   - Onboard LED          → GPIO 2   (status indicator)
 * 
 * Logic:
 *   Flame detected → Relay ON → Water pump activates
 *   No flame       → Relay OFF → Pump off
 * 
 * HTTP API (port 80):
 *   GET /api/sensors  → flame status + pump state
 *   GET /api/status   → device health
 * 
 * Libraries needed (Arduino Library Manager):
 *   - ArduinoJson (v6)
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// ── WiFi Credentials ──────────────────────────────────────────────────────────
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ── Pin Definitions ───────────────────────────────────────────────────────────
#define FLAME_PIN   27   // IR Flame Sensor digital output (LOW = flame)
#define RELAY_PIN   26   // Relay IN pin (HIGH = relay ON = pump ON)
#define LED_PIN     2    // Onboard LED

// ── Pump auto-off timeout ─────────────────────────────────────────────────────
// Pump stays ON for this many seconds after flame is gone (drain residual heat)
#define PUMP_COOLDOWN_SEC  10

// ── Objects ───────────────────────────────────────────────────────────────────
WebServer server(80);

// ── State ─────────────────────────────────────────────────────────────────────
bool          flameDetected   = false;
bool          pumpActive      = false;
unsigned long pumpOffTimer    = 0;      // millis when cooldown started
unsigned long startTime       = 0;
unsigned long lastFlameTime   = 0;
int           flameEventCount = 0;

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n[FireGuard] Booting...");

  // Pin setup
  pinMode(FLAME_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN,   OUTPUT);

  // Safe defaults — pump OFF on boot
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(LED_PIN,   LOW);

  // Connect WiFi
  connectWiFi();

  // Register HTTP routes
  server.on("/",            handleRoot);
  server.on("/api/sensors", handleSensors);
  server.on("/api/status",  handleStatus);
  server.onNotFound([]() {
    server.send(404, "application/json", "{\"error\":\"Not found\"}");
  });

  server.begin();
  Serial.println("[FireGuard] HTTP server started");
  startTime = millis();

  blinkLED(3, 150);   // 3 blinks = ready
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();
  checkFlameAndRelay();
  delay(50);
}

// ── Core Logic ────────────────────────────────────────────────────────────────
void checkFlameAndRelay() {
  // IR flame sensor: LOW = flame present (active low)
  bool flame = (digitalRead(FLAME_PIN) == LOW);

  // Track new flame event
  if (flame && !flameDetected) {
    flameEventCount++;
    lastFlameTime = millis();
    Serial.println("[ALERT] 🔥 FLAME DETECTED — Activating water pump!");
  }

  flameDetected = flame;

  if (flame) {
    // Flame present → pump ON immediately
    activatePump();
    pumpOffTimer = millis();   // reset cooldown window
  } else if (pumpActive) {
    // Flame gone → run pump for PUMP_COOLDOWN_SEC more seconds
    unsigned long elapsed = (millis() - pumpOffTimer) / 1000;
    if (elapsed >= PUMP_COOLDOWN_SEC) {
      deactivatePump();
      Serial.println("[INFO] Cooldown complete — pump OFF");
    }
  }

  // LED indicator
  if (flameDetected) {
    // Rapid flash when flame detected
    digitalWrite(LED_PIN, (millis() / 200) % 2);
  } else if (pumpActive) {
    // Slow flash during cooldown
    digitalWrite(LED_PIN, (millis() / 600) % 2);
  } else {
    // Steady ON = safe & connected
    digitalWrite(LED_PIN, WiFi.status() == WL_CONNECTED ? HIGH : LOW);
  }
}

void activatePump() {
  if (!pumpActive) {
    Serial.println("[PUMP] ON");
  }
  pumpActive = true;
  digitalWrite(RELAY_PIN, HIGH);
}

void deactivatePump() {
  pumpActive = false;
  pumpOffTimer = 0;
  digitalWrite(RELAY_PIN, LOW);
}

// ── WiFi ──────────────────────────────────────────────────────────────────────
void connectWiFi() {
  Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.println("[WiFi] Open this URL in your browser:");
    Serial.printf("       http://%s/api/sensors\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WiFi] Connection failed. Running without network.");
    Serial.println("[WiFi] Fire detection + relay still work offline.");
  }
}

// ── HTTP Handlers ─────────────────────────────────────────────────────────────

void handleRoot() {
  addCORSHeaders();
  server.send(200, "text/plain",
    "FireGuard API\n"
    "GET /api/sensors\n"
    "GET /api/status\n"
  );
}

// GET /api/sensors
// Returns data consumed by the FireGuard website dashboard
void handleSensors() {
  addCORSHeaders();

  bool flame = (digitalRead(FLAME_PIN) == LOW);

  // Cooldown remaining (seconds)
  int cooldownLeft = 0;
  if (!flame && pumpActive && pumpOffTimer > 0) {
    unsigned long elapsed = (millis() - pumpOffTimer) / 1000;
    cooldownLeft = max(0, (int)PUMP_COOLDOWN_SEC - (int)elapsed);
  }

  StaticJsonDocument<256> doc;
  doc["flame"]           = flame;
  doc["pump_active"]     = pumpActive;
  doc["cooldown_left"]   = cooldownLeft;  // seconds pump stays on after flame gone
  doc["flame_events"]    = flameEventCount;
  doc["uptime"]          = (millis() - startTime) / 1000;
  doc["heap"]            = ESP.getFreeHeap() / 1024;
  doc["rssi"]            = WiFi.RSSI();
  doc["alert_level"]     = flame ? 2 : (pumpActive ? 1 : 0);
  doc["timestamp"]       = millis();

  // Include last flame time as seconds ago
  if (lastFlameTime > 0) {
    doc["last_flame_sec"] = (millis() - lastFlameTime) / 1000;
  }

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// GET /api/status
void handleStatus() {
  addCORSHeaders();

  StaticJsonDocument<200> doc;
  doc["device"]    = "FireGuard-ESP32";
  doc["firmware"]  = "2.1.4";
  doc["ip"]        = WiFi.localIP().toString();
  doc["rssi"]      = WiFi.RSSI();
  doc["uptime"]    = (millis() - startTime) / 1000;
  doc["heap"]      = ESP.getFreeHeap() / 1024;
  doc["online"]    = true;

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// ── Helpers ───────────────────────────────────────────────────────────────────
void addCORSHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET");
  server.sendHeader("Cache-Control", "no-cache");
}

void blinkLED(int times, int ms) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH); delay(ms);
    digitalWrite(LED_PIN, LOW);  delay(ms);
  }
}
