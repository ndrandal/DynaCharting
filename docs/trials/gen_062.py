#!/usr/bin/env python3
"""Generate QR code pattern chart for DynaCharting Trial 062."""

import json
import struct
import math

# === QR Code Grid (21x21, Version 1) ===
GRID_SIZE = 21
grid = [[None for _ in range(GRID_SIZE)] for _ in range(GRID_SIZE)]  # None = unfilled

def set_module(col, row, black):
    """Set module at (col, row). black=True for dark, False for light."""
    grid[row][col] = black

def place_finder_pattern(col0, row0):
    """Place a 7x7 finder pattern with top-left at (col0, row0).
    Outer ring: black, middle ring: white, inner 3x3: black."""
    for r in range(7):
        for c in range(7):
            col = col0 + c
            row = row0 + r
            # Determine if this position is black or white
            # Outer ring (border of 7x7)
            if r == 0 or r == 6 or c == 0 or c == 6:
                set_module(col, row, True)  # black outer ring
            # Middle ring (border of inner 5x5)
            elif r == 1 or r == 5 or c == 1 or c == 5:
                set_module(col, row, False)  # white middle ring
            # Inner 3x3
            else:
                set_module(col, row, True)  # black inner square

# Place 3 finder patterns
place_finder_pattern(0, 0)      # Top-left: rows 0-6, cols 0-6
place_finder_pattern(14, 0)     # Top-right: rows 0-6, cols 14-20
place_finder_pattern(0, 14)     # Bottom-left: rows 14-20, cols 0-6

# Timing patterns
# Row 6, cols 8-12: alternating black/white starting with black
for c in range(8, 13):
    set_module(c, 6, (c - 8) % 2 == 0)  # col 8=black, 9=white, 10=black, 11=white, 12=black

# Col 6, rows 8-12: alternating black/white starting with black
for r in range(8, 13):
    set_module(6, r, (r - 8) % 2 == 0)  # row 8=black, 9=white, 10=black, 11=white, 12=black

# Dark module at (col=8, row=13)
set_module(8, 13, True)

# Fill data area: remaining unfilled modules
for r in range(GRID_SIZE):
    for c in range(GRID_SIZE):
        if grid[r][c] is None:
            # Deterministic pattern
            grid[r][c] = ((c * 7 + r * 13 + 42) % 3 == 0)

# === Separate into black and white module lists ===
black_modules = []
white_modules = []

for r in range(GRID_SIZE):
    for c in range(GRID_SIZE):
        if grid[r][c]:
            black_modules.append((c, r))
        else:
            white_modules.append((c, r))

print(f"Black modules: {len(black_modules)}")
print(f"White modules: {len(white_modules)}")
print(f"Total: {len(black_modules) + len(white_modules)} (expected 441)")

# === Build vertex data ===
# instancedRect@1 uses rect4: [xMin, yMin, xMax, yMax] per rect
# Row r maps to Y = 20 - r (row 0 at top, row 20 at bottom)

def module_rect(col, row):
    """Return rect4 floats for a module at grid position (col, row)."""
    y = 20 - row  # invert Y so row 0 is at top
    return [float(col), float(y), float(col + 1), float(y + 1)]

# Background: single large white rect covering quiet zone
bg_data = [-4.0, -4.0, 25.0, 25.0]

# White modules
white_data = []
for (c, r) in white_modules:
    white_data.extend(module_rect(c, r))

# Black modules
black_data = []
for (c, r) in black_modules:
    black_data.extend(module_rect(c, r))

print(f"Background floats: {len(bg_data)} (expected 4)")
print(f"White module floats: {len(white_data)} (expected {len(white_modules) * 4})")
print(f"Black module floats: {len(black_data)} (expected {len(black_modules) * 4})")

# === Transform ===
# Data space: [-4, 25] on both axes (29 units)
# Clip range: [-0.95, 0.95] (1.9 units)
# sx = sy = 1.9 / 29 = 0.0655172...
# tx = ty = -0.95 - (-4) * sx = -0.95 + 4 * 0.0655172 = -0.95 + 0.2620689 = -0.6879310...
sx = 1.9 / 29.0
sy = 1.9 / 29.0
tx = -0.95 - (-4.0) * sx
ty = -0.95 - (-4.0) * sy

print(f"Transform: sx={sx:.9f}, sy={sy:.9f}, tx={tx:.9f}, ty={ty:.9f}")

# Verify mapping:
# X=-4 -> -4 * sx + tx = -4*0.0655172 + (-0.6879310) = -0.2620689 + (-0.6879310) = -0.95 ✓
# X=25 -> 25 * sx + tx = 25*0.0655172 + (-0.6879310) = 1.6379310 + (-0.6879310) = 0.95 ✓
print(f"Verify X=-4 -> clip: {-4.0 * sx + tx:.6f} (expect -0.95)")
print(f"Verify X=25 -> clip: {25.0 * sx + tx:.6f} (expect 0.95)")
print(f"Verify Y=-4 -> clip: {-4.0 * sy + ty:.6f} (expect -0.95)")
print(f"Verify Y=25 -> clip: {25.0 * sy + ty:.6f} (expect 0.95)")

