# Task 5 Technical Implementation Report
## Firebase Logging, Automation & Data Export

### Executive Summary & Technical Architecture Notice
> **IMPORTANT ARCHITECTURAL CLARIFICATION:**  
> This project is officially titled **TASK 5: Firebase Logging, Automation & Data Export**. It is an advanced single-thread, non-blocking embedded automation, cloud historical synchronization, and analytics system. It is **NOT** an RTOS or FreeRTOS implementation. All tasks execute deterministically within a single execution loop orchestrated via `millis()` delta comparisons.

### Core Parameters
- **Sensor Acquisition Loop:** Every 2,000 ms (2 seconds).
- **Historical Database Logging Loop:** Every 60,000 ms (60 seconds).
- **Light Sensor Hysteresis Model:**
  - $V_{low} = 1800$ ADC counts: Bulb turns ON.
  - $V_{high} = 2200$ ADC counts: Bulb turns OFF.
  - Deadband ($1800 \le \text{LDR} \le 2200$): Retain previous state.
- **Cloud Database:** Google Firebase Realtime Database project `Apppp` (`/current` and `/history`).
- **NTP Time Synchronization:** `pool.ntp.org` providing epoch timestamps for historical logging.
