#include <Servo.h>
#include <SoftwareSerial.h>

// =========================================================================
// AgroFlow AI — Smart Irrigation & Precision Diversion System
// Version 2.1 (Rock-Solid 490 PPM Binary Threshold)
// =========================================================================

// --- Pin Definitions ---
const int PIN_TDS   = A0; // Analog TDS Sensor (Water Container / Line)
const int PIN_SOIL  = A1; // Capacitive Soil Moisture Sensor (Soil Tray)
const int PIN_SERVO = 3;  // SG90 Servo Diverter Valve (PWM)
const int PIN_BT_RX = 10; // Connect to HC-05 TX
const int PIN_BT_TX = 11; // Connect to HC-05 RX

// Bluetooth Serial Port (9600 Baud)
SoftwareSerial btSerial(PIN_BT_RX, PIN_BT_TX);

// --- Operational Threshold ---
// <490 PPM = 0° (Rock-Solid Clean Crop Path)
// >=490 PPM = 90° (Instant Direct to Bio-Filter)
const int TDS_CUTOFF = 490;

// --- Servo & Valve State ---
Servo diverterServo;
int currentServoAngle          = -1;
bool servoMoving               = false;
unsigned long servoMoveEndTime = 0;
bool valveManualOverride       = false;

// Non-blocking Telemetry Timing
unsigned long lastTelemetryTime = 0;
const unsigned long TELEMETRY_INTERVAL = 1000; // 1-second packet stream

// --- Smooth Servo Movement with Auto-Detach ---
// Once target angle is reached, PWM detaches so the servo is completely rock-solid with ZERO jitter
void moveServoSmoothly(int targetAngle) {
  targetAngle = (targetAngle >= 45) ? 90 : 0; // Strict binary lock: only 0° or 90°
  
  if (targetAngle != currentServoAngle) {
    currentServoAngle = targetAngle;
    if (!diverterServo.attached()) {
      diverterServo.attach(PIN_SERVO);
    }
    diverterServo.write(currentServoAngle);
    servoMoveEndTime = millis() + 500; // 500ms rotation window
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

  // 1. Manual: 90° (Bio-Filter)
  if (cmd == "CMD:VALVE:90" || cmd == "s" || cmd == "S" || cmd == "90") {
    valveManualOverride = true;
    moveServoSmoothly(90);
    broadcastMessage(F("{\"ack\":\"VALVE\",\"angle\":90,\"path\":\"BIO_FILTER\"}"));

  // 2. Manual: 0° (Direct Clean Crop Path)
  } else if (cmd == "CMD:VALVE:0" || cmd == "c" || cmd == "C" || cmd == "0") {
    valveManualOverride = true;
    moveServoSmoothly(0);
    broadcastMessage(F("{\"ack\":\"VALVE\",\"angle\":0,\"path\":\"DIRECT_CROP\"}"));

  // 3. Return to Automatic TDS Tracking Mode
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

  // Home Servo strictly at 0° on startup
  moveServoSmoothly(0);

  broadcastMessage(F("{\"status\":\"AgroFlow AI Online\",\"threshold\":490,\"edition\":\"Rock-Solid v2.1\"}"));
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

  // 3. Auto-detach Servo after rotation (Zero hum, zero jitter, locked in place)
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

  // Conversion: Capacitive Soil Moisture
  int soilMoisturePct = map(rawSoil, 650, 300, 0, 100);
  soilMoisturePct = constrain(soilMoisturePct, 0, 100);

  // 5. ROCK-SOLID CUTOFF LOGIC (Aapka Exact Niyam)
  // TDS >= 490 -> Turant 90° (Bio-filter)
  // TDS < 490  -> Rock-solid 0° (Direct khet me)
  if (!valveManualOverride) {
    if (tdsPPM >= TDS_CUTOFF) {
      moveServoSmoothly(90); // Ganda paani -> 90° Filter
    } else {
      moveServoSmoothly(0);  // Saaf paani -> 0° Fix
    }
  }

  // 6. Broadcast Telemetry Packet every 1 second
  if (now - lastTelemetryTime >= TELEMETRY_INTERVAL) {
    lastTelemetryTime = now;
    int filterActive = (currentServoAngle == 90) ? 1 : 0;

    String packet = String("{\"tds\":") + String(tdsPPM) +
                    ",\"moisture\":" + String(soilMoisturePct) +
                    ",\"servo\":" + String(currentServoAngle) +
                    ",\"filter\":" + String(filterActive) +
                    ",\"flow\":1.8}";
    broadcastMessage(packet);
  }
}
