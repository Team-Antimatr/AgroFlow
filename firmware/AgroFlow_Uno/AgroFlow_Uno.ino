#include <Servo.h>
#include <SoftwareSerial.h>

// =========================================================================
// AgroFlow AI — Version 2.5 (Decisive Actuation at Sampling End)
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

// --- 2-MINUTE (120 SECONDS) SAMPLING ENGINE ---
bool isSampling            = true;
unsigned long sampleStartTime = 0;
const unsigned long SAMPLE_DURATION = 120000; // 2 Minutes = 120,000 ms
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

// Start fresh 2-minute sampling window (Servo stays resting at 0°)
void startSampling() {
  isSampling = true;
  isLocked = false;
  sampleStartTime = millis();
  moveServoSmoothly(0); // Sampling ke dauran servo 0° par shaant rahega
  broadcastMessage(F("{\"status\":\"SAMPLING_STARTED\",\"duration_seconds\":120,\"servo\":\"IDLE_0\"}"));
}

void processCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  // 'r' = Refresh / Catch Again for 2 minutes!
  if (cmd == "r" || cmd == "R" || cmd == "CMD:REFRESH" || cmd == "CMD:RESET") {
    startSampling();
  }
  else if (cmd == "s" || cmd == "S" || cmd == "CMD:VALVE:90") {
    isSampling = false;
    isLocked = true;
    lockedTDS = 650;
    moveServoSmoothly(90);
    broadcastMessage(F("{\"ack\":\"VALVE\",\"angle\":90,\"path\":\"BIO_FILTER\"}"));
  }
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

  moveServoSmoothly(0);
  startSampling(); // Startup par 2 minute sampling chalu
}

void loop() {
  unsigned long now = millis();

  // 1. Inbound Commands
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

  // 3. SOIL MOISTURE: 100% ALWAYS LIVE & NEVER LOCKED (VARY HOTA RAHEGA)
  int rawSoil = analogRead(PIN_SOIL);
  int soilMoisturePct = map(rawSoil, 650, 300, 0, 100);
  soilMoisturePct = constrain(soilMoisturePct, 0, 100);

  // 4. TDS SAMPLING & DECISIVE SERVO MOVEMENT AT THE END
  int currentTDS = 0;
  int secondsLeft = 0;

  if (isSampling) {
    unsigned long elapsed = now - sampleStartTime;
    if (elapsed < SAMPLE_DURATION) {
      secondsLeft = (SAMPLE_DURATION - elapsed) / 1000 + 1; // 120s countdown
      
      // Live read for display only
      int rawTDS = analogRead(PIN_TDS);
      currentTDS = map(rawTDS, 0, 1023, 0, 1500);
      if (currentTDS < 0) currentTDS = 0;

      // SAMPLING CHALTE WAQT SERVO BILKUL NAHI HILEGA!
      // (Servo 0° par chup-chap khada rahega)
      
    } else {
      // 2 MINUTES POORE HOGAYE -> AB SERVO EK HI BAAR FINAL ACTION LEGA!
      isSampling = false;
      isLocked = true;
      int rawTDS = analogRead(PIN_TDS);
      lockedTDS = map(rawTDS, 0, 1023, 0, 1500);
      if (lockedTDS < 0) lockedTDS = 0;

      currentTDS = lockedTDS;

      // FINAL EVALUATION & SINGLE DECISIVE MOVE:
      if (lockedTDS >= TDS_CUTOFF) {
        moveServoSmoothly(90); // Ganda paani -> Ek baar me 90° Filter!
      } else {
        moveServoSmoothly(0);  // Saaf paani -> 0° par hi rahega!
      }

      broadcastMessage(String("{\"alert\":\"DECISION_COMPLETE\",\"final_tds\":") + String(lockedTDS) + 
                       ",\"servo\":" + String(currentServoAngle) + "}");
    }
  } else {
    // Locked State: Servo & TDS freeze at their final decided position
    currentTDS = lockedTDS;
  }

  // 5. Telemetry Broadcast (Every 1 Second)
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
