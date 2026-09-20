"""Test coupon for the magnet press fit.

Four pockets in a bar, each a different seat diameter, bored sideways so they
print the same way the real ones do -- a bore in a vertical wall, which is
what decides how it comes out. Press a magnet into each and keep the number
of the tightest one that still goes in without a fight. Ticks above each
pocket count 1 to 4, left to right.

Put that seat diameter into MAGNET_PRESS in case.py:
    MAGNET_PRESS = (seat - MAGNET_D) / 2

  tools/case/.venv/bin/python tools/case/magnet_test.py
"""

import pathlib

import case
from case import box, print_pose, stack, write_3mf, write_stl

SEATS = (12.10, 12.00, 11.90, 11.80)   # diameters to try
PITCH = 18.0
T = 6.0          # wall thickness, a little over the real 5.5 mm of boss
H = 16.0
DEPTH = 3.3      # face to floor, as in the shell


def coupon():
    n = len(SEATS)
    length = PITCH * n + 8
    bar = box(0, length, 0, T, 0, H)
    for i, seat in enumerate(SEATS):
        x = 12 + PITCH * i
        free = case.MAGNET_D / 2 + case.MAGNET_CLEAR
        pocket = stack(T - DEPTH, [(case.MAGNET_T + 0.1, seat / 2, seat / 2),
                                   (case.MAGNET_LEADIN, seat / 2, free),
                                   (DEPTH - case.MAGNET_T - 0.1 - case.MAGNET_LEADIN + 0.5, free, free)])
        bar -= pocket.rotate([0, 0, 90]).translate([x, 0, H / 2])
        for t in range(i + 1):
            bar += box(x - 3 + 1.6 * t, x - 3 + 1.6 * t + 0.8, 1.0, 5.0, H, H + 1.0)
    return bar


def main():
    out = pathlib.Path(__file__).parent / "stl"
    out.mkdir(exist_ok=True)
    solid = coupon()
    posed = print_pose(solid)
    air = case.floating(posed)
    write_3mf(posed, out / "magnet_test.3mf", "magnet_test")
    write_stl(posed, out / "magnet_test.stl")
    bb = solid.bounding_box()
    print(f"wrote magnet_test  {bb[3]-bb[0]:.0f} x {bb[4]-bb[1]:.0f} x {bb[5]-bb[2]:.0f} mm, "
          f"{solid.volume()/1000:.1f} cm3, {len(solid.decompose())} piece(s)"
          f"{'' if not air else f', {len(air)} FLOATING region(s)'}")
    for i, seat in enumerate(SEATS):
        print(f"  {i+1} tick{'s' if i else ' '}: seat {seat:.2f} mm  "
              f"-> MAGNET_PRESS = {(seat - case.MAGNET_D) / 2:+.3f}")
    if air:
        raise SystemExit("floating regions")


if __name__ == "__main__":
    main()
