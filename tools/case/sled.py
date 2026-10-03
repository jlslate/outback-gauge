"""Console mount for the gauge in a 2025 Outback.

  sled.3mf                  frame that drops into the rubber tray's well,
                            with two curved arms that hold the gauge upright
  case_back_slim.3mf        slim back cover, plain: the mount's magnets live in
                            the front shell's side wall, not back here

The tray stays in the slot and the sled sits in its well, so nothing is stuck
to the car and the tray still lifts out. The sled is a frame, not a solid
sheet: it rests on the flat rim around the moulded phone and Qi symbols and
bridges over them.

The gauge is cradled by two arms at the front. The arms hold it at 225 and
315 degrees, where the shell carries magnets in its side wall, and the gap
between them is well below the USB-C port, which points sideways out of the
case (case.USB_DEG; it prints on the viewer's left).

The gauge is turned toward the driver, who sits on the viewer's left, and
tipped up toward their eyes: YAW degrees about the vertical, then PITCH
degrees of tilt. The left arm's magnet stays where it was (it can go no
further back), so the right arm comes forward and the sled's front edge is no
longer parallel to its back. Every arm stands on a column straight down to the
sled, so nothing overhangs, and the front edge is cut to clear the arms: flat
across the left, slanting forward along the yaw to the right arm, then flat
again to the right side.

Tray well, measured: 147 mm long, 113 mm wide at the mouth tapering to 80 mm
at the back, 13 mm deep, no lip at the front.

  tools/case/.venv/bin/python tools/case/sled.py
"""

import math
import pathlib

import case
from case import box, cyl, floating, print_pose, stack, write_3mf, write_stl
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
FRONT_MARGIN = 2.0    # solid front rail, this far past the back of the arms

# ---- the cradle --------------------------------------------------------------
GAUGE_GAP = 2.0       # bottom of the case above the sled's top face; it was 6
                      # while the USB-C plug dropped out of a notch down there
GAUGE_X = 19.0        # sideways offset from the sled centerline (+x is the viewer's left)
YAW = 30.0            # degrees the face is turned toward the driver, about the vertical
PITCH = 20.0          # degrees the face is then tipped up
# Case mid-depth, back from the sled's front edge: half the case's depth, so
# the glass ends up flush with the front of the sled.
GAUGE_Y = -(case.CUP_LEN + case.MAGNET_FLOOR) / 2
ARM_FIT = 0.3         # gap between the arm's face and the case
ARM_DEPTH = 13.0      # how much of the case's 18.4 mm depth the arms hold
# Each arm is a flat pad, meeting the shell's own flat magnet pad, carried on
# a wedge with a vertical outer wall down to the sled. The top is ARM_T_TOP
# wide, so it finishes blunt rather than in a sharp tip.
ARM_T_TOP = 6.0
# The arms are as short as the magnet allows: their top is the highest point
# of the whole sled, and at 45 deg every millimetre here is 0.7 mm of height
# and 0.7 mm of width. ARM_UP only has to carry the pad past the top of the
# 12.3 mm bore, so it is that plus a wall rather than a round number. (It also
# sets how wide the cradle sits -- 10.0 put the outer arm over the sled's edge.)
ARM_PAD_WALL = 1.2    # material above the bore at the top of the pad
ARM_UP = case.MAGNET_D / 2 + case.MAGNET_CLEAR + ARM_PAD_WALL
ARM_DOWN = 7.5        # and down toward the gap at the bottom; keeps the 12.3 mm
                      # magnet inside the pad, which is centered on the tangent

NOMINAL_Z = SLED_T + GAUGE_GAP + case.R_OUT    # case center above the sled's underside, before it is tipped


def front_rail():
    """Wide enough that both arms sit entirely on the sled's front rail."""
    return -_arms().bounding_box()[1] + FRONT_MARGIN


def _right_arm_rear():
    """How far back the right arm's footprint reaches (its lowest y)."""
    polys = _arms().project().to_polygons()
    right = min(polys, key=lambda p: p[:, 0].mean())
    return float(right[:, 1].min())


