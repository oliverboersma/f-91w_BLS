# F-91W BLE protocol

Device name `F-91W`. The watch only advertises on demand (hold LIGHT for 1.5 s, or ALARM on the
BLE screen) for 60 s, then stops. It advertises the Heart Rate service UUID.

All multi-byte values are little-endian.

## Standard services (no pairing needed)

| Service | Characteristic | Access | Value |
|---|---|---|---|
| Battery 0x180F | Battery Level 0x2A19 | read, notify | u8 percent (CR2016: 3.0 V = 100 %, 2.5 V = 0 %) |
| Heart Rate 0x180D | HR Measurement 0x2A37 | notify | flags 0x00, u8 bpm; sent once per successful spot check |
| | Body Sensor Location 0x2A38 | read | 2 (wrist) |
| Device Information 0x180A | Manufacturer Name | read | from ST's DIS app |

## F-91W service `6f910001-c2a6-4b3e-8f1d-0a5e3b7c9d10`

Characteristic UUIDs are the same with `0001` replaced. Reads and writes need an authenticated
bond (LE Secure Connections when the phone supports it, passkey entry). The watch shows the 6-digit passkey as `PA  123456`.

| UUID | Name | Access | Value |
|---|---|---|---|
| `6f910002-…` | Time | read, write | u32 unix time (UTC), i16 time-zone offset in minutes. Writing sets the clock. |
| `6f910003-…` | Steps | read, notify | u32 steps today (resets at local midnight) |
| `6f910004-…` | History | read | 7 × u32 steps per day, `[0]` = yesterday |
| `6f910005-…` | Heart rate | read, notify | u8 status, u8 bpm, u8 confidence %, u8 0, u32 unix time of the reading |
| `6f910006-…` | Control | write | opcode + args, see below |
| `6f910007-…` | Raw PPG | notify | u16 sequence number, then up to 6 × u24 green counts (25 sps) |
| `6f910008-…` | Settings | read, write | u8 flags (bit0 24 h, bit1 hourly chime, bit2 button beep), u8 LCD contrast 0-7, u8 backlight colour (bit0 R, bit1 G, bit2 B), u8 PPG LED current 1-31 mA |

Heart-rate status: 0 ok, 1 too short / timed out, 2 no skin contact (low perfusion), 3 moved,
4 no pulse found, 5 low confidence, 255 no PPG sensor answering.

### Control opcodes

| Bytes | Action |
|---|---|
| `01` | start a heart-rate spot check (~11 s); result arrives on Heart rate and on 0x2A37 |
| `02` | find watch: melody + white backlight |
| `03` | reboot into ST's BLE_Ota loader (only when the app was built with `make OTA=1`) |
| `04 xx` | raw PPG streaming during spot checks: `xx` = 1 on, 0 off |
| `06` | forget all bonds |
