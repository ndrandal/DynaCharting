#!/usr/bin/env python3
"""Generate trials 178–211 for DynaCharting.

Each trial produces:
  - NNN-slug.json  (SceneDocument)
  - NNN-slug.md    (audit stub)

Run from the docs/trials/ directory.
"""

import json, math, os, random, textwrap
from typing import Any

OUT_DIR = os.path.dirname(os.path.abspath(__file__))

# ---------- deterministic seed ---------
random.seed(42)

# ---------- palette helpers ----------
DARK_BG = [0.06, 0.09, 0.16, 1.0]

def rgb(r, g, b, a=1.0):
    return [round(r, 4), round(g, 4), round(b, 4), round(a, 4)]

def viridis(t):
    """Attempt at viridis-like color mapping for t in [0,1]."""
    t = max(0.0, min(1.0, t))
    r = max(0, min(1, 0.267004 + t*(0.003299 + t*(-0.227411 + t*(2.735523 + t*(-3.021018 + t*1.242891))))))
    g = max(0, min(1, 0.004874 + t*(1.015861 + t*(0.167536 + t*(-2.651943 + t*(3.711772 + t*-1.248491))))))
    b = max(0, min(1, 0.329415 + t*(1.242891 + t*(-4.634700 + t*(8.461060 + t*(-6.915887 + t*2.081524))))))
    return rgb(r, g, b)

def hsl_to_rgb(h, s, l):
    """h in [0,360], s,l in [0,1]"""
    c = (1 - abs(2*l - 1)) * s
    x = c * (1 - abs((h/60) % 2 - 1))
    m = l - c/2
    if h < 60: r,g,b = c,x,0
    elif h < 120: r,g,b = x,c,0
    elif h < 180: r,g,b = 0,c,x
    elif h < 240: r,g,b = 0,x,c
    elif h < 300: r,g,b = x,0,c
    else: r,g,b = c,0,x
    return rgb(r+m, g+m, b+m)

def distinct_colors(n, s=0.7, l=0.55):
    return [hsl_to_rgb(i * 360.0 / n, s, l) for i in range(n)]

# ---------- geometry helpers ----------
def circle_fan(cx, cy, r, n=32):
    """Generate triangles for a filled circle using center-fan. Returns flat list of pos2_clip floats."""
    verts = []
    for i in range(n):
        a0 = 2 * math.pi * i / n
        a1 = 2 * math.pi * (i + 1) / n
        verts.extend([cx, cy,
                      cx + r * math.cos(a0), cy + r * math.sin(a0),
                      cx + r * math.cos(a1), cy + r * math.sin(a1)])
    return verts

def wedge(cx, cy, r_inner, r_outer, a_start, a_end, n_seg=8):
    """Generate triangles for a wedge/arc sector. Returns pos2_clip floats."""
    verts = []
    for i in range(n_seg):
        a0 = a_start + (a_end - a_start) * i / n_seg
        a1 = a_start + (a_end - a_start) * (i + 1) / n_seg
        cos0, sin0 = math.cos(a0), math.sin(a0)
        cos1, sin1 = math.cos(a1), math.sin(a1)
        # Two triangles: inner0, outer0, outer1 and inner0, outer1, inner1
        ix0, iy0 = cx + r_inner * cos0, cy + r_inner * sin0
        ox0, oy0 = cx + r_outer * cos0, cy + r_outer * sin0
        ix1, iy1 = cx + r_inner * cos1, cy + r_inner * sin1
        ox1, oy1 = cx + r_outer * cos1, cy + r_outer * sin1
        verts.extend([ix0, iy0, ox0, oy0, ox1, oy1])
        verts.extend([ix0, iy0, ox1, oy1, ix1, iy1])
    return verts

def round_data(data, digits=5):
    return [round(v, digits) for v in data]

# ---------- SceneDocument builder ----------
class DocBuilder:
    def __init__(self, width=960, height=640):
        self.doc = {
            "version": 1,
            "viewport": {"width": width, "height": height},
            "buffers": {},
            "transforms": {},
            "panes": {},
            "layers": {},
            "geometries": {},
            "drawItems": {}
        }
        self._next_id = 100  # for buf/geo/di groups

    def add_pane(self, pid, name, ymin, ymax, xmin=-0.95, xmax=0.95, bg=None):
        self.doc["panes"][str(pid)] = {
            "name": name,
            "region": {"clipYMin": ymin, "clipYMax": ymax, "clipXMin": xmin, "clipXMax": xmax},
            "hasClearColor": True,
            "clearColor": bg or DARK_BG[:]
        }

    def add_layer(self, lid, pane_id, name):
        self.doc["layers"][str(lid)] = {"paneId": pane_id, "name": name}

    def add_transform(self, tid, sx=1, sy=1, tx=0, ty=0):
        self.doc["transforms"][str(tid)] = {"sx": sx, "sy": sy, "tx": tx, "ty": ty}

    def alloc_ids(self):
        """Return (buf_id, geo_id, di_id) triple and advance counter."""
        b = self._next_id
        self._next_id += 3
        return b, b+1, b+2

    def add_draw(self, layer_id, name, pipeline, fmt, data, color,
                 transform_id=None, line_width=None, point_size=None,
                 dash_length=None, gap_length=None, corner_radius=None,
                 color_up=None, color_down=None, blend_mode=None,
                 visible=None):
        """Add buffer+geometry+drawItem group. Returns drawItem id."""
        buf_id, geo_id, di_id = self.alloc_ids()
        floats_per_vtx = {"pos2_clip": 2, "pos2_alpha": 3, "pos2_color4": 6,
                          "rect4": 4, "candle6": 6, "glyph8": 8, "pos2_uv4": 4}[fmt]
        data_rounded = round_data(data)
        vtx_count = len(data_rounded) // floats_per_vtx
        self.doc["buffers"][str(buf_id)] = {"data": data_rounded}
        self.doc["geometries"][str(geo_id)] = {
            "vertexBufferId": buf_id, "format": fmt, "vertexCount": vtx_count
        }
        di = {
            "layerId": layer_id, "name": name, "pipeline": pipeline,
            "geometryId": geo_id, "color": color
        }
        if transform_id is not None: di["transformId"] = transform_id
        if line_width is not None: di["lineWidth"] = line_width
        if point_size is not None: di["pointSize"] = point_size
        if dash_length is not None: di["dashLength"] = dash_length
        if gap_length is not None: di["gapLength"] = gap_length
        if corner_radius is not None: di["cornerRadius"] = corner_radius
        if color_up is not None: di["colorUp"] = color_up
        if color_down is not None: di["colorDown"] = color_down
        if blend_mode is not None: di["blendMode"] = blend_mode
        if visible is not None: di["visible"] = visible
        self.doc["drawItems"][str(di_id)] = di
        return di_id

    def build(self):
        return self.doc


def make_md(number, slug, title, goal, what_built, entity_counts, data_notes):
    """Generate markdown audit stub."""
    return textwrap.dedent(f"""\
    # Trial {number:03d}: {title}

    **Date:** 2026-03-22
    **Goal:** {goal}
    **Outcome:** Pending audit.

    ---

    ## What Was Built

    {what_built}

    ## Entity Counts

    {entity_counts}

    ## Data Notes

    {data_notes}
    """)

# ===================================================================
# Individual trial generators
# ===================================================================

def trial_178():
    """500 scatter points in 3 Gaussian clusters."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "points")
    d.add_transform(50)

    random.seed(178)
    # 3 clusters with centers and sigmas
    clusters = [
        (2.0, 5.0, 1.0, 1.2, 180, rgb(0.2, 0.6, 1.0)),   # blue
        (7.0, 3.0, 0.8, 0.9, 160, rgb(1.0, 0.3, 0.3)),   # red
        (5.5, 8.0, 1.2, 0.7, 160, rgb(0.3, 0.9, 0.3)),   # green
    ]
    all_pts = []
    total = 0
    for cx, cy, sx, sy, n, col in clusters:
        for _ in range(n):
            x = random.gauss(cx, sx)
            y = random.gauss(cy, sy)
            all_pts.extend([x, y])
            total += 1
    assert total == 500

    # data range approx -2..12 x -2..12
    d.add_transform(50, sx=0.19, sy=0.19, tx=-0.9, ty=-0.9)

    # We need three separate drawItems for colors
    for i, (cx, cy, sx, sy, n, col) in enumerate(clusters):
        pts = []
        random.seed(178 + i * 1000)  # deterministic per cluster
        for _ in range(n):
            x = random.gauss(cx, sx)
            y = random.gauss(cy, sy)
            pts.extend([x, y])
        d.add_draw(10, f"cluster{i}", "points@1", "pos2_clip", pts, col,
                   transform_id=50, point_size=3.0)

    md = make_md(178, "dense-scatter-500",
        "Dense Scatter Plot with 500 Points",
        "500 scatter points in 3 Gaussian-distributed color clusters.",
        "A 960x640 viewport with 500 points distributed across 3 Gaussian clusters "
        "(blue at ~(2,5), red at ~(7,3), green at ~(5.5,8)). Each cluster rendered as "
        "a separate DrawItem with pointSize=3. Transform maps data range ~[0,10] to clip space.",
        "1 pane, 1 layer, 1 transform, 3 buffers, 3 geometries, 3 drawItems (180+160+160 points).",
        "Gaussian random with seed=178. Cluster sizes: 180, 160, 160 = 500 total.")
    return "178-dense-scatter-500", d.build(), md


def trial_179():
    """20 overlaid line series, 25 points each."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "lines")
    d.add_transform(50, sx=0.076, sy=0.08, tx=-0.9, ty=0.0)

    random.seed(179)
    colors = distinct_colors(20)
    for i in range(20):
        segs = []
        y = random.uniform(-3, 3)
        xs = []
        ys = [y]
        for j in range(24):
            y += random.gauss(0, 0.5)
            ys.append(y)
        for j in range(24):
            segs.extend([j, ys[j], j+1, ys[j+1]])
        d.add_draw(10, f"line{i}", "lineAA@1", "rect4", segs, colors[i],
                   transform_id=50, line_width=1.5)

    md = make_md(179, "multi-line-20",
        "20 Overlaid Line Series",
        "20 line series with 25 data points each, overlaid in semi-random walk patterns.",
        "A 960x640 viewport with 20 lineAA lines, each with 24 segments (25 points). "
        "Each line has a distinct hue from the HSL color wheel. Data-space transform maps "
        "x=[0,24], y~[-8,8] into the pane.",
        "1 pane, 1 layer, 1 transform, 20 buffers, 20 geometries, 20 drawItems.",
        "Random walk with Gaussian steps (sigma=0.5). Seed=179. 20 distinct colors via HSL.")
    return "179-multi-line-20", d.build(), md


