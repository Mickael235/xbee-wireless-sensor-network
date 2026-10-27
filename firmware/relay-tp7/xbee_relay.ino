/*
  XBee relay (802.15.4, 16-bit MY addresses)
  ------------------------------------------
  Addresses:
    MY 2 = hub
    MY 4 = this relay
    MY 5 = friend (sends to the relay)
    MY 6 = receiver (servo + buzzer), only receives

  Routes:
    from MY 2 (hub)    -> forward to MY 6 (receiver: servo + buzzer)
    from MY 5 (friend) -> forward to MY 2 (hub)
  Everything received is also printed in the Serial Monitor (9600 baud).

  WHY API MODE:
    In transparent mode an XBee can't tell WHO sent a message and always sends
    to the single DL address. A relay needs both, so THIS module runs in
    API mode 1 (AP = 1). The other modules can stay in transparent mode (AP = 0).

  XCTU settings for the RELAY XBee:
    MY = 4        (this relay's address)
    AP = 1        (API enabled, no escaping)   <-- important
    BD = 3        (9600)
    CH, ID        same as all other modules
    DL / DH       not used in API mode

  Other modules (transparent mode, AP = 0):
    Hub    (MY 2): DL = 4   -> sends to the relay
    Friend (MY 5): DL = 4   -> sends to the relay
    You    (MY 6): just receives (its DL doesn't matter for receiving)

  Wiring:
    XBee DOUT -> Arduino pin 2 (RX)
    XBee DIN  -> Arduino pin 3 (TX)   (XBee shield / level shifter, XBee is 3.3V)
*/

#include <SoftwareSerial.h>

SoftwareSerial xbee(2, 3);

// ---------- Routing table: edit here if addresses change ----------
struct Route { uint16_t from; uint16_t to; };
const Route ROUTES[] = {
  { 2, 6 },   // hub (2)    -> you, the receiver (6)
  { 5, 2 },   // friend (5) -> hub (2)
  // { 5, 6 },   // uncomment if friend 5's messages should ALSO go to you
};
const uint8_t NUM_ROUTES = sizeof(ROUTES) / sizeof(ROUTES[0]);

const bool PRINT_ALL = true;   // print every message received / forwarded

// ---------- API frame reader ----------
const uint8_t MAX_FRAME = 110;   // 802.15.4 payload is max 100 bytes
uint8_t  frame[MAX_FRAME];
uint16_t frameLen = 0;
uint16_t idx = 0;
uint8_t  sum = 0;
uint8_t  state = 0;              // 0 start, 1 lenH, 2 lenL, 3 data, 4 checksum
uint8_t  nextFrameId = 1;

// Print payload as readable text
void printPayload(const uint8_t *data, uint8_t n) {
  for (uint8_t i = 0; i < n; i++) {
    char c = data[i];
    if (c == '\r') continue;
    if (c == '\n') Serial.print(F("\\n"));
    else if (c >= 32 && c < 127) Serial.write(c);
    else { Serial.print('['); Serial.print((uint8_t)c, HEX); Serial.print(']'); }
  }
  Serial.println();
}

// Send data to a 16-bit address with a TX Request (API id 0x01)
void sendTo(uint16_t dest, const uint8_t *data, uint8_t n) {
  uint8_t fid = nextFrameId++;
  if (nextFrameId == 0) nextFrameId = 1;     // 0 would disable TX status

  uint16_t len = 5 + n;                      // apiId + fid + dest(2) + options + data
  uint8_t cs = 0;

  xbee.write(0x7E);
  xbee.write(len >> 8);
  xbee.write(len & 0xFF);

  uint8_t header[5] = { 0x01, fid, (uint8_t)(dest >> 8), (uint8_t)(dest & 0xFF), 0x00 };
  for (uint8_t i = 0; i < 5; i++) { xbee.write(header[i]); cs += header[i]; }
  for (uint8_t i = 0; i < n; i++) { xbee.write(data[i]);   cs += data[i]; }

  xbee.write(0xFF - cs);
}

void handleFrame() {
  uint8_t apiId = frame[0];

  if (apiId == 0x81 && frameLen >= 5) {          // RX packet, 16-bit address
    uint16_t src  = ((uint16_t)frame[1] << 8) | frame[2];
    uint8_t  rssi = frame[3];
    uint8_t *data = frame + 5;
    uint8_t  n    = frameLen - 5;

    if (PRINT_ALL) {
      Serial.print(F("From ")); Serial.print(src);
      Serial.print(F(" (-")); Serial.print(rssi); Serial.print(F(" dBm): "));
      printPayload(data, n);
    }

    bool routed = false;
    for (uint8_t r = 0; r < NUM_ROUTES; r++) {
      if (ROUTES[r].from == src) {
        sendTo(ROUTES[r].to, data, n);
        routed = true;
        if (PRINT_ALL) {
          Serial.print(F("   -> forwarded to ")); Serial.println(ROUTES[r].to);
        }
      }
    }
    if (!routed && PRINT_ALL) Serial.println(F("   (no route, not forwarded)"));
  }
  else if (apiId == 0x89 && frameLen >= 3) {     // TX status
    uint8_t status = frame[2];
    if (status != 0) {
      Serial.print(F("   !! Send failed, frame ")); Serial.print(frame[1]);
      Serial.print(F(" status ")); Serial.print(status);
      Serial.println(status == 1 ? F(" (no ACK: destination off or wrong address?)")
                                 : F(""));
    }
  }
  else if (PRINT_ALL) {
    Serial.print(F("Other API frame 0x")); Serial.println(apiId, HEX);
  }
}

void readXBee() {
  while (xbee.available()) {
    uint8_t c = xbee.read();
    switch (state) {
      case 0:
        if (c == 0x7E) state = 1;
        break;
      case 1:
        frameLen = (uint16_t)c << 8;
        state = 2;
        break;
      case 2:
        frameLen |= c;
        if (frameLen == 0 || frameLen > MAX_FRAME) { state = 0; break; }
        idx = 0; sum = 0;
        state = 3;
        break;
      case 3:
        frame[idx++] = c;
        sum += c;
        if (idx >= frameLen) state = 4;
        break;
      case 4:
        if ((uint8_t)(sum + c) == 0xFF) handleFrame();
        else Serial.println(F("Bad checksum, frame dropped (is AP = 1 ?)"));
        state = 0;
        break;
    }
  }
}

void setup() {
  Serial.begin(9600);
  xbee.begin(9600);
  Serial.println(F("Relay ready (MY 4, API mode)"));
  for (uint8_t r = 0; r < NUM_ROUTES; r++) {
    Serial.print(F("  route: ")); Serial.print(ROUTES[r].from);
    Serial.print(F(" -> "));      Serial.println(ROUTES[r].to);
  }
}

void loop() {
  readXBee();
}
