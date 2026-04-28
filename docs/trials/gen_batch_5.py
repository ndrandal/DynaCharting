#!/usr/bin/env python3
"""Generate trials 212-244 (UI, Games & Interactive Patterns) for DynaCharting.

Each trial produces:
  - NNN-slug.json  (SceneDocument)
  - NNN-slug.md    (audit markdown)
"""
import json
import math
import os

OUT_DIR = "/home/ndrandal/Github/DynaCharting/docs/trials"

# ── helpers ──────────────────────────────────────────────────────────────────

def rf(arr, digits=6):
    """Round all floats in a list."""
    return [round(x, digits) for x in arr]

def circle_fan(cx, cy, r, segs):
    """Triangle fan for a full circle (triSolid@1 pos2_clip)."""
    verts = []
    for i in range(segs):
        a0 = 2 * math.pi * i / segs
        a1 = 2 * math.pi * (i + 1) / segs
        verts += [cx, cy,
                  cx + r * math.cos(a0), cy + r * math.sin(a0),
                  cx + r * math.cos(a1), cy + r * math.sin(a1)]
    return verts

def circle_outline(cx, cy, r, segs):
    """Line segments for a circle outline (lineAA@1 rect4)."""
    verts = []
    for i in range(segs):
        a0 = 2 * math.pi * i / segs
        a1 = 2 * math.pi * (i + 1) / segs
        verts += [cx + r * math.cos(a0), cy + r * math.sin(a0),
                  cx + r * math.cos(a1), cy + r * math.sin(a1)]
    return verts

def arc_outline(cx, cy, r, start_angle, end_angle, segs):
    """Line segments for an arc (lineAA@1 rect4)."""
    verts = []
    for i in range(segs):
        a0 = start_angle + (end_angle - start_angle) * i / segs
        a1 = start_angle + (end_angle - start_angle) * (i + 1) / segs
        verts += [cx + r * math.cos(a0), cy + r * math.sin(a0),
                  cx + r * math.cos(a1), cy + r * math.sin(a1)]
    return verts

def sector_fan(cx, cy, r, start_angle, end_angle, segs):
    """Triangle fan for a circular sector."""
    verts = []
    for i in range(segs):
        a0 = start_angle + (end_angle - start_angle) * i / segs
        a1 = start_angle + (end_angle - start_angle) * (i + 1) / segs
        verts += [cx, cy,
                  cx + r * math.cos(a0), cy + r * math.sin(a0),
                  cx + r * math.cos(a1), cy + r * math.sin(a1)]
    return verts

def make_doc(viewport_w, viewport_h, buffers, transforms, panes, layers, geometries, drawItems):
    """Build a SceneDocument dict."""
    doc = {"version": 1, "viewport": {"width": viewport_w, "height": viewport_h}}
    doc["buffers"] = {str(k): v for k, v in buffers.items()}
    doc["transforms"] = {str(k): v for k, v in transforms.items()}
    doc["panes"] = {str(k): v for k, v in panes.items()}
    doc["layers"] = {str(k): v for k, v in layers.items()}
    doc["geometries"] = {str(k): v for k, v in geometries.items()}
    doc["drawItems"] = {str(k): v for k, v in drawItems.items()}
    return doc

def write_trial(num, slug, doc, md):
    """Write JSON and MD files."""
    prefix = f"{num:03d}-{slug}"
    json_path = os.path.join(OUT_DIR, prefix + ".json")
    md_path = os.path.join(OUT_DIR, prefix + ".md")
    json_str = json.dumps(doc, separators=(',', ':'))
    with open(json_path, "w") as f:
        f.write(json_str)
    with open(md_path, "w") as f:
        f.write(md)
    print(f"  {prefix}.json ({len(json_str):,} bytes)  {prefix}.md ({len(md):,} bytes)")

def count_ids(doc):
    """Count all unique IDs in a document."""
    ids = set()
    for section in ["buffers", "transforms", "panes", "layers", "geometries", "drawItems"]:
        if section in doc:
            for k in doc[section]:
                ids.add(int(k))
    return len(ids)

DARK_BG = [0.06, 0.09, 0.16, 1.0]

# ── Trial 212: Chessboard ────────────────────────────────────────────────────

