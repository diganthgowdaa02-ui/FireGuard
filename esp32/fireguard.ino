/**
 * FireGuard ESP32 Firmware
 *
 * Wiring:
 *   IR Flame Sensor DO  → GPIO 27  (LOW = flame)
 *   Relay Module IN     → GPIO 26
 *   Onboard LED         → GPIO 2
 *
 *   9V External Battery:
 *     Battery (+) → Relay COM
 *     Relay NO    → Motor (+)
 *     Battery (-) → Motor (-)
 *     (ESP32 GND and Battery GND must share a common ground)
 *
 * Relay type: most modules are ACTIVE LOW
 *   RELAY_ON  = LOW  → relay coil energised → NO closes → motor runs
 *   RELAY_OFF = HIGH → relay coil off       → NO open   → motor stops
 *
 *   If your relay is ACTIVE HIGH, flip RELAY_ON/RELAY_OFF below.
 *
 * Behaviour:
 *   No flame  → relay OFF (motor stopped) — guaranteed even at boot
 *   Flame     → relay ON  (motor runs)
 *   Flame gone → 10 s cooldown → relay OFF
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// ── WiFi ──────────────────────────────────────────────────────────────────────
const char* WIFI_SSID     = "Diganth's A36";
const char* WIFI_PASSWORD = "diganth@098";

// ── Pins ──────────────────────────────────────────────────────────────────────
#define FLAME_PIN         27
#define RELAY_PIN         26
#define LED_PIN           2

// ── Relay polarity ────────────────────────────────────────────────────────────
// Most relay modules are ACTIVE LOW — change to HIGH/LOW if yours is active HIGH
#define RELAY_ON          LOW    // Signal that energises the relay coil
#define RELAY_OFF         HIGH   // Signal that de-energises the relay coil

// ── Cooldown ──────────────────────────────────────────────────────────────────
#define COOLDOWN_SEC      10     // Seconds motor keeps running after flame gone

// ── Globals ───────────────────────────────────────────────────────────────────
WebServer     server(80);

bool          flameDetected   = false;
bool          motorRunning    = false;
bool          inCooldown      = false;
unsigned long cooldownStart   = 0;
unsigned long startTime       = 0;
unsigned long lastFlameTime   = 0;
int           flameEventCount = 0;

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n[FireGuard] Booting...");

  // ── Set relay OFF before anything else ──
  // Do this BEFORE pinMode so the pin is never in a floating state
  digitalWrite(RELAY_PIN, RELAY_OFF);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);   // write again after pinMode to be sure

  pinMode(FLAME_PIN, INPUT);
  pinMode(LED_PIN,   OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("[FireGuard] Relay initialised OFF. Motor is stopped.");

  connectWiFi();

  server.on("/",            handleRoot);
  server.on("/api/sensors", handleSensors);
  server.on("/api/status",  handleStatus);
  server.onNotFound([]() {
    server.send(404, "application/json", "{\"error\":\"Not found\"}");
  });

  server.begin();
  Serial.println("[FireGuard] HTTP server ready.");
  startTime = millis();

  blinkLED(3, 150);   // 3 blinks = system ready
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();
  checkFlameAndRelay();
  delay(50);
}

// ── Core logic ────────────────────────────────────────────────────────────────
void checkFlameAndRelay() {
  bool flame = (digitalRead(FLAME_PIN) == LOW);   // active-low sensor

  // ── Rising edge: flame just appeared ──
  if (flame && !flameDetected) {
    flameEventCount++;
    lastFlameTime = millis();
    inCooldown    = false;   // discard any prior cooldown
    Serial.printf("[ALERT] Flame detected! Event #%d\n", flameEventCount);
    motorOn();
  }

  // ── Falling edge: flame just disappeared ──
  if (!flame && flameDetected) {
    inCooldown    = true;
    cooldownStart = millis();
    Serial.printf("[INFO]  Flame gone. Cooldown started (%d s).\n", COOLDOWN_SEC);
  }

  flameDetected = flame;

  // ── Steady state ──
  if (flame) {
    motorOn();                       // keep motor on while flame is present

  } else if (inCooldown) {
    unsigned long elapsed = (millis() - cooldownStart) / 1000;
    if (elapsed >= COOLDOWN_SEC) {
      inCooldown = false;
      motorOff();
      Serial.println("[INFO]  Cooldown complete. Motor OFF.");
    }
    // else: motor stays on, nothing to do

  } else {
    motorOff();                      // no flame, no cooldown → motor must be off
  }

  // ── LED ──
  if (flameDetected) {
    digitalWrite(LED_PIN, (millis() / 200) % 2);   // rapid flash = active fire
  } else if (inCooldown) {
    digitalWrite(LED_PIN, (millis() / 600) % 2);   // slow flash = cooldown
  } else {
    // Steady ON if WiFi connected, slow blink if not
    if (WiFi.status() == WL_CONNECTED) {
      digitalWrite(LED_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, (millis() / 1000) % 2);
    }
  }
}

// ── Motor helpers ─────────────────────────────────────────────────────────────
void motorOn() {
  if (!motorRunning) {
    motorRunning = true;
    digitalWrite(RELAY_PIN, RELAY_ON);
    Serial.println("[MOTOR] ON  — relay energised, 9V circuit closed.");
  }
}

void motorOff() {
  if (motorRunning) {
    motorRunning = false;
    digitalWrite(RELAY_PIN, RELAY_OFF);
    Serial.println("[MOTOR] OFF — relay de-energised, 9V circuit open.");
  }
}

// ── WiFi ──────────────────────────────────────────────────────────────────────
void connectWiFi() {
  Serial.printf("[WiFi]  Connecting to '%s'", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500);
    Serial.print(".");
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi]  Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WiFi]  API: http://%s/api/sensors\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WiFi]  Failed. Running offline — relay logic still works.");
  }
}

// ── HTTP: GET / ───────────────────────────────────────────────────────────────
void handleRoot() {
  addCORSHeaders();
  server.send(200, "text/plain",
    "FireGuard API v2\n"
    "  GET /api/sensors\n"
    "  GET /api/status\n"
  );
}

// ── HTTP: GET /api/sensors ────────────────────────────────────────────────────
void handleSensors() {
  addCORSHeaders();

  bool flame = (digitalRead(FLAME_PIN) == LOW);

  int cooldownLeft = 0;
  if (inCooldown && cooldownStart > 0) {
    unsigned long elapsed = (millis() - cooldownStart) / 1000;
    cooldownLeft = max(0, (int)COOLDOWN_SEC - (int)elapsed);
  }

  StaticJsonDocument<256> doc;
  doc["flame"]          = flame;
  doc["pump_active"]    = motorRunning;
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

// ── HTTP: GET /api/status ─────────────────────────────────────────────────────
void handleStatus() {
  addCORSHeaders();

  StaticJsonDocument<200> doc;
  doc["device"]   = "FireGuard-ESP32";
  doc["firmware"] = "2.2.0";
  doc["ip"]       = WiFi.localIP().toString();
  doc["rssi"]     = WiFi.RSSI();
  doc["uptime"]   = (millis() - startTime) / 1000;
  doc["heap"]     = ESP.getFreeHeap() / 1024;
  doc["online"]   = (WiFi.status() == WL_CONNECTED);

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
