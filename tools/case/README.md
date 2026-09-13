# Case

A two-piece round case for the ESP32-S3-Touch-LCD-1.46B **with widened
protective cover glass** (49 mm glass). The other glass versions need
different numbers at the top of `case.py`.

| File | What it is |
|---|---|
| `stl/case_front.stl` | Shell with a front lip that holds the glass |
| `stl/case_back.stl` | Slim back cover for USB power. Case is 18.2 mm deep. |
| `stl/case_back_battery.stl` | Back cover with a bay for an 802525 (400 mAh, 8×25×25 mm) LiPo. Case is 27.2 mm deep. |
| `stl/gopro_mount.stl` | Two-finger GoPro-style mount; fits any GoPro vent, dash or suction mount |

Outside diameter is 53.8 mm. The glass sits behind a 1.3 mm lip that covers
only the black border.

## Printing

- **PETG or ASA, not PLA.** PLA softens around 60 °C and a parked car gets hotter.
  At a print service, MJF nylon (PA12) is also a good choice.
- 0.2 mm layers, 3+ walls, 20–30% infill, **no supports**. Print the front
  shell lip-down, the back covers outside-face-down (arcs pointing up), and
  the mount base-down. The STLs are already in these orientations.
- Print the front shell alone first and check the glass fit before printing the rest.

## Hardware

- 3 × M2×4 screws (self-tapping or machine) to lock the back cover
- 2 × M3×6 button-head screws and 2 × M3 nuts for the mount
- 1 mm foam tape (weatherstrip or double-sided foam), a few cm
- A GoPro thumbscrew and car mount (usually sold together)

## Assembly

1. Bolt the mount to the outside of the back cover: screws go in from the
   inside (heads sit in the recesses) into the nuts trapped in the mount.
2. Stick a piece of foam tape on the end of each of the three arcs.
3. Drop the board into the front shell from behind, glass first, turning it
   so the USB-C port lines up with the notch at the bottom. PWR and BOOT then
   line up with the two small holes on the right side.
4. With the battery cover, plug in the battery and lay it in the bay.
5. Press the back cover in. It only lines up one way (there's no screw at the
   bottom). Drive the three M2 screws through the side holes into the arcs.

## Adjusting the fit

Everything is set by the constants at the top of `case.py`:

- Glass loose or tight in the shell: `FIT` (radial clearance, default 0.2 mm)
- Board rattles even with foam, or the cover won't close: `FOAM` (default 1.0 mm)
- USB-C plug won't reach: `USB_OPENING`
- Button holes don't line up: `PWR_DEG`, `BOOT_DEG`, `BUTTON_Z`

Regenerate after a change. The script checks the case against an outline of
the board and fails if anything collides.

```bash
python3 -m venv tools/case/.venv
tools/case/.venv/bin/pip install -r tools/case/requirements.txt
tools/case/.venv/bin/python tools/case/case.py
```

Board positions were measured from Waveshare's dimension drawing for this
version, so treat the first print as a test fit.