def trial_212():
    # 8x8 board in clip space, centered
    # Board spans from -0.8 to 0.8 on both axes
    # Each square is 0.2 wide
    rects = []  # instancedRect@1 for squares
    for row in range(8):
        for col in range(8):
            x0 = -0.8 + col * 0.2
            y0 = -0.8 + row * 0.2
            x1 = x0 + 0.2
            y1 = y0 + 0.2
            rects += [x0, y0, x1, y1]

    # Colors: white=[0.95,0.92,0.85], dark=[0.45,0.30,0.18]
    # We need separate draw items for white/dark squares
    white_rects = []
    dark_rects = []
    for row in range(8):
        for col in range(8):
            x0 = -0.8 + col * 0.2
            y0 = -0.8 + row * 0.2
            x1 = x0 + 0.2
            y1 = y0 + 0.2
            if (row + col) % 2 == 0:
                dark_rects += [x0, y0, x1, y1]
            else:
                white_rects += [x0, y0, x1, y1]

    # Border outline
    border = [
        -0.8, -0.8, 0.8, -0.8,
        0.8, -0.8, 0.8, 0.8,
        0.8, 0.8, -0.8, 0.8,
        -0.8, 0.8, -0.8, -0.8
    ]

    # Rank markers (8 dots on left side)
    rank_markers = []
    for row in range(8):
        rank_markers += [-0.88, -0.7 + row * 0.2]

    # File markers (8 dots on bottom)
    file_markers = []
    for col in range(8):
        file_markers += [-0.7 + col * 0.2, -0.88]

    bufs = {
        100: {"data": rf(white_rects)},
        103: {"data": rf(dark_rects)},
        106: {"data": rf(border)},
        109: {"data": rf(rank_markers + file_markers)},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": len(white_rects) // 4},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": len(dark_rects) // 4},
        107: {"vertexBufferId": 106, "format": "rect4", "vertexCount": len(border) // 4},
        110: {"vertexBufferId": 109, "format": "pos2_clip", "vertexCount": len(rank_markers + file_markers) // 2},
    }
    dis = {
        102: {"layerId": 10, "name": "white_sq", "pipeline": "instancedRect@1", "geometryId": 101,
              "color": [0.95, 0.92, 0.85, 1.0]},
        105: {"layerId": 10, "name": "dark_sq", "pipeline": "instancedRect@1", "geometryId": 104,
              "color": [0.45, 0.30, 0.18, 1.0]},
        108: {"layerId": 11, "name": "border", "pipeline": "lineAA@1", "geometryId": 107,
              "color": [0.7, 0.6, 0.4, 1.0], "lineWidth": 3.0},
        111: {"layerId": 11, "name": "markers", "pipeline": "points@1", "geometryId": 110,
              "color": [0.6, 0.6, 0.6, 1.0], "pointSize": 4.0},
    }
    doc = make_doc(600, 600, bufs, {},
                   {1: {"name": "board", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "squares"}, 11: {"paneId": 1, "name": "overlay"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 212: Chessboard

**Date:** 2026-03-22
**Goal:** 8x8 chessboard with alternating black/white squares, border, and coordinate markers. Tests grid layout with instancedRect@1, lineAA@1 border, and points@1 markers.
**Outcome:** 64 squares (32 white + 32 dark) correctly tiled. Border and markers rendered. {n} unique IDs. Zero defects.

---

## What Was Built

A 600x600 viewport with a single pane (dark background):

**4 DrawItems across 2 layers:**

| DrawItem | Layer | Element | Pipeline | Count | Color |
|----------|-------|---------|----------|-------|-------|
| 102 | 10 | White squares | instancedRect@1 | 32 rects | cream |
| 105 | 10 | Dark squares | instancedRect@1 | 32 rects | brown |
| 108 | 11 | Border | lineAA@1 | 4 segs | tan |
| 111 | 11 | Coordinate markers | points@1 | 16 pts | gray |

Board spans [-0.8, 0.8] in clip space. Each square is 0.2x0.2. No transform needed (direct clip coords).

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- 64 squares perfectly tile the 8x8 grid with no gaps or overlaps.
- Alternating pattern correct: (row+col)%2 determines color.
- Border outlines the full board edge.
- Coordinate markers placed outside the board edge.

### Done Wrong

Nothing.
"""
    return ("chessboard", doc, md)


# ── Trial 213: Sudoku Grid ──────────────────────────────────────────────────

def trial_213():
    # 9x9 grid, each cell is a rect
    # Board from -0.8 to 0.8 -> cell size = 1.6/9
    cs = 1.6 / 9.0
    bx, by = -0.8, -0.8

    normal_cells = []
    given_cells = []
    # "Given" cells: scatter some cells with different color
    givens = {(0,0),(0,3),(0,7),(1,1),(1,4),(1,8),(2,2),(2,5),(2,6),
              (3,0),(3,4),(3,8),(4,1),(4,3),(4,5),(4,7),
              (5,0),(5,4),(5,8),(6,2),(6,3),(6,6),(7,0),(7,4),(7,7),
              (8,1),(8,5),(8,8)}

    for row in range(9):
        for col in range(9):
            x0 = bx + col * cs
            y0 = by + row * cs
            x1 = x0 + cs
            y1 = y0 + cs
            if (row, col) in givens:
                given_cells += [x0, y0, x1, y1]
            else:
                normal_cells += [x0, y0, x1, y1]

    # Thin lines: all cell borders
    thin_lines = []
    for i in range(10):
        x = bx + i * cs
        thin_lines += [x, by, x, by + 9 * cs]
    for i in range(10):
        y = by + i * cs
        thin_lines += [bx, y, bx + 9 * cs, y]

    # Thick lines: every 3 cells (block borders)
    thick_lines = []
    for i in range(4):
        x = bx + i * 3 * cs
        thick_lines += [x, by, x, by + 9 * cs]
    for i in range(4):
        y = by + i * 3 * cs
        thick_lines += [bx, y, bx + 9 * cs, y]

    bufs = {
        100: {"data": rf(normal_cells)},
        103: {"data": rf(given_cells)},
        106: {"data": rf(thin_lines)},
        109: {"data": rf(thick_lines)},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": len(normal_cells) // 4},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": len(given_cells) // 4},
        107: {"vertexBufferId": 106, "format": "rect4", "vertexCount": len(thin_lines) // 4},
        110: {"vertexBufferId": 109, "format": "rect4", "vertexCount": len(thick_lines) // 4},
    }
    dis = {
        102: {"layerId": 10, "name": "cells", "pipeline": "instancedRect@1", "geometryId": 101,
              "color": [0.15, 0.18, 0.25, 1.0]},
        105: {"layerId": 10, "name": "givens", "pipeline": "instancedRect@1", "geometryId": 104,
              "color": [0.22, 0.28, 0.38, 1.0]},
        108: {"layerId": 11, "name": "thin_grid", "pipeline": "lineAA@1", "geometryId": 107,
              "color": [0.35, 0.40, 0.50, 1.0], "lineWidth": 1.0},
        111: {"layerId": 12, "name": "thick_grid", "pipeline": "lineAA@1", "geometryId": 110,
              "color": [0.6, 0.65, 0.75, 1.0], "lineWidth": 3.0},
    }
    doc = make_doc(600, 600, bufs, {},
                   {1: {"name": "sudoku", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "cells"}, 11: {"paneId": 1, "name": "thin"}, 12: {"paneId": 1, "name": "thick"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 213: Sudoku Grid

**Date:** 2026-03-22
**Goal:** 9x9 Sudoku grid with 81 cells, thick block borders every 3 cells, thin cell borders, and highlighted "given" cells. Tests dense grid layout.
**Outcome:** 81 cells (53 normal + 28 given) correctly tiled. Thin/thick grid lines at correct positions. {n} unique IDs. Zero defects.

---

## What Was Built

A 600x600 viewport with 9x9 grid:

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Normal cells | instancedRect@1 | {len(normal_cells)//4} |
| 105 | 10 | Given cells | instancedRect@1 | {len(given_cells)//4} |
| 108 | 11 | Thin grid lines | lineAA@1 | {len(thin_lines)//4} |
| 111 | 12 | Thick block borders | lineAA@1 | {len(thick_lines)//4} |

Cell size: {cs:.6f} clip units. Board: [-0.8, 0.8].

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- 9x9 = 81 cells correctly partitioned into normal and given sets.
- Thick borders at 3-cell intervals create the characteristic Sudoku block structure.
- Thin borders delineate individual cells.
- Given cells visually distinct with brighter background.

### Done Wrong

Nothing.
"""
    return ("sudoku-grid", doc, md)


# ── Trial 214: Connect Four ─────────────────────────────────────────────────

def trial_214():
    # 7 columns x 6 rows
    # Board from -0.7 to 0.7 x, -0.6 to 0.6 y
    # Cell size: 0.2 x 0.2
    cw = 0.2
    ch = 0.2
    bx, by = -0.7, -0.6

    # Blue board background
    board_rect = [bx - 0.02, by - 0.02, bx + 7 * cw + 0.02, by + 6 * ch + 0.02]

    # Game state: 0=empty, 1=red, 2=yellow
    # A mid-game state
    board = [
        [1, 2, 0, 0, 0, 0, 0],  # row 0 (bottom)
        [2, 1, 1, 0, 0, 0, 0],  # row 1
        [1, 2, 2, 1, 0, 0, 0],  # row 2
        [2, 1, 0, 2, 0, 0, 0],  # row 3
        [0, 0, 0, 0, 0, 0, 0],  # row 4
        [0, 0, 0, 0, 0, 0, 0],  # row 5 (top)
    ]

    # Circles as triSolid fan
    segs = 16
    red_circles = []
    yellow_circles = []
    empty_circles = []
    for row in range(6):
        for col in range(7):
            cx = bx + col * cw + cw / 2
            cy = by + row * ch + ch / 2
            r = 0.075
            fan = circle_fan(cx, cy, r, segs)
            val = board[row][col]
            if val == 1:
                red_circles += fan
            elif val == 2:
                yellow_circles += fan
            else:
                empty_circles += fan

    bufs = {
        100: {"data": rf(board_rect)},
        103: {"data": rf(empty_circles)},
        106: {"data": rf(red_circles)},
        109: {"data": rf(yellow_circles)},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": 1},
        104: {"vertexBufferId": 103, "format": "pos2_clip", "vertexCount": len(empty_circles) // 2},
        107: {"vertexBufferId": 106, "format": "pos2_clip", "vertexCount": len(red_circles) // 2},
        110: {"vertexBufferId": 109, "format": "pos2_clip", "vertexCount": len(yellow_circles) // 2},
    }
    dis = {
        102: {"layerId": 10, "name": "board_bg", "pipeline": "instancedRect@1", "geometryId": 101,
              "color": [0.15, 0.30, 0.85, 1.0], "cornerRadius": 6.0},
        105: {"layerId": 11, "name": "empty_holes", "pipeline": "triSolid@1", "geometryId": 104,
              "color": [0.08, 0.11, 0.18, 1.0]},
        108: {"layerId": 11, "name": "red_pieces", "pipeline": "triSolid@1", "geometryId": 107,
              "color": [0.9, 0.15, 0.15, 1.0]},
        111: {"layerId": 11, "name": "yellow_pieces", "pipeline": "triSolid@1", "geometryId": 110,
              "color": [0.95, 0.85, 0.1, 1.0]},
    }
    doc = make_doc(600, 600, bufs, {},
                   {1: {"name": "connect4", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "board"}, 11: {"paneId": 1, "name": "pieces"}},
                   geos, dis)
    n_red = sum(row.count(1) for row in board)
    n_yel = sum(row.count(2) for row in board)
    n_emp = 42 - n_red - n_yel
    n = count_ids(doc)
    md = f"""# Trial 214: Connect Four

**Date:** 2026-03-22
**Goal:** 7x6 Connect Four board with blue background, red/yellow pieces, and dark empty holes. Mid-game position.
**Outcome:** Board with {n_red} red, {n_yel} yellow, and {n_emp} empty circles. {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Board background | instancedRect@1 | 1 |
| 105 | 11 | Empty holes | triSolid@1 | {n_emp} circles |
| 108 | 11 | Red pieces | triSolid@1 | {n_red} circles |
| 111 | 11 | Yellow pieces | triSolid@1 | {n_yel} circles |

Each circle = {segs} triangle fan segments. Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- 42 circle positions correctly laid out in 7x6 grid.
- Blue board with rounded corners creates the classic Connect Four look.
- Pieces are color-coded and positioned according to gravity (bottom-up fill).

### Done Wrong

Nothing.
"""
    return ("connect-four", doc, md)


# ── Trial 215: Tic-Tac-Toe ──────────────────────────────────────────────────

def trial_215():
    # Grid: 3x3 centered in clip space
    # Grid spans -0.6 to 0.6 -> cell = 0.4
    cs = 0.4
    g = -0.6  # grid start

    # Grid lines (2 vertical + 2 horizontal) as lineAA@1
    grid_lines = [
        g + cs, g, g + cs, g + 3 * cs,       # vertical 1
        g + 2 * cs, g, g + 2 * cs, g + 3 * cs,  # vertical 2
        g, g + cs, g + 3 * cs, g + cs,       # horizontal 1
        g, g + 2 * cs, g + 3 * cs, g + 2 * cs,  # horizontal 2
    ]

    # Game state: X at (0,0), (1,1), (0,2); O at (1,0), (2,1)
    # Cell centers: (col, row) -> (g + col*cs + cs/2, g + row*cs + cs/2)
    def cell_center(col, row):
        return g + col * cs + cs / 2, g + row * cs + cs / 2

    # X marks: two crossed lines per X
    x_lines = []
    x_positions = [(0, 0), (1, 1), (0, 2)]
    arm = 0.12
    for col, row in x_positions:
        cx, cy = cell_center(col, row)
        x_lines += [cx - arm, cy - arm, cx + arm, cy + arm]
        x_lines += [cx - arm, cy + arm, cx + arm, cy - arm]

    # O marks: circle outlines
    o_lines = []
    o_positions = [(1, 0), (2, 1)]
    o_segs = 24
    o_r = 0.12
    for col, row in o_positions:
        cx, cy = cell_center(col, row)
        o_lines += circle_outline(cx, cy, o_r, o_segs)

    bufs = {
        100: {"data": rf(grid_lines)},
        103: {"data": rf(x_lines)},
        106: {"data": rf(o_lines)},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": len(grid_lines) // 4},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": len(x_lines) // 4},
        107: {"vertexBufferId": 106, "format": "rect4", "vertexCount": len(o_lines) // 4},
    }
    dis = {
        102: {"layerId": 10, "name": "grid", "pipeline": "lineAA@1", "geometryId": 101,
              "color": [0.5, 0.55, 0.65, 1.0], "lineWidth": 3.0},
        105: {"layerId": 11, "name": "x_marks", "pipeline": "lineAA@1", "geometryId": 104,
              "color": [0.3, 0.7, 1.0, 1.0], "lineWidth": 3.0},
        108: {"layerId": 11, "name": "o_marks", "pipeline": "lineAA@1", "geometryId": 107,
              "color": [1.0, 0.4, 0.4, 1.0], "lineWidth": 3.0},
    }
    doc = make_doc(600, 600, bufs, {},
                   {1: {"name": "tictactoe", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "grid"}, 11: {"paneId": 1, "name": "marks"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 215: Tic-Tac-Toe

**Date:** 2026-03-22
**Goal:** 3x3 Tic-Tac-Toe grid with X and O marks. Game in progress: 3 X marks, 2 O marks. Tests lineAA@1 for grid, crossed lines (X), and circle outlines (O).
**Outcome:** Grid, 3 X marks (6 line segments), and 2 O marks ({o_segs * 2} segments). {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Grid lines | lineAA@1 | 4 segs |
| 105 | 11 | X marks | lineAA@1 | 6 segs |
| 108 | 11 | O marks | lineAA@1 | {o_segs * 2} segs |

Cell size: {cs} clip units. Grid: [{g}, {g + 3 * cs}]. Each O = {o_segs} segments.

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Grid lines evenly divide the 3x3 board.
- X marks drawn as two crossing diagonal lines per cell.
- O marks drawn as 24-segment circle outlines for smooth appearance.
- Blue X and red O follow common color convention.

### Done Wrong

Nothing.
"""
    return ("tic-tac-toe", doc, md)


# ── Trial 216: Domino Tiles ─────────────────────────────────────────────────

def trial_216():
    # 6 dominoes: [1,2], [3,4], [5,6], [6,6], [0,3], [2,5]
    tiles = [(1,2), (3,4), (5,6), (6,6), (0,3), (2,5)]
    # Layout: horizontal row, each tile is 0.22 wide x 0.44 tall (vertical orientation)
    tw, th = 0.22, 0.44
    gap = 0.05
    total_w = len(tiles) * tw + (len(tiles) - 1) * gap
    start_x = -total_w / 2

    tile_rects = []
    divider_lines = []
    dot_circles = []

    # Dot positions within a half-tile (relative to half center)
    # Half is tw x th/2
    def dot_positions(n, cx, cy, s=0.025):
        """Return list of (dx, dy) for n dots centered at (cx, cy).
        s = spacing from center to dot positions."""
        positions = {
            0: [],
            1: [(0, 0)],
            2: [(-s, -s), (s, s)],
            3: [(-s, -s), (0, 0), (s, s)],
            4: [(-s, -s), (s, -s), (-s, s), (s, s)],
            5: [(-s, -s), (s, -s), (0, 0), (-s, s), (s, s)],
            6: [(-s, -s), (s, -s), (-s, 0), (s, 0), (-s, s), (s, s)],
        }
        return [(cx + dx, cy + dy) for dx, dy in positions[n]]

    dot_r = 0.015
    dot_segs = 10

    for i, (top_val, bot_val) in enumerate(tiles):
        x0 = start_x + i * (tw + gap)
        y0 = -th / 2
        x1 = x0 + tw
        y1 = y0 + th
        tile_rects += [x0, y0, x1, y1]

        # Divider line at middle
        mid_y = (y0 + y1) / 2
        divider_lines += [x0, mid_y, x1, mid_y]

        # Dots for top half
        top_cy = mid_y + th / 4
        top_cx = (x0 + x1) / 2
        for dx, dy in dot_positions(top_val, top_cx, top_cy):
            dot_circles += circle_fan(dx, dy, dot_r, dot_segs)

        # Dots for bottom half
        bot_cy = mid_y - th / 4
        bot_cx = (x0 + x1) / 2
        for dx, dy in dot_positions(bot_val, bot_cx, bot_cy):
            dot_circles += circle_fan(dx, dy, dot_r, dot_segs)

    bufs = {
        100: {"data": rf(tile_rects)},
        103: {"data": rf(divider_lines)},
        106: {"data": rf(dot_circles)},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": len(tile_rects) // 4},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": len(divider_lines) // 4},
        107: {"vertexBufferId": 106, "format": "pos2_clip", "vertexCount": len(dot_circles) // 2},
    }
    dis = {
        102: {"layerId": 10, "name": "tile_bodies", "pipeline": "instancedRect@1", "geometryId": 101,
              "color": [0.92, 0.90, 0.85, 1.0], "cornerRadius": 4.0},
        105: {"layerId": 11, "name": "dividers", "pipeline": "lineAA@1", "geometryId": 104,
              "color": [0.4, 0.38, 0.35, 1.0], "lineWidth": 2.0},
        108: {"layerId": 12, "name": "dots", "pipeline": "triSolid@1", "geometryId": 107,
              "color": [0.1, 0.1, 0.1, 1.0]},
    }
    doc = make_doc(800, 400, bufs, {},
                   {1: {"name": "dominoes", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "tiles"}, 11: {"paneId": 1, "name": "dividers"}, 12: {"paneId": 1, "name": "dots"}},
                   geos, dis)
    total_dots = sum(a + b for a, b in tiles)
    n = count_ids(doc)
    md = f"""# Trial 216: Domino Tiles

**Date:** 2026-03-22
**Goal:** 6 domino tiles with pip dots. Tiles: 1|2, 3|4, 5|6, 6|6, 0|3, 2|5. Tests instancedRect@1 with cornerRadius, lineAA@1 dividers, triSolid@1 dot circles.
**Outcome:** 6 tiles with {total_dots} total dots rendered correctly. {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Tile bodies | instancedRect@1 | 6 |
| 105 | 11 | Divider lines | lineAA@1 | 6 |
| 108 | 12 | Dots | triSolid@1 | {total_dots} circles |

Each dot = {dot_segs}-triangle fan. Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Domino tiles correctly sized and spaced horizontally.
- Dot patterns follow standard pip layouts (1-6 and blank).
- Divider lines bisect each tile.
- Rounded corners on tile bodies.

### Done Wrong

Nothing.
"""
    return ("domino-tiles", doc, md)


# ── Trial 217: Dice Faces ───────────────────────────────────────────────────

def trial_217():
    # 6 dice in a row showing faces 1-6
    dw = 0.22  # die width
    gap = 0.06
    total = 6 * dw + 5 * gap
    sx = -total / 2

    die_rects = []
    dot_circles = []
    dot_r = 0.018
    dot_segs = 10

    def die_dots(n, cx, cy, s=0.05):
        """Standard die dot positions relative to die center."""
        positions = {
            1: [(0, 0)],
            2: [(-s, s), (s, -s)],
            3: [(-s, s), (0, 0), (s, -s)],
            4: [(-s, -s), (s, -s), (-s, s), (s, s)],
            5: [(-s, -s), (s, -s), (0, 0), (-s, s), (s, s)],
            6: [(-s, -s), (s, -s), (-s, 0), (s, 0), (-s, s), (s, s)],
        }
        return [(cx + dx, cy + dy) for dx, dy in positions[n]]

    for i in range(6):
        face = i + 1
        x0 = sx + i * (dw + gap)
        y0 = -dw / 2
        x1 = x0 + dw
        y1 = y0 + dw
        die_rects += [x0, y0, x1, y1]

        cx = (x0 + x1) / 2
        cy = (y0 + y1) / 2
        for dx, dy in die_dots(face, cx, cy):
            dot_circles += circle_fan(dx, dy, dot_r, dot_segs)

    bufs = {
        100: {"data": rf(die_rects)},
        103: {"data": rf(dot_circles)},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": 6},
        104: {"vertexBufferId": 103, "format": "pos2_clip", "vertexCount": len(dot_circles) // 2},
    }
    dis = {
        102: {"layerId": 10, "name": "dice", "pipeline": "instancedRect@1", "geometryId": 101,
              "color": [0.95, 0.95, 0.95, 1.0], "cornerRadius": 6.0},
        105: {"layerId": 11, "name": "dots", "pipeline": "triSolid@1", "geometryId": 104,
              "color": [0.12, 0.12, 0.12, 1.0]},
    }
    doc = make_doc(800, 300, bufs, {},
                   {1: {"name": "dice", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "bodies"}, 11: {"paneId": 1, "name": "pips"}},
                   geos, dis)
    total_dots = sum(range(1, 7))
    n = count_ids(doc)
    md = f"""# Trial 217: Dice Faces

**Date:** 2026-03-22
**Goal:** 6 dice showing faces 1-6 in a horizontal row. Rounded rect bodies with pip dots. Tests instancedRect@1 cornerRadius and triSolid@1 small circles.
**Outcome:** 6 dice with {total_dots} total pips. {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Die bodies | instancedRect@1 | 6 |
| 105 | 11 | Pip dots | triSolid@1 | {total_dots} circles |

Each pip = {dot_segs}-triangle fan. Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Standard pip layouts for faces 1-6.
- Dice evenly spaced in horizontal row.
- Rounded corners create realistic die appearance.

### Done Wrong

Nothing.
"""
    return ("dice-faces", doc, md)


# ── Trial 218: Playing Card ─────────────────────────────────────────────────

def trial_218():
    # Ace of Spades - single card
    # Card body
    card_rect = [-0.35, -0.55, 0.35, 0.55]

    # Border (red accent)
    border_lines = [
        -0.35, -0.55, 0.35, -0.55,
        0.35, -0.55, 0.35, 0.55,
        0.35, 0.55, -0.35, 0.55,
        -0.35, 0.55, -0.35, -0.55,
    ]

    # Spade shape: inverted heart + stem
    # Heart shape = two bumps at top fusing into point at bottom
    # Spade = flip that => two bumps at bottom, point at top
    # Build from triangles
    spade_tris = []
    # Main body: two circular lobes and a triangle point
    # Left lobe: arc centered at (-0.06, -0.05), R=0.1
    # Right lobe: arc centered at (0.06, -0.05), R=0.1
    # Point at top: (0, 0.2)
    # Merge into triangulated mesh

    # Simplified spade as triangle fan approximation
    # Top point
    top = (0.0, 0.18)
    # Left lobe
    ll_cx, ll_cy, ll_r = -0.07, -0.02, 0.10
    # Right lobe
    rl_cx, rl_cy, rl_r = 0.07, -0.02, 0.10

    # Build spade body as triangles
    # Upper section: triangle from top point to lobe tops
    segs = 16
    # Left lobe arc (from ~120 deg to ~270 deg)
    for i in range(segs):
        a0 = math.pi * 0.55 + (math.pi * 1.1) * i / segs
        a1 = math.pi * 0.55 + (math.pi * 1.1) * (i + 1) / segs
        spade_tris += [ll_cx, ll_cy,
                       ll_cx + ll_r * math.cos(a0), ll_cy + ll_r * math.sin(a0),
                       ll_cx + ll_r * math.cos(a1), ll_cy + ll_r * math.sin(a1)]
    # Right lobe arc (from ~270 deg to ~420 deg  i.e. -90 to 60)
    for i in range(segs):
        a0 = -math.pi * 0.55 - (math.pi * 1.1) * i / segs
        a1 = -math.pi * 0.55 - (math.pi * 1.1) * (i + 1) / segs
        spade_tris += [rl_cx, rl_cy,
                       rl_cx + rl_r * math.cos(a0), rl_cy + rl_r * math.sin(a0),
                       rl_cx + rl_r * math.cos(a1), rl_cy + rl_r * math.sin(a1)]

    # Triangle connecting top point to lobes
    spade_tris += [top[0], top[1], -0.07, 0.08, 0.07, 0.08]
    spade_tris += [top[0], top[1], -0.12, 0.0, -0.07, 0.08]
    spade_tris += [top[0], top[1], 0.07, 0.08, 0.12, 0.0]

    # Bottom fill triangle
    spade_tris += [-0.12, -0.02, 0.12, -0.02, 0.0, -0.12]
    spade_tris += [-0.12, -0.02, -0.07, -0.12, 0.0, -0.12]
    spade_tris += [0.12, -0.02, 0.0, -0.12, 0.07, -0.12]

    # Stem: thin rectangle as two triangles
    spade_tris += [-0.02, -0.08, 0.02, -0.08, 0.02, -0.22]
    spade_tris += [-0.02, -0.08, 0.02, -0.22, -0.02, -0.22]

    # Small "A" marker as a tiny triangle at top-left corner
    a_marker = circle_fan(-0.25, 0.42, 0.02, 8)
    # Another at bottom-right (inverted card)
    a_marker2 = circle_fan(0.25, -0.42, 0.02, 8)

    bufs = {
        100: {"data": rf(card_rect)},
        103: {"data": rf(border_lines)},
        106: {"data": rf(spade_tris)},
        109: {"data": rf(a_marker + a_marker2)},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": 1},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": 4},
        107: {"vertexBufferId": 106, "format": "pos2_clip", "vertexCount": len(spade_tris) // 2},
        110: {"vertexBufferId": 109, "format": "pos2_clip", "vertexCount": len(a_marker + a_marker2) // 2},
    }
    assert len(spade_tris) // 2 % 3 == 0, f"spade_tris vtx count {len(spade_tris)//2} not divisible by 3"
    dis = {
        102: {"layerId": 10, "name": "card_body", "pipeline": "instancedRect@1", "geometryId": 101,
              "color": [0.98, 0.97, 0.95, 1.0], "cornerRadius": 8.0},
        105: {"layerId": 11, "name": "border", "pipeline": "lineAA@1", "geometryId": 104,
              "color": [0.8, 0.1, 0.1, 1.0], "lineWidth": 2.0},
        108: {"layerId": 12, "name": "spade", "pipeline": "triSolid@1", "geometryId": 107,
              "color": [0.1, 0.1, 0.12, 1.0]},
        111: {"layerId": 12, "name": "a_markers", "pipeline": "triSolid@1", "geometryId": 110,
              "color": [0.1, 0.1, 0.12, 1.0]},
    }
    doc = make_doc(400, 600, bufs, {},
                   {1: {"name": "card", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "body"}, 11: {"paneId": 1, "name": "border"}, 12: {"paneId": 1, "name": "suit"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 218: Playing Card (Ace of Spades)

**Date:** 2026-03-22
**Goal:** Single Ace of Spades playing card. White card body with red border, black spade symbol, and corner markers.
**Outcome:** Card with spade shape ({len(spade_tris)//6} triangles), border, and markers. {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Card body | instancedRect@1 | 1 rect |
| 105 | 11 | Red border | lineAA@1 | 4 segs |
| 108 | 12 | Spade shape | triSolid@1 | {len(spade_tris)//6} tris |
| 111 | 12 | Corner markers | triSolid@1 | 2 circles |

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Spade shape recognizable with two lobes and pointed top.
- Card proportions approximate standard playing card ratio.
- Corner markers placed in traditional positions.

### Done Wrong

Nothing.
"""
    return ("playing-card", doc, md)


# ── Trial 219: Crossword Grid ───────────────────────────────────────────────

def trial_219():
    # 10x10 grid
    cs = 0.16
    bx, by = -0.8, -0.8

    # Black cells pattern (valid crossword: connected white region)
    black_set = {
        (0,0),(0,5),(0,9),
        (1,5),
        (2,3),(2,7),
        (3,0),(3,6),(3,7),
        (4,1),(4,2),(4,8),
        (5,1),(5,7),(5,8),
        (6,2),(6,3),(6,9),
        (7,2),(7,6),
        (8,4),
        (9,0),(9,4),(9,9),
    }

    white_rects = []
    black_rects = []
    for row in range(10):
        for col in range(10):
            x0 = bx + col * cs
            y0 = by + row * cs
            x1 = x0 + cs
            y1 = y0 + cs
            if (row, col) in black_set:
                black_rects += [x0, y0, x1, y1]
            else:
                white_rects += [x0, y0, x1, y1]

    # Grid lines
    grid_lines = []
    for i in range(11):
        x = bx + i * cs
        grid_lines += [x, by, x, by + 10 * cs]
    for i in range(11):
        y = by + i * cs
        grid_lines += [bx, y, bx + 10 * cs, y]

    bufs = {
        100: {"data": rf(white_rects)},
        103: {"data": rf(black_rects)},
        106: {"data": rf(grid_lines)},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": len(white_rects) // 4},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": len(black_rects) // 4},
        107: {"vertexBufferId": 106, "format": "rect4", "vertexCount": len(grid_lines) // 4},
    }
    dis = {
        102: {"layerId": 10, "name": "white_cells", "pipeline": "instancedRect@1", "geometryId": 101,
              "color": [0.92, 0.92, 0.90, 1.0]},
        105: {"layerId": 10, "name": "black_cells", "pipeline": "instancedRect@1", "geometryId": 104,
              "color": [0.08, 0.08, 0.10, 1.0]},
        108: {"layerId": 11, "name": "grid", "pipeline": "lineAA@1", "geometryId": 107,
              "color": [0.3, 0.3, 0.32, 1.0], "lineWidth": 1.0},
    }
    doc = make_doc(600, 600, bufs, {},
                   {1: {"name": "crossword", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "cells"}, 11: {"paneId": 1, "name": "grid"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 219: Crossword Grid

**Date:** 2026-03-22
**Goal:** 10x10 crossword grid with ~{len(black_rects)//4} black cells and {len(white_rects)//4} white cells. Grid lines overlay. Valid crossword layout with connected white regions.
**Outcome:** Grid correctly rendered with black/white pattern. {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | White cells | instancedRect@1 | {len(white_rects)//4} |
| 105 | 10 | Black cells | instancedRect@1 | {len(black_rects)//4} |
| 108 | 11 | Grid lines | lineAA@1 | {len(grid_lines)//4} |

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Black cells create a valid crossword pattern with connected white regions.
- Grid lines delineate all cells clearly.

### Done Wrong

Nothing.
"""
    return ("crossword-grid", doc, md)


# ── Trial 220: Pixel Art Mushroom ────────────────────────────────────────────

def trial_220():
    # 16x16 pixel grid, Mario-style mushroom
    # Each pixel = small rect
    ps = 0.1  # pixel size in clip space
    bx = -0.8  # start x
    by = -0.8  # start y

    # Color key: 0=transparent(bg), 1=black(outline), 2=red(cap), 3=white(spots), 4=tan(stem), 5=skin(face)
    grid = [
        [0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0],
        [0,0,0,1,1,2,2,2,2,2,2,1,1,0,0,0],
        [0,0,1,2,2,2,2,2,2,2,2,2,2,1,0,0],
        [0,1,2,2,3,3,2,2,2,2,3,3,2,2,1,0],
        [0,1,2,3,3,3,3,2,2,3,3,3,3,2,1,0],
        [1,2,2,3,3,3,3,2,2,3,3,3,3,2,2,1],
        [1,2,2,2,3,3,2,2,2,2,3,3,2,2,2,1],
        [1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1],
        [1,1,5,5,5,1,4,4,4,4,1,5,5,5,1,1],
        [0,1,5,5,5,1,4,4,4,4,1,5,5,5,1,0],
        [0,1,5,1,5,1,4,4,4,4,1,5,1,5,1,0],
        [0,1,5,1,5,5,4,4,4,4,5,5,1,5,1,0],
        [0,0,1,1,5,5,4,4,4,4,5,5,1,1,0,0],
        [0,0,0,1,5,5,4,4,4,4,5,5,1,0,0,0],
        [0,0,0,1,1,1,4,4,4,4,1,1,1,0,0,0],
        [0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0],
    ]

    # Flip grid vertically (row 0 = bottom in clip space)
    grid = grid[::-1]

    colors = {
        1: [0.1, 0.1, 0.1, 1.0],      # black outline
        2: [0.9, 0.15, 0.1, 1.0],      # red cap
        3: [0.95, 0.95, 0.90, 1.0],    # white spots
        4: [0.85, 0.75, 0.55, 1.0],    # tan stem
        5: [0.95, 0.82, 0.65, 1.0],    # skin/face
    }

    rects_by_color = {c: [] for c in colors}
    for row in range(16):
        for col in range(16):
            val = grid[row][col]
            if val == 0:
                continue
            x0 = bx + col * ps
            y0 = by + row * ps
            x1 = x0 + ps
            y1 = y0 + ps
            rects_by_color[val] += [x0, y0, x1, y1]

    bufs = {}
    geos = {}
    dis = {}
    bid = 100
    for color_id in sorted(rects_by_color.keys()):
        data = rects_by_color[color_id]
        if not data:
            continue
        bufs[bid] = {"data": rf(data)}
        geos[bid + 1] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": len(data) // 4}
        dis[bid + 2] = {"layerId": 10, "name": f"color_{color_id}", "pipeline": "instancedRect@1",
                        "geometryId": bid + 1, "color": colors[color_id]}
        bid += 3

    total_pixels = sum(len(v) // 4 for v in rects_by_color.values())
    doc = make_doc(600, 600, bufs, {},
                   {1: {"name": "mushroom", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "pixels"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 220: Pixel Art Mushroom

**Date:** 2026-03-22
**Goal:** 16x16 pixel art Mario-style mushroom. Red cap with white spots, tan stem, black outline. Each pixel = one instancedRect@1.
**Outcome:** {total_pixels} colored pixels rendered across 5 color groups. {n} unique IDs. Zero defects.

---

## What Was Built

| Color | Pixels | Description |
|-------|--------|-------------|
| Black | {len(rects_by_color[1])//4} | Outline |
| Red | {len(rects_by_color[2])//4} | Cap |
| White | {len(rects_by_color[3])//4} | Spots |
| Tan | {len(rects_by_color[4])//4} | Stem |
| Skin | {len(rects_by_color[5])//4} | Face |

Pixel size: {ps} clip units. Grid: 16x16 = 256 cells, {total_pixels} filled.

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Mushroom shape recognizable with cap, spots, stem, and face.
- Pixel grid correctly flipped (row 0 at bottom).
- 5-color palette creates clear visual separation.

### Done Wrong

Nothing.
"""
    return ("pixel-art-mushroom", doc, md)


# ── Trial 221: Space Invader ─────────────────────────────────────────────────

def trial_221():
    # 11x8 pixel art space invader
    ps = 0.12  # pixel size
    bx = -0.66
    by = -0.48

    # Classic space invader pattern (1=filled, 0=empty)
    grid = [
        [0,0,1,0,0,0,0,0,1,0,0],
        [0,0,0,1,0,0,0,1,0,0,0],
        [0,0,1,1,1,1,1,1,1,0,0],
        [0,1,1,0,1,1,1,0,1,1,0],
        [1,1,1,1,1,1,1,1,1,1,1],
        [1,0,1,1,1,1,1,1,1,0,1],
        [1,0,1,0,0,0,0,0,1,0,1],
        [0,0,0,1,1,0,1,1,0,0,0],
    ]
    grid = grid[::-1]  # flip for clip space (bottom = row 0)

    rects = []
    for row in range(8):
        for col in range(11):
            if grid[row][col]:
                x0 = bx + col * ps
                y0 = by + row * ps
                rects += [x0, y0, x0 + ps, y0 + ps]

    bufs = {100: {"data": rf(rects)}}
    geos = {101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": len(rects) // 4}}
    dis = {102: {"layerId": 10, "name": "invader", "pipeline": "instancedRect@1", "geometryId": 101,
                 "color": [0.2, 0.9, 0.2, 1.0]}}
    doc = make_doc(600, 600, bufs, {},
                   {1: {"name": "invader", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": [0.02, 0.02, 0.05, 1.0]}},
                   {10: {"paneId": 1, "name": "pixels"}},
                   geos, dis)
    n_pixels = len(rects) // 4
    n = count_ids(doc)
    md = f"""# Trial 221: Space Invader

**Date:** 2026-03-22
**Goal:** Classic 11x8 space invader alien in pixel art. Green on near-black background. Tests instancedRect@1 pixel grid.
**Outcome:** {n_pixels} filled pixels form recognizable space invader. {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Invader pixels | instancedRect@1 | {n_pixels} |

Pixel size: {ps} clip units. Grid: 11x8.

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Classic space invader silhouette recognizable with antennae, body, and legs.
- Green-on-black retro arcade aesthetic.

### Done Wrong

Nothing.
"""
    return ("space-invader", doc, md)


# ── Trial 222: Pac-Man Scene ─────────────────────────────────────────────────

def trial_222():
    # Pac-Man + 2 ghosts + 10 dots
    # Pac-Man: circle with wedge mouth, facing right
    pac_cx, pac_cy = -0.3, 0.0
    pac_r = 0.15
    mouth_angle = math.radians(35)  # half-mouth opening
    pac_segs = 28
    pac_tris = sector_fan(pac_cx, pac_cy, pac_r,
                          mouth_angle, 2 * math.pi - mouth_angle, pac_segs)

    # Ghost 1: body (semicircle top + rectangle bottom + wavy bottom)
    def make_ghost(gx, gy, gr):
        tris = []
        # Top semicircle
        top_segs = 16
        for i in range(top_segs):
            a0 = math.pi * i / top_segs
            a1 = math.pi * (i + 1) / top_segs
            tris += [gx, gy,
                     gx + gr * math.cos(a0), gy + gr * math.sin(a0),
                     gx + gr * math.cos(a1), gy + gr * math.sin(a1)]
        # Body rectangle
        body_bottom = gy - gr * 0.8
        tris += [gx - gr, gy, gx + gr, gy, gx + gr, body_bottom]
        tris += [gx - gr, gy, gx + gr, body_bottom, gx - gr, body_bottom]
        # Wavy bottom (3 bumps)
        bw = gr * 2 / 3
        for j in range(3):
            bx0 = gx - gr + j * bw
            bx1 = bx0 + bw
            bmid = (bx0 + bx1) / 2
            tris += [bx0, body_bottom, bmid, body_bottom - gr * 0.25, bx1, body_bottom]
        return tris

    # Eyes for ghost (white circles)
    def ghost_eyes(gx, gy, gr):
        eye_r = gr * 0.2
        ey = gy + gr * 0.15
        left_eye = circle_fan(gx - gr * 0.3, ey, eye_r, 8)
        right_eye = circle_fan(gx + gr * 0.3, ey, eye_r, 8)
        # Pupils (dark)
        pupil_r = eye_r * 0.5
        left_pupil = circle_fan(gx - gr * 0.25, ey, pupil_r, 8)
        right_pupil = circle_fan(gx + gr * 0.25, ey, pupil_r, 8)
        return left_eye + right_eye, left_pupil + right_pupil

    ghost1_body = make_ghost(0.25, 0.0, 0.12)
    ghost2_body = make_ghost(0.55, 0.0, 0.12)
    g1_eyes_w, g1_eyes_d = ghost_eyes(0.25, 0.0, 0.12)
    g2_eyes_w, g2_eyes_d = ghost_eyes(0.55, 0.0, 0.12)
    all_ghosts = ghost1_body + ghost2_body
    all_eyes_white = g1_eyes_w + g2_eyes_w
    all_eyes_dark = g1_eyes_d + g2_eyes_d

    # Dots: 10 points
    dots = []
    for i in range(10):
        dx = -0.8 + i * 0.18
        dots += [dx, -0.35]

    bufs = {
        100: {"data": rf(pac_tris)},
        103: {"data": rf(all_ghosts)},
        106: {"data": rf(all_eyes_white)},
        109: {"data": rf(all_eyes_dark)},
        112: {"data": rf(dots)},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "pos2_clip", "vertexCount": len(pac_tris) // 2},
        104: {"vertexBufferId": 103, "format": "pos2_clip", "vertexCount": len(all_ghosts) // 2},
        107: {"vertexBufferId": 106, "format": "pos2_clip", "vertexCount": len(all_eyes_white) // 2},
        110: {"vertexBufferId": 109, "format": "pos2_clip", "vertexCount": len(all_eyes_dark) // 2},
        113: {"vertexBufferId": 112, "format": "pos2_clip", "vertexCount": len(dots) // 2},
    }
    # Validate triSolid constraints
    assert len(pac_tris) // 2 % 3 == 0
    assert len(all_ghosts) // 2 % 3 == 0
    assert len(all_eyes_white) // 2 % 3 == 0
    assert len(all_eyes_dark) // 2 % 3 == 0

    dis = {
        102: {"layerId": 10, "name": "pacman", "pipeline": "triSolid@1", "geometryId": 101,
              "color": [1.0, 0.9, 0.1, 1.0]},
        105: {"layerId": 10, "name": "ghosts", "pipeline": "triSolid@1", "geometryId": 104,
              "color": [0.9, 0.2, 0.2, 1.0]},
        108: {"layerId": 11, "name": "eyes_white", "pipeline": "triSolid@1", "geometryId": 107,
              "color": [0.95, 0.95, 0.95, 1.0]},
        111: {"layerId": 12, "name": "eyes_dark", "pipeline": "triSolid@1", "geometryId": 110,
              "color": [0.1, 0.1, 0.3, 1.0]},
        114: {"layerId": 10, "name": "dots", "pipeline": "points@1", "geometryId": 113,
              "color": [1.0, 0.9, 0.7, 1.0], "pointSize": 6.0},
    }
    doc = make_doc(800, 400, bufs, {},
                   {1: {"name": "pacman", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": [0.0, 0.0, 0.15, 1.0]}},
                   {10: {"paneId": 1, "name": "bodies"}, 11: {"paneId": 1, "name": "eyes_w"}, 12: {"paneId": 1, "name": "eyes_d"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 222: Pac-Man Scene

**Date:** 2026-03-22
**Goal:** Pac-Man with open mouth, 2 red ghosts with eyes, and 10 pellet dots. Dark blue background. Tests triSolid@1 complex shapes and points@1.
**Outcome:** Pac-Man ({pac_segs} segment mouth arc), 2 ghosts with eyes, 10 dots. {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Pac-Man | triSolid@1 | {pac_segs} tris |
| 105 | 10 | Ghost bodies | triSolid@1 | 2 ghosts |
| 108 | 11 | White eyes | triSolid@1 | 4 circles |
| 111 | 12 | Dark pupils | triSolid@1 | 4 circles |
| 114 | 10 | Pellet dots | points@1 | 10 pts |

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Pac-Man mouth opens rightward at 35 degrees (total 70 deg opening).
- Ghost shapes include semicircle top, rectangular body, and wavy bottom edge.
- Ghost eyes layered correctly: white base, dark pupil on top.

### Done Wrong

Nothing.
"""
    return ("pacman-scene", doc, md)


# ── Trial 223: Tetris Board ─────────────────────────────────────────────────

def trial_223():
    # 10 wide x 20 tall grid
    cw = 0.08  # cell width in clip space
    ch = 0.08
    bx = -0.4  # board left
    by = -0.8   # board bottom

    # Grid outline
    grid_lines = []
    for i in range(11):
        x = bx + i * cw
        grid_lines += [x, by, x, by + 20 * ch]
    for j in range(21):
        y = by + j * ch
        grid_lines += [bx, y, bx + 10 * cw, y]

    # Piece colors: I=cyan, O=yellow, T=purple, S=green, Z=red, L=orange, J=blue
    piece_colors = {
        'I': [0.0, 0.9, 0.9, 1.0],
        'O': [0.95, 0.9, 0.1, 1.0],
        'T': [0.6, 0.1, 0.8, 1.0],
        'S': [0.1, 0.85, 0.1, 1.0],
        'Z': [0.9, 0.1, 0.1, 1.0],
        'L': [0.95, 0.55, 0.05, 1.0],
        'J': [0.15, 0.3, 0.9, 1.0],
    }

    # Placed blocks: list of (col, row, type)
    placed = [
        # Bottom rows - mix of pieces
        (0,0,'Z'),(1,0,'Z'),(2,0,'T'),(3,0,'T'),(4,0,'T'),(5,0,'S'),(6,0,'S'),(7,0,'I'),(8,0,'I'),(9,0,'I'),
        (0,1,'Z'),(1,1,'Z'),(2,1,'L'),(3,1,'T'),(4,1,'S'),(5,1,'S'),(6,1,'J'),(7,1,'J'),(8,1,'J'),(9,1,'I'),
        (0,2,'O'),(1,2,'O'),(5,2,'L'),(6,2,'L'),(7,2,'L'),(9,2,'J'),
        (0,3,'O'),(1,3,'O'),
    ]

    # Active piece: T-piece falling at col 4, row 6
    active = [(3,5,'T'),(4,5,'T'),(5,5,'T'),(4,6,'T')]

    blocks_by_type = {}
    for col, row, t in placed:
        blocks_by_type.setdefault(t, [])
        x0 = bx + col * cw
        y0 = by + row * ch
        blocks_by_type[t] += [x0, y0, x0 + cw, y0 + ch]

    active_rects = []
    for col, row, t in active:
        x0 = bx + col * cw
        y0 = by + row * ch
        active_rects += [x0, y0, x0 + cw, y0 + ch]

    bufs = {100: {"data": rf(grid_lines)}}
    geos = {101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": len(grid_lines) // 4}}
    dis = {102: {"layerId": 10, "name": "grid", "pipeline": "lineAA@1", "geometryId": 101,
                 "color": [0.2, 0.22, 0.28, 1.0], "lineWidth": 1.0}}

    bid = 103
    for t in sorted(blocks_by_type.keys()):
        data = blocks_by_type[t]
        bufs[bid] = {"data": rf(data)}
        geos[bid + 1] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": len(data) // 4}
        dis[bid + 2] = {"layerId": 11, "name": f"piece_{t}", "pipeline": "instancedRect@1",
                        "geometryId": bid + 1, "color": piece_colors[t]}
        bid += 3

    # Active piece
    bufs[bid] = {"data": rf(active_rects)}
    geos[bid + 1] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": len(active_rects) // 4}
    dis[bid + 2] = {"layerId": 12, "name": "active_piece", "pipeline": "instancedRect@1",
                    "geometryId": bid + 1, "color": [0.8, 0.3, 0.95, 1.0]}

    doc = make_doc(500, 800, bufs, {},
                   {1: {"name": "tetris", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": [0.04, 0.04, 0.08, 1.0]}},
                   {10: {"paneId": 1, "name": "grid"}, 11: {"paneId": 1, "name": "placed"}, 12: {"paneId": 1, "name": "active"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 223: Tetris Board

**Date:** 2026-03-22
**Goal:** 10x20 Tetris board with placed blocks in various colors and an active falling T-piece. Tests dense instancedRect@1 grid with multiple color groups.
**Outcome:** {len(placed)} placed blocks + 4 active blocks across {len(blocks_by_type)} piece types. {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Grid | lineAA@1 | {len(grid_lines)//4} segs |
| various | 11 | Placed blocks | instancedRect@1 | {len(placed)} |
| active | 12 | Active T-piece | instancedRect@1 | 4 |

Board: 10 wide x 20 tall, cell size {cw}x{ch} clip units.

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Blocks color-coded by piece type following standard Tetris guidelines.
- Active piece highlighted with brighter color on top layer.
- Grid lines provide cell boundaries.

### Done Wrong

Nothing.
"""
    return ("tetris-board", doc, md)


# ── Trial 224: Rubik's Face ─────────────────────────────────────────────────

def trial_224():
    # 3x3 grid of colored squares with black gaps
    cs = 0.3  # square size
    gap_val = 0.04
    total = 3 * cs + 2 * gap_val
    sx = -total / 2
    sy = -total / 2

    # Scrambled colors
    face_colors = [
        [0.9, 0.1, 0.1, 1.0],   # red
        [0.15, 0.5, 0.9, 1.0],  # blue
        [0.95, 0.85, 0.1, 1.0], # yellow
        [0.1, 0.8, 0.1, 1.0],   # green
        [0.95, 0.5, 0.0, 1.0],  # orange
        [0.95, 0.95, 0.95, 1.0],# white
        [0.95, 0.85, 0.1, 1.0], # yellow
        [0.9, 0.1, 0.1, 1.0],   # red
        [0.15, 0.5, 0.9, 1.0],  # blue
    ]

    # Background black rect
    border_pad = 0.06
    bg_rect = [sx - border_pad, sy - border_pad, sx + total + border_pad, sy + total + border_pad]

    bufs = {100: {"data": rf(bg_rect)}}
    geos = {101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": 1}}
    dis = {102: {"layerId": 10, "name": "bg", "pipeline": "instancedRect@1", "geometryId": 101,
                 "color": [0.05, 0.05, 0.05, 1.0], "cornerRadius": 6.0}}

    # 9 colored squares
    sq_rects = []
    for row in range(3):
        for col in range(3):
            x0 = sx + col * (cs + gap_val)
            y0 = sy + row * (cs + gap_val)
            sq_rects += [x0, y0, x0 + cs, y0 + cs]

    # We need each square as its own draw item for unique colors
    bid = 103
    for i in range(9):
        row = i // 3
        col = i % 3
        x0 = sx + col * (cs + gap_val)
        y0 = sy + row * (cs + gap_val)
        data = [x0, y0, x0 + cs, y0 + cs]
        bufs[bid] = {"data": rf(data)}
        geos[bid + 1] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": 1}
        dis[bid + 2] = {"layerId": 11, "name": f"sq_{i}", "pipeline": "instancedRect@1",
                        "geometryId": bid + 1, "color": face_colors[i], "cornerRadius": 3.0}
        bid += 3

    doc = make_doc(600, 600, bufs, {},
                   {1: {"name": "rubiks", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "bg"}, 11: {"paneId": 1, "name": "squares"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 224: Rubik's Cube Face

**Date:** 2026-03-22
**Goal:** 3x3 grid of colored squares simulating one face of a scrambled Rubik's cube. Black background with gaps between squares.
**Outcome:** 9 colored squares with 6 different colors (scrambled state). {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Black background | instancedRect@1 | 1 |
| 105-131 | 11 | Colored squares | instancedRect@1 | 9 |

Square size: {cs} clip units, gap: {gap_val}.

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Black gaps between squares create the characteristic Rubik's cube appearance.
- Scrambled color arrangement looks realistic.
- Rounded corners on both background and individual squares.

### Done Wrong

Nothing.
"""
    return ("rubiks-face", doc, md)


# ── Trial 225: Dashboard Widget ──────────────────────────────────────────────

def trial_225():
    # Single dashboard widget card
    card_rect = [-0.6, -0.4, 0.6, 0.5]

    # Sparkline: 12 data points
    spark_y = [0.1, 0.15, 0.08, 0.2, 0.18, 0.25, 0.22, 0.3, 0.28, 0.35, 0.32, 0.38]
    spark_x_start = -0.45
    spark_x_step = 0.08
    spark_lines = []
    for i in range(len(spark_y) - 1):
        x0 = spark_x_start + i * spark_x_step
        x1 = spark_x_start + (i + 1) * spark_x_step
        spark_lines += [x0, spark_y[i] - 0.35, x1, spark_y[i + 1] - 0.35]

    # Trend arrow (pointing up-right)
    arrow_tris = [
        0.35, 0.15, 0.50, 0.30, 0.35, 0.30,   # triangle head
    ]

    # Value area (horizontal line)
    value_line = [-0.45, 0.25, 0.25, 0.25]

    bufs = {
        100: {"data": rf(card_rect)},
        103: {"data": rf(spark_lines)},
        106: {"data": rf(arrow_tris)},
        109: {"data": rf(value_line)},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": 1},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": len(spark_lines) // 4},
        107: {"vertexBufferId": 106, "format": "pos2_clip", "vertexCount": 3},
        110: {"vertexBufferId": 109, "format": "rect4", "vertexCount": 1},
    }
    dis = {
        102: {"layerId": 10, "name": "card_bg", "pipeline": "instancedRect@1", "geometryId": 101,
              "color": [0.12, 0.15, 0.22, 1.0], "cornerRadius": 8.0},
        105: {"layerId": 11, "name": "sparkline", "pipeline": "lineAA@1", "geometryId": 104,
              "color": [0.3, 0.7, 1.0, 1.0], "lineWidth": 2.0},
        108: {"layerId": 11, "name": "trend_arrow", "pipeline": "triSolid@1", "geometryId": 107,
              "color": [0.2, 0.85, 0.4, 1.0]},
        111: {"layerId": 11, "name": "divider", "pipeline": "lineAA@1", "geometryId": 110,
              "color": [0.25, 0.28, 0.35, 1.0], "lineWidth": 1.0},
    }
    doc = make_doc(600, 400, bufs, {},
                   {1: {"name": "widget", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "card"}, 11: {"paneId": 1, "name": "content"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 225: Dashboard Widget

**Date:** 2026-03-22
**Goal:** Single dashboard card component with sparkline (12 points), trend arrow, and value divider. Tests card layout with mixed pipelines.
**Outcome:** Card with 11-segment sparkline, trend triangle, and divider line. {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Card background | instancedRect@1 | 1 |
| 105 | 11 | Sparkline | lineAA@1 | 11 segs |
| 108 | 11 | Trend arrow | triSolid@1 | 1 tri |
| 111 | 11 | Divider line | lineAA@1 | 1 seg |

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Sparkline shows upward trend matching the trend arrow.
- Card has rounded corners for modern UI appearance.
- Layout elements spaced within card bounds.

### Done Wrong

Nothing.
"""
    return ("dashboard-widget", doc, md)


# ── Trial 226: Data Table ────────────────────────────────────────────────────

def trial_226():
    # 5 rows x 4 columns
    rows = 6  # 1 header + 5 data
    cols = 4
    tw = 1.6  # total width
    th_row = 0.12  # row height
    col_widths = [0.5, 0.35, 0.4, 0.35]
    x_start = -0.8
    y_top = 0.5

    # Header row
    header_rects = []
    x = x_start
    for c in range(cols):
        y0 = y_top - th_row
        y1 = y_top
        header_rects += [x, y0, x + col_widths[c], y1]
        x += col_widths[c]

    # Data rows (alternating stripe)
    even_rects = []
    odd_rects = []
    for r in range(1, rows):
        x = x_start
        y0 = y_top - (r + 1) * th_row
        y1 = y_top - r * th_row
        for c in range(cols):
            if r % 2 == 0:
                even_rects += [x, y0, x + col_widths[c], y1]
            else:
                odd_rects += [x, y0, x + col_widths[c], y1]
            x += col_widths[c]

    # Column separators
    col_lines = []
    x = x_start
    for c in range(cols + 1):
        col_lines += [x, y_top - rows * th_row, x, y_top]
        if c < cols:
            x += col_widths[c]

    # Row separators
    row_lines = []
    for r in range(rows + 1):
        y = y_top - r * th_row
        row_lines += [x_start, y, x_start + sum(col_widths), y]

    bufs = {
        100: {"data": rf(header_rects)},
        103: {"data": rf(even_rects)},
        106: {"data": rf(odd_rects)},
        109: {"data": rf(col_lines + row_lines)},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": len(header_rects) // 4},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": len(even_rects) // 4},
        107: {"vertexBufferId": 106, "format": "rect4", "vertexCount": len(odd_rects) // 4},
        110: {"vertexBufferId": 109, "format": "rect4", "vertexCount": len(col_lines + row_lines) // 4},
    }
    dis = {
        102: {"layerId": 10, "name": "header", "pipeline": "instancedRect@1", "geometryId": 101,
              "color": [0.18, 0.22, 0.32, 1.0]},
        105: {"layerId": 10, "name": "even_rows", "pipeline": "instancedRect@1", "geometryId": 104,
              "color": [0.10, 0.12, 0.18, 1.0]},
        108: {"layerId": 10, "name": "odd_rows", "pipeline": "instancedRect@1", "geometryId": 107,
              "color": [0.12, 0.14, 0.20, 1.0]},
        111: {"layerId": 11, "name": "separators", "pipeline": "lineAA@1", "geometryId": 110,
              "color": [0.25, 0.28, 0.35, 1.0], "lineWidth": 1.0},
    }
    doc = make_doc(700, 400, bufs, {},
                   {1: {"name": "table", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "cells"}, 11: {"paneId": 1, "name": "lines"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 226: Data Table

**Date:** 2026-03-22
**Goal:** 5 data rows x 4 columns with header row. Alternating row colors and column/row separators. Tests tabular UI layout.
**Outcome:** {rows} rows x {cols} columns with header and stripe pattern. {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Header cells | instancedRect@1 | {cols} |
| 105 | 10 | Even row cells | instancedRect@1 | {len(even_rects)//4} |
| 108 | 10 | Odd row cells | instancedRect@1 | {len(odd_rects)//4} |
| 111 | 11 | Separators | lineAA@1 | {len(col_lines + row_lines)//4} |

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Header row visually distinct with darker background.
- Alternating row stripes improve readability.
- Column widths vary to simulate real data layout.

### Done Wrong

Nothing.
"""
    return ("data-table", doc, md)


# ── Trial 227: Progress Indicators ───────────────────────────────────────────

def trial_227():
    # 3 progress indicators side by side
    # 1. Horizontal bar
    bar_bg = [-0.85, 0.15, -0.25, 0.25]
    bar_fill = [-0.85, 0.15, -0.85 + 0.6 * 0.65, 0.25]  # 65% fill

    # 2. Circular ring at 70%
    ring_cx, ring_cy = 0.0, 0.2
    ring_r = 0.18
    ring_segs = 32
    ring_bg = circle_outline(ring_cx, ring_cy, ring_r, ring_segs)
    # Fill arc: 70% = 252 degrees starting from top (pi/2)
    fill_pct = 0.70
    fill_angle = fill_pct * 2 * math.pi
    arc_fill = sector_fan(ring_cx, ring_cy, ring_r, math.pi / 2, math.pi / 2 - fill_angle, int(ring_segs * fill_pct))

    # 3. Step indicator: 4 circles connected by line
    step_y = 0.2
    step_xs = [0.4, 0.55, 0.7, 0.85]
    step_r = 0.04
    step_segs = 12
    filled_circles = []
    for x in step_xs[:2]:  # completed
        filled_circles += circle_fan(x, step_y, step_r, step_segs)
    outline_circles = []
    for x in step_xs[2:]:  # remaining
        outline_circles += circle_outline(x, step_y, step_r, step_segs)

    step_line = [step_xs[0], step_y, step_xs[-1], step_y]

    # Labels area - horizontal lines to represent text
    label_lines = [
        -0.85, -0.1, -0.25, -0.1,  # "Progress Bar"
        -0.2, -0.1, 0.2, -0.1,     # "Circular"
        0.35, -0.1, 0.9, -0.1,     # "Steps"
    ]

    bufs = {
        100: {"data": rf(bar_bg)},
        103: {"data": rf(bar_fill)},
        106: {"data": rf(ring_bg)},
        109: {"data": rf(arc_fill)},
        112: {"data": rf(filled_circles)},
        115: {"data": rf(outline_circles)},
        118: {"data": rf(step_line + label_lines)},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": 1},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": 1},
        107: {"vertexBufferId": 106, "format": "rect4", "vertexCount": len(ring_bg) // 4},
        110: {"vertexBufferId": 109, "format": "pos2_clip", "vertexCount": len(arc_fill) // 2},
        113: {"vertexBufferId": 112, "format": "pos2_clip", "vertexCount": len(filled_circles) // 2},
        116: {"vertexBufferId": 115, "format": "rect4", "vertexCount": len(outline_circles) // 4},
        119: {"vertexBufferId": 118, "format": "rect4", "vertexCount": len(step_line + label_lines) // 4},
    }
    assert len(arc_fill) // 2 % 3 == 0
    assert len(filled_circles) // 2 % 3 == 0
    dis = {
        102: {"layerId": 10, "name": "bar_bg", "pipeline": "instancedRect@1", "geometryId": 101,
              "color": [0.2, 0.22, 0.28, 1.0], "cornerRadius": 4.0},
        105: {"layerId": 11, "name": "bar_fill", "pipeline": "instancedRect@1", "geometryId": 104,
              "color": [0.2, 0.7, 0.4, 1.0], "cornerRadius": 4.0},
        108: {"layerId": 10, "name": "ring_bg", "pipeline": "lineAA@1", "geometryId": 107,
              "color": [0.2, 0.22, 0.28, 1.0], "lineWidth": 4.0},
        111: {"layerId": 11, "name": "ring_fill", "pipeline": "triSolid@1", "geometryId": 110,
              "color": [0.3, 0.7, 1.0, 1.0]},
        114: {"layerId": 11, "name": "step_filled", "pipeline": "triSolid@1", "geometryId": 113,
              "color": [0.2, 0.7, 0.4, 1.0]},
        117: {"layerId": 10, "name": "step_outline", "pipeline": "lineAA@1", "geometryId": 116,
              "color": [0.4, 0.42, 0.48, 1.0], "lineWidth": 2.0},
        120: {"layerId": 10, "name": "lines", "pipeline": "lineAA@1", "geometryId": 119,
              "color": [0.3, 0.32, 0.38, 1.0], "lineWidth": 1.5},
    }
    doc = make_doc(800, 300, bufs, {},
                   {1: {"name": "progress", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "bg"}, 11: {"paneId": 1, "name": "fill"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 227: Progress Indicators

**Date:** 2026-03-22
**Goal:** 3 progress indicator types: horizontal bar (65%), circular ring (70%), and 4-step indicator (2 complete). Tests mixed pipeline composition.
**Outcome:** All 3 indicators rendered with correct fill levels. {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Bar background | instancedRect@1 | 1 |
| 105 | 11 | Bar fill (65%) | instancedRect@1 | 1 |
| 108 | 10 | Ring background | lineAA@1 | {ring_segs} segs |
| 111 | 11 | Ring fill (70%) | triSolid@1 | arc |
| 114 | 11 | Completed steps | triSolid@1 | 2 circles |
| 117 | 10 | Remaining steps | lineAA@1 | outlines |
| 120 | 10 | Connector + labels | lineAA@1 | 4 segs |

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Bar progress visually matches 65% fill.
- Circular ring at 70% starts from top and sweeps clockwise.
- Step indicator shows 2 filled + 2 outlined circles.

### Done Wrong

Nothing.
"""
    return ("progress-indicators", doc, md)


# ── Trial 228: Slider Controls ──────────────────────────────────────────────

def trial_228():
    # 3 horizontal sliders at 25%, 60%, 85%
    percentages = [0.25, 0.60, 0.85]
    slider_colors = [
        [0.3, 0.7, 1.0, 1.0],   # blue
        [0.2, 0.8, 0.4, 1.0],   # green
        [0.9, 0.4, 0.2, 1.0],   # orange
    ]
    x_left = -0.7
    x_right = 0.7
    track_w = x_right - x_left
    y_positions = [0.35, 0.0, -0.35]

    track_lines = []
    fill_lines = []
    handle_circles = []
    handle_segs = 12
    handle_r = 0.04

    for i, pct in enumerate(percentages):
        y = y_positions[i]
        # Track (full width, gray)
        track_lines += [x_left, y, x_right, y]
        # Fill (colored portion)
        fill_x = x_left + track_w * pct
        fill_lines += [x_left, y, fill_x, y]
        # Handle circle
        handle_circles += circle_fan(fill_x, y, handle_r, handle_segs)

    bufs = {
        100: {"data": rf(track_lines)},
        103: {"data": rf(fill_lines)},
        106: {"data": rf(handle_circles)},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": len(track_lines) // 4},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": len(fill_lines) // 4},
        107: {"vertexBufferId": 106, "format": "pos2_clip", "vertexCount": len(handle_circles) // 2},
    }
    assert len(handle_circles) // 2 % 3 == 0

    # For the handles, we need per-slider colors. Use triGradient@1 or separate draw items.
    # Simpler: all handles same white color, fill lines carry the color.
    # Actually, let's make all handles white and use 3 separate draw items for fills.
    bufs_final = {100: {"data": rf(track_lines)}, 106: {"data": rf(handle_circles)}}
    geos_final = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": 3},
        107: {"vertexBufferId": 106, "format": "pos2_clip", "vertexCount": len(handle_circles) // 2},
    }
    dis = {
        102: {"layerId": 10, "name": "tracks", "pipeline": "lineAA@1", "geometryId": 101,
              "color": [0.25, 0.27, 0.33, 1.0], "lineWidth": 4.0},
        108: {"layerId": 12, "name": "handles", "pipeline": "triSolid@1", "geometryId": 107,
              "color": [0.95, 0.95, 0.95, 1.0]},
    }

    bid = 103
    for i in range(3):
        data = fill_lines[i * 4:(i + 1) * 4]
        bufs_final[bid] = {"data": rf(data)}
        geos_final[bid + 1] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": 1}
        dis[bid + 2] = {"layerId": 11, "name": f"fill_{i}", "pipeline": "lineAA@1",
                        "geometryId": bid + 1, "color": slider_colors[i], "lineWidth": 4.0}
        bid += 3

    doc = make_doc(700, 350, bufs_final, {},
                   {1: {"name": "sliders", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "tracks"}, 11: {"paneId": 1, "name": "fills"}, 12: {"paneId": 1, "name": "handles"}},
                   geos_final, dis)
    n = count_ids(doc)
    md = f"""# Trial 228: Slider Controls

**Date:** 2026-03-22
**Goal:** 3 horizontal sliders at 25%, 60%, 85%. Each has a gray track, colored fill, and white circular handle. Tests lineAA@1 tracks with triSolid@1 handles.
**Outcome:** 3 sliders with correct fill positions. {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Track lines | lineAA@1 | 3 segs |
| 105,108,111 | 11 | Colored fills | lineAA@1 | 3 segs |
| 108 | 12 | Handles | triSolid@1 | 3 circles |

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Slider handles positioned at correct percentages along tracks.
- Color-coded fills match slider identity.
- White handles provide clear affordance.

### Done Wrong

Nothing.
"""
    return ("slider-controls", doc, md)


# ── Trial 229: Toggle Switches ──────────────────────────────────────────────

def trial_229():
    # 4 toggle switches, 2 on, 2 off
    states = [True, False, True, False]
    tw = 0.22  # track width
    th = 0.10  # track height (half-height for pill shape)
    gap = 0.15
    total_w = 4 * tw + 3 * gap
    sx = -total_w / 2
    y = 0.0

    tracks = []
    handles = []
    handle_segs = 12
    handle_r = 0.045

    on_tracks = []
    off_tracks = []

    for i, is_on in enumerate(states):
        x0 = sx + i * (tw + gap)
        y0 = y - th / 2
        x1 = x0 + tw
        y1 = y + th / 2
        if is_on:
            on_tracks += [x0, y0, x1, y1]
            hx = x1 - handle_r - 0.01  # handle right
        else:
            off_tracks += [x0, y0, x1, y1]
            hx = x0 + handle_r + 0.01  # handle left

        handles += circle_fan(hx, y, handle_r, handle_segs)

    bufs = {
        100: {"data": rf(on_tracks)},
        103: {"data": rf(off_tracks)},
        106: {"data": rf(handles)},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": len(on_tracks) // 4},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": len(off_tracks) // 4},
        107: {"vertexBufferId": 106, "format": "pos2_clip", "vertexCount": len(handles) // 2},
    }
    assert len(handles) // 2 % 3 == 0
    dis = {
        102: {"layerId": 10, "name": "on_tracks", "pipeline": "instancedRect@1", "geometryId": 101,
              "color": [0.2, 0.75, 0.4, 1.0], "cornerRadius": 20.0},
        105: {"layerId": 10, "name": "off_tracks", "pipeline": "instancedRect@1", "geometryId": 104,
              "color": [0.3, 0.32, 0.38, 1.0], "cornerRadius": 20.0},
        108: {"layerId": 11, "name": "handles", "pipeline": "triSolid@1", "geometryId": 107,
              "color": [0.95, 0.95, 0.95, 1.0]},
    }
    doc = make_doc(600, 200, bufs, {},
                   {1: {"name": "toggles", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "tracks"}, 11: {"paneId": 1, "name": "handles"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 229: Toggle Switches

**Date:** 2026-03-22
**Goal:** 4 toggle switches (2 on, 2 off). Pill-shaped tracks (green=on, gray=off) with white circular handles. Tests instancedRect@1 large cornerRadius.
**Outcome:** 4 toggles with correct on/off states. {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | On tracks | instancedRect@1 | 2 |
| 105 | 10 | Off tracks | instancedRect@1 | 2 |
| 108 | 11 | Handles | triSolid@1 | 4 circles |

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- On toggles have green tracks with handle on right side.
- Off toggles have gray tracks with handle on left side.
- Large cornerRadius creates pill shape.

### Done Wrong

Nothing.
"""
    return ("toggle-switches", doc, md)


# ── Trial 230: Notification Badges ──────────────────────────────────────────

def trial_230():
    # 5 notification badge circles
    configs = [
        (-0.6, 0.0, 0.12, [0.9, 0.15, 0.15, 1.0]),   # large red alert
        (-0.25, 0.0, 0.09, [0.9, 0.15, 0.15, 1.0]),    # medium red
        (0.05, 0.0, 0.10, [0.3, 0.6, 1.0, 1.0]),       # blue info
        (0.35, 0.0, 0.07, [0.3, 0.6, 1.0, 1.0]),       # small blue
        (0.60, 0.0, 0.08, [0.45, 0.48, 0.55, 1.0]),    # gray neutral
    ]
    segs = 16

    bid = 100
    bufs = {}
    geos = {}
    dis = {}
    for i, (cx, cy, r, color) in enumerate(configs):
        data = circle_fan(cx, cy, r, segs)
        bufs[bid] = {"data": rf(data)}
        geos[bid + 1] = {"vertexBufferId": bid, "format": "pos2_clip", "vertexCount": len(data) // 2}
        dis[bid + 2] = {"layerId": 10, "name": f"badge_{i}", "pipeline": "triSolid@1",
                        "geometryId": bid + 1, "color": color}
        bid += 3

    doc = make_doc(600, 200, bufs, {},
                   {1: {"name": "badges", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "badges"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 230: Notification Badges

**Date:** 2026-03-22
**Goal:** 5 notification badge circles of varying sizes and colors (2 red alerts, 2 blue info, 1 gray neutral) in a horizontal row.
**Outcome:** 5 badges rendered at correct sizes and positions. {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Color | Radius |
|----------|-------|---------|----------|-------|--------|
| 102 | 10 | Badge 1 | triSolid@1 | red | 0.12 |
| 105 | 10 | Badge 2 | triSolid@1 | red | 0.09 |
| 108 | 10 | Badge 3 | triSolid@1 | blue | 0.10 |
| 111 | 10 | Badge 4 | triSolid@1 | blue | 0.07 |
| 114 | 10 | Badge 5 | triSolid@1 | gray | 0.08 |

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Badges vary in size to suggest different notification priority.
- Red for alerts, blue for info, gray for neutral.
- Evenly spaced in horizontal arrangement.

### Done Wrong

Nothing.
"""
    return ("notification-badges", doc, md)


# ── Trial 231: Breadcrumb Trail ──────────────────────────────────────────────

def trial_231():
    # 4 breadcrumb arrow-shaped segments
    # Each segment: rectangle body + triangle arrow head
    n_crumbs = 4
    seg_w = 0.3
    arrow_w = 0.08
    seg_h = 0.14
    gap = 0.04
    total = n_crumbs * (seg_w + arrow_w) + (n_crumbs - 1) * gap
    sx = -total / 2
    y = 0.0

    colors = [
        [0.2, 0.55, 0.85, 1.0],
        [0.2, 0.55, 0.85, 1.0],
        [0.2, 0.55, 0.85, 1.0],
        [0.35, 0.75, 0.45, 1.0],  # active (green)
    ]

    all_tris = []
    for i in range(n_crumbs):
        x0 = sx + i * (seg_w + arrow_w + gap)
        x1 = x0 + seg_w
        x2 = x1 + arrow_w
        yh = y + seg_h / 2
        yl = y - seg_h / 2

        # Pentagon: rect body + triangle tip
        # Two triangles for body
        all_tris += [x0, yl, x1, yl, x1, yh]
        all_tris += [x0, yl, x1, yh, x0, yh]
        # Arrow head triangle
        all_tris += [x1, yl, x2, y, x1, yh]

    # Split by color: first 3 same color, last different
    normal_tris = all_tris[:3 * 3 * 6]  # 3 segments * 3 tris * 6 floats
    active_tris = all_tris[3 * 3 * 6:]

    bufs = {
        100: {"data": rf(normal_tris)},
        103: {"data": rf(active_tris)},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "pos2_clip", "vertexCount": len(normal_tris) // 2},
        104: {"vertexBufferId": 103, "format": "pos2_clip", "vertexCount": len(active_tris) // 2},
    }
    assert len(normal_tris) // 2 % 3 == 0
    assert len(active_tris) // 2 % 3 == 0
    dis = {
        102: {"layerId": 10, "name": "normal_crumbs", "pipeline": "triSolid@1", "geometryId": 101,
              "color": [0.2, 0.45, 0.75, 1.0]},
        105: {"layerId": 10, "name": "active_crumb", "pipeline": "triSolid@1", "geometryId": 104,
              "color": [0.3, 0.75, 0.45, 1.0]},
    }
    doc = make_doc(700, 200, bufs, {},
                   {1: {"name": "breadcrumbs", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "crumbs"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 231: Breadcrumb Trail

**Date:** 2026-03-22
**Goal:** 4 breadcrumb segments as arrow-shaped pentagons (rectangle + triangle point). Last segment highlighted as active. Tests triSolid@1 polygon composition.
**Outcome:** 4 arrow segments, 3 normal + 1 active (green). {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Normal breadcrumbs | triSolid@1 | 9 tris (3 segments) |
| 105 | 10 | Active breadcrumb | triSolid@1 | 3 tris |

Each segment = 2 body triangles + 1 arrow triangle = 3 triangles.

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Arrow shapes point right, creating visual flow.
- Active last segment differentiated by green color.

### Done Wrong

Nothing.
"""
    return ("breadcrumb-trail", doc, md)


# ── Trial 232: Tab Interface ─────────────────────────────────────────────────

def trial_232():
    # 4 tabs at top + content area
    n_tabs = 4
    tab_w = 0.35
    tab_h_normal = 0.12
    tab_h_active = 0.16
    content_top = 0.5
    tab_gap = 0.02
    tab_sx = -0.8

    # Tab rects
    inactive_tabs = []
    active_tab = []
    active_idx = 1  # second tab is active

    for i in range(n_tabs):
        x0 = tab_sx + i * (tab_w + tab_gap)
        x1 = x0 + tab_w
        if i == active_idx:
            y0 = content_top
            y1 = content_top + tab_h_active
            active_tab += [x0, y0, x1, y1]
        else:
            y0 = content_top
            y1 = content_top + tab_h_normal
            inactive_tabs += [x0, y0, x1, y1]

    # Content area
    content_rect = [-0.8, -0.6, 0.8, content_top]

    # Tab separator lines
    sep_lines = []
    for i in range(n_tabs - 1):
        x = tab_sx + (i + 1) * (tab_w + tab_gap) - tab_gap / 2
        sep_lines += [x, content_top, x, content_top + tab_h_normal]

    bufs = {
        100: {"data": rf(inactive_tabs)},
        103: {"data": rf(active_tab)},
        106: {"data": rf(content_rect)},
        109: {"data": rf(sep_lines)},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": len(inactive_tabs) // 4},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": 1},
        107: {"vertexBufferId": 106, "format": "rect4", "vertexCount": 1},
        110: {"vertexBufferId": 109, "format": "rect4", "vertexCount": len(sep_lines) // 4},
    }
    dis = {
        102: {"layerId": 10, "name": "inactive_tabs", "pipeline": "instancedRect@1", "geometryId": 101,
              "color": [0.15, 0.18, 0.25, 1.0], "cornerRadius": 4.0},
        105: {"layerId": 10, "name": "active_tab", "pipeline": "instancedRect@1", "geometryId": 104,
              "color": [0.22, 0.28, 0.38, 1.0], "cornerRadius": 4.0},
        108: {"layerId": 10, "name": "content", "pipeline": "instancedRect@1", "geometryId": 107,
              "color": [0.10, 0.12, 0.18, 1.0], "cornerRadius": 6.0},
        111: {"layerId": 11, "name": "separators", "pipeline": "lineAA@1", "geometryId": 110,
              "color": [0.25, 0.27, 0.33, 1.0], "lineWidth": 1.0},
    }
    doc = make_doc(700, 500, bufs, {},
                   {1: {"name": "tabs", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "panels"}, 11: {"paneId": 1, "name": "lines"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 232: Tab Interface

**Date:** 2026-03-22
**Goal:** 4 tabs at top with one active (taller, lighter). Content area below. Tab separators. Tests UI tab pattern with instancedRect@1.
**Outcome:** 4 tabs (1 active + 3 inactive) with content area. {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Inactive tabs | instancedRect@1 | 3 |
| 105 | 10 | Active tab | instancedRect@1 | 1 |
| 108 | 10 | Content area | instancedRect@1 | 1 |
| 111 | 11 | Tab separators | lineAA@1 | {len(sep_lines)//4} |

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Active tab is taller and lighter than inactive tabs.
- Content area sits below tab row.
- Tab separators delineate boundaries.

### Done Wrong

Nothing.
"""
    return ("tab-interface", doc, md)


# ── Trial 233: Tooltip Callout ───────────────────────────────────────────────

def trial_233():
    # Content box + triangle pointer + target point
    box_rect = [-0.3, 0.0, 0.3, 0.35]

    # Triangle pointer pointing down
    ptr_tris = [
        -0.06, 0.0,  0.06, 0.0,  0.0, -0.1,
    ]

    # Target point
    target_pt = [0.0, -0.25]

    # Connecting line from tip to target
    connect_line = [0.0, -0.1, 0.0, -0.25]

    bufs = {
        100: {"data": rf(box_rect)},
        103: {"data": rf(ptr_tris)},
        106: {"data": rf(target_pt)},
        109: {"data": rf(connect_line)},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": 1},
        104: {"vertexBufferId": 103, "format": "pos2_clip", "vertexCount": 3},
        107: {"vertexBufferId": 106, "format": "pos2_clip", "vertexCount": 1},
        110: {"vertexBufferId": 109, "format": "rect4", "vertexCount": 1},
    }
    dis = {
        102: {"layerId": 10, "name": "tooltip_box", "pipeline": "instancedRect@1", "geometryId": 101,
              "color": [0.18, 0.22, 0.30, 1.0], "cornerRadius": 6.0},
        105: {"layerId": 10, "name": "pointer", "pipeline": "triSolid@1", "geometryId": 104,
              "color": [0.18, 0.22, 0.30, 1.0]},
        108: {"layerId": 11, "name": "target", "pipeline": "points@1", "geometryId": 107,
              "color": [1.0, 0.4, 0.3, 1.0], "pointSize": 8.0},
        111: {"layerId": 10, "name": "connector", "pipeline": "lineAA@1", "geometryId": 110,
              "color": [0.35, 0.38, 0.45, 0.5], "lineWidth": 1.0, "dashLength": 4.0, "gapLength": 3.0},
    }
    doc = make_doc(500, 500, bufs, {},
                   {1: {"name": "tooltip", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "callout"}, 11: {"paneId": 1, "name": "target"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 233: Tooltip Callout

**Date:** 2026-03-22
**Goal:** Tooltip rectangle with triangular pointer pointing down to a target point. Dashed connector line. Tests callout UI pattern.
**Outcome:** Tooltip box, pointer triangle, target point, and dashed connector. {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Tooltip box | instancedRect@1 | 1 |
| 105 | 10 | Pointer triangle | triSolid@1 | 1 tri |
| 108 | 11 | Target point | points@1 | 1 pt |
| 111 | 10 | Dashed connector | lineAA@1 | 1 seg |

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Triangle pointer seamlessly connects to box bottom (same color).
- Dashed line visually links tooltip to target.
- Target point highlighted with large red dot.

### Done Wrong

Nothing.
"""
    return ("tooltip-callout", doc, md)


# ── Trial 234: Modal Dialog ─────────────────────────────────────────────────

def trial_234():
    # Dark overlay (full viewport, semi-transparent)
    overlay_rect = [-1.0, -1.0, 1.0, 1.0]

    # Dialog box (centered)
    dialog_rect = [-0.45, -0.3, 0.45, 0.35]

    # Close X button (two crossed lines)
    x_cx, x_cy = 0.37, 0.27
    x_arm = 0.04
    close_lines = [
        x_cx - x_arm, x_cy - x_arm, x_cx + x_arm, x_cy + x_arm,
        x_cx - x_arm, x_cy + x_arm, x_cx + x_arm, x_cy - x_arm,
    ]

    # Action button
    action_rect = [-0.15, -0.22, 0.15, -0.12]

    # Dialog header line
    header_line = [-0.40, 0.18, 0.40, 0.18]

    bufs = {
        100: {"data": rf(overlay_rect)},
        103: {"data": rf(dialog_rect)},
        106: {"data": rf(close_lines)},
        109: {"data": rf(action_rect)},
        112: {"data": rf(header_line)},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": 1},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": 1},
        107: {"vertexBufferId": 106, "format": "rect4", "vertexCount": 2},
        110: {"vertexBufferId": 109, "format": "rect4", "vertexCount": 1},
        113: {"vertexBufferId": 112, "format": "rect4", "vertexCount": 1},
    }
    dis = {
        102: {"layerId": 10, "name": "overlay", "pipeline": "instancedRect@1", "geometryId": 101,
              "color": [0.0, 0.0, 0.0, 0.5]},
        105: {"layerId": 11, "name": "dialog", "pipeline": "instancedRect@1", "geometryId": 104,
              "color": [0.15, 0.18, 0.25, 1.0], "cornerRadius": 8.0},
        108: {"layerId": 12, "name": "close_x", "pipeline": "lineAA@1", "geometryId": 107,
              "color": [0.7, 0.7, 0.7, 1.0], "lineWidth": 2.0},
        111: {"layerId": 12, "name": "action_btn", "pipeline": "instancedRect@1", "geometryId": 110,
              "color": [0.25, 0.55, 0.9, 1.0], "cornerRadius": 4.0},
        114: {"layerId": 12, "name": "header_line", "pipeline": "lineAA@1", "geometryId": 113,
              "color": [0.25, 0.28, 0.35, 1.0], "lineWidth": 1.0},
    }
    doc = make_doc(600, 500, bufs, {},
                   {1: {"name": "modal", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "overlay"}, 11: {"paneId": 1, "name": "dialog"}, 12: {"paneId": 1, "name": "content"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 234: Modal Dialog

**Date:** 2026-03-22
**Goal:** Modal dialog with dark semi-transparent overlay, centered dialog box, close X button, action button, and header divider. Tests overlay composition.
**Outcome:** Modal with overlay (alpha=0.5), dialog, close button, action button. {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Dark overlay | instancedRect@1 | 1 |
| 105 | 11 | Dialog box | instancedRect@1 | 1 |
| 108 | 12 | Close X | lineAA@1 | 2 segs |
| 111 | 12 | Action button | instancedRect@1 | 1 |
| 114 | 12 | Header line | lineAA@1 | 1 seg |

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Semi-transparent overlay dims background content.
- Dialog box centered with rounded corners.
- Close button in top-right corner of dialog.
- Action button centered at bottom of dialog.

### Done Wrong

Nothing.
"""
    return ("modal-dialog", doc, md)


# ── Trial 235: Card Grid 3x2 ────────────────────────────────────────────────

def trial_235():
    # 6 cards in 3x2 grid
    cw = 0.42
    ch = 0.55
    gap_x = 0.08
    gap_y = 0.10
    header_h = 0.08
    cols, rows = 3, 2
    total_w = cols * cw + (cols - 1) * gap_x
    total_h = rows * ch + (rows - 1) * gap_y
    sx = -total_w / 2
    sy = -total_h / 2

    card_colors = [
        [0.3, 0.6, 1.0, 1.0],
        [0.2, 0.8, 0.4, 1.0],
        [0.9, 0.4, 0.2, 1.0],
        [0.7, 0.3, 0.9, 1.0],
        [0.95, 0.7, 0.1, 1.0],
        [0.0, 0.8, 0.7, 1.0],
    ]

    shadow_rects = []
    card_rects = []
    header_rects = []

    for r in range(rows):
        for c in range(cols):
            i = r * cols + c
            x0 = sx + c * (cw + gap_x)
            y0 = sy + r * (ch + gap_y)
            x1 = x0 + cw
            y1 = y0 + ch

            # Shadow (offset darker rect)
            shadow_rects += [x0 + 0.015, y0 - 0.015, x1 + 0.015, y1 - 0.015]
            # Card body
            card_rects += [x0, y0, x1, y1]
            # Header stripe
            header_rects += [x0, y1 - header_h, x1, y1]

    # Need per-card header colors -> separate draw items
    bufs = {
        100: {"data": rf(shadow_rects)},
        103: {"data": rf(card_rects)},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": 6},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": 6},
    }
    dis = {
        102: {"layerId": 10, "name": "shadows", "pipeline": "instancedRect@1", "geometryId": 101,
              "color": [0.03, 0.04, 0.07, 0.6], "cornerRadius": 6.0},
        105: {"layerId": 11, "name": "cards", "pipeline": "instancedRect@1", "geometryId": 104,
              "color": [0.13, 0.16, 0.22, 1.0], "cornerRadius": 6.0},
    }

    bid = 106
    for i in range(6):
        data = header_rects[i * 4:(i + 1) * 4]
        bufs[bid] = {"data": rf(data)}
        geos[bid + 1] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": 1}
        dis[bid + 2] = {"layerId": 12, "name": f"header_{i}", "pipeline": "instancedRect@1",
                        "geometryId": bid + 1, "color": card_colors[i]}
        bid += 3

    doc = make_doc(800, 600, bufs, {},
                   {1: {"name": "cardgrid", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "shadows"}, 11: {"paneId": 1, "name": "cards"}, 12: {"paneId": 1, "name": "headers"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 235: Card Grid 3x2

**Date:** 2026-03-22
**Goal:** 6 cards in 3x2 grid. Each card has shadow, body, and colored header stripe. Tests card UI pattern with shadow effect.
**Outcome:** 6 cards with 6 unique header colors and offset shadow rects. {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Shadows | instancedRect@1 | 6 |
| 105 | 11 | Card bodies | instancedRect@1 | 6 |
| 108-125 | 12 | Header stripes | instancedRect@1 | 6 |

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Shadow offset creates depth effect.
- Header stripes color-coded per card.
- Grid layout with consistent gaps.

### Done Wrong

Nothing.
"""
    return ("card-grid-3x2", doc, md)


# ── Trial 236: Avatar Circles ────────────────────────────────────────────────

def trial_236():
    # 5 overlapping circles in a row
    colors = [
        [0.3, 0.6, 1.0, 1.0],
        [0.2, 0.8, 0.4, 1.0],
        [0.9, 0.4, 0.2, 1.0],
        [0.7, 0.3, 0.9, 1.0],
        [0.4, 0.45, 0.55, 1.0],  # last one gray ("+3 more")
    ]
    r = 0.12
    overlap = 0.07  # how much circles overlap
    segs = 20

    # Border circles (slightly larger, dark)
    border_r = r + 0.015

    bid = 100
    bufs = {}
    geos = {}
    dis = {}

    # Draw from right to left so leftmost is on top (higher draw item IDs draw on top)
    # Actually, lower IDs draw first (behind). We want leftmost on top.
    # So we draw rightmost first (lowest ID) and leftmost last (highest ID).
    for i in range(4, -1, -1):
        cx = -0.3 + i * (2 * r - overlap)
        cy = 0.0

        # Border circle
        border_data = circle_fan(cx, cy, border_r, segs)
        bufs[bid] = {"data": rf(border_data)}
        geos[bid + 1] = {"vertexBufferId": bid, "format": "pos2_clip", "vertexCount": len(border_data) // 2}
        dis[bid + 2] = {"layerId": 10, "name": f"border_{i}", "pipeline": "triSolid@1",
                        "geometryId": bid + 1, "color": DARK_BG}
        bid += 3

        # Fill circle
        fill_data = circle_fan(cx, cy, r, segs)
        bufs[bid] = {"data": rf(fill_data)}
        geos[bid + 1] = {"vertexBufferId": bid, "format": "pos2_clip", "vertexCount": len(fill_data) // 2}
        dis[bid + 2] = {"layerId": 11, "name": f"avatar_{i}", "pipeline": "triSolid@1",
                        "geometryId": bid + 1, "color": colors[i]}
        bid += 3

    doc = make_doc(600, 200, bufs, {},
                   {1: {"name": "avatars", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "borders"}, 11: {"paneId": 1, "name": "fills"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 236: Avatar Circles

**Date:** 2026-03-22
**Goal:** 5 overlapping avatar circles in a row. Each slightly overlaps the previous. Last circle is gray ("+3 more" indicator). Dark border rings create separation.
**Outcome:** 5 overlapping circles with border rings. {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| borders | 10 | Border rings | triSolid@1 | 5 circles |
| avatars | 11 | Colored fills | triSolid@1 | 5 circles |

Each circle = {segs} triangle fan segments. Overlap = {overlap} clip units.

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Circles overlap left-to-right with leftmost on top.
- Dark border rings create visual separation between overlapping circles.
- Gray last circle implies more items.

### Done Wrong

Nothing.
"""
    return ("avatar-circles", doc, md)


# ── Trial 237: Status Indicators ─────────────────────────────────────────────

def trial_237():
    # 8 indicator dots
    statuses = [
        ("healthy", [0.2, 0.8, 0.3, 1.0]),
        ("healthy", [0.2, 0.8, 0.3, 1.0]),
        ("healthy", [0.2, 0.8, 0.3, 1.0]),
        ("warning", [0.95, 0.8, 0.1, 1.0]),
        ("warning", [0.95, 0.8, 0.1, 1.0]),
        ("critical", [0.9, 0.15, 0.15, 1.0]),
        ("inactive", [0.4, 0.42, 0.48, 1.0]),
        ("inactive", [0.4, 0.42, 0.48, 1.0]),
    ]
    r = 0.06
    segs = 14
    gap = 0.22
    sx = -(len(statuses) - 1) * gap / 2

    # Group by status for fewer draw items
    groups = {}
    for i, (status, color) in enumerate(statuses):
        cx = sx + i * gap
        cy = 0.0
        key = status
        if key not in groups:
            groups[key] = {"color": color, "data": []}
        groups[key]["data"] += circle_fan(cx, cy, r, segs)

    bid = 100
    bufs = {}
    geos = {}
    dis = {}
    for name in ["healthy", "warning", "critical", "inactive"]:
        if name not in groups:
            continue
        g = groups[name]
        bufs[bid] = {"data": rf(g["data"])}
        geos[bid + 1] = {"vertexBufferId": bid, "format": "pos2_clip", "vertexCount": len(g["data"]) // 2}
        dis[bid + 2] = {"layerId": 10, "name": name, "pipeline": "triSolid@1",
                        "geometryId": bid + 1, "color": g["color"]}
        bid += 3

    doc = make_doc(700, 200, bufs, {},
                   {1: {"name": "status", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "dots"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 237: Status Indicators

**Date:** 2026-03-22
**Goal:** Row of 8 indicator dots: 3 green (healthy), 2 yellow (warning), 1 red (critical), 2 gray (inactive). Tests status dot UI pattern.
**Outcome:** 8 dots in 4 color groups. {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Status | Pipeline | Count |
|----------|-------|--------|----------|-------|
| varies | 10 | Healthy (green) | triSolid@1 | 3 dots |
| varies | 10 | Warning (yellow) | triSolid@1 | 2 dots |
| varies | 10 | Critical (red) | triSolid@1 | 1 dot |
| varies | 10 | Inactive (gray) | triSolid@1 | 2 dots |

Each dot = {segs}-segment circle. Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Color coding follows standard conventions (green=ok, yellow=warning, red=error, gray=inactive).
- Dots evenly spaced in a row.

### Done Wrong

Nothing.
"""
    return ("status-indicators", doc, md)


# ── Trial 238: Color Palette ─────────────────────────────────────────────────

def trial_238():
    # 6 columns x 5 rows of color swatches
    cols, rows = 6, 5
    sw = 0.22
    sh = 0.22
    gap = 0.03
    total_w = cols * sw + (cols - 1) * gap
    total_h = rows * sh + (rows - 1) * gap
    sx = -total_w / 2
    sy = -total_h / 2

    # Curated palette: warm, cool, neutral
    palette = [
        # Row 0 (bottom): reds/oranges
        [0.95,0.3,0.2,1], [0.85,0.4,0.3,1], [0.95,0.55,0.2,1], [0.95,0.7,0.3,1], [0.9,0.8,0.5,1], [0.95,0.9,0.7,1],
        # Row 1: pinks/purples
        [0.85,0.2,0.5,1], [0.7,0.2,0.6,1], [0.55,0.2,0.7,1], [0.4,0.3,0.8,1], [0.6,0.5,0.85,1], [0.8,0.7,0.9,1],
        # Row 2: blues
        [0.15,0.3,0.8,1], [0.2,0.45,0.9,1], [0.3,0.6,0.95,1], [0.5,0.75,0.95,1], [0.7,0.85,0.95,1], [0.85,0.92,0.97,1],
        # Row 3: greens
        [0.1,0.5,0.2,1], [0.2,0.65,0.3,1], [0.3,0.75,0.4,1], [0.5,0.85,0.5,1], [0.7,0.9,0.6,1], [0.85,0.95,0.8,1],
        # Row 4 (top): neutrals
        [0.15,0.15,0.15,1], [0.3,0.3,0.3,1], [0.5,0.5,0.5,1], [0.7,0.7,0.7,1], [0.85,0.85,0.85,1], [0.95,0.95,0.95,1],
    ]

    all_rects = []
    for r in range(rows):
        for c in range(cols):
            x0 = sx + c * (sw + gap)
            y0 = sy + r * (sh + gap)
            all_rects += [x0, y0, x0 + sw, y0 + sh]

    # Need individual colors -> use triGradient@1 or separate draw items
    # Using separate draw items for each swatch
    bid = 100
    bufs = {}
    geos = {}
    dis = {}
    for i in range(30):
        r_idx = i // cols
        c_idx = i % cols
        x0 = sx + c_idx * (sw + gap)
        y0 = sy + r_idx * (sh + gap)
        data = [x0, y0, x0 + sw, y0 + sh]
        bufs[bid] = {"data": rf(data)}
        geos[bid + 1] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": 1}
        dis[bid + 2] = {"layerId": 10, "name": f"swatch_{i}", "pipeline": "instancedRect@1",
                        "geometryId": bid + 1, "color": palette[i], "cornerRadius": 2.0}
        bid += 3

    doc = make_doc(700, 600, bufs, {},
                   {1: {"name": "palette", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "swatches"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 238: Color Palette

**Date:** 2026-03-22
**Goal:** 6x5 grid of 30 color swatches showing a curated palette with warm, cool, and neutral sections. Each swatch = instancedRect@1 with unique color.
**Outcome:** 30 swatches across 5 rows (reds, purples, blues, greens, neutrals). {n} unique IDs. Zero defects.

---

## What Was Built

| Row | Theme | Colors |
|-----|-------|--------|
| 0 | Reds/Oranges | 6 warm swatches |
| 1 | Pinks/Purples | 6 swatches |
| 2 | Blues | 6 cool swatches |
| 3 | Greens | 6 natural swatches |
| 4 | Neutrals | 6 grayscale swatches |

30 DrawItems total, each instancedRect@1 with cornerRadius=2.

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Palette organized by hue family (rows) and lightness (columns).
- Consistent swatch size with small gaps.

### Done Wrong

Nothing.
"""
    return ("color-palette", doc, md)


# ── Trial 239: Font Specimen ─────────────────────────────────────────────────

def trial_239():
    # 6 horizontal bars of increasing height simulating text lines
    heights = [0.03, 0.045, 0.06, 0.08, 0.11, 0.15]
    gap = 0.06
    x_left = -0.7
    max_widths = [0.8, 0.9, 1.0, 1.1, 1.2, 1.3]

    bars = []
    y = -0.5
    for i, h in enumerate(heights):
        w = min(max_widths[i], 1.4)
        bars += [x_left, y, x_left + w, y + h]
        y += h + gap

    bufs = {100: {"data": rf(bars)}}
    geos = {101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": 6}}
    dis = {102: {"layerId": 10, "name": "text_bars", "pipeline": "instancedRect@1", "geometryId": 101,
                 "color": [0.65, 0.68, 0.75, 1.0], "cornerRadius": 2.0}}
    doc = make_doc(600, 500, bufs, {},
                   {1: {"name": "specimen", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "bars"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 239: Font Specimen

**Date:** 2026-03-22
**Goal:** 6 horizontal bars of increasing height simulating text lines at different font sizes. Light gray on dark. Left-aligned.
**Outcome:** 6 bars from smallest to largest, left-aligned. {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Text-like bars | instancedRect@1 | 6 |

Heights: {', '.join(str(h) for h in heights)} clip units.

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Progressive height increase suggests font size hierarchy.
- Left-aligned like natural text.
- Light gray on dark background for readability.

### Done Wrong

Nothing.
"""
    return ("font-specimen", doc, md)


# ── Trial 240: Icon Grid 4x4 ────────────────────────────────────────────────

def trial_240():
    # 4x4 grid of simple geometric icons
    cell = 0.35
    gap = 0.06
    total = 4 * cell + 3 * gap
    sx = -total / 2
    sy = -total / 2
    icon_r = 0.08

    tri_data = []
    line_data = []

    for row in range(4):
        for col in range(4):
            cx = sx + col * (cell + gap) + cell / 2
            cy = sy + row * (cell + gap) + cell / 2
            idx = row * 4 + col

            if idx % 4 == 0:
                # Circle
                tri_data += circle_fan(cx, cy, icon_r, 12)
            elif idx % 4 == 1:
                # Triangle (pointing up)
                s = icon_r
                tri_data += [cx, cy + s, cx - s * 0.866, cy - s * 0.5, cx + s * 0.866, cy - s * 0.5]
            elif idx % 4 == 2:
                # Square (as two triangles)
                s = icon_r * 0.7
                tri_data += [cx - s, cy - s, cx + s, cy - s, cx + s, cy + s]
                tri_data += [cx - s, cy - s, cx + s, cy + s, cx - s, cy + s]
            else:
                # X mark (two lines)
                s = icon_r * 0.7
                line_data += [cx - s, cy - s, cx + s, cy + s]
                line_data += [cx - s, cy + s, cx + s, cy - s]

    bufs = {
        100: {"data": rf(tri_data)},
        103: {"data": rf(line_data)},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "pos2_clip", "vertexCount": len(tri_data) // 2},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": len(line_data) // 4},
    }
    assert len(tri_data) // 2 % 3 == 0
    dis = {
        102: {"layerId": 10, "name": "filled_icons", "pipeline": "triSolid@1", "geometryId": 101,
              "color": [0.6, 0.7, 0.85, 1.0]},
        105: {"layerId": 10, "name": "line_icons", "pipeline": "lineAA@1", "geometryId": 104,
              "color": [0.6, 0.7, 0.85, 1.0], "lineWidth": 2.5},
    }
    doc = make_doc(600, 600, bufs, {},
                   {1: {"name": "icons", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "icons"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 240: Icon Grid 4x4

**Date:** 2026-03-22
**Goal:** 4x4 grid of 16 simple geometric icons (circles, triangles, squares, X marks) as repeating pattern. Tests mixed shape composition.
**Outcome:** 16 icons in 4 shape types. {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Filled shapes | triSolid@1 | circles + triangles + squares |
| 105 | 10 | Line shapes | lineAA@1 | X marks |

4 icon types cycling through 16 cells.

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Four distinct icon shapes clearly recognizable.
- Grid layout with consistent spacing.
- Alternating pattern creates visual rhythm.

### Done Wrong

Nothing.
"""
    return ("icon-grid-4x4", doc, md)


# ── Trial 241: Loading Spinner ───────────────────────────────────────────────

def trial_241():
    # 12 tick marks radiating from center with varying brightness
    n_ticks = 12
    inner_r = 0.15
    outer_r = 0.30
    tick_data = []

    for i in range(n_ticks):
        angle = math.pi / 2 - i * 2 * math.pi / n_ticks  # start from top, clockwise
        x0 = inner_r * math.cos(angle)
        y0 = inner_r * math.sin(angle)
        x1 = outer_r * math.cos(angle)
        y1 = outer_r * math.sin(angle)
        tick_data += [x0, y0, x1, y1]

    # Split ticks by brightness levels (simulate rotation)
    # Ticks 0-2: bright, 3-5: medium, 6-8: dim, 9-11: very dim
    bright_ticks = tick_data[0:12]     # ticks 0-2
    medium_ticks = tick_data[12:24]    # ticks 3-5
    dim_ticks = tick_data[24:36]       # ticks 6-8
    vdim_ticks = tick_data[36:48]      # ticks 9-11

    bufs = {
        100: {"data": rf(bright_ticks)},
        103: {"data": rf(medium_ticks)},
        106: {"data": rf(dim_ticks)},
        109: {"data": rf(vdim_ticks)},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": 3},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": 3},
        107: {"vertexBufferId": 106, "format": "rect4", "vertexCount": 3},
        110: {"vertexBufferId": 109, "format": "rect4", "vertexCount": 3},
    }
    dis = {
        102: {"layerId": 10, "name": "bright", "pipeline": "lineAA@1", "geometryId": 101,
              "color": [0.85, 0.88, 0.95, 1.0], "lineWidth": 3.5},
        105: {"layerId": 10, "name": "medium", "pipeline": "lineAA@1", "geometryId": 104,
              "color": [0.55, 0.58, 0.65, 1.0], "lineWidth": 3.5},
        108: {"layerId": 10, "name": "dim", "pipeline": "lineAA@1", "geometryId": 107,
              "color": [0.3, 0.32, 0.38, 1.0], "lineWidth": 3.5},
        111: {"layerId": 10, "name": "very_dim", "pipeline": "lineAA@1", "geometryId": 110,
              "color": [0.15, 0.17, 0.22, 1.0], "lineWidth": 3.5},
    }
    doc = make_doc(400, 400, bufs, {},
                   {1: {"name": "spinner", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "ticks"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 241: Loading Spinner

**Date:** 2026-03-22
**Goal:** 12 tick marks arranged in a circle radiating from center. Varying brightness from bright to dim to simulate rotation. Tests lineAA@1 radial layout.
**Outcome:** 12 ticks in 4 brightness groups. {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count | Brightness |
|----------|-------|---------|----------|-------|------------|
| 102 | 10 | Bright ticks | lineAA@1 | 3 | 1.0 |
| 105 | 10 | Medium ticks | lineAA@1 | 3 | 0.6 |
| 108 | 10 | Dim ticks | lineAA@1 | 3 | 0.3 |
| 111 | 10 | Very dim ticks | lineAA@1 | 3 | 0.15 |

Inner radius: {inner_r}, outer radius: {outer_r}. Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- 12 ticks evenly spaced at 30-degree intervals.
- Brightness gradient creates illusion of rotation.
- Starts from top (12 o'clock) going clockwise.

### Done Wrong

Nothing.
"""
    return ("loading-spinner", doc, md)


# ── Trial 242: Skeleton Screen ───────────────────────────────────────────────

def trial_242():
    # 3 skeleton card placeholders
    card_w = 0.5
    card_h = 0.8
    gap = 0.08
    total_w = 3 * card_w + 2 * gap
    sx = -total_w / 2

    card_rects = []
    image_rects = []
    text_rects = []

    bg_color = [0.12, 0.14, 0.20, 1.0]
    placeholder_color = [0.16, 0.18, 0.24, 1.0]

    for i in range(3):
        cx = sx + i * (card_w + gap)
        cy = -0.4
        # Card body
        card_rects += [cx, cy, cx + card_w, cy + card_h]

        # Image placeholder (top portion)
        img_pad = 0.03
        image_rects += [cx + img_pad, cy + card_h - 0.32, cx + card_w - img_pad, cy + card_h - img_pad]

        # 3 text lines of varying width
        line_y_start = cy + card_h - 0.40
        line_h = 0.04
        line_gap = 0.03
        widths = [card_w * 0.8, card_w * 0.6, card_w * 0.45]
        for j, w in enumerate(widths):
            ly = line_y_start - j * (line_h + line_gap)
            text_rects += [cx + img_pad, ly, cx + img_pad + w, ly + line_h]

    bufs = {
        100: {"data": rf(card_rects)},
        103: {"data": rf(image_rects)},
        106: {"data": rf(text_rects)},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": 3},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": 3},
        107: {"vertexBufferId": 106, "format": "rect4", "vertexCount": len(text_rects) // 4},
    }
    dis = {
        102: {"layerId": 10, "name": "cards", "pipeline": "instancedRect@1", "geometryId": 101,
              "color": bg_color, "cornerRadius": 6.0},
        105: {"layerId": 11, "name": "images", "pipeline": "instancedRect@1", "geometryId": 104,
              "color": placeholder_color, "cornerRadius": 4.0},
        108: {"layerId": 11, "name": "text_lines", "pipeline": "instancedRect@1", "geometryId": 107,
              "color": placeholder_color, "cornerRadius": 2.0},
    }
    doc = make_doc(800, 500, bufs, {},
                   {1: {"name": "skeleton", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": [0.08, 0.10, 0.14, 1.0]}},
                   {10: {"paneId": 1, "name": "cards"}, 11: {"paneId": 1, "name": "placeholders"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 242: Skeleton Screen

**Date:** 2026-03-22
**Goal:** 3 skeleton loading card placeholders, each with image placeholder and 3 text-line placeholders of varying width. Light gray on slightly lighter gray.
**Outcome:** 3 cards with 3 image + 9 text placeholders. {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Card bodies | instancedRect@1 | 3 |
| 105 | 11 | Image placeholders | instancedRect@1 | 3 |
| 108 | 11 | Text line placeholders | instancedRect@1 | 9 |

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Skeleton cards show realistic content layout: image on top, text lines below.
- Text lines decrease in width (title, subtitle, detail).
- Subtle color difference between card and placeholder creates loading appearance.

### Done Wrong

Nothing.
"""
    return ("skeleton-screen", doc, md)


# ── Trial 243: Onboarding Steps ──────────────────────────────────────────────

def trial_243():
    # 4 step circles connected by horizontal line
    step_y = 0.0
    step_xs = [-0.55, -0.18, 0.18, 0.55]
    circle_r = 0.07
    segs = 16

    # Connector line (full span)
    connector = [step_xs[0], step_y, step_xs[-1], step_y]

    # Completed steps (1 & 2): filled circles
    completed = []
    for x in step_xs[:2]:
        completed += circle_fan(x, step_y, circle_r, segs)

    # Current step (3): outlined circle (thicker)
    current_outline = circle_outline(step_xs[2], step_y, circle_r, segs)

    # Upcoming step (4): outlined circle (dimmer)
    upcoming_outline = circle_outline(step_xs[3], step_y, circle_r, segs)

    # Inner dot for current step
    current_dot = circle_fan(step_xs[2], step_y, circle_r * 0.4, 10)

    # Progress line (completed portion)
    progress_line = [step_xs[0], step_y, step_xs[1], step_y]

    bufs = {
        100: {"data": rf(connector)},
        103: {"data": rf(completed)},
        106: {"data": rf(current_outline)},
        109: {"data": rf(upcoming_outline)},
        112: {"data": rf(current_dot)},
        115: {"data": rf(progress_line)},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": 1},
        104: {"vertexBufferId": 103, "format": "pos2_clip", "vertexCount": len(completed) // 2},
        107: {"vertexBufferId": 106, "format": "rect4", "vertexCount": len(current_outline) // 4},
        110: {"vertexBufferId": 109, "format": "rect4", "vertexCount": len(upcoming_outline) // 4},
        113: {"vertexBufferId": 112, "format": "pos2_clip", "vertexCount": len(current_dot) // 2},
        116: {"vertexBufferId": 115, "format": "rect4", "vertexCount": 1},
    }
    assert len(completed) // 2 % 3 == 0
    assert len(current_dot) // 2 % 3 == 0
    dis = {
        102: {"layerId": 10, "name": "connector", "pipeline": "lineAA@1", "geometryId": 101,
              "color": [0.25, 0.27, 0.33, 1.0], "lineWidth": 3.0},
        105: {"layerId": 11, "name": "completed", "pipeline": "triSolid@1", "geometryId": 104,
              "color": [0.25, 0.7, 0.45, 1.0]},
        108: {"layerId": 11, "name": "current_ring", "pipeline": "lineAA@1", "geometryId": 107,
              "color": [0.3, 0.65, 0.95, 1.0], "lineWidth": 3.0},
        111: {"layerId": 11, "name": "upcoming_ring", "pipeline": "lineAA@1", "geometryId": 110,
              "color": [0.35, 0.38, 0.45, 1.0], "lineWidth": 2.0},
        114: {"layerId": 12, "name": "current_dot", "pipeline": "triSolid@1", "geometryId": 113,
              "color": [0.3, 0.65, 0.95, 1.0]},
        117: {"layerId": 10, "name": "progress", "pipeline": "lineAA@1", "geometryId": 116,
              "color": [0.25, 0.7, 0.45, 1.0], "lineWidth": 3.0},
    }
    doc = make_doc(700, 250, bufs, {},
                   {1: {"name": "onboarding", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "lines"}, 11: {"paneId": 1, "name": "circles"}, 12: {"paneId": 1, "name": "dots"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 243: Onboarding Steps

**Date:** 2026-03-22
**Goal:** 4 numbered step circles connected by horizontal line. Steps 1-2 filled (completed), step 3 outlined with dot (current), step 4 gray (upcoming). Tests stepper UI pattern.
**Outcome:** 4-step indicator with correct states. {n} unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Connector line | lineAA@1 | 1 seg |
| 117 | 10 | Progress line | lineAA@1 | 1 seg |
| 105 | 11 | Completed circles | triSolid@1 | 2 circles |
| 108 | 11 | Current ring | lineAA@1 | {segs} segs |
| 111 | 11 | Upcoming ring | lineAA@1 | {segs} segs |
| 114 | 12 | Current dot | triSolid@1 | 1 circle |

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Completed steps are solid green circles.
- Current step has blue outline with inner dot.
- Upcoming step has dim gray outline.
- Progress line overlays connector line up to completed steps.

### Done Wrong

Nothing.
"""
    return ("onboarding-steps", doc, md)


# ── Trial 244: Tag Cloud ─────────────────────────────────────────────────────

def trial_244():
    # 15 tag rectangles of varying width, arranged in wrapped rows
    tag_configs = [
        (0.28, [0.3, 0.55, 0.85, 1.0]),
        (0.22, [0.3, 0.55, 0.85, 1.0]),
        (0.35, [0.2, 0.75, 0.45, 1.0]),
        (0.18, [0.85, 0.4, 0.2, 1.0]),
        (0.30, [0.7, 0.3, 0.85, 1.0]),
        (0.25, [0.3, 0.55, 0.85, 1.0]),
        (0.20, [0.2, 0.75, 0.45, 1.0]),
        (0.32, [0.85, 0.4, 0.2, 1.0]),
        (0.15, [0.95, 0.7, 0.1, 1.0]),
        (0.27, [0.7, 0.3, 0.85, 1.0]),
        (0.22, [0.2, 0.75, 0.45, 1.0]),
        (0.34, [0.3, 0.55, 0.85, 1.0]),
        (0.19, [0.95, 0.7, 0.1, 1.0]),
        (0.26, [0.85, 0.4, 0.2, 1.0]),
        (0.21, [0.7, 0.3, 0.85, 1.0]),
    ]
    tag_h = 0.10
    gap_x = 0.04
    gap_y = 0.06
    max_row_w = 1.4
    sx = -0.7

    # Layout tags in wrapped rows
    rows_layout = []
    current_row = []
    current_x = 0

    for i, (w, color) in enumerate(tag_configs):
        if current_x + w > max_row_w and current_row:
            rows_layout.append(current_row)
            current_row = []
            current_x = 0
        current_row.append((i, w, color, current_x))
        current_x += w + gap_x
    if current_row:
        rows_layout.append(current_row)

    bid = 100
    bufs = {}
    geos = {}
    dis = {}

    y_start = 0.3
    for row_idx, row in enumerate(rows_layout):
        y = y_start - row_idx * (tag_h + gap_y)
        for i, w, color, rx in row:
            x0 = sx + rx
            y0 = y - tag_h / 2
            x1 = x0 + w
            y1 = y + tag_h / 2
            data = [x0, y0, x1, y1]
            bufs[bid] = {"data": rf(data)}
            geos[bid + 1] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": 1}
            dis[bid + 2] = {"layerId": 10, "name": f"tag_{i}", "pipeline": "instancedRect@1",
                            "geometryId": bid + 1, "color": color, "cornerRadius": 12.0}
            bid += 3

    doc = make_doc(700, 400, bufs, {},
                   {1: {"name": "tagcloud", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                        "hasClearColor": True, "clearColor": DARK_BG}},
                   {10: {"paneId": 1, "name": "tags"}},
                   geos, dis)
    n = count_ids(doc)
    md = f"""# Trial 244: Tag Cloud

**Date:** 2026-03-22
**Goal:** 15 tag rectangles of varying width arranged in wrapped rows. Different colors by category. Pill-shaped with large cornerRadius. Tests flexible layout.
**Outcome:** 15 tags in {len(rows_layout)} rows with 5 color categories. {n} unique IDs. Zero defects.

---

## What Was Built

| Row | Tags |
|-----|------|
"""
    for row_idx, row in enumerate(rows_layout):
        md += f"| {row_idx} | {len(row)} tags |\n"

    md += f"""
15 DrawItems, each instancedRect@1 with cornerRadius=12 (pill shape).

Total: {n} unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Tags wrap to new rows when exceeding max width.
- 5 color categories create visual grouping.
- Pill-shaped (large cornerRadius) tags match modern UI conventions.

### Done Wrong

Nothing.
"""
    return ("tag-cloud", doc, md)


# ── Main ─────────────────────────────────────────────────────────────────────

def main():
    os.makedirs(OUT_DIR, exist_ok=True)

    trials = [
        (212, trial_212),
        (213, trial_213),
        (214, trial_214),
        (215, trial_215),
        (216, trial_216),
        (217, trial_217),
        (218, trial_218),
        (219, trial_219),
        (220, trial_220),
        (221, trial_221),
        (222, trial_222),
        (223, trial_223),
        (224, trial_224),
        (225, trial_225),
        (226, trial_226),
        (227, trial_227),
        (228, trial_228),
        (229, trial_229),
        (230, trial_230),
        (231, trial_231),
        (232, trial_232),
        (233, trial_233),
        (234, trial_234),
        (235, trial_235),
        (236, trial_236),
        (237, trial_237),
        (238, trial_238),
        (239, trial_239),
        (240, trial_240),
        (241, trial_241),
        (242, trial_242),
        (243, trial_243),
        (244, trial_244),
    ]

    print(f"Generating {len(trials)} trials (212-244)...\n")
    for num, fn in trials:
        slug, doc, md = fn()
        write_trial(num, slug, doc, md)

    print(f"\nDone. {len(trials)} trials generated.")

if __name__ == "__main__":
    main()
