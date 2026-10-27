
#include <SoftwareSerial.h>
#include <Servo.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// ---------- SETTINGS ----------
const bool USE_API_MODE = false;
const bool PRINT_RAW = false;
const bool PRINT_ALL = true;

const uint8_t XBEE_RX = 2;
const uint8_t XBEE_TX = 3;
const uint8_t SERVO_PIN = 8;
const uint8_t BUZZER_PIN = 4;

const uint16_t RELAY_ADDR = 4;
const char* RELAY_NAME = "TP7";
const char* MY_NAME = "SERVO";

// false = active buzzer; true = passive buzzer
const bool BUZZER_PASSIVE = false;
const int BUZZER_FREQ = 1000;

const unsigned long PRESENCE_TIMEOUT_MS = 0;

// ---------- GLOBALS ----------
SoftwareSerial xbee(XBEE_RX, XBEE_TX);
Servo servo;

int lastAngle = -1;
bool buzzerOn = false;

char lineBuf[40];
uint8_t lineLen = 0;

// ---------- BUZZER ----------
void setBuzzer(bool on) {
  if (on) {
    tone(BUZZER_PIN, 1000);
    Serial.println(F("BUZZER ON - 1000 Hz"));
  } else {
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println(F("BUZZER OFF"));
  }

  buzzerOn = on;
}

// ---------- SERVO ----------
void handlePot(int value) {
  value = constrain(value, 0, 1023);

  int angle = map(value, 0, 1023, 0, 180);

  if (angle != lastAngle) {
    servo.write(angle);
    lastAngle = angle;

    Serial.print(F("POT = "));
    Serial.print(value);
    Serial.print(F(" -> SERVO ANGLE = "));
    Serial.println(angle);
  }
}

// ---------- INTEGER PARSER ----------
bool parseInt(const char* s, int minimum, int maximum, int& out) {
  while (isspace((unsigned char)*s)) s++;

  char* end;
  long value = strtol(s, &end, 10);

  if (end == s) return false;

  while (isspace((unsigned char)*end)) end++;

  if (*end != '\0') return false;
  if (value < minimum || value > maximum) return false;

  out = (int)value;
  return true;
}

// ---------- PROCESS MESSAGE ----------
void processCommand(const char* type, const char* value) {
  int v;

  if (strcmp(type, "POT") == 0) {
    if (parseInt(value, 0, 1023, v)) {
      handlePot(v);
    } else {
      Serial.println(F("ERROR: Invalid POT value"));
    }
  }
  else if (strcmp(type, "PIR") == 0) {
    if (parseInt(value, 0, 1, v)) {
      Serial.print(F("PIR RECEIVED = "));
      Serial.println(v);

      setBuzzer(v == 1);
    } else {
      Serial.println(F("ERROR: Invalid PIR value"));
    }
  }
  else {
    Serial.print(F("UNKNOWN TYPE: "));
    Serial.println(type);
  }
}

// ---------- SIMPLE FORMAT ----------
void handleSimple(const char* line) {
  const char* colon = strchr(line, ':');

  if (!colon) {
    Serial.print(F("IGNORED: "));
    Serial.println(line);
    return;
  }

  char type[8];
  uint8_t n = 0;

  for (const char* p = line; p < colon && n < sizeof(type) - 1; p++) {
    if (!isspace((unsigned char)*p)) {
      type[n++] = toupper((unsigned char)*p);
    }
  }

  type[n] = '\0';

  processCommand(type, colon + 1);
}

// ---------- FULL MESSAGE FORMAT ----------
void handleMessage(const char* line) {
  if (strchr(line, ';') == NULL) {
    handleSimple(line);
    return;
  }

  char copy[40];
  strncpy(copy, line, sizeof(copy) - 1);
  copy[sizeof(copy) - 1] = '\0';

  char* src = strtok(copy, ";");
  char* dest = strtok(NULL, ";");
  char* type = strtok(NULL, ";");
  char* value = strtok(NULL, ";");

  if (!src || !dest || !type || !value) {
    Serial.println(F("IGNORED: Invalid message format"));
    return;
  }

  if (strcmp(src, RELAY_NAME) != 0) {
    Serial.println(F("IGNORED: Not from TP7"));
    return;
  }

  if (strcmp(dest, MY_NAME) != 0 &&
      strcmp(dest, "ALL") != 0) {
    Serial.print(F("IGNORED: Destination = "));
    Serial.println(dest);
    return;
  }

  Serial.print(F("ACCEPTED MESSAGE: "));
  Serial.println(line);

  processCommand(type, value);
}

// ---------- LINE READER ----------
void feedLine(char c) {
  if (c == '\r' || c == '\n') {
    if (lineLen > 0) {
      lineBuf[lineLen] = '\0';
      handleMessage(lineBuf);
      lineLen = 0;
    }
    return;
  }

  if (lineLen < sizeof(lineBuf) - 1) {
    lineBuf[lineLen++] = c;
  } else {
    lineLen = 0;
    Serial.println(F("ERROR: Message too long"));
  }
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(9600);
  xbee.begin(9600);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  servo.attach(SERVO_PIN);
  servo.write(90);
  lastAngle = 90;

  Serial.println(F("XBee SERVO RECEIVER STARTED"));
  Serial.println(F("XBee mode: TRANSPARENT (AP = 0)"));
  Serial.println(F("Buzzer startup test..."));

  setBuzzer(true);
  delay(300);
  setBuzzer(false);

  Serial.println(F("READY"));
  Serial.println(F("Test: TP7;SERVO;PIR;1"));
}

// ---------- SERIAL MONITOR TEST ----------
char pcBuf[40];
uint8_t pcLen = 0;

void readPC() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\r' || c == '\n') {
      if (pcLen > 0) {
        pcBuf[pcLen] = '\0';

        Serial.print(F("[SERIAL TEST] "));
        Serial.println(pcBuf);

        handleMessage(pcBuf);
        pcLen = 0;
      }
    }
    else if (pcLen < sizeof(pcBuf) - 1) {
      pcBuf[pcLen++] = c;
    }
    else {
      pcLen = 0;
      Serial.println(F("ERROR: Test line too long"));
    }
  }
}

// ---------- MAIN LOOP ----------
void loop() {
  while (xbee.available()) {
    char c = (char)xbee.read();

    if (PRINT_RAW) {
      if (c >= 32 && c < 127) {
        Serial.write(c);
      } else if (c != '\r') {
        Serial.print(F("["));
        Serial.print((uint8_t)c, HEX);
        Serial.print(F("]"));
      }
    }

    if (!USE_API_MODE) {
      feedLine(c);
    }
  }

  readPC();
}