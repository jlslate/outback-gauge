"""Two loose parts for working out how to mount the gauge in the console.

  sled.3mf           frame that drops into the rubber tray's well
  case_back_slim.3mf plain slim back cover for the case (no battery, no magnets)

The tray stays in the slot and the sled sits in its well, so nothing is stuck
to the car and the tray still lifts out. The sled is a frame, not a solid
sheet: it rests on the flat rim around the moulded phone and Qi symbols and
bridges over them.

Tray well, measured: 147 mm long, 113 mm wide at the mouth tapering to 80 mm
at the back, 13 mm deep, no lip at the front.

  tools/case/.venv/bin/python tools/case/sled.py
"""

import pathlib

import case
from case import box, print_pose, write_3mf, write_stl
from manifold3d import CrossSection

# ---- the tray well -----------------------------------------------------------
WELL_W_FRONT = 113.0
WELL_W_REAR = 80.0
WELL_LEN = 147.0
CLEAR = 1.5           # gap to the well walls, so it drops in without forcing

# ---- the sled ----------------------------------------------------------------
SLED_LEN = 144.0      # full depth of the well, less the clearance
SLED_T = 3.0
SLED_RAIL = 11.0      # width of the frame rails


def _trapezoid(half_front, half_rear, length):
    return CrossSection([[(-half_front, 0), (-half_rear, -length), (half_rear, -length), (half_front, 0)]])


def sled():
    """Frame that sits on the flat rim of the tray well."""
    taper = (WELL_W_FRONT - WELL_W_REAR) / 2 / WELL_LEN
    half_f = (WELL_W_FRONT - 2 * CLEAR) / 2
    half_r = half_f - taper * SLED_LEN
    frame = _trapezoid(half_f, half_r, SLED_LEN).extrude(SLED_T)

    rail, rib = SLED_RAIL, 12.0
    inner = _trapezoid(half_f - rail, half_r - rail, SLED_LEN - 2 * rail).translate([0, -rail])
    windows = inner.extrude(SLED_T + 2).translate([0, 0, -1])
    windows -= box(-rib / 2, rib / 2, -SLED_LEN, 0, -1, SLED_T + 1)      # center rib
    windows -= box(-half_f, half_f, -SLED_LEN / 2 - rib / 2, -SLED_LEN / 2 + rib / 2, -1, SLED_T + 1)
    return frame - windows


def main():
    out = pathlib.Path(__file__).parent / "stl"
    out.mkdir(exist_ok=True)
    parts = {
        "sled": sled(),
        "case_back_slim": case.back_cover(bay=0.0, magnets=False),
    }
    for name, solid in parts.items():
        posed = print_pose(solid, flip=name.startswith("case_back"))
        write_3mf(posed, out / f"{name}.3mf", name)
        write_stl(posed, out / f"{name}.stl")
        bb = solid.bounding_box()
        print(f"wrote {name}  {bb[3]-bb[0]:.0f} x {bb[4]-bb[1]:.0f} x {bb[5]-bb[2]:.1f} mm, "
              f"{solid.volume()/1000:.0f} cm3")


if __name__ == "__main__":
    main()
