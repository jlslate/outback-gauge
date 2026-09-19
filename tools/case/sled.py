"""Console mount for the gauge in a 2025 Outback.

  sled.3mf                  frame that drops into the rubber tray's well,
                            with two curved arms that hold the gauge upright
  case_back_slim.3mf        slim back cover, plain: the mount's magnets live in
                            the front shell's side wall, not back here

The tray stays in the slot and the sled sits in its well, so nothing is stuck
to the car and the tray still lifts out. The sled is a frame, not a solid
sheet: it rests on the flat rim around the moulded phone and Qi symbols and
bridges over them.

The gauge stands perpendicular to the sled with its bottom edge 8 mm clear,
cradled by two arms at the front. The arms hold it at 240 and 300 degrees,
where the shell carries magnets in its side wall, and the gap between them
leaves the USB-C notch at the bottom of the case clear for the cable.

Tray well, measured: 147 mm long, 113 mm wide at the mouth tapering to 80 mm
at the back, 13 mm deep, no lip at the front.

  tools/case/.venv/bin/python tools/case/sled.py
"""

import math
import pathlib

import case
from case import box, cyl, print_pose, write_3mf, write_stl
from manifold3d import CrossSection, Manifold

# ---- the tray well -----------------------------------------------------------
WELL_W_FRONT = 113.0
WELL_W_REAR = 80.0
WELL_LEN = 147.0
CLEAR = 1.5           # gap to the well walls, so it drops in without forcing

# ---- the sled ----------------------------------------------------------------
SLED_LEN = 149.0      # runs the depth of the well and a little past the mouth
SLED_T = 3.0
SLED_RAIL = 11.0      # width of the frame rails

# ---- the cradle --------------------------------------------------------------
GAUGE_GAP = 8.0       # bottom of the case above the sled's top face
# Case mid-depth, back from the sled's front edge: half the case's depth, so
# the glass ends up flush with the front of the sled.
GAUGE_Y = -(case.CUP_LEN + case.MAGNET_FLOOR) / 2
ARM_FIT = 0.3         # gap between the arm's face and the case
ARM_DEPTH = 13.0      # how much of the case's 18.4 mm depth the arms hold
# Each arm is a flat pad, tangent to the case where its magnet sits, carried on
# a wedge with a vertical outer wall down to the sled. The top is ARM_T_TOP
# wide, so it finishes blunt rather than in a sharp tip.
ARM_T_TOP = 6.0
ARM_UP = 10.0         # how far the pad runs up the case from the magnet
ARM_DOWN = 7.5        # and down toward the gap at the bottom; keeps the 12.3 mm
                      # magnet inside the pad, which is centered on the tangent
MAGNET_POCKET = case.MAGNET_T + case.MAGNET_ADHESIVE + 0.1

CENTER_Z = SLED_T + GAUGE_GAP + case.R_OUT     # case center above the sled's underside


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


def _upright(solid):
    """Something built around the case's axis, stood up at the cradle."""
    return solid.rotate([90, 0, 0]).translate([0, GAUGE_Y, CENTER_Z])


def _bore(radius):
    """The space the case occupies, as a cylinder lying along y."""
    return _upright(cyl(200, radius, -100))


def _ccw(points):
    area = sum(x0 * y1 - x1 * y0 for (x0, y0), (x1, y1) in zip(points, points[1:] + points[:1]))
    return points if area > 0 else points[::-1]


def _arm_profile(deg):
    """Outline of one arm in the plane of the screen, as (x, height) pairs
    measured from the case's center."""
    a = math.radians(deg)
    face = case.R_OUT + ARM_FIT                       # the flat pad, tangent to the case
    n = (math.cos(a), math.sin(a))                    # outward, toward the pad
    t = (-math.sin(a), math.cos(a))                   # along the pad
    if t[1] < 0:                                      # point it up the case on both sides
        t = (-t[0], -t[1])
    at = lambda depth, along: (depth * n[0] + along * t[0], depth * n[1] + along * t[1])

    inner_top = at(face, ARM_UP)
    inner_bot = at(face, -ARM_DOWN)
    outer_x = at(face + ARM_T_TOP, ARM_UP)[0]         # the vertical outer wall
    sled = SLED_T - CENTER_Z                          # the sled's top face
    # Level across the top, so the arm finishes flat instead of in a peak.
    return _ccw([inner_top, (outer_x, inner_top[1]), (outer_x, sled), (inner_bot[0], sled), inner_bot])


def cradle():
    y0, y1 = -ARM_DEPTH / 2, ARM_DEPTH / 2
    part = None
    for deg in case.SIDE_MAGNET_DEG:
        arm = _upright(CrossSection([_arm_profile(deg)]).extrude(y1 - y0).translate([0, 0, y0]))
        part = arm if part is None else part + arm
    part -= _bore(case.R_OUT + ARM_FIT)               # keep the pads off the case

    for deg in case.SIDE_MAGNET_DEG:                  # pockets facing the shell's magnets
        pocket = Manifold.cylinder(MAGNET_POCKET + 1, case.MAGNET_D / 2 + 0.15,
                                   case.MAGNET_D / 2 + 0.15, 64)
        pocket = pocket.rotate([0, 90, 0]).translate([case.R_OUT + ARM_FIT - 1, 0, 0])
        part -= _upright(pocket.rotate([0, 0, deg]))
    return part


def check(part):
    """The case has to drop in, and the plug has to clear the gap between arms."""
    ok = True
    v = (part ^ _bore(case.R_OUT)).volume()
    print(f"  case in the cradle: overlap {v:6.3f} mm^3")
    ok &= v < 0.01
    # The plug drops out of the notch into the gap under the case, then the
    # cable runs back to the port; only that gap has to stay clear.
    plug = box(-6.4, 6.4, GAUGE_Y - 6, GAUGE_Y + 6, SLED_T, CENTER_Z - case.R_OUT + 1)
    v = (part ^ plug).volume()
    print(f"  plug in the gap:    overlap {v:6.3f} mm^3")
    return ok and v < 0.01


def main():
    out = pathlib.Path(__file__).parent / "stl"
    out.mkdir(exist_ok=True)
    whole = sled() + cradle()
    print("Clearance check:")
    fits = check(whole)

    for name, solid in {"sled": whole,
                        "case_back_slim": case.back_cover()}.items():
        posed = print_pose(solid, flip=name.startswith("case_back"))
        write_3mf(posed, out / f"{name}.3mf", name)
        write_stl(posed, out / f"{name}.stl")
        bb = solid.bounding_box()
        print(f"wrote {name}  {bb[3]-bb[0]:.0f} x {bb[4]-bb[1]:.0f} x {bb[5]-bb[2]:.1f} mm, "
              f"{solid.volume()/1000:.0f} cm3, {len(solid.decompose())} piece(s)")
    print(f"gauge center {CENTER_Z:.0f} mm above the sled's underside, "
          f"bottom edge {SLED_T + GAUGE_GAP:.0f} mm up; flat pads, {ARM_T_TOP:.0f} mm across the top")
    if not fits:
        raise SystemExit("clearance check failed")


if __name__ == "__main__":
    main()
