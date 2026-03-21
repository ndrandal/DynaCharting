#!/usr/bin/env python3
"""Generate a Sector Performance Dashboard with D80 declarative bindings.

Layout:
  Left sidebar (4 colored bars) — click to select a sector
  Main area — shows the selected sector's performance line
  Background grid in main area
  Text overlay labels

Binding logic:
  Click a bar → GPU pick → selection on that bar's DrawItem
  → setVisible binding shows the corresponding line, hides others
"""
import json, math

W, H = 900, 600

# ---- ID allocation (unified namespace) ----
# Buffers:  100-109  (grid, bars, lines)
# Transforms: 50
# Panes: 1
# Layers: 10, 11, 12
# Geometries: 200-209
# DrawItems: 300-309 (bars), 400-403 (lines), 500-509 (grid/decor)
# Bindings: 1001+

sectors = [
    {"name": "Technology", "color": [0.18, 0.80, 0.44, 1.0], "bar_color": [0.18, 0.80, 0.44, 0.9]},
    {"name": "Healthcare", "color": [0.20, 0.60, 0.86, 1.0], "bar_color": [0.20, 0.60, 0.86, 0.9]},
    {"name": "Energy",     "color": [0.90, 0.49, 0.13, 1.0], "bar_color": [0.90, 0.49, 0.13, 0.9]},
    {"name": "Finance",    "color": [0.91, 0.30, 0.24, 1.0], "bar_color": [0.91, 0.30, 0.24, 0.9]},
]

N = len(sectors)

# ---- Generate line data for each sector ----
# X range in clip space: -0.50 to 0.92 (main chart area)
# Y range in clip space: -0.82 to 0.82
NUM_POINTS = 60
x_min, x_max = -0.50, 0.92
y_min, y_max = -0.82, 0.82

def gen_line_data(seed, trend, amp, freq):
    """Generate a line with trend + sine + noise."""
    pts = []
    for i in range(NUM_POINTS):
        t = i / (NUM_POINTS - 1)
        x = x_min + t * (x_max - x_min)
        # Base trend + oscillation
        y_norm = trend * t + amp * math.sin(freq * t * 2 * math.pi + seed)
        # Add some secondary oscillation for realism
        y_norm += 0.08 * math.sin(3.7 * t * 2 * math.pi + seed * 2.1)
        y_norm += 0.05 * math.sin(7.3 * t * 2 * math.pi + seed * 0.7)
        # Map to clip space y range
        y = y_min + (y_norm + 0.3) / 1.0 * (y_max - y_min) * 0.5 + (y_max - y_min) * 0.25
        y = max(y_min, min(y_max, y))
        pts.extend([x, y])
    return pts

line_data = [
    gen_line_data(seed=0.0, trend=0.55, amp=0.15, freq=2.2),   # Tech: strong uptrend
    gen_line_data(seed=1.5, trend=0.25, amp=0.10, freq=1.8),   # Healthcare: steady
    gen_line_data(seed=3.0, trend=-0.10, amp=0.25, freq=3.1),  # Energy: volatile
    gen_line_data(seed=4.5, trend=0.35, amp=0.18, freq=2.7),   # Finance: recovery
]

# ---- Generate grid lines ----
grid_verts = []
# Vertical grid lines
for i in range(13):
    t = i / 12
    x = x_min + t * (x_max - x_min)
    grid_verts.extend([x, y_min, x, y_max])
# Horizontal grid lines
for i in range(9):
    t = i / 8
    y = y_min + t * (y_max - y_min)
    grid_verts.extend([x_min, y, x_max, y])

# ---- Sidebar bars (instancedRect@1, rect4 format: x0, y0, x1, y1) ----
bar_x0, bar_x1 = -0.96, -0.58
bar_height = 0.16
bar_gap = 0.04
# Center 4 bars vertically
total_bar_height = N * bar_height + (N - 1) * bar_gap
bar_top = total_bar_height / 2

bar_rects = []
for i in range(N):
    y1 = bar_top - i * (bar_height + bar_gap)
    y0 = y1 - bar_height
    bar_rects.append([bar_x0, y0, bar_x1, y1])

