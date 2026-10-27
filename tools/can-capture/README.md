# CAN capture

Listen-only recording of what the car's network does when you press a button,
through the OBDLink CX. It sends nothing to the car.

The aim is to see whether the start-stop button leaves any trace at the OBD
port. If it does, the log shows which message it is. If it doesn't, that is a
useful answer too: the button's traffic is on a part of the car's network the
port can't see.

## Set up (once)

```bash
python3 -m venv tools/can-capture/.venv
tools/can-capture/.venv/bin/python -m pip install --upgrade pip
tools/can-capture/.venv/bin/pip install -r tools/can-capture/requirements.txt
```

macOS will ask to let the terminal use Bluetooth the first time, and may ask
to pair with the adapter (passkey 000000 if it wants one).

## Record

1. Park, apply the brake or parking brake, engine running, in Park.
2. Plug the adapter in. Quit anything else using it (the gauge will grab it
   too, so unplug or power the gauge down first).
3. Start the recorder:

   ```bash
   tools/can-capture/.venv/bin/python tools/can-capture/capture.py
   ```

4. Sit still for 30 seconds. That is the baseline.
5. Press the start-stop **off** button, then press Return in the terminal. Wait
   a few seconds. Optionally type `off` before Return to label it.
6. Press the button again to turn start-stop back **on**, then Return (or type
   `on` and Return). Wait a few seconds.
7. Repeat steps 5 and 6 four or five times, leaving about 10 seconds between
   marks. Stop with Ctrl-C.

Do not touch the throttle, pedals, windows, lights or other buttons during the
run; every one of those is traffic on the bus that you'd otherwise have to
tell apart from the button.

## Analyze

```bash
tools/can-capture/.venv/bin/python tools/can-capture/analyze.py tools/can-capture/logs/can-<date>.log
```

It lists the bytes that held one value and then another around the marks, and
the message ids that fire right after marks more than they normally do.
`selftest.py` runs the analyzer against a made-up log with a planted signal.

Logs go to `tools/can-capture/logs/`, which is not committed.
