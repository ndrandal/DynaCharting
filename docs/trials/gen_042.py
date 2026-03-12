#!/usr/bin/env python3
"""Generate 042-hexbin-density.json"""

import json
import math

# === HEX GRID PARAMETERS ===
R = 4.0          # hex radius (center to vertex)
W = 2 * R * math.cos(math.pi / 6)  # flat-to-flat width = 6.928
col_spacing = W  # 6.928
row_spacing = 1.5 * R  # 6.0

# Data space: X=[0,100], Y=[0,80]
# Grid covers X=[2,98], Y=[2,78]

# Generate hex centers
hex_centers = []
row = 0
y = 2.0
while y <= 78.0:
    offset = W / 2 if (row % 2 == 1) else 0.0
    x = 2.0 + offset
    while x <= 98.0:
        hex_centers.append((x, y))
        x += col_spacing
    y += row_spacing
    row += 1

print(f"Total hex centers: {len(hex_centers)}")

# === DENSITY COMPUTATION ===
# Cluster 1: (30, 40), sigma=15
# Cluster 2: (70, 45), sigma=12
def density(hx, hy):
    d1 = 50.0 * math.exp(-((hx - 30)**2 + (hy - 40)**2) / (2 * 15**2))
    d2 = 40.0 * math.exp(-((hx - 70)**2 + (hy - 45)**2) / (2 * 12**2))
    return d1 + d2

# Classify hexagons into tiers
tiers = {1: [], 2: [], 3: [], 4: [], 5: []}
for (hx, hy) in hex_centers:
    d = density(hx, hy)
    if d < 0.5:
        continue  # skip empty
    elif d < 5:
        tiers[1].append((hx, hy, d))
    elif d < 15:
        tiers[2].append((hx, hy, d))
    elif d < 25:
        tiers[3].append((hx, hy, d))
    elif d < 35:
        tiers[4].append((hx, hy, d))
    else:
        tiers[5].append((hx, hy, d))

total_hexes = sum(len(v) for v in tiers.values())
for t in range(1, 6):
    print(f"Tier {t}: {len(tiers[t])} hexagons")
print(f"Total hexagons rendered: {total_hexes}")
print(f"Hexagons skipped (density < 0.5): {len(hex_centers) - total_hexes}")

# === HEXAGON TESSELLATION ===
# Pointy-top orientation: vertex i at angle = 60*i + 30 degrees
# Each hex = 6 triangles: (center, vertex_i, vertex_{(i+1)%6})
# 18 vertices per hex (6 tris * 3 verts), pos2_clip format (x, y) = 8 bytes each

def hex_vertices(cx, cy, radius):
    """Return list of 18 floats (9 x,y pairs = 6 triangles) for one hexagon."""
    verts = []
    rim = []
    for i in range(6):
        angle = math.radians(60 * i + 30)
        rim.append((cx + radius * math.cos(angle), cy + radius * math.sin(angle)))
    for i in range(6):
        # Triangle: center, rim[i], rim[(i+1)%6]
        verts.extend([cx, cy])
        verts.extend([rim[i][0], rim[i][1]])
        verts.extend([rim[(i + 1) % 6][0], rim[(i + 1) % 6][1]])
    return verts

# Build buffer data for each tier
tier_data = {}
for t in range(1, 6):
    data = []
    for (hx, hy, d) in tiers[t]:
        data.extend(hex_vertices(hx, hy, R))
    tier_data[t] = data

# Verify vertex counts
for t in range(1, 6):
    n_hexes = len(tiers[t])
    n_verts = n_hexes * 18  # 6 tris * 3 verts
    n_floats = n_verts * 2  # pos2_clip: x, y
    assert len(tier_data[t]) == n_floats, f"Tier {t}: expected {n_floats} floats, got {len(tier_data[t])}"
    print(f"Tier {t}: {n_hexes} hexes, {n_verts} vertices, {n_floats} floats, {n_floats * 4} bytes")

# === BORDER (lineAA@1, rect4) ===
# Rectangle border around data area [0,100] x [0,80]
# 4 line segments: bottom, right, top, left
border_data = [
    0.0, 0.0, 100.0, 0.0,    # bottom
    100.0, 0.0, 100.0, 80.0,  # right
    100.0, 80.0, 0.0, 80.0,   # top
    0.0, 80.0, 0.0, 0.0,      # left
]

# === COLORS ===
tier_colors = {
    1: [0x1e/255, 0x3a/255, 0x5f/255, 0.8],   # #1e3a5f
    2: [0x1d/255, 0x6a/255, 0x96/255, 0.9],   # #1d6a96
    3: [0x21/255, 0x96/255, 0xc3/255, 0.95],  # #2196c3
    4: [0x4f/255, 0xc3/255, 0xf7/255, 1.0],   # #4fc3f7
    5: [0xb3/255, 0xe5/255, 0xfc/255, 1.0],   # #b3e5fc
}

