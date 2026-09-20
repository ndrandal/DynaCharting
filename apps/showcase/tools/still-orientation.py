#!/usr/bin/env python3
"""ENC-1250 — is a committed showcase still upright, or vertically mirrored?

    python3 apps/showcase/tools/still-orientation.py price-line-area

Orientation cannot be eyeballed off these images. A mirrored random walk is still
a random walk, and the axis numbers beside it are a hand-typed DOM overlay
(specs/2026-09-19-chart-quality-bar/SPEC.md §1.3), so nothing inside the frame
contradicts a flip. It has to be MEASURED, against the data the capture replayed.

The method, for a baseline-area view (rect4 = x0, y0=baseline, x1, y1=value):

  1. Decode the still and, per pixel column, find the lowest row carrying fill
     colour — the boundary between the fill and the background.
  2. Decode records.json (the dataplane bytes the capture replayed) back into
     rect4 records, giving the value each column depicts.
  3. view.json's baked transform says a value maps to clip y = sy*v + ty, and the
     presented raster puts larger clip y at a SMALLER row. So the boundary row
     must move -sy/2*H pixels per unit of value if the frame is upright, and
     +sy/2*H if it is mirrored. The two hypotheses differ only in SIGN, which is
     what makes this decisive rather than approximate.
  4. Fit row against value by least squares and compare the slope to both.

The capture's X alignment is not knowable a priori (`xAnchor: true` re-derives it
at replay time, and a still is one frame of an animation), so step 4 searches
pixels-per-record and origin column for the alignment that maximises |r|. A wrong
alignment pairs a column with the wrong record and attenuates the slope toward
zero — it cannot manufacture a sign.

Exit 0 upright, 1 mirrored, 2 could not run.

Verified 2026-09-19 at 2423de6: price-line-area -> MIRRORED, slope +36.40 px/$,
r=+0.9895. Every committed still predates the ENC-696 blit fix — LIMITATIONS.md
DC-L14.
"""

import base64
import json
import os
import struct
import sys
import zlib

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
VIEWS = os.path.join(REPO, "apps", "showcase", "views")
STILLS = os.path.join(REPO, "apps", "showcase", "stills")


def die(msg, code=2):
    print("CANNOT RUN: " + msg, file=sys.stderr)
    sys.exit(code)


def decode_png(path):
    """Minimal PNG decoder: 8-bit truecolour (ct 2) or truecolour+alpha (ct 6)."""
    d = open(path, "rb").read()
    if d[:8] != b"\x89PNG\r\n\x1a\n":
        die("%s is not a PNG" % path)
    pos, idat, w, h, ct = 8, b"", None, None, None
    while pos < len(d):
        ln = struct.unpack(">I", d[pos:pos + 4])[0]
        typ = d[pos + 4:pos + 8]
        if typ == b"IHDR":
            w, h, bd, ct, comp, filt, il = struct.unpack(">IIBBBBB", d[pos + 8:pos + 21])
            if bd != 8 or ct not in (2, 6) or il != 0:
                die("unsupported PNG (bitdepth %d, colourtype %d, interlace %d)" % (bd, ct, il))
        elif typ == b"IDAT":
            idat += d[pos + 8:pos + 8 + ln]
        pos += 12 + ln
    nch = 3 if ct == 2 else 4
    raw = zlib.decompress(idat)
    stride = w * nch
    px = bytearray(w * h * nch)
    prev = bytearray(stride)
    o = 0
    for y in range(h):
        f = raw[o]
        o += 1
        line = bytearray(raw[o:o + stride])
        o += stride
        if f:
            for i in range(stride):
                a = line[i - nch] if i >= nch else 0
                b = prev[i]
                c = prev[i - nch] if i >= nch else 0
                if f == 1:
                    line[i] = (line[i] + a) & 255
                elif f == 2:
                    line[i] = (line[i] + b) & 255
                elif f == 3:
                    line[i] = (line[i] + ((a + b) >> 1)) & 255
                elif f == 4:
                    p = a + b - c
                    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                    pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                    line[i] = (line[i] + pr) & 255
                else:
                    die("unknown PNG filter %d" % f)
        px[y * stride:(y + 1) * stride] = line
        prev = line
    return w, h, nch, px


def decode_records(path):
    """records.json -> the rect4 records the replay ended up holding.

    Dataplane framing, per CLAUDE.md: [1B op][4B bufferId][4B offset][4B len][payload].
    """
    doc = json.load(open(path))
    buf = {}
    for fr in doc["frames"]:
        raw = base64.b64decode(fr["b64"])
        o = 0
        while o < len(raw):
            _op = raw[o]
            _bid, off, n = struct.unpack("<III", raw[o + 1:o + 13])
            o += 13
            for i in range(n):
                buf[off + i] = raw[o + i]
            o += n
    if not buf:
        die("%s carried no dataplane bytes" % path)
    b = bytes(buf.get(i, 0) for i in range(max(buf) + 1))
    return [struct.unpack("<ffff", b[i:i + 16]) for i in range(0, len(b) - 15, 16)]


