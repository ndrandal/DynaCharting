#!/usr/bin/env python3
"""Generate Voronoi diagram chart for Trial 059.

Approach: 25x25 grid of 4x4 unit cells, each colored by nearest seed.
Data space: [0,100] x [0,100], 800x800 square viewport.
12 seed colors → 12 instancedRect@1 DrawItems.
12 seed dots → 1 triAA@1 DrawItem (white dots, 16 segments per circle).
"""

import json
import math
import struct

# ── Seeds ──────────────────────────────────────────────────────────────
seeds = [
    (15, 80),   # 1 blue
    (40, 90),   # 2 emerald
    (75, 85),   # 3 amber
    (10, 50),   # 4 violet
    (35, 55),   # 5 pink
    (65, 60),   # 6 red
    (90, 45),   # 7 cyan
    (20, 20),   # 8 lime
    (50, 25),   # 9 orange
    (80, 15),   # 10 purple
    (45, 45),   # 11 teal
    (60, 30),   # 12 rose
]

colors_rgba = [
    (0.231, 0.510, 0.965, 0.85),  # 1 #3b82f6 blue
    (0.063, 0.725, 0.506, 0.85),  # 2 #10b981 emerald
    (0.961, 0.620, 0.043, 0.85),  # 3 #f59e0b amber
    (0.545, 0.361, 0.965, 0.85),  # 4 #8b5cf6 violet
    (0.925, 0.282, 0.600, 0.85),  # 5 #ec4899 pink
    (0.937, 0.267, 0.267, 0.85),  # 6 #ef4444 red
    (0.024, 0.714, 0.831, 0.85),  # 7 #06b6d4 cyan
    (0.518, 0.800, 0.086, 0.85),  # 8 #84cc16 lime
    (0.976, 0.451, 0.086, 0.85),  # 9 #f97316 orange
    (0.659, 0.333, 0.969, 0.85),  # 10 #a855f7 purple
    (0.078, 0.722, 0.651, 0.85),  # 11 #14b8a6 teal
    (0.882, 0.114, 0.282, 0.85),  # 12 #e11d48 rose
]

# ── Grid assignment ────────────────────────────────────────────────────
GRID = 25
CELL_SIZE = 4.0  # 100 / 25

# Group grid cells by nearest seed
cell_groups = [[] for _ in range(12)]  # one list per seed

for j in range(GRID):
    for i in range(GRID):
        cx = i * CELL_SIZE + CELL_SIZE / 2.0
        cy = j * CELL_SIZE + CELL_SIZE / 2.0

        best_dist = float('inf')
        best_seed = 0
        for s_idx, (sx, sy) in enumerate(seeds):
            d = (cx - sx)**2 + (cy - sy)**2
            if d < best_dist:
                best_dist = d
                best_seed = s_idx

        x_min = i * CELL_SIZE
        y_min = j * CELL_SIZE
        x_max = x_min + CELL_SIZE
        y_max = y_min + CELL_SIZE
        cell_groups[best_seed].append((x_min, y_min, x_max, y_max))

# Print cell counts
for idx, group in enumerate(cell_groups):
    print(f"Seed {idx+1} ({seeds[idx][0]},{seeds[idx][1]}): {len(group)} cells")

# ── Seed dot tessellation (triAA@1, pos2_alpha) ───────────────────────
# 16 segments per circle, 3 verts per triangle (center-fan)
# radius = 2.0 data units
SEGMENTS = 16
DOT_RADIUS = 2.0

dot_verts = []
for sx, sy in seeds:
    for seg in range(SEGMENTS):
        a0 = 2.0 * math.pi * seg / SEGMENTS
        a1 = 2.0 * math.pi * (seg + 1) / SEGMENTS
        # Center vertex (alpha=1.0 for interior)
        dot_verts.extend([sx, sy, 1.0])
        # Rim vertices (alpha=0.0 for AA fringe)
        dot_verts.extend([sx + DOT_RADIUS * math.cos(a0), sy + DOT_RADIUS * math.sin(a0), 0.0])
        dot_verts.extend([sx + DOT_RADIUS * math.cos(a1), sy + DOT_RADIUS * math.sin(a1), 0.0])

dot_vertex_count = 12 * SEGMENTS * 3  # 12 seeds × 16 segments × 3 verts = 576
assert len(dot_verts) == dot_vertex_count * 3  # 3 floats per vertex (pos2_alpha)

# ── Build JSON ─────────────────────────────────────────────────────────
# ID plan:
# Pane: 1
# Layers: 10 (cell fills), 11 (seed dots)
# Transform: 50
# Buffers: 100-111 (12 cell fills), 120 (seed dots)
# Geometries: 200-211 (12 cell fills), 220 (seed dots)
# DrawItems: 300-311 (12 cell fills), 320 (seed dots)

