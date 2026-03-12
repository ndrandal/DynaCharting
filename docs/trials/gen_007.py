#!/usr/bin/env python3
"""Generate 007-gradient-terrain-profile.json

Trial 007: A topographic elevation profile with gradient-colored terrain fill,
dashed reference lines, peak markers, and geology scatter plot.

Key features tested:
- triGradient@1 with pos2_color4 (6 floats per vertex: x, y, r, g, b, a)
- lineAA@1 with dashLength + gapLength
- points@1 for scatter/markers
"""

import json, math, os, shutil

# ═══════════════════════════════════════════════════════════════════════
# 1. ELEVATION PROFILE DATA (40 points, x=0..39)
# ═══════════════════════════════════════════════════════════════════════
# Start near sea level (~50m), rise through foothills to major peak (~1800m)
# around x=15, valley (~400m) at x=22, secondary peak (~1200m) at x=30,
# descend back to coast (~100m) at x=39.

N = 40  # points at x = 0,1,...,39

def smooth_noise(x, seed):
    """Small deterministic noise for natural look."""
    return math.sin(x * 0.9 + seed) * 30 + math.sin(x * 1.7 + seed * 2.3) * 15

# Build elevation profile using a combination of Gaussians
def elevation_at(x):
    """Return elevation in meters for position x (0..39)."""
    # Base: gentle slope starting near 50
    base = 50 + x * 2

    # Main peak centered at x=15, peak ~1750
    peak1 = 1700 * math.exp(-((x - 15) ** 2) / (2 * 5.0**2))

    # Valley suppression around x=22
    valley = -600 * math.exp(-((x - 22) ** 2) / (2 * 2.5**2))

    # Secondary peak at x=30, peak ~1200
    peak2 = 1100 * math.exp(-((x - 30) ** 2) / (2 * 3.5**2))

    # Coastal descent after x=33
    descent = 0
    if x > 33:
        descent = -(x - 33) ** 2 * 15

    elev = base + peak1 + valley + peak2 + descent + smooth_noise(x, 3.7)
    return max(elev, 10)  # never below 10m

elevations = [round(elevation_at(x), 1) for x in range(N)]

print("Elevation profile:")
for i, e in enumerate(elevations):
    bar = "#" * int(e / 30)
    print(f"  x={i:2d}: {e:7.1f}m  {bar}")

print(f"\nMax elevation: {max(elevations):.1f}m at x={elevations.index(max(elevations))}")
print(f"Min elevation: {min(elevations):.1f}m")

# ═══════════════════════════════════════════════════════════════════════
# 2. GRADIENT COLOR STOPS
# ═══════════════════════════════════════════════════════════════════════

GRADIENT_STOPS = [
    (0,    [0.05, 0.18, 0.42, 1.0]),   # sea level: deep blue
    (200,  [0.76, 0.70, 0.50, 1.0]),   # coastal: sandy tan
    (600,  [0.13, 0.55, 0.13, 1.0]),   # forest: dark green
    (1200, [0.55, 0.45, 0.35, 1.0]),   # alpine: gray-brown
    (1800, [0.92, 0.93, 0.96, 1.0]),   # snow: white
]

def color_at_y(y):
    """Interpolate gradient color at elevation y."""
    if y <= GRADIENT_STOPS[0][0]:
        return list(GRADIENT_STOPS[0][1])
    if y >= GRADIENT_STOPS[-1][0]:
        return list(GRADIENT_STOPS[-1][1])
    for i in range(len(GRADIENT_STOPS) - 1):
        y0, c0 = GRADIENT_STOPS[i]
        y1, c1 = GRADIENT_STOPS[i + 1]
        if y0 <= y <= y1:
            t = (y - y0) / (y1 - y0)
            return [round(c0[j] + (c1[j] - c0[j]) * t, 6) for j in range(4)]
    return list(GRADIENT_STOPS[-1][1])

