#include <Arduino.h>
#include <MyLD2410.h>
#include <SPI.h>
#include <MFRC522.h>

// ================= ПИНЫ =================
#define LD2410_RX 16
#define LD2410_TX 17
#define RC522_SS   5
#define RC522_RST  4
// SPI (SCK/MOSI/MISO) - аппаратные пины ESP32: GPIO18/23/19
#define SIREN_PIN 32   // сирену перенесли с GPIO18, т.к. он занят под аппаратный SPI (SCK)
#define LED_PIN   2

// ================= НАСТРОЙКИ =================
const String authorizedUIDs[] = {
  "04A1B2C3",     // ← Замени на реальные UID своих карт (формат HEX, см. Serial Monitor)
  "9F8E7D6C"
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
MFRC522 rfid(RC522_SS, RC522_RST);

// ================= HELPERS =================
String uidToString(byte* uid, byte len) {
  String s = "";
  for (byte i = 0; i < len; i++) {
    if (uid[i] < 0x10) s += "0";
    s += String(uid[i], HEX);
  }
  s.toUpperCase();
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

  SPI.begin(); // SCK=18, MISO=19, MOSI=23 по умолчанию на ESP32
  rfid.PCD_Init();

  byte version = rfid.PCD_ReadRegister(rfid.VersionReg);
  if (version == 0x00 || version == 0xFF) {
    Serial.println("RC522 not found!");
    while (1) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(200);
    }
  }
  Serial.print("RC522 OK, version 0x");
  Serial.println(version, HEX);
  Serial.println("Garage Alarm READY | Cards: " + String(numAuthorized));
}

// ================= RFID =================
void checkRFID() {
  if (millis() - lastRFIDReadTime < RFID_DEBOUNCE_MS) return;

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String tag = uidToString(rfid.uid.uidByte, rfid.uid.size);
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

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
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