def trial_180():
    """20x20 heatmap grid with viridis-like coloring."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "heatmap")

    # 400 rects colored individually — need triGradient or separate drawItems
    # Best approach: use instancedRect@1 with groups by color bucket
    # Simpler: use one drawItem per distinct value with triGradient for vertex color
    # Actually simplest: batch all 400 rects, use triGradient with per-vertex color
    # triGradient: 6 floats per vertex (x,y,r,g,b,a), 6 verts per rect (2 tris)

    data = []
    for iy in range(20):
        for ix in range(20):
            val = (math.sin(ix * 0.4) * math.cos(iy * 0.3) + 1) / 2.0  # [0,1]
            col = viridis(val)
            # rect corners in clip space
            margin = 0.005
            x0 = -0.9 + ix * 0.09 + margin
            x1 = -0.9 + (ix + 1) * 0.09 - margin
            y0 = -0.9 + iy * 0.09 + margin
            y1 = -0.9 + (iy + 1) * 0.09 - margin
            # 2 triangles, 6 vertices, each = x,y,r,g,b,a
            r, g, b, a = col
            data.extend([x0,y0,r,g,b,a, x1,y0,r,g,b,a, x1,y1,r,g,b,a])
            data.extend([x0,y0,r,g,b,a, x1,y1,r,g,b,a, x0,y1,r,g,b,a])

    d.add_draw(10, "heatmap", "triGradient@1", "pos2_color4", data,
               rgb(1, 1, 1))

    md = make_md(180, "heatmap-20x20",
        "20x20 Heatmap Grid",
        "400-cell heatmap with viridis-like color scale based on sin(x)*cos(y).",
        "A 960x640 viewport with 400 rectangles (20x20 grid) rendered as triGradient@1. "
        "Each cell colored per a viridis approximation of sin(x*0.4)*cos(y*0.3) mapped to [0,1]. "
        "Each rect = 2 triangles = 6 vertices in pos2_color4 format.",
        "1 pane, 1 layer, 0 transforms, 1 buffer (400*6*6=14400 floats), 1 geometry (2400 verts), 1 drawItem.",
        "Color value = (sin(ix*0.4)*cos(iy*0.3)+1)/2 through a viridis approximation.")
    return "180-heatmap-20x20", d.build(), md


def trial_181():
    """100 vertical bars with damped sine wave heights."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "bars")
    d.add_transform(50, sx=0.0186, sy=0.9, tx=-0.93, ty=-0.95)

    data = []
    for i in range(100):
        h = 0.5 + 0.5 * math.exp(-i * 0.02) * math.sin(i * 0.15)
        x0 = i - 0.35
        x1 = i + 0.35
        y0 = 0.0
        y1 = h
        data.extend([x0, y0, x1, y1])

    # Alternating blue/cyan
    # Need two drawItems: even=blue, odd=cyan
    even_data = []
    odd_data = []
    for i in range(100):
        h = 0.5 + 0.5 * math.exp(-i * 0.02) * math.sin(i * 0.15)
        x0 = i - 0.35
        x1 = i + 0.35
        y0 = 0.0
        y1 = h
        if i % 2 == 0:
            even_data.extend([x0, y0, x1, y1])
        else:
            odd_data.extend([x0, y0, x1, y1])

    d.add_draw(10, "barsBlue", "instancedRect@1", "rect4", even_data,
               rgb(0.2, 0.4, 0.9), transform_id=50)
    d.add_draw(10, "barsCyan", "instancedRect@1", "rect4", odd_data,
               rgb(0.2, 0.8, 0.9), transform_id=50)

    md = make_md(181, "bar-chart-100",
        "100-Bar Chart with Damped Sine Heights",
        "100 vertical bars with alternating blue/cyan, heights following a damped sine wave.",
        "A 960x640 viewport with 100 bars (50 blue, 50 cyan). Heights follow "
        "h = 0.5 + 0.5*exp(-i*0.02)*sin(i*0.15). Transform maps x=[0,100], y=[0,1] to clip.",
        "1 pane, 1 layer, 1 transform, 2 buffers, 2 geometries, 2 drawItems.",
        "Damped sine: exponential decay envelope modulating a sine wave. 50 bars each color.")
    return "181-bar-chart-100", d.build(), md


def trial_182():
    """5-series stacked area chart with 20 points each."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "area")
    d.add_transform(50, sx=0.095, sy=0.36, tx=-0.9, ty=-0.9)

    random.seed(182)
    n_pts = 20
    n_series = 5
    colors = [rgb(0.2, 0.5, 0.9, 0.8), rgb(0.9, 0.3, 0.3, 0.8),
              rgb(0.3, 0.8, 0.3, 0.8), rgb(0.9, 0.7, 0.1, 0.8),
              rgb(0.7, 0.3, 0.9, 0.8)]

    # Generate series values
    series = []
    for s in range(n_series):
        vals = [random.uniform(0.3, 1.0) for _ in range(n_pts)]
        series.append(vals)

    # Compute cumulative sums for stacking
    cumulative = [[0.0] * n_pts]
    for s in range(n_series):
        prev = cumulative[-1]
        new = [prev[i] + series[s][i] for i in range(n_pts)]
        cumulative.append(new)

    # Generate triangle strips for each series (bottom to top)
    for s in range(n_series):
        tris = []
        for i in range(n_pts - 1):
            x0 = i
            x1 = i + 1
            bot0 = cumulative[s][i]
            top0 = cumulative[s + 1][i]
            bot1 = cumulative[s][i + 1]
            top1 = cumulative[s + 1][i + 1]
            # Two triangles
            tris.extend([x0, bot0, x1, bot1, x1, top1])
            tris.extend([x0, bot0, x1, top1, x0, top0])
        d.add_draw(10, f"area{s}", "triSolid@1", "pos2_clip", tris, colors[s],
                   transform_id=50)

    md = make_md(182, "stacked-area-5",
        "5-Series Stacked Area Chart",
        "5 stacked area series with 20 data points each, rendered as triangle strips.",
        "A 960x640 viewport with 5 area series stacked bottom-to-top. Each series is "
        "triSolid@1 triangulated from cumulative sums. 19 quads (38 triangles, 114 vertices) per series. "
        "Transform maps x=[0,19], y=[0,~5] to clip space.",
        "1 pane, 1 layer, 1 transform, 5 buffers, 5 geometries, 5 drawItems.",
        "Random values [0.3,1.0] per point. Cumulative stacking. Seed=182.")
    return "182-stacked-area-5", d.build(), md


def trial_183():
    """100 OHLC candlesticks."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "candles")
    d.add_transform(50, sx=0.0186, sy=0.035, tx=-0.93, ty=-0.8)

    random.seed(183)
    data = []
    price = 100.0
    for i in range(100):
        o = price
        change = random.gauss(0, 2)
        c = o + change
        h = max(o, c) + random.uniform(0, 2)
        l = min(o, c) - random.uniform(0, 2)
        hw = 0.35
        data.extend([i, o, h, l, c, hw])
        price = c + random.gauss(0, 0.5)

    d.add_draw(10, "candles", "instancedCandle@1", "candle6", data,
               rgb(0.5, 0.5, 0.5),
               transform_id=50,
               color_up=rgb(0.18, 0.8, 0.35),
               color_down=rgb(0.9, 0.2, 0.2))

    md = make_md(183, "dense-candles-100",
        "100 OHLC Candlesticks",
        "100 candlesticks with simulated random-walk price data, green up / red down.",
        "A 960x640 viewport with 100 candlesticks (instancedCandle@1). Price starts at 100 "
        "with Gaussian random walk. Green for close>=open, red for close<open. Half-width=0.35.",
        "1 pane, 1 layer, 1 transform, 1 buffer (600 floats), 1 geometry (100 verts), 1 drawItem.",
        "Random walk: gauss(0,2) per-bar change. High/low add uniform(0,2) beyond open/close. Seed=183.")
    return "183-dense-candles-100", d.build(), md


def trial_184():
    """6 categories x 25 dots horizontal dot plot."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "dots")
    d.add_transform(50, sx=0.08, sy=0.27, tx=-0.9, ty=-0.8)

    random.seed(184)
    colors = distinct_colors(6)
    for cat in range(6):
        pts = []
        for j in range(25):
            x = random.gauss(5 + cat * 1.5, 1.5)
            y = cat + random.uniform(-0.05, 0.05)
            pts.extend([x, y])
        d.add_draw(10, f"cat{cat}", "points@1", "pos2_clip", pts, colors[cat],
                   transform_id=50, point_size=4.0)

    md = make_md(184, "dot-plot-horizontal",
        "Horizontal Dot Plot (6x25)",
        "6 categories x 25 dots each = 150 points, arranged horizontally per category row.",
        "A 960x640 viewport with 150 points across 6 horizontal category rows. Each category "
        "has 25 Gaussian-distributed points along x, with minimal y jitter. pointSize=4. "
        "Each category is a separate DrawItem with a distinct color.",
        "1 pane, 1 layer, 1 transform, 6 buffers, 6 geometries, 6 drawItems.",
        "X values: gauss(5+cat*1.5, 1.5). Y jitter: uniform(-0.05, 0.05). Seed=184.")
    return "184-dot-plot-horizontal", d.build(), md


def trial_185():
    """5 categories, 30 points each, strip plot with jitter."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "strips")
    d.add_transform(50, sx=0.18, sy=0.09, tx=-0.9, ty=-0.85)

    random.seed(185)
    colors = distinct_colors(5, s=0.8, l=0.6)
    for cat in range(5):
        pts = []
        base_y = cat * 2.0
        for j in range(30):
            x = random.gauss(cat + 2, 0.8)
            y = base_y + random.gauss(0, 0.3)
            pts.extend([x, y])
        d.add_draw(10, f"strip{cat}", "points@1", "pos2_clip", pts, colors[cat],
                   transform_id=50, point_size=3.0)

    md = make_md(185, "strip-plot-jitter",
        "Strip Plot with Jitter (5x30)",
        "5 categories x 30 points each = 150 points, jittered vertically within category bands.",
        "A 960x640 viewport with 150 points in 5 jittered strip categories. Y jitter is "
        "Gaussian (sigma=0.3) within each band. pointSize=3.",
        "1 pane, 1 layer, 1 transform, 5 buffers, 5 geometries, 5 drawItems.",
        "X: gauss(cat+2, 0.8). Y: cat*2 + gauss(0, 0.3). Seed=185.")
    return "185-strip-plot-jitter", d.build(), md


