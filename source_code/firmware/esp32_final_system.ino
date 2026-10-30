/*
 * Task 5: Firebase Autonomous Lighting, Historical Logging & Data Export
 * Hardware:
 *   - DHT11 -> GPIO 4
 *   - LDR Voltage Divider -> GPIO 34 (ADC1)
 *   - Relay / Bulb Output -> GPIO 5
 * Features:
 *   - Dual Operating Modes: AUTOMATIC (Edge LDR threshold) vs MANUAL (Cloud remote)
 *   - NTP Epoch Time Synchronization (pool.ntp.org)
 *   - Historical Log Engine: Packages JSON records & pushes to /logs every 60s
 */

#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "DHT.h"
#include <WiFiUdp.h>
#include <NTPClient.h>

#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define API_KEY       "YOUR_FIREBASE_API_KEY"
#define DATABASE_URL  "YOUR_FIREBASE_DATABASE_URL"

#define DHTPIN   4
#define DHTTYPE  DHT11
#define LDRPIN   34
#define BULBPIN  5

DHT dht(DHTPIN, DHTTYPE);

// NTP Client configuration (pool.ntp.org, GMT offset = 0 for UTC epoch)
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

FirebaseData fbdo;
FirebaseData streamData;
FirebaseAuth auth;
FirebaseConfig config;

// System State Variables
bool autoMode = true;         // True = Automatic, False = Manual
bool currentBulbState = false;
const int LDR_ON_THRESHOLD = 1800;   // Below 1800: Dark -> Bulb ON
const int LDR_OFF_THRESHOLD = 2200;  // Above 2200: Bright -> Bulb OFF

unsigned long lastSensorRead = 0;
unsigned long lastHistoryLog = 0;
const unsigned long SENSOR_INTERVAL = 2000;  // 2 seconds
const unsigned long LOG_INTERVAL = 60000;    // 60 seconds

// Cloud stream callback for mode and manual override
void streamCallback(FirebaseStream data) {
  String path = data.dataPath();
  Serial.printf("[STREAM INCOMING] Path: %s\n", path.c_str());

  if (path == "/mode") {
    String modeVal = data.stringData();
    autoMode = modeVal.equalsIgnoreCase("AUTO");
    Serial.printf("[MODE CHANGED] Operating Mode set to: %s\n", autoMode ? "AUTOMATIC" : "MANUAL");
  }
  else if (path == "/bulb" && !autoMode) {
    // Only accept manual toggle if system is in MANUAL mode
    currentBulbState = data.boolData();
    digitalWrite(BULBPIN, currentBulbState ? HIGH : LOW);
    Serial.printf("[MANUAL OVERRIDE] Relay set to: %s\n", currentBulbState ? "ON" : "OFF");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BULBPIN, OUTPUT);
  pinMode(LDRPIN, INPUT);
  digitalWrite(BULBPIN, LOW);

  dht.begin();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi Connected. Syncing NTP clock...");

  timeClient.begin();
  timeClient.update();

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  Firebase.signUp(&config, &auth, "", "");
  Firebase.begin(&config, &auth);

  // Initialize Realtime Listener for /control
  Firebase.RTDB.beginStream(&streamData, "/control");
  Firebase.RTDB.setStreamCallback(&streamData, streamCallback, nullptr);

  // Publish default initial states to cloud
  Firebase.RTDB.setString(&fbdo, "/control/mode", "AUTO");
  Firebase.RTDB.setInt(&fbdo, "/control/threshold", LDR_THRESHOLD);
}

void loop() {
  timeClient.update();
  unsigned long now = millis();

  // 1. High-frequency sensor & edge automation loop (Every 2 seconds)
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();
    int ldr = analogRead(LDRPIN);

    // Edge Decision Logic: Automatic Mode Lighting with Hysteresis (1800 ON / 2200 OFF)
    if (autoMode) {
      if (ldr < 1800 && !currentBulbState) {
        currentBulbState = true;
        digitalWrite(BULBPIN, HIGH);
        Serial.printf("[EDGE AUTO] LDR: %d (< 1800) -> Bulb turned ON\n", ldr);
        Firebase.RTDB.setBool(&fbdo, "/control/bulb", true);
      } else if (ldr > 2200 && currentBulbState) {
        currentBulbState = false;
        digitalWrite(BULBPIN, LOW);
        Serial.printf("[EDGE AUTO] LDR: %d (> 2200) -> Bulb turned OFF\n", ldr);
        Firebase.RTDB.setBool(&fbdo, "/control/bulb", false);
      }
      // Retain previous state between 1800 and 2200 to eliminate relay chatter
    }

    // Sync live sensor values
    Firebase.RTDB.setFloat(&fbdo, "/sensors/temperature", t);
    Firebase.RTDB.setFloat(&fbdo, "/sensors/humidity", h);
    Firebase.RTDB.setInt(&fbdo, "/sensors/ldr", ldr);
  }

  // 2. Historical Data Logging Loop (Every 60 seconds)
  if (now - lastHistoryLog >= LOG_INTERVAL) {
    lastHistoryLog = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();
    int ldr = analogRead(LDRPIN);
    unsigned long epoch = timeClient.getEpochTime();

    FirebaseJson json;
    json.set("timestamp", epoch);
    json.set("temperature", t);
    json.set("humidity", h);
    json.set("ldr", ldr);
    json.set("bulb", currentBulbState);
    json.set("mode", autoMode ? "AUTO" : "MANUAL");

    // Push record under /logs with unique push key
    if (Firebase.RTDB.pushJSON(&fbdo, "/logs", &json)) {
      Serial.printf("[LOGGING] Historic record saved at epoch %lu\n", epoch);
    } else {
      Serial.printf("[LOGGING ERROR] %s\n", fbdo.errorReason().c_str());
    }
  }
}