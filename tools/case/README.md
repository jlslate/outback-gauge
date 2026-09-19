# Case

A two-piece round case for the ESP32-S3-Touch-LCD-1.46B **with widened
protective cover glass** (49 mm glass). The other glass versions need
different numbers at the top of `case.py`.

The gauge lives in the console tray under the screen: a printed sled drops
into the tray's well and two arms hold the case upright on magnets, so
nothing is stuck to the car and the tray still lifts out.

| File | What it is | From |
|---|---|---|
| `stl/case_front` | Shell that holds the glass, with magnet pockets in its side wall at 240° and 300° and the bayonet grooves for the back | `case.py` |
| `stl/case_back_slim` | Back cover: three bayonet lugs, no magnet pockets | `sled.py` |
| `stl/sled` | Tray sled with the two cradle arms | `sled.py` |

Each part is written as both `.3mf` and `.stl`. Load the **3MF** if your
slicer offers the choice: it states millimeters, so nothing can import at the
wrong scale.

The case is 54.0 mm across and 18.4 mm deep with the slim back. The glass
sits behind a 1.3 mm lip that covers only the black border.

## The twist joint

The back cover has no screws. Three lugs on its pusher arcs drop into
channels cut through the shell's back face, and an 18° twist takes them into
grooves whose roofs ramp down 0.7 mm, pulling the cover forward onto the
foam. A ramped bump near the end of each groove is the detent: it takes a
push to turn past and then holds against the car's vibration, and the end of
the groove is the stop. The lugs are at 0°, 90° and 180°, so the cover only
lines up one way round. Scallops around the cover's rim are there to grip.

Being a first cut, the fit is the part most likely to need a tweak. If it
won't turn, raise `BAY_FIT`; if it turns but rocks, raise `BAY_PRELOAD`; if
it takes two hands to get past the detent, lower `DETENT`. Each is one
number at the top of `case.py` and a reprint of the one part.

`BAYONET = False` puts the old joint back: three M2×4 screws through the
wall into the arcs. Both parts have to be regenerated together — a bayonet
shell will not take a screw cover.

In the sled, the gauge stands square with its bottom edge 8 mm clear, its
face flush with the sled's front, and its screen center 38 mm above the tray
floor. The 14 mm gap between the arms leaves the USB-C notch clear for the
cable, which runs back to the dash port.

## Printing

- **PETG or ASA, not PLA.** PLA softens around 60 °C and a parked car gets hotter.
  At a print service, MJF nylon (PA12) is also a good choice.
- 0.2 mm layers, 3+ walls, 20–30% infill, **no supports**. Print the front
  shell lip-down and the back cover outside-face-down (arcs pointing up).
  The files are already in these orientations.
- The groove roofs and the undersides of the lugs are ~1 mm unsupported
  ledges. They print, but expect a little droop; if the twist is stiff at
  first, run a knife around them once.
- Print the front shell alone first and check the glass fit before printing the back.

## Hardware

- No screws: the back cover twists on. (With `BAYONET = False`, 3 × M2×4
  self-tapping pan-head screws instead.)
- 4 × N52 12×2 mm disc magnets and a drop of superglue: two in the shell's
  side wall, two facing them in the sled's arms. Peel the VHB off and glue
  them in; the pockets, not the adhesive, hold them
- Soft 1/16" (1.6 mm) closed-cell foam weatherstrip, a few cm. It needs to be
  thicker than the 1 mm gap so it squashes and takes up tolerance; dense 1 mm
  mounting tape is too firm.

## Assembly

1. Glue the gauge-side magnets into the back cover (see [Magnet mounting](#magnet-mounting), steps 1–2).
2. Cut three narrow strips of foam (about 3 mm wide) and stick one on the end of each arc.
3. Drop the board into the front shell from behind, glass first, turning it
   so the USB-C port lines up with the notch at the bottom. PWR and BOOT then
   line up with the two small holes on the right side.
4. Line the three lugs up with the three channels in the shell's back face —
   only one rotation fits — press the cover in against the foam, and twist it
   clockwise (seen from the back) about 18° until it clicks past the detents
   and stops.

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
- Twist joint too tight, too loose, or the detent too stiff: `BAY_FIT`,
  `BAY_PRELOAD`, `DETENT`; back to screws with `BAYONET = False`
- Different magnets: `MAGNET_D`, `MAGNET_T`, `MAGNET_X`
- Magnets standing proud of the shell: `SIDE_POCKET_SINK` (a flat disc in a
  round pocket sits above a curved surface unless the pocket is sunk)
- Gauge height, arm size and the tray well: the constants at the top of `sled.py`

Regenerate after a change. The script checks the case against an outline of
the board and the USB-C plug path, and fails if anything collides.

```bash
python3 -m venv tools/case/.venv
tools/case/.venv/bin/pip install -r tools/case/requirements.txt
tools/case/.venv/bin/python tools/case/case.py
```

Board positions were measured from Waveshare's dimension drawing for this
version, so treat the first print as a test fit.
