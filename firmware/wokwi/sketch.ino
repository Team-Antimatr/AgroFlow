#include <Servo.h>
#include <SoftwareSerial.h>

// =========================================================================
// AgroFlow AI — Smart Irrigation & Precision Diversion System
// Version 2.0 (Relay-Free Edition: Pure Smart Diverter & Soil Intelligence)
// =========================================================================

// --- Pin Definitions ---
const int PIN_TDS   = A0; // Analog TDS Sensor (Water Container / Line)
const int PIN_SOIL  = A1; // Capacitive Soil Moisture Sensor (Soil Tray)
const int PIN_SERVO = 3;  // SG90 Servo Diverter Valve (PWM)
const int PIN_BT_RX = 10; // Connect to HC-05 TX
const int PIN_BT_TX = 11; // Connect to HC-05 RX

// Bluetooth Serial Port (9600 Baud)
SoftwareSerial btSerial(PIN_BT_RX, PIN_BT_TX);

// --- Operational Thresholds ---
const int TDS_THRESHOLD_HIGH = 520; // >520 PPM: Impurity detected -> Divert to Bio-Filter (90°)
const int TDS_THRESHOLD_LOW  = 480; // <480 PPM: Clean water -> Direct to Crop (0°)
const int SOIL_DRY_PCT       = 35;  // <35%: Needs Water
const int SOIL_SATURATED_PCT = 65;  // >65%: Soil Saturated

// --- Servo & Valve State ---
Servo diverterServo;
int currentServoAngle       = -1;
bool servoMoving            = false;
unsigned long servoMoveEndTime = 0;
bool valveManualOverride    = false;

// Non-blocking Telemetry Timing
unsigned long lastTelemetryTime = 0;
const unsigned long TELEMETRY_INTERVAL = 1000; // 1-second packet stream

// --- Smooth Servo Movement with Auto-Detach ---
// (Rotates smoothly, then cuts PWM so the servo NEVER buzzes, heats, or wastes power)
void moveServoSmoothly(int targetAngle) {
  targetAngle = constrain(targetAngle, 0, 90);
  if (targetAngle != currentServoAngle) {
    currentServoAngle = targetAngle;
    if (!diverterServo.attached()) {
      diverterServo.attach(PIN_SERVO);
    }
    diverterServo.write(currentServoAngle);
    servoMoveEndTime = millis() + 600; // 600ms rotation window
    servoMoving = true;
  }
}

// Send formatted messages to both USB Serial and Bluetooth
void broadcastMessage(const String& msg) {
  Serial.println(msg);
  btSerial.println(msg);
}

// --- Process Inbound Commands (Serial & Bluetooth) ---
void processCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  // 1. Manual Valve: 90° (Bio-Filter Path)
  if (cmd == "CMD:VALVE:90" || cmd == "s" || cmd == "S" || cmd == "90") {
    valveManualOverride = true;
    moveServoSmoothly(90);
    broadcastMessage(F("{\"ack\":\"VALVE\",\"angle\":90,\"path\":\"BIO_FILTER\"}"));

  // 2. Manual Valve: 0° (Direct Clean Crop Path)
  } else if (cmd == "CMD:VALVE:0" || cmd == "c" || cmd == "C" || cmd == "0") {
    valveManualOverride = true;
    moveServoSmoothly(0);
    broadcastMessage(F("{\"ack\":\"VALVE\",\"angle\":0,\"path\":\"DIRECT_CROP\"}"));

  // 3. Return to Automatic AI / TDS Tracking Mode
  } else if (cmd == "CMD:AUTO" || cmd == "CMD:VALVE:AUTO" || cmd == "a" || cmd == "A") {
    valveManualOverride = false;
    broadcastMessage(F("{\"ack\":\"AUTO_MODE\",\"status\":\"TRACKING_TDS\"}"));
  }
}

void setup() {
  Serial.begin(9600);
  btSerial.begin(9600);
  Serial.setTimeout(50);
  btSerial.setTimeout(50);

  // Home Servo at 0° (Direct Clean Path)
  moveServoSmoothly(0);

  broadcastMessage(F("{\"status\":\"AgroFlow AI Online\",\"edition\":\"Relay-Free v2.0\"}"));
}

void loop() {
  unsigned long now = millis();

  // 1. Read Serial Commands (Laptop USB)
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    processCommand(cmd);
  }

  // 2. Read Bluetooth Commands (Mobile / Web App)
  if (btSerial.available() > 0) {
    String cmd = btSerial.readStringUntil('\n');
    processCommand(cmd);
  }

  // 3. Auto-detach Servo after 600ms rotation (Eliminates hum, heat & power sag)
  if (servoMoving && now >= servoMoveEndTime) {
    diverterServo.detach();
    servoMoving = false;
  }

  // 4. Read Analog Sensors
  int rawTDS  = analogRead(PIN_TDS);
  int rawSoil = analogRead(PIN_SOIL);

  // Conversion: TDS 0-1023 -> 0-1500 PPM approx
  int tdsPPM = map(rawTDS, 0, 1023, 0, 1500);
  if (tdsPPM < 0) tdsPPM = 0;

  // Conversion: Capacitive Soil Moisture (Air ~650, Water ~300)
  int soilMoisturePct = map(rawSoil, 650, 300, 0, 100);
  soilMoisturePct = constrain(soilMoisturePct, 0, 100);

  // Determine Soil Health State
  String soilStatus = "OPTIMAL";
  if (soilMoisturePct < SOIL_DRY_PCT) {
    soilStatus = "DRY";
  } else if (soilMoisturePct > SOIL_SATURATED_PCT) {
    soilStatus = "SATURATED";
  }

  // 5. AUTOMATIC IMPURITY DIVERSION (The Core Intelligence)
  // If not manually overridden, servo automatically steers water based on real-time TDS:
  if (!valveManualOverride) {
    if (tdsPPM > TDS_THRESHOLD_HIGH) {
      moveServoSmoothly(90); // Saline / Impure Water -> Route to Bio-Filter!
    } else if (tdsPPM < TDS_THRESHOLD_LOW) {
      moveServoSmoothly(0);  // Pure / Clean Water -> Route directly to Crop!
    }
  }

  // 6. Broadcast Telemetry Packet every 1 second
  if (now - lastTelemetryTime >= TELEMETRY_INTERVAL) {
    lastTelemetryTime = now;

    // Is bio-filter active? (1 if 90°, 0 if 0°)
    int filterActive = (currentServoAngle >= 45) ? 1 : 0;

    // Clean JSON packet for Dashboard & Gemini AI Agent
    String packet = String("{\"tds\":") + String(tdsPPM) +
                    ",\"moisture\":" + String(soilMoisturePct) +
                    ",\"soil_status\":\"" + soilStatus + "\"" +
                    ",\"servo\":" + String(currentServoAngle) +
                    ",\"filter\":" + String(filterActive) +
                    ",\"flow\":1.8}";
    broadcastMessage(packet);
  }
}
