#include <Servo.h>
#include <SoftwareSerial.h>

// =========================================================================
// AgroFlow AI — Version 2.3 (10-Second Smart Sampling & Live Soil Edition)
// =========================================================================

const int PIN_TDS   = A0; // TDS Sensor
const int PIN_SOIL  = A1; // Soil Moisture Sensor (ALWAYS 100% LIVE)
const int PIN_SERVO = 3;  // SG90 Servo
const int PIN_BT_RX = 10; // HC-05 TX
const int PIN_BT_TX = 11; // HC-05 RX

SoftwareSerial btSerial(PIN_BT_RX, PIN_BT_TX);

const int TDS_CUTOFF = 490;

Servo diverterServo;
int currentServoAngle          = -1;
bool servoMoving               = false;
unsigned long servoMoveEndTime = 0;
unsigned long lastTelemetryTime = 0;

// --- 10-SECOND SAMPLING & LOCK ENGINE ---
bool isSampling            = true;          // Shuru me 10 sec sample karega
unsigned long sampleStartTime = 0;
const unsigned long SAMPLE_DURATION = 10000; // 10 seconds sampling window
int lockedTDS              = 0;
bool isLocked              = false;

void moveServoSmoothly(int targetAngle) {
  targetAngle = (targetAngle >= 45) ? 90 : 0;
  
  if (targetAngle != currentServoAngle) {
    currentServoAngle = targetAngle;
    if (!diverterServo.attached()) {
      diverterServo.attach(PIN_SERVO);
    }
    diverterServo.write(currentServoAngle);
    servoMoveEndTime = millis() + 500;
    servoMoving = true;
  }
}

void broadcastMessage(const String& msg) {
  Serial.println(msg);
  btSerial.println(msg);
}

// Reset function: Starts fresh 10-second sampling
void startSampling() {
  isSampling = true;
  isLocked = false;
  sampleStartTime = millis();
  moveServoSmoothly(0);
  broadcastMessage(F("{\"status\":\"SAMPLING_STARTED\",\"duration\":10}"));
}

void processCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  // 'r' ya 'R' = Refresh / Resample for 10 seconds!
  if (cmd == "r" || cmd == "R" || cmd == "CMD:REFRESH" || cmd == "CMD:RESET") {
    startSampling();
  }
  // Manual override 90°
  else if (cmd == "s" || cmd == "S" || cmd == "CMD:VALVE:90") {
    isSampling = false;
    isLocked = true;
    lockedTDS = 650;
    moveServoSmoothly(90);
    broadcastMessage(F("{\"ack\":\"VALVE\",\"angle\":90,\"path\":\"BIO_FILTER\"}"));
  }
  // Manual override 0°
  else if (cmd == "c" || cmd == "C" || cmd == "CMD:VALVE:0") {
    isSampling = false;
    isLocked = true;
    lockedTDS = 45;
    moveServoSmoothly(0);
    broadcastMessage(F("{\"ack\":\"VALVE\",\"angle\":0,\"path\":\"DIRECT_CROP\"}"));
  }
}

void setup() {
  Serial.begin(9600);
  btSerial.begin(9600);
  Serial.setTimeout(50);
  btSerial.setTimeout(50);

  // Initial Home 0°
  moveServoSmoothly(0);

  // Start initial 10-second sampling window
  startSampling();
}

void loop() {
  unsigned long now = millis();

  // 1. Process Inbound Commands
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    processCommand(cmd);
  }
  if (btSerial.available() > 0) {
    String cmd = btSerial.readStringUntil('\n');
    processCommand(cmd);
  }

  // 2. Auto-detach Servo (Zero hum & power safe)
  if (servoMoving && now >= servoMoveEndTime) {
    diverterServo.detach();
    servoMoving = false;
  }

  // 3. SOIL MOISTURE: 100% ALWAYS LIVE & NEVER LOCKED
  // (Yeh lagatar real-time mitti padhta rahega aur vary karega)
  int rawSoil = analogRead(PIN_SOIL);
  int soilMoisturePct = map(rawSoil, 650, 300, 0, 100);
  soilMoisturePct = constrain(soilMoisturePct, 0, 100);

  // 4. TDS SENSING & 10-SECOND LOCK LOGIC
  int currentTDS = 0;
  int secondsLeft = 0;

  if (isSampling) {
    // 10-Second Catching Window Active
    unsigned long elapsed = now - sampleStartTime;
    if (elapsed < SAMPLE_DURATION) {
      secondsLeft = (SAMPLE_DURATION - elapsed) / 1000 + 1;
      
      // Live reading during sampling
      int rawTDS = analogRead(PIN_TDS);
      currentTDS = map(rawTDS, 0, 1023, 0, 1500);
      if (currentTDS < 0) currentTDS = 0;

      // Real-time servo tracking during sampling
      if (currentTDS >= TDS_CUTOFF) {
        moveServoSmoothly(90);
      } else {
        moveServoSmoothly(0);
      }
    } else {
      // 10 Seconds Completed -> LOCK THE TDS & SERVO NOW!
      isSampling = false;
      isLocked = true;
      int rawTDS = analogRead(PIN_TDS);
      lockedTDS = map(rawTDS, 0, 1023, 0, 1500);
      if (lockedTDS < 0) lockedTDS = 0;

      currentTDS = lockedTDS;

      // Lock Servo Position
      if (lockedTDS >= TDS_CUTOFF) {
        moveServoSmoothly(90); // Lock at 90° (Filter)
      } else {
        moveServoSmoothly(0);  // Lock at 0° (Clean)
      }

      broadcastMessage(String("{\"alert\":\"TDS_LOCKED\",\"tds\":") + String(lockedTDS) + 
                       ",\"servo\":" + String(currentServoAngle) + "}");
    }
  } else {
    // Already Locked State: TDS & Servo remain frozen at their evaluated state
    currentTDS = lockedTDS;
    if (lockedTDS >= TDS_CUTOFF) {
      moveServoSmoothly(90);
    } else {
      moveServoSmoothly(0);
    }
  }

  // 5. Broadcast 1-second Telemetry
  if (now - lastTelemetryTime >= 1000) {
    lastTelemetryTime = now;
    int filterActive = (currentServoAngle == 90) ? 1 : 0;

    String packet = String("{\"tds\":") + String(currentTDS) +
                    ",\"moisture\":" + String(soilMoisturePct) +
                    ",\"servo\":" + String(currentServoAngle) +
                    ",\"filter\":" + String(filterActive) +
                    ",\"sampling\":" + String(isSampling ? secondsLeft : 0) +
                    ",\"locked\":" + String(isLocked ? 1 : 0) +
                    ",\"flow\":1.8}";
    broadcastMessage(packet);
  }
}
