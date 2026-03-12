#!/usr/bin/env python3
"""Generate Trial 055: Target Bullseye JSON chart file."""
import json
import math

# ─── Constants ───
SEGMENTS = 48
TWO_PI = 2.0 * math.pi

# ─── Helper: filled circle as triSolid@1 (pos2_clip, 2 floats per vertex) ───
def filled_circle(cx, cy, r, segments=SEGMENTS):
    """Center-fan tessellation. segments triangles, each 3 vertices = segments*3 vertices."""
    verts = []
    for i in range(segments):
        a0 = TWO_PI * i / segments
        a1 = TWO_PI * (i + 1) / segments
        # center, rim_i, rim_{i+1}
        verts.extend([cx, cy])
        verts.extend([cx + r * math.cos(a0), cy + r * math.sin(a0)])
        verts.extend([cx + r * math.cos(a1), cy + r * math.sin(a1)])
    return verts

# ─── Helper: circle outline as lineAA@1 (rect4, 4 floats per segment) ───
def circle_outline(cx, cy, r, segments=SEGMENTS):
    """segments line segments around the circle."""
    segs = []
    for i in range(segments):
        a0 = TWO_PI * i / segments
        a1 = TWO_PI * (i + 1) / segments
        x0 = cx + r * math.cos(a0)
        y0 = cy + r * math.sin(a0)
        x1 = cx + r * math.cos(a1)
        y1 = cy + r * math.sin(a1)
        segs.extend([x0, y0, x1, y1])
    return segs

# ─── Helper: small AA circle for shot (triAA@1, pos2_alpha, 3 floats per vertex) ───
def shot_circle(cx, cy, r, segments=16):
    """Center-fan with alpha: center has alpha=1, rim has alpha=0.
    Each triangle: 3 vertices with (x, y, alpha).
    segments * 3 vertices = segments * 3 * 3 floats."""
    verts = []
    for i in range(segments):
        a0 = TWO_PI * i / segments
        a1 = TWO_PI * (i + 1) / segments
        # center vertex (alpha=1)
        verts.extend([cx, cy, 1.0])
        # rim vertex i (alpha=0)
        verts.extend([cx + r * math.cos(a0), cy + r * math.sin(a0), 0.0])
        # rim vertex i+1 (alpha=0)
        verts.extend([cx + r * math.cos(a1), cy + r * math.sin(a1), 0.0])
    return verts

# ─── Ring data (back to front) ───
rings = [
    {"radius": 45, "color_hex": "#1e40af", "layer": 10, "buf": 100, "geo": 200, "di": 300},
    {"radius": 36, "color_hex": "#dc2626", "layer": 11, "buf": 101, "geo": 201, "di": 301},
    {"radius": 27, "color_hex": "#2563eb", "layer": 12, "buf": 102, "geo": 202, "di": 302},
    {"radius": 18, "color_hex": "#dc2626", "layer": 13, "buf": 103, "geo": 203, "di": 303},
    {"radius":  9, "color_hex": "#fbbf24", "layer": 14, "buf": 104, "geo": 204, "di": 304},
]

def hex_to_rgba(h, a=1.0):
    """Convert hex color string to [r, g, b, a] in 0-1 range."""
    h = h.lstrip('#')
    r = int(h[0:2], 16) / 255.0
    g = int(h[2:4], 16) / 255.0
    b = int(h[4:6], 16) / 255.0
    return [round(r, 6), round(g, 6), round(b, 6), a]

# ─── Build the document ───
doc = {
    "version": 1,
    "viewport": {"width": 700, "height": 700},
    "buffers": {},
    "transforms": {
        "50": {"sx": 0.019, "sy": 0.019, "tx": 0.0, "ty": 0.0}
    },
    "panes": {
        "1": {
            "name": "Target",
            "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
            "hasClearColor": True,
            "clearColor": [0.058824, 0.090196, 0.164706, 1.0]  # #0f172a
        }
    },
    "layers": {},
    "geometries": {},
    "drawItems": {}
}

# ─── Layers ───
for ring in rings:
    doc["layers"][str(ring["layer"])] = {"paneId": 1, "name": f"Ring{ring['radius']}"}
doc["layers"]["15"] = {"paneId": 1, "name": "Separators"}
doc["layers"]["16"] = {"paneId": 1, "name": "Shots"}

# ─── 5 Ring fills ───
for ring in rings:
    data = filled_circle(0, 0, ring["radius"], SEGMENTS)
    vc = SEGMENTS * 3  # 144 vertices
    assert len(data) == vc * 2, f"Ring {ring['radius']}: expected {vc*2} floats, got {len(data)}"

    buf_id = str(ring["buf"])
    geo_id = str(ring["geo"])
    di_id = str(ring["di"])

    doc["buffers"][buf_id] = {"data": [round(v, 9) for v in data]}
    doc["geometries"][geo_id] = {
        "vertexBufferId": ring["buf"],
        "format": "pos2_clip",
        "vertexCount": vc
    }
    rgba = hex_to_rgba(ring["color_hex"])
    doc["drawItems"][di_id] = {
        "layerId": ring["layer"],
        "name": f"Ring{ring['radius']}",
        "pipeline": "triSolid@1",
        "geometryId": ring["geo"],
        "transformId": 50,
        "color": rgba
    }

