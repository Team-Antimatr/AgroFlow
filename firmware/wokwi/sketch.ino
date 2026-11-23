#include <Servo.h>
#include <SoftwareSerial.h>

// =========================================================================
// AgroFlow AI — Arduino Uno Firmware Sketch with Dual Serial & Bluetooth
// =========================================================================

// Pin Definitions
const int PIN_TDS        = A0; // TDS Sensor Potentiometer (0 to 1500 ppm)
const int PIN_SOIL       = A1; // Soil Moisture Potentiometer (0% to 100%)
const int PIN_SERVO      = 3;  // SG90 Servo Diverter (PWM)
const int PIN_RELAY_MAIN = 7;  // Relay 1: Reservoir 1 Submersible Lift Pump (ACTIVE)
// const int PIN_RELAY_DOSE = 8;  // Relay 2: Fertilizer Dosing Pump (TEMPORARILY DISABLED: Broken wire)
const int PIN_BT_RX      = 10; // Connect to HC-05 TX
const int PIN_BT_TX      = 11; // Connect to HC-05 RX (via 1k/2k voltage divider)

// Relay State Polarity (HIGH for Wokwi and standard Active-HIGH modules)
const int RELAY_ACTIVE   = HIGH;
const int RELAY_INACTIVE = LOW;

// Bluetooth Serial Port (9600 Baud)
SoftwareSerial btSerial(PIN_BT_RX, PIN_BT_TX);

// System Parameters & Thresholds
const int TDS_THRESHOLD_HIGH    = 520; // Impurity limit: >520 ppm routes to bio-filter
const int TDS_THRESHOLD_LOW     = 480; // Hysteresis buffer: <480 ppm returns to clean bypass
const int SOIL_MAX_MOISTURE_PCT = 60;  // Safety lock: >=60% aborts to prevent root rot

// Actuator State
Servo diverterServo;
bool mainPumpActive      = false;
bool dosingActive        = false;
bool valveManualOverride = false;
int currentServoAngle    = -1; // -1 ensures initial homing write occurs once
bool servoMoving         = false;
unsigned long servoMoveEndTime = 0;

void moveServoSmoothly(int targetAngle) {
  targetAngle = constrain(targetAngle, 0, 90);
  if (targetAngle != currentServoAngle) {
    currentServoAngle = targetAngle;
    if (!diverterServo.attached()) {
      diverterServo.attach(PIN_SERVO);
    }
    diverterServo.write(currentServoAngle);
    servoMoveEndTime = millis() + 600; // Allow 600ms to complete physical rotation
    servoMoving = true;
  }
}

// Timing Control (Non-blocking millis)
unsigned long lastTelemetryTime = 0;
const unsigned long TELEMETRY_INTERVAL = 1000; // 1-second interval
unsigned long doseEndTime = 0;

void broadcastMessage(const String& msg) {
  Serial.println(msg);
  btSerial.println(msg);
}

void startDosing(int ml) {
  /* -------------------------------------------------------------
   * FERTILIZATION PUMP TEMPORARILY DISABLED (BROKEN WIRE)
   * -------------------------------------------------------------
  if (ml <= 0) ml = 30;
  dosingActive = true;
  digitalWrite(PIN_RELAY_DOSE, RELAY_ACTIVE);
  doseEndTime = millis() + ((unsigned long)ml * 80);
  broadcastMessage(String("{\"ack\":\"DOSE\",\"ml\":") + String(ml) + "}");
  ------------------------------------------------------------- */
}

void processCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  // Start command (compatible with both Web Dashboard 'CMD:PUMP:ON' and Mobile App 'CMD_START_Z1')
  if (cmd == "CMD:PUMP:ON" || cmd.startsWith("CMD_START") || cmd.startsWith("CMD:START")) {
    int rawSoil = analogRead(PIN_SOIL);
    int soilMoisturePct = map(rawSoil, 0, 1023, 0, 100);

    if (soilMoisturePct >= SOIL_MAX_MOISTURE_PCT) {
      broadcastMessage(String("{\"err\":\"SOIL_SATURATED\",\"moisture\":") + String(soilMoisturePct) + ",\"action\":\"ABORTED\"}");
      return;
    }

    mainPumpActive = true;
    digitalWrite(PIN_RELAY_MAIN, RELAY_ACTIVE);

    String zone = "Z1";
    int zIdx = cmd.indexOf("_Z");
    if (zIdx != -1) {
      zone = cmd.substring(zIdx + 1);
    }
    broadcastMessage(String("{\"ack\":\"START\",\"zone\":\"") + zone + "\",\"pump\":1}");

    // Note: Fertilizer dosing pump call commented out (broken wire)
    // startDosing(30);

  // Stop command (compatible with both Web Dashboard 'CMD:PUMP:OFF' and Mobile App 'CMD_STOP')
  } else if (cmd == "CMD:PUMP:OFF" || cmd == "CMD_STOP" || cmd == "CMD:STOP") {
    mainPumpActive = false;
    digitalWrite(PIN_RELAY_MAIN, RELAY_INACTIVE);
    broadcastMessage(F("{\"ack\":\"PUMP_OFF\"}"));

  // Manual dosing command
  } else if (cmd.startsWith("CMD:DOSE:")) {
    int ml = cmd.substring(9).toInt();
    startDosing(ml);

  // Manual diverter valve angle command
  } else if (cmd.startsWith("CMD:VALVE:")) {
    valveManualOverride = true;
    int angle = cmd.substring(10).toInt();
    moveServoSmoothly(angle);
    broadcastMessage(String("{\"ack\":\"VALVE\",\"angle\":") + String(currentServoAngle) + "}");

  // Return valve to automatic impurity detection
  } else if (cmd == "CMD:AUTO" || cmd == "CMD:VALVE:AUTO") {
    valveManualOverride = false;
    broadcastMessage(F("{\"ack\":\"AUTO_MODE\"}"));
  }
}

