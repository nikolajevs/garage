#include <Arduino.h>
#include <MyLD2410.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Preferences.h>

// ================= ПИНЫ =================
#define LD2410_RX 16
#define LD2410_TX 17
#define RC522_SS   5
#define RC522_RST  4
// SPI (SCK/MOSI/MISO) - аппаратные пины ESP32: GPIO18/23/19
#define SIREN_PIN 32   // сирену перенесли с GPIO18, т.к. он занят под аппаратный SPI (SCK)
#define LED_PIN   2

// ================= НАСТРОЙКИ =================
const unsigned long EXIT_DELAY_MS   = 30000;
const unsigned long ENTRY_DELAY_MS  = 15000;
const unsigned long ALARM_TIMEOUT   = 180000;
const unsigned long MOTION_HOLD_MS  = 1000;
const unsigned long RFID_DEBOUNCE_MS = 600;
const unsigned long ADD_MODE_TIMEOUT_MS = 15000; // время на поднесение новой карты после мастер-карты
const int MAX_CARDS = 20;

// ================= СОСТОЯНИЯ =================
enum State { DISARMED, EXIT_DELAY, ARMED, ENTRY_DELAY, ALARM };
State state = DISARMED;
unsigned long stateTimer = 0;
unsigned long lastMotionTime = 0;
unsigned long lastRFIDReadTime = 0;

bool addModeActive = false;
unsigned long addModeStartedAt = 0;

// ================= УСТРОЙСТВА =================
MyLD2410 radar(Serial2);
MFRC522 rfid(RC522_SS, RC522_RST);
Preferences prefs;

// ================= ХРАНЕНИЕ КАРТ (NVS) =================
String masterUID = "";               // пусто = мастер-карта ещё не назначена
String storedCards[MAX_CARDS];
int storedCardCount = 0;

void loadFromNVS() {
  prefs.begin("garage", true);
  masterUID = prefs.getString("master", "");
  storedCardCount = prefs.getInt("count", 0);
  if (storedCardCount > MAX_CARDS) storedCardCount = MAX_CARDS;
  for (int i = 0; i < storedCardCount; i++) {
    storedCards[i] = prefs.getString(("card" + String(i)).c_str(), "");
  }
  prefs.end();
}

void saveMaster() {
  prefs.begin("garage", false);
  prefs.putString("master", masterUID);
  prefs.end();
}

void saveCards() {
  prefs.begin("garage", false);
  prefs.putInt("count", storedCardCount);
  for (int i = 0; i < storedCardCount; i++) {
    prefs.putString(("card" + String(i)).c_str(), storedCards[i]);
  }
  prefs.end();
}

bool isKnownCard(const String &uid) {
  for (int i = 0; i < storedCardCount; i++) {
    if (storedCards[i] == uid) return true;
  }
  return false;
}

bool addCard(const String &uid) {
  if (isKnownCard(uid)) return false;
  if (storedCardCount >= MAX_CARDS) return false;
  storedCards[storedCardCount++] = uid;
  saveCards();
  return true;
}

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

void blink(int times, int onMs, int offMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(onMs);
    digitalWrite(LED_PIN, LOW);
    if (i < times - 1) delay(offMs);
  }
}

void sirenBeep(int ms) {
  digitalWrite(SIREN_PIN, HIGH);
  delay(ms);
  digitalWrite(SIREN_PIN, LOW);
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

  loadFromNVS();

  if (masterUID == "") {
    Serial.println("Мастер-карта не назначена! Поднесите любую карту к RC522 - она станет мастер-картой.");
  } else {
    Serial.println("Мастер-карта: " + masterUID);
  }
  Serial.println("Обычных карт в списке: " + String(storedCardCount));
  Serial.println("Serial-команды: list, clear, resetmaster");
  Serial.println("Garage Alarm READY");
}

// ================= RFID =================
void checkRFID() {
  if (millis() - lastRFIDReadTime < RFID_DEBOUNCE_MS) return;
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String tag = uidToString(rfid.uid.uidByte, rfid.uid.size);
  lastRFIDReadTime = millis();
  Serial.println("TAG: " + tag);

  // --- Назначение мастер-карты при первом использовании ---
  if (masterUID == "") {
    masterUID = tag;
    saveMaster();
    Serial.println("Мастер-карта назначена: " + tag);
    blink(5, 100, 100);
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return;
  }

  // --- Поднесли мастер-карту: переключаем режим добавления ---
  if (tag == masterUID) {
    if (state != DISARMED) {
      Serial.println("Режим добавления карт доступен только при снятой охране.");
      blink(2, 400, 200);
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
      return;
    }
    addModeActive = !addModeActive;
    if (addModeActive) {
      addModeStartedAt = millis();
      Serial.println("Режим добавления ВКЛЮЧЁН. Поднесите новую карту в течение 15 сек.");
      blink(3, 150, 150);
    } else {
      Serial.println("Режим добавления выключен.");
      blink(1, 500, 0);
    }
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return;
  }

  // --- Режим добавления активен: сохраняем новую карту ---
  if (addModeActive) {
    addModeActive = false;
    if (addCard(tag)) {
      Serial.println("Карта добавлена: " + tag);
      blink(3, 100, 100);
    } else {
      Serial.println("Карта уже есть в списке или лимит карт исчерпан: " + tag);
      blink(5, 80, 80);
    }
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return;
  }

  // --- Обычная карта: ставим/снимаем с охраны ---
  if (isKnownCard(tag)) {
    if (state == DISARMED) {
      setState(EXIT_DELAY);
    } else {
      setState(DISARMED);
    }
    sirenBeep(150);
  } else {
    Serial.println("Access DENIED: " + tag);
    blink(1, 600, 0);
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// ================= SERIAL КОМАНДЫ =================
void checkSerialCommands() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toLowerCase();

  if (cmd == "list") {
    Serial.println("Мастер-карта: " + (masterUID == "" ? "не назначена" : masterUID));
    Serial.println("Обычные карты (" + String(storedCardCount) + "):");
    for (int i = 0; i < storedCardCount; i++) {
      Serial.println("  " + storedCards[i]);
    }
  } else if (cmd == "clear") {
    storedCardCount = 0;
    saveCards();
    Serial.println("Список обычных карт очищен (мастер-карта сохранена).");
  } else if (cmd == "resetmaster") {
    masterUID = "";
    saveMaster();
    Serial.println("Мастер-карта сброшена. Поднесите новую карту, чтобы назначить её мастер-картой.");
  } else if (cmd.length() > 0) {
    Serial.println("Команды: list, clear, resetmaster");
  }
}

// ================= LOOP =================
void loop() {
  checkSerialCommands();
  checkRFID();

  // автоматический выход из режима добавления по таймауту
  if (addModeActive && millis() - addModeStartedAt > ADD_MODE_TIMEOUT_MS) {
    addModeActive = false;
    Serial.println("Время на добавление карты истекло, режим добавления выключен.");
  }

  bool motion = false;
  if (radar.read()) {
    motion = radar.isMoving() || radar.isStill();
    if (motion) lastMotionTime = millis();
  }

  unsigned long now = millis();

  // индикация режима добавления имеет приоритет над обычной индикацией LED
  if (addModeActive) {
    digitalWrite(LED_PIN, (now / 200) % 2);
  } else {
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
  }

  delay(10);
}