def trial_186():
    """100 points in beeswarm/violin shape."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "beeswarm")

    random.seed(186)
    pts = []
    for i in range(100):
        y = random.gauss(0, 0.3)  # normal distribution vertically
        # Width (x jitter) proportional to density at this y
        density = math.exp(-y * y / (2 * 0.3 * 0.3))
        x = random.uniform(-density * 0.4, density * 0.4)
        pts.extend([x, y])

    d.add_draw(10, "swarm", "points@1", "pos2_clip", pts,
               rgb(0.4, 0.7, 1.0), point_size=4.0)

    md = make_md(186, "beeswarm-cluster",
        "Beeswarm Cluster Plot",
        "100 points in a beeswarm/violin shape, denser in the middle, sparse at edges.",
        "A 960x640 viewport with 100 points arranged in a beeswarm shape. Y values are "
        "Gaussian (sigma=0.3). X jitter width is proportional to the density at each Y, "
        "creating a violin-like shape. pointSize=4. Directly in clip space.",
        "1 pane, 1 layer, 0 transforms, 1 buffer (200 floats), 1 geometry (100 verts), 1 drawItem.",
        "Y: gauss(0, 0.3). X width: exp(-y^2/(2*0.09))*0.4. Seed=186.")
    return "186-beeswarm-cluster", d.build(), md


def trial_187():
    """Raincloud plot: half-violin + box + jittered points."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "violin")
    d.add_layer(11, 1, "box")
    d.add_layer(12, 1, "points")

    random.seed(187)
    # Generate data
    values = sorted([random.gauss(0, 0.25) for _ in range(40)])
    q1, med, q3 = values[10], values[20], values[30]
    vmin, vmax = values[0], values[-1]

    # Half-violin (upper half): kernel density approximation
    violin_tris = []
    n_bins = 20
    bin_edges = [vmin + i * (vmax - vmin) / n_bins for i in range(n_bins + 1)]
    densities = []
    for i in range(n_bins):
        lo, hi = bin_edges[i], bin_edges[i + 1]
        count = sum(1 for v in values if lo <= v < hi)
        densities.append(count / len(values))
    max_d = max(densities) if max(densities) > 0 else 1
    # Normalize density to max width 0.3 in clip space, placed above y=0.1
    base_y = 0.1
    for i in range(n_bins):
        x0 = -0.8 + i * (1.6 / n_bins)
        x1 = -0.8 + (i + 1) * (1.6 / n_bins)
        h = densities[i] / max_d * 0.4
        # Triangles for bar
        violin_tris.extend([x0, base_y, x1, base_y, x1, base_y + h])
        violin_tris.extend([x0, base_y, x1, base_y + h, x0, base_y + h])

    d.add_draw(10, "violin", "triSolid@1", "pos2_clip", violin_tris,
               rgb(0.5, 0.3, 0.8, 0.6))

    # Box (IQR)
    # Map data range to x in [-0.8, 0.8]
    def val_to_x(v):
        return -0.8 + (v - vmin) / (vmax - vmin) * 1.6

    box_data = [val_to_x(q1), -0.15, val_to_x(q3), 0.05]
    d.add_draw(11, "box", "instancedRect@1", "rect4", box_data,
               rgb(0.3, 0.3, 0.7, 0.8))

    # Median line
    mx = val_to_x(med)
    med_data = [mx, -0.15, mx, 0.05]
    d.add_draw(11, "median", "lineAA@1", "rect4", med_data,
               rgb(1.0, 1.0, 1.0), line_width=2.0)

    # Jittered points below
    pt_data = []
    for v in values:
        x = val_to_x(v)
        y = -0.35 + random.uniform(-0.1, 0.1)
        pt_data.extend([x, y])

    d.add_draw(12, "points", "points@1", "pos2_clip", pt_data,
               rgb(0.8, 0.5, 1.0), point_size=3.0)

    md = make_md(187, "raincloud-plot",
        "Raincloud Plot",
        "Combined half-violin, box plot, and jittered raw data points.",
        "A 960x640 viewport with 3 layers: (1) half-violin density histogram (triSolid@1, 20 bins), "
        "(2) IQR box (instancedRect@1) + median line (lineAA@1), (3) 40 jittered points below. "
        "All in clip space.",
        "1 pane, 3 layers, 0 transforms, 4 buffers, 4 geometries, 4 drawItems.",
        "40 Gaussian values (mu=0, sigma=0.25). Q1/median/Q3 at indices 10/20/30. Seed=187.")
    return "187-raincloud-plot", d.build(), md


def trial_188():
    """4x3 mosaic plot with proportional areas."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "mosaic")

    random.seed(188)
    rows, cols = 3, 4
    # Proportional widths and heights
    col_weights = [random.uniform(0.5, 2.0) for _ in range(cols)]
    row_weights = [random.uniform(0.5, 2.0) for _ in range(rows)]
    total_w = sum(col_weights)
    total_h = sum(row_weights)
    col_widths = [w / total_w * 1.8 for w in col_weights]
    row_heights = [h / total_h * 1.8 for h in row_weights]

    colors = distinct_colors(rows * cols)
    data = []
    y = -0.9
    ci = 0
    # We need per-rect color → use triGradient
    tri_data = []
    for r in range(rows):
        x = -0.9
        for c in range(cols):
            gap = 0.01
            x0, y0 = x + gap, y + gap
            x1, y1 = x + col_widths[c] - gap, y + row_heights[r] - gap
            col = colors[ci]
            cr, cg, cb, ca = col
            tri_data.extend([x0,y0,cr,cg,cb,ca, x1,y0,cr,cg,cb,ca, x1,y1,cr,cg,cb,ca])
            tri_data.extend([x0,y0,cr,cg,cb,ca, x1,y1,cr,cg,cb,ca, x0,y1,cr,cg,cb,ca])
            ci += 1
            x += col_widths[c]
        y += row_heights[r]

    d.add_draw(10, "mosaic", "triGradient@1", "pos2_color4", tri_data,
               rgb(1, 1, 1))

    md = make_md(188, "mosaic-plot",
        "Mosaic Plot (4x3)",
        "4x3 proportional area grid where widths/heights encode marginal frequencies.",
        "A 960x640 viewport with 12 rectangles in a 4x3 mosaic layout. Column widths and "
        "row heights are proportional to random weights. Each cell has a distinct color "
        "(triGradient@1). Gap of 0.01 between cells.",
        "1 pane, 1 layer, 0 transforms, 1 buffer (12*6*6=432 floats), 1 geometry (72 verts), 1 drawItem.",
        "Column/row weights: random uniform [0.5, 2.0]. Seed=188.")
    return "188-mosaic-plot", d.build(), md


def trial_189():
    """100% stacked horizontal bars (spine chart)."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "bars")

    random.seed(189)
    n_cats = 5
    n_segs = 3
    seg_colors = [rgb(0.3, 0.6, 0.9), rgb(0.9, 0.4, 0.2), rgb(0.3, 0.8, 0.4)]

    # triGradient for per-rect color
    tri_data = []
    bar_height = 0.3
    gap = 0.05
    for cat in range(n_cats):
        y0 = -0.85 + cat * (bar_height + gap)
        y1 = y0 + bar_height
        fracs = [random.uniform(0.2, 1.0) for _ in range(n_segs)]
        total = sum(fracs)
        fracs = [f / total for f in fracs]  # normalize to 1.0
        x = -0.85
        for s in range(n_segs):
            w = fracs[s] * 1.7
            x1 = x + w
            col = seg_colors[s]
            cr, cg, cb, ca = col
            tri_data.extend([x,y0,cr,cg,cb,ca, x1,y0,cr,cg,cb,ca, x1,y1,cr,cg,cb,ca])
            tri_data.extend([x,y0,cr,cg,cb,ca, x1,y1,cr,cg,cb,ca, x,y1,cr,cg,cb,ca])
            x = x1

    d.add_draw(10, "spine", "triGradient@1", "pos2_color4", tri_data,
               rgb(1, 1, 1))

    md = make_md(189, "spine-chart",
        "Spine Chart (100% Stacked Horizontal Bars)",
        "5 categories, each subdivided into 3 segments totaling 100%. 15 segments total.",
        "A 960x640 viewport with 15 stacked horizontal bar segments (triGradient@1). "
        "Each of 5 categories spans the full width, subdivided into 3 proportional segments. "
        "Blue/orange/green segment colors.",
        "1 pane, 1 layer, 0 transforms, 1 buffer (15*6*6=540 floats), 1 geometry (90 verts), 1 drawItem.",
        "Random proportions normalized to 1.0 per category. Seed=189.")
    return "189-spine-chart", d.build(), md


def trial_190():
    """Icicle chart: top-down hierarchical rectangles."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "icicle")

    # Root spans full width, children subdivide
    # Level 0: 1 node (full width)
    # Level 1: 3 nodes
    # Level 2: ~11 nodes (3-4 per level-1 node)
    random.seed(190)
    level_height = 0.55
    gap = 0.02

    # triGradient for per-node color
    tri_data = []
    colors_l0 = [rgb(0.3, 0.5, 0.8)]
    colors_l1 = [rgb(0.8, 0.3, 0.3), rgb(0.3, 0.8, 0.3), rgb(0.3, 0.3, 0.8)]
    colors_l2 = distinct_colors(12, s=0.6, l=0.5)

    def add_rect(x0, y0, x1, y1, col):
        cr, cg, cb, ca = col
        tri_data.extend([x0,y0,cr,cg,cb,ca, x1,y0,cr,cg,cb,ca, x1,y1,cr,cg,cb,ca])
        tri_data.extend([x0,y0,cr,cg,cb,ca, x1,y1,cr,cg,cb,ca, x0,y1,cr,cg,cb,ca])

    # Level 0
    add_rect(-0.9, 0.9 - level_height + gap, 0.9, 0.9 - gap, colors_l0[0])

    # Level 1: 3 children
    l1_widths = [0.4, 0.35, 0.25]
    l1_x = -0.9
    l1_children = []
    for i in range(3):
        w = l1_widths[i] * 1.8
        x0 = l1_x + gap
        x1 = l1_x + w - gap
        y_top = 0.9 - level_height
        y_bot = y_top - level_height + gap
        add_rect(x0, y_bot, x1, y_top - gap, colors_l1[i])
        l1_children.append((l1_x, l1_x + w))
        l1_x += w

    # Level 2: 3-4 children per level-1 node
    ci = 0
    for pi in range(3):
        parent_x0, parent_x1 = l1_children[pi]
        n_children = random.choice([3, 4])
        child_widths = [random.uniform(0.5, 2.0) for _ in range(n_children)]
        total = sum(child_widths)
        child_widths = [w / total * (parent_x1 - parent_x0) for w in child_widths]
        cx = parent_x0
        y_top = 0.9 - 2 * level_height
        y_bot = y_top - level_height + gap
        for ci2 in range(n_children):
            add_rect(cx + gap, y_bot, cx + child_widths[ci2] - gap, y_top - gap, colors_l2[ci])
            cx += child_widths[ci2]
            ci += 1

    d.add_draw(10, "icicle", "triGradient@1", "pos2_color4", tri_data,
               rgb(1, 1, 1))

    md = make_md(190, "icicle-chart",
        "Icicle Chart (3-Level Hierarchy)",
        "Top-down hierarchical rectangles with root spanning full width, children below.",
        "A 960x640 viewport with ~15 rectangles in 3 hierarchical levels (triGradient@1). "
        "Root spans full width at top. 3 level-1 children subdivide below. 10-12 level-2 "
        "children further subdivide. Gap=0.02 between nodes.",
        "1 pane, 1 layer, 0 transforms, 1 buffer, 1 geometry, 1 drawItem.",
        "Level-1 widths: 40/35/25%. Level-2: random 3-4 children per parent. Seed=190.")
    return "190-icicle-chart", d.build(), md


def trial_191():
    """Binary dendrogram with horizontal links."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "links")
    d.add_layer(11, 1, "leaves")
    d.add_transform(50, sx=0.12, sy=0.3, tx=-0.9, ty=-0.85)

    random.seed(191)
    n_leaves = 15
    # Leaf positions
    leaf_x = list(range(n_leaves))
    leaf_y = [0.0] * n_leaves

    # Build hierarchical clustering (simplified: merge adjacent pairs)
    active = list(range(n_leaves))
    merge_y = 1.0
    segs = []
    while len(active) > 1:
        new_active = []
        i = 0
        while i < len(active) - 1:
            l_idx = active[i]
            r_idx = active[i + 1]
            lx = leaf_x[l_idx] if isinstance(l_idx, int) else l_idx
            rx = leaf_x[r_idx] if isinstance(r_idx, int) else r_idx
            # Horizontal link at merge_y
            segs.extend([lx, merge_y, rx, merge_y])
            # Vertical links down to previous positions
            # We track positions through the merge
            new_active.append((lx + rx) / 2.0)
            # Vertical down-links
            segs.extend([lx, merge_y - 0.3, lx, merge_y])
            segs.extend([rx, merge_y - 0.3, rx, merge_y])
            i += 2
        if i < len(active):
            # Odd one out
            idx = active[i]
            pos = leaf_x[idx] if isinstance(idx, int) else idx
            new_active.append(pos)
        # For next round, update leaf_x mapping
        for j, pos in enumerate(new_active):
            if len(leaf_x) <= n_leaves + len(active) + j:
                leaf_x.append(pos)
            else:
                leaf_x[n_leaves + j] = pos
        active = list(range(len(leaf_x) - len(new_active), len(leaf_x)))
        merge_y += 1.0

    d.add_draw(10, "links", "lineAA@1", "rect4", segs,
               rgb(0.7, 0.7, 0.7), transform_id=50, line_width=1.5)

    # Leaf points
    leaf_pts = []
    for i in range(n_leaves):
        leaf_pts.extend([i, 0.0])
    d.add_draw(11, "leaves", "points@1", "pos2_clip", leaf_pts,
               rgb(0.3, 0.8, 1.0), transform_id=50, point_size=5.0)

    md = make_md(191, "dendrogram",
        "Binary Dendrogram",
        "Binary tree with horizontal merge links and 15 leaf nodes.",
        "A 960x640 viewport with a dendrogram: lineAA@1 links connecting merged clusters, "
        "and 15 leaf node points at the bottom. Adjacent pairs merge at increasing heights. "
        "Transform maps data space to clip.",
        "1 pane, 2 layers, 1 transform, 2 buffers, 2 geometries, 2 drawItems.",
        "15 leaves, merge adjacent pairs up the tree. Seed=191.")
    return "191-dendrogram", d.build(), md


