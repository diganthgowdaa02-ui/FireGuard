/**
 * FireGuard ESP32 Firmware v6.0 — Firebase Edition
 *
 * Firebase DB: https://fireguard-dfb77-default-rtdb.firebaseio.com
 *
 * Pins:
 *   GPIO 5  → IR Flame Sensor DO  (LOW = fire)
 *   GPIO 23 → Relay IN            (LOW = ON, HIGH = OFF)
 *   GPIO 18 → Buzzer
 *   GPIO 2  → Onboard LED
 *
 * Library needed in Arduino IDE (Library Manager):
 *   → Firebase ESP32 Client  by Mobizt
 *   → ArduinoJson            by Benoit Blanchon (v6)
 */

#include <WiFi.h>
#include <FirebaseESP32.h>

// ── WiFi ──────────────────────────────────────────────────────────────────────
#define WIFI_SSID      "Diganth's A36"
#define WIFI_PASSWORD  "diganth@098"

// ── Firebase ──────────────────────────────────────────────────────────────────
// Only the host — no https://, no trailing slash
#define DATABASE_HOST  "fireguard-dfb77-default-rtdb.firebaseio.com"
#define DATABASE_AUTH  ""   // empty = test mode (rules allow read/write)

// ── Pins ──────────────────────────────────────────────────────────────────────
const int flamePin  = 5;
const int relayPin  = 23;
const int buzzerPin = 18;
const int ledPin    = 2;

// ── Cooldown ──────────────────────────────────────────────────────────────────
#define COOLDOWN_SEC  10

// ── State ─────────────────────────────────────────────────────────────────────
FirebaseData   fbdo;
FirebaseAuth   auth;
FirebaseConfig config;

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

  // ── Pins ──
  pinMode(flamePin,  INPUT);
  pinMode(relayPin,  OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(ledPin,    OUTPUT);

  digitalWrite(relayPin,  HIGH);  // relay OFF
  digitalWrite(buzzerPin, LOW);
  digitalWrite(ledPin,    LOW);
  Serial.println("[PINS]    relay=HIGH(OFF) buzzer=LOW(OFF)");

  // ── WiFi ──
  Serial.printf("[WiFi]    Connecting to %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500); Serial.print("."); tries++;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n[WiFi]    FAILED — restarting in 5s");
    delay(5000);
    ESP.restart();
  }
  Serial.printf("\n[WiFi]    Connected! IP: %s\n",
                WiFi.localIP().toString().c_str());

  // ── Firebase ── legacy begin — works with all Mobizt library versions
  Firebase.begin(DATABASE_HOST, DATABASE_AUTH);
  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(1024);

  Serial.println("[Firebase] Initialised → " DATABASE_HOST);
  Serial.println("====================================");

  startTime = millis();
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  bool fireNow = (digitalRead(flamePin) == LOW);

  // ── Relay / Buzzer logic ──────────────────────────────────────────────────
  if (fireNow) {
    // Fire detected
    digitalWrite(relayPin,  LOW);    // pump ON
    digitalWrite(buzzerPin, HIGH);   // buzzer ON
    digitalWrite(ledPin,    HIGH);
    motorRunning = true;

    if (!prevFlame) {
      flameEventCount++;
      lastFlameTime = millis();
      inCooldown    = false;
      Serial.printf("[ALERT]   FIRE #%d — pump ON, buzzer ON\n", flameEventCount);
    }
    prevFlame = true;

  } else {
    // No fire
    if (prevFlame) {
      // Flame just disappeared → start cooldown
      inCooldown    = true;
      cooldownStart = millis();
      Serial.printf("[INFO]    Flame gone → cooldown %ds started\n", COOLDOWN_SEC);
    }
    prevFlame = false;

    if (inCooldown) {
      unsigned long elapsed = (millis() - cooldownStart) / 1000;
      if (elapsed >= COOLDOWN_SEC) {
        inCooldown   = false;
        motorRunning = false;
        digitalWrite(relayPin,  HIGH);   // pump OFF
        digitalWrite(buzzerPin, LOW);    // buzzer OFF
        digitalWrite(ledPin,    LOW);
        Serial.println("[INFO]    Cooldown done — pump OFF");
      }
      // else: keep pump running during cooldown
    } else {
      // Safe, no cooldown
      digitalWrite(relayPin,  HIGH);
      digitalWrite(buzzerPin, LOW);
      digitalWrite(ledPin,    WiFi.status() == WL_CONNECTED ? HIGH : LOW);
      motorRunning = false;
    }
  }

  // ── Push to Firebase every 2 seconds ─────────────────────────────────────
  if (millis() - lastPushMillis >= 2000) {
    lastPushMillis = millis();

    int cooldownLeft = 0;
    if (inCooldown) {
      unsigned long e = (millis() - cooldownStart) / 1000;
      cooldownLeft = max(0, COOLDOWN_SEC - (int)e);
    }

    // Build JSON object
    FirebaseJson json;
    json.set("flame",         fireNow);
    json.set("pump_active",   motorRunning);
    json.set("cooldown_left", cooldownLeft);
    json.set("flame_events",  flameEventCount);
    json.set("alert_level",   fireNow ? 2 : (inCooldown ? 1 : 0));
    json.set("uptime",        (int)((millis() - startTime) / 1000));
    json.set("heap",          (int)(ESP.getFreeHeap() / 1024));
    json.set("rssi",          (int)WiFi.RSSI());
    json.set("timestamp",     (int)(millis() / 1000));
    if (lastFlameTime > 0)
      json.set("last_flame_sec", (int)((millis() - lastFlameTime) / 1000));

    // Push to /fireguard/sensors in the database
    if (Firebase.updateNode(fbdo, "/fireguard/sensors", json)) {
      Serial.printf("[Firebase] Pushed OK — flame=%s pump=%s\n",
                    fireNow ? "YES" : "no",
                    motorRunning ? "ON" : "off");
    } else {
      Serial.printf("[Firebase] Push FAILED: %s\n",
                    fbdo.errorReason().c_str());
    }
  }

  delay(500);
}
