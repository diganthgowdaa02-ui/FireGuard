/**
 * FireGuard ESP32 Firmware v6.1 — Firebase Edition
 *
 * Firebase DB: fireguard-dfb77-default-rtdb.firebaseio.com
 *
 * Pins:
 *   GPIO 5  → IR Flame Sensor DO  (LOW = fire)
 *   GPIO 23 → Relay IN            (LOW = ON, HIGH = OFF)
 *   GPIO 18 → Buzzer
 *   GPIO 2  → Onboard LED
 *
 * Libraries needed (Arduino Library Manager):
 *   - Firebase ESP32 Client  by Mobizt  (v4.x)
 *   - ArduinoJson            by Benoit Blanchon (v6)
 */

#include <WiFi.h>
#include <FirebaseESP32.h>

// ── WiFi ──────────────────────────────────────────────────────────────────────
#define WIFI_SSID      "Diganth's A36"
#define WIFI_PASSWORD  "diganth@098"

// ── Firebase ──────────────────────────────────────────────────────────────────
#define DATABASE_URL   "https://fireguard-dfb77-default-rtdb.firebaseio.com"

// ── Pins ──────────────────────────────────────────────────────────────────────
const int flamePin  = 5;
const int relayPin  = 23;
const int buzzerPin = 18;
const int ledPin    = 2;

// ── Cooldown ──────────────────────────────────────────────────────────────────
#define COOLDOWN_SEC  10

// ── Firebase objects ──────────────────────────────────────────────────────────
FirebaseData   fbdo;
FirebaseAuth   auth;
FirebaseConfig config;

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

  // ── Pins ──
  pinMode(flamePin,  INPUT);
  pinMode(relayPin,  OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(ledPin,    OUTPUT);

  digitalWrite(relayPin,  HIGH);
  digitalWrite(buzzerPin, LOW);
  digitalWrite(ledPin,    LOW);
  Serial.println("[PINS]    Relay OFF, Buzzer OFF");

  // ── WiFi ──
  Serial.printf("[WiFi]    Connecting to %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500); Serial.print("."); tries++;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n[WiFi]    FAILED — restarting...");
    delay(3000);
    ESP.restart();
  }
  Serial.printf("\n[WiFi]    Connected! IP: %s\n",
                WiFi.localIP().toString().c_str());

  // ── Firebase (v4.x API) ──
  config.database_url = DATABASE_URL;

  // Anonymous sign-in for test mode database
  auth.user.email    = "";
  auth.user.password = "";

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(2048);

  // Wait for Firebase to be ready (max 10 seconds)
  Serial.print("[Firebase] Connecting");
  unsigned long fbStart = millis();
  while (!Firebase.ready() && millis() - fbStart < 10000) {
    delay(300); Serial.print(".");
  }
  if (Firebase.ready()) {
    Serial.println("\n[Firebase] Ready! → " DATABASE_URL);
  } else {
    Serial.println("\n[Firebase] Timeout — will retry in loop");
  }
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
      Serial.printf("[ALERT]   FIRE #%d — Pump ON\n", flameEventCount);
    }
    prevFlame = true;

  } else {
    if (prevFlame) {
      inCooldown    = true;
      cooldownStart = millis();
      Serial.printf("[INFO]    Flame gone — cooldown %ds\n", COOLDOWN_SEC);
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
        Serial.println("[INFO]    Cooldown done — Pump OFF");
      }
    } else {
      digitalWrite(relayPin,  HIGH);
      digitalWrite(buzzerPin, LOW);
      digitalWrite(ledPin,    WiFi.status() == WL_CONNECTED ? HIGH : LOW);
      motorRunning = false;
    }
  }

  // ── Push to Firebase every 2 seconds ─────────────────────────────────────
  if (millis() - lastPushMillis >= 2000) {
    lastPushMillis = millis();

    if (Firebase.ready()) {
      int cooldownLeft = 0;
      if (inCooldown) {
        unsigned long e = (millis() - cooldownStart) / 1000;
        cooldownLeft = max(0, COOLDOWN_SEC - (int)e);
      }

      FirebaseJson json;
      json.set("flame",          fireNow);
      json.set("pump_active",    motorRunning);
      json.set("cooldown_left",  cooldownLeft);
      json.set("flame_events",   flameEventCount);
      json.set("alert_level",    fireNow ? 2 : (inCooldown ? 1 : 0));
      json.set("uptime",         (int)((millis() - startTime) / 1000));
      json.set("heap",           (int)(ESP.getFreeHeap() / 1024));
      json.set("rssi",           (int)WiFi.RSSI());
      json.set("timestamp",      (int)(millis() / 1000));
      if (lastFlameTime > 0)
        json.set("last_flame_sec", (int)((millis() - lastFlameTime) / 1000));

      if (Firebase.updateNode(fbdo, "/fireguard/sensors", json)) {
        Serial.printf("[Firebase] OK — flame=%s pump=%s\n",
                      fireNow ? "YES" : "no",
                      motorRunning ? "ON" : "off");
      } else {
        Serial.printf("[Firebase] FAIL: %s\n", fbdo.errorReason().c_str());
      }
    } else {
      Serial.println("[Firebase] Not ready yet...");
    }
  }

  delay(500);
}
