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
 *   Flame gone     → 10s cooldown → Relay OFF → Pump off
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
const char* WIFI_SSID     = "Diganth's A36";
const char* WIFI_PASSWORD = "diganth@098";

// ── Pin Definitions ───────────────────────────────────────────────────────────
#define FLAME_PIN          27   // IR Flame Sensor DO (LOW = flame detected)
#define RELAY_PIN          26   // Relay IN (HIGH = relay ON = pump ON)
#define LED_PIN            2    // Onboard LED

// ── Cooldown ──────────────────────────────────────────────────────────────────
#define PUMP_COOLDOWN_SEC  10   // Seconds pump stays ON after flame disappears

// ── Objects ───────────────────────────────────────────────────────────────────
WebServer server(80);

// ── State ─────────────────────────────────────────────────────────────────────
bool          flameDetected   = false;
bool          pumpActive      = false;
bool          inCooldown      = false;        // true only during the post-flame cooldown
unsigned long cooldownStart   = 0;            // millis() when cooldown began
unsigned long startTime       = 0;
unsigned long lastFlameTime   = 0;
int           flameEventCount = 0;

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n[FireGuard] Booting...");

  pinMode(FLAME_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN,   OUTPUT);

  // Safe defaults — relay OFF on boot
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(LED_PIN,   LOW);

  connectWiFi();

  server.on("/",            handleRoot);
  server.on("/api/sensors", handleSensors);
  server.on("/api/status",  handleStatus);
  server.onNotFound([]() {
    server.send(404, "application/json", "{\"error\":\"Not found\"}");
  });

  server.begin();
  Serial.println("[FireGuard] HTTP server started");
  startTime = millis();

  blinkLED(3, 150);
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();
  checkFlameAndRelay();
  delay(50);
}

// ── Core Logic ────────────────────────────────────────────────────────────────
void checkFlameAndRelay() {
  // LOW = flame present (active-low sensor)
  bool flame = (digitalRead(FLAME_PIN) == LOW);

  // ── Flame just started ──
  if (flame && !flameDetected) {
    flameEventCount++;
    lastFlameTime = millis();
    inCooldown    = false;   // cancel any previous cooldown — flame is back
    Serial.println("[ALERT] FLAME DETECTED — activating pump!");
  }

  // ── Flame just stopped ──
  if (!flame && flameDetected) {
    // Start cooldown timer exactly once when flame disappears
    inCooldown    = true;
    cooldownStart = millis();
    Serial.println("[INFO] Flame gone — starting cooldown timer.");
  }

  flameDetected = flame;

  // ── Relay control ──
  if (flame) {
    // Flame present → pump ON
    activatePump();

  } else if (inCooldown) {
    // No flame but cooldown still running
    unsigned long elapsed = (millis() - cooldownStart) / 1000;
    if (elapsed >= PUMP_COOLDOWN_SEC) {
      inCooldown = false;
      deactivatePump();
      Serial.println("[INFO] Cooldown complete — pump OFF.");
    } else {
      activatePump();  // keep pump on during cooldown
    }

  } else {
    // No flame, no cooldown → pump stays OFF
    deactivatePump();
  }

  // ── LED indicator ──
  if (flameDetected) {
    digitalWrite(LED_PIN, (millis() / 200) % 2);   // fast flash = fire
  } else if (inCooldown) {
    digitalWrite(LED_PIN, (millis() / 600) % 2);   // slow flash = cooldown
  } else {
    digitalWrite(LED_PIN, WiFi.status() == WL_CONNECTED ? HIGH : LOW); // steady = safe
  }
}

void activatePump() {
  if (!pumpActive) {
    Serial.println("[PUMP] ON");
    pumpActive = true;
    digitalWrite(RELAY_PIN, HIGH);
  }
}

void deactivatePump() {
  if (pumpActive) {
    Serial.println("[PUMP] OFF");
    pumpActive = false;
    digitalWrite(RELAY_PIN, LOW);
  }
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
    Serial.printf("[WiFi] Dashboard API: http://%s/api/sensors\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WiFi] Connection failed — running offline.");
    Serial.println("[WiFi] Fire detection + relay still work without network.");
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

void handleSensors() {
  addCORSHeaders();

  bool flame = (digitalRead(FLAME_PIN) == LOW);

  int cooldownLeft = 0;
  if (inCooldown && cooldownStart > 0) {
    unsigned long elapsed = (millis() - cooldownStart) / 1000;
    cooldownLeft = max(0, (int)PUMP_COOLDOWN_SEC - (int)elapsed);
  }

  StaticJsonDocument<256> doc;
  doc["flame"]          = flame;
  doc["pump_active"]    = pumpActive;
  doc["cooldown_left"]  = cooldownLeft;
  doc["flame_events"]   = flameEventCount;
  doc["uptime"]         = (millis() - startTime) / 1000;
  doc["heap"]           = ESP.getFreeHeap() / 1024;
  doc["rssi"]           = WiFi.RSSI();
  doc["alert_level"]    = flame ? 2 : (inCooldown ? 1 : 0);
  doc["timestamp"]      = millis();

  if (lastFlameTime > 0) {
    doc["last_flame_sec"] = (millis() - lastFlameTime) / 1000;
  }

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleStatus() {
  addCORSHeaders();

  StaticJsonDocument<200> doc;
  doc["device"]   = "FireGuard-ESP32";
  doc["firmware"] = "2.1.4";
  doc["ip"]       = WiFi.localIP().toString();
  doc["rssi"]     = WiFi.RSSI();
  doc["uptime"]   = (millis() - startTime) / 1000;
  doc["heap"]     = ESP.getFreeHeap() / 1024;
  doc["online"]   = true;

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
