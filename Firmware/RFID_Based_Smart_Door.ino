// ============================================================
//  RFID-Based Smart Door System
//  Author : Pratik Pisal
//  Platform : ESP32 Dev Module
//  Framework: Arduino (ESP-IDF via arduino-esp32)
// ============================================================

#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <JQ6500_Serial.h>

// OTA headers
#include <WiFi.h>
#include <ArduinoOTA.h>

// ============================================================
//  Wi-Fi credentials  (change before flashing)
// ============================================================
#define WIFI_SSID     "YOUR_SSID"
#define WIFI_PASSWORD "YOUR_PASSWORD"
#define OTA_HOSTNAME  "smart-door-esp32"
#define OTA_PASSWORD  "ota_password"   // optional – remove line to disable

// ============================================================
//  Pin map
// ============================================================
#define SS_PIN           5
#define RST_PIN         27

#define PIR_PIN         13
#define PUSH_BTN_PIN    12

#define R_EN            32
#define L_EN            33
#define R_PWM           25
#define L_PWM           26
#define LOCK_PIN         4

#define DOOR_OPEN_LIMIT  14
#define DOOR_CLOSE_LIMIT 15

// ============================================================
//  Motor PWM config
// ============================================================
#define PWM_CHANNEL_R    0
#define PWM_CHANNEL_L    1
#define PWM_FREQ      1000   // Hz
#define PWM_BITS         8   // 8-bit → 0-255

#define MOTOR_FULL_SPEED   255
#define MOTOR_SLOW_SPEED   180
#define MOTOR_RAMP_MS     3000   // time before speed reduction

// ============================================================
//  Audio track index map
// ============================================================
#define AUDIO_ACTIVATED    1
#define AUDIO_DEACTIVATED  2
#define AUDIO_DOOR_OPEN    3
#define AUDIO_DOOR_CLOSE   4
#define AUDIO_ACCESS_DENY  5
#define AUDIO_OTA_START    6   // optional – add a track for OTA

// ============================================================
//  EEPROM
// ============================================================
#define EEPROM_SIZE      1
#define EEPROM_ADDR      0

// ============================================================
//  Timing constants
// ============================================================
#define DEBOUNCE_MS      200
#define DISPLAY_HOLD_MS 3000
#define MP3_INIT_DELAY  1000
#define OPEN_AUDIO_DELAY 200
#define CLOSE_AUDIO_DELAY 100

// ============================================================
//  Authorised RFID UIDs  – 4 bytes each
//  Add / remove rows as needed.
// ============================================================
static const byte AUTHORISED_UIDS[][4] = {
  {0x44, 0x34, 0xDD, 0x00},
  {0xEF, 0x76, 0x26, 0x1E},
  {0x68, 0x00, 0x0A, 0x78},
  {0xD5, 0x0B, 0xAA, 0x72},
  {0xE8, 0x29, 0x85, 0x77},
  {0xB3, 0x50, 0xF6, 0x13}
};
static const int NUM_CARDS = sizeof(AUTHORISED_UIDS) / sizeof(AUTHORISED_UIDS[0]);

// ============================================================
//  Peripheral objects
// ============================================================
MFRC522          rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);
JQ6500_Serial    mp3(Serial2);

// ============================================================
//  Global state
// ============================================================
bool systemActive = false;
bool doorOpen     = false;

// ============================================================
//  Forward declarations
// ============================================================
void setupPWM();
void setupOTA();
void setupWiFi();
void stopMotor();
void openDoor();
void closeDoor();
void setLock(bool engage);
void showStatus();
void saveState();
void loadState();
bool isAuthorised(const byte *uid, byte size);
void lcdMsg(const char *row0, const char *row1 = nullptr);

// ============================================================
//  setup()
// ============================================================
void setup() {
  Serial.begin(115200);

  // --- GPIO ---
  pinMode(PIR_PIN,          INPUT_PULLUP);
  pinMode(PUSH_BTN_PIN,     INPUT_PULLUP);
  pinMode(DOOR_OPEN_LIMIT,  INPUT_PULLUP);
  pinMode(DOOR_CLOSE_LIMIT, INPUT_PULLUP);
  pinMode(LOCK_PIN,         OUTPUT);
  pinMode(R_EN,             OUTPUT);
  pinMode(L_EN,             OUTPUT);

  // --- PWM channels ---
  setupPWM();
  stopMotor();

  // --- EEPROM & lock ---
  EEPROM.begin(EEPROM_SIZE);
  loadState();
  setLock(systemActive);

  // --- SPI / RFID ---
  SPI.begin();
  rfid.PCD_Init();

  // --- Audio ---
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
  delay(MP3_INIT_DELAY);
  mp3.reset();
  delay(MP3_INIT_DELAY);
  mp3.setVolume(25);

  // --- LCD ---
  lcd.begin();
  lcd.backlight();
  showStatus();

  // --- Wi-Fi & OTA ---
  setupWiFi();
  setupOTA();

  Serial.println("[BOOT] System ready.");
}