# === TRANSFORM ===
# Data space: X=[0,100], Y=[0,80]
# Map to clip [-0.95, 0.95]
# sx = 1.9/100 = 0.019, sy = 1.9/80 = 0.02375
# tx = -0.95, ty = -0.95
sx = 0.019
sy = 0.02375
tx = -0.95
ty = -0.95

# === BUILD JSON ===
doc = {
    "version": 1,
    "viewport": {"width": 900, "height": 700},
    "buffers": {},
    "transforms": {
        "50": {"sx": sx, "sy": sy, "tx": tx, "ty": ty}
    },
    "panes": {
        "1": {
            "name": "HexbinDensity",
            "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True,
            "clearColor": [0x0f/255, 0x17/255, 0x2a/255, 1.0]
        }
    },
    "layers": {
        "10": {"paneId": 1, "name": "hexdata"},
        "11": {"paneId": 1, "name": "border"}
    },
    "geometries": {},
    "drawItems": {}
}

# Tier DrawItems
tier_ids = {
    1: {"buf": 100, "geo": 101, "di": 102},
    2: {"buf": 103, "geo": 104, "di": 105},
    3: {"buf": 106, "geo": 107, "di": 108},
    4: {"buf": 109, "geo": 110, "di": 111},
    5: {"buf": 112, "geo": 113, "di": 114},
}

for t in range(1, 6):
    ids = tier_ids[t]
    n_hexes = len(tiers[t])
    n_verts = n_hexes * 18

    # Round floats to 6 decimal places
    rounded_data = [round(f, 6) for f in tier_data[t]]

    doc["buffers"][str(ids["buf"])] = {"data": rounded_data}
    doc["geometries"][str(ids["geo"])] = {
        "vertexBufferId": ids["buf"],
        "format": "pos2_clip",
        "vertexCount": n_verts
    }
    doc["drawItems"][str(ids["di"])] = {
        "layerId": 10,
        "pipeline": "triSolid@1",
        "geometryId": ids["geo"],
        "transformId": 50,
        "color": tier_colors[t]
    }

# Border
doc["buffers"]["115"] = {"data": border_data}
doc["geometries"]["116"] = {
    "vertexBufferId": 115,
    "format": "rect4",
    "vertexCount": 4
}
doc["drawItems"]["117"] = {
    "layerId": 11,
    "pipeline": "lineAA@1",
    "geometryId": 116,
    "transformId": 50,
    "color": [1.0, 1.0, 1.0, 0.2],
    "lineWidth": 1.0
}

# Text overlay
doc["textOverlay"] = {
    "fontSize": 16,
    "color": "#b2b5bc",
    "labels": [
        {"clipX": 0.0, "clipY": 0.92, "text": "Point Density \u2014 Hexbin Plot", "align": "c", "fontSize": 18},
        {"clipX": -0.88, "clipY": -0.88, "text": "\u25a0 <0.5: hidden", "align": "l", "fontSize": 11, "color": "#555555"},
        {"clipX": -0.88, "clipY": -0.83, "text": "\u25a0 0.5\u20135", "align": "l", "fontSize": 11, "color": "#1e3a5f"},
        {"clipX": -0.88, "clipY": -0.78, "text": "\u25a0 5\u201315", "align": "l", "fontSize": 11, "color": "#1d6a96"},
        {"clipX": -0.58, "clipY": -0.83, "text": "\u25a0 15\u201325", "align": "l", "fontSize": 11, "color": "#2196c3"},
        {"clipX": -0.58, "clipY": -0.78, "text": "\u25a0 25\u201335", "align": "l", "fontSize": 11, "color": "#4fc3f7"},
        {"clipX": -0.28, "clipY": -0.83, "text": "\u25a0 35+", "align": "l", "fontSize": 11, "color": "#b3e5fc"},
    ]
}

# Verify all IDs are unique
all_ids = set()
for section in ["buffers", "transforms", "panes", "layers", "geometries", "drawItems"]:
    for k in doc.get(section, {}):
        assert int(k) not in all_ids, f"Duplicate ID: {k} in {section}"
        all_ids.add(int(k))

print(f"\nAll {len(all_ids)} IDs unique: {sorted(all_ids)}")

# Write JSON
out_path = "/home/ndrandal/Github/DynaCharting/docs/trials/042-hexbin-density.json"
with open(out_path, 'w') as f:
    json.dump(doc, f, separators=(',', ':'))
print(f"\nWrote {out_path}")

# Also copy to charts/
import shutil
charts_path = "/home/ndrandal/Github/DynaCharting/charts/042-hexbin-density.json"
shutil.copy(out_path, charts_path)
print(f"Copied to {charts_path}")
