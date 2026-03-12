#!/usr/bin/env python3
"""Generate Sierpinski triangle fractal chart for DynaCharting Trial 063."""

import json
import math

# === Sierpinski Triangle Recursive Subdivision ===
# Data space: [0, 100] x [0, 100]
# Equilateral triangle vertices:
#   A = (5, 5)       -- bottom-left
#   B = (95, 5)      -- bottom-right
#   C = (50, 82.94)  -- top (height = 45 * sqrt(3) ≈ 77.94, centered at x=50)

height = 45.0 * math.sqrt(3)
A = (5.0, 5.0)
B = (95.0, 5.0)
C = (50.0, 5.0 + height)

print(f"Triangle vertices:")
print(f"  A = ({A[0]}, {A[1]})")
print(f"  B = ({B[0]}, {B[1]})")
print(f"  C = ({C[0]:.6f}, {C[1]:.6f})")
print(f"  Height = {height:.6f}")

# Collect leaf triangles grouped by depth-1 sub-tree
# group 0 = bottom-left branch, 1 = bottom-right branch, 2 = top branch
groups = {0: [], 1: [], 2: []}

def midpoint(p1, p2):
    return ((p1[0] + p2[0]) / 2.0, (p1[1] + p2[1]) / 2.0)

def sierpinski(a, b, c, depth, group, result):
    """Recursively subdivide. At depth=0, emit the triangle."""
    if depth == 0:
        result[group].extend([a[0], a[1], b[0], b[1], c[0], c[1]])
        return
    ab = midpoint(a, b)
    bc = midpoint(b, c)
    ca = midpoint(c, a)
    sierpinski(a, ab, ca, depth - 1, group, result)   # bottom-left
    sierpinski(ab, b, bc, depth - 1, group, result)    # bottom-right
    sierpinski(ca, bc, c, depth - 1, group, result)    # top

# At depth=6, we need the first split to assign groups.
# The top-level split (depth 6 -> 5) produces 3 sub-triangles.
# Each sub-triangle then recurses to depth 5 with its assigned group.

ab_top = midpoint(A, B)
bc_top = midpoint(B, C)
ca_top = midpoint(C, A)

# Bottom-left sub-tree (group 0): triangle A, AB, CA
sierpinski(A, ab_top, ca_top, 5, 0, groups)
# Bottom-right sub-tree (group 1): triangle AB, B, BC
sierpinski(ab_top, B, bc_top, 5, 1, groups)
# Top sub-tree (group 2): triangle CA, BC, C
sierpinski(ca_top, bc_top, C, 5, 2, groups)

# Verify counts
for g in range(3):
    num_floats = len(groups[g])
    num_verts = num_floats // 2
    num_tris = num_verts // 3
    print(f"Group {g}: {num_floats} floats, {num_verts} verts, {num_tris} triangles")