// ============================================================
//  loop()
// ============================================================
void loop() {
  // Always handle OTA requests first
  ArduinoOTA.handle();

  // ── RFID scan ──────────────────────────────────────────────
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    if (isAuthorised(rfid.uid.uidByte, rfid.uid.size)) {
      systemActive = !systemActive;
      saveState();

      if (systemActive) {
        setLock(true);
        mp3.playFileByIndexNumber(AUDIO_ACTIVATED);
        lcdMsg("System Activated", "   Welcome!");
      } else {
        setLock(false);
        mp3.playFileByIndexNumber(AUDIO_DEACTIVATED);
        lcdMsg("    System", " Deactivated");
        stopMotor();
        doorOpen = false;
      }

      delay(DISPLAY_HOLD_MS);
      showStatus();

    } else {
      mp3.playFileByIndexNumber(AUDIO_ACCESS_DENY);
      lcdMsg("Access Denied!", nullptr);
      delay(DEBOUNCE_MS * 10);   // 2 s hold
      showStatus();
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }

  // ── Door automation (only when active) ────────────────────
  if (!systemActive) return;

  const bool pirActive = (digitalRead(PIR_PIN)      == LOW);
  const bool btnActive = (digitalRead(PUSH_BTN_PIN) == LOW);

  if (!doorOpen && (pirActive || btnActive)) {
    Serial.println(pirActive ? "[SENS] PIR → Opening" : "[SENS] BTN → Opening");
    openDoor();
    doorOpen = true;
  }

  if (doorOpen && !pirActive && !btnActive) {
    Serial.println("[SENS] No input → Closing");
    closeDoor();
    doorOpen = false;
  }

  delay(DEBOUNCE_MS);
}

// ============================================================
//  Wi-Fi helper
// ============================================================
void setupWiFi() {
  Serial.printf("[WIFI] Connecting to %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Non-blocking wait – give up after 15 s so the door still works offline
  const unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) {
    delay(500);
    Serial.print('.');
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WIFI] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WIFI] Offline – OTA unavailable.");
  }
}

// ============================================================
//  OTA setup
// ============================================================
void setupOTA() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    Serial.println("[OTA] Starting update…");
    mp3.playFileByIndexNumber(AUDIO_OTA_START);
    lcdMsg("OTA Update", "Please wait...");
    stopMotor();
    setLock(false);   // fail-safe: release lock during flash
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Complete – rebooting.");
    lcdMsg("Update Done!", "Rebooting...");
  });

  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    static uint8_t lastPct = 255;
    uint8_t pct = (done * 100) / total;
    if (pct != lastPct) {
      Serial.printf("[OTA] %u%%\r", pct);
      lastPct = pct;
    }
  });

  ArduinoOTA.onError([](ota_error_t err) {
    Serial.printf("[OTA] Error[%u]: ", err);
    const char *msg = (err == OTA_AUTH_ERROR)    ? "Auth failed"   :
                      (err == OTA_BEGIN_ERROR)   ? "Begin failed"  :
                      (err == OTA_CONNECT_ERROR) ? "Connect fail"  :
                      (err == OTA_RECEIVE_ERROR) ? "Receive fail"  :
                      (err == OTA_END_ERROR)     ? "End failed"    : "Unknown";
    Serial.println(msg);
    lcdMsg("OTA Error!", msg);
  });

  ArduinoOTA.begin();
  Serial.println("[OTA] Ready.");
}

// ============================================================
//  Motor helpers
// ============================================================
void setupPWM() {
  ledcSetup(PWM_CHANNEL_R, PWM_FREQ, PWM_BITS);
  ledcAttachPin(R_PWM, PWM_CHANNEL_R);

  ledcSetup(PWM_CHANNEL_L, PWM_FREQ, PWM_BITS);
  ledcAttachPin(L_PWM, PWM_CHANNEL_L);
}

