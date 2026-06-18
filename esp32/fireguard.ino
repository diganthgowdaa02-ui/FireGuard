/**
 * FireGuard ESP32 Firmware v3.0
 *
 * Confirmed working pins (matched from your working test sketch):
 *   IR Flame Sensor DO → GPIO 5   (LOW = flame detected)
 *   Relay Module IN    → GPIO 23  (HIGH = relay OFF, LOW = relay ON)
 *   Buzzer             → GPIO 18  (HIGH = ON)
 *   Onboard LED        → GPIO 2
 *
 * 9V External Battery wiring:
 *   Battery (+) → Relay COM
 *   Relay NO    → Motor (+)
 *   Battery (-) → Motor (-)
 *   ESP32 GND   → Battery (-) ← shared ground required
 *
 * Behaviour:
 *   No flame  → relay HIGH (OFF), buzzer OFF, LED steady
 *   Flame     → relay LOW (ON), buzzer ON, LED rapid flash
 *   Flame gone → 10s cooldown → relay HIGH (OFF), buzzer OFF
 *
 * HTTP API (port 80):
 *   GET /api/sensors → live status JSON
 *   GET /api/status  → device health JSON
 *
 * Library needed: ArduinoJson v6 (Arduino Library Manager)
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// ── WiFi ──────────────────────────────────────────────────────────────────────
const char* WIFI_SSID     = "Diganth's A36";
const char* WIFI_PASSWORD = "diganth@098";

// ── Pins (confirmed from working test sketch) ─────────────────────────────────
const int FLAME_PIN  = 5;    // IR flame sensor DO
const int RELAY_PIN  = 23;   // Relay IN  (active LOW module)
const int BUZZER_PIN = 18;   // Buzzer
const int LED_PIN    = 2;    // Onboard LED

// ── Cooldown ──────────────────────────────────────────────────────────────────
#define COOLDOWN_SEC  10     // Seconds pump/buzzer keep running after flame gone

// ── State ─────────────────────────────────────────────────────────────────────
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

  // Pins
  pinMode(FLAME_PIN,  INPUT);
  pinMode(RELAY_PIN,  OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN,    OUTPUT);

  // Safe defaults — relay OFF, buzzer OFF
  digitalWrite(RELAY_PIN,  HIGH);  // HIGH = relay OFF (active LOW module)
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN,    LOW);

  Serial.println("[FireGuard] Relay OFF. Buzzer OFF. Ready.");

  connectWiFi();

  server.on("/",            handleRoot);
  server.on("/api/sensors", handleSensors);
  server.on("/api/status",  handleStatus);
  server.onNotFound([]() {
    server.send(404, "application/json", "{\"error\":\"Not found\"}");
  });

  server.begin();
  Serial.println("[FireGuard] HTTP server started.");
  startTime = millis();

  blinkLED(3, 150);  // 3 blinks = ready
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();
  checkFlameAndRelay();
  delay(500);  // same interval as your working sketch
}

// ── Core logic ────────────────────────────────────────────────────────────────
void checkFlameAndRelay() {
  int flameState = digitalRead(FLAME_PIN);
  bool flame = (flameState == LOW);  // LOW = flame detected (active low sensor)

  if (flame) {
    // ── Flame present ──
    if (!flameDetected) {
      // Rising edge — new flame event
      flameEventCount++;
      lastFlameTime = millis();
      inCooldown    = false;
      Serial.printf("[ALERT] FIRE DETECTED! Event #%d — pump & buzzer ON.\n", flameEventCount);
    } else {
      Serial.println("[ALERT] Fire ongoing — pump & buzzer ON.");
    }

    flameDetected = true;
    pumpOn();

  } else {
    // ── No flame ──
    Serial.println("[INFO]  No fire detected.");

    if (flameDetected) {
      // Falling edge — flame just went away, start cooldown
      inCooldown    = true;
      cooldownStart = millis();
      Serial.printf("[INFO]  Flame gone. Cooldown started (%d s).\n", COOLDOWN_SEC);
    }

    flameDetected = false;

    if (inCooldown) {
      unsigned long elapsed = (millis() - cooldownStart) / 1000;
      if (elapsed >= COOLDOWN_SEC) {
        inCooldown = false;
        pumpOff();
        Serial.println("[INFO]  Cooldown done. Pump & buzzer OFF.");
      } else {
        Serial.printf("[INFO]  Cooldown: %lus / %ds remaining.\n",
                      elapsed, COOLDOWN_SEC - (int)elapsed);
        pumpOn();  // keep running during cooldown
      }
    } else {
      pumpOff();
    }
  }

  // ── LED ──
  if (flameDetected) {
    digitalWrite(LED_PIN, (millis() / 200) % 2);  // fast flash = fire
  } else if (inCooldown) {
    digitalWrite(LED_PIN, (millis() / 600) % 2);  // slow flash = cooldown
  } else {
    digitalWrite(LED_PIN, WiFi.status() == WL_CONNECTED ? HIGH : LOW);
  }
}

// ── Pump + Buzzer helpers ─────────────────────────────────────────────────────
void pumpOn() {
  if (!motorRunning) {
    motorRunning = true;
    digitalWrite(RELAY_PIN,  LOW);   // LOW = relay ON (active LOW module)
    digitalWrite(BUZZER_PIN, HIGH);
    Serial.println("[PUMP]  ON");
  }
}

void pumpOff() {
  if (motorRunning) {
    motorRunning = false;
    digitalWrite(RELAY_PIN,  HIGH);  // HIGH = relay OFF
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println("[PUMP]  OFF");
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
    Serial.printf("[WiFi]  API: http://%s/api/sensors\n",
                  WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WiFi]  Failed. Relay logic still works offline.");
  }
}

// ── GET / ─────────────────────────────────────────────────────────────────────
void handleRoot() {
  addCORSHeaders();
  server.send(200, "text/plain",
    "FireGuard API v3\n"
    "  GET /api/sensors\n"
    "  GET /api/status\n"
  );
}

// ── GET /api/sensors ──────────────────────────────────────────────────────────
void handleSensors() {
  addCORSHeaders();

  bool flame = (digitalRead(FLAME_PIN) == LOW);

  int cooldownLeft = 0;
  if (inCooldown && cooldownStart > 0) {
    unsigned long elapsed = (millis() - cooldownStart) / 1000;
    cooldownLeft = max(0, COOLDOWN_SEC - (int)elapsed);
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

// ── GET /api/status ───────────────────────────────────────────────────────────
void handleStatus() {
  addCORSHeaders();

  StaticJsonDocument<200> doc;
  doc["device"]   = "FireGuard-ESP32";
  doc["firmware"] = "3.0.0";
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