# ═══════════════════════════════════════════════════════════════════════
# 3. TESSELLATE GRADIENT-FILLED TERRAIN (triGradient@1, pos2_color4)
# ═══════════════════════════════════════════════════════════════════════
# For each strip between x[i] and x[i+1]:
#   - Both columns go from y=0 to their respective elevations
#   - We divide into sub-bands at each gradient color stop
#   - Each sub-band gets 2 triangles (6 vertices)
#   - Each vertex color is interpolated at its Y position

def get_stop_ys():
    """Return the Y values of the gradient color stops."""
    return [s[0] for s in GRADIENT_STOPS]

def tessellate_gradient_strip(x0, x1, e0, e1):
    """Tessellate a single vertical strip from baseline 0 to elevation.
    Returns list of floats [x, y, r, g, b, a, ...] for triGradient@1.

    Strategy: for each sub-band between consecutive Y breakpoints,
    create a quad (2 triangles). Y breakpoints include:
    - y=0 (baseline)
    - each gradient stop that falls within the strip
    - min(e0, e1) (the lower of the two elevations)
    - max(e0, e1) (the higher)

    For the rectangular portion (y=0 to min_elev), both sides are full width.
    For the triangular top (if elevations differ), we need to handle the
    tapering edge carefully.
    """
    data = []
    min_elev = min(e0, e1)
    max_elev = max(e0, e1)

    # Collect all Y breakpoints in the strip
    stop_ys = get_stop_ys()
    breakpoints = set([0.0])
    for sy in stop_ys:
        if 0 < sy < max_elev:
            breakpoints.add(float(sy))
    breakpoints.add(min_elev)
    if max_elev > min_elev:
        breakpoints.add(max_elev)

    breakpoints = sorted(breakpoints)

    # For each pair of consecutive breakpoints, create a sub-band
    for bi in range(len(breakpoints) - 1):
        ybot = breakpoints[bi]
        ytop = breakpoints[bi + 1]

        if ytop <= 0:
            continue
        if ybot >= max_elev:
            continue

        # Determine x extents at ybot and ytop
        # Below min_elev: full width (x0 to x1)
        # Between min_elev and max_elev: tapering
        def x_at_y(y, x_low, x_high, e_low, e_high):
            """For the tapered top, find the x-extent at height y.
            The edge tapers from (x_low_side, min_elev) to (x_high_side, max_elev).
            Actually, we have two column heights. If e0 < e1:
              left column goes to e0, right column goes to e1
              Above e0, the left edge tapers inward
            """
            if y <= min_elev:
                return x0, x1
            if y >= max_elev:
                # Only the peak point
                if e0 >= e1:
                    return x0, x0  # peak is on the left
                else:
                    return x1, x1  # peak is on the right
            # Linear interpolation
            t = (y - min_elev) / (max_elev - min_elev)
            if e0 < e1:
                # Left side tapers, right side stays at x1
                xl = x0 + (x1 - x0) * t
                xr = x1
            else:
                # Right side tapers, left side stays at x0
                xl = x0
                xr = x1 - (x1 - x0) * t
            return xl, xr

        # Get x extents at bottom and top of this sub-band
        xl_bot, xr_bot = x_at_y(ybot, x0, x1, e0, e1)
        xl_top, xr_top = x_at_y(ytop, x0, x1, e0, e1)

        # Skip degenerate quads
        if abs(xr_bot - xl_bot) < 0.001 and abs(xr_top - xl_top) < 0.001:
            continue

        # Colors at the four corners (color depends only on Y)
        c_bot = color_at_y(ybot)
        c_top = color_at_y(ytop)

        # Two triangles forming a quad:
        # Triangle 1: bottom-left, bottom-right, top-left
        # Triangle 2: top-left, bottom-right, top-right

        # If top is degenerate (triangle tip), only emit 1 triangle
        if abs(xr_top - xl_top) < 0.001:
            # Triangle: bl, br, top_point
            xmid_top = (xl_top + xr_top) / 2
            data.extend([xl_bot, ybot] + c_bot)
            data.extend([xr_bot, ybot] + c_bot)
            data.extend([xmid_top, ytop] + c_top)
        elif abs(xr_bot - xl_bot) < 0.001:
            # Triangle: bot_point, tr, tl
            xmid_bot = (xl_bot + xr_bot) / 2
            data.extend([xmid_bot, ybot] + c_bot)
            data.extend([xr_top, ytop] + c_top)
            data.extend([xl_top, ytop] + c_top)
        else:
            # Full quad (2 triangles)
            # Tri 1: BL, BR, TL
            data.extend([xl_bot, ybot] + c_bot)
            data.extend([xr_bot, ybot] + c_bot)
            data.extend([xl_top, ytop] + c_top)
            # Tri 2: TL, BR, TR
            data.extend([xl_top, ytop] + c_top)
            data.extend([xr_bot, ybot] + c_bot)
            data.extend([xr_top, ytop] + c_top)

    return data

