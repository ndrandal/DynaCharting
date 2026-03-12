#!/usr/bin/env python3
"""Generate 005-stacked-area-chart.json"""
import json, math, os, shutil

# ── Data: 24 months of market share ──────────────────────────────────
# Series A (Desktop): ~55 → ~35, smooth decline
# Series B (Mobile):  ~25 → ~42, smooth growth
# Series C (Tablet):  ~12, peaks ~16 around month 12, back to ~13
# Series D (Other):   remainder (~8-12)

N = 24  # months 0..23

def lerp(a, b, t):
    return a + (b - a) * t

def smooth_noise(i, seed):
    """Deterministic smooth pseudo-noise"""
    return math.sin(i * 0.7 + seed) * 0.3 + math.sin(i * 1.3 + seed * 2.1) * 0.2

A = []
B = []
C = []
D = []

for i in range(N):
    t = i / (N - 1)  # 0..1

    # Desktop: smooth decline from 55 to 35 with mild noise
    a = lerp(55, 35, t) + smooth_noise(i, 1.0) * 1.5

    # Mobile: smooth growth from 25 to 42 with mild noise
    b = lerp(25, 42, t) + smooth_noise(i, 2.5) * 1.2

    # Tablet: bell curve peaking at month 12, ~12 → ~16 → ~13
    tablet_base = 12.5
    tablet_peak = 4.0 * math.exp(-((i - 12) ** 2) / (2 * 5.0**2))
    c = tablet_base + tablet_peak + smooth_noise(i, 4.0) * 0.6

    # Other: fills to ~100 with small noise
    d_raw = 100 - a - b - c
    d = max(d_raw + smooth_noise(i, 6.0) * 0.3, 3.0)

    # Normalize so they sum to exactly 100
    total = a + b + c + d
    scale = 100.0 / total
    a *= scale
    b *= scale
    c *= scale
    d *= scale

    A.append(round(a, 4))
    B.append(round(b, 4))
    C.append(round(c, 4))
    D.append(round(d, 4))

# Print data for verification
print("Month | Desktop |  Mobile |  Tablet |   Other |   Total")
print("------+---------+---------+---------+---------+--------")
for i in range(N):
    total = A[i] + B[i] + C[i] + D[i]
    print(f"  {i:3d} | {A[i]:7.2f} | {B[i]:7.2f} | {C[i]:7.2f} | {D[i]:7.2f} | {total:7.2f}")

# ── Compute cumulative stacks ────────────────────────────────────────
# bottom_A = 0 (baseline)
# top_A = A
# bottom_B = top_A
# top_B = top_A + B
# bottom_C = top_B
# top_C = top_B + C
# bottom_D = top_C
# top_D = top_C + D

top_A = [A[i] for i in range(N)]
top_B = [A[i] + B[i] for i in range(N)]
top_C = [A[i] + B[i] + C[i] for i in range(N)]
top_D = [A[i] + B[i] + C[i] + D[i] for i in range(N)]

# ── Tessellate area fills ────────────────────────────────────────────
# For each segment i (0..22), 6 vertices (2 triangles):
# Tri1: (i, top[i]), (i, bottom[i]), (i+1, top[i+1])
# Tri2: (i+1, top[i+1]), (i, bottom[i]), (i+1, bottom[i+1])
# pos2_clip format: [x, y] per vertex, 2 floats per vertex

def tessellate_area(top_arr, bottom_arr):
    """Returns flat float array for triSolid@1 pos2_clip area fill."""
    data = []
    for i in range(N - 1):
        x0 = float(i)
        x1 = float(i + 1)
        t0 = top_arr[i]
        t1 = top_arr[i + 1]
        b0 = bottom_arr[i]
        b1 = bottom_arr[i + 1]
        # Triangle 1: top-left, bottom-left, top-right
        data.extend([x0, t0, x0, b0, x1, t1])
        # Triangle 2: top-right, bottom-left, bottom-right
        data.extend([x1, t1, x0, b0, x1, b1])
    return [round(v, 6) for v in data]

