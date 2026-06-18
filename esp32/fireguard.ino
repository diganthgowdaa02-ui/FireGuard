/**
 * FireGuard ESP32 Firmware v7.0 — Firebase REST API (no library needed)
 *
 * Uses HTTPClient to POST directly to Firebase REST API.
 * No Firebase library required — just WiFi + HTTPClient (built into ESP32).
 *
 * Firebase DB: fireguard-dfb77-default-rtdb.firebaseio.com
 *
 * Pins:
 *   GPIO 5  → IR Flame Sensor DO  (LOW = fire)
 *   GPIO 23 → Relay IN            (LOW = ON, HIGH = OFF)
 *   GPIO 18 → Buzzer
 *   GPIO 2  → Onboard LED
 *
 * No extra libraries needed — HTTPClient and WiFi are built into ESP32 core.
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// ── WiFi ──────────────────────────────────────────────────────────────────────
#define WIFI_SSID      "Diganth's A36"
#define WIFI_PASSWORD  "diganth@098"

// ── Firebase REST endpoint ────────────────────────────────────────────────────
// Format: https://<project>-default-rtdb.firebaseio.com/<path>.json
#define FIREBASE_URL   "https://fireguard-dfb77-default-rtdb.firebaseio.com/fireguard/sensors.json"

// ── Pins ──────────────────────────────────────────────────────────────────────
const int flamePin  = 5;
const int relayPin  = 23;
const int buzzerPin = 18;
const int ledPin    = 2;

// ── Cooldown ──────────────────────────────────────────────────────────────────
#define COOLDOWN_SEC  10

// ── State ─────────────────────────────────────────────────────────────────────
bool          prevFlame       = false;
bool          inCooldown      = false;
unsigned long cooldownStart   = 0;
bool          motorRunning    = false;
int           flameEventCount = 0;
unsigned long lastFlameTime   = 0;
unsigned long startTime       = 0;
unsigned long lastPushMillis  = 0;

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n========== FireGuard Boot ==========");

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
  Serial.printf("\n[WiFi] Connected! IP: %s\n",
                WiFi.localIP().toString().c_str());
  Serial.println("[FireGuard] Ready — pushing to Firebase via REST");
  Serial.println("====================================");

  startTime = millis();
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  bool fireNow = (digitalRead(flamePin) == LOW);

  // ── Relay / Buzzer ────────────────────────────────────────────────────────
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
      Serial.printf("[INFO]  Flame gone — cooldown %ds\n", COOLDOWN_SEC);
    }
    prevFlame = false;

    if (inCooldown) {
      unsigned long elapsed = (millis() - cooldownStart) / 1000;
      if (elapsed >= COOLDOWN_SEC) {
        inCooldown   = false;
        motorRunning = false;
        digitalWrite(relayPin,  HIGH);
        digitalWrite(buzzerPin, LOW);
        digitalWrite(ledPin,    LOW);
        Serial.println("[INFO]  Cooldown done — Pump OFF");
      }
    } else {
      digitalWrite(relayPin,  HIGH);
      digitalWrite(buzzerPin, LOW);
      digitalWrite(ledPin,    HIGH);
      motorRunning = false;
    }
  }

  // ── Push to Firebase every 2 seconds via REST API ─────────────────────────
  if (millis() - lastPushMillis >= 2000) {
    lastPushMillis = millis();
    pushToFirebase(fireNow);
  }

  delay(500);
}

// ── Firebase REST push ────────────────────────────────────────────────────────
void pushToFirebase(bool fireNow) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Firebase] WiFi not connected — skipping push");
    return;
  }

  int cooldownLeft = 0;
  if (inCooldown) {
    unsigned long e = (millis() - cooldownStart) / 1000;
    cooldownLeft = max(0, COOLDOWN_SEC - (int)e);
  }

  // Build JSON manually — no library needed
  String json = "{";
  json += "\"flame\":"         + String(fireNow ? "true" : "false") + ",";
  json += "\"pump_active\":"   + String(motorRunning ? "true" : "false") + ",";
  json += "\"cooldown_left\":" + String(cooldownLeft) + ",";
  json += "\"flame_events\":"  + String(flameEventCount) + ",";
  json += "\"alert_level\":"   + String(fireNow ? 2 : (inCooldown ? 1 : 0)) + ",";
  json += "\"uptime\":"        + String((millis() - startTime) / 1000) + ",";
  json += "\"heap\":"          + String(ESP.getFreeHeap() / 1024) + ",";
  json += "\"rssi\":"          + String(WiFi.RSSI()) + ",";
  json += "\"timestamp\":"     + String(millis() / 1000);
  if (lastFlameTime > 0) {
    json += ",\"last_flame_sec\":" + String((millis() - lastFlameTime) / 1000);
  }
  json += "}";

  // Use PATCH (not PUT) so we update fields without overwriting the whole node
  WiFiClientSecure client;
  client.setInsecure();   // skip SSL cert verification (fine for IoT)

  HTTPClient http;
  http.begin(client, FIREBASE_URL);
  http.addHeader("Content-Type", "application/json");

  // Firebase REST uses PATCH to update specific fields
  int httpCode = http.sendRequest("PATCH", json);

  if (httpCode == 200) {
    Serial.printf("[Firebase] OK — flame=%s pump=%s\n",
                  fireNow ? "YES" : "no",
                  motorRunning ? "ON" : "off");
  } else {
    Serial.printf("[Firebase] FAIL — HTTP %d: %s\n",
                  httpCode, http.getString().c_str());
  }

  http.end();
}
