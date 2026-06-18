/**
 * FireGuard ESP32 - Minimal Direct Control
 * 
 * Stripped to absolute minimum.
 * No functions, no state variables, no cooldown.
 * Direct digitalWrite in loop only.
 * 
 * Serial Monitor @ 115200 to debug.
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

const char* WIFI_SSID     = "Diganth's A36";
const char* WIFI_PASSWORD = "diganth@098";

const int flamePin  = 5;
const int relayPin  = 23;
const int buzzerPin = 18;

int           flameEventCount = 0;
bool          motorRunning    = false;
unsigned long startTime       = 0;
unsigned long lastFlameTime   = 0;

WebServer server(80);

void setup() {
  Serial.begin(115200);
  delay(500);

  // Pins
  pinMode(flamePin,  INPUT);
  pinMode(relayPin,  OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  // Both OFF
  digitalWrite(relayPin,  HIGH);
  digitalWrite(buzzerPin, LOW);

  Serial.println("Boot: relay=HIGH(OFF) buzzer=LOW(OFF)");

  // ── HARD TEST: force relay LOW for 2 seconds ──
  // Watch if pump runs. This proves the relay/motor circuit.
  Serial.println(">>> SELF TEST: relay LOW for 2s — pump should run NOW <<<");
  digitalWrite(relayPin, LOW);
  delay(2000);
  digitalWrite(relayPin, HIGH);
  Serial.println(">>> SELF TEST done. relay HIGH (OFF) <<<");
  delay(500);

  connectWiFi();

  server.on("/api/sensors", handleSensors);
  server.on("/api/status",  handleStatus);
  server.onNotFound([]() {
    server.send(404, "application/json", "{\"error\":\"not found\"}");
  });
  server.begin();

  startTime = millis();
  Serial.println("Ready. Monitoring flame sensor...");
}

void loop() {
  server.handleClient();

  // Read flame pin directly — no debounce, no state, nothing extra
  int val = digitalRead(flamePin);

  if (val == LOW) {
    // FIRE — pull relay LOW immediately
    digitalWrite(relayPin,  LOW);   // relay ON → pump runs
    digitalWrite(buzzerPin, HIGH);  // buzzer ON

    if (!motorRunning) {
      motorRunning = true;
      flameEventCount++;
      lastFlameTime = millis();
      Serial.println(">>> FIRE! relay=LOW(ON) buzzer=HIGH(ON) <<<");
    }

  } else {
    // NO FIRE — push relay HIGH immediately
    digitalWrite(relayPin,  HIGH);  // relay OFF → pump stops
    digitalWrite(buzzerPin, LOW);   // buzzer OFF

    if (motorRunning) {
      motorRunning = false;
      Serial.println(">>> SAFE. relay=HIGH(OFF) buzzer=LOW(OFF) <<<");
    }
  }

  delay(200);
}

void connectWiFi() {
  Serial.printf("Connecting to %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int t = 0;
  while (WiFi.status() != WL_CONNECTED && t < 40) {
    delay(500); Serial.print("."); t++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nConnected: http://%s/api/sensors\n",
                  WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nNo WiFi — offline mode, relay still works.");
  }
}

void handleSensors() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-cache");

  bool flame = (digitalRead(flamePin) == LOW);

  StaticJsonDocument<200> doc;
  doc["flame"]         = flame;
  doc["pump_active"]   = motorRunning;
  doc["cooldown_left"] = 0;
  doc["flame_events"]  = flameEventCount;
  doc["uptime"]        = (millis() - startTime) / 1000;
  doc["heap"]          = ESP.getFreeHeap() / 1024;
  doc["rssi"]          = WiFi.RSSI();
  doc["alert_level"]   = flame ? 2 : 0;
  doc["timestamp"]     = millis();
  if (lastFlameTime > 0)
    doc["last_flame_sec"] = (millis() - lastFlameTime) / 1000;

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleStatus() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-cache");

  StaticJsonDocument<192> doc;
  doc["device"]   = "FireGuard-ESP32";
  doc["firmware"] = "5.0.0";
  doc["ip"]       = WiFi.localIP().toString();
  doc["rssi"]     = WiFi.RSSI();
  doc["uptime"]   = (millis() - startTime) / 1000;
  doc["heap"]     = ESP.getFreeHeap() / 1024;
  doc["online"]   = (WiFi.status() == WL_CONNECTED);

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}