baseline = [0.0] * N

area_A_data = tessellate_area(top_A, baseline)    # Desktop: 0 to top_A
area_B_data = tessellate_area(top_B, top_A)        # Mobile: top_A to top_B
area_C_data = tessellate_area(top_C, top_B)        # Tablet: top_B to top_C
area_D_data = tessellate_area(top_D, top_C)        # Other: top_C to top_D

AREA_VERTS = 23 * 6  # = 138 vertices per area fill
assert len(area_A_data) == AREA_VERTS * 2, f"Area A: {len(area_A_data)} != {AREA_VERTS * 2}"
assert len(area_B_data) == AREA_VERTS * 2
assert len(area_C_data) == AREA_VERTS * 2
assert len(area_D_data) == AREA_VERTS * 2

# ── Build boundary lines ─────────────────────────────────────────────
# lineAA@1 with rect4 format: [x0, y0, x1, y1] per segment
# 23 segments per line (24 points → 23 segments)

def build_line(y_arr):
    """Returns flat float array for lineAA@1 rect4 line segments."""
    data = []
    for i in range(N - 1):
        data.extend([float(i), y_arr[i], float(i + 1), y_arr[i + 1]])
    return [round(v, 6) for v in data]

line_A_data = build_line(top_A)
line_B_data = build_line(top_B)
line_C_data = build_line(top_C)
line_D_data = build_line(top_D)

LINE_SEGS = 23  # segments per line
assert len(line_A_data) == LINE_SEGS * 4, f"Line A: {len(line_A_data)} != {LINE_SEGS * 4}"
assert len(line_B_data) == LINE_SEGS * 4
assert len(line_C_data) == LINE_SEGS * 4
assert len(line_D_data) == LINE_SEGS * 4

# ── Transform ────────────────────────────────────────────────────────
# Data range: x [-0.5, 23.5], y [-2, 105]
# Pane clip: x [-0.92, 0.92], y [-0.92, 0.92]
xMin, xMax = -0.5, 23.5
yMin, yMax = -2.0, 105.0
cxMin, cxMax = -0.92, 0.92
cyMin, cyMax = -0.92, 0.92

sx = (cxMax - cxMin) / (xMax - xMin)
tx = cxMin - xMin * sx
sy = (cyMax - cyMin) / (yMax - yMin)
ty = cyMin - yMin * sy

print(f"\nTransform: sx={sx:.9f}, sy={sy:.9f}, tx={tx:.9f}, ty={ty:.9f}")

# Verify corners:
# (0, 0) -> (0 * sx + tx, 0 * sy + ty) = (tx, ty) = should be inside pane
# (23, 100) -> should be inside pane
p00 = (0 * sx + tx, 0 * sy + ty)
p23_100 = (23 * sx + tx, 100 * sy + ty)
print(f"Data (0, 0) -> clip ({p00[0]:.4f}, {p00[1]:.4f})")
print(f"Data (23, 100) -> clip ({p23_100[0]:.4f}, {p23_100[1]:.4f})")

# ── Hex color to RGBA ────────────────────────────────────────────────
def hex_to_rgba(h, a=1.0):
    h = h.lstrip('#')
    r = int(h[0:2], 16) / 255.0
    g = int(h[2:4], 16) / 255.0
    b = int(h[4:6], 16) / 255.0
    return [round(r, 4), round(g, 4), round(b, 4), round(a, 4)]

# Colors from spec
color_A_fill  = hex_to_rgba("1565C0", 0.75)  # dark blue
color_B_fill  = hex_to_rgba("43A047", 0.75)  # green
color_C_fill  = hex_to_rgba("FB8C00", 0.75)  # orange
color_D_fill  = hex_to_rgba("8E24AA", 0.75)  # purple

color_A_line  = hex_to_rgba("1976D2", 1.0)   # blue
color_B_line  = hex_to_rgba("66BB6A", 1.0)   # green
color_C_line  = hex_to_rgba("FFA726", 1.0)   # orange
color_D_line  = hex_to_rgba("AB47BC", 1.0)   # purple