def fit(xs, ys):
    n = len(xs)
    mx, my = sum(xs) / n, sum(ys) / n
    sxy = sum((a - mx) * (b - my) for a, b in zip(xs, ys))
    sxx = sum((a - mx) ** 2 for a in xs)
    syy = sum((b - my) ** 2 for b in ys)
    if sxx == 0 or syy == 0:
        return None
    A = sxy / sxx
    r = sxy / (sxx * syy) ** 0.5
    res = sorted(abs(b - (A * a + (my - A * mx))) for a, b in zip(xs, ys))
    return A, r, res[len(res) // 2]


def main():
    view = sys.argv[1] if len(sys.argv) > 1 else "price-line-area"
    view = os.path.basename(view).replace(".png", "")
    vd = os.path.join(VIEWS, view)
    still = os.path.join(STILLS, view + ".png")
    for p in (still, os.path.join(vd, "view.json"), os.path.join(vd, "records.json")):
        if not os.path.exists(p):
            die("missing %s" % p)

    vj = json.load(open(os.path.join(vd, "view.json")))
    tr = vj.get("transform") or {}
    sy = tr.get("sy")
    if not sy:
        die("%s/view.json has no transform.sy — this tool needs a baked Y mapping" % view)

    # This method reads the fill's lower boundary as "the value", which is only
    # true for a rect4 baseline area. Refuse anything else up front rather than
    # fitting noise: a candle6 stream parses as 4-float groups just fine and would
    # produce a meaningless slope.
    mf = os.path.join(vd, "manifest.ts")
    if not os.path.exists(mf) or "'rect4'" not in open(mf).read():
        die("%s is not a rect4 baseline-area view (manifest declares no 'rect4' "
            "format) — this tool reads the fill's lower edge as the value and has "
            "no meaning for other marks" % view)

    recs = [r for r in decode_records(os.path.join(vd, "records.json")) if r != (0, 0, 0, 0)]
    if len(recs) < 30:
        die("only %d usable rect4 records" % len(recs))

    w, h, nch, px = decode_png(still)

    # The fill colour, from the view's own legend (falling back to "any strongly
    # saturated pixel"). We want the LOWEST fill row per column.
    legend = (vj.get("chrome") or {}).get("legend") or []
    col = (legend[0].get("color") if legend else None) or [0.24, 0.86, 0.52, 1.0]
    tgt = [int(255 * c) for c in col[:3]]
    dom = max(range(3), key=lambda i: tgt[i])
    others = [i for i in range(3) if i != dom]

    def is_fill(x, y):
        i = (y * w + x) * nch
        c = (px[i], px[i + 1], px[i + 2])
        return c[dom] > 50 and all(c[dom] > c[o] + 20 for o in others)

    bottom = {}
    for x in range(w):
        last = None
        for y in range(h):
            if is_fill(x, y):
                last = y
        bottom[x] = last
    if sum(1 for v in bottom.values() if v is not None) < 50:
        die("found almost no fill-coloured pixels in %s" % still)

    # Search the capture's X alignment; see the module docstring.
    best = None
    for ppr10 in range(20, 200):
        ppr = ppr10 / 10.0
        for x0 in range(0, 240, 2):
            X, Y = [], []
            for idx, rec in enumerate(recs):
                c = int(x0 + idx * ppr)
                if c < 2 or c >= w - 2 or bottom.get(c) is None:
                    continue
                X.append(rec[3])
                Y.append(bottom[c])
            if len(X) < 50:
                continue
            f = fit(X, Y)
            if f and (best is None or abs(f[1]) > abs(best[0][1])):
                best = (f, ppr, x0, len(X))
    if best is None:
        die("could not align the still to its records")
    (A, r, mad), ppr, x0, n = best

    up = -sy / 2.0 * h
    mir = +sy / 2.0 * h
    mirrored = abs(A - mir) < abs(A - up)

    print("still     : %s  (%dx%d)" % (os.path.relpath(still, REPO), w, h))
    print("records   : %d rect4 records, %d matched to columns" % (len(recs), n))
    print("alignment : %.1f px/record from column %d" % (ppr, x0))
    print("fit       : slope %+.2f px/unit   r = %+.4f   median|resid| = %.1f px"
          % (A, r, mad))
    print("predicted : upright %+.2f   mirrored %+.2f   (sy=%.9f, H=%d)"
          % (up, mir, sy, h))
    print()
    if abs(r) < 0.8:
        die("fit too weak to rule (|r| = %.3f < 0.8)" % abs(r))
    print("VERDICT   : %s" % ("MIRRORED — see LIMITATIONS.md DC-L14" if mirrored
                              else "UPRIGHT"))
    return 1 if mirrored else 0


if __name__ == "__main__":
    sys.exit(main())
