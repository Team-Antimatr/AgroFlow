#include <Servo.h>

// ==========================================
// AgroFlow AI — Arduino Uno Firmware Sketch
// ==========================================

// Pin Definitions
const int PIN_TDS        = A0; // TDS Sensor Potentiometer (0 to 1500 ppm)
const int PIN_SOIL       = A1; // Soil Moisture Potentiometer (0% to 100%)
const int PIN_SERVO      = 3;  // SG90 Servo Diverter (PWM)
const int PIN_RELAY_MAIN = 7;  // Relay 1: Reservoir 1 Submersible Lift Pump
const int PIN_RELAY_DOSE = 8;  // Relay 2: Reservoir 2 Fertilizer Dosing Pump

// System Parameters & Thresholds
const int TDS_THRESHOLD_PPM     = 500; // Impurity limit: >500 ppm routes to bio-filter
const int SOIL_MAX_MOISTURE_PCT = 60;  // Safety lock: >=60% aborts to prevent root rot

// Actuator State
Servo diverterServo;
bool mainPumpActive = false;
bool dosingActive   = false;
int currentServoAngle = 0;

// Timing Control (Non-blocking millis)
unsigned long lastTelemetryTime = 0;
const unsigned long TELEMETRY_INTERVAL = 1000; // 1-second interval
unsigned long cycleStartTime = 0;
bool cycleRunning = true; // Auto-active in demo mode

void setup() {
  Serial.begin(9600);

  // Initialize Relay Output Pins
  pinMode(PIN_RELAY_MAIN, OUTPUT);
  pinMode(PIN_RELAY_DOSE, OUTPUT);
  digitalWrite(PIN_RELAY_MAIN, LOW);
  digitalWrite(PIN_RELAY_DOSE, LOW);

  // Attach and Home Servo Motor (0° = Clean Bypass)
  diverterServo.attach(PIN_SERVO);
  diverterServo.write(0);
  currentServoAngle = 0;

  cycleStartTime = millis();
  Serial.println(F("{\"status\":\"AgroFlow AI Initialized\",\"version\":\"1.0.0\"}"));
}

void loop() {
  unsigned long now = millis();

  // 1. Read Analog Inputs
  int rawTDS = analogRead(PIN_TDS);
  int rawSoil = analogRead(PIN_SOIL);

  // Map to Real-World Units
  int tdsPPM = map(rawTDS, 0, 1023, 0, 1500);
  int soilMoisturePct = map(rawSoil, 0, 1023, 0, 100);

  // 2. Closed-Loop Safety Check
  if (soilMoisturePct >= SOIL_MAX_MOISTURE_PCT) {
    // Soil already saturated — abort watering
    mainPumpActive = false;
    dosingActive = false;
    digitalWrite(PIN_RELAY_MAIN, LOW);
    digitalWrite(PIN_RELAY_DOSE, LOW);
  } else {
    // Permitted to irrigate
    mainPumpActive = true;
    digitalWrite(PIN_RELAY_MAIN, HIGH);

    // 3. Automated Impurity Detection & Servo Diversion
    if (tdsPPM > TDS_THRESHOLD_PPM) {
      currentServoAngle = 90; // Divert to Bio-Sand & Charcoal Filter
    } else {
      currentServoAngle = 0;  // Direct Clean Bypass
    }
    diverterServo.write(currentServoAngle);

    // 4. Precision Fertigation (Pulse dosing pump for first 3 seconds)
    unsigned long elapsed = now - cycleStartTime;
    if (elapsed < 3000) {
      dosingActive = true;
      digitalWrite(PIN_RELAY_DOSE, HIGH);
    } else {
      dosingActive = false;
      digitalWrite(PIN_RELAY_DOSE, LOW);
    }
  }

  // 5. Broadcast Telemetry Packet every 1 second
  if (now - lastTelemetryTime >= TELEMETRY_INTERVAL) {
    lastTelemetryTime = now;

    // Send formatted JSON for Android App & Serial Monitor
    Serial.print(F("{\"tds\":"));
    Serial.print(tdsPPM);
    Serial.print(F(",\"moisture\":"));
    Serial.print(soilMoisturePct);
    Serial.print(F(",\"servo\":"));
    Serial.print(currentServoAngle);
    Serial.print(F(",\"pump\":"));
    Serial.print(mainPumpActive ? 1 : 0);
    Serial.print(F(",\"dose\":"));
    Serial.print(dosingActive ? 1 : 0);
    Serial.print(F(",\"flow\":1.8"));
    Serial.println(F("}"));
  }
}
