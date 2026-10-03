"""Draws the sled from above, as someone sitting in front of it would see it:
front edge at the bottom of the page, the viewer's left on the page's left.

  tools/case/.venv/bin/python tools/case/plan_view.py
"""

import pathlib

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import PathPatch
from matplotlib.path import Path

import case
import sled


def main():
    fig, ax = plt.subplots(figsize=(8, 5.6), facecolor="#1b1c20")
    ax.set_facecolor("#1b1c20")

    def draw(cs, **kw):
        # +x is the viewer's left; +y is the front. Page: left = +x, down = front.
        # One compound path, so a window in the sled stays a hole.
        verts, codes = [], []
        for poly in cs.to_polygons():
            pts = [(-x, -y) for x, y in poly]
            verts += pts + [pts[0]]
            codes += [Path.MOVETO] + [Path.LINETO] * (len(pts) - 1) + [Path.CLOSEPOLY]
        ax.add_patch(PathPatch(Path(verts, codes), **kw))

    draw(sled.sled().slice(1.5), fc="#3a3d45", ec="#8a8f9a")
    draw(sled.cradle().project(), fc="#FF6D00", ec="none", alpha=0.85)
    draw(sled._upright(case.cyl(18.4, case.R_OUT, -9.2)).project(), fc="#29B6F6", alpha=0.45, ec="#29B6F6")
    draw(sled._upright(case.usb_plug().translate([0, 0, sled.GAUGE_Y])).project(), fc="#ffffff", ec="none", alpha=0.35)
    for deg in case.SIDE_MAGNET_DEG:
        x, y = sled._magnet_world(deg)[:2]
        ax.plot(-x, -y, "o", color="white", ms=6)

    ax.set_aspect("equal")
    ax.set_xlim(-60, 60)
    ax.set_ylim(-42, 40)
    ax.axis("off")
    ax.set_title(f"Sled from above, gauge turned {sled.YAW:.0f}° toward the driver (left), tipped up {sled.PITCH:.0f}°\n"
                 "front edge at the bottom; USB-C plug (white) leaves on the left",
                 color="#eee", fontsize=10)
    out = pathlib.Path(__file__).parents[2] / "docs" / "sled-plan.png"
    fig.savefig(out, dpi=110, facecolor=fig.get_facecolor(), bbox_inches="tight")
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