# Generate all gradient fill data
gradient_data = []
for i in range(N - 1):
    strip = tessellate_gradient_strip(float(i), float(i + 1), elevations[i], elevations[i + 1])
    gradient_data.extend(strip)

gradient_data = [round(v, 6) for v in gradient_data]

# pos2_color4: 6 floats per vertex, must be multiple of 3 (triangles)
assert len(gradient_data) % 6 == 0, f"Gradient data not multiple of 6: {len(gradient_data)}"
gradient_vertex_count = len(gradient_data) // 6
assert gradient_vertex_count % 3 == 0, f"Gradient vertex count not multiple of 3: {gradient_vertex_count}"
print(f"\nGradient fill: {gradient_vertex_count} vertices, {len(gradient_data)} floats, {gradient_vertex_count // 3} triangles")

# ═══════════════════════════════════════════════════════════════════════
# 4. TERRAIN OUTLINE (lineAA@1, rect4)
# ═══════════════════════════════════════════════════════════════════════
# 39 segments connecting the 40 elevation points

outline_data = []
for i in range(N - 1):
    outline_data.extend([float(i), elevations[i], float(i + 1), elevations[i + 1]])
outline_data = [round(v, 6) for v in outline_data]
outline_segments = N - 1  # 39

assert len(outline_data) == outline_segments * 4
print(f"Terrain outline: {outline_segments} segments, {len(outline_data)} floats")

# ═══════════════════════════════════════════════════════════════════════
# 5. DASHED REFERENCE LINES (lineAA@1, rect4)
# ═══════════════════════════════════════════════════════════════════════
# Each is a single horizontal segment spanning x=-1 to x=40

def make_hline(y_val):
    """Single horizontal lineAA segment at given y."""
    return [-1.0, float(y_val), 40.0, float(y_val)]

dash_line_200 = make_hline(200)   # Tree Line
dash_line_600 = make_hline(600)   # Alpine Zone
dash_line_1200 = make_hline(1200) # Snow Line

# ═══════════════════════════════════════════════════════════════════════
# 6. PEAK MARKERS (points@1, pos2_clip)
# ═══════════════════════════════════════════════════════════════════════
# Two separate draw items for different colors and point sizes

peak1_x = 15
peak2_x = 30
peak1_y = elevations[peak1_x]
peak2_y = elevations[peak2_x]

# Major peak (red)
peak1_data = [float(peak1_x), peak1_y]
# Secondary peak (orange)
peak2_data = [float(peak2_x), peak2_y]

print(f"\nPeak 1: ({peak1_x}, {peak1_y}m)")
print(f"Peak 2: ({peak2_x}, {peak2_y}m)")

# ═══════════════════════════════════════════════════════════════════════
# 7. GEOLOGY SCATTER PLOT DATA (points@1, pos2_clip)
# ═══════════════════════════════════════════════════════════════════════
# Pane 2: 150 points in 3 categories
# Data space: x = 0..39 (distance), y = 0..150 (depth, surface at top)

def pseudo_scatter(count, x_base, x_range, y_base, y_range, seed_x, seed_y):
    """Generate deterministic pseudo-random scatter points."""
    data = []
    for i in range(count):
        x = x_base + ((i * seed_x * 7.3) % x_range)
        y = y_base + ((i * seed_y * 5.7) % y_range)
        data.extend([round(x, 3), round(y, 3)])
    return data

