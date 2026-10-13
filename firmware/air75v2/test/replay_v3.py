#!/usr/bin/env python3
"""Replay a real Air75 V3 capture (tools/capture-lighting.txt) against the native device simulator and compare."""
import re, subprocess, sys, collections
CAP = sys.argv[1] if len(sys.argv) > 1 else "../../tools/capture-lighting.txt"
frames = []
for line in open(CAP):
    m = re.match(r"^(\d+)ms (tx|rx) id=\d+ (.*)$", line.strip())
    if m: frames.append((m[2], bytes(int(x, 16) for x in m[3].split())))
tx = [f for d, f in frames if d == "tx"]
# pair each tx with the real response that followed it (the next rx before the next tx)
pairs, i = [], 0
while i < len(frames):
    if frames[i][0] == "tx":
        resp = None
        j = i + 1
        while j < len(frames) and frames[j][0] == "rx":
            if frames[j][1][1] == frames[i][1][1]: resp = frames[j][1]; break
            j += 1
        pairs.append((frames[i][1], resp)); 
    i += 1
sim = subprocess.Popen(["./test/sim"], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
stats = collections.Counter(); bad = collections.defaultdict(list)
def csum(f): return sum(f[4:]) & 255
for req, real in pairs:
    sim.stdin.write(req); sim.stdin.flush()
    ours = sim.stdout.read(64)
    cmd = req[1]
    silent = ours == bytes(64)
    stats["frames"] += 1
    if real is None:
        stats["real silent"] += 1
        if not silent: bad["we answered, real didn't"].append(cmd)
        continue
    if silent:
        bad["we are silent, real answered"].append(cmd); continue
    ok = ours[0] == 0xAA and ours[1] == cmd and ours[2] == 0 and ours[3] == csum(ours)
    ok &= ours[4:8] == real[4:8] if cmd != 0xEE else True      # header echo (len/addr/handle ^ K)
    ok &= ours[0] == real[0] and ours[1] == real[1]
    if cmd in (0xB3, 0xC3, 0xD6, 0xB9, 0xBC, 0xB6):             # writes are acknowledged by echoing the request
        ok &= ours[8:] == real[8:] and ours[3] == real[3]
    if ok: stats["ok"] += 1
    else: bad["header/ack mismatch"].append(cmd)
print(dict(stats))
cmds = collections.Counter(f[1] for f in tx)
print("commands in capture:", {hex(k): v for k, v in sorted(cmds.items())})
for k, v in bad.items(): print("PROBLEM", k, {hex(c): n for c, n in collections.Counter(v).items()})
print("RESULT:", "PASS" if not bad else "FAIL")
sim.stdin.close()
