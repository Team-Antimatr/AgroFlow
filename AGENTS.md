# Contributor & Agent Guide (AGENTS.md)

Welcome to **AgroFlow AI**! Whether you are a human developer, team member, or an AI coding agent, this guide explains how the project works, how tasks are structured, and how to build and test each part of the system.

---

## 1. Project Mission & Big Picture

Many farmers unknowingly irrigate crops with high-salinity groundwater, which damages soil and burns plant roots. 

**AgroFlow AI** solves this with an automated 3-stage loop:
1. **Source Water Quality Check:** An inline TDS sensor checks water salinity (`ppm`).
2. **Smart Bio-Filter Diversion:** 
   - If salinity is high (`> 500 ppm`), an SG90 servo valve diverts water into a multi-layer Bio-Sand & Activated Charcoal filter.
   - If clean (`<= 500 ppm`), water bypasses the filter directly to save filter capacity.
3. **Precision Fertigation & Farm Delivery:** Clean water is mixed with liquid fertilizer (via dosing pump) and delivered to target farm zones (Zone 1, 2, or 3), while live telemetry streams to the Android mobile app.

---

## 2. Hardware Pinout Reference (Arduino Uno)

Keep this pin mapping consistent across all firmware and circuit simulations:

| Hardware Component | Component Pin | Arduino Uno Pin | Purpose |
| :--- | :--- | :--- | :--- |
| **TDS Water Sensor** | Signal (A) | `A0` (Analog In) | Measures salinity / Total Dissolved Solids (`ppm`) |
| **Soil Moisture Sensor** | Signal (A0) | `A1` (Analog In) | Measures soil hydration (`%`) |
| **SG90 Servo Motor** | Orange (PWM) | `Pin 3` (Digital PWM) | `0°` = Clean Bypass \| `90°` = Bio-Filter Path |
| **Relay Channel 1 (Main Pump)** | IN1 | `Pin 7` (Digital Out) | Switches 12V Submersible Lift Pump in Reservoir 1 |
| **Relay Channel 2 (Fertilizer)**| IN2 | `Pin 8` (Digital Out) | Switches 12V Dosing Pump in Reservoir 2 (`30 mL/cycle`) |
| **HC-05 Bluetooth Module** | TX / RX | `Pin 10 / Pin 11` | SoftwareSerial (9600 baud) two-way app link |

---

## 3. Project Architecture & Directory Layout

```text
AgroFlow/
├── firmware/              # Arduino Uno .ino sketches & Wokwi simulation files
│   ├── AgroFlow_Uno/
│   │   └── AgroFlow_Uno.ino
│   └── wokwi/
│       └── diagram.json
├── android/               # Android Mobile Application (Jetpack Compose / Kotlin)
│   ├── app/
│   │   └── src/main/java/com/agroflow/ai/
│   │       ├── ui/screens/        # Tab 1 (Schedule), Tab 2 (Telemetry), Tab 3 (Demo)
│   │       ├── bluetooth/         # HC-05 SPP connection manager
│   │       └── data/              # Firebase models & telemetry parser
├── cloud/                 # Firebase Cloud Functions & AI Prompt Templates
│   ├── firestore_schema.json
│   └── ai_agronomist_prompt.txt
├── docs/                  # Architecture diagrams & schematics
│   ├── AgroFlow.drawio.html       # Visual architecture viewer
│   └── AgroFlow.drawio            # Standard draw.io XML diagram
├── AGENTS.md              # This developer & agent guide
└── README.md              # Public project overview & links
```

---

## 4. Work Streams & Responsibilities

### A. Firmware & IoT Developer
- **Goal:** Write non-blocking Arduino C++ code to read sensors, trigger relays, move the servo, and output clean JSON strings over serial.
- **Rules:**
  - Do NOT use long `delay()` calls; use `millis()` timers.
  - Implement a safety check: if soil moisture is already $\ge 60\%$, abort watering to prevent root rot.
  - Format serial output as simple JSON strings so the mobile app can parse easily:
    ```json
    {"tds":850,"moisture":28,"servo":90,"pump":1,"dose":1}
    ```

### B. Mobile App Developer (Android)
- **Goal:** Build a clean 3-tab Android UI in Jetpack Compose.
  - **Tab 1 (Scheduler):** Schedule cards with a single `[START]` button + a 10-second safety disable cooldown.
  - **Tab 2 (Telemetry Dashboard):** 5 clean widgets (TDS Purity with Green/Red badge, Bio-filter status, Soil score, Mechanism pills, AI Agronomist advice).
  - **Tab 3 (Offline Demo Sandbox):** Mock sliders to demonstrate the system to judges without physical hardware.
- **Rules:**
  - Maintain a persistent Bluetooth background connection with auto-reconnect.
  - Keep UI snappy; parse telemetry on a background coroutine dispatcher.

### C. Hydraulics & Hardware Assembler
- **Goal:** Physical prototype build and leak testing.
- **Rules:**
  - Reservoir 1 = Untreated Groundwater (3–5 Liters).
  - Reservoir 2 = Liquid Fertilizer stock (500 mL).
  - Bio-filter column: Fine sand on top, granular activated charcoal in middle, gravel at bottom.
  - Ensure common ground between Arduino 5V and external 12V pump power supply.

---

## 5. Development Workflow & Git Rules

1. **Branching:**
   - Create feature branches named after the issue number: `feature/issue-1-firmware`, `feature/issue-3-app-ui`.
2. **Commit Messages:**
   - Keep commits descriptive:
     `git commit -m "feat(firmware): add TDS threshold logic and servo diversion"`
3. **Verification Before Submitting:**
   - Firmware changes must pass simulation in Wokwi.
   - App changes must build cleanly with `./gradlew assembleDebug`.
   - Never commit API keys or Firebase secret keys directly into code.

---

## 6. Key Milestones
- **Project Completion Deadline:** 25 November 2026
- **Paid Milestone Release:** 1 December 2026