def _trapezoid(half_front, half_rear, length):
    return CrossSection([[(-half_front, 0), (-half_rear, -length), (half_rear, -length), (half_front, 0)]])


def _front_chain():
    """The front edge's corners, from the left to the right: the upper hull of
    the arms' footprint, moved forward by FRONT_MARGIN. It is the shortest
    edge that still clears both arms, so it follows the arms and not a fixed
    slope."""
    pts = sorted({(round(x, 6), round(y, 6)) for poly in _arms().project().to_polygons() for x, y in poly})                                              # right (-x) to left
    hull = []
    for p in pts:
        while len(hull) >= 2 and ((hull[-1][0] - hull[-2][0]) * (p[1] - hull[-2][1])
                                  - (hull[-1][1] - hull[-2][1]) * (p[0] - hull[-2][0])) >= 0:
            hull.pop()
        hull.append(p)
    return [(x, y + FRONT_MARGIN) for x, y in hull[::-1]]       # left (+x) to right


def _front_edge():
    """The front edge's corners from the left arm to the right arm's front
    corner, with the line through the first two carried back to the sled's
    left side so the angle continues instead of ending in a square corner."""
    chain = _front_chain()
    tip = max(range(len(chain)), key=lambda i: chain[i][1])
    return chain[:tip + 1]


def _cross_y0(p, q):
    """x where the line through p and q is level with the well's mouth (y = 0)."""
    return p[0] + (0.0 - p[1]) * (q[0] - p[0]) / (q[1] - p[1])


def front_wedge(half_f):
    """The part of the sled that comes forward past the well's mouth: the
    angled edge along the arms up to the right arm's front corner, then flat
    to the right side."""
    edge = _front_edge()
    ahead = [p for p in edge if p[1] > 0]
    first = edge.index(ahead[0])
    start = (_cross_y0(edge[first - 1], edge[first]), 0.0) if first else edge[0]
    tip = edge[-1]
    return CrossSection([[start] + ahead + [(-half_f, tip[1]), (-half_f, 0)]])


def left_cut(half_f):
    """The corner of the well's outline that sits ahead of the angled edge,
    so the edge carries on to the sled's left side."""
    edge = _front_edge()
    ahead = [p for p in edge if p[1] > 0]
    first = edge.index(ahead[0])
    p, q = edge[first - 1], edge[first]
    x_c, far = _cross_y0(p, q), half_f + 5
    y_far = p[1] + (far - p[0]) * (q[1] - p[1]) / (q[0] - p[0])
    return CrossSection([[(x_c, 0.5), (x_c, 0.0), (far, y_far), (far, 0.5)]])


def sled():
    """Frame that sits on the flat rim of the tray well."""
    taper = (WELL_W_FRONT - WELL_W_REAR) / 2 / WELL_LEN
    half_f = (WELL_W_FRONT - 2 * CLEAR) / 2
    half_r = half_f - taper * SLED_LEN
    frame = _trapezoid(half_f, half_r, SLED_LEN) - left_cut(half_f)
    frame += front_wedge(half_f)
    frame = frame.extrude(SLED_T)

    # The front rail is widened to reach past the back of the arms, so each one
    # stands on solid material all the way instead of overhanging a window.
    rail, rib, front = SLED_RAIL, 12.0, front_rail()
    at = lambda y: half_f - taper * y                      # half width, y back from the mouth
    inner = _trapezoid(at(front) - rail, at(SLED_LEN - rail) - rail,
                       SLED_LEN - rail - front).translate([0, -front])
    # The right arm stands well forward of the left, so the right-hand front
    # window can run on up to it instead of stopping where the left arm's rail ends.
    y_right = _right_arm_rear() - FRONT_MARGIN
    if y_right > -front:
        inner += CrossSection([[(-rib / 2, y_right), (-(half_f - rail), y_right), (-(half_f - rail), 0),
                                (-(at(front) - rail), -front), (-rib / 2, -front)]])
    windows = inner.extrude(SLED_T + 2).translate([0, 0, -1])
    windows -= box(-rib / 2, rib / 2, -SLED_LEN, 0, -1, SLED_T + 1)      # center rib
    windows -= box(-half_f, half_f, -SLED_LEN / 2 - rib / 2, -SLED_LEN / 2 + rib / 2, -1, SLED_T + 1)
    return frame - windows


