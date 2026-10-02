"""Draws the sled from above, as someone sitting in front of it would see it:
front edge at the bottom of the page, the viewer's left on the page's left.

  tools/case/.venv/bin/python tools/case/plan_view.py
"""

import pathlib

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

import case
import sled


def main():
    fig, ax = plt.subplots(figsize=(8, 5.6), facecolor="#1b1c20")
    ax.set_facecolor("#1b1c20")

    def draw(cs, **kw):
        # +x is the viewer's left; +y is the front. Page: left = +x, down = front.
        for poly in cs.to_polygons():
            ax.fill([-x for x, y in poly], [-y for x, y in poly], **kw)

    draw(sled.sled().slice(1.5), color="#3a3d45", ec="#8a8f9a")
    draw(sled.cradle().project(), color="#FF6D00", alpha=0.85)
    draw(sled._upright(case.cyl(18.4, case.R_OUT, -9.2)).project(), color="#29B6F6", alpha=0.45, ec="#29B6F6")
    draw(sled._upright(case.usb_plug().translate([0, 0, sled.GAUGE_Y])).project(), color="#ffffff", alpha=0.35)
    for deg in case.SIDE_MAGNET_DEG:
        x, y = sled._yawed_magnet(deg)
        ax.plot(-x, -y, "o", color="white", ms=6)

    ax.set_aspect("equal")
    ax.set_xlim(-60, 60)
    ax.set_ylim(-22, 40)
    ax.axis("off")
    ax.set_title(f"Sled from above, gauge turned {sled.YAW:.1f}° toward the driver (left)\n"
                 "front edge at the bottom; USB-C plug (white) leaves on the left",
                 color="#eee", fontsize=10)
    out = pathlib.Path(__file__).parents[2] / "docs" / "sled-plan.png"
    fig.savefig(out, dpi=110, facecolor=fig.get_facecolor(), bbox_inches="tight")
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
