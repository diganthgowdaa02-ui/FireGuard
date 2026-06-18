/**
 * FireGuard ESP32 Firmware v6.0 — Firebase Edition
 *
 * Pins:
 *   GPIO 5  → IR Flame Sensor DO  (LOW = fire)
 *   GPIO 23 → Relay IN            (LOW = ON, HIGH = OFF)
 *   GPIO 18 → Buzzer
 *   GPIO 2  → Onboard LED
 *
 * Pushes sensor data to Firebase Realtime Database every 2 seconds.
 * Dashboard reads from Firebase — works from anywhere in the world.
 *
 * Libraries needed (Arduino Library Manager):
 *   - ArduinoJson (v6)
 *   - Firebase ESP32 Client  (by Mobizt)
 */

#include <WiFi.h>
#include <FirebaseESP32.h>
#include <ArduinoJson.h>

// ── WiFi ──────────────────────────────────────────────────────────────────────
#define WIFI_SSID     "Diganth's A36"
#define WIFI_PASSWORD "diganth@098"

// ── Firebase ──────────────────────────────────────────────────────────────────
// Paste your Firebase Realtime Database URL here (no trailing slash)
#define FIREBASE_HOST "fireguard-xxxxx-default-rtdb.firebaseio.com"
// Leave secret empty for test mode — or add your database secret here
#define FIREBASE_AUTH ""

// ── Pins ──────────────────────────────────────────────────────────────────────
const int flamePin  = 5;
const int relayPin  = 23;
const int buzzerPin = 18;
const int LED_PIN   = 2;

// ── Cooldown ──────────────────────────────────────────────────────────────────
#define COOLDOWN_SEC 10

// ── State ─────────────────────────────────────────────────────────────────────
bool          inCooldown      = false;
unsigned long cooldownStart   = 0;
bool          prevFlame       = false;
int           flameEventCount = 0;
unsigned long lastFlameTime   = 0;
unsigned long startTime       = 0;
bool          motorRunning    = false;
unsigned long lastPushTime    = 0;

FirebaseData   fbData;
FirebaseConfig fbConfig;
FirebaseAuth   fbAuth;

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[FireGuard] Booting...");

  pinMode(flamePin,  INPUT);
  pinMode(relayPin,  OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(LED_PIN,   OUTPUT);

  digitalWrite(relayPin,  HIGH);  // relay OFF
  digitalWrite(buzzerPin, LOW);
  digitalWrite(LED_PIN,   LOW);

  // WiFi
  Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());

  // Firebase
  fbConfig.host           = FIREBASE_HOST;
  fbConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&fbConfig, &fbAuth);
  Firebase.reconnectWiFi(true);
  Serial.println("[Firebase] Connected!");

  startTime = millis();
  Serial.println("[FireGuard] Ready.");
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  int  flameState = digitalRead(flamePin);
  bool fireNow    = (flameState == LOW);

  // ── Relay + Buzzer control ──
  if (fireNow) {
    digitalWrite(relayPin,  LOW);
    digitalWrite(buzzerPin, HIGH);
    digitalWrite(LED_PIN,   HIGH);
    motorRunning = true;

    if (!prevFlame) {
      flameEventCount++;
      lastFlameTime = millis();
      inCooldown    = false;
      Serial.println("[ALERT] FIRE! Pump ON.");
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
      unsigned long elapsed = (millis() - cooldownStart) / 1000;
      if (elapsed >= COOLDOWN_SEC) {
        inCooldown   = false;
        motorRunning = false;
        digitalWrite(relayPin,  HIGH);
        digitalWrite(buzzerPin, LOW);
        digitalWrite(LED_PIN,   LOW);
        Serial.println("[INFO] Cooldown done. Pump OFF.");
      }
    } else {
      digitalWrite(relayPin,  HIGH);
      digitalWrite(buzzerPin, LOW);
      motorRunning = false;
      if (!inCooldown) digitalWrite(LED_PIN, WiFi.status() == WL_CONNECTED ? HIGH : LOW);
    }
  }

  // ── Push to Firebase every 2 seconds ──
  if (millis() - lastPushTime >= 2000) {
    lastPushTime = millis();

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
    json.set("timestamp",      (int)(millis()));
    if (lastFlameTime > 0)
      json.set("last_flame_sec", (int)((millis() - lastFlameTime) / 1000));

    if (Firebase.updateNode(fbData, "/fireguard/sensors", json)) {
      Serial.println("[Firebase] Data pushed OK");
    } else {
      Serial.printf("[Firebase] Push failed: %s\n", fbData.errorReason().c_str());
    }
  }

  delay(500);
}
