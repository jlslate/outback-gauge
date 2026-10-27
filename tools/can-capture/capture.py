"""Listen-only CAN capture through an OBDLink CX over Bluetooth LE.

Puts the adapter in monitor mode (STMA, or ATMA on other ELM327s), which only
receives: it sends nothing to the car. Every frame is logged with a timestamp,
and pressing Return marks the moment, so you can press the car's start-stop
button, hit Return, and later see which frames changed right then.

  tools/can-capture/.venv/bin/python tools/can-capture/capture.py
  tools/can-capture/.venv/bin/python tools/can-capture/capture.py --seconds 180

While it runs, type a word and press Return to mark a moment with that label
(for example "off" when you pressed the button to turn start-stop off, "on"
when you turned it back on). A bare Return is just "mark". Ctrl-C stops.

The log goes to tools/can-capture/logs/. Run analyze.py on it afterwards.

macOS asks to pair the first time, because the CX will not talk over an
unencrypted link. The passkey, if it asks, is 000000.
"""

import argparse
import asyncio
import pathlib
import sys
import time

from bleak import BleakClient, BleakScanner

NAME_HINTS = ("obdlink", "obd", "vlink", "vgate", "icar")      # same as the gauge firmware
STANDARD = {0x1800, 0x1801, 0x180A, 0x180F, 0xFEF5}            # not the serial service


def short(uuid):
    """16-bit form of a Bluetooth base-UUID, or None for a vendor UUID."""
    u = str(uuid).lower()
    return int(u[4:8], 16) if u.startswith("0000") and u.endswith("-0000-1000-8000-00805f9b34fb") else None


async def find_device(address, seconds=8):
    if address:
        return await BleakScanner.find_device_by_address(address, timeout=seconds)
    found = await BleakScanner.discover(timeout=seconds, return_adv=True)
    best = None
    for dev, adv in found.values():
        name = (dev.name or adv.local_name or "").lower()
        if any(h in name for h in NAME_HINTS) and (best is None or adv.rssi > best[1]):
            best = (dev, adv.rssi)
    return best[0] if best else None


def pick_serial(client):
    """First non-standard service with a characteristic to write and one that
    notifies (often the same one), as the gauge does."""
    for svc in client.services:
        if short(svc.uuid) in STANDARD:
            continue
        notify = next((c for c in svc.characteristics if "notify" in c.properties or "indicate" in c.properties), None)
        write = next((c for c in svc.characteristics if "write" in c.properties or "write-without-response" in c.properties), None)
        if notify and write:
            return notify, write
    return None, None


class Capture:
    def __init__(self, log):
        self.log, self.buf, self.t0 = log, "", time.monotonic()
        self.frames = 0
        self.replies = asyncio.Queue()

    def stamp(self):
        return time.monotonic() - self.t0

    def on_data(self, _, data):
        self.buf += data.decode("ascii", "ignore")
        while "\r" in self.buf:
            line, self.buf = self.buf.split("\r", 1)
            line = line.strip()
            if not line:
                continue
            self.replies.put_nowait(line)

    def write_line(self, text):
        self.log.write(f"{self.stamp():9.3f} {text}\n")


async def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--address", help="adapter address, if more than one is around")
    ap.add_argument("--seconds", type=float, default=0, help="stop after this long (default: until Ctrl-C)")
    args = ap.parse_args()

    print("Scanning for the adapter...")
    dev = await find_device(args.address)
    if not dev:
        sys.exit("No OBD adapter found. Plug it in, wake the car, and try again (--address if it has an odd name).")
    print(f"Found {dev.name} ({dev.address})")

    logdir = pathlib.Path(__file__).parent / "logs"
    logdir.mkdir(exist_ok=True)
    path = logdir / time.strftime("can-%Y%m%d-%H%M%S.log")

    async with BleakClient(dev) as client:
        notify, write = pick_serial(client)
        if not notify:
            sys.exit("No serial-style service on that device.")
        print(f"Serial service found (notify {notify.uuid}, write {write.uuid})")

        with open(path, "w") as log:
            cap = Capture(log)
            log.write(f"# can-capture {time.strftime('%Y-%m-%d %H:%M:%S')} adapter {dev.name}\n")
            await client.start_notify(notify, cap.on_data)

            async def send(cmd, wait=1.5):
                await client.write_gatt_char(write, (cmd + "\r").encode(), response="write" in write.properties)
                lines = []
                try:
                    while True:
                        lines.append(await asyncio.wait_for(cap.replies.get(), wait))
                        if lines[-1].endswith(">"):
                            break
                except asyncio.TimeoutError:
                    pass
                text = " ".join(lines).replace(">", "").strip()
                cap.write_line(f"# {cmd} -> {text}")
                return text

            await send("ATZ", 3)
            for cmd in ("ATE0", "ATL0", "ATS1", "ATH1", "ATCAF0", "ATSP6"):   # raw frames, with ids, spaced
                print(f"  {cmd}: {await send(cmd)}")
            print(f"  adapter: {await send('STI')}")
            reply = await send("STFAC")                       # clear any filters (STN only)
            monitor = "STMA" if "?" not in reply else "ATMA"
            print(f"Monitoring with {monitor}. This only listens.")
            print("Press Return to mark a moment (type a word first to label it). Ctrl-C to stop.\n")

            loop = asyncio.get_running_loop()
            marks = 0

            def on_stdin():
                nonlocal marks
                label = sys.stdin.readline().strip()
                marks += 1
                cap.write_line(f"#MARK {marks} {label}".rstrip())
                print(f"  mark {marks} at {cap.stamp():.1f} s {label}")

            loop.add_reader(sys.stdin.fileno(), on_stdin)
            await client.write_gatt_char(write, (monitor + "\r").encode(), response="write" in write.properties)
            end = cap.stamp() + args.seconds if args.seconds else None
            last_report = 0
            try:
                while end is None or cap.stamp() < end:
                    try:
                        line = await asyncio.wait_for(cap.replies.get(), 0.5)
                    except asyncio.TimeoutError:
                        line = None
                    if line:
                        cap.write_line(line)
                        cap.frames += 1
                    if cap.stamp() - last_report >= 10:
                        last_report = cap.stamp()
                        print(f"  {cap.stamp():5.0f} s, {cap.frames} lines so far")
                        log.flush()
            except (KeyboardInterrupt, asyncio.CancelledError):
                pass
            finally:
                loop.remove_reader(sys.stdin.fileno())
                try:
                    await client.write_gatt_char(write, b"\r", response="write" in write.properties)   # any byte stops monitoring
                except Exception:
                    pass
                log.flush()
    print(f"\n{cap.frames} lines, {marks} marks. Saved {path}")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