# Verify no ID collisions
all_ids = [1, 10, 11, 50, 120, 220, 320]
for i in range(12):
    all_ids.extend([100 + i, 200 + i, 300 + i])
assert len(all_ids) == len(set(all_ids)), f"ID collision! {len(all_ids)} vs {len(set(all_ids))}"

# Transform: map [0,100] x [0,100] → clip [-0.95, 0.95]
# sx = 1.9 / 100 = 0.019
# tx = -0.95 - 0 * 0.019 = -0.95
sx_val = 1.9 / 100.0
sy_val = 1.9 / 100.0
tx_val = -0.95
ty_val = -0.95

doc = {
    "version": 1,
    "viewport": {"width": 800, "height": 800},
    "buffers": {},
    "transforms": {
        "50": {"sx": sx_val, "sy": sy_val, "tx": tx_val, "ty": ty_val}
    },
    "panes": {
        "1": {
            "name": "Voronoi",
            "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
            "hasClearColor": True,
            "clearColor": [0.06, 0.06, 0.09, 1.0]
        }
    },
    "layers": {
        "10": {"paneId": 1, "name": "CellFills"},
        "11": {"paneId": 1, "name": "SeedDots"}
    },
    "geometries": {},
    "drawItems": {}
}

# Add cell fill buffers, geometries, drawItems
for i in range(12):
    buf_id = str(100 + i)
    geo_id = str(200 + i)
    di_id = str(300 + i)

    # rect4 data: [xMin, yMin, xMax, yMax] per cell
    data = []
    for (x0, y0, x1, y1) in cell_groups[i]:
        data.extend([x0, y0, x1, y1])

    vc = len(cell_groups[i])

    if vc == 0:
        # Skip empty groups (unlikely but safe)
        continue

    doc["buffers"][buf_id] = {"data": data}
    doc["geometries"][geo_id] = {
        "vertexBufferId": 100 + i,
        "format": "rect4",
        "vertexCount": vc
    }

    r, g, b, a = colors_rgba[i]
    doc["drawItems"][di_id] = {
        "layerId": 10,
        "pipeline": "instancedRect@1",
        "geometryId": 200 + i,
        "transformId": 50,
        "color": [r, g, b, a]
    }

# Seed dots buffer, geometry, drawItem
doc["buffers"]["120"] = {"data": [round(v, 6) for v in dot_verts]}
doc["geometries"]["220"] = {
    "vertexBufferId": 120,
    "format": "pos2_alpha",
    "vertexCount": dot_vertex_count
}
doc["drawItems"]["320"] = {
    "layerId": 11,
    "pipeline": "triAA@1",
    "geometryId": 220,
    "transformId": 50,
    "color": [1.0, 1.0, 1.0, 0.9]
}

# Add text overlay
doc["textOverlay"] = {
    "fontSize": 16,
    "color": "#b2b5bc",
    "labels": [
        {"clipX": 0.0, "clipY": 0.95, "text": "Voronoi Diagram - 12 Cells", "align": "c"},
        {"clipX": 0.0, "clipY": -0.97, "text": "25x25 pixelated grid, nearest-seed coloring", "align": "c", "fontSize": 12, "color": "#666"}
    ]
}

# ── Validation ─────────────────────────────────────────────────────────
total_cells = sum(len(g) for g in cell_groups)
print(f"\nTotal grid cells: {total_cells} (expected {GRID*GRID}={GRID**2})")
assert total_cells == GRID * GRID

# Verify buffer sizes match vertex counts
for i in range(12):
    buf_id = str(100 + i)
    geo_id = str(200 + i)
    if buf_id in doc["buffers"]:
        data_len = len(doc["buffers"][buf_id]["data"])
        vc = doc["geometries"][geo_id]["vertexCount"]
        expected_floats = vc * 4  # rect4 = 4 floats
        assert data_len == expected_floats, f"Buffer {buf_id}: {data_len} floats != {vc}*4={expected_floats}"

# Verify dot buffer
dot_data_len = len(doc["buffers"]["120"]["data"])
expected_dot_floats = dot_vertex_count * 3  # pos2_alpha = 3 floats
assert dot_data_len == expected_dot_floats, f"Dot buffer: {dot_data_len} floats != {dot_vertex_count}*3={expected_dot_floats}"

print(f"Dot vertices: {dot_vertex_count}")
print(f"Dot buffer floats: {dot_data_len}")
print("All validations passed!")

# ── Write JSON ─────────────────────────────────────────────────────────
output_path = "/home/ndrandal/Github/DynaCharting/docs/trials/059-voronoi-diagram.json"
with open(output_path, 'w') as f:
    json.dump(doc, f, indent=2)
print(f"\nWrote: {output_path}")

# Copy to charts/
import shutil
charts_path = "/home/ndrandal/Github/DynaCharting/charts/059-voronoi-diagram.json"
shutil.copy(output_path, charts_path)
print(f"Copied: {charts_path}")
