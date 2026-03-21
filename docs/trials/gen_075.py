#!/usr/bin/env python3
"""Generate yin-yang chart JSON for Trial 075.

Approach: layered additive/subtractive painting.
  Layer 10: Full dark circle (R=40)
  Layer 11: Right semicircle filled white (R=40) — overpaints right half
  Layer 12: Upper inner semicircle filled dark (R=20, center (0,20)) — carves dark into upper-right
  Layer 13: Lower inner semicircle filled white (R=20, center (0,-20)) — extends white into lower-left
  Layer 14: Small dots (white at (0,20), dark at (0,-20))
  Layer 15: Outer border circle (lineAA@1)
"""
import json
import math

N = 64  # segments for full circles
S = 32  # segments for semicircles

# === Helper: fan tessellation of a circular sector ===
def circle_fan(cx, cy, r, theta_start, theta_end, segs):
    """Generate triangle-fan vertices for a circular sector."""
    verts = []
    for i in range(segs):
        a0 = theta_start + (theta_end - theta_start) * i / segs
        a1 = theta_start + (theta_end - theta_start) * (i + 1) / segs
        verts.extend([cx, cy])
        verts.extend([cx + r * math.cos(a0), cy + r * math.sin(a0)])
        verts.extend([cx + r * math.cos(a1), cy + r * math.sin(a1)])
    return verts

# === 1. Full dark circle: center (0,0), R=40, full 2pi ===
dark_circle = circle_fan(0, 0, 40, 0, 2 * math.pi, N)
dark_circle_vc = N * 3  # 192

# === 2. Right semicircle white: center (0,0), R=40, theta from -pi/2 to pi/2 ===
right_semi = circle_fan(0, 0, 40, -math.pi / 2, math.pi / 2, S)
right_semi_vc = S * 3  # 96

# === 3. Upper inner semicircle dark: center (0,20), R=20 ===
# This semicircle covers the RIGHT side of center (0,20), from top to bottom
# theta from -pi/2 to pi/2, which gives the right semicircle
# But we need the LEFT semicircle (curving into white area on the right)
# Actually: the upper dark bulge extends from (0,40) to (0,0) curving RIGHT
# Wait - in the yin-yang, the dark half's upper portion bulges INTO the right (white) side.
# The dark "fish head" at top is a semicircle centered at (0,20), R=20, extending to the RIGHT.
# So it's the RIGHT semicircle: theta from -pi/2 to pi/2.
# x = 0 + 20*cos(theta), which is positive (rightward) for theta in [-pi/2, pi/2].
upper_dark = circle_fan(0, 20, 20, -math.pi / 2, math.pi / 2, S)
upper_dark_vc = S * 3  # 96

# === 4. Lower inner semicircle white: center (0,-20), R=20 ===
# The white half's lower portion bulges INTO the left (dark) side.
# So it's the LEFT semicircle: theta from pi/2 to 3pi/2.
# x = 0 + 20*cos(theta), which is negative (leftward) for theta in [pi/2, 3pi/2].
lower_white = circle_fan(0, -20, 20, math.pi / 2, 3 * math.pi / 2, S)
lower_white_vc = S * 3  # 96

# === 5. Small white dot at (0, 20), R=5 ===
DOT_SEGS = 32
white_dot = circle_fan(0, 20, 5, 0, 2 * math.pi, DOT_SEGS)
white_dot_vc = DOT_SEGS * 3  # 96

# === 6. Small dark dot at (0, -20), R=5 ===
dark_dot = circle_fan(0, -20, 5, 0, 2 * math.pi, DOT_SEGS)
dark_dot_vc = DOT_SEGS * 3  # 96

# === 7. Outer border circle: lineAA@1 (rect4 format) ===
BORDER_SEGS = 64
border_circle = []
for i in range(BORDER_SEGS):
    a0 = 2 * math.pi * i / BORDER_SEGS
    a1 = 2 * math.pi * (i + 1) / BORDER_SEGS
    border_circle.extend([
        40 * math.cos(a0), 40 * math.sin(a0),
        40 * math.cos(a1), 40 * math.sin(a1)
    ])
border_vc = BORDER_SEGS  # 64

# === Round all floats ===
def rf(arr):
    return [round(x, 9) for x in arr]

dark_circle = rf(dark_circle)
right_semi = rf(right_semi)
upper_dark = rf(upper_dark)
lower_white = rf(lower_white)
white_dot = rf(white_dot)
dark_dot = rf(dark_dot)
border_circle = rf(border_circle)

# Verify buffer sizes
assert len(dark_circle) == dark_circle_vc * 2
assert len(right_semi) == right_semi_vc * 2
assert len(upper_dark) == upper_dark_vc * 2
assert len(lower_white) == lower_white_vc * 2
assert len(white_dot) == white_dot_vc * 2
assert len(dark_dot) == dark_dot_vc * 2
assert len(border_circle) == border_vc * 4

print(f"Dark circle: {dark_circle_vc} vertices, {len(dark_circle)} floats")
print(f"Right semicircle: {right_semi_vc} vertices, {len(right_semi)} floats")
print(f"Upper dark semi: {upper_dark_vc} vertices, {len(upper_dark)} floats")
print(f"Lower white semi: {lower_white_vc} vertices, {len(lower_white)} floats")
print(f"White dot: {white_dot_vc} vertices, {len(white_dot)} floats")
print(f"Dark dot: {dark_dot_vc} vertices, {len(dark_dot)} floats")
print(f"Border circle: {border_vc} vertices, {len(border_circle)} floats")

