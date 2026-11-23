#include <Servo.h>
#include <SoftwareSerial.h>

// =========================================================================
// AgroFlow AI — Arduino Uno Firmware (Tested for Real Hardware & Bench Test)
// Version: 1.4.3 — Active-LOW Relays, Dual Channel Trigger, Clean Telemetry
// =========================================================================

// --- Pin Definitions ---
const int PIN_TDS         = A0; // Analog TDS Sensor (Water Container)
const int PIN_SOIL        = A1; // Capacitive Soil Moisture Sensor (Soil Tray)
const int PIN_SERVO       = 3;  // SG90 Servo Diverter (PWM)
const int PIN_RELAY_MAIN  = 7;  // Relay Channel 1 (IN1)
const int PIN_RELAY_CH2   = 8;  // Relay Channel 2 (IN2 - Both pins trigger together for foolproof testing)
const int PIN_BT_RX       = 10; // Connect to HC-05 TX
const int PIN_BT_TX       = 11; // Connect to HC-05 RX

// --- Relay Polarity (CRITICAL: Real Songle Relay boards are Active-LOW) ---
// LOW  = Relay ON (LED ON, NO connects to COM, Pump runs)
// HIGH = Relay OFF (LED OFF, NO disconnected, Pump stops)
const int RELAY_ACTIVE    = LOW;   
const int RELAY_INACTIVE  = HIGH;  

// Bluetooth Serial Port (9600 Baud)
SoftwareSerial btSerial(PIN_BT_RX, PIN_BT_TX);

// System Thresholds
const int TDS_THRESHOLD_HIGH = 520; // >520 ppm: Impurity -> Servo 90°
const int TDS_THRESHOLD_LOW  = 480; // <480 ppm: Clean water -> Servo 0°

// Actuator & State
Servo diverterServo;
bool mainPumpActive      = false;
bool valveManualOverride = false;
int currentServoAngle    = -1;
bool servoMoving         = false;
unsigned long servoMoveEndTime = 0;

// Non-blocking Timing
unsigned long lastTelemetryTime = 0;
const unsigned long TELEMETRY_INTERVAL = 1000; // 1-second packet stream

// --- Smooth Servo Movement with Auto-Detach ---
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

void setPumpState(bool turnOn) {
  mainPumpActive = turnOn;
  int state = turnOn ? RELAY_ACTIVE : RELAY_INACTIVE;
  // Trigger BOTH Pin 7 (IN1) and Pin 8 (IN2) so wiring to either channel works!
  digitalWrite(PIN_RELAY_MAIN, state);
  digitalWrite(PIN_RELAY_CH2, state);
}

void broadcastMessage(const String& msg) {
  Serial.println(msg);
  btSerial.println(msg);
}

// --- Process Inbound Commands ---
void processCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  // 1. Pump ON (Forced manual start - no sensor lock blocking your test)
  if (cmd == "CMD:PUMP:ON" || cmd == "CMD_START" || cmd.startsWith("CMD_START") || cmd == "1") {
    setPumpState(true);
    broadcastMessage(F("{\"ack\":\"PUMP_ON\",\"relay\":1,\"status\":\"RUNNING\"}"));

  // 2. Pump OFF
  } else if (cmd == "CMD:PUMP:OFF" || cmd == "CMD_STOP" || cmd.startsWith("CMD_STOP") || cmd == "0") {
    setPumpState(false);
    broadcastMessage(F("{\"ack\":\"PUMP_OFF\",\"relay\":0,\"status\":\"STOPPED\"}"));

  // 3. Manual Valve Angle (CMD:VALVE:90 or CMD:VALVE:0)
  } else if (cmd.startsWith("CMD:VALVE:")) {
    valveManualOverride = true;
    int angle = cmd.substring(10).toInt();
    moveServoSmoothly(angle);
    broadcastMessage(String("{\"ack\":\"VALVE\",\"angle\":") + String(currentServoAngle) + "}");

  // 4. Return to Auto TDS Tracking
  } else if (cmd == "CMD:AUTO" || cmd == "CMD:VALVE:AUTO") {
    valveManualOverride = false;
    broadcastMessage(F("{\"ack\":\"AUTO_MODE\",\"status\":\"TRACKING_TDS\"}"));
  }
}

void setup() {
  Serial.begin(9600);
  btSerial.begin(9600);

  Serial.setTimeout(50);
  btSerial.setTimeout(50);

  // Initialize Relay Pins (Default OFF = HIGH)
  pinMode(PIN_RELAY_MAIN, OUTPUT);
  pinMode(PIN_RELAY_CH2, OUTPUT);
  setPumpState(false);

  // Home Servo at 0° (Clean/Direct Path)
  moveServoSmoothly(0);

  broadcastMessage(F("{\"status\":\"AgroFlow Ready\",\"version\":\"1.4.3\"}"));
}

void loop() {
  unsigned long now = millis();

  // 1. Read Serial Commands (USB Cable)
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    processCommand(cmd);
  }

  // 2. Read Bluetooth Commands (HC-05)
  if (btSerial.available() > 0) {
    String cmd = btSerial.readStringUntil('\n');
    processCommand(cmd);
  }

  // 3. Auto-detach Servo once physical rotation completes (zero hum, zero current draw)
  if (servoMoving && now >= servoMoveEndTime) {
    diverterServo.detach();
    servoMoving = false;
  }

  // 4. Read Sensors
  int rawTDS  = analogRead(PIN_TDS);
  int rawSoil = analogRead(PIN_SOIL);

  // Conversions
  int tdsPPM = map(rawTDS, 0, 1023, 0, 1500);
  if (tdsPPM < 0) tdsPPM = 0;

  // Capacitive calibration: Air ~650 (Dry), Water ~300 (Wet)
  int soilMoisturePct = map(rawSoil, 650, 300, 0, 100);
  soilMoisturePct = constrain(soilMoisturePct, 0, 100);

  // 5. Automatic Impurity Detection (Servo moves to 90° if TDS > 520, 0° if < 480)
  if (!valveManualOverride) {
    if (tdsPPM > TDS_THRESHOLD_HIGH) {
      moveServoSmoothly(90);
    } else if (tdsPPM < TDS_THRESHOLD_LOW) {
      moveServoSmoothly(0);
    }
  }

  // 6. Broadcast 1-second Telemetry
  if (now - lastTelemetryTime >= TELEMETRY_INTERVAL) {
    lastTelemetryTime = now;
    String packet = String("{\"tds\":") + String(tdsPPM) +
                    ",\"moisture\":" + String(soilMoisturePct) +
                    ",\"servo\":" + String(currentServoAngle) +
                    ",\"pump\":" + String(mainPumpActive ? 1 : 0) +
                    ",\"dose\":0" +
                    ",\"flow\":1.8}";
    broadcastMessage(packet);
  }
}