# ── Text overlay ─────────────────────────────────────────────────────
text_labels = []

# Title top-center
text_labels.append({
    "clipX": 0.0, "clipY": 0.96, "text": "Device Market Share 2024-2025",
    "align": "c", "fontSize": 15, "color": "#ffffff"
})

# Y-axis labels on left: 0%, 25%, 50%, 75%, 100%
for pct in [0, 25, 50, 75, 100]:
    clip_y = pct * sy + ty
    text_labels.append({
        "clipX": -0.97, "clipY": round(clip_y, 4), "text": f"{pct}%",
        "align": "l", "fontSize": 11, "color": "#888888"
    })

# X-axis labels along bottom: month indices 0,3,6,9,12,15,18,21 → Jan,Apr,Jul,Oct,Jan,Apr,Jul,Oct
month_labels = ["Jan", "Apr", "Jul", "Oct", "Jan'25", "Apr", "Jul", "Oct"]
month_indices = [0, 3, 6, 9, 12, 15, 18, 21]
for idx, label in zip(month_indices, month_labels):
    clip_x = idx * sx + tx
    text_labels.append({
        "clipX": round(clip_x, 4), "clipY": -0.97, "text": label,
        "align": "c", "fontSize": 11, "color": "#888888"
    })

# Legend in top-right
legend_items = [
    ("Desktop", "#1976D2"),
    ("Mobile",  "#66BB6A"),
    ("Tablet",  "#FFA726"),
    ("Other",   "#AB47BC"),
]
for i, (name, color) in enumerate(legend_items):
    text_labels.append({
        "clipX": 0.78, "clipY": round(0.88 - i * 0.06, 4),
        "text": name, "align": "l", "fontSize": 11, "color": color
    })