# ---- Axis frame (border around main chart area) ----
frame_verts = []
# Bottom
frame_verts.extend([x_min, y_min, x_max, y_min])
# Top
frame_verts.extend([x_min, y_max, x_max, y_max])
# Left
frame_verts.extend([x_min, y_min, x_min, y_max])
# Right
frame_verts.extend([x_max, y_min, x_max, y_max])

# ---- Selection indicator bar (thin line under selected bar) ----
# We'll use a small rect that appears when any sector is selected
# This will be a shared "glow" indicator — one per bar
indicator_rects = []
for i in range(N):
    y1 = bar_top - i * (bar_height + bar_gap)
    y0 = y1 - bar_height
    # Slightly larger highlight rect behind the bar
    indicator_rects.append([bar_x0 - 0.02, y0 - 0.015, bar_x1 + 0.02, y1 + 0.015])

# ---- Build the JSON document ----
doc = {
    "version": 1,
    "viewport": {"width": W, "height": H},
    "buffers": {},
    "transforms": {
        "50": {"tx": 0, "ty": 0, "sx": 1, "sy": 1}
    },
    "panes": {
        "1": {"name": "main", "hasClearColor": True, "clearColor": [0.09, 0.09, 0.12, 1.0]}
    },
    "layers": {
        "10": {"paneId": 1, "name": "grid"},
        "11": {"paneId": 1, "name": "data"},
        "12": {"paneId": 1, "name": "bars"},
    },
    "geometries": {},
    "drawItems": {},
    "bindings": {},
    "textOverlay": {
        "fontSize": 14,
        "color": "#8a8d93",
        "labels": []
    }
}

# ---- Grid buffer + geometry + drawItem ----
doc["buffers"]["100"] = {"data": grid_verts}
grid_vc = len(grid_verts) // 2
doc["geometries"]["200"] = {
    "vertexBufferId": 100, "format": "pos2_clip", "vertexCount": grid_vc
}
doc["drawItems"]["500"] = {
    "layerId": 10, "pipeline": "line2d@1", "geometryId": 200,
    "transformId": 50, "color": [0.20, 0.20, 0.25, 0.5], "lineWidth": 1.0
}

# ---- Axis frame ----
doc["buffers"]["101"] = {"data": frame_verts}
frame_vc = len(frame_verts) // 2
doc["geometries"]["201"] = {
    "vertexBufferId": 101, "format": "pos2_clip", "vertexCount": frame_vc
}
doc["drawItems"]["501"] = {
    "layerId": 10, "pipeline": "line2d@1", "geometryId": 201,
    "transformId": 50, "color": [0.35, 0.35, 0.40, 0.8], "lineWidth": 1.5
}

# ---- Sidebar bars (each its own DrawItem for picking) ----
for i in range(N):
    buf_id = 110 + i
    geom_id = 210 + i
    di_id = 300 + i

    doc["buffers"][str(buf_id)] = {"data": bar_rects[i]}
    doc["geometries"][str(geom_id)] = {
        "vertexBufferId": buf_id, "format": "rect4", "vertexCount": 1
    }
    doc["drawItems"][str(di_id)] = {
        "layerId": 12, "pipeline": "instancedRect@1", "geometryId": geom_id,
        "transformId": 50,
        "color": sectors[i]["bar_color"],
        "cornerRadius": 6.0
    }

# ---- Selection indicator rects (initially invisible) ----
for i in range(N):
    buf_id = 120 + i
    geom_id = 220 + i
    di_id = 310 + i

    doc["buffers"][str(buf_id)] = {"data": indicator_rects[i]}
    doc["geometries"][str(geom_id)] = {
        "vertexBufferId": buf_id, "format": "rect4", "vertexCount": 1
    }
    doc["drawItems"][str(di_id)] = {
        "layerId": 11, "pipeline": "instancedRect@1", "geometryId": geom_id,
        "transformId": 50,
        "color": [sectors[i]["color"][0], sectors[i]["color"][1], sectors[i]["color"][2], 0.25],
        "cornerRadius": 8.0,
        "visible": False
    }

