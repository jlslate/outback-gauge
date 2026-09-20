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
| `stl/magnet_test` | Four test pockets for dialling in the magnet press fit | `magnet_test.py` |

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
- 4 × N52 12×2 mm disc magnets. **No glue** — each pocket has a collar the
  disc presses past and then cannot back out of. Peel any adhesive off the
  backs; it only gets in the way. The pockets are 2.5 mm deep for a 2.0 mm
  disc on purpose — see `MAGNET_STANDOFF`.
- Soft 1/16" (1.6 mm) closed-cell foam weatherstrip, a few cm. It needs to be
  thicker than the 1 mm gap so it squashes and takes up tolerance; dense 1 mm
  mounting tape is too firm.

## Assembly

1. Set the four magnets into the shell and the arms (see [Magnet mounting](#magnet-mounting)).
2. Cut three narrow strips of foam (about 3 mm wide) and stick one on the end of each arc.
3. Drop the board into the front shell from behind, glass first, turning it
   so the USB-C port lines up with the notch at the bottom. PWR and BOOT then
   line up with the two small holes on the right side.
4. Line the three lugs up with the three channels in the shell's back face —
   only one rotation fits — press the cover in against the foam, and twist it
   clockwise (seen from the back) about 18° until it clicks past the detents
   and stops.

## Magnet mounting

Four magnets in four pockets: two in the shell's side wall at 240° and 300°,
two facing them in the sled's arms. Nothing is stuck to the car, and nothing
is glued.

Each pocket is a loose bore that necks down to an interference seat, with a
cone between them. The disc goes in easily for the first millimetre and is
then pressed the last 2 mm into the seat, where the plastic grips it.
Superglue does not hold these — cyanoacrylate gets no grip on the nickel
plating — so the plastic does the work instead.

The interference is in the bore diameter, not in a lip or a collar. A ledge
inside a bore that is thinner than one extrusion width does not get printed
at all, which is a good way to produce a pocket that looks right on screen
and holds nothing.

**Print `magnet_test` first.** How a bore comes out varies by printer by more
than the fit tolerance, so a guess costs an hour of shell. The coupon has
four pockets bored sideways, the way the real ones print, at 12.10, 12.00,
11.90 and 11.80 mm, with 1 to 4 ticks above them. Press a magnet into each,
keep the tightest one that goes in without a fight, and put its number into
`MAGNET_PRESS`.

1. Snap the magnets together in two pairs so each pair attracts, and mark the
   outward face of each with a pen. Get a pair backwards and the gauge pushes
   itself off the cradle.
2. Peel any adhesive off the backs.
3. Start each disc square in its pocket, marked face out, and press it in
   with something flat — a coin, a socket, the back of a screwdriver. It
   takes a firm push and then goes. Don't hammer it: N52 is brittle and
   chips.
4. If one won't start, a few turns of sandpaper wrapped round a pen opens the
   seat. If one drops straight in, lower `MAGNET_PRESS` and reprint that
   part.

The gauge then drops into the cradle and the two magnets pull it against the
pads. The arms carry its weight in the V between them; the magnets only stop
it lifting out and rattling.

## Adjusting the fit

Everything is set by the constants at the top of `case.py`:

- Glass loose or tight in the shell: `FIT` (radial clearance, default 0.3 mm, sized for MJF nylon; 0.2 suits a well-tuned FDM printer)
- Board rattles even with foam, or the cover won't close: `FOAM` (default 1.0 mm)
- USB-C plug won't reach: `USB_OPENING`
- Button holes don't line up: `PWR_DEG`, `BOOT_DEG`, `BUTTON_Z`
- Twist joint too tight, too loose, or the detent too stiff: `BAY_FIT`,
  `BAY_PRELOAD`, `DETENT`; back to screws with `BAYONET = False`
- Different magnets: `MAGNET_D`, `MAGNET_T`, `MAGNET_X`
- Gauge too hard or too easy to lift off the cradle: `MAGNET_STANDOFF` (bigger
  is weaker). Two N52 12×2 pairs in contact would be several kilos, enough to
  pull an arm off the sled; the ~2 mm gap is deliberate
- Magnets drop into their pockets, or won't press in: `MAGNET_PRESS`, the
  radius of the seat relative to the disc. Print `magnet_test` to find it
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