# ── Assemble document ────────────────────────────────────────────────
doc = {
    "version": 1,
    "viewport": {"width": 1100, "height": 650},
    "buffers": {
        "100": {"data": area_A_data},
        "103": {"data": area_B_data},
        "106": {"data": area_C_data},
        "109": {"data": area_D_data},
        "112": {"data": line_A_data},
        "115": {"data": line_B_data},
        "118": {"data": line_C_data},
        "121": {"data": line_D_data},
    },
    "transforms": {
        "50": {
            "sx": round(sx, 9),
            "sy": round(sy, 9),
            "tx": round(tx, 9),
            "ty": round(ty, 9),
        }
    },
    "panes": {
        "1": {
            "name": "main",
            "region": {
                "clipYMin": -0.92,
                "clipYMax": 0.92,
                "clipXMin": -0.92,
                "clipXMax": 0.92,
            },
            "hasClearColor": True,
            "clearColor": [0.10, 0.10, 0.15, 1.0],
        }
    },
    "layers": {
        "10": {"paneId": 1, "name": "areaA"},
        "11": {"paneId": 1, "name": "areaB"},
        "12": {"paneId": 1, "name": "areaC"},
        "13": {"paneId": 1, "name": "areaD"},
        "14": {"paneId": 1, "name": "lineA"},
        "15": {"paneId": 1, "name": "lineB"},
        "16": {"paneId": 1, "name": "lineC"},
        "17": {"paneId": 1, "name": "lineD"},
    },
    "geometries": {
        "101": {"vertexBufferId": 100, "format": "pos2_clip", "vertexCount": AREA_VERTS},
        "104": {"vertexBufferId": 103, "format": "pos2_clip", "vertexCount": AREA_VERTS},
        "107": {"vertexBufferId": 106, "format": "pos2_clip", "vertexCount": AREA_VERTS},
        "110": {"vertexBufferId": 109, "format": "pos2_clip", "vertexCount": AREA_VERTS},
        "113": {"vertexBufferId": 112, "format": "rect4", "vertexCount": LINE_SEGS},
        "116": {"vertexBufferId": 115, "format": "rect4", "vertexCount": LINE_SEGS},
        "119": {"vertexBufferId": 118, "format": "rect4", "vertexCount": LINE_SEGS},
        "122": {"vertexBufferId": 121, "format": "rect4", "vertexCount": LINE_SEGS},
    },
    "drawItems": {
        # Area fills (lower layers → drawn first)
        "102": {
            "layerId": 10, "name": "AreaDesktop",
            "pipeline": "triSolid@1", "geometryId": 101, "transformId": 50,
            "color": color_A_fill,
        },
        "105": {
            "layerId": 11, "name": "AreaMobile",
            "pipeline": "triSolid@1", "geometryId": 104, "transformId": 50,
            "color": color_B_fill,
        },
        "108": {
            "layerId": 12, "name": "AreaTablet",
            "pipeline": "triSolid@1", "geometryId": 107, "transformId": 50,
            "color": color_C_fill,
        },
        "111": {
            "layerId": 13, "name": "AreaOther",
            "pipeline": "triSolid@1", "geometryId": 110, "transformId": 50,
            "color": color_D_fill,
        },
        # Boundary lines (higher layers → drawn on top)
        "114": {
            "layerId": 14, "name": "LineDesktop",
            "pipeline": "lineAA@1", "geometryId": 113, "transformId": 50,
            "color": color_A_line, "lineWidth": 2.0,
        },
        "117": {
            "layerId": 15, "name": "LineMobile",
            "pipeline": "lineAA@1", "geometryId": 116, "transformId": 50,
            "color": color_B_line, "lineWidth": 2.0,
        },
        "120": {
            "layerId": 16, "name": "LineTablet",
            "pipeline": "lineAA@1", "geometryId": 119, "transformId": 50,
            "color": color_C_line, "lineWidth": 2.0,
        },
        "123": {
            "layerId": 17, "name": "LineOther",
            "pipeline": "lineAA@1", "geometryId": 122, "transformId": 50,
            "color": color_D_line, "lineWidth": 1.5,
        },
    },
    "viewports": {
        "main": {
            "transformId": 50,
            "paneId": 1,
            "xMin": -0.5,
            "xMax": 23.5,
            "yMin": -2.0,
            "yMax": 105.0,
            "panY": False,
            "zoomY": False,
        }
    },
    "textOverlay": {
        "fontSize": 12,
        "color": "#b2b5bc",
        "labels": text_labels,
    },
}

# ── Write ────────────────────────────────────────────────────────────
out_path = os.path.join(os.path.dirname(__file__), "005-stacked-area-chart.json")
with open(out_path, 'w') as f:
    json.dump(doc, f, indent=2)
print(f"\nWrote {out_path} ({os.path.getsize(out_path)} bytes)")

# Also copy to charts/
charts_path = os.path.join(os.path.dirname(__file__), "../../charts/005-stacked-area-chart.json")
shutil.copy2(out_path, charts_path)
print(f"Copied to {charts_path}")

# ── Verify ID uniqueness ─────────────────────────────────────────────
all_ids = set()
for section in ["panes", "layers", "transforms", "buffers", "geometries", "drawItems"]:
    if section in doc:
        for k in doc[section]:
            kid = int(k)
            assert kid not in all_ids, f"DUPLICATE ID {kid} in {section}!"
            all_ids.add(kid)
print(f"\nAll {len(all_ids)} IDs unique: {sorted(all_ids)}")

# Verify vertex counts
for gid, g in doc["geometries"].items():
    bid = str(g["vertexBufferId"])
    buf_data = doc["buffers"][bid]["data"]
    fmt = g["format"]
    vc = g["vertexCount"]
    if fmt == "pos2_clip":
        expected_floats = vc * 2
    elif fmt == "rect4":
        expected_floats = vc * 4
    assert len(buf_data) == expected_floats, \
        f"Geom {gid}: expected {expected_floats} floats, got {len(buf_data)}"
    print(f"Geom {gid} ({fmt}): {vc} verts, {len(buf_data)} floats ✓")