# Colors (0-1 range)
# Black modules: #1e293b -> (30/255, 41/255, 59/255) = (0.11765, 0.16078, 0.23137)
# White modules/background: #f8fafc -> (248/255, 250/255, 252/255) = (0.97255, 0.98039, 0.98824)
color_black = [30/255, 41/255, 59/255, 1.0]
color_white = [248/255, 250/255, 252/255, 1.0]

print(f"Color black: [{color_black[0]:.5f}, {color_black[1]:.5f}, {color_black[2]:.5f}, {color_black[3]:.1f}]")
print(f"Color white: [{color_white[0]:.5f}, {color_white[1]:.5f}, {color_white[2]:.5f}, {color_white[3]:.1f}]")

# === Build SceneDocument ===
doc = {
    "version": 1,
    "viewport": {"width": 600, "height": 600},
    "buffers": {
        "100": {"data": bg_data},
        "101": {"data": white_data},
        "102": {"data": black_data}
    },
    "transforms": {
        "50": {"sx": sx, "sy": sy, "tx": tx, "ty": ty}
    },
    "panes": {
        "1": {
            "name": "QR",
            "region": {
                "clipYMin": -1.0,
                "clipYMax": 1.0,
                "clipXMin": -1.0,
                "clipXMax": 1.0
            },
            "hasClearColor": True,
            "clearColor": [color_white[0], color_white[1], color_white[2], 1.0]
        }
    },
    "layers": {
        "10": {"paneId": 1, "name": "Background"},
        "11": {"paneId": 1, "name": "WhiteModules"},
        "12": {"paneId": 1, "name": "BlackModules"}
    },
    "geometries": {
        "200": {"vertexBufferId": 100, "format": "rect4", "vertexCount": 1},
        "201": {"vertexBufferId": 101, "format": "rect4", "vertexCount": len(white_modules)},
        "202": {"vertexBufferId": 102, "format": "rect4", "vertexCount": len(black_modules)}
    },
    "drawItems": {
        "300": {
            "layerId": 10,
            "name": "Background",
            "pipeline": "instancedRect@1",
            "geometryId": 200,
            "transformId": 50,
            "color": [color_white[0], color_white[1], color_white[2], 1.0]
        },
        "301": {
            "layerId": 11,
            "name": "WhiteModules",
            "pipeline": "instancedRect@1",
            "geometryId": 201,
            "transformId": 50,
            "color": [color_white[0], color_white[1], color_white[2], 1.0]
        },
        "302": {
            "layerId": 12,
            "name": "BlackModules",
            "pipeline": "instancedRect@1",
            "geometryId": 202,
            "transformId": 50,
            "color": [color_black[0], color_black[1], color_black[2], 1.0]
        }
    }
}

# === Write JSON ===
output_path_trials = "/home/ndrandal/Github/DynaCharting/docs/trials/062-qr-code.json"
output_path_charts = "/home/ndrandal/Github/DynaCharting/charts/062-qr-code.json"

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
# Check finder patterns
print("\n=== Verification ===")

# Top-left finder (rows 0-6, cols 0-6)
def verify_finder(name, col0, row0):
    ok = True
    for r in range(7):
        for c in range(7):
            col = col0 + c
            row = row0 + r
            expected = None
            if r == 0 or r == 6 or c == 0 or c == 6:
                expected = True  # outer ring black
            elif r == 1 or r == 5 or c == 1 or c == 5:
                expected = False  # middle ring white
            else:
                expected = True  # inner 3x3 black
            if grid[row][col] != expected:
                print(f"  FAIL {name} ({col},{row}): got {grid[row][col]}, expected {expected}")
                ok = False
    if ok:
        print(f"  {name}: OK")

verify_finder("Top-left", 0, 0)
verify_finder("Top-right", 14, 0)
verify_finder("Bottom-left", 0, 14)

# Timing patterns
timing_ok = True
for c in range(8, 13):
    expected = (c - 8) % 2 == 0
    if grid[6][c] != expected:
        print(f"  FAIL timing row6 col{c}: got {grid[6][c]}, expected {expected}")
        timing_ok = False
for r in range(8, 13):
    expected = (r - 8) % 2 == 0
    if grid[r][6] != expected:
        print(f"  FAIL timing col6 row{r}: got {grid[r][6]}, expected {expected}")
        timing_ok = False
if timing_ok:
    print("  Timing patterns: OK")

# Dark module
if grid[13][8]:
    print("  Dark module (8,13): OK")
else:
    print("  FAIL Dark module (8,13)")

# Count
print(f"\n  Total modules: {len(black_modules) + len(white_modules)} (expected 441)")
print(f"  Black: {len(black_modules)}, White: {len(white_modules)}")

# Verify all IDs are unique
all_ids = [1, 10, 11, 12, 50, 100, 101, 102, 200, 201, 202, 300, 301, 302]
assert len(all_ids) == len(set(all_ids)), "ID collision!"
print(f"  All {len(all_ids)} IDs unique: OK")

# Print visual grid for inspection
print("\n=== Visual Grid (# = black, . = white) ===")
for r in range(GRID_SIZE):
    row_str = ""
    for c in range(GRID_SIZE):
        row_str += "#" if grid[r][c] else "."
    print(f"  row {r:2d}: {row_str}")