def trial_192():
    """20 diverging bars extending left/right from center."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "bars")

    random.seed(192)
    tri_data = []
    n_bars = 20
    bar_h = 0.08
    gap = 0.005
    for i in range(n_bars):
        val = random.gauss(0, 0.35)
        y0 = -0.85 + i * (bar_h + gap)
        y1 = y0 + bar_h
        if val >= 0:
            col = rgb(0.2, 0.75, 0.3)
            x0, x1 = 0.0, min(val, 0.85)
        else:
            col = rgb(0.9, 0.25, 0.2)
            x0, x1 = max(val, -0.85), 0.0
        cr, cg, cb, ca = col
        tri_data.extend([x0,y0,cr,cg,cb,ca, x1,y0,cr,cg,cb,ca, x1,y1,cr,cg,cb,ca])
        tri_data.extend([x0,y0,cr,cg,cb,ca, x1,y1,cr,cg,cb,ca, x0,y1,cr,cg,cb,ca])

    d.add_draw(10, "divBars", "triGradient@1", "pos2_color4", tri_data,
               rgb(1, 1, 1))

    # Center axis
    d.add_draw(10, "axis", "lineAA@1", "rect4", [0.0, -0.9, 0.0, 0.9],
               rgb(0.5, 0.5, 0.5), line_width=1.0)

    md = make_md(192, "diverging-bars",
        "Diverging Bar Chart",
        "20 bars extending left (negative/red) and right (positive/green) from center axis.",
        "A 960x640 viewport with 20 horizontal bars diverging from a center axis. "
        "Positive values go right (green), negative go left (red). A thin gray center line. "
        "triGradient@1 for per-bar colors, lineAA@1 for the axis.",
        "1 pane, 1 layer, 0 transforms, 2 buffers, 2 geometries, 2 drawItems.",
        "Values: gauss(0, 0.35), clamped to [-0.85, 0.85]. Seed=192.")
    return "192-diverging-bars", d.build(), md


def trial_193():
    """Connected scatter: 30 points + connecting line."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "line")
    d.add_layer(11, 1, "points")
    d.add_transform(50, sx=0.058, sy=0.4, tx=-0.9, ty=0.0)

    random.seed(193)
    xs, ys = [], []
    x, y = 0.0, 0.0
    for i in range(30):
        xs.append(x)
        ys.append(y)
        x += random.uniform(0.5, 1.5)
        y += random.gauss(0, 0.5)

    segs = []
    for i in range(29):
        segs.extend([xs[i], ys[i], xs[i+1], ys[i+1]])

    pts = []
    for i in range(30):
        pts.extend([xs[i], ys[i]])

    d.add_draw(10, "path", "lineAA@1", "rect4", segs,
               rgb(0.4, 0.6, 0.9, 0.6), transform_id=50, line_width=1.5)
    d.add_draw(11, "dots", "points@1", "pos2_clip", pts,
               rgb(1.0, 0.5, 0.2), transform_id=50, point_size=5.0)

    md = make_md(193, "connected-scatter",
        "Connected Scatter Plot",
        "30 time-ordered points connected by a line, showing trajectory over time.",
        "A 960x640 viewport with a lineAA path (29 segments) and 30 overlay points. "
        "Line is semi-transparent blue, points are orange with pointSize=5. "
        "Data-space transform maps the trajectory to clip.",
        "1 pane, 2 layers, 1 transform, 2 buffers, 2 geometries, 2 drawItems.",
        "X increments by uniform(0.5,1.5). Y random walk gauss(0,0.5). Seed=193.")
    return "193-connected-scatter", d.build(), md


def trial_194():
    """Multi-slope chart: 10 lines connecting two columns of dots."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "slopes")
    d.add_layer(11, 1, "dots")

    random.seed(194)
    n_items = 10
    colors = distinct_colors(n_items)

    # Period A (x=-0.7) and Period B (x=0.7)
    a_vals = [random.uniform(0.1, 0.9) for _ in range(n_items)]
    b_vals = [a + random.gauss(0, 0.2) for a in a_vals]
    b_vals = [max(0.05, min(0.95, v)) for v in b_vals]

    # Map values to y in [-0.8, 0.8]
    def val_to_y(v):
        return -0.8 + v * 1.6

    segs = []
    pts_a = []
    pts_b = []
    for i in range(n_items):
        ya = val_to_y(a_vals[i])
        yb = val_to_y(b_vals[i])
        segs.extend([-0.7, ya, 0.7, yb])
        pts_a.extend([-0.7, ya])
        pts_b.extend([0.7, yb])

    # Individual colored lines
    for i in range(n_items):
        ya = val_to_y(a_vals[i])
        yb = val_to_y(b_vals[i])
        d.add_draw(10, f"slope{i}", "lineAA@1", "rect4", [-0.7, ya, 0.7, yb],
                   colors[i], line_width=2.0)

    # All dots
    all_pts = pts_a + pts_b
    d.add_draw(11, "dotsAll", "points@1", "pos2_clip", all_pts,
               rgb(1.0, 1.0, 1.0), point_size=6.0)

    md = make_md(194, "multi-slope",
        "Multi-Slope Chart",
        "10 slope lines connecting two columns of dots, showing change between periods A and B.",
        "A 960x640 viewport with 10 colored slope lines (lineAA@1) connecting points at "
        "x=-0.7 (period A) and x=0.7 (period B). White dots at both endpoints. Each line "
        "has a distinct color.",
        "1 pane, 2 layers, 0 transforms, 11 buffers, 11 geometries, 11 drawItems (10 lines + 1 points).",
        "A values: uniform(0.1, 0.9). B = A + gauss(0, 0.2). Seed=194.")
    return "194-multi-slope", d.build(), md


def trial_195():
    """Bump chart: 8 items ranked across 6 time periods."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "paths")

    random.seed(195)
    n_items = 8
    n_periods = 6
    colors = distinct_colors(n_items)

    # Generate rankings: each period is a permutation of 1..8
    rankings = []
    current = list(range(n_items))
    for p in range(n_periods):
        random.shuffle(current)
        rankings.append(current[:])

    # Map to positions
    def pos(period, rank):
        x = -0.8 + period * (1.6 / (n_periods - 1))
        y = 0.8 - rank * (1.6 / (n_items - 1))
        return x, y

    for item in range(n_items):
        segs = []
        for p in range(n_periods - 1):
            rank0 = rankings[p].index(item)
            rank1 = rankings[p + 1].index(item)
            x0, y0 = pos(p, rank0)
            x1, y1 = pos(p + 1, rank1)
            segs.extend([x0, y0, x1, y1])
        d.add_draw(10, f"item{item}", "lineAA@1", "rect4", segs,
                   colors[item], line_width=2.5)

    md = make_md(195, "bump-chart-8x6",
        "Bump Chart (8 Items x 6 Periods)",
        "8 items ranked across 6 time periods, with colored lineAA paths showing rank changes.",
        "A 960x640 viewport with 8 colored paths (lineAA@1, 5 segments each) showing "
        "rank positions across 6 time periods. Rankings are random permutations. Each item "
        "has a distinct color, lineWidth=2.5.",
        "1 pane, 1 layer, 0 transforms, 8 buffers, 8 geometries, 8 drawItems.",
        "Rankings: random shuffles per period. Seed=195.")
    return "195-bump-chart-8x6", d.build(), md


