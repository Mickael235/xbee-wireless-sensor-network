# HUB firmware (MY = 2)

Place the validated Arduino sketch here as `hub.ino`, ensuring the IDE sketch folder is named `hub`.

Code checklist before publishing:
- Correct `SoftwareSerial xbee(2, 3)` pin mapping (shield DLINE selection).
- `Serial.begin(9600)` and `xbee.begin(9600)`.
- Check `strcmp(type, "PIR") == 0` (not `== 1`) in TP7 message parsing.
- Confirm `AP=0`, `DL=FFFF` only if matching the deployed radio configuration.
- Make sure PIR broadcasts sent to the LCD do not get re-forwarded by TP7.
- Recheck which messages are raw (`POT:391`, `PIR:1`) vs structured (`TP7;HUB;PIR;1`).
- Upload tested source only; this scaffold intentionally does not invent the final firmware.