total_tris = sum(len(groups[g]) // 6 for g in range(3))
total_verts = sum(len(groups[g]) // 2 for g in range(3))
print(f"Total: {total_tris} triangles, {total_verts} vertices")
assert total_tris == 729, f"Expected 729 triangles, got {total_tris}"
assert total_verts == 2187, f"Expected 2187 vertices, got {total_verts}"

# === Transform ===
# Data space: [0, 100] x [0, 100]
# Clip range: [-0.95, 0.95] (1.9 units) for a square viewport
# sx = (0.95 - (-0.95)) / (100 - 0) = 1.9 / 100 = 0.019
# tx = -0.95 - 0 * 0.019 = -0.95
# sy = 0.019, ty = -0.95
sx = 1.9 / 100.0
sy = 1.9 / 100.0
tx = -0.95
ty = -0.95

print(f"\nTransform: sx={sx}, sy={sy}, tx={tx}, ty={ty}")

# Verify mapping:
print(f"Verify X=0 -> clip: {0.0 * sx + tx:.6f} (expect -0.95)")
print(f"Verify X=100 -> clip: {100.0 * sx + tx:.6f} (expect 0.95)")
print(f"Verify Y=0 -> clip: {0.0 * sy + ty:.6f} (expect -0.95)")
print(f"Verify Y=100 -> clip: {100.0 * sy + ty:.6f} (expect 0.95)")

# Colors (0-1 range)
# Blue: #3b82f6 -> (59/255, 130/255, 246/255)
# Emerald: #10b981 -> (16/255, 185/255, 129/255)
# Amber: #f59e0b -> (245/255, 158/255, 11/255)
color_blue = [59/255, 130/255, 246/255, 1.0]
color_emerald = [16/255, 185/255, 129/255, 1.0]
color_amber = [245/255, 158/255, 11/255, 1.0]

print(f"\nColor blue: [{color_blue[0]:.5f}, {color_blue[1]:.5f}, {color_blue[2]:.5f}, {color_blue[3]:.1f}]")
print(f"Color emerald: [{color_emerald[0]:.5f}, {color_emerald[1]:.5f}, {color_emerald[2]:.5f}, {color_emerald[3]:.1f}]")
print(f"Color amber: [{color_amber[0]:.5f}, {color_amber[1]:.5f}, {color_amber[2]:.5f}, {color_amber[3]:.1f}]")

# === Build SceneDocument ===
# ID plan:
#   Pane: 1
#   Layers: 10 (background), 11 (blue), 12 (emerald), 13 (amber)
#   Transform: 50
#   Buffers: 100 (bg), 101 (blue), 102 (emerald), 103 (amber)
#   Geometries: 200 (bg), 201 (blue), 202 (emerald), 203 (amber)
#   DrawItems: 300 (bg), 301 (blue), 302 (emerald), 303 (amber)

# Background: dark rectangle covering entire data space
# Using instancedRect@1 with rect4: [xMin, yMin, xMax, yMax]
bg_data = [-5.0, -5.0, 105.0, 105.0]

doc = {
    "version": 1,
    "viewport": {"width": 700, "height": 700},
    "buffers": {
        "100": {"data": bg_data},
        "101": {"data": groups[0]},
        "102": {"data": groups[1]},
        "103": {"data": groups[2]}
    },
    "transforms": {
        "50": {"sx": sx, "sy": sy, "tx": tx, "ty": ty}
    },
    "panes": {
        "1": {
            "name": "Sierpinski",
            "region": {
                "clipYMin": -1.0,
                "clipYMax": 1.0,
                "clipXMin": -1.0,
                "clipXMax": 1.0
            },
            "hasClearColor": True,
            "clearColor": [0.058824, 0.090196, 0.164706, 1.0]  # #0f172a
        }
    },
    "layers": {
        "10": {"paneId": 1, "name": "Background"},
        "11": {"paneId": 1, "name": "BlueTriangles"},
        "12": {"paneId": 1, "name": "EmeraldTriangles"},
        "13": {"paneId": 1, "name": "AmberTriangles"}
    },
    "geometries": {
        "200": {"vertexBufferId": 100, "format": "rect4", "vertexCount": 1},
        "201": {"vertexBufferId": 101, "format": "pos2_clip", "vertexCount": 729},
        "202": {"vertexBufferId": 102, "format": "pos2_clip", "vertexCount": 729},
        "203": {"vertexBufferId": 103, "format": "pos2_clip", "vertexCount": 729}
    },
    "drawItems": {
        "300": {
            "layerId": 10,
            "name": "Background",
            "pipeline": "instancedRect@1",
            "geometryId": 200,
            "transformId": 50,
            "color": [0.058824, 0.090196, 0.164706, 1.0]
        },
        "301": {
            "layerId": 11,
            "name": "BlueTriangles",
            "pipeline": "triSolid@1",
            "geometryId": 201,
            "transformId": 50,
            "color": [color_blue[0], color_blue[1], color_blue[2], color_blue[3]]
        },
        "302": {
            "layerId": 12,
            "name": "EmeraldTriangles",
            "pipeline": "triSolid@1",
            "geometryId": 202,
            "transformId": 50,
            "color": [color_emerald[0], color_emerald[1], color_emerald[2], color_emerald[3]]
        },
        "303": {
            "layerId": 13,
            "name": "AmberTriangles",
            "pipeline": "triSolid@1",
            "geometryId": 203,
            "transformId": 50,
            "color": [color_amber[0], color_amber[1], color_amber[2], color_amber[3]]
        }
    },
    "textOverlay": {
        "fontSize": 16,
        "color": "#b2b5bc",
        "labels": [
            {"clipX": 0.0, "clipY": 0.96, "text": "Sierpinski Triangle \u2014 Depth 6 (729 triangles)", "align": "c"},
            {"clipX": -0.92, "clipY": -0.96, "text": "3^6 = 729 leaf triangles", "align": "l", "fontSize": 12, "color": "#666"}
        ]
    }
}

# === Write JSON ===
output_path_trials = "/home/ndrandal/Github/DynaCharting/docs/trials/063-sierpinski-triangle.json"
output_path_charts = "/home/ndrandal/Github/DynaCharting/charts/063-sierpinski-triangle.json"

json_str = json.dumps(doc, indent=2)

with open(output_path_trials, 'w') as f:
    f.write(json_str)
    f.write('\n')

with open(output_path_charts, 'w') as f:
    f.write(json_str)
    f.write('\n')

print(f"\nWritten to {output_path_trials}")
print(f"Written to {output_path_charts}")

# === Verification ===
print("\n=== Verification ===")

# Check vertex counts per group
for g, name in [(0, "Blue/bottom-left"), (1, "Emerald/bottom-right"), (2, "Amber/top")]:
    nf = len(groups[g])
    nv = nf // 2
    nt = nv // 3
    assert nt == 243, f"Group {g} ({name}): expected 243 triangles, got {nt}"
    assert nv == 729, f"Group {g} ({name}): expected 729 vertices, got {nv}"
    assert nf == 1458, f"Group {g} ({name}): expected 1458 floats, got {nf}"
    print(f"  Group {g} ({name}): {nt} triangles, {nv} vertices, {nf} floats: OK")

# Verify all vertex data is within data space bounds [0, 100]
all_ok = True
for g in range(3):
    data = groups[g]
    for i in range(0, len(data), 2):
        x, y = data[i], data[i+1]
        if x < 0 or x > 100 or y < 0 or y > 100:
            print(f"  FAIL: Group {g} vertex at index {i//2}: ({x}, {y}) out of [0,100]")
            all_ok = False
if all_ok:
    print("  All vertices within [0, 100] data space: OK")

# Verify vertex counts match geometry declarations
for geom_id, expected_vc in [("201", 729), ("202", 729), ("203", 729)]:
    actual_vc = doc["geometries"][geom_id]["vertexCount"]
    assert actual_vc == expected_vc, f"Geometry {geom_id}: expected vertexCount={expected_vc}, got {actual_vc}"
print("  Geometry vertex counts match: OK")

# Verify buffer float counts match vertex counts * 2 (pos2_clip = 2 floats per vertex)
for buf_id, geom_id in [("101", "201"), ("102", "202"), ("103", "203")]:
    buf_floats = len(doc["buffers"][buf_id]["data"])
    vc = doc["geometries"][geom_id]["vertexCount"]
    expected_floats = vc * 2  # pos2_clip = 2 floats per vertex
    assert buf_floats == expected_floats, f"Buffer {buf_id}: {buf_floats} floats, expected {expected_floats} for {vc} vertices"
print("  Buffer sizes match vertex counts: OK")

# Verify triSolid@1 requires vertexCount multiple of 3
for geom_id in ["201", "202", "203"]:
    vc = doc["geometries"][geom_id]["vertexCount"]
    assert vc % 3 == 0, f"Geometry {geom_id}: vertexCount {vc} not multiple of 3"
print("  triSolid@1 vertex count divisibility: OK")

# Verify all IDs are unique
all_ids = [1, 10, 11, 12, 13, 50, 100, 101, 102, 103, 200, 201, 202, 203, 300, 301, 302, 303]
assert len(all_ids) == len(set(all_ids)), "ID collision!"
print(f"  All {len(all_ids)} IDs unique: OK")

# Verify triangle area sanity (no degenerate triangles)
degenerate_count = 0
for g in range(3):
    data = groups[g]
    for i in range(0, len(data), 6):
        ax, ay = data[i], data[i+1]
        bx, by = data[i+2], data[i+3]
        cx, cy = data[i+4], data[i+5]
        # Signed area
        area = abs((bx - ax) * (cy - ay) - (cx - ax) * (by - ay)) / 2.0
        if area < 1e-10:
            degenerate_count += 1
if degenerate_count == 0:
    print("  No degenerate triangles: OK")
else:
    print(f"  WARNING: {degenerate_count} degenerate triangles!")

# Check self-similarity: all leaf triangles should have the same area
areas = []
for g in range(3):
    data = groups[g]
    for i in range(0, len(data), 6):
        ax, ay = data[i], data[i+1]
        bx, by = data[i+2], data[i+3]
        cx, cy = data[i+4], data[i+5]
        area = abs((bx - ax) * (cy - ay) - (cx - ax) * (by - ay)) / 2.0
        areas.append(area)

min_area = min(areas)
max_area = max(areas)
print(f"  Leaf triangle areas: min={min_area:.6f}, max={max_area:.6f}, ratio={max_area/min_area:.6f}")
if abs(max_area - min_area) < 1e-6:
    print("  All leaf triangles have equal area: OK")
else:
    print("  WARNING: leaf triangles have unequal areas!")

# Verify the 3 color groups correspond to the correct spatial regions
# Bottom-left group should have centroid X < 50
# Bottom-right group should have centroid X > 50
# Top group should have centroid Y > ~40
for g, name, check in [(0, "Bottom-left", lambda cx, cy: cx < 50),
                        (1, "Bottom-right", lambda cx, cy: cx > 50),
                        (2, "Top", lambda cx, cy: cy > 40)]:
    data = groups[g]
    cx_sum, cy_sum, count = 0, 0, 0
    for i in range(0, len(data), 6):
        cx_sum += (data[i] + data[i+2] + data[i+4]) / 3.0
        cy_sum += (data[i+1] + data[i+3] + data[i+5]) / 3.0
        count += 1
    cx_avg = cx_sum / count
    cy_avg = cy_sum / count
    ok = check(cx_avg, cy_avg)
    print(f"  Group {g} ({name}): centroid=({cx_avg:.2f}, {cy_avg:.2f}) -> {'OK' if ok else 'FAIL'}")

print("\nDone.")