# Sedimentary: 60 points, scattered broadly x=0-39, shallow y=0-80
sedimentary_data = []
for i in range(60):
    x = (i * 7.3 + 1.1) % 39.0
    y = (i * 5.7 + 2.3) % 80.0
    sedimentary_data.extend([round(x, 3), round(y, 3)])

# Igneous: 50 points, clustered near peaks x=10-20 and x=27-33, y=20-120
igneous_data = []
for i in range(50):
    if i < 30:
        x = 10.0 + (i * 7.3 + 0.5) % 10.0  # x=10-20
    else:
        x = 27.0 + (i * 7.3 + 0.5) % 6.0   # x=27-33
    y = 20.0 + (i * 5.7 + 3.1) % 100.0
    igneous_data.extend([round(x, 3), round(y, 3)])

# Metamorphic: 40 points, scattered deep y=60-150
metamorphic_data = []
for i in range(40):
    x = (i * 7.3 + 4.7) % 39.0
    y = 60.0 + (i * 5.7 + 1.9) % 90.0
    metamorphic_data.extend([round(x, 3), round(y, 3)])

print(f"\nGeology scatter: sedimentary={len(sedimentary_data)//2}, igneous={len(igneous_data)//2}, metamorphic={len(metamorphic_data)//2}")

# ═══════════════════════════════════════════════════════════════════════
# 8. LAYOUT COMPUTATION
# ═══════════════════════════════════════════════════════════════════════

W, H = 1100, 700

# Pane regions in clip space
# Outer padding: 1% → 0.02 clip on each side → clipMin=-0.98, clipMax=0.98
# Total clip Y range: 1.96
# Gap: 2% of 700px = 14px → 14/350 = 0.04 clip
# Pane 1: 73% height → 73/98 * (1.96 - 0.04) = 1.4302 clip
# Pane 2: 25% height → 25/98 * (1.96 - 0.04) = 0.4898 clip

CX_MIN = -0.98
CX_MAX = 0.98
CY_MIN = -0.98
CY_MAX = 0.98

total_clip_y = CY_MAX - CY_MIN  # 1.96
gap_clip = 0.04
avail_clip = total_clip_y - gap_clip  # 1.92

pane1_height_clip = round(avail_clip * 73 / 98, 4)  # 1.4302
pane2_height_clip = round(avail_clip * 25 / 98, 4)  # 0.4898

pane1_top = CY_MAX
pane1_bot = round(CY_MAX - pane1_height_clip, 4)
pane2_top = round(pane1_bot - gap_clip, 4)
pane2_bot = CY_MIN

# Verify
print(f"\n--- Layout ---")
print(f"Pane 1: clipY [{pane1_bot}, {pane1_top}], height {pane1_height_clip:.4f}")
print(f"Pane 2: clipY [{pane2_bot}, {pane2_top}], height {pane2_top - pane2_bot:.4f}")
print(f"Gap: clipY [{pane1_bot}, {pane2_top}] = {pane2_top - pane1_bot:.4f} (should be ~ -0.04)")

# Pixel verification
p1_px_top = (1 - pane1_top) / 2 * H
p1_px_bot = (1 - pane1_bot) / 2 * H
p2_px_top = (1 - pane2_top) / 2 * H
p2_px_bot = (1 - pane2_bot) / 2 * H
print(f"\nPixels:")
print(f"Pane 1: y={p1_px_top:.0f} to {p1_px_bot:.0f}, height={p1_px_bot - p1_px_top:.0f}px ({(p1_px_bot - p1_px_top)/H*100:.1f}%)")
print(f"Pane 2: y={p2_px_top:.0f} to {p2_px_bot:.0f}, height={p2_px_bot - p2_px_top:.0f}px ({(p2_px_bot - p2_px_top)/H*100:.1f}%)")
print(f"Gap: {p2_px_top - p1_px_bot:.0f}px")

