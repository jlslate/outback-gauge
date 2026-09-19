"""Case for the Waveshare ESP32-S3-Touch-LCD-1.46B, widened-cover-glass version.

Writes each part to tools/case/stl/ as both .3mf and .stl:
  case_front.stl                shell with a front lip that holds the 49 mm glass
  case_back_battery_magnet.stl  back cover with a bay for an 802525 LiPo and
                                pockets for two 12x2 mm mounting magnets

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
import zipfile

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

# Two 12x2 mm discs sit in pockets in the back's outside face and snap onto a
# matching pair stuck to the car. Two side by side stop it rotating.
MAGNET_D, MAGNET_T = 12.0, 2.0
MAGNET_ADHESIVE = 0.4       # 3M 4920 VHB on the magnets as sold
MAGNET_X = 10.0             # pocket centers at x = +/-10 (left and right)
MAGNET_FLOOR = 3.2          # back cover floor; leaves 0.7 mm under each pocket
MAGNET_POCKET = MAGNET_T + MAGNET_ADHESIVE + 0.1

# The sled's cradle arms hold the shell by two magnets set into its side wall,
# at the angles where the arms touch (0 deg = 3 o'clock, counter-clockwise).
# The wall is only 2.2 mm, so it is thickened inward behind each magnet; at
# these angles the board's edge is far enough in to leave room.
SIDE_MAGNET_DEG = (240.0, 300.0)
SIDE_BOSS_R = 22.5          # inner face of the added material, behind the pocket
SIDE_BOSS_HALF_DEG = 13.0     # stays clear of the USB-C plug at the bottom
SIDE_MAGNET_Z = 9.5         # depth of the pocket centers behind the glass
# The shell's outside is convex, so a magnet sitting level with the tangent
# point would stand proud around the rim of its pocket. 0.8 mm of extra depth
# sinks the whole disc below the surface. (The arms are concave, so theirs
# sit level.)
SIDE_POCKET_SINK = 0.8


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

    for deg in SIDE_MAGNET_DEG:                       # cradle magnets
        shell += sector(SIDE_BOSS_R, R_BORE, deg, SIDE_BOSS_HALF_DEG, GLASS_BACK + 0.6, CUP_LEN)
        pocket = Manifold.cylinder(MAGNET_POCKET + SIDE_POCKET_SINK + 1, MAGNET_D / 2 + 0.15,
                                   MAGNET_D / 2 + 0.15, 64)
        pocket = pocket.rotate([0, 90, 0]).translate([R_OUT - MAGNET_POCKET - SIDE_POCKET_SINK, 0, 0])
        shell -= radial(pocket, deg, SIDE_MAGNET_Z)
    return shell


def back_cover(bay=BATTERY_BAY, magnets=True):
    """Back cover. `bay` adds depth for the battery; `magnets` cuts the two
    mounting pockets (the dock carries the gauge instead, so it uses neither)."""
    rim = CUP_LEN
    inner = rim + bay           # inside face of the floor; the battery lies on it
    back = inner + MAGNET_FLOOR

    cover = cyl(MAGNET_FLOOR, R_OUT, inner)
    if bay:
        cover += cyl(bay, R_OUT, rim) - cyl(bay, ARC_R_OUT, rim)
    for deg, half in ARCS:
        cover += sector(ARC_R_IN, ARC_R_OUT, deg, half, GLASS_BACK + FOAM, inner + 0.01)
        cover -= radial_hole(1.7, deg, LOCK_SCREW_Z, ARC_R_IN - 1)  # M2 self-tapping pilot

    if magnets:
        for x in (-MAGNET_X, MAGNET_X):
            cover -= cyl(MAGNET_POCKET + 1, MAGNET_D / 2 + 0.15, back - MAGNET_POCKET, x=x)

    for deg in (30, 150, 210, 330):                                 # vents, clear of the magnets
        cover -= sector(18.5, 20.5, deg, 14, inner - 1, back + 1)
    cover -= ring_chamfer(R_OUT, back, 0.6, front=False)
    return cover


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
        v = (solid ^ board).volume()
        print(f"  {name:28s} overlap with board   {v:7.3f} mm^3")
        ok &= v < 0.01
    v = (parts["case_front"] ^ usb_plug()).volume()
    print(f"  {'USB-C plug path':28s} overlap with shell   {v:7.3f} mm^3")
    ok &= v < 0.01
    battery = box(-12.5, 12.5, -12.5, 12.5, CUP_LEN + 0.5, CUP_LEN + 8.5)
    v = (parts["case_back_battery_magnet"] ^ battery).volume()
    print(f"  {'802525 battery':28s} overlap with cover   {v:7.3f} mm^3")
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


CONTENT_TYPES = """<?xml version="1.0" encoding="UTF-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">\
<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>\
<Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/></Types>"""

RELS = """<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">\
<Relationship Id="rel0" Target="/3D/3dmodel.model" \
Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/></Relationships>"""


def write_3mf(solid, path, name):
    """3MF carries millimeter units, so slicers import it at the right size."""
    mesh = solid.to_mesh()
    verts = np.asarray(mesh.vert_properties)[:, :3]
    tris = np.asarray(mesh.tri_verts)
    body = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<model unit="millimeter" xml:lang="en-US" '
        'xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02">',
        f'<metadata name="Title">{name}</metadata>',
        '<resources><object id="1" type="model"><mesh><vertices>',
    ]
    body += [f'<vertex x="{x:.4f}" y="{y:.4f}" z="{z:.4f}"/>' for x, y, z in verts]
    body.append("</vertices><triangles>")
    body += [f'<triangle v1="{a}" v2="{b}" v3="{c}"/>' for a, b, c in tris]
    body.append('</triangles></mesh></object></resources><build><item objectid="1"/></build></model>')

    # Fixed timestamps keep the zip byte-identical when nothing changed, so
    # regenerating doesn't show up as a diff.
    def entry(name):
        return zipfile.ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))

    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr(entry("[Content_Types].xml"), CONTENT_TYPES)
        z.writestr(entry("_rels/.rels"), RELS)
        z.writestr(entry("3D/3dmodel.model"), "".join(body))


def print_pose(solid, flip=False):
    """Flat face on the bed at z = 0."""
    if flip:
        solid = solid.mirror([0, 0, 1])
    lo = solid.bounding_box()[2]
    return solid.translate([0, 0, -lo])


def main():
    parts = {
        "case_front": front_shell(),
        "case_back_battery_magnet": back_cover(),
    }
    print("Fit check against the board outline:")
    fits = check(parts)

    out = pathlib.Path(__file__).parent / "stl"
    out.mkdir(exist_ok=True)
    for name, solid in parts.items():
        flip = name.startswith("case_back")  # outside face down, arcs pointing up
        posed = print_pose(solid, flip)
        write_3mf(posed, out / f"{name}.3mf", name)
        write_stl(posed, out / f"{name}.stl")
        bb = solid.bounding_box()
        print(f"  wrote {name}.3mf and .stl  {bb[3]-bb[0]:.1f} x {bb[4]-bb[1]:.1f} x {bb[5]-bb[2]:.1f} mm")
    print(f"Case: {2 * R_OUT:.1f} mm across, {CUP_LEN + BATTERY_BAY + MAGNET_FLOOR:.1f} mm deep, "
          f"{MAGNET_T + MAGNET_ADHESIVE:.1f} mm off the trim on its magnets")
    if not fits:
        raise SystemExit("fit check failed")


if __name__ == "__main__":
    main()