# === Colors (RGBA normalized) ===
white_color = [round(248/255, 6), round(250/255, 6), round(252/255, 6), 1.0]
dark_color = [round(30/255, 6), round(41/255, 6), round(59/255, 6), 1.0]
border_color = [round(148/255, 6), round(163/255, 6), round(184/255, 6), 0.8]
bg_color = [round(15/255, 6), round(23/255, 6), round(42/255, 6), 1.0]

# === ID Plan ===
# Pane: 1
# Layers: 10, 11, 12, 13, 14, 15
# Transform: 50
# Buffers: 100, 101, 102, 103, 104, 105, 106
# Geometries: 200, 201, 202, 203, 204, 205, 206
# DrawItems: 300, 301, 302, 303, 304, 305, 306

all_ids = [1, 10, 11, 12, 13, 14, 15, 50,
           100, 101, 102, 103, 104, 105, 106,
           200, 201, 202, 203, 204, 205, 206,
           300, 301, 302, 303, 304, 305, 306]
assert len(all_ids) == len(set(all_ids)), "ID collision!"
print(f"\nAll {len(all_ids)} IDs unique: {sorted(all_ids)}")

doc = {
    "version": 1,
    "viewport": {"width": 600, "height": 600},
    "buffers": {
        "100": {"data": dark_circle},
        "101": {"data": right_semi},
        "102": {"data": upper_dark},
        "103": {"data": lower_white},
        "104": {"data": white_dot},
        "105": {"data": dark_dot},
        "106": {"data": border_circle}
    },
    "transforms": {
        "50": {"sx": 0.019, "sy": 0.019, "tx": 0.0, "ty": 0.0}
    },
    "panes": {
        "1": {
            "name": "main",
            "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
            "hasClearColor": True,
            "clearColor": bg_color
        }
    },
    "layers": {
        "10": {"paneId": 1, "name": "darkBase"},
        "11": {"paneId": 1, "name": "whiteHalf"},
        "12": {"paneId": 1, "name": "upperDark"},
        "13": {"paneId": 1, "name": "lowerWhite"},
        "14": {"paneId": 1, "name": "dots"},
        "15": {"paneId": 1, "name": "border"}
    },
    "geometries": {
        "200": {"vertexBufferId": 100, "format": "pos2_clip", "vertexCount": dark_circle_vc},
        "201": {"vertexBufferId": 101, "format": "pos2_clip", "vertexCount": right_semi_vc},
        "202": {"vertexBufferId": 102, "format": "pos2_clip", "vertexCount": upper_dark_vc},
        "203": {"vertexBufferId": 103, "format": "pos2_clip", "vertexCount": lower_white_vc},
        "204": {"vertexBufferId": 104, "format": "pos2_clip", "vertexCount": white_dot_vc},
        "205": {"vertexBufferId": 105, "format": "pos2_clip", "vertexCount": dark_dot_vc},
        "206": {"vertexBufferId": 106, "format": "rect4", "vertexCount": border_vc}
    },
    "drawItems": {
        "300": {
            "layerId": 10,
            "pipeline": "triSolid@1",
            "geometryId": 200,
            "transformId": 50,
            "color": dark_color
        },
        "301": {
            "layerId": 11,
            "pipeline": "triSolid@1",
            "geometryId": 201,
            "transformId": 50,
            "color": white_color
        },
        "302": {
            "layerId": 12,
            "pipeline": "triSolid@1",
            "geometryId": 202,
            "transformId": 50,
            "color": dark_color
        },
        "303": {
            "layerId": 13,
            "pipeline": "triSolid@1",
            "geometryId": 203,
            "transformId": 50,
            "color": white_color
        },
        "304": {
            "layerId": 14,
            "pipeline": "triSolid@1",
            "geometryId": 204,
            "transformId": 50,
            "color": white_color
        },
        "305": {
            "layerId": 14,
            "pipeline": "triSolid@1",
            "geometryId": 205,
            "transformId": 50,
            "color": dark_color
        },
        "306": {
            "layerId": 15,
            "pipeline": "lineAA@1",
            "geometryId": 206,
            "transformId": 50,
            "color": border_color,
            "lineWidth": 2.0
        }
    },
    "textOverlay": {
        "fontSize": 14,
        "color": "#b2b5bc",
        "labels": [
            {"clipX": 0.0, "clipY": 0.9, "text": "Yin-Yang (Taijitu)", "align": "c"}
        ]
    }
}

json_str = json.dumps(doc, separators=(',', ':'))

# Write to both locations
import os
os.makedirs("/home/ndrandal/Github/DynaCharting/docs/trials", exist_ok=True)
with open("/home/ndrandal/Github/DynaCharting/docs/trials/075-yin-yang.json", "w") as f:
    f.write(json_str)
with open("/home/ndrandal/Github/DynaCharting/charts/075-yin-yang.json", "w") as f:
    f.write(json_str)

print(f"\nJSON written. Size: {len(json_str)} bytes")
print(f"Total unique IDs: {len(all_ids)}")