# ─── Ring separator lines (4 circles at radii 9, 18, 27, 36) ───
sep_radii = [9, 18, 27, 36]
sep_data = []
for r in sep_radii:
    sep_data.extend(circle_outline(0, 0, r, SEGMENTS))
sep_vc = len(sep_radii) * SEGMENTS  # 4 * 48 = 192 instances
assert len(sep_data) == sep_vc * 4, f"Separators: expected {sep_vc*4} floats, got {len(sep_data)}"

doc["buffers"]["105"] = {"data": [round(v, 9) for v in sep_data]}
doc["geometries"]["205"] = {
    "vertexBufferId": 105,
    "format": "rect4",
    "vertexCount": sep_vc
}
doc["drawItems"]["305"] = {
    "layerId": 15,
    "name": "RingSeparators",
    "pipeline": "lineAA@1",
    "geometryId": 205,
    "transformId": 50,
    "color": [1.0, 1.0, 1.0, 0.3],
    "lineWidth": 1.0
}

# ─── Crosshair lines (2 instances: horizontal + vertical) ───
crosshair_data = [
    -45.0, 0.0, 45.0, 0.0,   # horizontal
    0.0, -45.0, 0.0, 45.0    # vertical
]
doc["buffers"]["106"] = {"data": crosshair_data}
doc["geometries"]["206"] = {
    "vertexBufferId": 106,
    "format": "rect4",
    "vertexCount": 2
}
doc["drawItems"]["306"] = {
    "layerId": 15,
    "name": "Crosshairs",
    "pipeline": "lineAA@1",
    "geometryId": 206,
    "transformId": 50,
    "color": [1.0, 1.0, 1.0, 0.15],
    "lineWidth": 1.0
}

# ─── 12 shot positions (triAA@1, pos2_alpha) ───
shots = [
    (2, 3), (-1, -2), (5, 8), (-10, 4),
    (15, -12), (-8, -18), (22, 10),
    (-20, -22), (28, 15),
    (-30, 25), (35, -20), (-5, 42)
]
SHOT_RADIUS = 2.0
SHOT_SEGMENTS = 16

all_shot_data = []
for (sx, sy) in shots:
    all_shot_data.extend(shot_circle(sx, sy, SHOT_RADIUS, SHOT_SEGMENTS))

# 12 shots * 16 segments * 3 vertices = 576 vertices, 3 floats each = 1728 floats
shot_vc = 12 * SHOT_SEGMENTS * 3  # 576 vertices
assert len(all_shot_data) == shot_vc * 3, f"Shots: expected {shot_vc*3} floats, got {len(all_shot_data)}"

doc["buffers"]["107"] = {"data": [round(v, 9) for v in all_shot_data]}
doc["geometries"]["207"] = {
    "vertexBufferId": 107,
    "format": "pos2_alpha",
    "vertexCount": shot_vc
}
doc["drawItems"]["307"] = {
    "layerId": 16,
    "name": "Shots",
    "pipeline": "triAA@1",
    "geometryId": 207,
    "transformId": 50,
    "color": [1.0, 1.0, 1.0, 1.0]
}

# ─── Text overlay ───
doc["textOverlay"] = {
    "fontSize": 14,
    "color": "#b2b5bc",
    "labels": [
        {"clipX": 0.0, "clipY": 0.95, "text": "Archery Target - 12 Shots", "align": "c", "fontSize": 16},
        {"clipX": 0.0, "clipY": -0.95, "text": "Bullseye: 2 | Ring 2: 2 | Ring 3: 3 | Ring 4: 2 | Ring 5: 3", "align": "c", "fontSize": 11}
    ]
}

# ─── Verify all IDs are unique ───
all_ids = set()
all_ids.add(1)  # pane
all_ids.add(50)  # transform
for lid in doc["layers"]:
    assert int(lid) not in all_ids, f"Duplicate layer ID: {lid}"
    all_ids.add(int(lid))
for bid in doc["buffers"]:
    assert int(bid) not in all_ids, f"Duplicate buffer ID: {bid}"
    all_ids.add(int(bid))
for gid in doc["geometries"]:
    assert int(gid) not in all_ids, f"Duplicate geometry ID: {gid}"
    all_ids.add(int(gid))
for did in doc["drawItems"]:
    assert int(did) not in all_ids, f"Duplicate drawItem ID: {did}"
    all_ids.add(int(did))

print(f"Total unique IDs: {len(all_ids)}")
print(f"IDs: {sorted(all_ids)}")

# ─── Write JSON ───
output_path = "/home/ndrandal/Github/DynaCharting/docs/trials/055-target-bullseye.json"
with open(output_path, 'w') as f:
    json.dump(doc, f, indent=2)
print(f"Written to {output_path}")

# Also copy to charts/
import shutil
charts_path = "/home/ndrandal/Github/DynaCharting/charts/055-target-bullseye.json"
shutil.copy2(output_path, charts_path)
print(f"Copied to {charts_path}")