def trial_196():
    """Alluvial flow: bands between 3 columns."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "flows")

    random.seed(196)
    # 4 categories flow between 3 columns
    # Column positions
    col_x = [-0.7, 0.0, 0.7]
    band_width = 0.15

    # Category sizes at each column
    sizes = [[0.3, 0.25, 0.25, 0.2],
             [0.2, 0.3, 0.3, 0.2],
             [0.35, 0.15, 0.2, 0.3]]
    colors_flow = [rgb(0.3, 0.6, 0.9, 0.6), rgb(0.9, 0.4, 0.2, 0.6),
                   rgb(0.3, 0.8, 0.4, 0.6), rgb(0.8, 0.3, 0.8, 0.6)]

    tri_data = []

    def get_y_ranges(col_idx):
        """Return list of (y_bottom, y_top) for each category at this column."""
        s = sizes[col_idx]
        total = sum(s)
        ranges = []
        y = -0.8
        for si in s:
            h = si / total * 1.6
            ranges.append((y, y + h))
            y += h + 0.02
        return ranges

    # Draw flows between adjacent columns
    for col in range(2):
        left_ranges = get_y_ranges(col)
        right_ranges = get_y_ranges(col + 1)
        xl = col_x[col] + band_width
        xr = col_x[col + 1] - band_width

        for cat in range(4):
            ly0, ly1 = left_ranges[cat]
            ry0, ry1 = right_ranges[cat]
            col_c = colors_flow[cat]
            cr, cg, cb, ca = col_c

            # Create band as series of quads (interpolating y positions)
            n_seg = 10
            for s in range(n_seg):
                t0 = s / n_seg
                t1 = (s + 1) / n_seg
                # Smooth interpolation
                st0 = t0 * t0 * (3 - 2 * t0)
                st1 = t1 * t1 * (3 - 2 * t1)
                x0 = xl + t0 * (xr - xl)
                x1 = xl + t1 * (xr - xl)
                bot0 = ly0 + st0 * (ry0 - ly0)
                top0 = ly1 + st0 * (ry1 - ly1)
                bot1 = ly0 + st1 * (ry0 - ly0)
                top1 = ly1 + st1 * (ry1 - ly1)
                tri_data.extend([x0,bot0,cr,cg,cb,ca, x1,bot1,cr,cg,cb,ca, x1,top1,cr,cg,cb,ca])
                tri_data.extend([x0,bot0,cr,cg,cb,ca, x1,top1,cr,cg,cb,ca, x0,top0,cr,cg,cb,ca])

    d.add_draw(10, "flows", "triGradient@1", "pos2_color4", tri_data,
               rgb(1, 1, 1))

    # Column blocks
    d.add_layer(11, 1, "columns")
    for col in range(3):
        ranges = get_y_ranges(col)
        for cat in range(4):
            y0, y1 = ranges[cat]
            x0 = col_x[col] - band_width
            x1 = col_x[col] + band_width
            col_c = colors_flow[cat]
            cr, cg, cb, ca = col_c[0], col_c[1], col_c[2], 1.0
            col_solid = [cr, cg, cb, ca]

    # Simple column rects
    col_tri = []
    for col_idx in range(3):
        ranges = get_y_ranges(col_idx)
        for cat in range(4):
            y0, y1 = ranges[cat]
            x0 = col_x[col_idx] - band_width
            x1 = col_x[col_idx] + band_width
            cr, cg, cb, _ = colors_flow[cat]
            ca = 1.0
            col_tri.extend([x0,y0,cr,cg,cb,ca, x1,y0,cr,cg,cb,ca, x1,y1,cr,cg,cb,ca])
            col_tri.extend([x0,y0,cr,cg,cb,ca, x1,y1,cr,cg,cb,ca, x0,y1,cr,cg,cb,ca])

    d.add_draw(11, "colBlocks", "triGradient@1", "pos2_color4", col_tri,
               rgb(1, 1, 1))

    md = make_md(196, "alluvial-flow",
        "Alluvial Flow Diagram",
        "Flowing bands between 3 columns showing how 4 categories redistribute.",
        "A 960x640 viewport with flowing bands (triGradient@1) connecting 3 columns. "
        "4 categories flow between columns with smooth cubic interpolation (10 segments per flow). "
        "Solid column blocks overlay the flow bands.",
        "1 pane, 2 layers, 0 transforms, 2 buffers, 2 geometries, 2 drawItems.",
        "Category sizes vary per column. Smoothstep interpolation for band curvature. Seed=196.")
    return "196-alluvial-flow", d.build(), md


def trial_197():
    """15 proportional symbol circles on a grid."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "circles")

    random.seed(197)
    # 15 circles in a 5x3 grid
    tri_data = []
    colors_list = distinct_colors(15, s=0.6, l=0.55)
    idx = 0
    for row in range(3):
        for col in range(5):
            cx = -0.7 + col * 0.35
            cy = -0.55 + row * 0.55
            val = random.uniform(0.3, 1.0)
            r = val * 0.12  # max radius 0.12
            col_c = colors_list[idx]
            # Generate circle triangles with per-vertex color (triGradient)
            n_seg = 24
            cr, cg, cb, ca = col_c
            for s in range(n_seg):
                a0 = 2 * math.pi * s / n_seg
                a1 = 2 * math.pi * (s + 1) / n_seg
                tri_data.extend([cx, cy, cr, cg, cb, ca])
                tri_data.extend([cx + r*math.cos(a0), cy + r*math.sin(a0), cr, cg, cb, ca])
                tri_data.extend([cx + r*math.cos(a1), cy + r*math.sin(a1), cr, cg, cb, ca])
            idx += 1

    d.add_draw(10, "symbols", "triGradient@1", "pos2_color4", tri_data,
               rgb(1, 1, 1))

    md = make_md(197, "proportional-symbols",
        "Proportional Symbol Map",
        "15 circles of varying radius on a 5x3 grid, sized by data value.",
        "A 960x640 viewport with 15 circles (triGradient@1, 24-segment center-fan each). "
        "Radius proportional to random value [0.3, 1.0]. Each circle has a distinct color. "
        "Arranged in a 5x3 grid layout.",
        "1 pane, 1 layer, 0 transforms, 1 buffer (15*24*3*6=6480 floats), 1 geometry (1080 verts), 1 drawItem.",
        "Radii: value * 0.12. 24 triangle segments per circle. Seed=197.")
    return "197-proportional-symbols", d.build(), md


def trial_198():
    """4x4 dot density grid with ~200 points."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "dots")

    random.seed(198)
    pts = []
    densities = [[random.randint(5, 25) for _ in range(4)] for _ in range(4)]
    for row in range(4):
        for col in range(4):
            cx = -0.7 + col * 0.45
            cy = -0.7 + row * 0.45
            n = densities[row][col]
            for _ in range(n):
                x = cx + random.uniform(-0.18, 0.18)
                y = cy + random.uniform(-0.18, 0.18)
                pts.extend([x, y])

    total = len(pts) // 2
    d.add_draw(10, "dots", "points@1", "pos2_clip", pts,
               rgb(0.3, 0.8, 0.5), point_size=3.0)

    md = make_md(198, "dot-density-grid",
        "Dot Density Grid (4x4)",
        "4x4 grid regions with random point densities, ~200 points total.",
        f"A 960x640 viewport with {total} points (points@1) scattered across 16 grid cells. "
        "Density varies by region (5-25 points per cell). pointSize=3. All in clip space.",
        f"1 pane, 1 layer, 0 transforms, 1 buffer ({total*2} floats), 1 geometry ({total} verts), 1 drawItem.",
        "Per-cell count: randint(5,25). Points uniform within cell bounds. Seed=198.")
    return "198-dot-density-grid", d.build(), md


def trial_199():
    """12 cartogram rectangles sized by value."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "rects")

    random.seed(199)
    # 12 rects in approximate 4x3 geographic-ish layout
    tri_data = []
    values = [random.uniform(0.3, 1.0) for _ in range(12)]
    colors_list = distinct_colors(12, s=0.65, l=0.5)

    # Layout: 3 rows of 4, with sizes proportional to values
    idx = 0
    y_cursor = -0.85
    for row in range(3):
        row_vals = values[row*4:(row+1)*4]
        total = sum(row_vals)
        row_height = 0.5 * (sum(row_vals) / 4.0)  # average-based height
        x_cursor = -0.85
        for col in range(4):
            w = row_vals[col] / total * 1.7
            h = row_height
            gap = 0.02
            x0, y0 = x_cursor + gap, y_cursor + gap
            x1, y1 = x_cursor + w - gap, y_cursor + h - gap
            cr, cg, cb, ca = colors_list[idx]
            tri_data.extend([x0,y0,cr,cg,cb,ca, x1,y0,cr,cg,cb,ca, x1,y1,cr,cg,cb,ca])
            tri_data.extend([x0,y0,cr,cg,cb,ca, x1,y1,cr,cg,cb,ca, x0,y1,cr,cg,cb,ca])
            x_cursor += w
            idx += 1
        y_cursor += row_height

    d.add_draw(10, "cartogram", "triGradient@1", "pos2_color4", tri_data,
               rgb(1, 1, 1))

    md = make_md(199, "cartogram-rects",
        "Cartogram Rectangles",
        "12 rectangles sized proportional to values, packed in approximate geographic layout.",
        "A 960x640 viewport with 12 rectangles (triGradient@1) in a 4x3 grid. "
        "Widths proportional to value within each row. Heights scaled by row average. "
        "Each rectangle has a distinct color.",
        "1 pane, 1 layer, 0 transforms, 1 buffer (12*6*6=432 floats), 1 geometry (72 verts), 1 drawItem.",
        "Values: uniform(0.3, 1.0). Width proportional within row. Seed=199.")
    return "199-cartogram-rects", d.build(), md


def trial_200():
    """Coxcomb chart: 12 angular sectors of varying radius."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "sectors")

    random.seed(200)
    tri_data = []
    n_sectors = 12
    colors_list = distinct_colors(n_sectors)
    aspect = 960.0 / 640.0  # correct for non-square viewport

    for i in range(n_sectors):
        val = random.uniform(0.2, 0.8)
        a_start = 2 * math.pi * i / n_sectors
        a_end = 2 * math.pi * (i + 1) / n_sectors
        r = val
        col = colors_list[i]
        cr, cg, cb, ca = col
        # Wedge from center
        n_seg = 8
        for s in range(n_seg):
            a0 = a_start + (a_end - a_start) * s / n_seg
            a1 = a_start + (a_end - a_start) * (s + 1) / n_seg
            x0, y0 = r * math.cos(a0) / aspect, r * math.sin(a0)
            x1, y1 = r * math.cos(a1) / aspect, r * math.sin(a1)
            tri_data.extend([0, 0, cr, cg, cb, ca])
            tri_data.extend([x0, y0, cr, cg, cb, ca])
            tri_data.extend([x1, y1, cr, cg, cb, ca])

    d.add_draw(10, "coxcomb", "triGradient@1", "pos2_color4", tri_data,
               rgb(1, 1, 1))

    md = make_md(200, "coxcomb-chart",
        "Coxcomb (Nightingale Rose) Chart",
        "12 angular sectors with varying radii, like a Nightingale rose diagram.",
        "A 960x640 viewport with 12 colored wedge sectors (triGradient@1). Each sector's "
        "radius encodes a random value [0.2, 0.8]. 8 triangle segments per wedge. "
        "Aspect-ratio corrected for non-square viewport.",
        f"1 pane, 1 layer, 0 transforms, 1 buffer ({n_sectors*8*3*6} floats), 1 geometry ({n_sectors*8*3} verts), 1 drawItem.",
        "Radii: uniform(0.2, 0.8). 12 equal angular sectors (30 deg each). Seed=200.")
    return "200-coxcomb-chart", d.build(), md


def trial_201():
    """Wind rose: 16 directions x 3 speed bins."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "sectors")

    random.seed(201)
    n_dirs = 16
    n_bins = 3
    bin_colors = [rgb(0.3, 0.6, 0.9), rgb(0.9, 0.7, 0.2), rgb(0.9, 0.3, 0.2)]
    aspect = 960.0 / 640.0

    tri_data = []
    for d_idx in range(n_dirs):
        a_start = 2 * math.pi * d_idx / n_dirs - math.pi / n_dirs
        a_end = 2 * math.pi * d_idx / n_dirs + math.pi / n_dirs
        r_inner = 0.0
        for b_idx in range(n_bins):
            freq = random.uniform(0.05, 0.25)
            r_outer = r_inner + freq
            col = bin_colors[b_idx]
            cr, cg, cb, ca = col
            n_seg = 4
            for s in range(n_seg):
                a0 = a_start + (a_end - a_start) * s / n_seg
                a1 = a_start + (a_end - a_start) * (s + 1) / n_seg
                cos0, sin0 = math.cos(a0), math.sin(a0)
                cos1, sin1 = math.cos(a1), math.sin(a1)
                ix0 = r_inner * cos0 / aspect
                iy0 = r_inner * sin0
                ox0 = r_outer * cos0 / aspect
                oy0 = r_outer * sin0
                ix1 = r_inner * cos1 / aspect
                iy1 = r_inner * sin1
                ox1 = r_outer * cos1 / aspect
                oy1 = r_outer * sin1
                tri_data.extend([ix0,iy0,cr,cg,cb,ca, ox0,oy0,cr,cg,cb,ca, ox1,oy1,cr,cg,cb,ca])
                tri_data.extend([ix0,iy0,cr,cg,cb,ca, ox1,oy1,cr,cg,cb,ca, ix1,iy1,cr,cg,cb,ca])
            r_inner = r_outer

    d.add_draw(10, "windrose", "triGradient@1", "pos2_color4", tri_data,
               rgb(1, 1, 1))

    md = make_md(201, "wind-rose",
        "Wind Rose Diagram",
        "16 directions x 3 speed bins = 48 wedge segments showing directional frequency.",
        "A 960x640 viewport with 48 wedge segments (triGradient@1) in a wind rose pattern. "
        "16 angular directions, each with 3 concentric speed bins (blue/yellow/red). "
        "4 triangle segments per wedge. Aspect-ratio corrected.",
        f"1 pane, 1 layer, 0 transforms, 1 buffer, 1 geometry, 1 drawItem.",
        "Frequency per bin: uniform(0.05, 0.25). 16 directions * 3 bins = 48 segments. Seed=201.")
    return "201-wind-rose", d.build(), md