# ═══════════════════════════════════════════════════════════════════════
# 9. TRANSFORMS — computed from viewport data ranges
# ═══════════════════════════════════════════════════════════════════════
# Pane 1 viewport: xMin=-1, xMax=40, yMin=-50, yMax=1950
# Pane 2 viewport: xMin=-1, xMax=40, yMin=-5, yMax=160

# Transform 50 (Pane 1)
vp1_xMin, vp1_xMax = -1.0, 40.0
vp1_yMin, vp1_yMax = -50.0, 1950.0

sx1 = (CX_MAX - CX_MIN) / (vp1_xMax - vp1_xMin)
tx1 = CX_MIN - vp1_xMin * sx1
sy1 = (pane1_top - pane1_bot) / (vp1_yMax - vp1_yMin)
ty1 = pane1_bot - vp1_yMin * sy1

print(f"\nTransform 50 (Terrain): sx={sx1:.9f}, sy={sy1:.9f}, tx={tx1:.9f}, ty={ty1:.9f}")

# Verify: data (0, 0) -> should be near left edge, near bottom
p_00 = (0 * sx1 + tx1, 0 * sy1 + ty1)
p_15_peak = (15 * sx1 + tx1, peak1_y * sy1 + ty1)
print(f"  Data (0, 0) -> clip ({p_00[0]:.4f}, {p_00[1]:.4f})")
print(f"  Data (15, {peak1_y}) -> clip ({p_15_peak[0]:.4f}, {p_15_peak[1]:.4f})")

# Transform 51 (Pane 2)
vp2_xMin, vp2_xMax = -1.0, 40.0
vp2_yMin, vp2_yMax = -5.0, 160.0

sx2 = (CX_MAX - CX_MIN) / (vp2_xMax - vp2_xMin)
tx2 = CX_MIN - vp2_xMin * sx2
sy2 = (pane2_top - pane2_bot) / (vp2_yMax - vp2_yMin)
ty2 = pane2_bot - vp2_yMin * sy2

print(f"\nTransform 51 (Geology): sx={sx2:.9f}, sy={sy2:.9f}, tx={tx2:.9f}, ty={ty2:.9f}")

# ═══════════════════════════════════════════════════════════════════════
# 10. ID ALLOCATION
# ═══════════════════════════════════════════════════════════════════════
# Panes:       1, 2
# Layers:     10-19
# Transforms: 50, 51
# Buffers:    100, 103, 106, 109, 112, 115, 118, 121, 124, 127
# Geometries: 101, 104, 107, 110, 113, 116, 119, 122, 125, 128
# DrawItems:  102, 105, 108, 111, 114, 117, 120, 123, 126, 129

# Pane 1 layers:
#   10: gradient fill (triGradient@1)
#   11: dashed reference lines (lineAA@1)
#   12: terrain outline (lineAA@1)
#   13: peak markers (points@1)
# Pane 2 layers:
#   14: sedimentary (points@1)
#   15: igneous (points@1)
#   16: metamorphic (points@1)

# Draw elements:
#  Gradient terrain fill: buf=100, geom=101, di=102  (layer 10)
#  Terrain outline:       buf=103, geom=104, di=105  (layer 12)
#  Dash line 200m:        buf=106, geom=107, di=108  (layer 11)
#  Dash line 600m:        buf=109, geom=110, di=111  (layer 11)
#  Dash line 1200m:       buf=112, geom=113, di=114  (layer 11)
#  Peak marker 1 (red):   buf=115, geom=116, di=117  (layer 13)
#  Peak marker 2 (orange):buf=118, geom=119, di=120  (layer 13)
#  Sedimentary scatter:   buf=121, geom=122, di=123  (layer 14)
#  Igneous scatter:       buf=124, geom=125, di=126  (layer 15)
#  Metamorphic scatter:   buf=127, geom=128, di=129  (layer 16)

# ═══════════════════════════════════════════════════════════════════════
# 11. HEX COLOR HELPER
# ═══════════════════════════════════════════════════════════════════════

