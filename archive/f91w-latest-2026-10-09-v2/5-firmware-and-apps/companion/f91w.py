#!/usr/bin/env python3
"""F-91W BLE companion (command line). Needs Python 3.9+ and `pip install bleak`.

    python3 f91w.py sync                 set the watch clock from this computer, print steps/HR/battery
    python3 f91w.py hr                   start a heart-rate spot check and wait for the result
    python3 f91w.py ppg out.csv          spot check while logging raw PPG samples (for tuning the algorithm)
    python3 f91w.py find                 make the watch beep
    python3 f91w.py set --24h 1 --chime 0 --beep 1 --contrast 4 --light green --led-ma 20
    python3 f91w.py ota                  reboot into ST's BLE_Ota loader (then use ST BLE Toolbox)
    python3 f91w.py forget               clear the watch's bonds

Press LIGHT for 1.5 s (or ALARM on the BLE screen) to make the watch advertise. The first time,
your OS asks for a passkey: it is shown on the watch display.
"""
import argparse, asyncio, struct, sys, time
from datetime import datetime

from bleak import BleakClient, BleakScanner

BASE = "6f91{:04x}-c2a6-4b3e-8f1d-0a5e3b7c9d10"
SVC, TIME, STEPS, HIST, HR, CTRL, PPG, SETTINGS = (BASE.format(i) for i in range(1, 9))
BATTERY = "00002a19-0000-1000-8000-00805f9b34fb"

HR_STATUS = {0: "ok", 1: "too short", 2: "no skin contact", 3: "moved", 4: "no pulse found",
             5: "low confidence", 0xFF: "no PPG sensor"}
COLORS = {"off": 0, "red": 1, "green": 2, "yellow": 3, "blue": 4, "magenta": 5, "cyan": 6, "white": 7}


async def find_watch(timeout=15.0):
    print("Scanning (hold LIGHT on the watch for 1.5 s to advertise)...")
    dev = await BleakScanner.find_device_by_filter(
        lambda d, ad: SVC in [u.lower() for u in ad.service_uuids] or (d.name or "").startswith("F-91W"),
        timeout=timeout)
    if dev is None:
        sys.exit("watch not found")
    return dev


def tz_minutes():
    return int(datetime.now().astimezone().utcoffset().total_seconds() // 60)


def fmt_hr(b):
    status, bpm, conf, _, ts = struct.unpack("<BBBBI", b)
    when = datetime.fromtimestamp(ts).strftime("%Y-%m-%d %H:%M") if ts else "never"
    if status == 0:
        return f"{bpm} bpm (confidence {conf} %, {when})"
    return f"{HR_STATUS.get(status, status)} ({when})"


async def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=["sync", "hr", "ppg", "find", "set", "ota", "forget"])
    ap.add_argument("csv", nargs="?", help="output file for `ppg`")
    ap.add_argument("--24h", dest="h24", type=int)
    ap.add_argument("--chime", type=int)
    ap.add_argument("--beep", type=int)
    ap.add_argument("--contrast", type=int)
    ap.add_argument("--light", choices=COLORS)
    ap.add_argument("--led-ma", type=int)
    a = ap.parse_args()

    dev = await find_watch()
    async with BleakClient(dev) as c:
        print(f"connected to {dev.name} ({dev.address})")
        if a.cmd == "sync":
            await c.write_gatt_char(TIME, struct.pack("<Ih", int(time.time()), tz_minutes()), response=True)
            steps = struct.unpack("<I", await c.read_gatt_char(STEPS))[0]
            hist = struct.unpack("<7I", await c.read_gatt_char(HIST))
            batt = (await c.read_gatt_char(BATTERY))[0]
            print(f"clock set. steps today {steps}, last 7 days {list(hist)} (yesterday first)")
            print(f"last heart rate: {fmt_hr(await c.read_gatt_char(HR))}. battery {batt} %")

        elif a.cmd in ("hr", "ppg"):
            done = asyncio.Event()
            out = open(a.csv or "ppg.csv", "w") if a.cmd == "ppg" else None
            if out:
                out.write("index,green\n")

                def on_ppg(_, b):
                    seq = struct.unpack_from("<H", b)[0]
                    for i in range((len(b) - 2) // 3):
                        v = b[2 + 3 * i] | b[3 + 3 * i] << 8 | b[4 + 3 * i] << 16
                        out.write(f"{seq + i},{v}\n")
                await c.start_notify(PPG, on_ppg)
                await c.write_gatt_char(CTRL, bytes([0x04, 1]), response=True)

            def on_hr(_, b):
                print("result:", fmt_hr(b))
                done.set()
            await c.start_notify(HR, on_hr)
            await c.write_gatt_char(CTRL, bytes([0x01]), response=True)
            print("measuring: keep still for about 12 s...")
            await asyncio.wait_for(done.wait(), 30)
            if out:
                await c.write_gatt_char(CTRL, bytes([0x04, 0]), response=True)
                out.close()
                print(f"raw samples written to {out.name} (25 sps, green LED)")

        elif a.cmd == "find":
            await c.write_gatt_char(CTRL, bytes([0x02]), response=True)

        elif a.cmd == "set":
            flags, contrast, light, led = await c.read_gatt_char(SETTINGS)
            for bit, v in ((1, a.h24), (2, a.chime), (4, a.beep)):
                if v is not None:
                    flags = (flags | bit) if v else (flags & ~bit)
            contrast = contrast if a.contrast is None else a.contrast
            light = light if a.light is None else COLORS[a.light]
            led = led if a.led_ma is None else a.led_ma
            await c.write_gatt_char(SETTINGS, bytes([flags, contrast, light, led]), response=True)
            print(f"24h={flags & 1} chime={flags >> 1 & 1} beep={flags >> 2 & 1} contrast={contrast} light={light} led={led} mA")

        elif a.cmd == "ota":
            try:
                await c.write_gatt_char(CTRL, bytes([0x03]), response=True)
            except Exception:
                pass  # the watch may reset before the write response arrives
            print("watch is rebooting into the BLE_Ota loader")

        elif a.cmd == "forget":
            await c.write_gatt_char(CTRL, bytes([0x06]), response=True)
            print("bonds cleared; also remove the watch from this computer's Bluetooth devices")


if __name__ == "__main__":
    asyncio.run(main())
