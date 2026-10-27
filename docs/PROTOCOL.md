# Application message protocol

XBee PRO 802.15.4 S1 in transparent mode (`AP=0`) passes serial data. Newline delimits application messages. Intended text formats:

| Origin | Message | Expected consumer |
|---|---|---|
| Potentiometer | `POT:391` | HUB |
| PIR | `0` / `1` or `PIR:1` depending on sensor firmware | TP7 |
| HUB | `POT:391` broadcast | LCD and TP7 |
| TP7 | `TP7;HUB;PIR;1` | HUB |
| TP7 | `TP7;SERVO;PIR;1` | Servo/buzzer |
| TP7 | `TP7;SERVO;POT;391` | Servo |
| Servo | `SERVO;TP7;BUZZER;ON` if supported | Relay |
| TP7 | `TP7;HUB;STATUS;OK` | HUB |

**Implementation discrepancy to resolve:** The TP7 team report describes dynamic changes to `DL` using AT command mode; earlier shared relay code uses broadcast `DL=FFFF`. These are distinct firmware versions. Record the tested version and verify routing end-to-end before final release.
