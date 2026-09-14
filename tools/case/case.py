"""Case for the Waveshare ESP32-S3-Touch-LCD-1.46B, widened-cover-glass version.

Writes printable STLs to tools/case/stl/:
  case_front.stl         shell with a front lip that holds the 49 mm glass
  case_back.stl          slim back cover (USB power, no battery)
  case_back_battery.stl  deeper back cover with room for an 802525 LiPo
  case_back_magnet.stl   slim back cover with pockets for two 12x2 mm magnets
  gopro_mount.stl        two-finger GoPro-style mount, bolts to either back

The board drops into the shell from behind, glass first, until the glass
meets the lip. Three arcs on the back cover reach forward and press on the
glass's rear edge (through a strip of soft 1/16" foam, which takes up
tolerance and damps vibration). Three M2x4 screws through the shell wall
lock the arcs in place.

Coordinates: looking at the screen, x right, y up; z is depth from the
front face of the case, increasing toward the back. Board positions come
from Waveshare's dimension drawing for this version (ESP32-S3-Touch-LCD-1.46B
-details-size-2.jpg), measured at 10.63 px/mm.

Usage: tools/case/.venv/bin/python tools/case/case.py
"""

import math
import pathlib

import numpy as np
from manifold3d import CrossSection, Manifold

SEG = 128  # circle smoothness

# ---- the board ---------------------------------------------------------------
GLASS_D = 49.0       # widened cover glass
GLASS_T = 2.7        # measured off the side view; the foam gap absorbs error
STACK = 13.2         # glass front to header pin tips (drawing: 13.20)
USB_Z = (8.3, 11.4)  # USB-C receptacle, depth range behind the glass front
BUTTON_Z = 9.0       # PWR/BOOT side switches, depth behind the glass front
PWR_DEG, BOOT_DEG = 32, -38  # angles seen from the front (0 = 3 o'clock)

# ---- the case ----------------------------------------------------------------
LIP = 1.2            # front lip thickness
LIP_OVERLAP = 1.3    # how far the lip covers the glass edge (all black border)
FIT = 0.3            # radial clearance around the glass; sized for MJF (±0.3 mm)
WALL = 2.2
BACK_CLEAR = 0.8     # behind the header pins
FOAM = 1.0           # gap between pusher arcs and glass, filled with compressed 1.6 mm foam
FLOOR = 3.0          # back cover thickness (screw heads recess into it)
BATTERY_BAY = 9.0    # extra depth for an 8 mm thick cell
EDGE_CHAMFER = 0.8

R_BORE = GLASS_D / 2 + FIT
R_OUT = R_BORE + WALL
R_WINDOW = GLASS_D / 2 - LIP_OVERLAP
CUP_LEN = LIP + STACK + BACK_CLEAR
GLASS_BACK = LIP + GLASS_T

# Pusher arcs: (center deg, half-width deg). They stay clear of the USB-C
# notch at the bottom and the PWR/BOOT holes on the right.
ARCS = [(90, 30), (180, 30), (0, 12)]
ARC_R_IN = 22.2      # board parts all sit inside r = 21.5 away from the USB port
ARC_R_OUT = R_BORE - 0.15
LOCK_SCREW_Z = CUP_LEN - 3.0

USB_OPENING = (12.5, 7.9)   # width, height; fits a typical USB-C plug overmold
BUTTON_SLOT = (3.5, 4.0)    # tangential width, height (poke with a toothpick)

MOUNT_SCREW_Y = 13.0        # two M3 screws hold the GoPro mount on

# Magnet back: two 12x2 mm discs sit in pockets in the outside face and snap
# onto a matching pair stuck to the car. Two side by side stop it rotating.
MAGNET_D, MAGNET_T = 12.0, 2.0
MAGNET_ADHESIVE = 0.4       # 3M 4920 VHB on the magnets as sold
MAGNET_X = 10.0             # pocket centers at x = +/-10 (left and right)
MAGNET_FLOOR = 3.2          # a bit thicker than FLOOR so the pocket keeps 0.7 mm under it
MAGNET_POCKET = MAGNET_T + MAGNET_ADHESIVE + 0.1


# ---- helpers -----------------------------------------------------------------

def cyl(h, r, z=0.0, r_top=None, x=0.0, y=0.0, seg=SEG):
    return Manifold.cylinder(h, r, r if r_top is None else r_top, seg).translate([x, y, z])


def box(x0, x1, y0, y1, z0, z1):
    return Manifold.cube([x1 - x0, y1 - y0, z1 - z0]).translate([x0, y0, z0])


def sector(r_in, r_out, center_deg, half_deg, z0, z1):
    a0, a1 = math.radians(center_deg - half_deg), math.radians(center_deg + half_deg)
    n = max(8, int(half_deg * 2))
    outer = [(r_out * math.cos(a0 + (a1 - a0) * i / n), r_out * math.sin(a0 + (a1 - a0) * i / n)) for i in range(n + 1)]
    inner = [(r_in * math.cos(a1 - (a1 - a0) * i / n), r_in * math.sin(a1 - (a1 - a0) * i / n)) for i in range(n + 1)]
    return CrossSection([outer + inner]).extrude(z1 - z0).translate([0, 0, z0])