# ---- Line charts (one per sector, initially invisible) ----
for i in range(N):
    buf_id = 130 + i
    geom_id = 230 + i
    di_id = 400 + i

    doc["buffers"][str(buf_id)] = {"data": line_data[i]}
    line_vc = len(line_data[i]) // 2
    doc["geometries"][str(geom_id)] = {
        "vertexBufferId": buf_id, "format": "pos2_clip", "vertexCount": line_vc
    }
    doc["drawItems"][str(di_id)] = {
        "layerId": 11, "pipeline": "line2d@1", "geometryId": geom_id,
        "transformId": 50,
        "color": sectors[i]["color"],
        "lineWidth": 2.5,
        "visible": False
    }

# ---- "No selection" placeholder line (a flat dashed line) ----
placeholder_verts = [x_min, 0.0, x_max, 0.0]
doc["buffers"]["140"] = {"data": placeholder_verts}
doc["geometries"]["240"] = {
    "vertexBufferId": 140, "format": "pos2_clip", "vertexCount": 2
}
doc["drawItems"]["410"] = {
    "layerId": 11, "pipeline": "line2d@1", "geometryId": 240,
    "transformId": 50,
    "color": [0.3, 0.3, 0.35, 0.5],
    "lineWidth": 1.0,
    "dashLength": 8.0, "gapLength": 6.0,
    "visible": True
}

# ---- Bindings: click bar N → show line N + indicator N, hide others ----
binding_id = 1001

for i in range(N):
    bar_di = 300 + i
    line_di = 400 + i
    indicator_di = 310 + i

    # Show this sector's line when its bar is selected
    doc["bindings"][str(binding_id)] = {
        "trigger": {"type": "selection", "drawItemId": bar_di},
        "effect": {
            "type": "setVisible",
            "drawItemId": line_di,
            "visible": True,
            "defaultVisible": False
        }
    }
    binding_id += 1

    # Show this sector's selection indicator
    doc["bindings"][str(binding_id)] = {
        "trigger": {"type": "selection", "drawItemId": bar_di},
        "effect": {
            "type": "setVisible",
            "drawItemId": indicator_di,
            "visible": True,
            "defaultVisible": False
        }
    }
    binding_id += 1

    # Hide the placeholder when this sector is selected
    doc["bindings"][str(binding_id)] = {
        "trigger": {"type": "selection", "drawItemId": bar_di},
        "effect": {
            "type": "setVisible",
            "drawItemId": 410,  # placeholder line
            "visible": False,
            "defaultVisible": True
        }
    }
    binding_id += 1

# ---- Text overlay labels ----
labels = doc["textOverlay"]["labels"]

# Title
labels.append({"clipX": -0.78, "clipY": 0.94, "text": "SECTOR PERFORMANCE", "align": "c",
               "color": "#e0e0e0", "fontSize": 18})

# Sidebar labels (centered on each bar)
for i in range(N):
    y_center = bar_top - i * (bar_height + bar_gap) - bar_height / 2
    labels.append({
        "clipX": -0.77, "clipY": y_center,
        "text": sectors[i]["name"],
        "align": "c", "color": "#ffffff", "fontSize": 13
    })

# Chart area title
labels.append({"clipX": 0.21, "clipY": 0.92, "text": "Click a sector to view performance",
               "align": "c", "color": "#666670", "fontSize": 13})

# X-axis labels (months)
months = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]
for i, month in enumerate(months):
    t = i / 11
    x = x_min + t * (x_max - x_min)
    labels.append({"clipX": x, "clipY": y_min - 0.06, "text": month, "align": "c",
                   "color": "#555560", "fontSize": 11})

# Y-axis labels
for i in range(5):
    t = i / 4
    y = y_min + t * (y_max - y_min)
    val = int(-20 + t * 60)
    labels.append({"clipX": x_min - 0.04, "clipY": y + 0.01,
                   "text": f"{val:+d}%", "align": "r", "color": "#555560", "fontSize": 11})

# Write JSON
output_path = "charts/078-binding-dashboard.json"
with open(output_path, "w") as f:
    json.dump(doc, f, indent=2)

print(f"Generated {output_path}")
print(f"  {N} sectors, {NUM_POINTS} points each")
print(f"  {len(doc['buffers'])} buffers, {len(doc['geometries'])} geometries")
print(f"  {len(doc['drawItems'])} drawItems, {len(doc['bindings'])} bindings")
print(f"  {len(labels)} text labels")
