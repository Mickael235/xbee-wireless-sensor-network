

#include <SoftwareSerial.h>

const uint8_t XBEE_RX = 2;
const uint8_t XBEE_TX = 3;
const uint8_t POT_PIN = A0;

const int CHANGE_THRESHOLD = 5;            // send only if value moved by more than this
const unsigned long RESEND_MS = 1000;      // also resend every 1 s even if no change
const unsigned long READ_INTERVAL_MS = 50; // read the pot 20 times per second

SoftwareSerial xbee(XBEE_RX, XBEE_TX);

int lastSent = -100;
unsigned long lastSendTime = 0;

void sendPot(int value) {
  xbee.print("POT:");
  xbee.println(value);

  Serial.print("Sent POT:");
  Serial.println(value);

  lastSent = value;
  lastSendTime = millis();
}

void setup() {
  Serial.begin(9600);   // USB debug
  xbee.begin(9600);     // must match XBee BD setting
  Serial.println("Pot sender ready");
}

void loop() {
  int value = analogRead(POT_PIN);   // 0 .. 1023

  bool changed = abs(value - lastSent) > CHANGE_THRESHOLD;
  bool timeToResend = millis() - lastSendTime >= RESEND_MS;

  if (changed || timeToResend) {
    sendPot(value);
  }

  delay(READ_INTERVAL_MS);
}