def hex_to_rgba(h, a=1.0):
    h = h.lstrip('#')
    r = int(h[0:2], 16) / 255.0
    g = int(h[2:4], 16) / 255.0
    b = int(h[4:6], 16) / 255.0
    return [round(r, 4), round(g, 4), round(b, 4), round(a, 4)]

# ═══════════════════════════════════════════════════════════════════════
# 12. TEXT OVERLAY
# ═══════════════════════════════════════════════════════════════════════

text_labels = []

# Title: top-left of pane 1
text_labels.append({
    "clipX": round(CX_MIN + 0.02, 4),
    "clipY": round(pane1_top - 0.03, 4),
    "text": "Alpine Transect: Elevation Profile",
    "align": "l", "fontSize": 14, "color": "#ffffff"
})

# Subtitle: bottom-center of pane 1
text_labels.append({
    "clipX": 0.0,
    "clipY": round(pane1_bot + 0.02, 4),
    "text": "Distance (km)",
    "align": "c", "fontSize": 11, "color": "#888888"
})

# Zone labels (right-aligned, next to dashed lines)
zone_labels = [
    (200,  "Tree Line 200m",  "#4a7c59"),
    (600,  "Alpine 600m",     "#8B7355"),
    (1200, "Snow Line 1200m", "#a0a8b0"),
]
for y_val, label, color in zone_labels:
    clip_y = y_val * sy1 + ty1
    text_labels.append({
        "clipX": round(CX_MAX - 0.02, 4),
        "clipY": round(clip_y, 4),
        "text": label,
        "align": "r", "fontSize": 10, "color": color
    })

# Y-axis labels: 0m, 600m, 1200m, 1800m
for y_val in [0, 600, 1200, 1800]:
    clip_y = y_val * sy1 + ty1
    text_labels.append({
        "clipX": round(CX_MIN + 0.01, 4),
        "clipY": round(clip_y, 4),
        "text": f"{y_val}m",
        "align": "l", "fontSize": 10, "color": "#888888"
    })

# Pane 2 title
text_labels.append({
    "clipX": round(CX_MIN + 0.02, 4),
    "clipY": round(pane2_top - 0.03, 4),
    "text": "Subsurface Geology",
    "align": "l", "fontSize": 12, "color": "#ffffff"
})

# Legend for pane 2
legend_items = [
    ("Sedimentary",  "#D4A574"),
    ("Igneous",      "#C62828"),
    ("Metamorphic",  "#1565C0"),
]
for i, (name, color) in enumerate(legend_items):
    text_labels.append({
        "clipX": round(CX_MAX - 0.02, 4),
        "clipY": round(pane2_top - 0.04 - i * 0.05, 4),
        "text": name,
        "align": "r", "fontSize": 10, "color": color
    })

# ═══════════════════════════════════════════════════════════════════════
# 13. ASSEMBLE DOCUMENT
# ═══════════════════════════════════════════════════════════════════════

