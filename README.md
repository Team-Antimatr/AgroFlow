# AgroFlow AI
### Smart Water Filtration & Precision Fertigation Unit

![Status](https://shields.io)
![Milestone](https://shields.io)
![Platform](https://shields.io)
![App](https://shields.io)

---

## Project Overview & Problem Statement

Many small-scale farmers unknowingly irrigate crops with high-salinity or polluted groundwater, leading to soil degradation, stunted crop growth, and wasted water and fertilizers.

**AgroFlow AI** is an automated water quality management, bio-filtration, and precision fertigation hub integrated with a custom AI-powered mobile application.

---

## Core System Workflow

1. **AI Daily Schedule Cards:**  
   The mobile app displays AI-generated irrigation schedule cards with specific timings for different farm zones (`Zone 1`, `Zone 2`, `Zone 3`).
2. **One-Tap Mechanism Activation:**  
   Clicking the **START** button on a schedule card triggers the hardware wirelessly via Bluetooth/Wi-Fi.
3. **Real-Time Impurity Detection & Auto-Diversion:**  
   Before watering, **TDS (dissolved salts)** sensors inspect the source water. If impurity levels exceed safe agricultural limits (`> 500 ppm`), an automated **Servo Diverter Valve** routes the water through a multi-layer **Bio-Sand & Activated Charcoal Filter**.
4. **Precision Fertigation & Zone Delivery:**  
   Clean water is enriched with liquid nutrients/fertilizer via a dosing pump and delivered directly to the active crop zone.
5. **Live AI Telemetry Dashboard:**  
   During operation, the hardware streams live data back to the app, displaying:
   - **Water Impurity Levels (`ppm`)**
   - **Water Filtration Rate (`L/min`)**
   - **AI-Evaluated Soil Maturation & Moisture Status (`%`)**
   - **Active Mechanism & Valve Signals**

---

## Project Roles & Scope

- **IoT & Hardware Integration:** Sensor calibration, relay switching, and submersible pump assembly.
- **Circuit Design (Simulation-Based):** Virtual circuit prototyping and logic verification on Wokwi.
- **App Development (Android CLI):** Custom mobile interface for schedule cards, Bluetooth/Wi-Fi control, and live telemetry.
- **API & AI Model Integration:** AI-driven soil maturation analysis, scheduling, and structured telemetry advisory.
- **UI/UX Designs & Labels:** App screen wireframes in Figma and physical hardware/reservoir labeling.
- **Presentation & Documentation:** Exhibition pitch deck and technical documentation.

---

## Project Resources & Links

| Resource | Platform | Link |
| :--- | :--- | :--- |
| **IoT Instruments & BOM List** | Canva | [View Instrument List](https://canva.link) |
| **Circuit Design & Simulation** | Wokwi | [Open Wokwi Simulation](https://wokwi.com) |
| **Circuit & AI Architecture Context** | AI Reference | [View Circuit AI Context](https://share.google) |
| **App UI/UX Designs** | Figma | [Open Figma Designs](https://figma.com) |
| **Project Presentation** | Canva | [Canva Workspace](https://canva.com) |
| **Backend & Deployment** | Firebase | [Firebase Console](https://firebase.com) |

---

## Hardware Pinout Reference (Arduino Uno)

| Component | Component Pin | Arduino Uno Pin | Function |
| :--- | :--- | :--- | :--- |
| **TDS Water Quality Sensor** | `Signal (A)` | `A0` | Reads source water salinity/impurity (`ppm`) |
| **Soil Moisture Sensor** | `Signal (A0)` | `A1` | Reads soil hydration percentage (`%`) |
| **SG90 Servo Motor** | `PWM (Orange)` | `Pin 3` | Diverts impure water (`90°`) vs. direct clean flow (`0°`) |
| **Relay IN1 (Main Pump)** | `IN1` | `Pin 7` | Controls Reservoir 1 main submersible pump |
| **Relay IN2 (Fertigation Pump)** | `IN2` | `Pin 8` | Controls liquid fertilizer/nutrient dosing pump |
| **HC-05 Bluetooth Module** | `TX` / `RX` | `Pin 10` / `Pin 11` | Two-way serial communication with Android App |

---

## Project Timeline & Milestones

| Milestone | Target Date | Status |
| :--- | :--- | :--- |
| **Project Completion Deadline** | **25 November 2026** | In Progress |
| **Paid Milestone Release** | **1 December 2026** | Scheduled |

---



