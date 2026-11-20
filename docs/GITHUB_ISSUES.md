# AgroFlow AI — Project Issues & Action Items

This document contains the 6 structured, execution-ready issues designed for team members and contributors.

---

### Issue 1: [Firmware] Arduino Uno code for sensors, servo diverter & pump relays
**Labels:** `firmware`, `hardware`

#### 🎯 Goal
Write the Arduino Uno firmware sketch (`.ino`) that reads the water quality and controls the pumps and diverter valve.

#### 📌 Arduino Uno Pin Reference
- `A0` (Analog In): **TDS Water Sensor** (reads salinity ppm)
- `A1` (Analog In): **Soil Moisture Sensor** (reads soil moisture %)
- `Pin 3` (PWM): **SG90 Servo Motor** (0° = Clean Bypass, 90° = Bio-Filter Path)
- `Pin 7` (Digital Out): **Relay 1** (Reservoir 1 Submersible Lift Pump)
- `Pin 8` (Digital Out): **Relay 2** (Reservoir 2 Fertilizer Dosing Pump)
- `Pin 10 / Pin 11`: **HC-05 Bluetooth** (SoftwareSerial RX/TX at 9600 baud)

#### 📋 Steps to Execute
1. [ ] Create `firmware/AgroFlow_Uno/AgroFlow_Uno.ino`.
2. [ ] Read `A0` and convert voltage into TDS ppm value.
3. [ ] If TDS > 500 ppm, turn SG90 servo to **90°** (route water through the Bio-Sand filter). If TDS <= 500 ppm, keep servo at **0°** (bypass filter).
4. [ ] Turn ON Relay 1 (Pin 7) to start water flow.
5. [ ] Turn ON Relay 2 (Pin 8) for a timed pulse to inject ~30 mL of liquid fertilizer.
6. [ ] Safety rule: If soil moisture (`A1`) is already >= 60%, do not run the pump.
7. [ ] Send clean JSON string to Bluetooth every 1 second:
   ```json
   {"tds":850,"moisture":28,"servo":90,"pump":1,"dose":1}
   ```
8. [ ] Test the logic on Wokwi virtual simulator first.

#### ✅ How to Test (Definition of Done)
- Turning the TDS potentiometer above 500 ppm rotates the servo to 90°.
- Turning it below 500 ppm keeps it at 0°.
- Relay 1 and Relay 2 turn ON and OFF cleanly without freezing the Arduino.

---

### Issue 2: [Hydraulics] Build physical dual-reservoir, servo valve & bio-sand filter
**Labels:** `hydraulics`, `hardware`, `exhibition`

#### 🎯 Goal
Assemble the physical plumbing, water containers, servo diverter valve, and 3-stage bio-sand filter.

#### 📋 Steps to Execute
1. [ ] **Reservoir 1 (Groundwater):** Set up a 3 to 5 Liter transparent container for untreated groundwater. Place the 12V submersible lift pump inside.
2. [ ] **TDS Sensor Chamber:** Fit the TDS probe into an in-line tee-pipe right after the main pump output.
3. [ ] **SG90 Diverter Valve:** Connect the SG90 servo motor horn to a 2-way valve:
   - 0° position directs water straight to bypass.
   - 90° position diverts water into the bio-filter column.
4. [ ] **Bio-Sand & Charcoal Filter Column:** Fill a clear cylinder with 3 layers:
   - Bottom: Coarse gravel and drain mesh
   - Middle: Granular activated charcoal (removes salts and smell)
   - Top: Fine silica sand (catches dirt and suspended particles)
5. [ ] **Reservoir 2 (Fertilizer):** Set up a 500 mL bottle with the dosing pump connected to the clean water line.
6. [ ] **Farm Zone Drip Header:** Connect tubing to 3 small plant zones (Zone 1, Zone 2, Zone 3) with a soil moisture sensor in the soil.
7. [ ] Run water leak test and check flow rate (~1.5 to 2.0 L/min).

#### ✅ How to Test (Definition of Done)
- No water leaks in tubes or joints.
- Moving the servo cleanly redirects water between the filter path and bypass path.
- Saline test water passed through the charcoal filter shows reduced TDS.

---

### Issue 3: [Android App] Tab 1 - AI Schedule Cards & Bluetooth START Button
**Labels:** `mobile-app`, `ui-ux`

#### 🎯 Goal
Build Tab 1 in the Android app (Jetpack Compose / Kotlin) to display daily schedule cards and a one-tap START button to wirelessly trigger the hardware.

#### 📋 Steps to Execute
1. [ ] **Bluetooth Connection (HC-05):**
   - Implement Bluetooth SPP connection to pair with `HC-05`.
   - Show connection status bar at top (Connected / Disconnected).
2. [ ] **AI Daily Schedule Cards:**
   - Display cards for **Zone 1** (Morning 07:30 AM), **Zone 2** (11:30 AM), and **Zone 3** (Evening 05:30 PM).
   - Each card displays crop name, recommended water volume, and fertilizer dose.
3. [ ] **One-Tap Mechanism Activation:**
   - Add a green **[START]** button on each card.
   - When tapped, sends string `CMD_START_Z1` via Bluetooth to the Arduino Uno.
4. [ ] **Safety Cooldown Guard:**
   - Once a cycle starts, show an active countdown timer.
   - When cycle ends, disable the card for **10 seconds** cooldown before it can be tapped again.
