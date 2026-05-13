#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// ================= NETWORK =================
const char* ssid = "YOUR_WIFI";
const char* password = "YOUR_PASSWORD";

#define BOT_TOKEN "YOUR_BOT_TOKEN"
#define CHAT_ID "YOUR_CHAT_ID"

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// ================= SENSOR PINS =================
// IR
#define IR1 34
#define IR2 35
#define IR3 21
#define IR4 22

// Ultrasonic
#define TRIG1 5
#define ECHO1 17
#define TRIG2 18
#define ECHO2 16
#define TRIG3 27
#define ECHO3 26
#define TRIG4 25
#define ECHO4 33

// FSR
#define FSR1 32
#define FSR2 13

// LDR + SOUND
#define LDR 36
#define SOUND 39

// ================= CORE VARIABLES =================
int currentZone = 0, lastZone = 0;

float baseScore = 0;
float confidence = 0;
float adaptiveThreshold = 0.5;

int suspicion = 0;

// Context
float lightLevel, soundLevel;
float environmentFactor = 1.0;

// Pattern Memory
int zoneHistory[50];
int historyIndex = 0;

// ================= ULTRASONIC =================
long getDistance(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long duration = pulseIn(echo, HIGH, 30000);
  return duration * 0.034 / 2;
}

// ================= ZONE =================
int getZone() {
  if (digitalRead(IR1) == 0) return 1;
  if (digitalRead(IR2) == 0) return 2;
  if (digitalRead(IR3) == 0) return 3;
  if (digitalRead(IR4) == 0) return 4;
  return 0;
}

// ================= PERMUTATION ENGINE =================
// (Represents 700+ contextual combinations)
float permutationEngine(int zone, float light, float sound, int fsrActive) {

  float weight = 0;

  // Zone weight
  weight += zone * 0.1;

  // Light context
  if (light < 1000) weight += 0.2;   // dark → more suspicious
  else weight += 0.05;

  // Sound context
  if (sound > 1200) weight += 0.2;

  // FSR contribution
  if (fsrActive) weight += 0.25;

  // Cross-condition logic (permutation core)
  if (zone == 2 && light < 800 && sound > 1000) weight += 0.3;
  if (zone == 3 && fsrActive && sound > 900) weight += 0.25;

  return weight;
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  pinMode(IR1, INPUT);
  pinMode(IR2, INPUT);
  pinMode(IR3, INPUT);
  pinMode(IR4, INPUT);

  pinMode(TRIG1, OUTPUT); pinMode(ECHO1, INPUT);
  pinMode(TRIG2, OUTPUT); pinMode(ECHO2, INPUT);
  pinMode(TRIG3, OUTPUT); pinMode(ECHO3, INPUT);
  pinMode(TRIG4, OUTPUT); pinMode(ECHO4, INPUT);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  client.setInsecure();
}

// ================= LOOP =================
void loop() {

  currentZone = getZone();

  // Sensor Readings
  float d1 = getDistance(TRIG1, ECHO1);
  float d2 = getDistance(TRIG2, ECHO2);
  float d3 = getDistance(TRIG3, ECHO3);
  float d4 = getDistance(TRIG4, ECHO4);

  int fsrActive = (analogRead(FSR1) > 500 || analogRead(FSR2) > 500);

  lightLevel = analogRead(LDR);
  soundLevel = analogRead(SOUND);

  // ================= CONTEXT MODEL =================
  if (lightLevel < 800) environmentFactor = 1.2;
  else environmentFactor = 0.9;

  // ================= BASE SCORE =================
  baseScore = 0;

  if (currentZone != 0) baseScore += 0.3;

  if (d1 < 100 || d2 < 100 || d3 < 100 || d4 < 100) baseScore += 0.2;

  if (fsrActive) baseScore += 0.2;

  // ================= PERMUTATION LOGIC =================
  float permScore = permutationEngine(currentZone, lightLevel, soundLevel, fsrActive);

  baseScore += permScore * environmentFactor;

  // ================= ADAPTIVE LEARNING =================
  adaptiveThreshold = (adaptiveThreshold * 0.9) + (baseScore * 0.1);

  confidence = baseScore / (adaptiveThreshold + 0.01);

  if (confidence > 1.0) confidence = 1.0;

  // ================= PATTERN MEMORY =================
  zoneHistory[historyIndex++] = currentZone;
  if (historyIndex >= 50) historyIndex = 0;

  int oscillation = 0;
  for (int i = 0; i < 48; i++) {
    if (zoneHistory[i] == zoneHistory[i + 2]) oscillation++;
  }

  if (oscillation > 5) suspicion += 2;

  // ================= BEHAVIOR MODEL =================
  if (currentZone != lastZone) {
    if (abs(currentZone - lastZone) > 2) {
      suspicion += 3; // jump behavior
    }
  }

  // ================= CLASSIFICATION =================
  String state;

  if (confidence > 0.85 || suspicion > 6) state = "CRITICAL_INTRUSION";
  else if (confidence > 0.6) state = "HIGH_ACTIVITY";
  else if (confidence > 0.3) state = "MODERATE_ACTIVITY";
  else state = "NORMAL";

  // ================= SERIAL =================
  Serial.print("Zone:");
  Serial.print(currentZone);
  Serial.print(" | Score:");
  Serial.print(baseScore, 2);
  Serial.print(" | Conf:");
  Serial.print(confidence, 2);
  Serial.print(" | Susp:");
  Serial.print(suspicion);
  Serial.print(" | State:");
  Serial.println(state);

  // ================= TELEGRAM =================
  if (state == "CRITICAL_INTRUSION") {
    String msg = "Alert\nZone:" + String(currentZone) +
                 "\nConfidence:" + String(confidence, 2) +
                 "\nSuspicion:" + String(suspicion);

    bot.sendMessage(CHAT_ID, msg, "");
    delay(3000);
  }

  lastZone = currentZone;

  delay(120);
}
