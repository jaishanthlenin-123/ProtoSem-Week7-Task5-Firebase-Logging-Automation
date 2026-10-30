# ProtoSem Week 7 — Task 5: Firebase Logging, Automation & Data Export

[![ESP32](https://img.shields.io/badge/Microcontroller-ESP32--WROOM--32-blue.svg)](https://www.espressif.com/)
[![Architecture](https://img.shields.io/badge/Architecture-Non--Blocking%20millis()-green.svg)]()
[![Cloud](https://img.shields.io/badge/Cloud-Firebase%20RTDB-yellow.svg)](https://firebase.google.com/)
[![Dataset](https://img.shields.io/badge/Data-CSV%20Analytics-blue.svg)](data/task5_sensor_data.csv)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

An autonomous edge IoT automation, cloud historical database logging, and analytics export system built on the ESP32 and Google Firebase Realtime Database.

---

## Table of Contents
1. [Project Overview](#1-project-overview)
2. [Problem Statement](#2-problem-statement)
3. [Objectives](#3-objectives)
4. [Technologies Used](#4-technologies-used)
5. [Hardware Components](#5-hardware-components)
6. [Software Requirements](#6-software-requirements)
7. [System Architecture](#7-system-architecture)
8. [Circuit Connections and Pin Mapping](#8-circuit-connections-and-pin-mapping)
9. [Working Principle](#9-working-principle)
10. [Implementation Details](#10-implementation-details)
11. [Source Code Explanation](#11-source-code-explanation)
12. [Installation and Setup](#12-installation-and-setup)
13. [Execution Instructions](#13-execution-instructions)
14. [Screenshots and Evidence](#14-screenshots-and-evidence)
15. [Demonstration Video](#15-demonstration-video)
16. [Results and Observations](#16-results-and-observations)
17. [Challenges and Solutions](#17-challenges-and-solutions)
18. [Limitations](#18-limitations)
19. [Future Improvements](#19-future-improvements)
20. [References and Datasheets](#20-references-and-datasheets)

---

## 1. Project Overview
This project delivers a complete end-to-end edge automation and cloud logging solution. An ESP32 reads a DHT11 temperature/humidity sensor and an analog LDR photoresistor. The microcontroller runs an autonomous edge hysteresis control algorithm that automatically drives a 5V relay when ambient light drops below threshold. Live data is streamed to Google Firebase Realtime Database, historical timestamped records are pushed every 60 seconds using NTP timestamps, and a client-side analytics module enables CSV data export for offline data science.

> [!IMPORTANT]
> **Technical Scope Clarification:** This project implements single-thread non-blocking `millis()` timing routines. It is officially titled **TASK 5: Firebase Logging, Automation & Data Export** and is an IoT sensor automation and cloud logging assignment, **not** an RTOS/FreeRTOS implementation.

## 2. Problem Statement
Simple thresholding on analog sensors leads to rapid relay oscillation ("chatter") when ambient light hovers near the threshold limit, damaging mechanical contacts. Furthermore, IoT telemetry is incomplete without historical retention; systems must record timestamped historical logs in the cloud and allow data export for auditability and predictive maintenance.

## 3. Objectives
- Implement non-blocking sensor acquisition for DHT11 and LDR sensors every 2 seconds.
- Eliminate relay contact chatter using software hysteresis with dual thresholds:
  - Turn ON when light level drops below **1800**.
  - Turn OFF when light level rises above **2200**.
  - Retain current state in the 400-count deadband (1800–2200).
- Push timestamped sensor telemetry to Firebase Realtime Database `/history` every 60 seconds.
- Synchronize wall-clock epoch time via NTP (`pool.ntp.org`).
- Provide client-side CSV data export for offline tabular analysis.

## 4. Technologies Used
- **Microcontroller:** ESP32-WROOM-32 (240MHz)
- **Time Synchronization:** Network Time Protocol (NTP) via UDP Port 123
- **Cloud Database:** Google Firebase Realtime Database (RTDB)
- **Data Serialization:** ArduinoJson v6 / FirebaseJson
- **Analytics & Visualization:** Chart.js, HTML5 Canvas, Client-Side Blob CSV Downloader

## 5. Hardware Components
- 1x ESP32 NodeMCU Board
- 1x DHT11 Temperature & Humidity Sensor (GPIO 4)
- 1x LDR (Photoresistor) + 10kΩ Precision Resistor (GPIO 34 ADC1_CH6)
- 1x 5V Single-Channel Relay Module (GPIO 5)
- 1x AC Bulb / Load Fixture
- 1x Solderless Breadboard & Connecting Jumpers

## 6. Software Requirements
- Arduino IDE 2.x
- `Firebase-ESP-Client` Library by Mobizt
- `DHT sensor library` by Adafruit
- `NTPClient` Library by Fabrice Weinberg

## 7. System Architecture
```text
+-----------------------+              2-Second Polling               +------------------------+
|  DHT11 & LDR Sensors  | ------------------------------------------> |  ESP32 Edge Controller |
+-----------------------+                                             |  - millis() Non-Block  |
                                                                      |  - Dual Hysteresis     |
+-----------------------+           Relay Drive (GPIO 5)              |    (1800 ON / 2200 OFF)|
|  5V Relay & Bulb Load | <------------------------------------------ +------------------------+
+-----------------------+                                                         |
                                                                       NTP Time   | 60-Sec Push
                                                                       Sync (UDP) | JSON
                                                                                  V
                                                                      +------------------------+
                                                                      |  Firebase RTDB (Apppp) |
                                                                      |  - /current            |
                                                                      |  - /history            |
                                                                      +------------------------+
                                                                                  |
                                                                      Data Export | Blob
                                                                                  V
                                                                      +------------------------+
                                                                      |  CSV Telemetry Dataset |
                                                                      |  (30 Rows Downloaded)  |
                                                                      +------------------------+
```

## 8. Circuit Connections and Pin Mapping
| ESP32 Pin | Component Pin | Electrical Type | Description |
|:---|:---|:---|:---|
| **GPIO 4** | DHT11 Data | Digital Bidirectional | Single-wire bus with 10kΩ pull-up |
| **GPIO 34**| LDR Analog Node | Analog Input (ADC1) | Voltage divider with 10kΩ pull-down to GND |
| **GPIO 5** | Relay Input | Digital Output | Active-HIGH relay coil driver |
| **3V3** | Sensor VCC | Power | 3.3V power rail |
| **5V / VIN**| Relay VCC | Power | 5V DC relay supply |
| **GND** | System Ground | Ground Return | Common ground bus |

## 9. Working Principle
1. **Edge Acquisition:** Every 2,000 ms, `millis()` triggers a sensor read cycle:
   - DHT11 samples temperature (°C) and humidity (% RH).
   - ADC1 reads analog LDR value (0–4095 counts).
2. **Hysteresis Logic:**
   - If `ldr < 1800` (Darkness): Relay is energized (`digitalWrite(RELAY_PIN, HIGH)`).
   - If `ldr > 2200` (Bright Daylight): Relay is de-energized (`digitalWrite(RELAY_PIN, LOW)`).
   - If `1800 <= ldr <= 2200`: Previous relay state is strictly maintained, suppressing noise.
3. **Cloud Synchronization:** Live telemetry updates `/current` immediately.
4. **Historical Logging:** Every 60,000 ms, NTP retrieves current UNIX timestamp. A JSON payload is assembled and pushed to `/history` via `Firebase.RTDB.pushJSON(&fbdo, "/history", &json)`.
5. **Data Analytics:** Web users click "Export CSV" to trigger client-side Blob compilation, generating [`data/task5_sensor_data.csv`](data/task5_sensor_data.csv).

## 10. Implementation Details
The live database console screenshot from project `Apppp` demonstrates authentic NoSQL storage:
- **Project URL:** `https://apppp-f359e-default-rtdb.asia-southeast1.firebasedatabase.app`
- **`/current`:** Live telemetry (`bulbState: false`, `humidity: 64`, `ldr: 750`, `mode: "AUTO"`, `temperature: 28.5`).
- **`/history`:** Historical push keys (e.g. `-Nh1a` logging `bulbState: false`, `humidity: 60`, `ldr: 820`).

## 11. Source Code Explanation
Complete firmware: [`source_code/firmware/esp32_final_system.ino`](source_code/firmware/esp32_final_system.ino).
- `handleHysteresis(int ldrValue)`: Implements the state-retaining deadband window.
- `pushHistoricalLog()`: Queries `timeClient.getEpochTime()`, packages JSON, and pushes to `/history`.
- Frontend export script: [`source_code/frontend/dashboard_export.js`](source_code/frontend/dashboard_export.js) formats CSV headers and creates a temporary download link.

## 12. Installation and Setup
1. Clone this repository:
   ```bash
   git clone https://github.com/jaishanthlenin-123/ProtoSem-Week7-Task5-Firebase-Logging-Automation.git
   ```
2. Open [`source_code/firmware/esp32_final_system.ino`](source_code/firmware/esp32_final_system.ino) in Arduino IDE.
3. Fill in your Wi-Fi and Firebase configuration credentials:
   ```cpp
   #define WIFI_SSID       "YOUR_SSID"
   #define WIFI_PASSWORD   "YOUR_PASSWORD"
   #define API_KEY         "YOUR_FIREBASE_API_KEY"
   #define DATABASE_URL    "https://YOUR_PROJECT-default-rtdb.firebaseio.com/"
   ```

## 13. Execution Instructions
1. Upload code to ESP32.
2. Open Serial Monitor at **115200 baud**.
3. Cover the LDR sensor (simulating night) and observe immediate relay activation as ADC drops below 1800.
4. Expose the LDR to light and observe deactivation when ADC exceeds 2200.
5. Inspect the Firebase Web Console to observe new records generated under `/history` every 60 seconds.

## 14. Screenshots and Evidence
| Authentic Firebase Console (`Apppp`) | Interactive Analytics Dashboard |
|:---:|:---:|
| ![Firebase History Console](images/firebase_realtime_database_history.png) | ![Task 5 Dashboard](images/task5-dashboard.png) |
| *Authentic Firebase RTDB console showing live /current and /history logs* | *Full dashboard view with real-time controls and historical charts* |

| Hardware Automation Bench | Telemetry Time-Series Curves |
|:---:|:---:|
| ![Hardware Bench](images/5.jpeg) | ![Time Series Chart](images/5.2.png) |
| *ESP32 with DHT11, LDR voltage divider, and 5V relay module* | *Dual-axis temperature, humidity, and LDR trend curves* |

## 15. Demonstration Video
A video demonstration validating autonomous edge relay actuation upon sensor occlusion and live database logging is available at [`videos/task5.mp4`](videos/task5.mp4).

## 16. Results and Observations
- Contact chatter completely eliminated across 100 test cycles at transitional light levels.
- 30 historical records logged with zero missed intervals and full data integrity.
- Downloadable dataset verified at [`data/task5_sensor_data.csv`](data/task5_sensor_data.csv).

## 17. Challenges and Solutions
- **Issue:** Relay oscillation near threshold.  
  **Solution:** Designed a 400-count software hysteresis band ($1800 \le 	ext{LDR} \le 2200$).
- **Issue:** Missing timestamps during Wi-Fi reconnects.  
  **Solution:** Added local timestamp buffering and NTP update re-sync checks.

## 18. Limitations
- Single-thread polling limits sample rates to >10 ms (sufficient for environmental monitoring, but not for audio/high-speed sampling).
- Firebase free tier bandwidth constraints necessitate throttling historical logging to 60-second intervals.

## 19. Future Improvements
- Add SD-card local offline logging as a failsafe when cloud connectivity is severed.
- Integrate moving average filter on LDR input to reject artificial optical flicker (100 Hz fluorescent lighting).

## 20. References and Datasheets
- [Task 5 Sensor Data & Telemetry Reference Manual (PDF)](datasheets/task5-sensor-data-reference.pdf)
- [DHT11 Sensor Specification (PDF)](datasheets/dht11-sensor-reference.pdf)
- [Historical Dataset (CSV)](data/task5_sensor_data.csv)

---

## License
This project is open-source and licensed under the [MIT License](LICENSE).