def radial(solid_along_x, deg, z):
    """Rotate something built along +x to point outward at `deg`, at depth z."""
    return solid_along_x.rotate([0, 0, deg]).translate([0, 0, z])


def radial_hole(d, deg, z, r0=0.0, r1=40.0):
    return radial(Manifold.cylinder(r1 - r0, d / 2, d / 2, 32).rotate([0, 90, 0]).translate([r0, 0, 0]), deg, z)


def ring_chamfer(r, z, c, front=True):
    """Solid to subtract that bevels the outer edge of a disc of radius r at depth z."""
    if front:
        return cyl(c, r + 1, z, r_top=r + 1) - cyl(c, r - c, z, r_top=r)
    return cyl(c, r + 1, z - c, r_top=r + 1) - cyl(c, r, z - c, r_top=r - c)


# ---- parts -------------------------------------------------------------------

def front_shell():
    shell = cyl(CUP_LEN, R_OUT)
    shell -= cyl(CUP_LEN, R_BORE, LIP)
    shell -= cyl(LIP + 1, R_WINDOW, -0.5)
    shell -= cyl(0.8, R_WINDOW + 0.8, 0, r_top=R_WINDOW)  # soften the window edge
    shell -= ring_chamfer(R_OUT, 0, EDGE_CHAMFER)
    shell -= ring_chamfer(R_OUT, CUP_LEN, 0.5, front=False)

    usb_w, usb_h = USB_OPENING
    usb_mid = LIP + sum(USB_Z) / 2
    shell -= box(-usb_w / 2, usb_w / 2, -R_OUT - 1, -R_BORE + 1, usb_mid - usb_h / 2, CUP_LEN + 1)

    bw, bh = BUTTON_SLOT
    for deg in (PWR_DEG, BOOT_DEG):
        slot = box(R_BORE - 1, R_OUT + 1, -bw / 2, bw / 2, -bh / 2, bh / 2)
        shell -= radial(slot, deg, LIP + BUTTON_Z)

    for deg, _ in ARCS:
        shell -= radial_hole(2.2, deg, LOCK_SCREW_Z, R_BORE - 0.5)
    return shell


def back_cover(bay=0.0, magnets=False):
    rim = CUP_LEN
    inner = rim + bay           # inside face of the floor
    floor = MAGNET_FLOOR if magnets else FLOOR
    back = inner + floor

    cover = cyl(floor, R_OUT, inner)
    if bay:
        cover += cyl(bay, R_OUT, rim) - cyl(bay, ARC_R_OUT, rim)
    for deg, half in ARCS:
        cover += sector(ARC_R_IN, ARC_R_OUT, deg, half, GLASS_BACK + FOAM, inner + 0.01)
        cover -= radial_hole(1.7, deg, LOCK_SCREW_Z, ARC_R_IN - 1)  # M2 self-tapping pilot

    if magnets:
        for x in (-MAGNET_X, MAGNET_X):
            cover -= cyl(MAGNET_POCKET + 1, MAGNET_D / 2 + 0.15, back - MAGNET_POCKET, x=x)
    else:
        for y in (-MOUNT_SCREW_Y, MOUNT_SCREW_Y):
            cover -= cyl(floor + 2, 1.65, inner - 1, y=y)
            cover -= cyl(2.0, 3.1, inner - 0.01, y=y)               # M3 head sits flush inside

    for deg in (30, 150, 210, 330):                                 # vents, clear of the screws
        cover -= sector(18.5, 20.5, deg, 14, inner - 1, back + 1)
    cover -= ring_chamfer(R_OUT, back, 0.6, front=False)
    return cover


def gopro_mount():
    base_x, base_y, base_t = 20.0, 34.0, 4.0
    finger_t, gap, r_end, hole_h = 2.9, 3.3, 7.5, 9.0
    mount = box(-base_x / 2, base_x / 2, -base_y / 2, base_y / 2, 0, base_t)

    profile = CrossSection.square([2 * r_end, hole_h]).translate([-r_end, base_t]) + \
        CrossSection.circle(r_end, SEG).translate([0, base_t + hole_h])
    finger = profile.extrude(finger_t)
    # Profile lies in (y, z); extrusion runs along x.
    finger = finger.transform(np.array([[0, 0, 1, 0], [1, 0, 0, 0], [0, 1, 0, 0]], dtype=float))
    for x0 in (gap / 2, -gap / 2 - finger_t):
        mount += finger.translate([x0, 0, 0])
    mount -= Manifold.cylinder(30, 2.65, 2.65, 48).rotate([0, 90, 0]).translate([-15, 0, base_t + hole_h])

    for y in (-MOUNT_SCREW_Y, MOUNT_SCREW_Y):
        mount -= cyl(base_t + 2, 1.65, -1, y=y)
        mount -= cyl(2.6, 3.3, base_t - 2.6 + 0.01, y=y, seg=6)   # M3 nut trap
    return mount