doc = {
    "version": 1,
    "viewport": {"width": W, "height": H},
    "buffers": {
        "100": {"data": gradient_data},
        "103": {"data": outline_data},
        "106": {"data": dash_line_200},
        "109": {"data": dash_line_600},
        "112": {"data": dash_line_1200},
        "115": {"data": peak1_data},
        "118": {"data": peak2_data},
        "121": {"data": sedimentary_data},
        "124": {"data": igneous_data},
        "127": {"data": metamorphic_data},
    },
    "transforms": {
        "50": {
            "sx": round(sx1, 9),
            "sy": round(sy1, 9),
            "tx": round(tx1, 9),
            "ty": round(ty1, 9),
        },
        "51": {
            "sx": round(sx2, 9),
            "sy": round(sy2, 9),
            "tx": round(tx2, 9),
            "ty": round(ty2, 9),
        },
    },
    "panes": {
        "1": {
            "name": "Terrain",
            "region": {
                "clipYMin": pane1_bot,
                "clipYMax": pane1_top,
                "clipXMin": CX_MIN,
                "clipXMax": CX_MAX,
            },
            "hasClearColor": True,
            "clearColor": [0.08, 0.10, 0.14, 1.0],
        },
        "2": {
            "name": "Geology",
            "region": {
                "clipYMin": pane2_bot,
                "clipYMax": pane2_top,
                "clipXMin": CX_MIN,
                "clipXMax": CX_MAX,
            },
            "hasClearColor": True,
            "clearColor": [0.08, 0.10, 0.14, 1.0],
        },
    },
    "layers": {
        "10": {"paneId": 1, "name": "GradientFill"},
        "11": {"paneId": 1, "name": "DashLines"},
        "12": {"paneId": 1, "name": "Outline"},
        "13": {"paneId": 1, "name": "Peaks"},
        "14": {"paneId": 2, "name": "Sedimentary"},
        "15": {"paneId": 2, "name": "Igneous"},
        "16": {"paneId": 2, "name": "Metamorphic"},
    },
    "geometries": {
        "101": {"vertexBufferId": 100, "format": "pos2_color4", "vertexCount": gradient_vertex_count},
        "104": {"vertexBufferId": 103, "format": "rect4", "vertexCount": outline_segments},
        "107": {"vertexBufferId": 106, "format": "rect4", "vertexCount": 1},
        "110": {"vertexBufferId": 109, "format": "rect4", "vertexCount": 1},
        "113": {"vertexBufferId": 112, "format": "rect4", "vertexCount": 1},
        "116": {"vertexBufferId": 115, "format": "pos2_clip", "vertexCount": 1},
        "119": {"vertexBufferId": 118, "format": "pos2_clip", "vertexCount": 1},
        "122": {"vertexBufferId": 121, "format": "pos2_clip", "vertexCount": len(sedimentary_data) // 2},
        "125": {"vertexBufferId": 124, "format": "pos2_clip", "vertexCount": len(igneous_data) // 2},
        "128": {"vertexBufferId": 127, "format": "pos2_clip", "vertexCount": len(metamorphic_data) // 2},
    },
    "drawItems": {
        # Gradient terrain fill
        "102": {
            "layerId": 10, "name": "TerrainGradient",
            "pipeline": "triGradient@1", "geometryId": 101, "transformId": 50,
            "color": [1.0, 1.0, 1.0, 1.0],
        },
        # Terrain outline
        "105": {
            "layerId": 12, "name": "TerrainOutline",
            "pipeline": "lineAA@1", "geometryId": 104, "transformId": 50,
            "color": hex_to_rgba("e0e0e0", 1.0), "lineWidth": 1.5,
        },
        # Dashed line: Tree Line 200m
        "108": {
            "layerId": 11, "name": "TreeLine200",
            "pipeline": "lineAA@1", "geometryId": 107, "transformId": 50,
            "color": hex_to_rgba("4a7c59", 1.0), "lineWidth": 1.0,
            "dashLength": 12.0, "gapLength": 8.0,
        },
        # Dashed line: Alpine Zone 600m
        "111": {
            "layerId": 11, "name": "Alpine600",
            "pipeline": "lineAA@1", "geometryId": 110, "transformId": 50,
            "color": hex_to_rgba("8B7355", 1.0), "lineWidth": 1.0,
            "dashLength": 10.0, "gapLength": 6.0,
        },
        # Dashed line: Snow Line 1200m
        "114": {
            "layerId": 11, "name": "SnowLine1200",
            "pipeline": "lineAA@1", "geometryId": 113, "transformId": 50,
            "color": hex_to_rgba("a0a8b0", 1.0), "lineWidth": 1.0,
            "dashLength": 8.0, "gapLength": 8.0,
        },
        # Peak marker 1: major peak (red)
        "117": {
            "layerId": 13, "name": "MajorPeak",
            "pipeline": "points@1", "geometryId": 116, "transformId": 50,
            "color": hex_to_rgba("FF4444", 1.0), "pointSize": 8.0,
        },
        # Peak marker 2: secondary peak (orange)
        "120": {
            "layerId": 13, "name": "SecondaryPeak",
            "pipeline": "points@1", "geometryId": 119, "transformId": 50,
            "color": hex_to_rgba("FF8800", 1.0), "pointSize": 7.0,
        },
        # Scatter: sedimentary
        "123": {
            "layerId": 14, "name": "Sedimentary",
            "pipeline": "points@1", "geometryId": 122, "transformId": 51,
            "color": hex_to_rgba("D4A574", 1.0), "pointSize": 4.0,
        },
        # Scatter: igneous
        "126": {
            "layerId": 15, "name": "Igneous",
            "pipeline": "points@1", "geometryId": 125, "transformId": 51,
            "color": hex_to_rgba("C62828", 1.0), "pointSize": 4.5,
        },
        # Scatter: metamorphic
        "129": {
            "layerId": 16, "name": "Metamorphic",
            "pipeline": "points@1", "geometryId": 128, "transformId": 51,
            "color": hex_to_rgba("1565C0", 1.0), "pointSize": 4.0,
        },
    },
    "viewports": {
        "terrain": {
            "transformId": 50, "paneId": 1,
            "xMin": vp1_xMin, "xMax": vp1_xMax,
            "yMin": vp1_yMin, "yMax": vp1_yMax,
            "linkGroup": "transect",
        },
        "geology": {
            "transformId": 51, "paneId": 2,
            "xMin": vp2_xMin, "xMax": vp2_xMax,
            "yMin": vp2_yMin, "yMax": vp2_yMax,
            "linkGroup": "transect",
            "panY": False, "zoomY": False,
        },
    },
    "textOverlay": {
        "fontSize": 12,
        "color": "#b2b5bc",
        "labels": text_labels,
    },
}

# ═══════════════════════════════════════════════════════════════════════
# 14. VALIDATION
# ═══════════════════════════════════════════════════════════════════════

# Check ID uniqueness
all_ids = set()
for section in ["panes", "layers", "transforms", "buffers", "geometries", "drawItems"]:
    if section in doc:
        for k in doc[section]:
            kid = int(k)
            assert kid not in all_ids, f"DUPLICATE ID {kid} in {section}!"
            all_ids.add(kid)
print(f"\nAll {len(all_ids)} IDs unique: {sorted(all_ids)}")

# Verify vertex counts match buffer data sizes
format_floats = {
    "pos2_clip": 2,
    "pos2_alpha": 3,
    "pos2_color4": 6,
    "rect4": 4,
    "candle6": 6,
    "glyph8": 8,
    "pos2_uv4": 4,
}

for gid, g in doc["geometries"].items():
    bid = str(g["vertexBufferId"])
    buf_data = doc["buffers"][bid]["data"]
    fmt = g["format"]
    vc = g["vertexCount"]
    fpv = format_floats[fmt]
    expected_floats = vc * fpv
    actual_floats = len(buf_data)
    assert actual_floats == expected_floats, \
        f"Geom {gid} ({fmt}): expected {expected_floats} floats ({vc} verts × {fpv}), got {actual_floats}"
    print(f"Geom {gid} ({fmt}): {vc} verts, {actual_floats} floats OK")

# Verify triGradient vertex count is multiple of 3
assert gradient_vertex_count % 3 == 0, f"triGradient vertexCount {gradient_vertex_count} not multiple of 3!"

# ═══════════════════════════════════════════════════════════════════════
# 15. WRITE OUTPUT
# ═══════════════════════════════════════════════════════════════════════

out_dir = os.path.dirname(os.path.abspath(__file__))
out_path = os.path.join(out_dir, "007-gradient-terrain-profile.json")
with open(out_path, 'w') as f:
    json.dump(doc, f, indent=2)
print(f"\nWrote {out_path} ({os.path.getsize(out_path)} bytes)")

# Copy to charts/
charts_dir = os.path.join(out_dir, "..", "..", "charts")
charts_path = os.path.join(charts_dir, "007-gradient-terrain-profile.json")
shutil.copy2(out_path, charts_path)
print(f"Copied to {charts_path}")

print("\n=== DONE ===")
