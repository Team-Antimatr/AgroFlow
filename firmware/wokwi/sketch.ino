#include <Servo.h>
#include <SoftwareSerial.h>

// =========================================================================
// AgroFlow AI — Arduino Uno Firmware Sketch with Dual Serial & Bluetooth
// =========================================================================

// Pin Definitions
const int PIN_TDS        = A0; // TDS Sensor Potentiometer (0 to 1500 ppm)
const int PIN_SOIL       = A1; // Soil Moisture Potentiometer (0% to 100%)
const int PIN_SERVO      = 3;  // SG90 Servo Diverter (PWM)
const int PIN_RELAY_MAIN = 7;  // Relay 1: Reservoir 1 Submersible Lift Pump
const int PIN_RELAY_DOSE = 8;  // Relay 2: Reservoir 2 Fertilizer Dosing Pump
const int PIN_BT_RX      = 10; // Connect to HC-05 TX
const int PIN_BT_TX      = 11; // Connect to HC-05 RX (via 1k/2k voltage divider)

// Bluetooth Serial Port (9600 Baud)
SoftwareSerial btSerial(PIN_BT_RX, PIN_BT_TX);

// System Parameters & Thresholds
const int TDS_THRESHOLD_HIGH    = 520; // Impurity limit: >520 ppm routes to bio-filter
const int TDS_THRESHOLD_LOW     = 480; // Hysteresis buffer: <480 ppm returns to clean bypass
const int SOIL_MAX_MOISTURE_PCT = 60;  // Safety lock: >=60% aborts to prevent root rot

// Actuator State
Servo diverterServo;
bool mainPumpActive = false;
bool dosingActive   = false;
bool manualOverride = false;
int currentServoAngle = -1; // -1 ensures initial homing write occurs once
bool servoMoving = false;
unsigned long servoMoveEndTime = 0;

void moveServoSmoothly(int targetAngle) {
  targetAngle = constrain(targetAngle, 0, 90);
  if (targetAngle != currentServoAngle) {
    currentServoAngle = targetAngle;
    if (!diverterServo.attached()) {
      diverterServo.attach(PIN_SERVO);
    }
    diverterServo.write(currentServoAngle);
    servoMoveEndTime = millis() + 450; // Allow 450ms to reach target
    servoMoving = true;
  }
}

// Timing Control (Non-blocking millis)
unsigned long lastTelemetryTime = 0;
const unsigned long TELEMETRY_INTERVAL = 1000; // 1-second interval
unsigned long cycleStartTime = 0;
unsigned long doseEndTime = 0;

void broadcastMessage(const String& msg) {
  Serial.println(msg);
  btSerial.println(msg);
}

void processCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  if (cmd == "CMD:PUMP:ON") {
    manualOverride = true;
    mainPumpActive = true;
    digitalWrite(PIN_RELAY_MAIN, HIGH);
    broadcastMessage(F("{\"ack\":\"PUMP_ON\"}"));
  } else if (cmd == "CMD:PUMP:OFF") {
    manualOverride = true;
    mainPumpActive = false;
    digitalWrite(PIN_RELAY_MAIN, LOW);
    broadcastMessage(F("{\"ack\":\"PUMP_OFF\"}"));
  } else if (cmd.startsWith("CMD:DOSE:")) {
    int ml = cmd.substring(9).toInt();
    if (ml <= 0) ml = 30;
    dosingActive = true;
    digitalWrite(PIN_RELAY_DOSE, HIGH);
    doseEndTime = millis() + (ml * 80); // ~80ms per ml calibration
    broadcastMessage(String("{\"ack\":\"DOSE\",\"ml\":") + String(ml) + "}");
  } else if (cmd.startsWith("CMD:VALVE:")) {
    manualOverride = true;
    int angle = cmd.substring(10).toInt();
    moveServoSmoothly(angle);
    broadcastMessage(String("{\"ack\":\"VALVE\",\"angle\":") + String(currentServoAngle) + "}");
  } else if (cmd == "CMD:AUTO") {
    manualOverride = false;
    broadcastMessage(F("{\"ack\":\"AUTO_MODE\"}"));
  }
}

void setup() {
  Serial.begin(9600);
  btSerial.begin(9600);

  // Initialize Relay Output Pins (Default OFF)
  pinMode(PIN_RELAY_MAIN, OUTPUT);
  pinMode(PIN_RELAY_DOSE, OUTPUT);
  digitalWrite(PIN_RELAY_MAIN, LOW);
  digitalWrite(PIN_RELAY_DOSE, LOW);

  // Initial Home Servo (0° = Clean Bypass)
  moveServoSmoothly(0);

  cycleStartTime = millis();
  broadcastMessage(F("{\"status\":\"AgroFlow AI Initialized\",\"version\":\"1.2.0\",\"baud\":9600}"));
}

void loop() {
  unsigned long now = millis();

  // 1. Process Inbound Commands from USB Serial or Bluetooth
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    processCommand(cmd);
  }
  if (btSerial.available() > 0) {
    String cmd = btSerial.readStringUntil('\n');
    processCommand(cmd);
  }

  // 2. Auto-detach Servo once it reaches position (100% eliminates buzzing, jitter & power sag)
  if (servoMoving && now >= servoMoveEndTime) {
    diverterServo.detach();
    servoMoving = false;
  }

  // 3. Handle Dosing Auto-Cutoff
  if (dosingActive && now >= doseEndTime && doseEndTime > 0) {
    dosingActive = false;
    digitalWrite(PIN_RELAY_DOSE, LOW);
    doseEndTime = 0;
  }

  // 4. Read Analog Inputs
  int rawTDS = analogRead(PIN_TDS);
  int rawSoil = analogRead(PIN_SOIL);

  // Map to Real-World Units
  int tdsPPM = map(rawTDS, 0, 1023, 0, 1500);
  int soilMoisturePct = map(rawSoil, 0, 1023, 0, 100);

  // 5. Closed-Loop Automation (Only when not in manual override)
  if (!manualOverride) {
    if (soilMoisturePct >= SOIL_MAX_MOISTURE_PCT) {
      // Soil already saturated — auto stop
      mainPumpActive = false;
      digitalWrite(PIN_RELAY_MAIN, LOW);
    } else {
      mainPumpActive = true;
      digitalWrite(PIN_RELAY_MAIN, HIGH);

      // Automated Impurity Detection & Servo Diversion with Hysteresis
      if (tdsPPM > TDS_THRESHOLD_HIGH) {
        moveServoSmoothly(90); // Divert to Bio-Sand & Charcoal Filter
      } else if (tdsPPM < TDS_THRESHOLD_LOW) {
        moveServoSmoothly(0);  // Direct Clean Bypass
      }
    }
  }

  // 4. Broadcast Telemetry Packet every 1 second
  if (now - lastTelemetryTime >= TELEMETRY_INTERVAL) {
    lastTelemetryTime = now;

    // Send formatted JSON for Browser Dashboard & Android App
    String packet = String("{\"tds\":") + String(tdsPPM) +
                    ",\"moisture\":" + String(soilMoisturePct) +
                    ",\"servo\":" + String(currentServoAngle) +
                    ",\"pump\":" + String(mainPumpActive ? 1 : 0) +
                    ",\"dose\":" + String(dosingActive ? 1 : 0) +
                    ",\"flow\":1.8}";
    broadcastMessage(packet);
  }
}
