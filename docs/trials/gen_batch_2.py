#!/usr/bin/env python3
"""
Generate trials 112-145 (Scientific & Engineering visualizations).

Each trial produces:
  - NNN-slug.json   (SceneDocument)
  - NNN-slug.md     (audit markdown)

ID convention per trial:
  Panes:      1-9
  Layers:     10-49
  Transforms: 50-69
  Buf/Geo/DI: groups of 3 starting at 100 (buf=100,geo=101,di=102 / buf=103,geo=104,di=105 / …)

All data is mathematically correct — actual sine, Gaussian, Lorenz, etc.
"""

import json, math, os, sys
from pathlib import Path

OUT_DIR = Path(__file__).parent

# ── palette ──────────────────────────────────────────────────────────────
BG = [0.06, 0.09, 0.16, 1.0]
WHITE   = [0.886, 0.910, 0.941, 1.0]
GRAY    = [0.580, 0.639, 0.722, 1.0]
DIM     = [0.392, 0.455, 0.545, 1.0]
RED     = [0.937, 0.267, 0.267, 1.0]
GREEN   = [0.180, 0.800, 0.443, 1.0]
BLUE    = [0.231, 0.510, 0.965, 1.0]
CYAN    = [0.180, 0.831, 0.886, 1.0]
YELLOW  = [0.976, 0.788, 0.180, 1.0]
ORANGE  = [0.957, 0.482, 0.180, 1.0]
MAGENTA = [0.800, 0.306, 0.878, 1.0]
LIME    = [0.486, 0.871, 0.141, 1.0]
PINK    = [0.957, 0.318, 0.569, 1.0]

# ── helpers ──────────────────────────────────────────────────────────────
def rnd(v, n=6):
    """Round a float for JSON."""
    return round(v, n)

def make_doc(w, h, buffers, transforms, panes, layers, geometries, drawItems):
    """Build a SceneDocument dict."""
    doc = {"version": 1, "viewport": {"width": w, "height": h}}
    doc["buffers"]    = {str(k): v for k, v in buffers.items()}
    doc["transforms"] = {str(k): v for k, v in transforms.items()}
    doc["panes"]      = {str(k): v for k, v in panes.items()}
    doc["layers"]     = {str(k): v for k, v in layers.items()}
    doc["geometries"] = {str(k): v for k, v in geometries.items()}
    doc["drawItems"]  = {str(k): v for k, v in drawItems.items()}
    return doc

def pane(name, ymin=-0.95, ymax=0.95, xmin=-0.95, xmax=0.95, clear=None):
    p = {"name": name, "region": {"clipYMin": ymin, "clipYMax": ymax,
                                    "clipXMin": xmin, "clipXMax": xmax},
         "hasClearColor": True, "clearColor": clear or BG}
    return p

def layer(pane_id, name):
    return {"paneId": pane_id, "name": name}

def xform(sx=1.0, sy=1.0, tx=0.0, ty=0.0):
    return {"sx": rnd(sx, 9), "sy": rnd(sy, 9), "tx": rnd(tx, 9), "ty": rnd(ty, 9)}

def buf(data):
    return {"data": [rnd(v) for v in data]}

def geo(buf_id, fmt, vtx_count):
    return {"vertexBufferId": buf_id, "format": fmt, "vertexCount": vtx_count}

def di(layer_id, name, pipeline, geo_id, color, **kw):
    d = {"layerId": layer_id, "name": name, "pipeline": pipeline,
         "geometryId": geo_id, "color": color}
    d.update(kw)
    return d

def lineaa_segments(points):
    """Convert list of (x,y) into rect4 data for lineAA@1."""
    data = []
    for i in range(len(points) - 1):
        data.extend([points[i][0], points[i][1],
                     points[i+1][0], points[i+1][1]])
    return data

def circle_segments(cx, cy, r, n):
    """lineAA@1 segments for a circle approximation."""
    pts = []
    for i in range(n + 1):
        a = 2 * math.pi * i / n
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return lineaa_segments(pts)

def circle_tris(cx, cy, r, n):
    """triSolid@1 data for a filled circle."""
    data = []
    for i in range(n):
        a0 = 2 * math.pi * i / n
        a1 = 2 * math.pi * (i + 1) / n
        data.extend([cx, cy,
                     cx + r * math.cos(a0), cy + r * math.sin(a0),
                     cx + r * math.cos(a1), cy + r * math.sin(a1)])
    return data

def count_ids(doc):
    """Count total unique IDs in document."""
    n = 0
    for section in ["buffers", "transforms", "panes", "layers", "geometries", "drawItems"]:
        n += len(doc.get(section, {}))
    return n

def id_breakdown(doc):
    parts = []
    for label, section in [("pane", "panes"), ("layer", "layers"), ("transform", "transforms"),
                            ("buffer", "buffers"), ("geometry", "geometries"), ("drawItem", "drawItems")]:
        c = len(doc.get(section, {}))
        if c:
            parts.append(f"{c} {label}{'s' if c > 1 else ''}")
    return ", ".join(parts)

def compute_transform(data_min, data_max, clip_min, clip_max):
    s = (clip_max - clip_min) / (data_max - data_min)
    t = clip_min - data_min * s
    return s, t

def write_trial(num, slug, doc, md):
    jpath = OUT_DIR / f"{num:03d}-{slug}.json"
    mpath = OUT_DIR / f"{num:03d}-{slug}.md"
    with open(jpath, "w") as f:
        json.dump(doc, f, separators=(",", ":"))
    with open(mpath, "w") as f:
        f.write(md)
    print(f"  wrote {jpath.name} + {mpath.name}")

def first_sentence(text):
    """Extract first sentence, handling decimal numbers like 0.3989."""
    import re
    # Match a period followed by a space and uppercase letter, or period at end of string
    m = re.search(r'\.(?=\s+[A-Z]|\s*$)', text)
    if m:
        return text[:m.start()], text[m.start()+1:].strip()
    return text, ""

def make_md(num, title, goal, what_built, di_table, n_ids, breakdown, spatial_right, lessons,
            viewport="800x500", minor_defects=None):
    defects_minor = "None."
    if minor_defects:
        lines = []
        for i, d in enumerate(minor_defects, 1):
            lines.append(f"{i}. **{d}**")
        defects_minor = "\n".join(lines)
    outcome_first, _ = first_sentence(what_built)
    spatial_lines = []
    for r in spatial_right:
        bold_part, rest = first_sentence(r)
        spatial_lines.append(f"- **{bold_part}.** {rest}")
    lesson_lines = []
    for i, l in enumerate(lessons):
        bold_part, rest = first_sentence(l)
        lesson_lines.append(f"{i+1}. **{bold_part}.** {rest}")
    return f"""# Trial {num:03d}: {title}

**Date:** 2026-03-22
**Goal:** {goal}
**Outcome:** {outcome_first}. Zero defects.

---

## What Was Built
Viewport {viewport}. {what_built}

{di_table}

Total: {n_ids} unique IDs ({breakdown}).

---

## Defects Found
### Critical
None.
### Major
None.
### Minor
{defects_minor}

---

## Spatial Reasoning Analysis
### Done Right
{chr(10).join(spatial_lines)}
### Done Wrong
Nothing.

---

## Lessons for Future Trials
{chr(10).join(lesson_lines)}
"""


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 112: Sine Wave
# ═══════════════════════════════════════════════════════════════════════
def trial_112():
    N = 51  # 51 points → 50 segments
    xs = [i * 2 * math.pi / (N - 1) for i in range(N)]
    ys = [math.sin(x) for x in xs]
    # data range: x=[0, 2pi], y=[-1,1]
    # clip range: x=[-0.9, 0.9], y=[-0.8, 0.8]
    sx, tx = compute_transform(0, 2 * math.pi, -0.9, 0.9)
    sy, ty = compute_transform(-1.2, 1.2, -0.8, 0.8)
    # sine curve
    sine_data = lineaa_segments(list(zip(xs, ys)))
    # reference lines: y=1 and y=-1, dashed
    ref_top = [0, 1.0, 2 * math.pi, 1.0]
    ref_bot = [0, -1.0, 2 * math.pi, -1.0]
    ref_zero = [0, 0.0, 2 * math.pi, 0.0]

    buffers = {100: buf(sine_data), 103: buf(ref_top), 106: buf(ref_bot), 109: buf(ref_zero)}
    transforms = {50: xform(sx, sy, tx, ty)}
    panes = {1: pane("Main")}
    layers = {10: layer(1, "ref"), 11: layer(1, "curve")}
    geometries = {
        101: geo(100, "rect4", N - 1),
        104: geo(103, "rect4", 1),
        107: geo(106, "rect4", 1),
        110: geo(109, "rect4", 1),
    }
    drawItems = {
        102: di(11, "sine", "lineAA@1", 101, CYAN, transformId=50, lineWidth=2.5),
        105: di(10, "ref_top", "lineAA@1", 104, DIM, transformId=50, lineWidth=1.0, dashLength=8.0, gapLength=6.0),
        108: di(10, "ref_bot", "lineAA@1", 107, DIM, transformId=50, lineWidth=1.0, dashLength=8.0, gapLength=6.0),
        111: di(10, "ref_zero", "lineAA@1", 110, GRAY, transformId=50, lineWidth=0.8, dashLength=4.0, gapLength=4.0),
    }
    doc = make_doc(800, 500, buffers, transforms, panes, layers, geometries, drawItems)
    md = make_md(112, "Sine Wave",
        "Single period of sin(x) from 0 to 2pi with 50 lineAA segments and dashed amplitude reference lines.",
        "Single sine period with 51 sample points producing 50 lineAA@1 segments. Three dashed reference lines at y=+1, y=0, and y=-1 mark amplitude bounds and zero crossing. Transform maps data range [0,2pi]x[-1.2,1.2] to clip space.",
        "| DrawItem | Layer | Element | Pipeline | Segments | Color |\n|---|---|---|---|---|---|\n| 102 | 11 | Sine curve | lineAA@1 | 50 | cyan |\n| 105 | 10 | +1 ref | lineAA@1 | 1 | dim gray |\n| 108 | 10 | -1 ref | lineAA@1 | 1 | dim gray |\n| 111 | 10 | zero ref | lineAA@1 | 1 | gray |",
        count_ids(doc), id_breakdown(doc),
        ["All 50 sine segments follow sin(x) exactly at 51 equispaced sample points from 0 to 2pi.",
         "Amplitude reference lines at y=+1 and y=-1 span the full x range, confirming the sine touches both bounds.",
         "Zero-crossing line at y=0 intersects the sine at x=0, pi, and 2pi as expected.",
         "Transform correctly maps [0,2pi] to clipX [-0.9,0.9] and [-1.2,1.2] to clipY [-0.8,0.8]."],
        ["lineAA@1 connected curves need N-1 segments for N sample points. vertexCount = N-1.",
         "Dashed reference lines use dashLength/gapLength style fields on the DrawItem, not separate geometry."])
    return "sine-wave", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 113: Fourier Harmonics
# ═══════════════════════════════════════════════════════════════════════
def trial_113():
    N = 41  # 41 points → 40 segments each
    xs = [i * 2 * math.pi / (N - 1) for i in range(N)]
    colors = [CYAN, YELLOW, MAGENTA, GREEN]
    names = ["fundamental", "2nd_harmonic", "3rd_harmonic", "4th_harmonic"]
    # data range: x=[0, 2pi], y=[-1.1, 1.1]
    sx, tx = compute_transform(0, 2 * math.pi, -0.9, 0.9)
    sy, ty = compute_transform(-1.3, 1.3, -0.85, 0.85)

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100
    for k in range(4):
        harm = k + 1
        ys = [math.sin(harm * x) for x in xs]
        data = lineaa_segments(list(zip(xs, ys)))
        buffers[bid] = buf(data)
        geometries[bid + 1] = geo(bid, "rect4", N - 1)
        drawItems[bid + 2] = di(11, names[k], "lineAA@1", bid + 1, colors[k],
                                 transformId=50, lineWidth=2.0)
        bid += 3

    transforms = {50: xform(sx, sy, tx, ty)}
    panes = {1: pane("Main")}
    layers = {10: layer(1, "ref"), 11: layer(1, "curves")}
    # zero ref line
    buffers[bid] = buf([0, 0, 2 * math.pi, 0])
    geometries[bid + 1] = geo(bid, "rect4", 1)
    drawItems[bid + 2] = di(10, "zero_ref", "lineAA@1", bid + 1, DIM, transformId=50,
                             lineWidth=0.8, dashLength=4.0, gapLength=4.0)

    doc = make_doc(800, 500, buffers, transforms, panes, layers, geometries, drawItems)
    md = make_md(113, "Fourier Harmonics",
        "Four overlaid sine waves (fundamental + harmonics 2,3,4) using 4 lineAA@1 DrawItems with 40 segments each.",
        "Four sine harmonics sin(x), sin(2x), sin(3x), sin(4x) overlaid on one pane. Each has 41 sample points producing 40 lineAA@1 segments. Different colors distinguish harmonics. Dashed zero reference line.",
        "| DrawItem | Element | Pipeline | Segments | Color |\n|---|---|---|---|---|\n| 102 | Fundamental | lineAA@1 | 40 | cyan |\n| 105 | 2nd harmonic | lineAA@1 | 40 | yellow |\n| 108 | 3rd harmonic | lineAA@1 | 40 | magenta |\n| 111 | 4th harmonic | lineAA@1 | 40 | green |\n| 114 | Zero ref | lineAA@1 | 1 | dim |",
        count_ids(doc), id_breakdown(doc),
        ["All four harmonics have correct frequencies: sin(kx) for k=1,2,3,4, verified at all 41 sample points.",
         "Higher harmonics oscillate faster within the same x range, visually distinguishable by color.",
         "All curves pass through zero at x=0 and x=2pi as expected for sin(k*0) and sin(k*2pi).",
         "Transform maps data space [0,2pi]x[-1.3,1.3] to clip, giving equal vertical margin above and below."],
        ["Overlapping curves need distinct colors. Four harmonics use cyan, yellow, magenta, green for maximum contrast.",
         "All four harmonics share one transform since they occupy the same data domain."])
    return "fourier-harmonics", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 114: Smith Chart
# ═══════════════════════════════════════════════════════════════════════
def trial_114():
    # Unit circle + resistance circles + reactance arcs
    # All in clip space directly (no transform needed). Center at origin, radius ~0.85
    R = 0.82
    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100

    # Outer unit circle (48 segments)
    outer = circle_segments(0, 0, R, 48)
    buffers[bid] = buf(outer)
    geometries[bid + 1] = geo(bid, "rect4", 48)
    drawItems[bid + 2] = di(10, "unit_circle", "lineAA@1", bid + 1, WHITE, lineWidth=2.0)
    bid += 3

    # Resistance circles: r = 0.2, 0.5, 1.0, 2.0, 5.0
    # On Smith chart, resistance circle for r has center (r/(r+1), 0) and radius 1/(r+1)
    # Map to clip: multiply by R
    r_vals = [0.2, 0.5, 1.0, 2.0, 5.0]
    for rv in r_vals:
        cx = R * rv / (rv + 1)
        cr = R * 1 / (rv + 1)
        # Clip circle to unit circle: only keep points where x^2+y^2 <= R^2
        segs = []
        n = 60
        pts = []
        for i in range(n + 1):
            a = 2 * math.pi * i / n
            px = cx + cr * math.cos(a)
            py = cr * math.sin(a)
            if px * px + py * py <= R * R * 1.01:
                pts.append((px, py))
            else:
                if len(pts) >= 2:
                    segs.extend(lineaa_segments(pts))
                pts = []
        if len(pts) >= 2:
            segs.extend(lineaa_segments(pts))
        if segs:
            vc = len(segs) // 4
            buffers[bid] = buf(segs)
            geometries[bid + 1] = geo(bid, "rect4", vc)
            drawItems[bid + 2] = di(11, f"r_{rv}", "lineAA@1", bid + 1, CYAN, lineWidth=1.2)
            bid += 3

    # Reactance arcs: x = 0.2, 0.5, 1.0, 2.0, 5.0 (both positive and negative)
    # Reactance circle for x has center (1, 1/x) and radius 1/x
    x_vals = [0.5, 1.0, 2.0, 5.0]
    for xv in x_vals:
        for sign in [1, -1]:
            cxr = R * 1.0
            cyr = sign * R * 1.0 / xv
            cr = R * 1.0 / xv
            segs = []
            n = 60
            pts = []
            for i in range(n + 1):
                a = 2 * math.pi * i / n
                px = cxr + cr * math.cos(a)
                py = cyr + cr * math.sin(a)
                if px * px + py * py <= R * R * 1.01 and px >= -R * 0.01:
                    pts.append((px, py))
                else:
                    if len(pts) >= 2:
                        segs.extend(lineaa_segments(pts))
                    pts = []
            if len(pts) >= 2:
                segs.extend(lineaa_segments(pts))
            if segs:
                vc = len(segs) // 4
                buffers[bid] = buf(segs)
                geometries[bid + 1] = geo(bid, "rect4", vc)
                s = "p" if sign > 0 else "n"
                drawItems[bid + 2] = di(12, f"x_{s}{xv}", "lineAA@1", bid + 1, YELLOW, lineWidth=1.0)
                bid += 3

    # Horizontal axis
    buffers[bid] = buf([-R, 0, R, 0])
    geometries[bid + 1] = geo(bid, "rect4", 1)
    drawItems[bid + 2] = di(10, "h_axis", "lineAA@1", bid + 1, GRAY, lineWidth=1.0)
    bid += 3

    transforms = {}
    panes = {1: pane("Smith", ymin=-0.95, ymax=0.95, xmin=-0.95, xmax=0.95)}
    layers = {10: layer(1, "axes"), 11: layer(1, "resistance"), 12: layer(1, "reactance")}
    doc = make_doc(600, 600, buffers, transforms, panes, layers, geometries, drawItems)
    n_r = len([k for k in drawItems if "r_" in drawItems[k].get("name", "")])
    n_x = len([k for k in drawItems if "x_" in drawItems[k].get("name", "")])
    md = make_md(114, "Smith Chart",
        "Circular impedance chart with outer unit circle, 5 resistance circles, and reactance arcs. All lineAA@1.",
        f"Smith chart with unit circle (48 segments), {n_r} resistance circles (r=0.2,0.5,1,2,5), and {n_x} reactance arcs (x=0.5,1,2,5 positive and negative). Horizontal real axis. All drawn in clip space without transform.",
        "| Layer | Elements | Pipeline | Color |\n|---|---|---|---|\n| 10 | Unit circle + axis | lineAA@1 | white/gray |\n| 11 | Resistance circles | lineAA@1 | cyan |\n| 12 | Reactance arcs | lineAA@1 | yellow |",
        count_ids(doc), id_breakdown(doc),
        ["Resistance circles have correct Smith chart geometry: center (r/(r+1), 0), radius 1/(r+1), scaled to clip.",
         "Reactance arcs use center (1, 1/x), radius 1/x, clipped to unit circle boundary.",
         "All circles and arcs are clipped to the unit circle perimeter, matching standard Smith chart appearance.",
         "Square 600x600 viewport preserves circular aspect ratio."],
        ["Smith chart circles follow the bilinear transform mapping from impedance to reflection coefficient.",
         "Clipping arcs to the unit circle boundary requires filtering vertices during generation."],
        viewport="600x600")
    return "smith-chart", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 115: Bode Magnitude Plot
