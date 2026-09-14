# Case

A two-piece round case for the ESP32-S3-Touch-LCD-1.46B **with widened
protective cover glass** (49 mm glass). The other glass versions need
different numbers at the top of `case.py`.

| File | What it is |
|---|---|
| `stl/case_front.stl` | Shell with a front lip that holds the glass |
| `stl/case_back.stl` | Slim back cover for USB power. Case is 18.2 mm deep. |
| `stl/case_back_battery.stl` | Back cover with a bay for an 802525 (400 mAh, 8×25×25 mm) LiPo. Case is 27.2 mm deep. |
| `stl/case_back_magnet.stl` | Slim back cover with pockets for two 12×2 mm magnets; snaps onto a matching pair stuck to the car. Case is 18.4 mm deep and sits 2.4 mm off the surface. |
| `stl/gopro_mount.stl` | Two-finger GoPro-style mount; fits any GoPro vent, dash or suction mount |

Outside diameter is 54.0 mm. The glass sits behind a 1.3 mm lip that covers
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
- GoPro mount only: 2 × M3×6 button-head screws and 2 × M3 nuts
- Magnet back only: 4 × N52 12×2 mm disc magnets with 3M VHB adhesive, and a drop of superglue
- Soft 1/16" (1.6 mm) closed-cell foam weatherstrip, a few cm. It needs to be thicker than the 1 mm gap so it squashes and takes up tolerance; dense 1 mm mounting tape is too firm.
- GoPro mount only: a GoPro thumbscrew and car mount (usually sold together)

## Assembly

1. Bolt the mount to the outside of the back cover: screws go in from the
   inside (heads sit in the recesses) into the nuts trapped in the mount.
2. Cut three narrow strips of foam (about 3 mm wide) and stick one on the end of each arc.
3. Drop the board into the front shell from behind, glass first, turning it
   so the USB-C port lines up with the notch at the bottom. PWR and BOOT then
   line up with the two small holes on the right side.
4. With the battery cover, plug in the battery and lay it in the bay.
5. Press the back cover in. It only lines up one way (there's no screw at the
   bottom). Drive the three M2 screws through the side holes into the arcs.

## Magnet mounting

Adhesive magnets all face the same way out of the pack, so two of them stuck
back to back would repel. The gauge-side pair gets flipped and glued instead,
and the car-side pair is placed by letting the gauge carry it into position:

1. Snap two magnets together in pairs, liners still on, so each pair attracts.
2. Peel the adhesive off the magnet that will go in the gauge. Put a small
   drop of superglue in each pocket and press that magnet in, with its
   partner still stuck to it. Let the glue set.
3. Clean the spot in the car with rubbing alcohol. Peel the liners off the
   two car-side magnets (still riding on the gauge) and press the gauge onto
   the spot firmly for 30 seconds.
4. Leave it for 24 hours so the VHB can cure before taking the gauge off.
   Slide or tilt it off rather than pulling straight out.

Two magnets side by side keep the gauge from turning. It can go on upside
down if you rotate it 180°, so check the picture.

## Adjusting the fit

Everything is set by the constants at the top of `case.py`:

- Glass loose or tight in the shell: `FIT` (radial clearance, default 0.3 mm, sized for MJF nylon; 0.2 suits a well-tuned FDM printer)
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