5. [ ] **Custom Schedule Builder:**
   - Add a `+ Add Custom Schedule` modal with a time picker (AM/PM) and zone dropdown.

#### ✅ How to Test (Definition of Done)
- App connects to HC-05 via Bluetooth.
- Clicking [START] sends command over Bluetooth.
- Button disables for 10 seconds after cycle to protect the pump relays.

---

### Issue 4: [Android App] Tab 2 - Live Telemetry Dashboard & 5 Diagnostic Cards
**Labels:** `mobile-app`, `ui-ux`

#### 🎯 Goal
Build Tab 2 ('AgroTDS') in the Android app to receive live sensor data and display the 5 real-time diagnostic cards.

#### 📋 The 5 Cards to Build:
1. [ ] **Card 1: Water Purity & Salinity Diagnostic**
   - Shows TDS reading (e.g. `850 ppm`).
   - Dynamic Color Badge: **Green (< 500 ppm Safe)** | **Red (> 500 ppm High Salinity)**.
   - Shows crop risk text (*High risk: potential root burn*).
2. [ ] **Card 2: Bio-Filtration Performance**
   - Shows Diverter Gate: `DIVERTED TO FILTER (90°)` or `DIRECT BYPASS (0°)`.
   - Shows Filtration Flow Rate (`1.8 L/min`).
   - Shows Purification Efficiency (`76% Impurity Reduction`).
3. [ ] **Card 3: AI Soil Maturation & Fertility Score**
   - Progress bar for Soil Moisture (`28%`, Dry -> Optimal -> Wet).
   - AI Soil Maturation Score (`64 / 100`).
4. [ ] **Card 4: Live Hardware Status Pills**
   - Servo Status Pill (`OPEN 90°`).
   - Main Pump Pill (`RUNNING ON`).
   - Fertilizer Dosing Pump Pill (`DOSING 30 mL`).
5. [ ] **Card 5: AI Agronomist Summary**
   - Displays real-time natural language recommendation text.
   - Next auto-adjusted schedule timestamp.

#### 📋 Tab 3 (Offline Demo Mode):
- [ ] Add Tab 3 that clones Tab 2 with test sliders (TDS slider, moisture slider) so judges can test the app without physical hardware.

#### ✅ How to Test (Definition of Done)
- Incoming JSON telemetry parses in real-time without app lag.
- Badge turns red when TDS > 500 ppm and green when <= 500 ppm.
- Tab 3 sliders allow offline interactive testing for exhibition demo.

---

### Issue 5: [Cloud & AI] Firebase setup & AI Agronomist summary generator
**Labels:** `backend`, `ai-cloud`

#### 🎯 Goal
Set up Firebase to store telemetry history and integrate the AI prompt for automatic agronomist advice generation.

#### 📋 Steps to Execute
1. [ ] **Firebase Setup:**
   - Create Firebase project for AgroFlow.
   - Set up Firestore collections:
     - `telemetry_logs`: stores timestamp, TDS, soil moisture, and pump status.
     - `schedules`: stores AI and user-defined irrigation slots.
2. [ ] **AI Agronomist Prompt Integration:**
   - Create a lightweight LLM inference function (Gemini / Cloud Function):
   - Input: `{ tds: 850, soil_moisture: 28, zone: "Zone 1" }`
   - Output expected (concise, under 50 words):
     > *"Source water salinity is elevated (850 ppm). Water has been automatically routed through the bio-filter before adding nutrients. Soil moisture is 28%; cycle will restore optimal hydration in 45s without salt buildup."*
3. [ ] Connect Android app to read the AI summary and sync telemetry.

#### ✅ How to Test (Definition of Done)
- New telemetry data is visible in Firebase Firestore.
- Calling the AI function returns a clean, helpful agronomist message for the app card.

---

### Issue 6: [Exhibition] Presentation slides, printable hardware labels & demo rehearsal
**Labels:** `exhibition`, `documentation`

#### 🎯 Goal
Prepare all presentation materials, physical hardware labels, and rehearse the live demonstration for the project exhibition.

#### ⏰ Deadlines
- **Project Completion Deadline:** 25 November 2026
- **Paid Milestone Release:** 1 December 2026

#### 📋 Steps to Execute
1. [ ] **Printable Hardware Labels (Canva):**
   - Design and print clear labels for physical components:
     - `Reservoir 1: Untreated Groundwater`
     - `TDS Salinity Sensor Cell`
     - `SG90 Automatic Diverter Valve (0° / 90°)`
     - `Multi-Layer Bio-Sand & Charcoal Filter Bed`
     - `Reservoir 2: Liquid NPK Fertilizer`
     - `Arduino Uno IoT Processing Unit`
2. [ ] **Exhibition Pitch Deck (Canva):**
   - Slide 1: Problem (salinity destroys crops and soil).
   - Slide 2: AgroFlow solution (automatic detection + bio-sand diversion + precision fertigation).
   - Slide 3: Hardware architecture & pinout.
   - Slide 4: Mobile App & AI dashboard overview.
3. [ ] **Live Demo Rehearsal:**
   - Rehearse the 2-minute pitch:
     Tap [START] on app -> Main pump runs -> Saline water detected -> Servo moves to 90° -> Clean water reaches crops -> Live telemetry updates on screen.

#### ✅ How to Test (Definition of Done)
- All physical bottles and hardware have clean printed labels attached.
- Pitch deck is ready and the live demonstration runs smoothly from phone to hardware.