def _disc_r():
    """Radius, from the case's axis, of the center of a magnet in an arm."""
    return case.SIDE_PAD_R + ARM_FIT + case.MAGNET_LEADIN + case.MAGNET_T / 2


# The turn is about the left magnet (the 315 deg one, +x), which stays put in
# plan. Everything is built stood up and square at the nominal height, then
# yawed and tipped about that point, then lifted until the tipped case's
# lowest edge is GAUGE_GAP clear of the sled.
_LEFT = max(case.SIDE_MAGNET_DEG, key=lambda d: math.cos(math.radians(d)))
PIVOT = (GAUGE_X + _disc_r() * math.cos(math.radians(_LEFT)), -case.SIDE_MAGNET_Z,
         NOMINAL_Z + _disc_r() * math.sin(math.radians(_LEFT)))


def _pose(solid):
    solid = solid.rotate([90, 0, 0]).translate([GAUGE_X, GAUGE_Y, NOMINAL_Z])
    return solid.translate([-v for v in PIVOT]).rotate([PITCH, 0, -YAW]).translate(PIVOT)


# The case's own body, lying along y: how far the tipped case reaches down.
LIFT = SLED_T + GAUGE_GAP - _pose(cyl(case.CUP_LEN + case.MAGNET_FLOOR, case.R_OUT,
                                      -(case.CUP_LEN + case.MAGNET_FLOOR) / 2)).bounding_box()[2]


def _upright(solid):
    """Something built around the case's axis, stood up at the cradle, turned
    toward the driver and tipped up."""
    return _pose(solid).translate([0, 0, LIFT])


def _world_point(x, y, z):
    """Where a point in the case's own (centered) frame ends up."""
    bb = _upright(Manifold.cube([0.2, 0.2, 0.2], True).translate([x, y, z])).bounding_box()
    return tuple((bb[i] + bb[i + 3]) / 2 for i in range(3))


def _magnet_world(deg):
    r, a = _disc_r(), math.radians(deg)
    return _world_point(r * math.cos(a), r * math.sin(a), -case.SIDE_MAGNET_Z + (case.CUP_LEN + case.MAGNET_FLOOR) / 2)


def _bore(radius):
    """The space the case occupies, as a cylinder lying along its axis. Only as
    long as the case: the tipped axis runs down toward the front, and a longer
    tube would cut into the sled out there."""
    length = case.CUP_LEN + case.MAGNET_FLOOR
    return _upright(cyl(length, radius, -length / 2))


def _ccw(points):
    area = sum(x0 * y1 - x1 * y0 for (x0, y0), (x1, y1) in zip(points, points[1:] + points[:1]))
    return points if area > 0 else points[::-1]


def _arm_profile(deg):
    """Outline of one arm's pad in the plane of the screen, as (x, height)
    pairs measured from the case's center."""
    a = math.radians(deg)
    face = case.SIDE_PAD_R + ARM_FIT                  # flat, against the shell's flat pad
    n = (math.cos(a), math.sin(a))                    # outward, toward the pad
    t = (-math.sin(a), math.cos(a))                   # along the pad
    if t[1] < 0:                                      # point it up the case on both sides
        t = (-t[0], -t[1])
    at = lambda depth, along: (depth * n[0] + along * t[0], depth * n[1] + along * t[1])

    inner_top = at(face, ARM_UP)
    inner_bot = at(face, -ARM_DOWN)
    outer_x = at(face + ARM_T_TOP, ARM_UP)[0]         # the outer wall
    # Level across the top, so the arm finishes flat instead of in a peak.
    return _ccw([inner_top, (outer_x, inner_top[1]), (outer_x, inner_bot[1]), inner_bot])


