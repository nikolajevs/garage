#include <Arduino.h>
#include <MyLD2410.h>
#include <PN532_HSU.h>
#include <PN532.h>

// ================= ПИНЫ =================
#define LD2410_RX 16
#define LD2410_TX 17
#define RFID_RX   4
#define RFID_TX   5
#define SIREN_PIN 18
#define LED_PIN   2

// ================= НАСТРОЙКИ =================
const String authorizedUIDs[] = {
  "4.123.45.67",     // ← Замени на реальные UID
  "12.34.56.78"
};
const int numAuthorized = sizeof(authorizedUIDs) / sizeof(authorizedUIDs[0]);

const unsigned long EXIT_DELAY_MS   = 30000;
const unsigned long ENTRY_DELAY_MS  = 15000;
const unsigned long ALARM_TIMEOUT   = 180000;
const unsigned long MOTION_HOLD_MS  = 1000;
const unsigned long RFID_DEBOUNCE_MS = 600;

// ================= СОСТОЯНИЯ =================
enum State { DISARMED, EXIT_DELAY, ARMED, ENTRY_DELAY, ALARM };
State state = DISARMED;
unsigned long stateTimer = 0;
unsigned long lastMotionTime = 0;
unsigned long lastRFIDReadTime = 0;

// ================= УСТРОЙСТВА =================
MyLD2410 radar(Serial2);
HardwareSerial RFIDSerial(1);
PN532_HSU pn532hsu(RFIDSerial);
PN532 nfc(pn532hsu);

// ================= HELPERS =================
String uidToString(uint8_t* uid, uint8_t len) {
  String s = "";
  for (uint8_t i = 0; i < len; i++) {
    if (i) s += ".";
    s += String(uid[i]);
  }
  return s;
}

bool isAuthorized(const String& tag) {
  for (int i = 0; i < numAuthorized; i++) {
    if (tag == authorizedUIDs[i]) return true;
  }
  return false;
}

void setState(State s) {
  state = s;
  stateTimer = millis();
  Serial.printf("State -> %d\n", s);
  if (s == DISARMED) {
    digitalWrite(SIREN_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  pinMode(SIREN_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(SIREN_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  Serial2.begin(256000, SERIAL_8N1, LD2410_RX, LD2410_TX);
  radar.begin();

  RFIDSerial.begin(115200, SERIAL_8N1, RFID_RX, RFID_TX);
  nfc.begin();

  uint32_t version = nfc.getFirmwareVersion();
  if (!version) {
    Serial.println("PN532 not found!");
    while (1) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(200);
    }
  }
  nfc.SAMConfig();
  Serial.println("Garage Alarm READY | Cards: " + String(numAuthorized));
}

// ================= RFID =================
void checkRFID() {
  if (millis() - lastRFIDReadTime < RFID_DEBOUNCE_MS) return;

  uint8_t uid[7] = {0};
  uint8_t len = 0;

  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &len, 50)) {
    String tag = uidToString(uid, len);
    Serial.println("TAG: " + tag);

    if (isAuthorized(tag)) {
      if (state == DISARMED) {
        setState(EXIT_DELAY);
      } else {
        setState(DISARMED);
      }
      digitalWrite(SIREN_PIN, HIGH);
      delay(150);
      digitalWrite(SIREN_PIN, LOW);
      lastRFIDReadTime = millis();
    } else {
      Serial.println("Access DENIED");
    }
  }
}

// ================= LOOP =================
void loop() {
  checkRFID();

  bool motion = false;
  if (radar.read()) {
    motion = radar.isMoving() || radar.isStill();
    if (motion) lastMotionTime = millis();
  }

  unsigned long now = millis();

  switch (state) {
    case DISARMED:
      digitalWrite(LED_PIN, LOW);
      break;
    case EXIT_DELAY:
      digitalWrite(LED_PIN, (now / 300) % 2);
      if (now - stateTimer > EXIT_DELAY_MS) setState(ARMED);
      break;
    case ARMED:
      digitalWrite(LED_PIN, HIGH);
      if (motion && (now - lastMotionTime >= MOTION_HOLD_MS)) {
        setState(ENTRY_DELAY);
      }
      break;
    case ENTRY_DELAY:
      digitalWrite(LED_PIN, (now / 150) % 2);
      if (now - stateTimer > ENTRY_DELAY_MS) setState(ALARM);
      break;
    case ALARM:
      digitalWrite(SIREN_PIN, HIGH);
      digitalWrite(LED_PIN, (now / 80) % 2);
      if (now - stateTimer > ALARM_TIMEOUT) {
        digitalWrite(SIREN_PIN, LOW);
      }
      break;
  }

  delay(10);
}