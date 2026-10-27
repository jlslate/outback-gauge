"""Finds what changed when you pressed the button, from a capture.py log.

  tools/can-capture/.venv/bin/python tools/can-capture/analyze.py LOGFILE

For each mark it compares the frames just before with the frames just after,
and reports:

  1. Bytes that held one steady value before a mark and a different steady
     value after it (counters and checksums never hold still, so they drop
     out), ranked by how many marks they changed at. A byte that flips at every
     press, or flips on the "off" marks and back on the "on" marks, is the car
     telling the rest of the network what state start-stop is in.
  2. Ids that sent frames just after marks far more often than at other times,
     which is how a one-off button-press message would look.

It reads logs only. Nothing here talks to the car.
"""

import argparse
import collections
import re
import sys

LINE = re.compile(r"^\s*(\d+\.\d+)\s+(.*)$")
FRAME = re.compile(r"^([0-9A-F]{3}|[0-9A-F]{8})((?:\s+[0-9A-F]{2})+)$")


def parse(path):
    frames, marks = [], []
    for raw in open(path):
        m = LINE.match(raw)
        if not m:
            continue
        t, text = float(m.group(1)), m.group(2).strip()
        if text.startswith("#MARK"):
            parts = text.split(None, 2)
            marks.append((t, parts[2] if len(parts) > 2 else ""))
        elif not text.startswith("#"):
            f = FRAME.match(text.upper())
            if f:
                frames.append((t, f.group(1), [int(b, 16) for b in f.group(2).split()]))
    return frames, marks


def steady(values, share=0.9):
    """The one value that most of these samples agree on, or None."""
    if len(values) < 3:
        return None
    v, n = collections.Counter(values).most_common(1)[0]
    return v if n / len(values) >= share else None


def changed_bytes(frames, marks, before=2.0, gap=0.3, after=2.5):
    series = collections.defaultdict(list)                         # (id, byte index) -> [(t, value)]
    for t, ident, data in frames:
        for i, b in enumerate(data):
            series[(ident, i)].append((t, b))
    hits = collections.defaultdict(list)                           # key -> [(mark number, from, to)]
    for k, (mt, _) in enumerate(marks, 1):
        for key, pts in series.items():
            pre = steady([v for t, v in pts if mt - before <= t < mt - 0.05])
            post = steady([v for t, v in pts if mt + gap <= t < mt + after])
            if pre is not None and post is not None and pre != post:
                hits[key].append((k, pre, post))
    return hits


def event_ids(frames, marks, window=1.0):
    """Ids whose traffic is much denser just after marks than overall."""
    total = collections.Counter(i for _, i, _ in frames)
    span = (frames[-1][0] - frames[0][0]) or 1.0
    near = collections.Counter()
    for mt, _ in marks:
        near.update(i for t, i, _ in frames if mt <= t < mt + window)
    out = []
    for ident, n in near.items():
        expected = total[ident] / span * window * len(marks)
        if n >= 2 and n > 3 * max(expected, 0.5):
            out.append((ident, n, expected))
    return sorted(out, key=lambda x: -x[1] / max(x[2], 0.5))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("log")
    ap.add_argument("--top", type=int, default=15)
    args = ap.parse_args()

    frames, marks = parse(args.log)
    if not frames:
        sys.exit("No frames in that log. Was the adapter in monitor mode and the car awake?")
    ids = collections.Counter(i for _, i, _ in frames)
    span = frames[-1][0] - frames[0][0]
    print(f"{len(frames)} frames, {len(ids)} ids, {span:.0f} s, {len(marks)} marks")
    for k, (t, label) in enumerate(marks, 1):
        print(f"  mark {k} at {t:.1f} s {label}")
    if not marks:
        sys.exit("No marks in the log, so there is nothing to compare. Press Return when you press the button.")

    hits = changed_bytes(frames, marks)
    print(f"\nBytes that held a steady value, then a different one, around marks (of {len(marks)}):")
    ranked = sorted(hits.items(), key=lambda kv: (-len(kv[1]), kv[0]))
    shown = 0
    for (ident, idx), ev in ranked:
        if shown >= args.top:
            break
        trail = ", ".join(f"#{k}: {a:02X}->{b:02X}" for k, a, b in ev[:6])
        print(f"  id {ident} byte {idx}: {len(ev)} marks   {trail}")
        shown += 1
    if not hits:
        print("  none. Either nothing on the bus changed, or the button's traffic is not visible at this port.")

    print("\nIds that fire just after marks much more than usual:")
    events = event_ids(frames, marks)
    for ident, n, expected in events[:args.top]:
        print(f"  id {ident}: {n} frames after marks, about {expected:.1f} expected by chance")
    if not events:
        print("  none")


if __name__ == "__main__":
    main()
