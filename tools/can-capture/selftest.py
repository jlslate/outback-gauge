"""Builds a fake capture and checks that analyze.py finds the planted signal.

  tools/can-capture/.venv/bin/python tools/can-capture/selftest.py
"""
import os
import random
import subprocess
import sys
import tempfile

random.seed(1)
lines, t, counter = [], 0.0, 0
marks = [20, 35, 50, 65, 80, 95]
state = 0
while t < 110:
    t += 0.01
    counter = (counter + 1) % 16
    lines.append(f"{t:9.3f} 3A0 {counter:02X} {random.randrange(256):02X} 00 11 22 33 44 55")    # counter + noise byte
    if int(t * 100) % 10 == 0:
        lines.append(f"{t:9.3f} 4B1 00 {state:02X} {0xA5:02X} 00 00 00 00 00")                  # the planted status byte
    if int(t * 100) % 50 == 0:
        lines.append(f"{t:9.3f} 5C2 01 02 03 04 05 06 07 08")
    for m in marks:
        if abs(t - (m + 0.2)) < 0.004:
            state ^= 1                                                                         # flips just after each mark
            lines.append(f"{t:9.3f} 2E0 01 00 00 00 00 00 00 00")                              # a one-off button frame
            lines.append(f"{t + 0.001:9.3f} 2E0 01 00 00 00 00 00 00 00")
for i, m in enumerate(marks, 1):
    lines.append(f"{m:9.3f} #MARK {i} {'off' if i % 2 else 'on'}")
lines.sort(key=lambda s: float(s.split()[0]))
with tempfile.NamedTemporaryFile("w", suffix=".log", delete=False) as f:
    f.write("\n".join(lines) + "\n")
out = subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), "analyze.py"), f.name],
                     capture_output=True, text=True).stdout
print(out)
ok = "id 4B1 byte 1: 6 marks" in out and "id 2E0" in out and "id 3A0" not in out.split("Ids that fire")[0].split("Bytes that")[1]
print("selftest:", "PASS" if ok else "FAIL")
os.unlink(f.name)
sys.exit(0 if ok else 1)