def trial_202():
    """4 overlaid radar/star polygons with 6 axes."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "grid")
    d.add_layer(11, 1, "polygons")

    random.seed(202)
    n_axes = 6
    n_polys = 4
    colors_poly = [rgb(0.3, 0.6, 0.9, 0.8), rgb(0.9, 0.3, 0.3, 0.8),
                   rgb(0.3, 0.8, 0.3, 0.8), rgb(0.9, 0.7, 0.2, 0.8)]
    aspect = 960.0 / 640.0

    # Grid axes
    grid_segs = []
    for i in range(n_axes):
        angle = 2 * math.pi * i / n_axes - math.pi / 2
        x = 0.7 * math.cos(angle) / aspect
        y = 0.7 * math.sin(angle)
        grid_segs.extend([0, 0, x, y])

    d.add_draw(10, "axes", "lineAA@1", "rect4", grid_segs,
               rgb(0.3, 0.3, 0.4), line_width=1.0)

    # Concentric rings
    ring_segs = []
    for ring in range(3):
        r = 0.7 * (ring + 1) / 3
        for i in range(n_axes):
            a0 = 2 * math.pi * i / n_axes - math.pi / 2
            a1 = 2 * math.pi * ((i + 1) % n_axes) / n_axes - math.pi / 2
            ring_segs.extend([r*math.cos(a0)/aspect, r*math.sin(a0),
                             r*math.cos(a1)/aspect, r*math.sin(a1)])

    d.add_draw(10, "rings", "lineAA@1", "rect4", ring_segs,
               rgb(0.25, 0.25, 0.35), line_width=1.0)

    # Polygons
    for p in range(n_polys):
        vals = [random.uniform(0.2, 0.9) for _ in range(n_axes)]
        segs = []
        for i in range(n_axes):
            a0 = 2 * math.pi * i / n_axes - math.pi / 2
            a1 = 2 * math.pi * ((i + 1) % n_axes) / n_axes - math.pi / 2
            r0 = vals[i] * 0.7
            r1 = vals[(i + 1) % n_axes] * 0.7
            segs.extend([r0*math.cos(a0)/aspect, r0*math.sin(a0),
                        r1*math.cos(a1)/aspect, r1*math.sin(a1)])
        d.add_draw(11, f"poly{p}", "lineAA@1", "rect4", segs,
                   colors_poly[p], line_width=2.0)

    md = make_md(202, "multi-star-plot",
        "Multi-Star (Radar) Plot",
        "4 overlaid radar polygons with 6 axes, each polygon a different color.",
        "A 960x640 viewport with 6 axis lines, 3 concentric grid rings, and 4 colored "
        "radar polygons (all lineAA@1). Each polygon has random values [0.2, 0.9] on 6 axes. "
        "Aspect-ratio corrected for circular appearance.",
        "1 pane, 2 layers, 0 transforms, 6 buffers, 6 geometries, 6 drawItems (axes + rings + 4 polys).",
        "Values per polygon: uniform(0.2, 0.9) on 6 axes. Seed=202.")
    return "202-multi-star-plot", d.build(), md


def trial_203():
    """8 radial bars from center."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "bars")

    random.seed(203)
    n_bars = 8
    colors_list = distinct_colors(n_bars, s=0.7, l=0.55)
    aspect = 960.0 / 640.0

    tri_data = []
    r_inner = 0.1
    for i in range(n_bars):
        val = random.uniform(0.3, 0.8)
        r_outer = r_inner + val * 0.6
        a_start = 2 * math.pi * i / n_bars
        a_end = 2 * math.pi * (i + 1) / n_bars
        gap = 0.03  # angular gap
        a_start += gap
        a_end -= gap
        col = colors_list[i]
        cr, cg, cb, ca = col
        n_seg = 8
        for s in range(n_seg):
            a0 = a_start + (a_end - a_start) * s / n_seg
            a1 = a_start + (a_end - a_start) * (s + 1) / n_seg
            cos0, sin0 = math.cos(a0), math.sin(a0)
            cos1, sin1 = math.cos(a1), math.sin(a1)
            ix0 = r_inner * cos0 / aspect
            iy0 = r_inner * sin0
            ox0 = r_outer * cos0 / aspect
            oy0 = r_outer * sin0
            ix1 = r_inner * cos1 / aspect
            iy1 = r_inner * sin1
            ox1 = r_outer * cos1 / aspect
            oy1 = r_outer * sin1
            tri_data.extend([ix0,iy0,cr,cg,cb,ca, ox0,oy0,cr,cg,cb,ca, ox1,oy1,cr,cg,cb,ca])
            tri_data.extend([ix0,iy0,cr,cg,cb,ca, ox1,oy1,cr,cg,cb,ca, ix1,iy1,cr,cg,cb,ca])

    d.add_draw(10, "radialBars", "triGradient@1", "pos2_color4", tri_data,
               rgb(1, 1, 1))

    md = make_md(203, "radial-bar-chart",
        "Radial Bar Chart",
        "8 bars radiating from center with varying lengths, each a different color.",
        "A 960x640 viewport with 8 radial bar segments (triGradient@1). Each bar extends "
        "from inner radius 0.1, with length proportional to value. 8 triangle segments per bar. "
        "Angular gaps between bars. Aspect-ratio corrected.",
        "1 pane, 1 layer, 0 transforms, 1 buffer, 1 geometry, 1 drawItem.",
        "Values: uniform(0.3, 0.8). Outer radius = 0.1 + val * 0.6. Seed=203.")
    return "203-radial-bar-chart", d.build(), md


def trial_204():
    """Spiral data: points along Archimedean spiral + spiral arm line."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "spiral")
    d.add_layer(11, 1, "data")

    aspect = 960.0 / 640.0

    # Spiral arm: 200 segments
    spiral_segs = []
    n_arm = 200
    for i in range(n_arm):
        t0 = i / n_arm * 6 * math.pi
        t1 = (i + 1) / n_arm * 6 * math.pi
        r0 = 0.05 + t0 / (6 * math.pi) * 0.65
        r1 = 0.05 + t1 / (6 * math.pi) * 0.65
        x0 = r0 * math.cos(t0) / aspect
        y0 = r0 * math.sin(t0)
        x1 = r1 * math.cos(t1) / aspect
        y1 = r1 * math.sin(t1)
        spiral_segs.extend([x0, y0, x1, y1])

    d.add_draw(10, "arm", "lineAA@1", "rect4", spiral_segs,
               rgb(0.3, 0.4, 0.6, 0.5), line_width=1.0)

    # 30 data points along the spiral
    random.seed(204)
    pts = []
    for i in range(30):
        t = i / 29 * 6 * math.pi
        r = 0.05 + t / (6 * math.pi) * 0.65
        r += random.gauss(0, 0.02)
        x = r * math.cos(t) / aspect
        y = r * math.sin(t)
        pts.extend([x, y])

    d.add_draw(11, "datapts", "points@1", "pos2_clip", pts,
               rgb(1.0, 0.6, 0.2), point_size=5.0)

    md = make_md(204, "spiral-data",
        "Spiral Data Visualization",
        "30 data points placed along an Archimedean spiral arm with 200 line segments.",
        "A 960x640 viewport with a spiral arm (lineAA@1, 200 segments) and 30 data points "
        "(points@1) placed along the spiral with slight radial jitter. 3 full turns. "
        "Aspect-ratio corrected.",
        "1 pane, 2 layers, 0 transforms, 2 buffers, 2 geometries, 2 drawItems.",
        "Archimedean spiral: r = 0.05 + t/(6pi)*0.65. Point jitter: gauss(0, 0.02). Seed=204.")
    return "204-spiral-data", d.build(), md


def trial_205():
    """Polar heatmap: 8 angular x 5 radial cells."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "cells")

    random.seed(205)
    n_angular = 8
    n_radial = 5
    aspect = 960.0 / 640.0

    tri_data = []
    for ai in range(n_angular):
        a_start = 2 * math.pi * ai / n_angular
        a_end = 2 * math.pi * (ai + 1) / n_angular
        for ri in range(n_radial):
            r_inner = 0.1 + ri * 0.13
            r_outer = 0.1 + (ri + 1) * 0.13
            val = random.uniform(0, 1)
            col = viridis(val)
            cr, cg, cb, ca = col
            n_seg = 6
            for s in range(n_seg):
                a0 = a_start + (a_end - a_start) * s / n_seg
                a1 = a_start + (a_end - a_start) * (s + 1) / n_seg
                cos0, sin0 = math.cos(a0), math.sin(a0)
                cos1, sin1 = math.cos(a1), math.sin(a1)
                ix0 = r_inner * cos0 / aspect
                iy0 = r_inner * sin0
                ox0 = r_outer * cos0 / aspect
                oy0 = r_outer * sin0
                ix1 = r_inner * cos1 / aspect
                iy1 = r_inner * sin1
                ox1 = r_outer * cos1 / aspect
                oy1 = r_outer * sin1
                tri_data.extend([ix0,iy0,cr,cg,cb,ca, ox0,oy0,cr,cg,cb,ca, ox1,oy1,cr,cg,cb,ca])
                tri_data.extend([ix0,iy0,cr,cg,cb,ca, ox1,oy1,cr,cg,cb,ca, ix1,iy1,cr,cg,cb,ca])

    d.add_draw(10, "polarHeat", "triGradient@1", "pos2_color4", tri_data,
               rgb(1, 1, 1))

    md = make_md(205, "polar-heatmap",
        "Polar Heatmap (8x5)",
        "8 angular x 5 radial cells forming an annular heatmap with viridis coloring.",
        "A 960x640 viewport with 40 wedge-shaped cells (triGradient@1). Each cell colored "
        "by viridis approximation of a random value. 6 triangle segments per cell for smooth arcs. "
        "Aspect-ratio corrected.",
        f"1 pane, 1 layer, 0 transforms, 1 buffer, 1 geometry, 1 drawItem.",
        "40 cells (8 angular * 5 radial). Values: uniform(0,1). Seed=205.")
    return "205-polar-heatmap", d.build(), md