def _arms():
    """Both arms: each pad, and the column straight down from it to the sled.
    The pad's inner face looks up and inward at the case, so everything swept
    down from it is on the far side of the face and clear of the case."""
    part = None
    for deg in case.SIDE_MAGNET_DEG:
        pad = _upright(CrossSection([_arm_profile(deg)]).extrude(ARM_DEPTH).translate([0, 0, -ARM_DEPTH / 2]))
        column = Manifold.batch_hull([pad, pad.translate([0, 0, -200])]) ^ box(-500, 500, -500, 500, SLED_T - 0.01, 500)
        part = column if part is None else part + column
    return part


def cradle():
    part = _arms()
    part -= _bore(case.R_OUT + ARM_FIT)               # keep the pads off the case

    # Pockets facing the shell's magnets. The disc goes in from the pad face,
    # so the collar sits at the mouth and the disc snaps in behind it.
    seat = case.MAGNET_D / 2 + case.MAGNET_PRESS
    free = case.MAGNET_D / 2 + case.MAGNET_CLEAR
    face = case.SIDE_PAD_R + ARM_FIT
    for deg in case.SIDE_MAGNET_DEG:
        pocket = stack(face - 1, [(1.0, free, free),                        # clear of the case
                                  (case.MAGNET_LEADIN, free, seat),         # cone into the seat
                                  (case.MAGNET_T + 0.2, seat, seat)])       # pressed in here
        part -= _upright(pocket.rotate([0, 0, deg]))
    # Turning the pockets off-axis leaves a few zero-volume slivers where they
    # graze the pad; drop them so the slicer never sees them.
    return Manifold.compose([p for p in part.decompose() if p.volume() > 1.0])


def overhang():
    """Arm footprint that hangs off the sled, seen from above."""
    return (cradle().project() - sled().slice(SLED_T / 2)).area()


def check(part):
    """The case has to drop in, the plug has to clear the gap between arms, and
    the arms have to stand on solid sled."""
    ok = True
    over = overhang()
    print(f"  arms off the sled:  {over:6.3f} mm^2 of footprint")
    ok &= over < 0.01
    v = (part ^ _bore(case.R_OUT)).volume()
    print(f"  case in the cradle: overlap {v:6.3f} mm^3")
    ok &= v < 0.01
    # The plug leaves the case sideways through its notch at USB_DEG, at the
    # height of the gauge's center, and the cable runs off from there.
    plug = _upright(case.usb_plug().translate([0, 0, GAUGE_Y]))
    v = (part ^ plug).volume()
    print(f"  plug out the side:  overlap {v:6.3f} mm^3")
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
        air = floating(posed)
        fits &= not air
        print(f"wrote {name}  {bb[3]-bb[0]:.0f} x {bb[4]-bb[1]:.0f} x {bb[5]-bb[2]:.1f} mm, "
              f"{solid.volume()/1000:.0f} cm3, {len(solid.decompose())} piece(s)"
              f"{'' if not air else f', {len(air)} FLOATING region(s)'}")
    chain = _front_edge()
    ml, mr = (_magnet_world(d) for d in case.SIDE_MAGNET_DEG)
    left, right = (ml, mr) if ml[0] > mr[0] else (mr, ml)
    print(f"gauge turned {YAW:.0f} deg toward the driver and tipped up {PITCH:.0f} deg, "
          f"lifted {LIFT:.1f} mm to clear the sled")
    print(f"right magnet is {right[1] - left[1]:+.1f} mm forward and {right[2] - left[2]:+.1f} mm up from the left one")
    y_left = sled().bounding_box()
    print(f"front edge reaches {chain[-1][1]:.1f} mm past the well's mouth on the right; "
          f"the sled is {y_left[4] - y_left[1]:.0f} mm long")
    if not fits:
        raise SystemExit("clearance check failed")


if __name__ == "__main__":
    main()