void setup() {
  Serial.begin(9600);
  btSerial.begin(9600);

  // Non-blocking serial timeout
  Serial.setTimeout(50);
  btSerial.setTimeout(50);

  // Initialize Relay Output Pins (Default OFF)
  pinMode(PIN_RELAY_MAIN, OUTPUT);
  digitalWrite(PIN_RELAY_MAIN, RELAY_INACTIVE);

  // Relay 2 (Dosing) temporarily commented out
  // pinMode(PIN_RELAY_DOSE, OUTPUT);
  // digitalWrite(PIN_RELAY_DOSE, RELAY_INACTIVE);

  // Initial Home Servo: 0° (Direct Clean Path to Soil)
  moveServoSmoothly(0);

  broadcastMessage(F("{\"status\":\"AgroFlow AI Initialized\",\"version\":\"1.2.1\",\"baud\":9600}"));
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

  // 2. Auto-detach Servo once it reaches position (eliminates buzzing & power sag)
  if (servoMoving && now >= servoMoveEndTime) {
    diverterServo.detach();
    servoMoving = false;
  }

  /* -------------------------------------------------------------
   * 3. DOSING AUTO-CUTOFF (TEMPORARILY COMMENTED OUT)
   * if (dosingActive && now >= doseEndTime && doseEndTime > 0) {
   *   dosingActive = false;
   *   digitalWrite(PIN_RELAY_DOSE, RELAY_INACTIVE);
   *   doseEndTime = 0;
   * }
   * ------------------------------------------------------------- */

  // 4. Read Analog Inputs
  int rawTDS = analogRead(PIN_TDS);
  int rawSoil = analogRead(PIN_SOIL);

  // Map to Real-World Units
  int tdsPPM = map(rawTDS, 0, 1023, 0, 1500);
  int soilMoisturePct = map(rawSoil, 0, 1023, 0, 100);

  // 5. Safety Cutoff: Prevent overwatering and root rot if soil moisture >= 60%
  if (mainPumpActive && soilMoisturePct >= SOIL_MAX_MOISTURE_PCT) {
    mainPumpActive = false;
    digitalWrite(PIN_RELAY_MAIN, RELAY_INACTIVE);
    broadcastMessage(String("{\"alert\":\"AUTO_SHUTOFF\",\"reason\":\"SOIL_SATURATED\",\"moisture\":") + String(soilMoisturePct) + "}");
  }

  /* -------------------------------------------------------------
   * 6. BIO-FILTER DIVERSION (COMMENTED OUT FOR DIRECT-TO-SOIL PROTOTYPE)
   * The pipe goes directly to the soil tray; bio-filter is skipped for now.
   * Servo stays locked at 0° (Direct Path).
   *
   * (Uncomment below when Bio-Filter column is physically connected):
   * if (!valveManualOverride) {
   *   if (tdsPPM > TDS_THRESHOLD_HIGH) {
   *     moveServoSmoothly(90); // Divert to Bio-Sand & Charcoal Filter
   *   } else if (tdsPPM < TDS_THRESHOLD_LOW) {
   *     moveServoSmoothly(0);  // Direct Clean Bypass
   *   }
   * }
   * ------------------------------------------------------------- */

  // 7. Broadcast Telemetry Packet every 1 second
  if (now - lastTelemetryTime >= TELEMETRY_INTERVAL) {
    lastTelemetryTime = now;

    // Send formatted JSON for Browser Dashboard & Android App
    String packet = String("{\"tds\":") + String(tdsPPM) +
                    ",\"moisture\":" + String(soilMoisturePct) +
                    ",\"servo\":" + String(currentServoAngle) +
                    ",\"pump\":" + String(mainPumpActive ? 1 : 0) +
                    ",\"dose\":0" +
                    ",\"flow\":1.8}";
    broadcastMessage(packet);
  }
}