def trial_206():
    """Angular histogram: 12 bins as rose wedges."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "histogram")

    random.seed(206)
    n_bins = 12
    aspect = 960.0 / 640.0

    tri_data = []
    colors_list = distinct_colors(n_bins)
    for i in range(n_bins):
        freq = random.uniform(0.15, 0.7)
        a_start = 2 * math.pi * i / n_bins
        a_end = 2 * math.pi * (i + 1) / n_bins
        r = freq
        col = colors_list[i]
        cr, cg, cb, ca = col
        n_seg = 6
        for s in range(n_seg):
            a0 = a_start + (a_end - a_start) * s / n_seg
            a1 = a_start + (a_end - a_start) * (s + 1) / n_seg
            x0 = r * math.cos(a0) / aspect
            y0 = r * math.sin(a0)
            x1 = r * math.cos(a1) / aspect
            y1 = r * math.sin(a1)
            tri_data.extend([0,0,cr,cg,cb,ca, x0,y0,cr,cg,cb,ca, x1,y1,cr,cg,cb,ca])

    d.add_draw(10, "angHist", "triGradient@1", "pos2_color4", tri_data,
               rgb(1, 1, 1))

    md = make_md(206, "angular-histogram",
        "Angular Histogram (Rose Diagram)",
        "12 angular bins showing directional frequency distribution as wedge sectors.",
        "A 960x640 viewport with 12 rose wedges (triGradient@1). Radius proportional to "
        "frequency [0.15, 0.7]. 6 segments per wedge for smooth arcs. Each bin a distinct color. "
        "Aspect-ratio corrected.",
        "1 pane, 1 layer, 0 transforms, 1 buffer, 1 geometry, 1 drawItem.",
        "Frequencies: uniform(0.15, 0.7). 12 bins = 30 degree sectors. Seed=206.")
    return "206-angular-histogram", d.build(), md


def trial_207():
    """3 nested donut rings with 4-6 segments each."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "donuts")

    random.seed(207)
    aspect = 960.0 / 640.0

    rings = [
        (0.15, 0.28, 4, [rgb(0.3,0.5,0.9), rgb(0.4,0.6,1.0), rgb(0.5,0.7,1.0), rgb(0.6,0.8,1.0)]),
        (0.32, 0.48, 5, [rgb(0.9,0.3,0.3), rgb(1.0,0.4,0.3), rgb(1.0,0.5,0.4), rgb(0.9,0.6,0.4), rgb(0.8,0.5,0.3)]),
        (0.52, 0.7, 6, [rgb(0.2,0.8,0.3), rgb(0.3,0.9,0.4), rgb(0.4,0.8,0.5), rgb(0.5,0.9,0.3), rgb(0.3,0.7,0.4), rgb(0.4,0.8,0.3)]),
    ]

    tri_data = []
    for r_inner, r_outer, n_segs, seg_colors in rings:
        fracs = [random.uniform(0.5, 2.0) for _ in range(n_segs)]
        total = sum(fracs)
        fracs = [f / total for f in fracs]
        angle = 0.0
        for si in range(n_segs):
            a_start = angle
            a_end = angle + fracs[si] * 2 * math.pi
            col = seg_colors[si]
            cr, cg, cb, ca = col
            n_sub = 8
            for s in range(n_sub):
                a0 = a_start + (a_end - a_start) * s / n_sub
                a1 = a_start + (a_end - a_start) * (s + 1) / n_sub
                cos0, sin0 = math.cos(a0), math.sin(a0)
                cos1, sin1 = math.cos(a1), math.sin(a1)
                ix0 = r_inner * cos0 / aspect
                iy0 = r_inner * sin0
                ox0 = r_outer * cos0 / aspect
                oy0 = r_outer * sin0
                ix1 = r_inner * cos1 / aspect
                iy1 = r_inner * sin1
                ox1 = r_outer * cos1 / aspect
                oy1 = r_outer * sin1
                tri_data.extend([ix0,iy0,cr,cg,cb,ca, ox0,oy0,cr,cg,cb,ca, ox1,oy1,cr,cg,cb,ca])
                tri_data.extend([ix0,iy0,cr,cg,cb,ca, ox1,oy1,cr,cg,cb,ca, ix1,iy1,cr,cg,cb,ca])
            angle = a_end

    d.add_draw(10, "donuts", "triGradient@1", "pos2_color4", tri_data,
               rgb(1, 1, 1))

    md = make_md(207, "concentric-donuts-3",
        "Concentric Donut Rings (3 Levels)",
        "3 nested donut rings with 4/5/6 segments each, different color palettes per ring.",
        "A 960x640 viewport with 3 concentric donut rings (triGradient@1). Inner ring: 4 blue segments. "
        "Middle ring: 5 red/orange segments. Outer ring: 6 green segments. 8 sub-segments per wedge. "
        "Aspect-ratio corrected.",
        "1 pane, 1 layer, 0 transforms, 1 buffer, 1 geometry, 1 drawItem.",
        "Segment fractions: random normalized. Ring radii: 0.15-0.28, 0.32-0.48, 0.52-0.7. Seed=207.")
    return "207-concentric-donuts-3", d.build(), md


def trial_208():
    """2-level nested treemap with outlines."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "fills")
    d.add_layer(11, 1, "outlines")

    random.seed(208)
    # 4 parent groups, 3-4 children each
    parents = []
    for p in range(4):
        n_children = random.choice([3, 4])
        children = [random.uniform(1, 5) for _ in range(n_children)]
        parents.append(children)

    # Layout: squarify-lite — just split into rows
    parent_totals = [sum(c) for c in parents]
    grand_total = sum(parent_totals)

    parent_colors = [rgb(0.3,0.4,0.7,0.3), rgb(0.7,0.3,0.3,0.3),
                     rgb(0.3,0.7,0.3,0.3), rgb(0.7,0.7,0.3,0.3)]

    tri_data = []
    outline_segs = []

    # Split viewport into 2 rows of 2 parents
    x_start, y_start = -0.9, -0.9
    total_w, total_h = 1.8, 1.8

    layouts = [
        (x_start, y_start + total_h/2, total_w * parent_totals[0]/(parent_totals[0]+parent_totals[1]), total_h/2),
        (x_start + total_w * parent_totals[0]/(parent_totals[0]+parent_totals[1]), y_start + total_h/2,
         total_w * parent_totals[1]/(parent_totals[0]+parent_totals[1]), total_h/2),
        (x_start, y_start, total_w * parent_totals[2]/(parent_totals[2]+parent_totals[3]), total_h/2),
        (x_start + total_w * parent_totals[2]/(parent_totals[2]+parent_totals[3]), y_start,
         total_w * parent_totals[3]/(parent_totals[2]+parent_totals[3]), total_h/2),
    ]

    child_colors = distinct_colors(16, s=0.6, l=0.5)
    ci = 0
    for pi, (px, py, pw, ph) in enumerate(layouts):
        children = parents[pi]
        child_total = sum(children)
        # Stack children horizontally within parent
        cx = px + 0.02
        inner_w = pw - 0.04
        inner_h = ph - 0.04
        for child_val in children:
            cw = child_val / child_total * inner_w
            col = child_colors[ci]
            cr, cg, cb, ca = col
            x0, y0 = cx, py + 0.02
            x1, y1 = cx + cw - 0.01, py + inner_h + 0.02
            tri_data.extend([x0,y0,cr,cg,cb,ca, x1,y0,cr,cg,cb,ca, x1,y1,cr,cg,cb,ca])
            tri_data.extend([x0,y0,cr,cg,cb,ca, x1,y1,cr,cg,cb,ca, x0,y1,cr,cg,cb,ca])
            cx += cw
            ci += 1

        # Parent outline
        outline_segs.extend([px, py, px+pw, py])
        outline_segs.extend([px+pw, py, px+pw, py+ph])
        outline_segs.extend([px+pw, py+ph, px, py+ph])
        outline_segs.extend([px, py+ph, px, py])

    d.add_draw(10, "fills", "triGradient@1", "pos2_color4", tri_data,
               rgb(1, 1, 1))
    d.add_draw(11, "outlines", "lineAA@1", "rect4", outline_segs,
               rgb(0.8, 0.8, 0.8), line_width=2.0)

    md = make_md(208, "nested-treemap",
        "Nested Treemap (2-Level)",
        "4 parent rectangles each containing 3-4 child rects, with parent outlines.",
        "A 960x640 viewport with ~15 child rectangles (triGradient@1) nested inside 4 parent "
        "regions. Parent outlines drawn with lineAA@1. Children sized proportional to value. "
        "2x2 parent layout, children stacked horizontally.",
        "1 pane, 2 layers, 0 transforms, 2 buffers, 2 geometries, 2 drawItems.",
        "Parent sizes by child sum. Child values: uniform(1,5). Seed=208.")
    return "208-nested-treemap", d.build(), md


def trial_209():
    """3-level sunburst: center + 4 ring-1 + 12 ring-2 sectors."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "sunburst")

    random.seed(209)
    aspect = 960.0 / 640.0
    tri_data = []

    # Center circle
    center_col = rgb(0.9, 0.7, 0.2)
    cr, cg, cb, ca = center_col
    r_center = 0.15
    n_seg = 24
    for s in range(n_seg):
        a0 = 2 * math.pi * s / n_seg
        a1 = 2 * math.pi * (s + 1) / n_seg
        tri_data.extend([0,0,cr,cg,cb,ca])
        tri_data.extend([r_center*math.cos(a0)/aspect, r_center*math.sin(a0), cr,cg,cb,ca])
        tri_data.extend([r_center*math.cos(a1)/aspect, r_center*math.sin(a1), cr,cg,cb,ca])

    # Ring 1: 4 sectors
    ring1_colors = [rgb(0.3,0.5,0.9), rgb(0.9,0.3,0.3), rgb(0.3,0.8,0.3), rgb(0.8,0.3,0.8)]
    ring1_fracs = [0.3, 0.25, 0.25, 0.2]
    ring1_inner, ring1_outer = 0.18, 0.38

    angle = 0.0
    ring1_angles = []
    for i in range(4):
        a_start = angle
        a_end = angle + ring1_fracs[i] * 2 * math.pi
        ring1_angles.append((a_start, a_end))
        col = ring1_colors[i]
        cr, cg, cb, ca = col
        for s in range(8):
            a0 = a_start + (a_end - a_start) * s / 8
            a1 = a_start + (a_end - a_start) * (s + 1) / 8
            cos0, sin0 = math.cos(a0), math.sin(a0)
            cos1, sin1 = math.cos(a1), math.sin(a1)
            ix0 = ring1_inner * cos0 / aspect
            iy0 = ring1_inner * sin0
            ox0 = ring1_outer * cos0 / aspect
            oy0 = ring1_outer * sin0
            ix1 = ring1_inner * cos1 / aspect
            iy1 = ring1_inner * sin1
            ox1 = ring1_outer * cos1 / aspect
            oy1 = ring1_outer * sin1
            tri_data.extend([ix0,iy0,cr,cg,cb,ca, ox0,oy0,cr,cg,cb,ca, ox1,oy1,cr,cg,cb,ca])
            tri_data.extend([ix0,iy0,cr,cg,cb,ca, ox1,oy1,cr,cg,cb,ca, ix1,iy1,cr,cg,cb,ca])
        angle = a_end

    # Ring 2: 3 children per ring-1 sector = 12 sectors
    ring2_inner, ring2_outer = 0.41, 0.65
    ring2_colors = distinct_colors(12, s=0.5, l=0.5)
    ci = 0
    for pi in range(4):
        pa_start, pa_end = ring1_angles[pi]
        child_fracs = [random.uniform(0.5, 2.0) for _ in range(3)]
        total = sum(child_fracs)
        child_fracs = [f / total for f in child_fracs]
        ca_start = pa_start
        for chi in range(3):
            ca_end = ca_start + child_fracs[chi] * (pa_end - pa_start)
            col = ring2_colors[ci]
            cr, cg, cb, ca = col
            for s in range(6):
                a0 = ca_start + (ca_end - ca_start) * s / 6
                a1 = ca_start + (ca_end - ca_start) * (s + 1) / 6
                cos0, sin0 = math.cos(a0), math.sin(a0)
                cos1, sin1 = math.cos(a1), math.sin(a1)
                ix0 = ring2_inner * cos0 / aspect
                iy0 = ring2_inner * sin0
                ox0 = ring2_outer * cos0 / aspect
                oy0 = ring2_outer * sin0
                ix1 = ring2_inner * cos1 / aspect
                iy1 = ring2_inner * sin1
                ox1 = ring2_outer * cos1 / aspect
                oy1 = ring2_outer * sin1
                tri_data.extend([ix0,iy0,cr,cg,cb,ca, ox0,oy0,cr,cg,cb,ca, ox1,oy1,cr,cg,cb,ca])
                tri_data.extend([ix0,iy0,cr,cg,cb,ca, ox1,oy1,cr,cg,cb,ca, ix1,iy1,cr,cg,cb,ca])
            ca_start = ca_end
            ci += 1

    d.add_draw(10, "sunburst", "triGradient@1", "pos2_color4", tri_data,
               rgb(1, 1, 1))

    md = make_md(209, "sunburst-3level",
        "3-Level Sunburst Diagram",
        "Center circle + 4 ring-1 sectors + 12 ring-2 sectors in radial hierarchy.",
        "A 960x640 viewport with a 3-level sunburst (triGradient@1). Center: gold circle. "
        "Ring 1: 4 colored sectors. Ring 2: 12 sub-sectors (3 per parent). All wedges rendered "
        "with 6-8 triangle segments for smooth arcs. Aspect-ratio corrected.",
        "1 pane, 1 layer, 0 transforms, 1 buffer, 1 geometry, 1 drawItem.",
        "Ring-1 fractions: 30/25/25/20%. Ring-2: random subdivisions per parent. Seed=209.")
    return "209-sunburst-3level", d.build(), md