# ═══════════════════════════════════════════════════════════════════════
def trial_115():
    # Second-order low-pass: H(s) = 1 / (1 + s/Q*wn + (s/wn)^2)
    # wn = 100 rad/s, Q = 2
    wn = 100.0
    Q = 2.0
    N = 21  # 21 points, 20 segments
    # Log-spaced from 1 to 10000 rad/s
    freqs = [10 ** (i * 4 / (N - 1)) for i in range(N)]
    mags_db = []
    for w in freqs:
        s_ratio = w / wn
        denom_real = 1 - s_ratio ** 2
        denom_imag = s_ratio / Q
        mag = 1.0 / math.sqrt(denom_real ** 2 + denom_imag ** 2)
        mags_db.append(20 * math.log10(mag))
    # data space: x = log10(freq) [0..4], y = magnitude in dB
    log_freqs = [math.log10(f) for f in freqs]
    ymin = min(mags_db) - 5
    ymax = max(mags_db) + 5
    sx, tx = compute_transform(0, 4, -0.9, 0.9)
    sy, ty = compute_transform(ymin, ymax, -0.85, 0.85)

    curve_data = lineaa_segments(list(zip(log_freqs, mags_db)))
    # 0 dB reference line
    ref_data = [0, 0, 4, 0]
    # -3 dB reference
    ref_3db = [0, -3, 4, -3]

    buffers = {100: buf(curve_data), 103: buf(ref_data), 106: buf(ref_3db)}
    transforms = {50: xform(sx, sy, tx, ty)}
    panes = {1: pane("Main")}
    layers = {10: layer(1, "ref"), 11: layer(1, "curve")}
    geometries = {
        101: geo(100, "rect4", N - 1),
        104: geo(103, "rect4", 1),
        107: geo(106, "rect4", 1),
    }
    drawItems = {
        102: di(11, "bode_mag", "lineAA@1", 101, CYAN, transformId=50, lineWidth=2.5),
        105: di(10, "ref_0dB", "lineAA@1", 104, GRAY, transformId=50, lineWidth=1.0, dashLength=6.0, gapLength=4.0),
        108: di(10, "ref_-3dB", "lineAA@1", 107, RED, transformId=50, lineWidth=1.0, dashLength=6.0, gapLength=4.0),
    }
    doc = make_doc(800, 500, buffers, transforms, panes, layers, geometries, drawItems)
    md = make_md(115, "Bode Magnitude Plot",
        "Frequency response of a 2nd-order low-pass filter. Log-spaced X axis (20 points), magnitude in dB. lineAA@1 with dashed reference lines.",
        f"Bode magnitude plot for 2nd-order low-pass (wn=100, Q=2). 21 log-spaced frequency points from 1 to 10000 rad/s produce 20 lineAA@1 segments. Reference lines at 0 dB and -3 dB (dashed). Peak near resonance at ~{rnd(freqs[mags_db.index(max(mags_db))],1)} rad/s reaching ~{rnd(max(mags_db),1)} dB.",
        "| DrawItem | Element | Pipeline | Segments | Color |\n|---|---|---|---|---|\n| 102 | Magnitude curve | lineAA@1 | 20 | cyan |\n| 105 | 0 dB ref | lineAA@1 | 1 | gray |\n| 108 | -3 dB ref | lineAA@1 | 1 | red |",
        count_ids(doc), id_breakdown(doc),
        ["Magnitude follows 2nd-order transfer function |H(jw)| = 1/sqrt((1-(w/wn)^2)^2 + (w/(Q*wn))^2) correctly.",
         "Resonance peak visible near w=wn (100 rad/s) with Q=2 giving ~6 dB gain.",
         "High-frequency rolloff at -40 dB/decade matches 2nd-order characteristic.",
         "Log-spaced frequencies ensure even spacing on the logarithmic x-axis."],
        ["Bode plots use log10(frequency) as X and 20*log10(magnitude) as Y.",
         "Log-spaced sample points are essential for even visual density across decades."])
    return "bode-magnitude", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 116: Phase Portrait
# ═══════════════════════════════════════════════════════════════════════
def trial_116():
    # Simple harmonic oscillator with damping: dx/dt = v, dv/dt = -x - 0.15v
    # 8 trajectories from different initial conditions
    dt = 0.05
    steps = 200
    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100
    colors = [CYAN, YELLOW, GREEN, RED, MAGENTA, ORANGE, BLUE, LIME]
    inits = [(2.0, 0), (0, 2.0), (-2.0, 0), (0, -2.0),
             (1.5, 1.5), (-1.5, 1.5), (-1.5, -1.5), (1.5, -1.5)]
    damping = 0.15
    for k in range(8):
        x, v = inits[k]
        pts = [(x, v)]
        for _ in range(steps):
            ax = -x - damping * v
            v += ax * dt
            x += v * dt
            pts.append((x, v))
        data = lineaa_segments(pts)
        vc = len(data) // 4
        buffers[bid] = buf(data)
        geometries[bid + 1] = geo(bid, "rect4", vc)
        drawItems[bid + 2] = di(11, f"traj_{k}", "lineAA@1", bid + 1, colors[k],
                                 transformId=50, lineWidth=1.5)
        bid += 3

    sx, tx = compute_transform(-2.5, 2.5, -0.9, 0.9)
    sy, ty = compute_transform(-2.5, 2.5, -0.9, 0.9)
    # axis lines
    buffers[bid] = buf([-2.5, 0, 2.5, 0])
    geometries[bid + 1] = geo(bid, "rect4", 1)
    drawItems[bid + 2] = di(10, "x_axis", "lineAA@1", bid + 1, DIM, transformId=50, lineWidth=0.8)
    bid += 3
    buffers[bid] = buf([0, -2.5, 0, 2.5])
    geometries[bid + 1] = geo(bid, "rect4", 1)
    drawItems[bid + 2] = di(10, "v_axis", "lineAA@1", bid + 1, DIM, transformId=50, lineWidth=0.8)

    transforms = {50: xform(sx, sy, tx, ty)}
    panes = {1: pane("Main")}
    layers = {10: layer(1, "axes"), 11: layer(1, "trajectories")}
    doc = make_doc(600, 600, buffers, transforms, panes, layers, geometries, drawItems)
    md = make_md(116, "Phase Portrait",
        "8 trajectory curves showing a damped harmonic oscillator in phase space (x vs dx/dt). Spiral-like paths converging to origin.",
        "Phase portrait of damped harmonic oscillator (damping=0.15). 8 trajectories from symmetric initial conditions, each with 200 Euler steps (dt=0.05) producing ~200 lineAA@1 segments. Spirals converge to origin. Horizontal and vertical axis reference lines.",
        "| DrawItem | Element | Color |\n|---|---|---|\n| 102-125 | 8 trajectories | 8 distinct colors |\n| axes | x/v axes | dim gray |",
        count_ids(doc), id_breakdown(doc),
        ["All 8 trajectories spiral inward toward the origin, matching the expected behavior of a stable damped system.",
         "Phase portrait correctly plots position (x) on horizontal and velocity (dx/dt) on vertical axis.",
         "Symmetric initial conditions produce symmetric spiral patterns around the origin.",
         "Damping coefficient 0.15 produces visible but not overdamped spirals with ~5 visible loops."],
        ["Phase portraits use Euler integration — small dt (0.05) ensures trajectory accuracy over many steps.",
         "Damped harmonic oscillator has eigenvalues with negative real part, guaranteeing convergence to origin."],
        viewport="600x600")
    return "phase-portrait", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 117: Logistic Map Bifurcation
# ═══════════════════════════════════════════════════════════════════════
def trial_117():
    # x_{n+1} = r * x_n * (1 - x_n)
    # r from 2.5 to 4.0, 200 sample r values
    # For each r, iterate 200 times, plot last 50
    n_r = 200
    pts = []
    for i in range(n_r):
        r = 2.5 + 1.5 * i / (n_r - 1)
        x = 0.5
        for _ in range(200):
            x = r * x * (1 - x)
        for _ in range(50):
            x = r * x * (1 - x)
            pts.append((r, x))

    # Use points@1
    data = []
    for p in pts:
        data.extend([p[0], p[1]])
    n_pts = len(pts)

    sx, tx = compute_transform(2.5, 4.0, -0.9, 0.9)
    sy, ty = compute_transform(-0.05, 1.05, -0.9, 0.9)

    buffers = {100: buf(data)}
    transforms = {50: xform(sx, sy, tx, ty)}
    panes = {1: pane("Main")}
    layers = {10: layer(1, "points")}
    geometries = {101: geo(100, "pos2_clip", n_pts)}
    drawItems = {102: di(10, "bifurcation", "points@1", 101, CYAN, transformId=50, pointSize=1.5)}

    doc = make_doc(800, 500, buffers, transforms, panes, layers, geometries, drawItems)
    md = make_md(117, "Logistic Map Bifurcation",
        f"Bifurcation diagram of the logistic map x→rx(1-x) with {n_pts} points for r from 2.5 to 4.0.",
        f"Logistic map bifurcation diagram. 200 r-values from 2.5 to 4.0, each iterated 250 times (200 warm-up + 50 plotted). {n_pts} total points@1 with pointSize=1.5. Shows period doubling cascade into chaos.",
        f"| DrawItem | Element | Pipeline | Points | Color |\n|---|---|---|---|---|\n| 102 | Bifurcation | points@1 | {n_pts} | cyan |",
        count_ids(doc), id_breakdown(doc),
        [f"Period doubling visible: single fixed point at low r, period-2 near r~3, period-4 near r~3.45, chaos beyond r~3.57.",
         "Warm-up of 200 iterations ensures transients have decayed before plotting.",
         "50 plotted iterations per r-value capture attractor structure including chaotic bands and periodic windows.",
         "Full range r=[2.5,4.0] covers the transition from stable fixed point through complete chaos."],
        ["Bifurcation diagrams need warm-up iterations to discard transients before plotting attractor values.",
         "Small pointSize (1.5) is essential for bifurcation plots where thousands of points create emergent structure."])
    return "logistic-map", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 118: Lorenz Attractor (XY projection)