# ---- fit check ---------------------------------------------------------------

def board_envelope():
    """Rough solid of the board as positioned in the case, for interference checks."""
    z = lambda d: LIP + d  # depth behind the glass front -> case z
    parts = [
        cyl(GLASS_T, GLASS_D / 2, LIP),
        cyl(z(7.0) - GLASS_BACK, 19.7, GLASS_BACK) + box(-6, 6, -20.8, -18, GLASS_BACK, z(7.0)),     # LCD
        cyl(z(11.87) - z(7.0), 20.65, z(7.0), y=-1.65) ^ box(-20, 20, -30, 30, 0, 30),            # PCB + parts
        box(-16.4, -13.1, -7.4, 5.3, z(8.1), z(STACK)),                                            # header pins
        box(-4.5, 4.5, -24.1, -17, z(USB_Z[0]), z(USB_Z[1])),                                      # USB-C
    ]
    for x, y in ((0, 17), (12.1, -16.4), (-11.9, -16.8)):
        parts.append(cyl(z(11.87) - z(8.1), 1.8, z(8.1), x=x, y=y))                               # standoffs
    env = parts[0]
    for p in parts[1:]:
        env += p
    return env


def usb_plug():
    mid = LIP + sum(USB_Z) / 2
    return box(-6.1, 6.1, -40, -24.2, mid - 3.25, mid + 3.25)


def check(parts):
    board = board_envelope()
    ok = True
    for name, solid in parts.items():
        if not name.startswith("case_"):
            continue  # the mount sits outside the case
        v = (solid ^ board).volume()
        print(f"  {name:22s} overlap with board {v:7.3f} mm^3")
        ok &= v < 0.01
    v = (parts["case_front"] ^ usb_plug()).volume()
    print(f"  {'USB-C plug path':22s} overlap with shell {v:7.3f} mm^3")
    battery = box(-12.5, 12.5, -12.5, 12.5, CUP_LEN + 0.5, CUP_LEN + 8.5)
    v = (parts["case_back_battery"] ^ battery).volume()
    print(f"  {'802525 battery':22s} overlap with cover {v:7.3f} mm^3")
    return ok and v < 0.01


# ---- output ------------------------------------------------------------------

def write_stl(solid, path):
    mesh = solid.to_mesh()
    verts = np.asarray(mesh.vert_properties)[:, :3]
    tris = np.asarray(mesh.tri_verts)
    a, b, c = verts[tris[:, 0]], verts[tris[:, 1]], verts[tris[:, 2]]
    n = np.cross(b - a, c - a)
    n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
    rec = np.zeros(len(tris), dtype=[("n", "<f4", 3), ("v", "<f4", (3, 3)), ("attr", "<u2")])
    rec["n"], rec["v"] = n, np.stack([a, b, c], axis=1)
    with open(path, "wb") as f:
        f.write(b"outback-gauge case".ljust(80, b" "))
        f.write(np.uint32(len(tris)).tobytes())
        f.write(rec.tobytes())


def print_pose(solid, flip=False):
    """Flat face on the bed at z = 0."""
    if flip:
        solid = solid.mirror([0, 0, 1])
    lo = solid.bounding_box()[2]
    return solid.translate([0, 0, -lo])


def main():
    parts = {
        "case_front": front_shell(),
        "case_back": back_cover(),
        "case_back_battery": back_cover(BATTERY_BAY),
        "case_back_magnet": back_cover(magnets=True),
        "gopro_mount": gopro_mount(),
    }
    print("Fit check against the board outline:")
    fits = check(parts)

    out = pathlib.Path(__file__).parent / "stl"
    out.mkdir(exist_ok=True)
    for name, solid in parts.items():
        flip = name.startswith("case_back")  # outside face down, arcs pointing up
        write_stl(print_pose(solid, flip), out / f"{name}.stl")
        bb = solid.bounding_box()
        print(f"  wrote {name}.stl  {bb[3]-bb[0]:.1f} x {bb[4]-bb[1]:.1f} x {bb[5]-bb[2]:.1f} mm")
    print(f"Case depth: {CUP_LEN + FLOOR:.1f} mm slim, {CUP_LEN + BATTERY_BAY + FLOOR:.1f} mm with battery, "
          f"{CUP_LEN + MAGNET_FLOOR:.1f} mm magnet back (+{MAGNET_T + MAGNET_ADHESIVE:.1f} mm for the car-side "
          f"magnets), {2 * R_OUT:.1f} mm across")
    if not fits:
        raise SystemExit("fit check failed")


if __name__ == "__main__":
    main()