def trial_210():
    """12 packed non-overlapping circles."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "circles")

    random.seed(210)
    aspect = 960.0 / 640.0

    # Simple packing: place circles with collision avoidance
    circles = []
    max_attempts = 1000
    for i in range(12):
        r = random.uniform(0.06, 0.18)
        placed = False
        for _ in range(max_attempts):
            cx = random.uniform(-0.7 + r/aspect, 0.7 - r/aspect)
            cy = random.uniform(-0.7 + r, 0.7 - r)
            ok = True
            for (ox, oy, or_) in circles:
                dx = (cx - ox) * aspect  # account for aspect ratio in distance
                dy = cy - oy
                dist = math.sqrt(dx*dx + dy*dy)
                if dist < (r + or_) * 1.1:  # small margin
                    ok = False
                    break
            if ok:
                circles.append((cx, cy, r))
                placed = True
                break
        if not placed:
            # Shrink and retry
            r *= 0.5
            cx = random.uniform(-0.6, 0.6)
            cy = random.uniform(-0.6, 0.6)
            circles.append((cx, cy, r))

    colors_list = distinct_colors(12, s=0.7, l=0.55)
    tri_data = []
    for i, (cx, cy, r) in enumerate(circles):
        col = colors_list[i]
        cr, cg, cb, ca = col
        n_seg = 32
        for s in range(n_seg):
            a0 = 2 * math.pi * s / n_seg
            a1 = 2 * math.pi * (s + 1) / n_seg
            tri_data.extend([cx, cy, cr, cg, cb, ca])
            tri_data.extend([cx + r*math.cos(a0)/aspect, cy + r*math.sin(a0), cr, cg, cb, ca])
            tri_data.extend([cx + r*math.cos(a1)/aspect, cy + r*math.sin(a1), cr, cg, cb, ca])

    d.add_draw(10, "packed", "triGradient@1", "pos2_color4", tri_data,
               rgb(1, 1, 1))

    md = make_md(210, "packed-circles",
        "Packed Circles (12)",
        "12 non-overlapping circles of varying radii packed inside a rectangular region.",
        "A 960x640 viewport with 12 circles (triGradient@1, 32-segment center-fan). "
        "Collision avoidance ensures no overlap. Radii range [0.06, 0.18]. Each circle "
        "has a distinct color. Aspect-ratio corrected.",
        "1 pane, 1 layer, 0 transforms, 1 buffer, 1 geometry, 1 drawItem.",
        "Radii: uniform(0.06, 0.18). Placement: random with collision check. Seed=210.")
    return "210-packed-circles", d.build(), md


def trial_211():
    """Approximate Voronoi: 8 regions triangulated + seed points."""
    d = DocBuilder(960, 640)
    d.add_pane(1, "Main", -0.95, 0.95)
    d.add_layer(10, 1, "regions")
    d.add_layer(11, 1, "seeds")

    random.seed(211)
    # 8 seed points
    seeds = [(random.uniform(-0.7, 0.7), random.uniform(-0.7, 0.7)) for _ in range(8)]
    colors_list = distinct_colors(8, s=0.5, l=0.45)

    # Approximate Voronoi via rasterized approach:
    # Sample a grid, assign each sample to nearest seed, build triangles
    res = 40
    tri_data = []
    cell_size_x = 1.8 / res
    cell_size_y = 1.8 / res

    for iy in range(res):
        for ix in range(res):
            cx = -0.9 + (ix + 0.5) * cell_size_x
            cy = -0.9 + (iy + 0.5) * cell_size_y
            # Find nearest seed
            best_d = float('inf')
            best_s = 0
            for si, (sx, sy) in enumerate(seeds):
                dd = (cx - sx)**2 + (cy - sy)**2
                if dd < best_d:
                    best_d = dd
                    best_s = si
            col = colors_list[best_s]
            cr, cg, cb, ca = col
            x0 = -0.9 + ix * cell_size_x
            y0 = -0.9 + iy * cell_size_y
            x1 = x0 + cell_size_x
            y1 = y0 + cell_size_y
            tri_data.extend([x0,y0,cr,cg,cb,ca, x1,y0,cr,cg,cb,ca, x1,y1,cr,cg,cb,ca])
            tri_data.extend([x0,y0,cr,cg,cb,ca, x1,y1,cr,cg,cb,ca, x0,y1,cr,cg,cb,ca])

    d.add_draw(10, "voronoi", "triGradient@1", "pos2_color4", tri_data,
               rgb(1, 1, 1))

    # Seed points
    seed_pts = []
    for sx, sy in seeds:
        seed_pts.extend([sx, sy])
    d.add_draw(11, "seeds", "points@1", "pos2_clip", seed_pts,
               rgb(1.0, 1.0, 1.0), point_size=6.0)

    md = make_md(211, "voronoi-cells",
        "Voronoi Cell Diagram",
        "8 approximate Voronoi regions via grid rasterization with seed points overlaid.",
        "A 960x640 viewport with approximate Voronoi regions (triGradient@1, 40x40 grid = "
        "1600 cells). Each grid cell colored by nearest seed. 8 white seed points overlaid. "
        "Rasterized approximation — not exact Voronoi edges.",
        "1 pane, 2 layers, 0 transforms, 2 buffers, 2 geometries, 2 drawItems.",
        "8 random seeds in [-0.7, 0.7]. 40x40 raster grid. Nearest-neighbor assignment. Seed=211.")
    return "211-voronoi-cells", d.build(), md


# ===================================================================
# Main — write all files
# ===================================================================

TRIALS = [
    trial_178, trial_179, trial_180, trial_181, trial_182, trial_183,
    trial_184, trial_185, trial_186, trial_187, trial_188, trial_189,
    trial_190, trial_191, trial_192, trial_193, trial_194, trial_195,
    trial_196, trial_197, trial_198, trial_199, trial_200, trial_201,
    trial_202, trial_203, trial_204, trial_205, trial_206, trial_207,
    trial_208, trial_209, trial_210, trial_211,
]

def main():
    for fn in TRIALS:
        slug, doc, md_text = fn()
        json_path = os.path.join(OUT_DIR, f"{slug}.json")
        md_path = os.path.join(OUT_DIR, f"{slug}.md")

        with open(json_path, "w") as f:
            json.dump(doc, f, indent=2)
            f.write("\n")
        with open(md_path, "w") as f:
            f.write(md_text)

        # Quick validation
        d = doc
        for buf_id, buf in d["buffers"].items():
            assert isinstance(buf["data"], list), f"{slug}: buffer {buf_id} data not a list"
        for geo_id, geo in d["geometries"].items():
            buf = d["buffers"][str(geo["vertexBufferId"])]
            fmt = geo["format"]
            fpv = {"pos2_clip": 2, "pos2_alpha": 3, "pos2_color4": 6,
                   "rect4": 4, "candle6": 6, "glyph8": 8, "pos2_uv4": 4}[fmt]
            expected = len(buf["data"]) // fpv
            assert geo["vertexCount"] == expected, \
                f"{slug}: geo {geo_id} vtxCount {geo['vertexCount']} != {expected}"
            assert len(buf["data"]) % fpv == 0, \
                f"{slug}: geo {geo_id} data length {len(buf['data'])} not divisible by {fpv}"
            # Pipeline-specific checks
        for di_id, di in d["drawItems"].items():
            pip = di["pipeline"]
            geo = d["geometries"][str(di["geometryId"])]
            vc = geo["vertexCount"]
            if "triSolid" in pip or "triAA" in pip or "triGradient" in pip:
                assert vc % 3 == 0, f"{slug}: di {di_id} vtxCount {vc} not %3"
            elif pip == "line2d@1":
                assert vc % 2 == 0, f"{slug}: di {di_id} vtxCount {vc} not %2"
        # Check all IDs are unique across the document
        all_ids = set()
        for section in ["buffers", "transforms", "panes", "layers", "geometries", "drawItems"]:
            for k in d.get(section, {}):
                assert k not in all_ids, f"{slug}: duplicate ID {k}"
                all_ids.add(k)

        print(f"  OK  {slug}")

    print(f"\nGenerated {len(TRIALS)} trials in {OUT_DIR}")

if __name__ == "__main__":
    main()