void stopMotor() {
  ledcWrite(PWM_CHANNEL_R, 0);
  ledcWrite(PWM_CHANNEL_L, 0);
  digitalWrite(R_EN, LOW);
  digitalWrite(L_EN, LOW);
}

// Drive motor in one direction; chR is the active channel, chI is inactive.
static void driveMotor(uint8_t chActive, uint8_t chInactive, uint8_t speed) {
  ledcWrite(chActive,   speed);
  ledcWrite(chInactive, 0);
}

// ============================================================
//  Door motion
// ============================================================
void openDoor() {
  Serial.println("[DOOR] Opening…");
  mp3.playFileByIndexNumber(AUDIO_DOOR_OPEN);
  delay(OPEN_AUDIO_DELAY);

  digitalWrite(R_EN, HIGH);
  digitalWrite(L_EN, HIGH);

  unsigned long startTime = millis();
  bool reducedSpeed = false;

  while (digitalRead(DOOR_OPEN_LIMIT) == HIGH) {
    ArduinoOTA.handle();  // keep OTA responsive during motion

    if (!systemActive) {
      Serial.println("[DOOR] Deactivated mid-open → aborting");
      stopMotor();
      closeDoor();
      return;
    }

    unsigned long elapsed = millis() - startTime;
    if (elapsed < MOTOR_RAMP_MS) {
      driveMotor(PWM_CHANNEL_R, PWM_CHANNEL_L, MOTOR_FULL_SPEED);
    } else {
      if (!reducedSpeed) {
        Serial.println("[DOOR] Speed reduced");
        reducedSpeed = true;
      }
      driveMotor(PWM_CHANNEL_R, PWM_CHANNEL_L, MOTOR_SLOW_SPEED);
    }

    delay(10);
  }

  Serial.println("[DOOR] Open limit reached");
  stopMotor();
}

void closeDoor() {
  Serial.println("[DOOR] Closing…");
  mp3.playFileByIndexNumber(AUDIO_DOOR_CLOSE);
  delay(CLOSE_AUDIO_DELAY);

  digitalWrite(R_EN, HIGH);
  digitalWrite(L_EN, HIGH);

  unsigned long startTime = millis();
  bool reducedSpeed = false;

  while (digitalRead(DOOR_CLOSE_LIMIT) == HIGH) {
    ArduinoOTA.handle();  // keep OTA responsive during motion

    if (!systemActive) {
      Serial.println("[DOOR] Deactivated mid-close → stopping");
      stopMotor();
      return;
    }

    unsigned long elapsed = millis() - startTime;
    if (elapsed < MOTOR_RAMP_MS) {
      driveMotor(PWM_CHANNEL_L, PWM_CHANNEL_R, MOTOR_FULL_SPEED);
    } else {
      if (!reducedSpeed) {
        Serial.println("[DOOR] Speed reduced");
        reducedSpeed = true;
      }
      driveMotor(PWM_CHANNEL_L, PWM_CHANNEL_R, MOTOR_SLOW_SPEED);
    }

    delay(5);
  }

  Serial.println("[DOOR] Close limit reached");
  stopMotor();
}

// ============================================================
//  Solenoid lock
// ============================================================
void setLock(bool engage) {
  digitalWrite(LOCK_PIN, engage ? HIGH : LOW);
  Serial.printf("[LOCK] %s\n", engage ? "Engaged" : "Released");
}

// ============================================================
//  RFID authorisation
// ============================================================
bool isAuthorised(const byte *uid, byte size) {
  if (size != 4) return false;   // all stored UIDs are 4 bytes
  for (int i = 0; i < NUM_CARDS; i++) {
    if (memcmp(uid, AUTHORISED_UIDS[i], 4) == 0) return true;
  }
  return false;
}

// ============================================================
//  LCD helpers
// ============================================================
void lcdMsg(const char *row0, const char *row1) {
  lcd.clear();
  if (row0) { lcd.setCursor(0, 0); lcd.print(row0); }
  if (row1) { lcd.setCursor(0, 1); lcd.print(row1); }
}

void showStatus() {
  lcdMsg(systemActive ? "System: ACTIVE" : "System: INACTV",
         "Scan RFID Card");
}

// ============================================================
//  EEPROM persistence
// ============================================================
void saveState() {
  EEPROM.write(EEPROM_ADDR, static_cast<uint8_t>(systemActive));
  EEPROM.commit();
}

void loadState() {
  systemActive = static_cast<bool>(EEPROM.read(EEPROM_ADDR));
}