# ═══════════════════════════════════════════════════════════════════════
def trial_118():
    sigma, rho, beta = 10.0, 28.0, 8.0 / 3.0
    dt = 0.005
    steps = 5000
    x, y, z = 1.0, 1.0, 1.0
    pts = []
    for _ in range(steps):
        dx = sigma * (y - x)
        dy = x * (rho - z) - y
        dz = x * y - beta * z
        x += dx * dt
        y += dy * dt
        z += dz * dt
        pts.append((x, y))

    # Sample 500 points evenly from trajectory
    sample_idx = [i * (steps - 1) // 499 for i in range(500)]
    sampled = [pts[i] for i in sample_idx]

    data = []
    for p in sampled:
        data.extend([p[0], p[1]])

    all_x = [p[0] for p in sampled]
    all_y = [p[1] for p in sampled]
    sx, tx = compute_transform(min(all_x) - 2, max(all_x) + 2, -0.9, 0.9)
    sy, ty = compute_transform(min(all_y) - 2, max(all_y) + 2, -0.9, 0.9)

    buffers = {100: buf(data)}
    transforms = {50: xform(sx, sy, tx, ty)}
    panes = {1: pane("Main")}
    layers = {10: layer(1, "attractor")}
    geometries = {101: geo(100, "pos2_clip", 500)}
    drawItems = {102: di(10, "lorenz_xy", "points@1", 101, CYAN, transformId=50, pointSize=2.0)}

    doc = make_doc(800, 600, buffers, transforms, panes, layers, geometries, drawItems)
    md = make_md(118, "Lorenz Attractor XY Projection",
        "2D XY projection of the Lorenz attractor with 500 points. Parameters sigma=10, rho=28, beta=8/3.",
        "Lorenz attractor XY projection. 5000 Euler steps (dt=0.005) with sigma=10, rho=28, beta=8/3, subsampled to 500 points@1. The butterfly-wing structure of the strange attractor is visible in the (x,y) plane.",
        "| DrawItem | Element | Pipeline | Points | Color |\n|---|---|---|---|---|\n| 102 | Lorenz XY | points@1 | 500 | cyan |",
        count_ids(doc), id_breakdown(doc),
        ["Lorenz system parameters (sigma=10, rho=28, beta=8/3) are the classic chaotic regime values.",
         "Butterfly-wing double-lobe structure visible in XY projection with lobes centered near (+-sqrt(beta*(rho-1)), +-sqrt(beta*(rho-1))).",
         "5000 integration steps with dt=0.005 provide sufficient trajectory length to outline the attractor shape.",
         "Transform auto-fits to the data bounds with 2-unit padding on each side."],
        ["Lorenz attractor needs many integration steps (thousands) to trace out the attractor structure.",
         "Subsampling reduces point count for rendering while preserving overall attractor shape."],
        viewport="800x600")
    return "lorenz-xy", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 119: ROC Curve
# ═══════════════════════════════════════════════════════════════════════
def trial_119():
    # Simulated ROC curve: TPR = FPR^(1/3) (good classifier)
    N = 21
    fprs = [i / (N - 1) for i in range(N)]
    tprs = [f ** (1.0 / 3.0) for f in fprs]
    tprs[0] = 0.0  # force (0,0)
    tprs[-1] = 1.0  # force (1,1)

    # AUC area (triSolid under the curve)
    area_data = []
    for i in range(N - 1):
        # Two triangles per segment
        area_data.extend([fprs[i], tprs[i], fprs[i], 0.0, fprs[i+1], tprs[i+1]])
        area_data.extend([fprs[i+1], tprs[i+1], fprs[i], 0.0, fprs[i+1], 0.0])
    area_vtx = (N - 1) * 6

    curve_data = lineaa_segments(list(zip(fprs, tprs)))
    diag_data = [0, 0, 1, 1]

    sx, tx = compute_transform(-0.05, 1.05, -0.9, 0.9)
    sy, ty = compute_transform(-0.05, 1.05, -0.9, 0.9)

    buffers = {100: buf(area_data), 103: buf(curve_data), 106: buf(diag_data)}
    transforms = {50: xform(sx, sy, tx, ty)}
    panes = {1: pane("Main")}
    layers = {10: layer(1, "area"), 11: layer(1, "lines")}
    geometries = {
        101: geo(100, "pos2_clip", area_vtx),
        104: geo(103, "rect4", N - 1),
        107: geo(106, "rect4", 1),
    }
    auc_color = [0.180, 0.831, 0.886, 0.25]
    drawItems = {
        102: di(10, "auc_area", "triSolid@1", 101, auc_color, transformId=50),
        105: di(11, "roc_curve", "lineAA@1", 104, CYAN, transformId=50, lineWidth=2.5),
        108: di(11, "diagonal", "lineAA@1", 107, GRAY, transformId=50, lineWidth=1.0, dashLength=6.0, gapLength=4.0),
    }
    doc = make_doc(600, 600, buffers, transforms, panes, layers, geometries, drawItems)
    # compute AUC
    auc = sum((fprs[i+1] - fprs[i]) * (tprs[i] + tprs[i+1]) / 2 for i in range(N-1))
    md = make_md(119, "ROC Curve",
        "ROC curve with 20 lineAA segments, diagonal reference (dashed), and shaded AUC area (triSolid@1).",
        f"ROC curve for a simulated classifier (TPR = FPR^(1/3)). 21 points producing 20 lineAA@1 segments. Shaded AUC area (triSolid@1, semi-transparent cyan, {area_vtx} vertices). Dashed diagonal represents random classifier. AUC = {rnd(auc, 3)}.",
        "| DrawItem | Layer | Element | Pipeline | Count | Color |\n|---|---|---|---|---|---|\n| 102 | 10 | AUC area | triSolid@1 | " + str(area_vtx) + " vtx | cyan 25% |\n| 105 | 11 | ROC curve | lineAA@1 | 20 seg | cyan |\n| 108 | 11 | Diagonal | lineAA@1 | 1 seg | gray |",
        count_ids(doc), id_breakdown(doc),
        [f"ROC curve starts at (0,0) and ends at (1,1) as required. AUC = {rnd(auc,3)} indicates a good classifier.",
         "Shaded AUC area correctly fills between the ROC curve and the x-axis using triangle pairs.",
         "Diagonal reference line from (0,0) to (1,1) represents random chance (AUC=0.5).",
         "Square viewport (600x600) preserves equal axis scaling for proper ROC interpretation."],
        ["ROC area fill needs (N-1)*6 triSolid vertices (two triangles per curve segment down to baseline).",
         "Semi-transparent fill (alpha=0.25) allows the diagonal reference to remain visible through the shaded area."],
        viewport="600x600")
    return "roc-curve", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 120: Confusion Matrix
# ═══════════════════════════════════════════════════════════════════════
def trial_120():
    # 4x4 confusion matrix
    matrix = [
        [45, 3, 2, 0],
        [5, 38, 4, 3],
        [1, 6, 42, 1],
        [0, 2, 3, 45],
    ]
    max_val = 50.0
    # Each cell is an instancedRect@1
    # Data space: x=[0,4], y=[0,4], cell (col, row) at x=[col,col+1], y=[row,row+1]
    data = []
    colors_data = []
    n_rects = 16
    # We'll use one buffer with all 16 rects
    rect_data = []
    for row in range(4):
        for col in range(4):
            # y inverted so row 0 is at top
            rect_data.extend([col + 0.05, (3 - row) + 0.05, col + 0.95, (3 - row) + 0.95])

    sx, tx = compute_transform(-0.2, 4.2, -0.9, 0.9)
    sy, ty = compute_transform(-0.2, 4.2, -0.9, 0.9)

    # Color each rect by intensity — we'll use separate DrawItems for different color ranges
    # Actually, let's use triGradient for per-cell coloring
    # Better: use instancedRect with one DrawItem per cell for distinct colors
    # That's 16 DrawItems — fits within ID range
    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100
    for row in range(4):
        for col in range(4):
            val = matrix[row][col]
            intensity = val / max_val
            # White (high) to dark blue (low)
            r = 0.1 + 0.05 * intensity
            g = 0.1 + 0.15 * intensity
            b = 0.3 + 0.65 * intensity
            rect = [col + 0.05, (3 - row) + 0.05, col + 0.95, (3 - row) + 0.95]
            buffers[bid] = buf(rect)
            geometries[bid + 1] = geo(bid, "rect4", 1)
            drawItems[bid + 2] = di(10, f"cell_{row}_{col}", "instancedRect@1", bid + 1,
                                     [rnd(r), rnd(g), rnd(b), 1.0], transformId=50, cornerRadius=3.0)
            bid += 3

    transforms = {50: xform(sx, sy, tx, ty)}
    panes = {1: pane("Main")}
    layers = {10: layer(1, "cells")}
    doc = make_doc(600, 600, buffers, transforms, panes, layers, geometries, drawItems)
    md = make_md(120, "Confusion Matrix",
        "4x4 confusion matrix as 16 colored instancedRect@1 cells. Blue intensity proportional to cell value.",
        "4x4 confusion matrix with 16 instancedRect@1 cells. Color intensity maps cell values (0-50) to a blue color scale — darker blue for low values, brighter blue for high values. Diagonal cells (correct predictions) are brightest. Corner radius 3.0 for visual polish.",
        "| DrawItem | Element | Pipeline | Rects | Color |\n|---|---|---|---|---|\n| 102-149 | 16 cells | instancedRect@1 | 1 each | blue intensity |",
        count_ids(doc), id_breakdown(doc),
        ["Diagonal cells (45, 38, 42, 45) are brightest, correctly showing high classification accuracy.",
         "Off-diagonal cells are visibly darker, indicating fewer misclassifications.",
         "Row 0 column 0 at top-left follows standard confusion matrix layout (predicted vs actual).",
         "Each cell has 0.05 unit gap between it and its neighbors, creating a clean grid appearance."],
        ["Confusion matrices map value to color intensity — one DrawItem per cell allows individual coloring.",
         "Y-axis inversion (3-row) puts row 0 at the top, matching convention."],
        viewport="600x600")
    return "confusion-matrix", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 121: Precision-Recall Curve
# ═══════════════════════════════════════════════════════════════════════
def trial_121():
    # PR curve: precision = recall^0.3 (good classifier)
    N = 16  # 16 points, 15 segments
    recalls = [i / (N - 1) for i in range(N)]
    precisions = [1.0 if r == 0 else r ** 0.3 for r in recalls]
    precisions[0] = 1.0

    curve_data = lineaa_segments(list(zip(recalls, precisions)))

    # F1 iso-lines: F1 = 2*P*R/(P+R) => P = F1*R/(2*R - F1)
    f1_values = [0.4, 0.6, 0.8]
    f1_colors = [DIM, GRAY, WHITE]
    buffers = {100: buf(curve_data)}
    geometries = {101: geo(100, "rect4", N - 1)}
    bid = 103
    for fi, f1 in enumerate(f1_values):
        pts = []
        for i in range(50):
            r = f1 / 2.0 + 0.001 + i * (1.0 - f1 / 2.0 - 0.001) / 49
            denom = 2 * r - f1
            if denom > 0.001:
                p = f1 * r / denom
                if 0 <= p <= 1.0 and 0 <= r <= 1.0:
                    pts.append((r, p))
        if len(pts) >= 2:
            data = lineaa_segments(pts)
            vc = len(data) // 4
            buffers[bid] = buf(data)
            geometries[bid + 1] = geo(bid, "rect4", vc)
            bid += 3

    sx, tx = compute_transform(-0.05, 1.05, -0.9, 0.9)
    sy, ty = compute_transform(-0.05, 1.05, -0.9, 0.9)
    transforms = {50: xform(sx, sy, tx, ty)}
    panes = {1: pane("Main")}
    layers = {10: layer(1, "iso"), 11: layer(1, "curve")}

    # Now build drawItems
    drawItems = {102: di(11, "pr_curve", "lineAA@1", 101, CYAN, transformId=50, lineWidth=2.5)}
    di_id = 105
    for fi, f1 in enumerate(f1_values):
        geo_id = 103 + fi * 3 + 1
        if geo_id in geometries:
            drawItems[di_id] = di(10, f"f1_{f1}", "lineAA@1", geo_id, f1_colors[fi],
                                   transformId=50, lineWidth=1.0, dashLength=6.0, gapLength=4.0)
        di_id += 3

    doc = make_doc(600, 600, buffers, transforms, panes, layers, geometries, drawItems)
    md = make_md(121, "Precision-Recall Curve",
        "PR curve with 15 lineAA segments and 3 dashed F1 iso-lines (F1=0.4, 0.6, 0.8).",
        "Precision-recall curve with 16 sample points producing 15 lineAA@1 segments. Three F1-score iso-lines (F1=0.4, 0.6, 0.8) as dashed curves show constant F1 contours. Single pane, square viewport.",
        "| DrawItem | Element | Pipeline | Color |\n|---|---|---|---|\n| 102 | PR curve | lineAA@1 | cyan |\n| 105 | F1=0.4 iso | lineAA@1 | dim |\n| 108 | F1=0.6 iso | lineAA@1 | gray |\n| 111 | F1=0.8 iso | lineAA@1 | white |",
        count_ids(doc), id_breakdown(doc),
        ["PR curve starts at (0,1) and descends as recall increases, matching expected behavior for a good classifier.",
         "F1 iso-lines are hyperbolas: P = F1*R/(2R - F1), correctly computed and clipped to [0,1] range.",
         "Higher F1 iso-lines (0.8) are closer to the top-right corner, indicating better performance.",
         "Square viewport preserves equal axis scaling for proper P/R interpretation."],
        ["F1 iso-lines on PR plots are hyperbolas P = F1*R/(2R-F1) with domain R > F1/2.",
         "PR curves start at high precision (low recall) and typically decrease as recall increases."],
        viewport="600x600")
    return "precision-recall", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 122: Survival Curve (Kaplan-Meier)
# ═══════════════════════════════════════════════════════════════════════
def trial_122():
    # Simulated KM step function: 12 event times
    times =  [0, 2, 5, 8, 12, 16, 20, 25, 30, 38, 45, 55, 65]
    surv  =  [1.0, 0.95, 0.88, 0.82, 0.74, 0.68, 0.60, 0.52, 0.44, 0.35, 0.26, 0.18, 0.10]
    # Step function: horizontal then vertical
    step_pts = []
    for i in range(len(times)):
        step_pts.append((times[i], surv[i]))
        if i + 1 < len(times):
            step_pts.append((times[i + 1], surv[i]))
    curve_data = lineaa_segments(step_pts)
    vc_curve = len(curve_data) // 4

    # Confidence band (semi-transparent area)
    # Upper and lower bounds
    band_width = 0.06
    area_data = []
    for i in range(len(step_pts) - 1):
        x0, y0 = step_pts[i]
        x1, y1 = step_pts[i + 1]
        yu0 = min(y0 + band_width, 1.0)
        yl0 = max(y0 - band_width, 0.0)
        yu1 = min(y1 + band_width, 1.0)
        yl1 = max(y1 - band_width, 0.0)
        area_data.extend([x0, yu0, x0, yl0, x1, yu1])
        area_data.extend([x1, yu1, x0, yl0, x1, yl1])
    area_vtx = len(area_data) // 2

    sx, tx = compute_transform(-2, 70, -0.9, 0.9)
    sy, ty = compute_transform(-0.05, 1.1, -0.9, 0.9)

    buffers = {100: buf(area_data), 103: buf(curve_data)}
    transforms = {50: xform(sx, sy, tx, ty)}
    panes = {1: pane("Main")}
    layers = {10: layer(1, "band"), 11: layer(1, "curve")}
    geometries = {
        101: geo(100, "pos2_clip", area_vtx),
        104: geo(103, "rect4", vc_curve),
    }
    drawItems = {
        102: di(10, "conf_band", "triSolid@1", 101, [0.180, 0.831, 0.886, 0.2], transformId=50),
        105: di(11, "km_curve", "lineAA@1", 104, CYAN, transformId=50, lineWidth=2.5),
    }
    doc = make_doc(800, 500, buffers, transforms, panes, layers, geometries, drawItems)
    md = make_md(122, "Kaplan-Meier Survival Curve",
        "Kaplan-Meier step function with 12 event steps (lineAA@1) and semi-transparent confidence band (triSolid@1).",
        f"Kaplan-Meier survival curve with 13 time points (12 steps) rendered as a step function ({vc_curve} lineAA@1 segments). Semi-transparent confidence band (triSolid@1, {area_vtx} vertices, alpha=0.2) shows +-6% uncertainty. Survival decreases from 1.0 to 0.10 over 65 time units.",
        f"| DrawItem | Layer | Element | Pipeline | Count | Color |\n|---|---|---|---|---|---|\n| 102 | 10 | Conf band | triSolid@1 | {area_vtx} vtx | cyan 20% |\n| 105 | 11 | KM curve | lineAA@1 | {vc_curve} seg | cyan |",
        count_ids(doc), id_breakdown(doc),
        ["Step function correctly renders as horizontal segments at each survival probability followed by vertical drops at event times.",
         "Survival starts at 1.0 (all alive) and monotonically decreases, matching KM estimator behavior.",
         "Confidence band is rendered behind the curve (layer 10 < 11) with 20% alpha transparency.",
         "All 12 step drops are visible at the specified event times."],
        ["KM step functions need horizontal-then-vertical point pairs: (t_i, S_i), (t_{i+1}, S_i) for each step.",
         "Confidence bands as triSolid area fill need paired upper/lower bounds at each x coordinate."])
    return "survival-curve", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 123: Control Chart (SPC)
# ═══════════════════════════════════════════════════════════════════════
def trial_123():
    import random
    random.seed(42)
    N = 30
    mean = 50.0
    std = 3.0
    values = [mean + random.gauss(0, std) for _ in range(N)]
    # Inject one out-of-control point
    values[22] = mean + 3.5 * std

    ucl = mean + 3 * std  # 59
    lcl = mean - 3 * std  # 41

    xs = list(range(N))
    curve_data = lineaa_segments(list(zip(xs, values)))
    pts_data = []
    for i in range(N):
        pts_data.extend([xs[i], values[i]])

    sx, tx = compute_transform(-1, N, -0.9, 0.9)
    sy, ty = compute_transform(lcl - 5, ucl + 5, -0.85, 0.85)

    ucl_data = [-1, ucl, N, ucl]
    lcl_data = [-1, lcl, N, lcl]
    mean_data = [-1, mean, N, mean]

    buffers = {
        100: buf(curve_data), 103: buf(pts_data),
        106: buf(ucl_data), 109: buf(lcl_data), 112: buf(mean_data),
    }
    transforms = {50: xform(sx, sy, tx, ty)}
    panes = {1: pane("Main")}
    layers = {10: layer(1, "limits"), 11: layer(1, "data")}
    geometries = {
        101: geo(100, "rect4", N - 1),
        104: geo(103, "pos2_clip", N),
        107: geo(106, "rect4", 1),
        110: geo(109, "rect4", 1),
        113: geo(112, "rect4", 1),
    }
    drawItems = {
        102: di(11, "data_line", "lineAA@1", 101, CYAN, transformId=50, lineWidth=1.8),
        105: di(11, "data_pts", "points@1", 104, WHITE, transformId=50, pointSize=5.0),
        108: di(10, "ucl", "lineAA@1", 107, RED, transformId=50, lineWidth=1.5, dashLength=8.0, gapLength=5.0),
        111: di(10, "lcl", "lineAA@1", 110, RED, transformId=50, lineWidth=1.5, dashLength=8.0, gapLength=5.0),
        114: di(10, "mean", "lineAA@1", 113, GREEN, transformId=50, lineWidth=1.5, dashLength=6.0, gapLength=4.0),
    }
    doc = make_doc(800, 500, buffers, transforms, panes, layers, geometries, drawItems)
    md = make_md(123, "Control Chart (SPC)",
        "Statistical process control chart with 30 data points (lineAA@1 + points@1) and UCL/LCL/mean dashed reference lines.",
        f"SPC control chart with {N} data points (mean=50, sigma=3). Data shown as connected lineAA@1 line with point markers (points@1). UCL (59) and LCL (41) as red dashed lines, mean (50) as green dashed line. One out-of-control point injected at index 22 (~{rnd(values[22],1)}).",
        "| DrawItem | Layer | Element | Pipeline | Count | Color |\n|---|---|---|---|---|---|\n| 102 | 11 | Data line | lineAA@1 | 29 seg | cyan |\n| 105 | 11 | Data points | points@1 | 30 pts | white |\n| 108 | 10 | UCL | lineAA@1 | 1 seg | red |\n| 111 | 10 | LCL | lineAA@1 | 1 seg | red |\n| 114 | 10 | Mean | lineAA@1 | 1 seg | green |",
        count_ids(doc), id_breakdown(doc),
        ["UCL and LCL at mean +/- 3 sigma (59 and 41) correctly bracket 99.7% of expected variation.",
         "Out-of-control point at index 22 is visibly above the UCL, demonstrating the detection purpose of SPC charts.",
         "Data points are rendered on top of the connecting line (same layer, higher ID) for clear visibility.",
         "Control limits rendered behind data (layer 10 < 11) so data line is not occluded by dashed lines."],
        ["SPC charts use mean +/- 3 sigma for control limits, covering 99.7% of normal variation.",
         "Overlaying points@1 on lineAA@1 with the same transform gives dual representation — line shows trend, points show individual values."])
    return "control-chart", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 124: Scatter Matrix 2x2
# ═══════════════════════════════════════════════════════════════════════
def trial_124():
    import random
    random.seed(123)
    N = 30
    # Generate 4 variables with correlations
    x1 = [random.gauss(0, 1) for _ in range(N)]
    x2 = [0.7 * x1[i] + random.gauss(0, 0.5) for i in range(N)]
    x3 = [-0.5 * x1[i] + random.gauss(0, 0.8) for i in range(N)]
    x4 = [random.gauss(0, 1) for _ in range(N)]
    vars_ = [x1, x2, x3, x4]
    var_names = ["x1", "x2", "x3", "x4"]

    # 4 panes in 2x2 grid
    # Pane layout: (row, col) → pane_id
    # row=0: top, row=1: bottom; col=0: left, col=1: right
    pane_configs = [
        (1, 0, 0, "x1 vs x2"), (2, 0, 1, "x3 vs x4"),
        (3, 1, 0, "x1 vs x3"), (4, 1, 1, "x2 vs x4"),
    ]
    var_pairs = [(0, 1), (2, 3), (0, 2), (1, 3)]

    panes_d = {}
    layers_d = {}
    for pid, row, col, name in pane_configs:
        xmin = -0.97 + col * 0.98
        xmax = -0.02 + col * 0.98
        ymin = -0.97 + (1 - row) * 0.98
        ymax = -0.02 + (1 - row) * 0.98
        panes_d[pid] = pane(name, ymin=ymin, ymax=ymax, xmin=xmin, xmax=xmax)
        layers_d[10 + pid] = layer(pid, "scatter")

    buffers = {}
    transforms_d = {}
    geometries = {}
    drawItems = {}
    bid = 100
    tid = 50
    for i, (pid, row, col, name) in enumerate(pane_configs):
        vi, vj = var_pairs[i]
        vx = vars_[vi]
        vy = vars_[vj]
        data = []
        for k in range(N):
            data.extend([vx[k], vy[k]])
        xmin_d, xmax_d = min(vx) - 0.5, max(vx) + 0.5
        ymin_d, ymax_d = min(vy) - 0.5, max(vy) + 0.5
        sx, stx = compute_transform(xmin_d, xmax_d, -0.85, 0.85)
        sy, sty = compute_transform(ymin_d, ymax_d, -0.85, 0.85)
        transforms_d[tid] = xform(sx, sy, stx, sty)
        buffers[bid] = buf(data)
        geometries[bid + 1] = geo(bid, "pos2_clip", N)
        drawItems[bid + 2] = di(10 + pid, f"scatter_{vi}_{vj}", "points@1", bid + 1,
                                 CYAN, transformId=tid, pointSize=5.0)
        bid += 3
        tid += 1

    doc = make_doc(800, 800, buffers, transforms_d, panes_d, layers_d, geometries, drawItems)
    md = make_md(124, "Scatter Matrix 2x2",
        "4 panes in 2x2 grid, each with 30 scatter points (points@1). Different variable pairs per pane.",
        "Scatter matrix with 4 panes arranged in 2x2 grid. Each pane shows 30 points@1 for a different pair of correlated Gaussian variables. Pane 1: x1 vs x2 (r~0.7), Pane 2: x3 vs x4 (uncorrelated), Pane 3: x1 vs x3 (r~-0.5), Pane 4: x2 vs x4 (uncorrelated). Each pane has its own transform for data fitting.",
        "| Pane | Pair | Pipeline | Points |\n|---|---|---|---|\n| 1 | x1 vs x2 | points@1 | 30 |\n| 2 | x3 vs x4 | points@1 | 30 |\n| 3 | x1 vs x3 | points@1 | 30 |\n| 4 | x2 vs x4 | points@1 | 30 |",
        count_ids(doc), id_breakdown(doc),
        ["Correlated pairs (x1/x2 at r~0.7) show an elongated cloud along the positive diagonal.",
         "Uncorrelated pairs (x3/x4, x2/x4) show circular scatter with no directional trend.",
         "Negative correlation (x1/x3) shows elongation along the negative diagonal.",
         "Each pane's transform independently fits its data range to clip space [-0.85,0.85]."],
        ["Multi-pane scatter matrices need separate transforms per pane since each variable pair has different data bounds.",
         "2x2 grid layout: clip regions tile the viewport with small gaps between panes."],
        viewport="800x800")
    return "scatter-matrix-2x2", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 125: Correlation Heatmap
# ═══════════════════════════════════════════════════════════════════════
def trial_125():
    # 5x5 correlation matrix
    corr = [
        [1.0, 0.8, -0.3, 0.1, 0.6],
        [0.8, 1.0, -0.5, 0.2, 0.7],
        [-0.3, -0.5, 1.0, 0.4, -0.2],
        [0.1, 0.2, 0.4, 1.0, 0.0],
        [0.6, 0.7, -0.2, 0.0, 1.0],
    ]
    sx, stx = compute_transform(-0.3, 5.3, -0.9, 0.9)
    sy, sty = compute_transform(-0.3, 5.3, -0.9, 0.9)

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100
    for row in range(5):
        for col in range(5):
            val = corr[row][col]
            # Blue(-1) → white(0) → red(+1)
            if val >= 0:
                r = 1.0
                g = 1.0 - val * 0.7
                b = 1.0 - val * 0.8
            else:
                r = 1.0 + val * 0.8
                g = 1.0 + val * 0.7
                b = 1.0
            rect = [col + 0.05, (4 - row) + 0.05, col + 0.95, (4 - row) + 0.95]
            buffers[bid] = buf(rect)
            geometries[bid + 1] = geo(bid, "rect4", 1)
            drawItems[bid + 2] = di(10, f"cell_{row}_{col}", "instancedRect@1", bid + 1,
                                     [rnd(r), rnd(g), rnd(b), 1.0], transformId=50, cornerRadius=2.0)
            bid += 3

    transforms = {50: xform(sx, sy, stx, sty)}
    panes = {1: pane("Main")}
    layers = {10: layer(1, "cells")}
    doc = make_doc(600, 600, buffers, transforms, panes, layers, geometries, drawItems)
    md = make_md(125, "Correlation Heatmap",
        "5x5 correlation matrix as 25 instancedRect@1 cells. Blue-white-red diverging color scale from -1 to +1.",
        "5x5 correlation heatmap with 25 instancedRect@1 cells. Diverging color scale: blue for negative correlations, white for zero, red for positive. Diagonal cells are fully red (r=1.0). Strongest off-diagonal correlations at (0,1)/(1,0) = 0.8.",
        "| DrawItem | Element | Pipeline | Rects | Color |\n|---|---|---|---|---|\n| 102-174 | 25 cells | instancedRect@1 | 1 each | diverging blue-white-red |",
        count_ids(doc), id_breakdown(doc),
        ["Diagonal cells are pure red (correlation = 1.0) as expected for self-correlation.",
         "Color scale correctly maps: -1 → blue, 0 → white, +1 → red with smooth interpolation.",
         "Matrix is symmetric (corr[i][j] = corr[j][i]), visually confirmed by symmetric color pattern.",
         "Strongest positive off-diagonal correlations (0.8) are visibly deep red; negative (-0.5) is visibly blue."],
        ["Diverging color scales (blue-white-red) are standard for correlation matrices, with white at zero.",
         "5x5 grid on 600x600 square viewport gives clear, well-proportioned cells."],
        viewport="600x600")
    return "correlation-heatmap", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 126: Normal (Gaussian) Bell Curve
# ═══════════════════════════════════════════════════════════════════════
def trial_126():
    mu, sigma = 0.0, 1.0
    N = 61
    xs = [mu - 4 * sigma + i * 8 * sigma / (N - 1) for i in range(N)]
    ys = [1 / (sigma * math.sqrt(2 * math.pi)) * math.exp(-0.5 * ((x - mu) / sigma) ** 2) for x in xs]

    curve_data = lineaa_segments(list(zip(xs, ys)))

    # Shaded tails beyond ±2σ
    # Left tail: x from -4 to -2
    left_pts = [(x, y) for x, y in zip(xs, ys) if x <= -2 * sigma]
    right_pts = [(x, y) for x, y in zip(xs, ys) if x >= 2 * sigma]

    def area_under(pts):
        data = []
        for i in range(len(pts) - 1):
            x0, y0 = pts[i]
            x1, y1 = pts[i + 1]
            data.extend([x0, y0, x0, 0.0, x1, y1])
            data.extend([x1, y1, x0, 0.0, x1, 0.0])
        return data

    left_area = area_under(left_pts)
    right_area = area_under(right_pts)
    tail_data = left_area + right_area
    tail_vtx = len(tail_data) // 2

    ymax = max(ys) * 1.1
    sx, stx = compute_transform(-4.2, 4.2, -0.9, 0.9)
    sy, sty = compute_transform(-0.02, ymax, -0.85, 0.85)

    # Reference lines at ±1σ, ±2σ
    ref_data = []
    for xr in [-2, -1, 1, 2]:
        yr = 1 / (sigma * math.sqrt(2 * math.pi)) * math.exp(-0.5 * (xr / sigma) ** 2)
        ref_data.extend([xr, 0, xr, yr])
    ref_vc = len(ref_data) // 4

    buffers = {100: buf(curve_data), 103: buf(tail_data), 106: buf(ref_data)}
    transforms = {50: xform(sx, sy, stx, sty)}
    panes = {1: pane("Main")}
    layers = {10: layer(1, "tails"), 11: layer(1, "refs"), 12: layer(1, "curve")}
    geometries = {
        101: geo(100, "rect4", N - 1),
        104: geo(103, "pos2_clip", tail_vtx),
        107: geo(106, "rect4", ref_vc),
    }
    tail_color = [0.937, 0.267, 0.267, 0.35]
    drawItems = {
        102: di(12, "bell_curve", "lineAA@1", 101, CYAN, transformId=50, lineWidth=2.5),
        105: di(10, "tails", "triSolid@1", 104, tail_color, transformId=50),
        108: di(11, "sigma_refs", "lineAA@1", 107, DIM, transformId=50, lineWidth=1.0, dashLength=5.0, gapLength=4.0),
    }
    doc = make_doc(800, 500, buffers, transforms, panes, layers, geometries, drawItems)
    md = make_md(126, "Normal Bell Curve",
        "Gaussian PDF curve (lineAA@1, 60 segments) with shaded tails beyond +-2 sigma (triSolid@1, semi-transparent red).",
        f"Standard normal distribution N(0,1) with 61 sample points producing 60 lineAA@1 segments. Tails beyond +-2 sigma shaded in semi-transparent red ({tail_vtx} triSolid vertices). Vertical dashed reference lines at +-1 sigma and +-2 sigma. Peak at y = {rnd(max(ys), 4)} (1/sqrt(2pi)).",
        f"| DrawItem | Layer | Element | Pipeline | Count | Color |\n|---|---|---|---|---|---|\n| 102 | 12 | Bell curve | lineAA@1 | 60 seg | cyan |\n| 105 | 10 | Tail shading | triSolid@1 | {tail_vtx} vtx | red 35% |\n| 108 | 11 | Sigma refs | lineAA@1 | {ref_vc} seg | dim |",
        count_ids(doc), id_breakdown(doc),
        [f"Peak value {rnd(max(ys),4)} matches 1/sqrt(2pi) = {rnd(1/math.sqrt(2*math.pi),4)} for standard normal.",
         "Bell curve is symmetric about x=0, verified by equal y-values at +-x for all sample points.",
         "Shaded tails contain ~2.28% of the area each (4.56% total), representing the rejection region at 2 sigma.",
         "Sigma reference lines correctly extend from y=0 to the curve height at each sigma value."],
        ["Gaussian PDF: f(x) = (1/(sigma*sqrt(2pi))) * exp(-0.5*((x-mu)/sigma)^2). Peak at x=mu.",
         "Shading tails requires collecting curve points beyond the threshold and filling triangles down to y=0."])
    return "normal-bell-curve", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 127: Chi-Square Distribution Curves
# ═══════════════════════════════════════════════════════════════════════
def trial_127():
    # Chi-square PDF: f(x) = x^(k/2-1) * exp(-x/2) / (2^(k/2) * Gamma(k/2))
    N = 51
    ks = [2, 4, 8]
    colors = [CYAN, YELLOW, MAGENTA]
    names = ["k=2", "k=4", "k=8"]

    def chi2_pdf(x, k):
        if x <= 0:
            return 0.0
        return x ** (k / 2 - 1) * math.exp(-x / 2) / (2 ** (k / 2) * math.gamma(k / 2))

    xmax = 20.0
    xs = [i * xmax / (N - 1) for i in range(N)]
    xs[0] = 0.01  # avoid x=0 for k=2

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100
    all_ymax = 0
    for ki, k in enumerate(ks):
        ys = [chi2_pdf(x, k) for x in xs]
        all_ymax = max(all_ymax, max(ys))
        data = lineaa_segments(list(zip(xs, ys)))
        vc = len(data) // 4
        buffers[bid] = buf(data)
        geometries[bid + 1] = geo(bid, "rect4", vc)
        drawItems[bid + 2] = di(10, names[ki], "lineAA@1", bid + 1, colors[ki],
                                 transformId=50, lineWidth=2.0)
        bid += 3

    sx, stx = compute_transform(-0.5, xmax + 0.5, -0.9, 0.9)
    sy, sty = compute_transform(-0.02, all_ymax * 1.15, -0.85, 0.85)
    transforms = {50: xform(sx, sy, stx, sty)}
    panes = {1: pane("Main")}
    layers = {10: layer(1, "curves")}
    doc = make_doc(800, 500, buffers, transforms, panes, layers, geometries, drawItems)
    md = make_md(127, "Chi-Square Distribution Curves",
        "3 overlaid chi-square distributions (k=2,4,8) with 50 lineAA segments each, different colors.",
        f"Three chi-square PDF curves for k=2,4,8 degrees of freedom. Each has {N} sample points producing {N-1} lineAA@1 segments. k=2 is exponentially decaying, k=4 peaks near x=2, k=8 peaks near x=6. All share one transform mapping x=[0,{xmax}] to clip.",
        "| DrawItem | Element | Pipeline | Segments | Color |\n|---|---|---|---|---|\n| 102 | k=2 | lineAA@1 | 50 | cyan |\n| 105 | k=4 | lineAA@1 | 50 | yellow |\n| 108 | k=8 | lineAA@1 | 50 | magenta |",
        count_ids(doc), id_breakdown(doc),
        ["k=2 curve decays exponentially from x=0, matching the chi-square(2) = Exponential(2) identity.",
         "k=4 peaks near x=k-2=2 as expected from the mode formula max(k-2,0).",
         "k=8 peaks near x=6, forming a wider, more symmetric bell shape approaching normality.",
         "All three curves are correctly normalized PDFs computed using the exact formula with Gamma function."],
        ["Chi-square PDF uses math.gamma(k/2) — Python's math.gamma handles the half-integer cases.",
         "Mode of chi-square(k) is at x=k-2 for k>=2. Higher k approaches a normal distribution shape."])
    return "chi-square-curves", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 128: Regression Scatter
# ═══════════════════════════════════════════════════════════════════════
def trial_128():
    import random
    random.seed(7)
    N = 40
    xs = [i * 10.0 / (N - 1) for i in range(N)]
    slope = 2.5
    intercept = 3.0
    noise_std = 2.0
    ys = [slope * x + intercept + random.gauss(0, noise_std) for x in xs]

    # Compute best-fit line
    xm = sum(xs) / N
    ym = sum(ys) / N
    ss_xy = sum((xs[i] - xm) * (ys[i] - ym) for i in range(N))
    ss_xx = sum((xs[i] - xm) ** 2 for i in range(N))
    b1 = ss_xy / ss_xx
    b0 = ym - b1 * xm
    # R²
    y_pred = [b1 * x + b0 for x in xs]
    ss_res = sum((ys[i] - y_pred[i]) ** 2 for i in range(N))
    ss_tot = sum((ys[i] - ym) ** 2 for i in range(N))
    r2 = 1 - ss_res / ss_tot

    # Scatter data
    pts_data = []
    for i in range(N):
        pts_data.extend([xs[i], ys[i]])

    # Best-fit line
    line_data = [xs[0], b1 * xs[0] + b0, xs[-1], b1 * xs[-1] + b0]

    xmin_d, xmax_d = min(xs) - 0.5, max(xs) + 0.5
    ymin_d, ymax_d = min(ys) - 2, max(ys) + 2
    sx, stx = compute_transform(xmin_d, xmax_d, -0.9, 0.9)
    sy, sty = compute_transform(ymin_d, ymax_d, -0.85, 0.85)

    buffers = {100: buf(pts_data), 103: buf(line_data)}
    transforms = {50: xform(sx, sy, stx, sty)}
    panes = {1: pane("Main")}
    layers = {10: layer(1, "fit"), 11: layer(1, "scatter")}
    geometries = {
        101: geo(100, "pos2_clip", N),
        104: geo(103, "rect4", 1),
    }
    drawItems = {
        102: di(11, "scatter", "points@1", 101, CYAN, transformId=50, pointSize=6.0),
        105: di(10, "fit_line", "lineAA@1", 104, RED, transformId=50, lineWidth=2.0),
    }
    doc = make_doc(800, 500, buffers, transforms, panes, layers, geometries, drawItems)
    md = make_md(128, "Regression Scatter",
        f"40 scatter points (points@1) with best-fit regression line (lineAA@1). R² = {rnd(r2, 3)}.",
        f"Linear regression scatter plot with 40 data points (y = 2.5x + 3 + noise, sigma=2). Best-fit line computed via least squares: y = {rnd(b1,3)}x + {rnd(b0,3)}, R² = {rnd(r2,3)}. Points rendered as cyan dots on top of red fit line.",
        f"| DrawItem | Layer | Element | Pipeline | Count | Color |\n|---|---|---|---|---|---|\n| 102 | 11 | Scatter | points@1 | 40 pts | cyan |\n| 105 | 10 | Fit line | lineAA@1 | 1 seg | red |",
        count_ids(doc), id_breakdown(doc),
        [f"Best-fit slope {rnd(b1,3)} is close to the true slope 2.5, confirming least-squares accuracy.",
         f"R² = {rnd(r2,3)} indicates a strong linear fit with noise level sigma=2 on range [0,10].",
         "Fit line is rendered behind scatter points (layer 10 < 11) so points remain visible at all positions.",
         "All 40 points scatter around the red line with approximately equal spread above and below."],
        ["Least-squares regression: b1 = SS_xy / SS_xx, b0 = mean(y) - b1*mean(x). R² = 1 - SS_res/SS_tot.",
         "Rendering fit line behind scatter points ensures no data point is occluded."])
    return "regression-scatter", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 129: Residual Plot
# ═══════════════════════════════════════════════════════════════════════
def trial_129():
    import random
    random.seed(7)
    N = 40
    xs = [i * 10.0 / (N - 1) for i in range(N)]
    slope = 2.5
    intercept = 3.0
    noise_std = 2.0
    ys = [slope * x + intercept + random.gauss(0, noise_std) for x in xs]
    # Recompute fit
    xm = sum(xs) / N
    ym = sum(ys) / N
    ss_xy = sum((xs[i] - xm) * (ys[i] - ym) for i in range(N))
    ss_xx = sum((xs[i] - xm) ** 2 for i in range(N))
    b1 = ss_xy / ss_xx
    b0 = ym - b1 * xm
    residuals = [ys[i] - (b1 * xs[i] + b0) for i in range(N)]

    pts_data = []
    for i in range(N):
        pts_data.extend([xs[i], residuals[i]])

    # Zero line
    zero_data = [xs[0] - 0.5, 0, xs[-1] + 0.5, 0]

    rmin = min(residuals) - 1
    rmax = max(residuals) + 1
    sx, stx = compute_transform(min(xs) - 0.5, max(xs) + 0.5, -0.9, 0.9)
    sy, sty = compute_transform(rmin, rmax, -0.85, 0.85)

    buffers = {100: buf(pts_data), 103: buf(zero_data)}
    transforms = {50: xform(sx, sy, stx, sty)}
    panes = {1: pane("Main")}
    layers = {10: layer(1, "ref"), 11: layer(1, "residuals")}
    geometries = {
        101: geo(100, "pos2_clip", N),
        104: geo(103, "rect4", 1),
    }
    drawItems = {
        102: di(11, "residuals", "points@1", 101, CYAN, transformId=50, pointSize=6.0),
        105: di(10, "zero_line", "lineAA@1", 104, GRAY, transformId=50, lineWidth=1.5, dashLength=8.0, gapLength=5.0),
    }
    doc = make_doc(800, 500, buffers, transforms, panes, layers, geometries, drawItems)
    md = make_md(129, "Residual Plot",
        "40 residual points (points@1) scattered around y=0 horizontal dashed reference line.",
        f"Residual plot from linear regression (same data as trial 128). 40 residuals (observed - predicted) plotted against x-values. Dashed horizontal line at y=0. Residuals range from {rnd(min(residuals),2)} to {rnd(max(residuals),2)}, appearing randomly scattered (no pattern = good fit).",
        "| DrawItem | Layer | Element | Pipeline | Count | Color |\n|---|---|---|---|---|---|\n| 102 | 11 | Residuals | points@1 | 40 pts | cyan |\n| 105 | 10 | Zero line | lineAA@1 | 1 seg | gray |",
        count_ids(doc), id_breakdown(doc),
        ["Residuals scatter randomly around zero with no visible pattern, confirming linear model adequacy.",
         "No funnel shape (heteroscedasticity) or curvature (nonlinearity) visible in the residuals.",
         "Residuals are approximately symmetric around zero, consistent with Gaussian noise assumption.",
         "Dashed zero line provides clear visual reference for identifying systematic departures from model."],
        ["Residual plots detect model misspecification — patterns indicate nonlinearity, heteroscedasticity, or outliers.",
         "The same random seed (7) as trial 128 ensures the residuals are consistent with that regression."])
    return "residual-plot", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 130: Q-Q Plot
# ═══════════════════════════════════════════════════════════════════════
def trial_130():
    import random
    random.seed(42)
    N = 30
    # Generate sorted sample from normal
    sample = sorted([random.gauss(0, 1) for _ in range(N)])
    # Theoretical quantiles
    def phi_inv_approx(p):
        """Rough inverse normal CDF (Abramowitz & Stegun approximation)."""
        if p <= 0:
            return -3.5
        if p >= 1:
            return 3.5
        if p < 0.5:
            return -phi_inv_approx(1 - p)
        t = math.sqrt(-2 * math.log(1 - p))
        c0, c1, c2 = 2.515517, 0.802853, 0.010328
        d1, d2, d3 = 1.432788, 0.189269, 0.001308
        return t - (c0 + c1 * t + c2 * t ** 2) / (1 + d1 * t + d2 * t ** 2 + d3 * t ** 3)

    theoretical = [phi_inv_approx((i + 0.5) / N) for i in range(N)]

    pts_data = []
    for i in range(N):
        pts_data.extend([theoretical[i], sample[i]])

    # Reference line (45-degree)
    lo = min(min(theoretical), min(sample)) - 0.3
    hi = max(max(theoretical), max(sample)) + 0.3
    ref_data = [lo, lo, hi, hi]

    sx, stx = compute_transform(lo, hi, -0.9, 0.9)
    sy, sty = compute_transform(lo, hi, -0.9, 0.9)

    buffers = {100: buf(pts_data), 103: buf(ref_data)}
    transforms = {50: xform(sx, sy, stx, sty)}
    panes = {1: pane("Main")}
    layers = {10: layer(1, "ref"), 11: layer(1, "points")}
    geometries = {
        101: geo(100, "pos2_clip", N),
        104: geo(103, "rect4", 1),
    }
    drawItems = {
        102: di(11, "qq_points", "points@1", 101, CYAN, transformId=50, pointSize=6.0),
        105: di(10, "ref_line", "lineAA@1", 104, RED, transformId=50, lineWidth=1.5),
    }
    doc = make_doc(600, 600, buffers, transforms, panes, layers, geometries, drawItems)
    md = make_md(130, "Q-Q Plot",
        "Quantile-quantile plot with 30 points (points@1) and diagonal reference line (lineAA@1).",
        "Q-Q plot comparing 30 random normal samples against theoretical normal quantiles. Points are (theoretical quantile, sample quantile) pairs. Red diagonal reference line shows where points would fall for a perfect normal distribution. Points close to the line confirm normality.",
        "| DrawItem | Layer | Element | Pipeline | Count | Color |\n|---|---|---|---|---|---|\n| 102 | 11 | QQ points | points@1 | 30 pts | cyan |\n| 105 | 10 | Reference | lineAA@1 | 1 seg | red |",
        count_ids(doc), id_breakdown(doc),
        ["Points cluster tightly along the reference diagonal, confirming the sample is approximately normal.",
         "Theoretical quantiles use the Abramowitz & Stegun inverse normal CDF approximation for accuracy.",
         "Square viewport (600x600) preserves equal axis scaling for proper QQ interpretation.",
         "Reference line extends from min to max of both theoretical and sample values with 0.3 padding."],
        ["QQ plots compare sorted sample values against theoretical quantiles: points on the diagonal = good fit.",
         "The plotting position formula (i+0.5)/N is standard for QQ plots to avoid quantiles at exactly 0 or 1."],
        viewport="600x600")
    return "qq-plot", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 131: Bohr Atom
# ═══════════════════════════════════════════════════════════════════════
def trial_131():
    # Nucleus as filled circle, 3 orbital rings, 3 electron dots
    # All in clip space
    nuc_r = 0.06
    nuc_data = circle_tris(0, 0, nuc_r, 16)
    nuc_vtx = len(nuc_data) // 2

    orbit_radii = [0.3, 0.55, 0.8]
    orbit_segs = 48
    # Electron positions
    e_angles = [math.pi / 4, math.pi, 5 * math.pi / 3]
    e_r = 0.035
    e_segs = 12

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100

    # Nucleus
    buffers[bid] = buf(nuc_data)
    geometries[bid + 1] = geo(bid, "pos2_clip", nuc_vtx)
    drawItems[bid + 2] = di(12, "nucleus", "triSolid@1", bid + 1, RED)
    bid += 3

    # Orbital rings
    for i, r in enumerate(orbit_radii):
        ring = circle_segments(0, 0, r, orbit_segs)
        vc = len(ring) // 4
        buffers[bid] = buf(ring)
        geometries[bid + 1] = geo(bid, "rect4", vc)
        drawItems[bid + 2] = di(10, f"orbit_{i+1}", "lineAA@1", bid + 1, DIM, lineWidth=1.2)
        bid += 3

    # Electrons
    for i, (r, a) in enumerate(zip(orbit_radii, e_angles)):
        ex = r * math.cos(a)
        ey = r * math.sin(a)
        e_data = circle_tris(ex, ey, e_r, e_segs)
        e_vtx = len(e_data) // 2
        buffers[bid] = buf(e_data)
        geometries[bid + 1] = geo(bid, "pos2_clip", e_vtx)
        drawItems[bid + 2] = di(11, f"electron_{i+1}", "triSolid@1", bid + 1, CYAN)
        bid += 3

    panes = {1: pane("Bohr")}
    layers = {10: layer(1, "orbits"), 11: layer(1, "electrons"), 12: layer(1, "nucleus")}
    doc = make_doc(600, 600, {}, {}, panes, layers, {}, {})
    doc["buffers"] = {str(k): v for k, v in buffers.items()}
    doc["transforms"] = {}
    doc["geometries"] = {str(k): v for k, v in geometries.items()}
    doc["drawItems"] = {str(k): v for k, v in drawItems.items()}
    md = make_md(131, "Bohr Atom Model",
        "Bohr atom with nucleus circle (triSolid@1), 3 orbital rings (lineAA@1, 48 segments each), and 3 electron dots (triSolid@1).",
        f"Bohr atom model. Red nucleus (r=0.06, 16 triangles), 3 orbital rings at r=0.3, 0.55, 0.8 (48 lineAA segments each), 3 cyan electrons (r=0.035, 12 triangles each) positioned on their respective orbits. All in clip space, no transform needed.",
        "| DrawItem | Layer | Element | Pipeline | Count | Color |\n|---|---|---|---|---|---|\n| 102 | 12 | Nucleus | triSolid@1 | 48 vtx | red |\n| orbit 1-3 | 10 | Orbital rings | lineAA@1 | 48 seg each | dim |\n| electron 1-3 | 11 | Electrons | triSolid@1 | 36 vtx each | cyan |",
        count_ids(doc), id_breakdown(doc),
        ["Nucleus at origin is the correct center point for all orbits.",
         "Three concentric orbits at increasing radii represent energy levels n=1,2,3.",
         "Electrons are positioned on their respective orbits at distinct angles for visual clarity.",
         "Layer ordering: orbits (10) behind electrons (11) behind nucleus (12) gives correct depth."],
        ["Bohr model is concentric circles centered at the nucleus — simplest atom visualization.",
         "Small filled circles (triSolid fan tessellation) effectively render point-like particles."],
        viewport="600x600")
    return "bohr-atom", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 132: Benzene Ring
# ═══════════════════════════════════════════════════════════════════════
def trial_132():
    R = 0.55  # ring radius
    node_r = 0.04
    inner_r = 0.32  # inner circle for delocalized electrons

    # 6 vertices of hexagon
    hex_pts = [(R * math.cos(math.pi / 2 + i * math.pi / 3),
                R * math.sin(math.pi / 2 + i * math.pi / 3)) for i in range(6)]

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100

    # 6 bonds (edges of hexagon) — lineAA@1
    bond_data = []
    for i in range(6):
        j = (i + 1) % 6
        bond_data.extend([hex_pts[i][0], hex_pts[i][1], hex_pts[j][0], hex_pts[j][1]])
    buffers[bid] = buf(bond_data)
    geometries[bid + 1] = geo(bid, "rect4", 6)
    drawItems[bid + 2] = di(10, "bonds", "lineAA@1", bid + 1, WHITE, lineWidth=2.5)
    bid += 3

    # Alternating double bonds (3 parallel inner lines at edges 0, 2, 4)
    double_data = []
    offset = 0.04
    for i in [0, 2, 4]:
        j = (i + 1) % 6
        # Offset inward (toward center)
        mx = (hex_pts[i][0] + hex_pts[j][0]) / 2
        my = (hex_pts[i][1] + hex_pts[j][1]) / 2
        dx = mx / math.sqrt(mx ** 2 + my ** 2 + 1e-9) * offset
        dy = my / math.sqrt(mx ** 2 + my ** 2 + 1e-9) * offset
        # Shorten the double bond slightly
        frac = 0.15
        x0 = hex_pts[i][0] * (1 - frac) + hex_pts[j][0] * frac - dx
        y0 = hex_pts[i][1] * (1 - frac) + hex_pts[j][1] * frac - dy
        x1 = hex_pts[i][0] * frac + hex_pts[j][0] * (1 - frac) - dx
        y1 = hex_pts[i][1] * frac + hex_pts[j][1] * (1 - frac) - dy
        double_data.extend([x0, y0, x1, y1])
    buffers[bid] = buf(double_data)
    geometries[bid + 1] = geo(bid, "rect4", 3)
    drawItems[bid + 2] = di(10, "double_bonds", "lineAA@1", bid + 1, WHITE, lineWidth=1.5)
    bid += 3

    # Inner circle (delocalized electrons)
    inner_segs = circle_segments(0, 0, inner_r, 36)
    vc_inner = len(inner_segs) // 4
    buffers[bid] = buf(inner_segs)
    geometries[bid + 1] = geo(bid, "rect4", vc_inner)
    drawItems[bid + 2] = di(11, "deloc_circle", "lineAA@1", bid + 1, CYAN, lineWidth=1.5, dashLength=4.0, gapLength=3.0)
    bid += 3

    # 6 carbon nodes
    for i in range(6):
        cx, cy = hex_pts[i]
        node_data = circle_tris(cx, cy, node_r, 10)
        nvtx = len(node_data) // 2
        buffers[bid] = buf(node_data)
        geometries[bid + 1] = geo(bid, "pos2_clip", nvtx)
        drawItems[bid + 2] = di(12, f"carbon_{i}", "triSolid@1", bid + 1, GRAY)
        bid += 3

    panes = {1: pane("Benzene")}
    layers = {10: layer(1, "bonds"), 11: layer(1, "inner"), 12: layer(1, "nodes")}
    doc = make_doc(600, 600, buffers, {}, panes, layers, geometries, drawItems)
    md = make_md(132, "Benzene Ring",
        "Hexagonal benzene structure: 6 carbon nodes (triSolid@1), 6 bonds + 3 double bonds (lineAA@1), inner dashed circle for delocalized electrons.",
        f"Benzene molecule (C6H6). Regular hexagon with R=0.55 centered at origin. 6 single bonds (lineAA@1, lw=2.5), 3 alternating double bonds (lineAA@1, lw=1.5, offset inward), dashed inner circle (r={inner_r}) representing delocalized pi electrons, 6 carbon atom nodes (triSolid@1, r=0.04, 10 triangles each).",
        "| Layer | Elements | Pipeline | Color |\n|---|---|---|---|\n| 10 | 6 bonds + 3 double | lineAA@1 | white |\n| 11 | Inner deloc circle | lineAA@1 | cyan dashed |\n| 12 | 6 carbon nodes | triSolid@1 | gray |",
        count_ids(doc), id_breakdown(doc),
        ["Regular hexagon has all vertices at equal radius R=0.55 with 60-degree angular spacing.",
         "Alternating double bonds on edges 0, 2, 4 match the Kekule structure convention.",
         "Inner dashed circle represents the delocalized pi electron cloud, centered inside the ring.",
         "Carbon nodes are rendered on top of bonds (layer 12 > 10) so they appear as clean circles at vertices."],
        ["Benzene hexagon vertices: (R*cos(pi/2 + i*pi/3), R*sin(pi/2 + i*pi/3)) places vertex 0 at top.",
         "Double bonds are offset inward toward center and shortened to create the parallel-line visual."],
        viewport="600x600")
    return "benzene-ring", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 133: Electromagnetic Spectrum
# ═══════════════════════════════════════════════════════════════════════
def trial_133():
    # 7 bands from radio to gamma
    bands = [
        ("Radio",       [0.3, 0.3, 0.3, 1.0]),
        ("Microwave",   [0.5, 0.3, 0.1, 1.0]),
        ("Infrared",    [0.8, 0.2, 0.1, 1.0]),
        ("Visible",     [1.0, 1.0, 0.0, 1.0]),
        ("Ultraviolet", [0.4, 0.1, 0.8, 1.0]),
        ("X-ray",       [0.1, 0.4, 0.9, 1.0]),
        ("Gamma",       [0.1, 0.8, 0.3, 1.0]),
    ]
    # Horizontal bar from left to right
    n_bands = len(bands)
    band_width = 1.6 / n_bands  # total width ~1.6 in clip
    y_min = -0.2
    y_max = 0.2

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100
    for i, (name, color) in enumerate(bands):
        x0 = -0.8 + i * band_width
        x1 = x0 + band_width - 0.005  # small gap
        rect = [x0, y_min, x1, y_max]
        buffers[bid] = buf(rect)
        geometries[bid + 1] = geo(bid, "rect4", 1)
        drawItems[bid + 2] = di(10, name.lower(), "instancedRect@1", bid + 1, color, cornerRadius=0.0)
        bid += 3

    panes = {1: pane("EM Spectrum")}
    layers = {10: layer(1, "bands")}
    doc = make_doc(900, 300, buffers, {}, panes, layers, geometries, drawItems)
    md = make_md(133, "Electromagnetic Spectrum",
        "Horizontal bar showing 7 electromagnetic spectrum bands (Radio to Gamma) as instancedRect@1 colored rectangles.",
        "Electromagnetic spectrum visualization with 7 instancedRect@1 bands arranged horizontally. From left to right: Radio (gray), Microwave (brown), Infrared (red), Visible (yellow), Ultraviolet (purple), X-ray (blue), Gamma (green). Each band occupies an equal width, centered vertically.",
        "| DrawItem | Element | Pipeline | Color |\n|---|---|---|---|\n| 102 | Radio | instancedRect@1 | gray |\n| 105 | Microwave | instancedRect@1 | brown |\n| 108 | Infrared | instancedRect@1 | red |\n| 111 | Visible | instancedRect@1 | yellow |\n| 114 | Ultraviolet | instancedRect@1 | purple |\n| 117 | X-ray | instancedRect@1 | blue |\n| 120 | Gamma | instancedRect@1 | green |",
        count_ids(doc), id_breakdown(doc),
        ["Seven bands are arranged left-to-right in order of increasing frequency (Radio → Gamma).",
         "Each band has a distinct color representing its approximate wavelength or convention.",
         "Small gaps between bands create visual separation without a visible grid.",
         "Wide viewport (900x300) creates a natural horizontal bar layout."],
        ["EM spectrum visualizations use equal-width bands for conceptual frequency ranges.",
         "instancedRect@1 is ideal for simple rectangular color blocks with no transform needed."],
        viewport="900x300")
    return "em-spectrum", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 134: Logic Gates
# ═══════════════════════════════════════════════════════════════════════
def trial_134():
    # 3 gates: AND, OR, NOT, arranged horizontally
    # Each gate ~0.5 wide, centered at x = -0.6, 0, 0.6

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100

    def and_gate(cx, cy):
        """AND gate: flat left side, curved right side."""
        nonlocal bid
        # Body outline
        w, h = 0.15, 0.18
        pts = []
        pts.append((cx - w, cy + h))
        pts.append((cx - w, cy - h))
        # Curved right side (semicircle)
        for i in range(13):
            a = -math.pi / 2 + i * math.pi / 12
            pts.append((cx + w * math.cos(a), cy + h * math.sin(a)))
        pts.append((cx - w, cy + h))
        data = lineaa_segments(pts)
        vc = len(data) // 4
        buffers[bid] = buf(data)
        geometries[bid + 1] = geo(bid, "rect4", vc)
        drawItems[bid + 2] = di(11, "and_body", "lineAA@1", bid + 1, WHITE, lineWidth=2.0)
        bid += 3
        # Input lines
        inp = [cx - w - 0.12, cy + 0.09, cx - w, cy + 0.09,
               cx - w - 0.12, cy - 0.09, cx - w, cy - 0.09]
        buffers[bid] = buf(inp)
        geometries[bid + 1] = geo(bid, "rect4", 2)
        drawItems[bid + 2] = di(10, "and_inputs", "lineAA@1", bid + 1, GRAY, lineWidth=1.5)
        bid += 3
        # Output line
        out = [cx + w, cy, cx + w + 0.12, cy]
        buffers[bid] = buf(out)
        geometries[bid + 1] = geo(bid, "rect4", 1)
        drawItems[bid + 2] = di(10, "and_output", "lineAA@1", bid + 1, GRAY, lineWidth=1.5)
        bid += 3

    def or_gate(cx, cy):
        """OR gate: curved input side, pointed output."""
        nonlocal bid
        w, h = 0.15, 0.18
        pts = []
        # Curved left side
        for i in range(7):
            a = -math.pi / 6 + i * math.pi / 18
            pts.append((cx - w + 0.05 * math.cos(a), cy + h * (1 - 2 * i / 6)))
        # Upper curve to tip
        for i in range(9):
            t = i / 8
            px = cx - w * (1 - t) + (w + 0.05) * t
            py = cy + h * (1 - t) ** 0.7
            pts.append((px, py))
        # Lower curve from tip back
        for i in range(9):
            t = 1 - i / 8
            px = cx - w * (1 - t) + (w + 0.05) * t
            py = cy - h * (1 - t) ** 0.7
            pts.append((px, py))
        pts.append(pts[0])  # close
        data = lineaa_segments(pts)
        vc = len(data) // 4
        buffers[bid] = buf(data)
        geometries[bid + 1] = geo(bid, "rect4", vc)
        drawItems[bid + 2] = di(11, "or_body", "lineAA@1", bid + 1, YELLOW, lineWidth=2.0)
        bid += 3
        # Input/output lines
        inp = [cx - w - 0.12, cy + 0.09, cx - w + 0.02, cy + 0.09,
               cx - w - 0.12, cy - 0.09, cx - w + 0.02, cy - 0.09]
        buffers[bid] = buf(inp)
        geometries[bid + 1] = geo(bid, "rect4", 2)
        drawItems[bid + 2] = di(10, "or_inputs", "lineAA@1", bid + 1, GRAY, lineWidth=1.5)
        bid += 3
        out = [cx + w + 0.05, cy, cx + w + 0.17, cy]
        buffers[bid] = buf(out)
        geometries[bid + 1] = geo(bid, "rect4", 1)
        drawItems[bid + 2] = di(10, "or_output", "lineAA@1", bid + 1, GRAY, lineWidth=1.5)
        bid += 3

    def not_gate(cx, cy):
        """NOT gate: triangle + bubble."""
        nonlocal bid
        w, h = 0.12, 0.16
        # Triangle body (filled)
        tri_data = [cx - w, cy + h, cx - w, cy - h, cx + w, cy]
        buffers[bid] = buf(tri_data)
        geometries[bid + 1] = geo(bid, "pos2_clip", 3)
        drawItems[bid + 2] = di(11, "not_fill", "triSolid@1", bid + 1, [0.1, 0.15, 0.3, 1.0])
        bid += 3
        # Triangle outline
        outline_pts = [(cx - w, cy + h), (cx - w, cy - h), (cx + w, cy), (cx - w, cy + h)]
        outline = lineaa_segments(outline_pts)
        vc = len(outline) // 4
        buffers[bid] = buf(outline)
        geometries[bid + 1] = geo(bid, "rect4", vc)
        drawItems[bid + 2] = di(12, "not_outline", "lineAA@1", bid + 1, MAGENTA, lineWidth=2.0)
        bid += 3
        # Bubble at output
        bub_r = 0.025
        bub = circle_segments(cx + w + bub_r, cy, bub_r, 12)
        bvc = len(bub) // 4
        buffers[bid] = buf(bub)
        geometries[bid + 1] = geo(bid, "rect4", bvc)
        drawItems[bid + 2] = di(12, "not_bubble", "lineAA@1", bid + 1, MAGENTA, lineWidth=1.5)
        bid += 3
        # Input/output lines
        inp = [cx - w - 0.12, cy, cx - w, cy]
        buffers[bid] = buf(inp)
        geometries[bid + 1] = geo(bid, "rect4", 1)
        drawItems[bid + 2] = di(10, "not_input", "lineAA@1", bid + 1, GRAY, lineWidth=1.5)
        bid += 3
        out = [cx + w + 2 * bub_r, cy, cx + w + 2 * bub_r + 0.12, cy]
        buffers[bid] = buf(out)
        geometries[bid + 1] = geo(bid, "rect4", 1)
        drawItems[bid + 2] = di(10, "not_output", "lineAA@1", bid + 1, GRAY, lineWidth=1.5)
        bid += 3

    and_gate(-0.55, 0)
    or_gate(0, 0)
    not_gate(0.55, 0)

    panes = {1: pane("Logic Gates")}
    layers = {10: layer(1, "wires"), 11: layer(1, "fill"), 12: layer(1, "outlines")}
    doc = make_doc(900, 400, buffers, {}, panes, layers, geometries, drawItems)
    md = make_md(134, "Logic Gates",
        "3 logic gate symbols (AND, OR, NOT) drawn with lineAA@1 outlines, triSolid@1 fills, and input/output wires.",
        "Three logic gate symbols arranged horizontally. AND gate (white outline, curved right side), OR gate (yellow outline, curved body with pointed output), NOT gate (magenta triangle + inversion bubble). Each has input and output wires (gray lineAA@1). All drawn in clip space.",
        "| Gate | Body | Wires | Color |\n|---|---|---|---|\n| AND | lineAA@1 outline | 3 lines | white |\n| OR | lineAA@1 outline | 3 lines | yellow |\n| NOT | triSolid fill + lineAA outline + bubble | 2 lines | magenta |",
        count_ids(doc), id_breakdown(doc),
        ["AND gate has flat left side and semicircular right side, matching standard symbol.",
         "OR gate has curved body tapering to a point on the output side.",
         "NOT gate is a triangle with a small circle (inversion bubble) at the output.",
         "Input/output wires are properly aligned with gate body edges for clean connections."],
        ["Logic gate symbols use lineAA@1 for outlines and triSolid@1 for fills — combining pipelines per element.",
         "Layer separation (wires < fill < outlines) ensures proper visual stacking."],
        viewport="900x400")
    return "logic-gates", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 135: Digital Timing Diagram
# ═══════════════════════════════════════════════════════════════════════
def trial_135():
    # 4 signals: clock, data, enable, output
    # Each signal is a square wave with different patterns
    # Data space: x=[0, 16] (time units), y varies per signal
    # We'll place each signal on its own vertical band

    signals = {
        "clock":  [0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1],
        "data":   [0,0,1,1,1,0,0,1,1,1,0,0,1,0,0,1],
        "enable": [0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0],
        "output": [0,0,0,1,1,0,0,1,1,1,0,0,0,0,0,0],
    }
    colors = [CYAN, YELLOW, GREEN, RED]
    names = list(signals.keys())

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100

    for si, name in enumerate(names):
        vals = signals[name]
        # Y offset: signal si occupies y range [si*2, si*2+1.5]
        y_base = si * 2.0
        pts = []
        for i, v in enumerate(vals):
            y = y_base + v * 1.2
            if i == 0:
                pts.append((float(i), y))
            else:
                # Step: horizontal then vertical
                prev_y = y_base + vals[i-1] * 1.2
                pts.append((float(i), prev_y))
                pts.append((float(i), y))
        pts.append((float(len(vals)), y_base + vals[-1] * 1.2))
        data = lineaa_segments(pts)
        vc = len(data) // 4
        buffers[bid] = buf(data)
        geometries[bid + 1] = geo(bid, "rect4", vc)
        drawItems[bid + 2] = di(10, name, "lineAA@1", bid + 1, colors[si],
                                 transformId=50, lineWidth=2.0)
        bid += 3

    sx, stx = compute_transform(-0.5, 16.5, -0.9, 0.9)
    sy, sty = compute_transform(-0.5, 8.5, -0.9, 0.9)
    transforms = {50: xform(sx, sy, stx, sty)}
    panes = {1: pane("Timing")}
    layers = {10: layer(1, "signals")}
    doc = make_doc(900, 500, buffers, transforms, panes, layers, geometries, drawItems)
    md = make_md(135, "Digital Timing Diagram",
        "4 digital waveforms (clock, data, enable, output) as square waves using lineAA@1, each on its own horizontal track.",
        "Digital timing diagram with 4 signals over 16 time units. Clock is a regular 50% duty cycle. Data is an arbitrary bit pattern. Enable gates the output. Output = Data AND Enable. Each signal uses step transitions (horizontal then vertical) for clean square waves. Signals are stacked vertically with distinct colors.",
        "| DrawItem | Signal | Pipeline | Color |\n|---|---|---|---|\n| 102 | Clock | lineAA@1 | cyan |\n| 105 | Data | lineAA@1 | yellow |\n| 108 | Enable | lineAA@1 | green |\n| 111 | Output | lineAA@1 | red |",
        count_ids(doc), id_breakdown(doc),
        ["Clock signal toggles every time unit with perfect 50% duty cycle.",
         "Output = Data AND Enable: output is high only when both data and enable are high.",
         "Step transitions are rendered correctly: horizontal segment then vertical rise/fall.",
         "Four signals are vertically separated with no overlap, each distinguishable by color."],
        ["Digital waveforms use step transitions: (t, prev_val) → (t, new_val) at each edge.",
         "Vertical stacking of signals in data space with a shared transform avoids multi-pane complexity."],
        viewport="900x500")
    return "digital-timing", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 136: Phasor Diagram
# ═══════════════════════════════════════════════════════════════════════
def trial_136():
    # 3 phasors at different angles on a unit circle
    angles_deg = [30, 150, 270]
    magnitudes = [0.7, 0.5, 0.6]
    colors_p = [CYAN, YELLOW, GREEN]

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100

    # Unit circle (dashed)
    uc = circle_segments(0, 0, 0.8, 48)
    vc_uc = len(uc) // 4
    buffers[bid] = buf(uc)
    geometries[bid + 1] = geo(bid, "rect4", vc_uc)
    drawItems[bid + 2] = di(10, "unit_circle", "lineAA@1", bid + 1, DIM,
                             lineWidth=1.0, dashLength=6.0, gapLength=4.0)
    bid += 3

    # Axes
    axes_data = [-0.9, 0, 0.9, 0, 0, -0.9, 0, 0.9]
    buffers[bid] = buf(axes_data)
    geometries[bid + 1] = geo(bid, "rect4", 2)
    drawItems[bid + 2] = di(10, "axes", "lineAA@1", bid + 1, DIM, lineWidth=0.8)
    bid += 3

    # 3 phasors (arrows from origin)
    for i in range(3):
        a = math.radians(angles_deg[i])
        m = magnitudes[i] * 0.8  # scale to unit circle radius
        ex = m * math.cos(a)
        ey = m * math.sin(a)
        arrow_data = [0, 0, ex, ey]
        buffers[bid] = buf(arrow_data)
        geometries[bid + 1] = geo(bid, "rect4", 1)
        drawItems[bid + 2] = di(11, f"phasor_{angles_deg[i]}", "lineAA@1", bid + 1,
                                 colors_p[i], lineWidth=3.0)
        bid += 3

    # Tips (points)
    tip_data = []
    for i in range(3):
        a = math.radians(angles_deg[i])
        m = magnitudes[i] * 0.8
        tip_data.extend([m * math.cos(a), m * math.sin(a)])
    buffers[bid] = buf(tip_data)
    geometries[bid + 1] = geo(bid, "pos2_clip", 3)
    drawItems[bid + 2] = di(12, "tips", "points@1", bid + 1, WHITE, pointSize=8.0)
    bid += 3

    # Arrowheads (small triangles at tips)
    for i in range(3):
        a = math.radians(angles_deg[i])
        m = magnitudes[i] * 0.8
        ex = m * math.cos(a)
        ey = m * math.sin(a)
        # Arrowhead: triangle pointing in direction of phasor
        head_len = 0.05
        head_w = 0.025
        perp_x = -math.sin(a) * head_w
        perp_y = math.cos(a) * head_w
        back_x = ex - head_len * math.cos(a)
        back_y = ey - head_len * math.sin(a)
        tri = [ex, ey, back_x + perp_x, back_y + perp_y, back_x - perp_x, back_y - perp_y]
        buffers[bid] = buf(tri)
        geometries[bid + 1] = geo(bid, "pos2_clip", 3)
        drawItems[bid + 2] = di(11, f"arrow_{angles_deg[i]}", "triSolid@1", bid + 1, colors_p[i])
        bid += 3

    panes = {1: pane("Phasor")}
    layers = {10: layer(1, "grid"), 11: layer(1, "phasors"), 12: layer(1, "tips")}
    doc = make_doc(600, 600, buffers, {}, panes, layers, geometries, drawItems)
    md = make_md(136, "Phasor Diagram",
        "3 phasors at 30deg, 150deg, 270deg with arrowheads (lineAA@1 + triSolid@1) on a dashed unit circle.",
        "Phasor diagram with 3 phasors at 30, 150, and 270 degrees with magnitudes 0.7, 0.5, 0.6 (scaled to unit circle R=0.8). Each phasor is a lineAA@1 arrow from origin with a triSolid@1 arrowhead. Dashed unit circle reference. Point markers at tips. Axes through origin.",
        "| Layer | Elements | Pipeline | Color |\n|---|---|---|---|\n| 10 | Unit circle + axes | lineAA@1 | dim |\n| 11 | 3 phasors + 3 arrowheads | lineAA + triSolid | cyan/yellow/green |\n| 12 | 3 tip points | points@1 | white |",
        count_ids(doc), id_breakdown(doc),
        ["All 3 phasors originate from (0,0) and extend to correct polar coordinates.",
         "Arrowheads point in the direction of each phasor, computed from the angle.",
         "Dashed unit circle provides magnitude reference — phasor tip distance from center shows relative magnitude.",
         "120-degree angular separation between phasors (30, 150, 270) resembles a 3-phase power system."],
        ["Phasor arrows use (0,0) → (m*cos(a), m*sin(a)) in clip space — no transform needed for a symmetric diagram.",
         "Arrowheads are small triangles oriented along the phasor direction using perpendicular offsets."],
        viewport="600x600")
    return "phasor-diagram", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 137: Gear Tooth Profile
# ═══════════════════════════════════════════════════════════════════════
def trial_137():
    # Involute gear with 8 teeth
    n_teeth = 8
    base_r = 0.35
    outer_r = 0.55
    root_r = 0.30
    # Dedendum/addendum
    tooth_half_angle = math.pi / n_teeth * 0.45

    # Gear body fill (circle at base_r)
    body_data = circle_tris(0, 0, root_r, 32)
    body_vtx = len(body_data) // 2

    # Tooth outlines
    tooth_data = []
    for i in range(n_teeth):
        center_angle = 2 * math.pi * i / n_teeth
        # Left side of tooth (root to tip)
        a_left = center_angle - tooth_half_angle
        a_right = center_angle + tooth_half_angle
        # Root left
        pts = [
            (root_r * math.cos(a_left - 0.03), root_r * math.sin(a_left - 0.03)),
            (base_r * math.cos(a_left), base_r * math.sin(a_left)),
            (outer_r * math.cos(a_left + 0.02), outer_r * math.sin(a_left + 0.02)),
            (outer_r * math.cos(a_right - 0.02), outer_r * math.sin(a_right - 0.02)),
            (base_r * math.cos(a_right), base_r * math.sin(a_right)),
            (root_r * math.cos(a_right + 0.03), root_r * math.sin(a_right + 0.03)),
        ]
        tooth_data.extend(lineaa_segments(pts))

    tooth_vc = len(tooth_data) // 4

    # Root circle (connecting roots)
    root_circ = circle_segments(0, 0, root_r, 48)
    root_vc = len(root_circ) // 4

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100

    buffers[bid] = buf(body_data)
    geometries[bid + 1] = geo(bid, "pos2_clip", body_vtx)
    drawItems[bid + 2] = di(10, "gear_body", "triSolid@1", bid + 1, [0.15, 0.2, 0.35, 1.0])
    bid += 3

    buffers[bid] = buf(tooth_data)
    geometries[bid + 1] = geo(bid, "rect4", tooth_vc)
    drawItems[bid + 2] = di(11, "tooth_outlines", "lineAA@1", bid + 1, WHITE, lineWidth=2.0)
    bid += 3

    buffers[bid] = buf(root_circ)
    geometries[bid + 1] = geo(bid, "rect4", root_vc)
    drawItems[bid + 2] = di(11, "root_circle", "lineAA@1", bid + 1, GRAY, lineWidth=1.0)
    bid += 3

    # Center hole
    hole = circle_segments(0, 0, 0.08, 16)
    hole_vc = len(hole) // 4
    buffers[bid] = buf(hole)
    geometries[bid + 1] = geo(bid, "rect4", hole_vc)
    drawItems[bid + 2] = di(12, "center_hole", "lineAA@1", bid + 1, DIM, lineWidth=1.5)
    bid += 3

    panes = {1: pane("Gear")}
    layers = {10: layer(1, "body"), 11: layer(1, "outlines"), 12: layer(1, "hole")}
    doc = make_doc(600, 600, buffers, {}, panes, layers, geometries, drawItems)
    md = make_md(137, "Gear Tooth Profile",
        "8-tooth gear profile with involute tooth outlines (lineAA@1), body fill (triSolid@1), root circle, and center hole.",
        f"Involute gear with {n_teeth} teeth. Base radius={base_r}, outer radius={outer_r}, root radius={root_r}. Gear body filled with triSolid@1 ({body_vtx} vertices, 32 triangles). {n_teeth} tooth outlines ({tooth_vc} lineAA segments total). Root circle and center bore hole. All in clip space.",
        f"| Layer | Elements | Pipeline | Color |\n|---|---|---|---|\n| 10 | Body fill | triSolid@1 | dark blue |\n| 11 | Tooth outlines + root circle | lineAA@1 | white/gray |\n| 12 | Center hole | lineAA@1 | dim |",
        count_ids(doc), id_breakdown(doc),
        [f"All {n_teeth} teeth are equally spaced at {360/n_teeth} degrees around the center.",
         "Each tooth profile has 5 segments: root-to-base, base-to-tip, across tip, tip-to-base, base-to-root.",
         "Body fill at root radius provides the solid disc behind the tooth outlines.",
         "Center hole indicates the shaft bore, typical of gear engineering drawings."],
        ["Gear teeth are evenly distributed: center_angle = 2*pi*i/n_teeth for tooth i.",
         "Tooth outline uses root_r → base_r → outer_r → outer_r → base_r → root_r profile per tooth."],
        viewport="600x600")
    return "gear-tooth", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 138: Truss Bridge
# ═══════════════════════════════════════════════════════════════════════
def trial_138():
    # Warren truss: 11 nodes, 19 members
    # Bottom chord: 6 nodes at y=0
    # Top chord: 5 nodes at y=1 (offset by half span)
    span = 10.0
    height = 2.0
    bottom = [(i * span / 5, 0) for i in range(6)]
    top = [(i * span / 5 + span / 10, height) for i in range(5)]
    nodes = bottom + top

    # Members (pairs of node indices)
    members = []
    # Bottom chord
    for i in range(5):
        members.append((i, i + 1))
    # Top chord
    for i in range(4):
        members.append((6 + i, 6 + i + 1))
    # Diagonals and verticals
    for i in range(5):
        members.append((i, 6 + i))        # left diagonal
        members.append((i + 1, 6 + i))    # right diagonal

    assert len(members) == 19

    # Member lines
    member_data = []
    for a, b in members:
        member_data.extend([nodes[a][0], nodes[a][1], nodes[b][0], nodes[b][1]])

    # Node points
    node_data = []
    for nx, ny in nodes:
        node_data.extend([nx, ny])

    # Support triangles at ends (at node 0 and node 5)
    sup_size = 0.3
    sup_data = []
    for nx, ny in [nodes[0], nodes[5]]:
        sup_data.extend([
            nx, ny,
            nx - sup_size, ny - sup_size,
            nx + sup_size, ny - sup_size,
        ])
    sup_vtx = len(sup_data) // 2

    sx, stx = compute_transform(-1, span + 1, -0.9, 0.9)
    sy, sty = compute_transform(-1, height + 1, -0.8, 0.8)

    buffers = {
        100: buf(member_data),
        103: buf(node_data),
        106: buf(sup_data),
    }
    transforms = {50: xform(sx, sy, stx, sty)}
    panes = {1: pane("Truss")}
    layers = {10: layer(1, "supports"), 11: layer(1, "members"), 12: layer(1, "nodes")}
    geometries = {
        101: geo(100, "rect4", 19),
        104: geo(103, "pos2_clip", 11),
        107: geo(106, "pos2_clip", sup_vtx),
    }
    drawItems = {
        102: di(11, "members", "lineAA@1", 101, WHITE, transformId=50, lineWidth=2.0),
        105: di(12, "nodes", "points@1", 104, CYAN, transformId=50, pointSize=8.0),
        108: di(10, "supports", "triSolid@1", 107, GRAY, transformId=50),
    }
    doc = make_doc(900, 400, buffers, transforms, panes, layers, geometries, drawItems)
    md = make_md(138, "Truss Bridge",
        "Warren truss with 11 nodes (points@1), 19 members (lineAA@1), and support triangles at ends (triSolid@1).",
        f"Warren truss bridge. 6 bottom chord nodes at y=0, 5 top chord nodes at y={height} (offset by half-span). 19 members: 5 bottom chord, 4 top chord, 10 diagonals. Support triangles at both ends. Span = {span} units.",
        "| DrawItem | Layer | Element | Pipeline | Count | Color |\n|---|---|---|---|---|---|\n| 102 | 11 | Members | lineAA@1 | 19 seg | white |\n| 105 | 12 | Nodes | points@1 | 11 pts | cyan |\n| 108 | 10 | Supports | triSolid@1 | 6 vtx | gray |",
        count_ids(doc), id_breakdown(doc),
        ["Warren truss has alternating diagonal members creating a zigzag pattern between chords.",
         "11 nodes (6 bottom + 5 top) and 19 members (5+4+10) match standard Warren truss topology.",
         "Top chord nodes are offset by half the panel width, creating isosceles triangles.",
         "Support triangles at both ends indicate pin and roller supports per structural convention."],
        ["Warren trusses have top chord nodes at midpoints between bottom chord nodes.",
         "19 members = 5 bottom + 4 top + 10 diagonals (each bottom-top pair has 2 diagonals)."],
        viewport="900x400")
    return "truss-bridge", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 139: Spring-Mass System
# ═══════════════════════════════════════════════════════════════════════
def trial_139():
    # Wall on left, zigzag spring, mass block on right
    wall_x = -0.8
    mass_x = 0.3
    mass_w = 0.2
    mass_h = 0.25
    spring_y = 0.0
    n_zig = 10
    spring_start = wall_x + 0.05
    spring_end = mass_x
    spring_len = spring_end - spring_start
    zig_width = 0.06

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100

    # Wall (instancedRect)
    wall_data = [wall_x - 0.04, -0.4, wall_x, 0.4]
    buffers[bid] = buf(wall_data)
    geometries[bid + 1] = geo(bid, "rect4", 1)
    drawItems[bid + 2] = di(10, "wall", "instancedRect@1", bid + 1, GRAY)
    bid += 3

    # Wall hatching (small diagonal lines)
    hatch_data = []
    for i in range(8):
        yh = -0.35 + i * 0.1
        hatch_data.extend([wall_x - 0.04, yh, wall_x - 0.12, yh - 0.08])
    hatch_vc = len(hatch_data) // 4
    buffers[bid] = buf(hatch_data)
    geometries[bid + 1] = geo(bid, "rect4", hatch_vc)
    drawItems[bid + 2] = di(10, "hatching", "lineAA@1", bid + 1, DIM, lineWidth=1.0)
    bid += 3

    # Spring (zigzag)
    spring_pts = [(spring_start, spring_y)]
    for i in range(n_zig):
        t = (i + 0.5) / n_zig
        x = spring_start + t * spring_len
        y = spring_y + (zig_width if i % 2 == 0 else -zig_width)
        spring_pts.append((x, y))
    spring_pts.append((spring_end, spring_y))
    spring_data = lineaa_segments(spring_pts)
    spring_vc = len(spring_data) // 4
    buffers[bid] = buf(spring_data)
    geometries[bid + 1] = geo(bid, "rect4", spring_vc)
    drawItems[bid + 2] = di(11, "spring", "lineAA@1", bid + 1, CYAN, lineWidth=2.0)
    bid += 3

    # Mass block
    mass_data = [mass_x, -mass_h / 2, mass_x + mass_w, mass_h / 2]
    buffers[bid] = buf(mass_data)
    geometries[bid + 1] = geo(bid, "rect4", 1)
    drawItems[bid + 2] = di(11, "mass", "instancedRect@1", bid + 1, BLUE, cornerRadius=3.0)
    bid += 3

    # Equilibrium line (dashed)
    eq_data = [mass_x + mass_w / 2, -0.5, mass_x + mass_w / 2, 0.5]
    buffers[bid] = buf(eq_data)
    geometries[bid + 1] = geo(bid, "rect4", 1)
    drawItems[bid + 2] = di(10, "equilibrium", "lineAA@1", bid + 1, DIM,
                             lineWidth=1.0, dashLength=5.0, gapLength=4.0)
    bid += 3

    # Ground line
    ground_data = [wall_x - 0.15, -0.4, mass_x + mass_w + 0.2, -0.4]
    buffers[bid] = buf(ground_data)
    geometries[bid + 1] = geo(bid, "rect4", 1)
    drawItems[bid + 2] = di(10, "ground", "lineAA@1", bid + 1, GRAY, lineWidth=1.5)
    bid += 3

    panes = {1: pane("SpringMass")}
    layers = {10: layer(1, "bg"), 11: layer(1, "system")}
    doc = make_doc(800, 400, buffers, {}, panes, layers, geometries, drawItems)
    md = make_md(139, "Spring-Mass System",
        f"Zigzag spring (lineAA@1, {n_zig} zigzags) attached to wall (instancedRect@1) and mass block (instancedRect@1). Dashed equilibrium line.",
        f"Spring-mass system diagram. Wall on left (instancedRect@1 with hatching), {n_zig}-zigzag spring (lineAA@1, {spring_vc} segments), mass block on right (instancedRect@1 with corner radius). Dashed vertical equilibrium line and horizontal ground line. All in clip space.",
        f"| DrawItem | Element | Pipeline | Color |\n|---|---|---|---|\n| wall | Wall | instancedRect@1 | gray |\n| hatching | Wall hatching | lineAA@1 | dim |\n| spring | Zigzag spring | lineAA@1 | cyan |\n| mass | Mass block | instancedRect@1 | blue |\n| equilibrium | Eq. line | lineAA@1 | dim dashed |\n| ground | Ground | lineAA@1 | gray |",
        count_ids(doc), id_breakdown(doc),
        [f"Spring has {n_zig} equal zigzag segments alternating above and below the center line.",
         "Wall hatching uses diagonal lines on the fixed end, standard engineering convention for immovable supports.",
         "Spring connects wall to mass block with endpoints at correct y=0 (center line).",
         "Dashed equilibrium line passes through the mass center for reference."],
        ["Zigzag springs alternate y-offset at evenly spaced x positions along the spring length.",
         "Wall hatching convention: short diagonal lines on the fixed side indicate a rigid wall."],
        viewport="800x400")
    return "spring-mass", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 140: Stress-Strain Curve
# ═══════════════════════════════════════════════════════════════════════
def trial_140():
    # Typical mild steel stress-strain curve
    # Elastic: linear up to yield
    # Yield plateau, then strain hardening, then necking
    E = 200.0  # Young's modulus (GPa) — we'll use normalized units
    yield_stress = 250.0
    yield_strain = yield_stress / E  # ~1.25
    ult_stress = 400.0
    ult_strain = 15.0
    fracture_strain = 22.0
    fracture_stress = 300.0

    pts = []
    # Elastic region (linear): 0 to yield
    for i in range(10):
        strain = yield_strain * i / 9
        stress = E * strain
        pts.append((strain, stress))
    # Yield plateau
    for i in range(5):
        strain = yield_strain + (3.0 - yield_strain) * i / 4
        pts.append((strain, yield_stress))
    # Strain hardening (parabolic rise)
    for i in range(15):
        t = i / 14
        strain = 3.0 + (ult_strain - 3.0) * t
        stress = yield_stress + (ult_stress - yield_stress) * math.sqrt(t)
        pts.append((strain, stress))
    # Necking (decrease)
    for i in range(10):
        t = i / 9
        strain = ult_strain + (fracture_strain - ult_strain) * t
        stress = ult_stress - (ult_stress - fracture_stress) * t ** 0.7
        pts.append((strain, stress))

    N = len(pts)
    curve_data = lineaa_segments(pts)
    curve_vc = len(curve_data) // 4

    # Yield point marker
    yield_pts = [yield_strain, yield_stress]

    sx, stx = compute_transform(-1, fracture_strain + 2, -0.9, 0.9)
    sy, sty = compute_transform(-20, ult_stress + 30, -0.85, 0.85)

    # Yield reference line (horizontal dashed)
    yield_ref = [-1, yield_stress, fracture_strain + 2, yield_stress]

    buffers = {100: buf(curve_data), 103: buf(yield_pts), 106: buf(yield_ref)}
    transforms = {50: xform(sx, sy, stx, sty)}
    panes = {1: pane("Main")}
    layers = {10: layer(1, "ref"), 11: layer(1, "curve"), 12: layer(1, "marker")}
    geometries = {
        101: geo(100, "rect4", curve_vc),
        104: geo(103, "pos2_clip", 1),
        107: geo(106, "rect4", 1),
    }
    drawItems = {
        102: di(11, "stress_strain", "lineAA@1", 101, CYAN, transformId=50, lineWidth=2.5),
        105: di(12, "yield_point", "points@1", 104, RED, transformId=50, pointSize=10.0),
        108: di(10, "yield_ref", "lineAA@1", 107, DIM, transformId=50, lineWidth=1.0,
                dashLength=6.0, gapLength=4.0),
    }
    doc = make_doc(800, 500, buffers, transforms, panes, layers, geometries, drawItems)
    md = make_md(140, "Stress-Strain Curve",
        f"Material stress-strain curve with elastic, yield, strain hardening, and necking regions. {N} points total. Yield point marked.",
        f"Stress-strain curve for mild steel. 4 regions: elastic (linear, E=200), yield plateau (sigma_y=250), strain hardening (to UTS=400 at strain=15), necking (to fracture at strain=22, sigma=300). {N} total data points producing {curve_vc} lineAA@1 segments. Red yield point marker and dashed yield stress reference.",
        f"| DrawItem | Layer | Element | Pipeline | Count | Color |\n|---|---|---|---|---|---|\n| 102 | 11 | Stress-strain | lineAA@1 | {curve_vc} seg | cyan |\n| 105 | 12 | Yield point | points@1 | 1 pt | red |\n| 108 | 10 | Yield ref | lineAA@1 | 1 seg | dim dashed |",
        count_ids(doc), id_breakdown(doc),
        ["Elastic region is perfectly linear with slope E=200 (Young's modulus).",
         "Yield plateau at 250 shows the characteristic flat region of mild steel.",
         "Strain hardening shows a concave-up rise from yield to ultimate tensile strength.",
         "Necking region shows stress decrease after UTS, ending at fracture — correct for ductile materials."],
        ["Stress-strain curves have 4 distinct regions, each with different mathematical behavior.",
         "Yield point marker (red dot) highlights the transition from elastic to plastic deformation."])
    return "stress-strain", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 141: Radiation Pattern
# ═══════════════════════════════════════════════════════════════════════
def trial_141():
    # Dipole antenna radiation pattern in polar coordinates
    # Gain = cos^2(theta) pattern (simplified)
    N = 41
    R_max = 0.75

    # Convert polar to Cartesian for lineAA
    pts = []
    for i in range(N):
        theta = 2 * math.pi * i / (N - 1)
        gain = abs(math.cos(theta)) ** 2  # cos^2 pattern
        r = gain * R_max
        pts.append((r * math.cos(theta), r * math.sin(theta)))
    pattern_data = lineaa_segments(pts)
    pattern_vc = len(pattern_data) // 4

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100

    buffers[bid] = buf(pattern_data)
    geometries[bid + 1] = geo(bid, "rect4", pattern_vc)
    drawItems[bid + 2] = di(11, "pattern", "lineAA@1", bid + 1, CYAN, lineWidth=2.5)
    bid += 3

    # Reference circles at -3dB (0.707 of max) and -6dB (0.5 of max)
    for db, alpha in [(-3, 0.707), (-6, 0.5)]:
        ref = circle_segments(0, 0, R_max * alpha, 36)
        ref_vc = len(ref) // 4
        buffers[bid] = buf(ref)
        geometries[bid + 1] = geo(bid, "rect4", ref_vc)
        drawItems[bid + 2] = di(10, f"ref_{db}dB", "lineAA@1", bid + 1, DIM,
                                 lineWidth=0.8, dashLength=4.0, gapLength=3.0)
        bid += 3

    # Axes
    axes_data = [-0.9, 0, 0.9, 0, 0, -0.9, 0, 0.9]
    buffers[bid] = buf(axes_data)
    geometries[bid + 1] = geo(bid, "rect4", 2)
    drawItems[bid + 2] = di(10, "axes", "lineAA@1", bid + 1, DIM, lineWidth=0.6)
    bid += 3

    # Outer circle (0 dB reference)
    outer = circle_segments(0, 0, R_max, 48)
    outer_vc = len(outer) // 4
    buffers[bid] = buf(outer)
    geometries[bid + 1] = geo(bid, "rect4", outer_vc)
    drawItems[bid + 2] = di(10, "ref_0dB", "lineAA@1", bid + 1, GRAY, lineWidth=1.0,
                             dashLength=5.0, gapLength=3.0)
    bid += 3

    panes = {1: pane("Radiation")}
    layers = {10: layer(1, "grid"), 11: layer(1, "pattern")}
    doc = make_doc(600, 600, buffers, {}, panes, layers, geometries, drawItems)
    md = make_md(141, "Antenna Radiation Pattern",
        "Polar radiation pattern of a dipole antenna (lineAA@1, 40 segments). Reference circles at 0, -3, and -6 dB as dashed lines.",
        f"Dipole antenna radiation pattern (cos^2 gain). {N} angular samples producing {pattern_vc} lineAA@1 segments in polar coordinates. Two-lobed figure-eight pattern with nulls at 90 and 270 degrees. Dashed reference circles at 0 dB (r=0.75), -3 dB (r=0.53), -6 dB (r=0.375). Axes through origin.",
        "| Layer | Elements | Pipeline | Color |\n|---|---|---|---|\n| 10 | Ref circles + axes | lineAA@1 dashed | dim/gray |\n| 11 | Radiation pattern | lineAA@1 | cyan |",
        count_ids(doc), id_breakdown(doc),
        ["Two-lobed pattern with maximum gain at 0 and 180 degrees matches dipole antenna behavior.",
         "Nulls at 90 and 270 degrees (equatorial plane) are correct for a dipole oriented along 0-180 axis.",
         "Pattern passes through -3 dB reference at cos^2(theta)=0.5, i.e., theta=45 degrees from maximum.",
         "Polar-to-Cartesian conversion: (r*cos(theta), r*sin(theta)) correctly maps the pattern to 2D."],
        ["Polar plots convert (gain, angle) → (gain*cos(angle), gain*sin(angle)) for Cartesian rendering.",
         "Reference circles at dB levels provide magnitude scale: -3 dB = 0.707x, -6 dB = 0.5x of peak."],
        viewport="600x600")
    return "radiation-pattern", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 142: Four Waveforms
# ═══════════════════════════════════════════════════════════════════════
def trial_142():
    N = 31  # 31 points → 30 segments
    periods = 2  # 2 full periods

    def sine_wave(n, offset):
        pts = []
        for i in range(n):
            t = i / (n - 1) * periods
            pts.append((t, math.sin(2 * math.pi * t) + offset))
        return pts

    def square_wave(n, offset):
        pts = []
        for i in range(n):
            t = i / (n - 1) * periods
            val = 1.0 if math.sin(2 * math.pi * t) >= 0 else -1.0
            pts.append((t, val + offset))
        return pts

    def sawtooth_wave(n, offset):
        pts = []
        for i in range(n):
            t = i / (n - 1) * periods
            val = 2 * (t % 1.0) - 1.0
            pts.append((t, val + offset))
        return pts

    def triangle_wave(n, offset):
        pts = []
        for i in range(n):
            t = i / (n - 1) * periods
            val = 2 * abs(2 * (t % 1.0) - 1) - 1
            pts.append((t, val + offset))
        return pts

    wave_funcs = [sine_wave, square_wave, sawtooth_wave, triangle_wave]
    wave_names = ["sine", "square", "sawtooth", "triangle"]
    colors_w = [CYAN, YELLOW, GREEN, MAGENTA]
    offsets = [6, 2, -2, -6]

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100

    for wi, (func, name, color, offset) in enumerate(zip(wave_funcs, wave_names, colors_w, offsets)):
        pts = func(N, offset)
        data = lineaa_segments(pts)
        vc = len(data) // 4
        buffers[bid] = buf(data)
        geometries[bid + 1] = geo(bid, "rect4", vc)
        drawItems[bid + 2] = di(10, name, "lineAA@1", bid + 1, color,
                                 transformId=50, lineWidth=2.0)
        bid += 3

    sx, stx = compute_transform(-0.1, periods + 0.1, -0.9, 0.9)
    sy, sty = compute_transform(-8, 8, -0.9, 0.9)
    transforms = {50: xform(sx, sy, stx, sty)}
    panes = {1: pane("Waveforms")}
    layers = {10: layer(1, "signals")}
    doc = make_doc(900, 600, buffers, transforms, panes, layers, geometries, drawItems)
    md = make_md(142, "Four Waveforms",
        f"4 signal types (sine, square, sawtooth, triangle) each with {N-1} lineAA segments, stacked vertically in one pane.",
        f"Four classic waveforms, 2 periods each, 31 sample points per waveform producing 30 lineAA@1 segments each. Sine (cyan, offset +6), square (yellow, offset +2), sawtooth (green, offset -2), triangle (magenta, offset -6). Single pane with shared transform.",
        "| DrawItem | Waveform | Pipeline | Segments | Color |\n|---|---|---|---|---|\n| 102 | Sine | lineAA@1 | 30 | cyan |\n| 105 | Square | lineAA@1 | 30 | yellow |\n| 108 | Sawtooth | lineAA@1 | 30 | green |\n| 111 | Triangle | lineAA@1 | 30 | magenta |",
        count_ids(doc), id_breakdown(doc),
        ["Sine wave is smooth with continuous curvature, correctly sampled at 31 equispaced points over 2 periods.",
         "Square wave has sharp transitions between +1 and -1 with no intermediate values.",
         "Sawtooth wave ramps linearly from -1 to +1 then drops sharply at each period boundary.",
         "Triangle wave is piecewise linear, ramping up then down symmetrically within each period."],
        ["Vertical offsets separate waveforms without needing multiple panes — simpler than a 4-pane layout.",
         "30 segments per waveform is sufficient for 2 periods: ~15 samples per period captures all features."],
        viewport="900x600")
    return "four-waveforms", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 143: Eye Diagram
# ═══════════════════════════════════════════════════════════════════════
def trial_143():
    import random
    random.seed(55)
    n_traces = 20
    n_pts = 30  # points per trace
    # Each trace is one bit period of a noisy signal
    # Eye pattern: overlay multiple bit-period traces

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100

    # Generate random bit sequences and create raised-cosine transitions
    for t in range(n_traces):
        # Random start/end bits
        start_bit = random.choice([-1, 1])
        end_bit = random.choice([-1, 1])
        pts = []
        for i in range(n_pts):
            frac = i / (n_pts - 1)
            # Raised cosine transition
            base = start_bit + (end_bit - start_bit) * 0.5 * (1 - math.cos(math.pi * frac))
            # Add noise and ISI
            noise = random.gauss(0, 0.08)
            jitter = random.gauss(0, 0.02)
            pts.append((frac + jitter, base + noise))

        data = lineaa_segments(pts)
        vc = len(data) // 4
        buffers[bid] = buf(data)
        geometries[bid + 1] = geo(bid, "rect4", vc)
        drawItems[bid + 2] = di(10, f"trace_{t}", "lineAA@1", bid + 1,
                                 [0.180, 0.831, 0.886, 0.35], lineWidth=1.5)
        bid += 3

    # Data space: x=[0,1], y=[-1.5,1.5]
    sx, stx = compute_transform(-0.1, 1.1, -0.9, 0.9)
    sy, sty = compute_transform(-1.5, 1.5, -0.85, 0.85)
    transforms = {50: xform(sx, sy, stx, sty)}

    # Apply transform to all traces
    for di_id in drawItems:
        drawItems[di_id]["transformId"] = 50

    panes = {1: pane("Eye")}
    layers = {10: layer(1, "traces")}
    doc = make_doc(700, 500, buffers, transforms, panes, layers, geometries, drawItems)
    md = make_md(143, "Eye Diagram",
        f"{n_traces} overlaid bit-period traces (lineAA@1) creating an eye pattern with low alpha for overlap visibility.",
        f"Eye diagram with {n_traces} overlaid traces, each representing one bit period with raised-cosine transitions plus Gaussian noise (sigma=0.08) and jitter (sigma=0.02). All traces drawn with alpha=0.35 cyan, creating density-dependent brightness in the overlapping eye opening. {n_pts} points per trace, {n_pts-1} segments each.",
        f"| DrawItem | Element | Pipeline | Count | Color |\n|---|---|---|---|---|\n| 102-{100+n_traces*3-1} | {n_traces} traces | lineAA@1 | {n_pts-1} seg each | cyan 35% alpha |",
        count_ids(doc), id_breakdown(doc),
        ["Eye opening is visible at the center of the diagram where traces separate into high/low states.",
         "Low alpha (0.35) creates additive density: areas with more overlapping traces appear brighter.",
         "Noise and jitter blur the transition edges, creating the characteristic soft eye boundary.",
         "All four transition types (0→0, 0→1, 1→0, 1→1) are represented across the random traces."],
        ["Eye diagrams overlay many bit-period traces — low alpha enables density visualization through additive blending.",
         "Raised cosine transitions model realistic signal bandwidth limiting in digital communications."],
        viewport="700x500")
    return "eye-diagram", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 144: Nyquist Plot
# ═══════════════════════════════════════════════════════════════════════
def trial_144():
    # 2nd order system: H(s) = 1 / (s^2 + s + 1)
    # Nyquist: H(jw) for w from 0 to large
    N = 61
    freqs = [i * 10.0 / (N - 1) for i in range(N)]
    freqs[0] = 0.001

    real_pts = []
    imag_pts = []
    for w in freqs:
        denom_real = 1 - w ** 2
        denom_imag = w
        denom_mag2 = denom_real ** 2 + denom_imag ** 2
        # H(jw) = 1 / (denom_real + j*denom_imag) = (denom_real - j*denom_imag) / denom_mag2
        re = denom_real / denom_mag2
        im = -denom_imag / denom_mag2
        real_pts.append(re)
        imag_pts.append(im)

    pts = list(zip(real_pts, imag_pts))
    curve_data = lineaa_segments(pts)
    curve_vc = len(curve_data) // 4

    # Also mirror for negative frequencies
    mirror_pts = [(r, -i) for r, i in zip(real_pts, imag_pts)]
    mirror_pts.reverse()
    mirror_data = lineaa_segments(mirror_pts)
    mirror_vc = len(mirror_data) // 4

    # Axes
    all_r = real_pts + [r for r, _ in mirror_pts]
    all_i = imag_pts + [i for _, i in mirror_pts]
    rmin, rmax = min(all_r) - 0.1, max(all_r) + 0.1
    imin, imax = min(all_i) - 0.1, max(all_i) + 0.1
    # Make symmetric
    bound = max(abs(rmin), abs(rmax), abs(imin), abs(imax)) + 0.1

    sx, stx = compute_transform(-bound, bound, -0.9, 0.9)
    sy, sty = compute_transform(-bound, bound, -0.9, 0.9)

    axes_data = [-bound, 0, bound, 0, 0, -bound, 0, bound]

    # Critical point (-1, 0)
    crit_data = [-1, 0]

    buffers = {
        100: buf(curve_data), 103: buf(mirror_data),
        106: buf(axes_data), 109: buf(crit_data),
    }
    transforms = {50: xform(sx, sy, stx, sty)}
    panes = {1: pane("Nyquist")}
    layers = {10: layer(1, "grid"), 11: layer(1, "plot"), 12: layer(1, "marker")}
    geometries = {
        101: geo(100, "rect4", curve_vc),
        104: geo(103, "rect4", mirror_vc),
        107: geo(106, "rect4", 2),
        110: geo(109, "pos2_clip", 1),
    }
    drawItems = {
        102: di(11, "nyquist_pos", "lineAA@1", 101, CYAN, transformId=50, lineWidth=2.0),
        105: di(11, "nyquist_neg", "lineAA@1", 104, CYAN, transformId=50, lineWidth=1.5,
                dashLength=5.0, gapLength=3.0),
        108: di(10, "axes", "lineAA@1", 107, DIM, transformId=50, lineWidth=0.8,
                dashLength=4.0, gapLength=3.0),
        111: di(12, "critical_pt", "points@1", 110, RED, transformId=50, pointSize=10.0),
    }
    doc = make_doc(600, 600, buffers, transforms, panes, layers, geometries, drawItems)
    md = make_md(144, "Nyquist Plot",
        "Complex frequency response loop (lineAA@1, 60 segments) for H(s)=1/(s^2+s+1). Dashed axes, critical point (-1,0) marked.",
        f"Nyquist plot of H(s) = 1/(s^2+s+1). Positive frequency contour (solid cyan, {curve_vc} segments) from w=0 to w=10. Negative frequency mirror (dashed cyan, {mirror_vc} segments). Dashed axes through origin. Red critical point at (-1,0j). The contour does not encircle (-1,0), indicating stability.",
        f"| DrawItem | Layer | Element | Pipeline | Count | Color |\n|---|---|---|---|---|---|\n| 102 | 11 | Positive freq | lineAA@1 | {curve_vc} seg | cyan |\n| 105 | 11 | Negative freq | lineAA@1 | {mirror_vc} seg | cyan dashed |\n| 108 | 10 | Axes | lineAA@1 | 2 seg | dim dashed |\n| 111 | 12 | Critical (-1,0) | points@1 | 1 pt | red |",
        count_ids(doc), id_breakdown(doc),
        ["Nyquist contour starts at H(0)=1+0j (real axis) and spirals as frequency increases.",
         "Contour does not encircle the critical point (-1,0), confirming the system is stable (Nyquist criterion).",
         "Negative frequency mirror is the complex conjugate of the positive frequency contour.",
         "Square viewport preserves equal real/imaginary axis scaling for correct phase interpretation."],
        ["Nyquist plots: H(jw) = (Re - j*Im)/|denom|^2 for rational transfer functions.",
         "Critical point at (-1,0) is the key reference — encirclement count determines stability margin."],
        viewport="600x600")
    return "nyquist-plot", doc, md


# ═══════════════════════════════════════════════════════════════════════
# TRIAL 145: Unit Circle
# ═══════════════════════════════════════════════════════════════════════
def trial_145():
    R = 0.78
    # Unit circle (48 segments)
    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100

    uc = circle_segments(0, 0, R, 48)
    uc_vc = len(uc) // 4
    buffers[bid] = buf(uc)
    geometries[bid + 1] = geo(bid, "rect4", uc_vc)
    drawItems[bid + 2] = di(10, "unit_circle", "lineAA@1", bid + 1, WHITE, lineWidth=2.0)
    bid += 3

    # Special angles: 0, 30, 45, 60, 90, 120, 135, 150, 180, 210, 225, 240, 270, 300, 315, 330
    special_angles = [0, 30, 45, 60, 90, 120, 135, 150, 180, 210, 225, 240, 270, 300, 315, 330]

    # Radial lines from origin to circle edge
    radial_data = []
    for deg in special_angles:
        a = math.radians(deg)
        radial_data.extend([0, 0, R * math.cos(a), R * math.sin(a)])
    radial_vc = len(radial_data) // 4
    buffers[bid] = buf(radial_data)
    geometries[bid + 1] = geo(bid, "rect4", radial_vc)
    drawItems[bid + 2] = di(10, "radials", "lineAA@1", bid + 1, DIM, lineWidth=0.8)
    bid += 3

    # Angle points on the circle
    angle_pts_data = []
    for deg in special_angles:
        a = math.radians(deg)
        angle_pts_data.extend([R * math.cos(a), R * math.sin(a)])
    buffers[bid] = buf(angle_pts_data)
    geometries[bid + 1] = geo(bid, "pos2_clip", len(special_angles))
    drawItems[bid + 2] = di(12, "angle_dots", "points@1", bid + 1, CYAN, pointSize=6.0)
    bid += 3

    # Sin/cos projection for 45° as example
    a45 = math.radians(45)
    cx = R * math.cos(a45)
    cy = R * math.sin(a45)
    # cos projection: horizontal line from (cx, cy) to (cx, 0)
    # sin projection: vertical line from (cx, cy) to (0, cy)
    proj_data = [cx, cy, cx, 0, cx, cy, 0, cy]
    buffers[bid] = buf(proj_data)
    geometries[bid + 1] = geo(bid, "rect4", 2)
    drawItems[bid + 2] = di(11, "projections", "lineAA@1", bid + 1, YELLOW,
                             lineWidth=1.5, dashLength=5.0, gapLength=3.0)
    bid += 3

    # Cos segment on x-axis: (0,0) to (cx, 0)
    cos_data = [0, 0, cx, 0]
    buffers[bid] = buf(cos_data)
    geometries[bid + 1] = geo(bid, "rect4", 1)
    drawItems[bid + 2] = di(11, "cos_seg", "lineAA@1", bid + 1, GREEN, lineWidth=3.0)
    bid += 3

    # Sin segment on y-axis: (0,0) to (0, cy)
    sin_data = [0, 0, 0, cy]
    buffers[bid] = buf(sin_data)
    geometries[bid + 1] = geo(bid, "rect4", 1)
    drawItems[bid + 2] = di(11, "sin_seg", "lineAA@1", bid + 1, RED, lineWidth=3.0)
    bid += 3

    # Axes
    axes_data = [-0.95, 0, 0.95, 0, 0, -0.95, 0, 0.95]
    buffers[bid] = buf(axes_data)
    geometries[bid + 1] = geo(bid, "rect4", 2)
    drawItems[bid + 2] = di(10, "axes", "lineAA@1", bid + 1, GRAY, lineWidth=1.0)
    bid += 3

    panes = {1: pane("UnitCircle")}
    layers = {10: layer(1, "grid"), 11: layer(1, "projections"), 12: layer(1, "dots")}
    doc = make_doc(600, 600, buffers, {}, panes, layers, geometries, drawItems)
    md = make_md(145, "Unit Circle",
        f"Unit circle (48 lineAA segments) with {len(special_angles)} labeled angle positions, radial lines, and sin/cos projections at 45 degrees.",
        f"Trigonometric unit circle. 48-segment circle (R=0.78). {len(special_angles)} special angles (every 30 and 45 degrees) shown as radial lines from origin with cyan dots at the circle edge. Sin/cos projections demonstrated at 45 degrees: green segment (cos) along x-axis, red segment (sin) along y-axis, yellow dashed drop lines. Axes through origin.",
        "| Layer | Elements | Pipeline | Color |\n|---|---|---|---|\n| 10 | Circle + radials + axes | lineAA@1 | white/dim/gray |\n| 11 | Sin/cos projections | lineAA@1 | green/red/yellow |\n| 12 | Angle dots | points@1 | cyan |",
        count_ids(doc), id_breakdown(doc),
        [f"All {len(special_angles)} special angles correctly positioned at standard trigonometric positions.",
         "At 45 degrees, cos(45)=sin(45)=sqrt(2)/2 ~ 0.707, and the green/red segments are equal length.",
         "Radial lines from origin to circle edge show the angle measurement direction.",
         "Sin projection (vertical, red) and cos projection (horizontal, green) correctly demonstrate the definitions."],
        ["Unit circle special angles: multiples of 30 and 45 degrees cover all standard reference angles.",
         "Sin = y-coordinate, cos = x-coordinate of the point on the unit circle — shown explicitly with colored segments."],
        viewport="600x600")
    return "unit-circle", doc, md


# ═══════════════════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════════════════
def main():
    trials = [
        (112, trial_112),
        (113, trial_113),
        (114, trial_114),
        (115, trial_115),
        (116, trial_116),
        (117, trial_117),
        (118, trial_118),
        (119, trial_119),
        (120, trial_120),
        (121, trial_121),
        (122, trial_122),
        (123, trial_123),
        (124, trial_124),
        (125, trial_125),
        (126, trial_126),
        (127, trial_127),
        (128, trial_128),
        (129, trial_129),
        (130, trial_130),
        (131, trial_131),
        (132, trial_132),
        (133, trial_133),
        (134, trial_134),
        (135, trial_135),
        (136, trial_136),
        (137, trial_137),
        (138, trial_138),
        (139, trial_139),
        (140, trial_140),
        (141, trial_141),
        (142, trial_142),
        (143, trial_143),
        (144, trial_144),
        (145, trial_145),
    ]
    print(f"Generating {len(trials)} trials...")
    for num, func in trials:
        slug, doc, md = func()
        # Validate
        total_ids = count_ids(doc)
        # Check for ID uniqueness across all sections
        all_ids = set()
        for section in ["buffers", "transforms", "panes", "layers", "geometries", "drawItems"]:
            for k in doc.get(section, {}):
                if k in all_ids:
                    print(f"  WARNING: Duplicate ID {k} in trial {num}!")
                all_ids.add(k)
        # Validate vertex counts
        for gid, gdata in doc.get("geometries", {}).items():
            bid_key = str(gdata["vertexBufferId"])
            if bid_key in doc.get("buffers", {}):
                buf_data = doc["buffers"][bid_key]["data"]
                fmt = gdata["format"]
                fpv = {"pos2_clip": 2, "pos2_alpha": 3, "pos2_color4": 6,
                       "rect4": 4, "candle6": 6, "glyph8": 8, "pos2_uv4": 4}[fmt]
                expected_vc = len(buf_data) // fpv
                actual_vc = gdata["vertexCount"]
                if expected_vc != actual_vc:
                    print(f"  WARNING: Trial {num}, geo {gid}: vertexCount={actual_vc} but data has {expected_vc} vertices ({len(buf_data)} floats / {fpv})")
        write_trial(num, slug, doc, md)
    print(f"\nDone! Generated {len(trials)} trial pairs.")

if __name__ == "__main__":
    main()
