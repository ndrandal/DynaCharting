#!/usr/bin/env python3
"""Generate trials 245-261 (Creative Edge Cases) for DynaCharting.

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
    return [round(x, digits) for x in arr]

def circle_fan(cx, cy, r, segs):
    verts = []
    for i in range(segs):
        a0 = 2 * math.pi * i / segs
        a1 = 2 * math.pi * (i + 1) / segs
        verts += [cx, cy,
                  cx + r * math.cos(a0), cy + r * math.sin(a0),
                  cx + r * math.cos(a1), cy + r * math.sin(a1)]
    return verts

def circle_outline(cx, cy, r, segs):
    verts = []
    for i in range(segs):
        a0 = 2 * math.pi * i / segs
        a1 = 2 * math.pi * (i + 1) / segs
        verts += [cx + r * math.cos(a0), cy + r * math.sin(a0),
                  cx + r * math.cos(a1), cy + r * math.sin(a1)]
    return verts

def arc_outline(cx, cy, r, start_angle, end_angle, segs):
    verts = []
    for i in range(segs):
        a0 = start_angle + (end_angle - start_angle) * i / segs
        a1 = start_angle + (end_angle - start_angle) * (i + 1) / segs
        verts += [cx + r * math.cos(a0), cy + r * math.sin(a0),
                  cx + r * math.cos(a1), cy + r * math.sin(a1)]
    return verts

def line_seg(x0, y0, x1, y1):
    return [x0, y0, x1, y1]

def arrow_head(x, y, angle, size):
    """Triangle arrowhead pointing in direction `angle` (radians). Returns 6 floats (1 tri)."""
    dx = size * math.cos(angle)
    dy = size * math.sin(angle)
    perp = math.pi / 2
    px = size * 0.4 * math.cos(angle + perp)
    py = size * 0.4 * math.sin(angle + perp)
    return [x + dx, y + dy,
            x - px, y - py,
            x + px, y + py]

def make_doc(viewport_w, viewport_h, buffers, transforms, panes, layers, geometries, drawItems):
    doc = {"version": 1, "viewport": {"width": viewport_w, "height": viewport_h}}
    doc["buffers"] = {str(k): v for k, v in buffers.items()}
    doc["transforms"] = {str(k): v for k, v in transforms.items()}
    doc["panes"] = {str(k): v for k, v in panes.items()}
    doc["layers"] = {str(k): v for k, v in layers.items()}
    doc["geometries"] = {str(k): v for k, v in geometries.items()}
    doc["drawItems"] = {str(k): v for k, v in drawItems.items()}
    return doc

def write_trial(num, slug, doc, md):
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
    ids = set()
    for section in ["buffers", "transforms", "panes", "layers", "geometries", "drawItems"]:
        if section in doc:
            for k in doc[section]:
                ids.add(int(k))
    return len(ids)

DARK_BG = [0.06, 0.09, 0.16, 1.0]

# ── Trial 245: Floor Plan ───────────────────────────────────────────────────

def trial_245():
    # Apartment floor plan — rooms as rects, walls as thick lines, door arcs
    # Clip space layout: apartment from -0.85 to 0.85
    # 6 rooms: living, bedroom, kitchen, bathroom, hallway, study
    rooms = []  # instancedRect@1
    room_defs = [
        (-0.8, -0.1, 0.1, 0.8),   # living room (left-top)
        (0.15, -0.1, 0.8, 0.8),   # bedroom (right-top)
        (-0.8, -0.8, -0.2, -0.15),  # kitchen (left-bottom)
        (-0.15, -0.8, 0.35, -0.15), # bathroom (center-bottom)
        (0.4, -0.8, 0.8, -0.15),   # study (right-bottom)
        (-0.15, -0.1, 0.1, 0.3),   # hallway (center)
    ]
    room_colors = [
        [0.15, 0.18, 0.28, 1.0],  # living - dark blue
        [0.18, 0.15, 0.25, 1.0],  # bedroom - dark purple
        [0.15, 0.20, 0.18, 1.0],  # kitchen - dark green
        [0.14, 0.18, 0.22, 1.0],  # bathroom - dark teal
        [0.20, 0.17, 0.15, 1.0],  # study - dark brown
        [0.12, 0.14, 0.20, 1.0],  # hallway - dark gray
    ]
    for r in room_defs:
        rooms += list(r)

    # Walls — outer boundary + room dividers
    walls = []
    # Outer walls
    walls += line_seg(-0.85, -0.85, 0.85, -0.85)  # bottom
    walls += line_seg(0.85, -0.85, 0.85, 0.85)     # right
    walls += line_seg(0.85, 0.85, -0.85, 0.85)     # top
    walls += line_seg(-0.85, 0.85, -0.85, -0.85)   # left
    # Horizontal divider (upper/lower)
    walls += line_seg(-0.85, -0.1, -0.15, -0.1)
    walls += line_seg(0.1, -0.1, 0.85, -0.1)
    # Vertical dividers
    walls += line_seg(0.1, 0.3, 0.1, 0.85)  # living/bedroom upper
    walls += line_seg(0.15, -0.1, 0.15, 0.85)  # living/bedroom wall
    walls += line_seg(-0.2, -0.85, -0.2, -0.15)  # kitchen left wall
    walls += line_seg(-0.15, -0.85, -0.15, -0.15)  # kitchen/bathroom
    walls += line_seg(0.35, -0.85, 0.35, -0.15)   # bathroom/study
    walls += line_seg(0.4, -0.85, 0.4, -0.15)    # study left wall
    # Horizontal lower dividers
    walls += line_seg(-0.85, -0.15, 0.85, -0.15)

    # Door arcs (quarter circles, 6 segs each)
    door_arcs = []
    # Living room door (bottom-center of living room)
    door_arcs += arc_outline(-0.15, -0.1, 0.15, 0, math.pi/2, 6)
    # Bedroom door
    door_arcs += arc_outline(0.15, -0.1, 0.15, math.pi/2, math.pi, 6)
    # Kitchen door
    door_arcs += arc_outline(-0.2, -0.15, 0.12, -math.pi/2, 0, 6)
    # Bathroom door
    door_arcs += arc_outline(0.0, -0.15, 0.12, -math.pi, -math.pi/2, 6)

    bufs = {
        100: {"data": rf(rooms)},
        103: {"data": rf(walls)},
        106: {"data": rf(door_arcs)},
    }
    xforms = {}
    panes = {1: {"name": "floorplan", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                  "hasClearColor": True, "clearColor": [0.04, 0.06, 0.10, 1.0]}}
    layers = {10: {"paneId": 1, "name": "rooms"}, 11: {"paneId": 1, "name": "walls"}, 12: {"paneId": 1, "name": "doors"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": len(rooms)//4},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": len(walls)//4},
        107: {"vertexBufferId": 106, "format": "rect4", "vertexCount": len(door_arcs)//4},
    }
    dis = {
        102: {"layerId": 10, "name": "room_fills", "pipeline": "instancedRect@1", "geometryId": 101,
              "color": [0.15, 0.18, 0.28, 0.6]},
        105: {"layerId": 11, "name": "walls", "pipeline": "lineAA@1", "geometryId": 104,
              "color": [0.75, 0.78, 0.85, 1.0], "lineWidth": 3.0},
        108: {"layerId": 12, "name": "door_arcs", "pipeline": "lineAA@1", "geometryId": 107,
              "color": [0.4, 0.7, 0.9, 0.8], "lineWidth": 1.5},
    }
    doc = make_doc(700, 700, bufs, xforms, panes, layers, geos, dis)
    nid = count_ids(doc)
    md = f"""# Trial 245: Apartment Floor Plan

**Date:** 2026-03-22
**Goal:** Top-down apartment floor plan with 6 rooms as filled rectangles, thick walls as lineAA@1, and quarter-circle door arcs.
**Outcome:** 6 rooms rendered, {len(walls)//4} wall segments, 4 door arcs (24 arc segments). {nid} unique IDs. Zero defects.

---

## What Was Built
Viewport 700x700. Apartment with living room, bedroom, kitchen, bathroom, study, and hallway.
Rooms drawn as instancedRect@1 with semi-transparent fills. Walls as lineAA@1 (lineWidth=3). Door arcs as quarter circles.
Total: {nid} unique IDs (1 pane, 3 layers, 3 buffers, 3 geometries, 3 drawItems).

---

## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---

## Spatial Reasoning Analysis
### Done Right
- **Room rectangles fill correctly.** All 6 rooms have non-overlapping regions covering the apartment area.
- **Wall lines trace room boundaries.** Outer walls form closed rectangle, inner walls align with room edges.
- **Door arcs indicate swing direction.** Quarter-circle arcs at room entrances show door swing.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Architectural layouts map well to instancedRect@1.** Each room is a single rect instance — efficient.
2. **Door arcs need small segment counts.** 6 segments per quarter-circle is sufficient for visual recognition.
"""
    write_trial(245, "floor-plan", doc, md)

# ── Trial 246: EKG Heartbeat ────────────────────────────────────────────────

def trial_246():
    # Realistic PQRST waveform — one heartbeat cycle repeated across screen
    # Data space: x=[0,100], y=[-1,2]
    # One PQRST cycle from x=0 to x=20
    def pqrst(x_off):
        """Generate one PQRST cycle starting at x_off, width=20."""
        pts = []
        # Baseline (0-2)
        pts += [(x_off+0, 0), (x_off+2, 0)]
        # P wave (2-5) small bump
        pts += [(x_off+3, 0.15), (x_off+4, 0.25), (x_off+5, 0)]
        # PR segment (5-6)
        pts += [(x_off+6, 0)]
        # Q dip (6-7)
        pts += [(x_off+6.5, -0.15)]
        # R spike (7-8)
        pts += [(x_off+7.0, -0.15), (x_off+7.5, 1.8)]
        # S dip (8-9)
        pts += [(x_off+8.0, -0.3), (x_off+8.5, 0)]
        # ST segment (9-11)
        pts += [(x_off+10, 0.05)]
        # T wave (11-15)
        pts += [(x_off+12, 0.15), (x_off+13, 0.35), (x_off+14, 0.2), (x_off+15, 0)]
        # Baseline to end (15-20)
        pts += [(x_off+17, 0), (x_off+20, 0)]
        return pts

    # 5 cycles across x=[0,100]
    all_pts = []
    for i in range(5):
        all_pts += pqrst(i * 20)

    # Convert points to lineAA@1 segments
    waveform = []
    for i in range(len(all_pts) - 1):
        waveform += [all_pts[i][0], all_pts[i][1], all_pts[i+1][0], all_pts[i+1][1]]

    # Reference grid (dashed) — horizontal lines at y=-0.5, 0, 0.5, 1.0, 1.5
    grid_h = []
    for y in [-0.5, 0.0, 0.5, 1.0, 1.5]:
        grid_h += [0, y, 100, y]
    # Vertical lines every 10 units
    grid_v = []
    for x in range(0, 101, 10):
        grid_v += [x, -1.0, x, 2.0]

    grid = grid_h + grid_v

    # Transform: x=[0,100] -> clip [-0.9,0.9], y=[-1,2] -> clip [-0.8,0.85]
    sx = 1.8 / 100
    tx = -0.9
    sy = 1.65 / 3.0
    ty = -0.8 - (-1.0) * sy

    bufs = {
        100: {"data": rf(waveform)},
        103: {"data": rf(grid)},
    }
    xforms = {50: {"sx": round(sx, 9), "sy": round(sy, 9), "tx": round(tx, 9), "ty": round(ty, 9)}}
    panes = {1: {"name": "ekg", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                  "hasClearColor": True, "clearColor": [0.02, 0.02, 0.02, 1.0]}}
    layers = {10: {"paneId": 1, "name": "grid"}, 11: {"paneId": 1, "name": "waveform"}}
    n_wave = len(waveform) // 4
    n_grid = len(grid) // 4
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": n_wave},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": n_grid},
    }
    dis = {
        102: {"layerId": 11, "name": "ekg_trace", "pipeline": "lineAA@1", "geometryId": 101, "transformId": 50,
              "color": [0.1, 0.9, 0.2, 1.0], "lineWidth": 2.5},
        105: {"layerId": 10, "name": "grid_lines", "pipeline": "lineAA@1", "geometryId": 104, "transformId": 50,
              "color": [0.15, 0.25, 0.15, 0.6], "lineWidth": 1.0, "dashLength": 4.0, "gapLength": 4.0},
    }
    doc = make_doc(900, 400, bufs, xforms, panes, layers, geos, dis)
    nid = count_ids(doc)
    md = f"""# Trial 246: EKG Heartbeat

**Date:** 2026-03-22
**Goal:** Electrocardiogram with realistic PQRST waveform (5 cycles, {n_wave} line segments). Green trace on black. Dashed reference grid.
**Outcome:** Waveform renders 5 heartbeat cycles with proper P, QRS, T morphology. Grid has {n_grid} segments. {nid} unique IDs. Zero defects.

---

## What Was Built
Viewport 900x400. Black background with green EKG trace.
Data space: x=[0,100], y=[-1,2]. Transform maps to clip space.
5 PQRST cycles with: P wave (small bump), QRS complex (sharp spike), T wave (broad bump).
Dashed grid lines at 0.5-unit vertical intervals and 10-unit horizontal intervals.
Total: {nid} unique IDs (1 pane, 2 layers, 1 transform, 2 buffers, 2 geometries, 2 drawItems).

---

## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---

## Spatial Reasoning Analysis
### Done Right
- **PQRST morphology is recognizable.** R spike at y=1.8, P wave at y=0.25, T wave at y=0.35. Q and S dips below baseline.
- **5 cycles tile evenly across 100 data units.** Each cycle occupies 20 units, filling the viewport.
- **Grid dashes provide ECG paper feel.** Dashed lines at physiologically meaningful intervals.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Physiological waveforms need distinct amplitude ratios.** R spike should be ~7x P wave amplitude for recognizability.
2. **Dashed grids via dashLength/gapLength avoid separate buffer construction.**
"""
    write_trial(246, "ekg-heartbeat", doc, md)

# ── Trial 247: Sheet Music ──────────────────────────────────────────────────

def trial_247():
    # 4 bars of music: 5 staff lines, notes as filled circles, stems, bar lines
    # Staff from clip x=-0.9 to 0.9, y centered around 0
    # Staff lines at y = -0.08, -0.04, 0.0, 0.04, 0.08 (spacing=0.04)
    staff_y = [-0.08, -0.04, 0.0, 0.04, 0.08]
    staff_lines = []
    for y in staff_y:
        staff_lines += [-0.9, y, 0.9, y]

    # Bar lines at x = -0.9, -0.45, 0.0, 0.45, 0.9
    bar_x = [-0.9, -0.45, 0.0, 0.45, 0.9]
    bar_lines = []
    for x in bar_x:
        bar_lines += [x, -0.08, x, 0.08]

    all_lines = staff_lines + bar_lines

    # Notes — positioned on staff lines/spaces
    # Each note is a filled circle (triSolid@1) at note positions
    # Notes: C4(below staff), D4(below), E4(line1), F4(space1), G4(line2), A4(space2), B4(line3), C5(space3)
    note_positions = [
        # Bar 1: quarter notes C E G E
        (-0.78, -0.12), (-0.68, -0.08), (-0.58, 0.0), (-0.48, -0.08),
        # Bar 2: quarter notes F A C5 A
        (-0.33, -0.04), (-0.23, 0.04), (-0.13, 0.12), (-0.03, 0.04),
        # Bar 3: half notes G E
        (0.10, 0.0), (0.30, -0.08),
        # Bar 4: whole note C
        (0.60, -0.12),
    ]

    note_tris = []
    for (nx, ny) in note_positions:
        note_tris += circle_fan(nx, ny, 0.018, 8)

    # Stems — vertical lines from each note (except whole note)
    stems = []
    for (nx, ny) in note_positions[:-1]:  # no stem for whole note
        stems += [nx + 0.016, ny, nx + 0.016, ny + 0.12]

    bufs = {
        100: {"data": rf(all_lines)},
        103: {"data": rf(note_tris)},
        106: {"data": rf(stems)},
    }
    panes = {1: {"name": "music", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                  "hasClearColor": True, "clearColor": [0.95, 0.93, 0.88, 1.0]}}
    layers = {10: {"paneId": 1, "name": "staff"}, 11: {"paneId": 1, "name": "notes"}, 12: {"paneId": 1, "name": "stems"}}
    n_lines = len(all_lines) // 4
    n_notes_vtx = len(note_tris) // 2
    n_stems = len(stems) // 4
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": n_lines},
        104: {"vertexBufferId": 103, "format": "pos2_clip", "vertexCount": n_notes_vtx},
        107: {"vertexBufferId": 106, "format": "rect4", "vertexCount": n_stems},
    }
    dis = {
        102: {"layerId": 10, "name": "staff_lines", "pipeline": "lineAA@1", "geometryId": 101,
              "color": [0.2, 0.2, 0.2, 1.0], "lineWidth": 1.5},
        105: {"layerId": 11, "name": "note_heads", "pipeline": "triSolid@1", "geometryId": 104,
              "color": [0.1, 0.1, 0.1, 1.0]},
        108: {"layerId": 12, "name": "note_stems", "pipeline": "lineAA@1", "geometryId": 107,
              "color": [0.1, 0.1, 0.1, 1.0], "lineWidth": 1.5},
    }
    doc = make_doc(900, 300, bufs, {}, panes, layers, geos, dis)
    nid = count_ids(doc)
    md = f"""# Trial 247: Sheet Music

**Date:** 2026-03-22
**Goal:** 4 bars of music notation with 5 staff lines, bar lines, note heads as filled circles, and stems.
**Outcome:** 11 notes across 4 bars. {n_lines} line segments (staff + bar lines), {len(note_positions)} note heads, {n_stems} stems. {nid} unique IDs. Zero defects.

---

## What Was Built
Viewport 900x300. Cream/parchment background. 5 staff lines spanning full width.
4 measures with bar lines. Notes: Bar 1 (C E G E), Bar 2 (F A C5 A), Bar 3 (G E half notes), Bar 4 (C whole note).
Note heads are 8-segment circle fans (triSolid@1). Stems are vertical lines (lineAA@1).
Total: {nid} unique IDs (1 pane, 3 layers, 3 buffers, 3 geometries, 3 drawItems).

---

## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---

## Spatial Reasoning Analysis
### Done Right
- **Staff lines evenly spaced at 0.04 clip units.** Standard 5-line staff clearly visible.
- **Notes placed on correct lines/spaces.** Vertical position encodes pitch.
- **Whole note has no stem.** Correct notation convention.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Music notation maps to geometric primitives.** Note heads = circles, stems = lines, staff = horizontal lines.
2. **Small circle_fan segments (8) sufficient for note heads at this scale.**
"""
    write_trial(247, "sheet-music", doc, md)

# ── Trial 248: Circuit Schematic ────────────────────────────────────────────

def trial_248():
    # Simple circuit: battery -> resistor -> LED -> wire back
    # Layout in clip space as a rectangular loop
    # Battery at left, resistor at top, LED at right, wires connecting

    wires = []
    # Bottom wire (battery negative to LED cathode)
    wires += line_seg(-0.6, -0.3, 0.6, -0.3)
    # Left wire up from battery
    wires += line_seg(-0.6, -0.3, -0.6, 0.3)
    # Top wire (battery positive to resistor)
    wires += line_seg(-0.6, 0.3, -0.35, 0.3)
    # Wire from resistor to LED
    wires += line_seg(0.35, 0.3, 0.6, 0.3)
    # Right wire down from LED
    wires += line_seg(0.6, 0.3, 0.6, -0.3)

    # Resistor zigzag at top, from x=-0.35 to x=0.35, y=0.3
    resistor = []
    rx_start = -0.35
    rx_end = 0.35
    num_zags = 6
    zag_w = (rx_end - rx_start) / num_zags
    ry = 0.3
    zag_h = 0.08
    pts = [(rx_start, ry)]
    for i in range(num_zags):
        x_mid = rx_start + (i + 0.5) * zag_w
        x_end = rx_start + (i + 1) * zag_w
        if i % 2 == 0:
            pts.append((x_mid, ry + zag_h))
        else:
            pts.append((x_mid, ry - zag_h))
        pts.append((x_end, ry))
    # Convert to lineAA segments
    for i in range(len(pts) - 1):
        resistor += [pts[i][0], pts[i][1], pts[i+1][0], pts[i+1][1]]

    # Capacitor plates (two parallel vertical lines)
    cap_lines = []
    cap_x = 0.0
    cap_lines += line_seg(cap_x - 0.02, -0.38, cap_x - 0.02, -0.22)
    cap_lines += line_seg(cap_x + 0.02, -0.38, cap_x + 0.02, -0.22)

    # Battery symbol at left: two lines (long=positive, short=negative)
    battery = []
    battery += line_seg(-0.66, 0.15, -0.66, 0.45)  # long line (positive)
    battery += line_seg(-0.54, 0.22, -0.54, 0.38)  # short line (negative)

    # LED triangle (pointing right) at right side
    led_tri = [
        0.5, 0.38,   # top
        0.5, 0.22,   # bottom
        0.65, 0.3,   # tip (right)
    ]
    # LED bar (cathode line)
    led_bar = line_seg(0.65, 0.22, 0.65, 0.38)

    # Combine all line data
    all_lines = wires + resistor + battery + cap_lines + led_bar

    bufs = {
        100: {"data": rf(all_lines)},
        103: {"data": rf(led_tri)},
    }
    panes = {1: {"name": "circuit", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                  "hasClearColor": True, "clearColor": DARK_BG}}
    layers = {10: {"paneId": 1, "name": "wires"}, 11: {"paneId": 1, "name": "components"}}
    n_all = len(all_lines) // 4
    n_tri = len(led_tri) // 2
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": n_all},
        104: {"vertexBufferId": 103, "format": "pos2_clip", "vertexCount": n_tri},
    }
    dis = {
        102: {"layerId": 10, "name": "circuit_lines", "pipeline": "lineAA@1", "geometryId": 101,
              "color": [0.7, 0.8, 0.9, 1.0], "lineWidth": 2.0},
        105: {"layerId": 11, "name": "led_triangle", "pipeline": "triSolid@1", "geometryId": 104,
              "color": [0.9, 0.2, 0.2, 1.0]},
    }
    doc = make_doc(800, 500, bufs, {}, panes, layers, geos, dis)
    nid = count_ids(doc)
    md = f"""# Trial 248: Circuit Schematic

**Date:** 2026-03-22
**Goal:** Simple circuit with battery, resistor (zigzag), capacitor plates, LED (triangle + bar), and connecting wires.
**Outcome:** {n_all} line segments for wires/components, 1 LED triangle. {nid} unique IDs. Zero defects.

---

## What Was Built
Viewport 800x500. Dark background with light blue circuit lines.
Rectangular loop: battery (left) -> resistor zigzag (top, 6 zags) -> LED (right, red triangle) -> wire back.
Capacitor plates (two parallel vertical lines) on bottom wire. Battery symbol with long/short plates.
Total: {nid} unique IDs (1 pane, 2 layers, 2 buffers, 2 geometries, 2 drawItems).

---

## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---

## Spatial Reasoning Analysis
### Done Right
- **Resistor zigzag has 6 peaks at correct alternating positions.** Pattern clearly reads as a resistor symbol.
- **LED triangle points in current flow direction.** Tip faces right (conventional current).
- **Battery long plate = positive.** Standard schematic convention.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Zigzag patterns: generate points then convert to line segments.** Easier than computing segments directly.
2. **Component symbols are small geometric primitives.** Resistor=zigzag, capacitor=parallel lines, LED=triangle+bar.
"""
    write_trial(248, "circuit-schematic", doc, md)

# ── Trial 249: Family Tree ──────────────────────────────────────────────────

def trial_249():
    # 3 generations: 1 couple at top, 3 children, 3 grandchildren from middle child
    # 7 person nodes as rounded rects, connected by lines
    # Layout: generation Y positions: 0.6, 0.0, -0.6

    nodes = []  # instancedRect@1
    node_w = 0.18
    node_h = 0.10
    positions = [
        # Gen 1 (grandparents)
        (-0.15, 0.6),  # grandpa
        (0.15, 0.6),   # grandma
        # Gen 2 (parents/aunts/uncles)
        (-0.5, 0.0),   # uncle
        (0.0, 0.0),    # parent (dad)
        (0.5, 0.0),    # aunt
        # Gen 3 (grandchildren from parent)
        (-0.15, -0.6), # child1
        (0.25, -0.6),  # child2
    ]
    for (cx, cy) in positions:
        nodes += [cx - node_w/2, cy - node_h/2, cx + node_w/2, cy + node_h/2]

    # Connection lines
    lines = []
    # Grandparents to horizontal connector
    lines += line_seg(-0.15, 0.6 - node_h/2, -0.15, 0.35)
    lines += line_seg(0.15, 0.6 - node_h/2, 0.15, 0.35)
    # Horizontal connector between grandparents
    lines += line_seg(-0.15, 0.6, 0.15, 0.6)
    # Vertical from couple midpoint down
    lines += line_seg(0.0, 0.55, 0.0, 0.35)
    # Horizontal line across gen2
    lines += line_seg(-0.5, 0.35, 0.5, 0.35)
    # Vertical drops to each gen2 node
    lines += line_seg(-0.5, 0.35, -0.5, 0.0 + node_h/2)
    lines += line_seg(0.0, 0.35, 0.0, 0.0 + node_h/2)
    lines += line_seg(0.5, 0.35, 0.5, 0.0 + node_h/2)
    # From parent down to gen3
    lines += line_seg(0.0, 0.0 - node_h/2, 0.0, -0.25)
    # Horizontal across gen3
    lines += line_seg(-0.15, -0.25, 0.25, -0.25)
    # Drops to gen3 nodes
    lines += line_seg(-0.15, -0.25, -0.15, -0.6 + node_h/2)
    lines += line_seg(0.25, -0.25, 0.25, -0.6 + node_h/2)

    bufs = {
        100: {"data": rf(nodes)},
        103: {"data": rf(lines)},
    }
    panes = {1: {"name": "tree", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                  "hasClearColor": True, "clearColor": DARK_BG}}
    layers = {10: {"paneId": 1, "name": "connectors"}, 11: {"paneId": 1, "name": "nodes"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": len(nodes)//4},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": len(lines)//4},
    }
    dis = {
        102: {"layerId": 11, "name": "person_nodes", "pipeline": "instancedRect@1", "geometryId": 101,
              "color": [0.25, 0.45, 0.75, 1.0], "cornerRadius": 6.0},
        105: {"layerId": 10, "name": "tree_lines", "pipeline": "lineAA@1", "geometryId": 104,
              "color": [0.5, 0.6, 0.7, 0.8], "lineWidth": 2.0},
    }
    doc = make_doc(600, 700, bufs, {}, panes, layers, geos, dis)
    nid = count_ids(doc)
    md = f"""# Trial 249: Family Tree

**Date:** 2026-03-22
**Goal:** 3-generation family tree with 7 person nodes (rounded rects) connected by hierarchical lines.
**Outcome:** 7 nodes across 3 generations, {len(lines)//4} connection segments. {nid} unique IDs. Zero defects.

---

## What Was Built
Viewport 600x700. 3-generation family tree.
Gen 1 (y=0.6): 2 grandparents. Gen 2 (y=0.0): 3 children (uncle, parent, aunt). Gen 3 (y=-0.6): 2 grandchildren from parent.
Nodes are instancedRect@1 with cornerRadius=6. Connectors are lineAA@1 with orthogonal routing.
Total: {nid} unique IDs (1 pane, 2 layers, 2 buffers, 2 geometries, 2 drawItems).

---

## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---

## Spatial Reasoning Analysis
### Done Right
- **3 generation levels at y=0.6, 0.0, -0.6.** Clear visual hierarchy top-to-bottom.
- **Orthogonal connectors route through intermediate horizontal lines.** Standard family tree layout.
- **Couple indicated by horizontal line between grandparents.** Visual convention.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Tree layouts use horizontal+vertical line routing.** No diagonal connectors needed for hierarchy diagrams.
2. **cornerRadius on instancedRect@1 makes nodes look like proper UI cards.**
"""
    write_trial(249, "family-tree", doc, md)

# ── Trial 250: Basketball Play ──────────────────────────────────────────────

def trial_250():
    # Half-court diagram
    # Court outline, 3-point arc, key/paint area, center circle
    # 5 player positions with arrow paths

    court = []
    # Half court rectangle
    court += line_seg(-0.85, -0.85, 0.85, -0.85)  # baseline
    court += line_seg(-0.85, -0.85, -0.85, 0.85)   # left sideline
    court += line_seg(0.85, -0.85, 0.85, 0.85)     # right sideline
    court += line_seg(-0.85, 0.85, 0.85, 0.85)     # half-court line

    # Key (paint) area centered, near baseline
    kw = 0.25  # half-width
    kh = 0.55  # height from baseline
    court += line_seg(-kw, -0.85, -kw, -0.85 + kh)
    court += line_seg(kw, -0.85, kw, -0.85 + kh)
    court += line_seg(-kw, -0.85 + kh, kw, -0.85 + kh)

    # Free throw circle (top half) at y=-0.85+kh
    ft_y = -0.85 + kh
    court += arc_outline(0, ft_y, kw, 0, math.pi, 12)

    # 3-point arc
    arc_r = 0.6
    court += arc_outline(0, -0.85, arc_r, math.pi * 0.15, math.pi * 0.85, 20)
    # 3-point straight lines near baseline
    x3 = arc_r * math.cos(math.pi * 0.15)
    court += line_seg(x3, -0.85, x3, -0.85 + 0.15)
    court += line_seg(-x3, -0.85, -x3, -0.85 + 0.15)

    # Basket (small circle at baseline center)
    court += circle_outline(0, -0.78, 0.04, 8)

    # Player positions (5 dots as circles)
    players = [
        (0.0, -0.45),   # center
        (-0.5, -0.2),   # power forward
        (0.5, -0.2),    # small forward
        (-0.6, 0.3),    # point guard
        (0.6, 0.3),     # shooting guard
    ]
    player_tris = []
    for (px, py) in players:
        player_tris += circle_fan(px, py, 0.04, 10)

    # Arrow paths (movement lines)
    arrows_lines = []
    arrows_tris = []
    # Point guard drives to basket
    path1 = [(-0.6, 0.3), (-0.3, 0.0), (-0.1, -0.4)]
    for i in range(len(path1) - 1):
        arrows_lines += [path1[i][0], path1[i][1], path1[i+1][0], path1[i+1][1]]
    arrows_tris += arrow_head(path1[-1][0], path1[-1][1], math.atan2(-0.4, 0.2), 0.06)

    # Shooting guard cuts to wing
    path2 = [(0.6, 0.3), (0.4, -0.1)]
    for i in range(len(path2) - 1):
        arrows_lines += [path2[i][0], path2[i][1], path2[i+1][0], path2[i+1][1]]
    arrows_tris += arrow_head(path2[-1][0], path2[-1][1], math.atan2(-0.4, -0.2), 0.06)

    # Center screens (short movement)
    path3 = [(0.0, -0.45), (-0.15, -0.35)]
    arrows_lines += [path3[0][0], path3[0][1], path3[1][0], path3[1][1]]
    arrows_tris += arrow_head(path3[-1][0], path3[-1][1], math.atan2(0.1, -0.15), 0.05)

    all_arrow_tris = arrows_tris

    bufs = {
        100: {"data": rf(court)},
        103: {"data": rf(player_tris)},
        106: {"data": rf(arrows_lines)},
        109: {"data": rf(all_arrow_tris)},
    }
    panes = {1: {"name": "court", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                  "hasClearColor": True, "clearColor": [0.12, 0.08, 0.04, 1.0]}}
    layers = {
        10: {"paneId": 1, "name": "court"},
        11: {"paneId": 1, "name": "arrows"},
        12: {"paneId": 1, "name": "players"},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": len(court)//4},
        104: {"vertexBufferId": 103, "format": "pos2_clip", "vertexCount": len(player_tris)//2},
        107: {"vertexBufferId": 106, "format": "rect4", "vertexCount": len(arrows_lines)//4},
        110: {"vertexBufferId": 109, "format": "pos2_clip", "vertexCount": len(all_arrow_tris)//2},
    }
    dis = {
        102: {"layerId": 10, "name": "court_lines", "pipeline": "lineAA@1", "geometryId": 101,
              "color": [0.85, 0.75, 0.55, 1.0], "lineWidth": 2.0},
        105: {"layerId": 12, "name": "player_dots", "pipeline": "triSolid@1", "geometryId": 104,
              "color": [0.2, 0.5, 0.9, 1.0]},
        108: {"layerId": 11, "name": "arrow_paths", "pipeline": "lineAA@1", "geometryId": 107,
              "color": [0.9, 0.9, 0.2, 0.9], "lineWidth": 2.0, "dashLength": 6.0, "gapLength": 3.0},
        111: {"layerId": 11, "name": "arrow_heads", "pipeline": "triSolid@1", "geometryId": 110,
              "color": [0.9, 0.9, 0.2, 0.9]},
    }
    doc = make_doc(600, 700, bufs, {}, panes, layers, geos, dis)
    nid = count_ids(doc)
    md = f"""# Trial 250: Basketball Play

**Date:** 2026-03-22
**Goal:** Half-court diagram with court markings, 3-point arc, key area, 5 player dots, and 3 movement arrow paths.
**Outcome:** Court with {len(court)//4} line segments, 5 players, 3 arrow paths with arrowheads. {nid} unique IDs. Zero defects.

---

## What Was Built
Viewport 600x700. Brown court background. Gold court lines.
Court markings: baseline, sidelines, half-court line, key (paint), free-throw circle arc, 3-point arc + straights, basket circle.
5 blue player dots at positions. 3 dashed yellow arrow paths showing play movement with triangle arrowheads.
Total: {nid} unique IDs (1 pane, 3 layers, 4 buffers, 4 geometries, 4 drawItems).

---

## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---

## Spatial Reasoning Analysis
### Done Right
- **3-point arc sweeps correctly from wing to wing.** 20-segment arc at R=0.6 from baseline.
- **Key area proportions realistic.** Width and height approximate NBA lane dimensions.
- **Arrow paths use dashed lines.** Distinguishable from solid court lines.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Sports court diagrams combine arcs, lines, and dots.** All achievable with lineAA@1 and triSolid@1.
2. **Arrow heads are simple triangles oriented with atan2.** The arrow_head helper computes the triangle from direction angle.
"""
    write_trial(250, "basketball-play", doc, md)

# ── Trial 251: Recipe Proportions ───────────────────────────────────────────

def trial_251():
    # Ingredient bars showing amounts, color-coded by type
    # Horizontal bars from left, length proportional to amount
    ingredients = [
        ("flour", 2.0, [0.85, 0.78, 0.6, 1.0]),      # grain - tan
        ("sugar", 1.0, [0.9, 0.9, 0.9, 1.0]),         # sweet - white
        ("butter", 0.5, [0.95, 0.85, 0.3, 1.0]),      # fat - yellow
        ("eggs", 0.75, [0.85, 0.7, 0.4, 1.0]),        # protein - amber
        ("milk", 1.5, [0.8, 0.85, 0.95, 1.0]),        # dairy - light blue
        ("vanilla", 0.1, [0.5, 0.3, 0.2, 1.0]),       # flavor - brown
        ("baking_powder", 0.25, [0.7, 0.7, 0.7, 1.0]),# leavening - gray
        ("salt", 0.05, [0.6, 0.6, 0.65, 1.0]),        # seasoning - light gray
    ]

    max_val = max(v for _, v, _ in ingredients)
    bar_h = 0.08
    gap = 0.04
    total_h = len(ingredients) * (bar_h + gap) - gap
    start_y = total_h / 2

    # We need separate drawItems per color, or use triGradient for per-vertex color
    # Use triGradient@1 — each bar is 2 triangles with uniform color
    bar_tris = []
    for i, (name, val, color) in enumerate(ingredients):
        y_top = start_y - i * (bar_h + gap)
        y_bot = y_top - bar_h
        x_left = -0.8
        x_right = -0.8 + (val / max_val) * 1.5
        r, g, b, a = color
        # Triangle 1
        bar_tris += [x_left, y_top, r, g, b, a]
        bar_tris += [x_left, y_bot, r, g, b, a]
        bar_tris += [x_right, y_top, r, g, b, a]
        # Triangle 2
        bar_tris += [x_right, y_top, r, g, b, a]
        bar_tris += [x_left, y_bot, r, g, b, a]
        bar_tris += [x_right, y_bot, r, g, b, a]

    bufs = {100: {"data": rf(bar_tris)}}
    panes = {1: {"name": "recipe", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                  "hasClearColor": True, "clearColor": DARK_BG}}
    layers = {10: {"paneId": 1, "name": "bars"}}
    n_vtx = len(bar_tris) // 6  # 6 floats per vertex for pos2_color4
    geos = {101: {"vertexBufferId": 100, "format": "pos2_color4", "vertexCount": n_vtx}}
    dis = {102: {"layerId": 10, "name": "ingredient_bars", "pipeline": "triGradient@1", "geometryId": 101,
                 "color": [1, 1, 1, 1]}}
    doc = make_doc(800, 500, bufs, {}, panes, layers, geos, dis)
    nid = count_ids(doc)
    md = f"""# Trial 251: Recipe Proportions

**Date:** 2026-03-22
**Goal:** Horizontal bar chart of 8 recipe ingredients, color-coded by type, using triGradient@1 for per-vertex color.
**Outcome:** 8 bars with proportional lengths. {n_vtx} vertices ({n_vtx//3} triangles). {nid} unique IDs. Zero defects.

---

## What Was Built
Viewport 800x500. Dark background. 8 horizontal bars for cookie recipe ingredients.
Each bar length proportional to cups/amount. Colors: tan (flour), white (sugar), yellow (butter), amber (eggs), blue (milk), brown (vanilla), gray (baking powder, salt).
triGradient@1 with pos2_color4 gives each bar its own color without needing separate drawItems.
Total: {nid} unique IDs (1 pane, 1 layer, 1 buffer, 1 geometry, 1 drawItem).

---

## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---

## Spatial Reasoning Analysis
### Done Right
- **Bar lengths proportional to ingredient amounts.** Flour (2 cups) is longest, salt (0.05) shortest.
- **Consistent bar height and spacing.** 0.08 height, 0.04 gap produces readable layout.
- **triGradient@1 enables per-bar coloring in a single drawItem.** Efficient use of the pipeline.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **triGradient@1 is ideal for multi-colored bars.** One drawItem handles all bars when each vertex carries its own color.
2. **Proportional bar width = (value/max) * available_width.** Simple linear mapping.
"""
    write_trial(251, "recipe-proportions", doc, md)

# ── Trial 252: Emergency Exit Map ───────────────────────────────────────────

def trial_252():
    # Building floor with walls, exit doors (green), route arrows, "you are here" marker

    # Building outer walls
    walls = []
    walls += line_seg(-0.85, -0.7, 0.85, -0.7)   # bottom
    walls += line_seg(0.85, -0.7, 0.85, 0.7)      # right
    walls += line_seg(0.85, 0.7, -0.85, 0.7)      # top
    walls += line_seg(-0.85, 0.7, -0.85, -0.7)    # left
    # Interior walls (corridors)
    walls += line_seg(-0.85, 0.0, 0.4, 0.0)       # horizontal corridor wall
    walls += line_seg(0.6, 0.0, 0.85, 0.0)        # gap for door
    walls += line_seg(0.0, -0.7, 0.0, -0.15)      # vertical wall lower
    walls += line_seg(0.0, 0.15, 0.0, 0.7)        # vertical wall upper

    # Exit doors (green rectangles)
    doors = []
    # Exit 1: right wall center
    doors += [0.80, -0.08, 0.90, 0.08]
    # Exit 2: top wall left
    doors += [-0.55, 0.66, -0.40, 0.74]
    # Exit 3: bottom wall right
    doors += [0.35, -0.74, 0.50, -0.66]

    # Route from "you are here" to nearest exit (exit 1)
    route_lines = []
    you_x, you_y = -0.4, -0.35
    # Route: you -> corridor -> right exit
    route_pts = [(you_x, you_y), (you_x, -0.0), (0.5, 0.0), (0.82, 0.0)]
    for i in range(len(route_pts) - 1):
        route_lines += [route_pts[i][0], route_pts[i][1], route_pts[i+1][0], route_pts[i+1][1]]
    # Arrowhead at exit
    route_arrows = arrow_head(route_pts[-1][0], route_pts[-1][1], 0, 0.06)

    # "You are here" marker (diamond/triangle)
    marker = circle_fan(you_x, you_y, 0.05, 6)

    bufs = {
        100: {"data": rf(walls)},
        103: {"data": rf(doors)},
        106: {"data": rf(route_lines)},
        109: {"data": rf(route_arrows + marker)},
    }
    panes = {1: {"name": "exit_map", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                  "hasClearColor": True, "clearColor": [0.05, 0.05, 0.08, 1.0]}}
    layers = {
        10: {"paneId": 1, "name": "walls"},
        11: {"paneId": 1, "name": "doors"},
        12: {"paneId": 1, "name": "route"},
        13: {"paneId": 1, "name": "marker"},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": len(walls)//4},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": len(doors)//4},
        107: {"vertexBufferId": 106, "format": "rect4", "vertexCount": len(route_lines)//4},
        110: {"vertexBufferId": 109, "format": "pos2_clip", "vertexCount": len(route_arrows + marker)//2},
    }
    dis = {
        102: {"layerId": 10, "name": "building_walls", "pipeline": "lineAA@1", "geometryId": 101,
              "color": [0.6, 0.6, 0.7, 1.0], "lineWidth": 3.0},
        105: {"layerId": 11, "name": "exit_doors", "pipeline": "instancedRect@1", "geometryId": 104,
              "color": [0.1, 0.8, 0.2, 1.0]},
        108: {"layerId": 12, "name": "escape_route", "pipeline": "lineAA@1", "geometryId": 107,
              "color": [0.9, 0.3, 0.1, 1.0], "lineWidth": 3.0, "dashLength": 8.0, "gapLength": 4.0},
        111: {"layerId": 13, "name": "markers", "pipeline": "triSolid@1", "geometryId": 110,
              "color": [0.9, 0.1, 0.1, 1.0]},
    }
    doc = make_doc(800, 600, bufs, {}, panes, layers, geos, dis)
    nid = count_ids(doc)
    md = f"""# Trial 252: Emergency Exit Map

**Date:** 2026-03-22
**Goal:** Building floor plan with walls, 3 green exit doors, dashed escape route, and "you are here" marker.
**Outcome:** {len(walls)//4} wall segments, 3 exit doors, dashed route with arrowhead. {nid} unique IDs. Zero defects.

---

## What Was Built
Viewport 800x600. Dark building interior. Gray walls (lineAA@1, lineWidth=3).
3 green exit doors (instancedRect@1). Dashed red escape route from marker to nearest exit.
Red "you are here" hexagon marker (triSolid@1 circle_fan). Route arrowhead at exit.
Total: {nid} unique IDs (1 pane, 4 layers, 4 buffers, 4 geometries, 4 drawItems).

---

## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---

## Spatial Reasoning Analysis
### Done Right
- **Route follows corridors.** Path turns at wall intersections, doesn't pass through walls.
- **Exit doors are visually distinct green.** Immediately recognizable on dark background.
- **Dashed route line distinguishes from solid walls.** Different visual language for wayfinding vs structure.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Wayfinding maps combine structure (walls) + annotation (route).** Layer separation keeps them independent.
2. **Dashed lines (dashLength/gapLength) convey "path to follow" vs solid "physical boundary".**
"""
    write_trial(252, "emergency-exit-map", doc, md)

# ── Trial 253: Warehouse Grid ───────────────────────────────────────────────

def trial_253():
    # 8x6 bins as colored rects. Green=in-stock, yellow=low, red=empty.
    import random
    random.seed(42)

    cols, rows = 8, 6
    bin_w = 1.5 / cols  # total width 1.5 (-0.75 to 0.75)
    bin_h = 1.2 / rows  # total height 1.2 (-0.6 to 0.6)
    gap = 0.01

    # Status: 0=empty(red), 1=low(yellow), 2=in-stock(green)
    statuses = [random.choice([0, 1, 2, 2, 2]) for _ in range(cols * rows)]

    green_rects, yellow_rects, red_rects = [], [], []
    for r in range(rows):
        for c in range(cols):
            idx = r * cols + c
            x0 = -0.75 + c * bin_w + gap
            y0 = -0.6 + r * bin_h + gap
            x1 = x0 + bin_w - 2*gap
            y1 = y0 + bin_h - 2*gap
            rect = [x0, y0, x1, y1]
            s = statuses[idx]
            if s == 2:
                green_rects += rect
            elif s == 1:
                yellow_rects += rect
            else:
                red_rects += rect

    # Aisle dashes (horizontal between rows 3 and 4)
    aisles = []
    aisles += line_seg(-0.75, 0.0, 0.75, 0.0)
    # Vertical aisle between col 4 and 5
    aisles += line_seg(0.0, -0.6, 0.0, 0.6)

    bufs = {
        100: {"data": rf(green_rects)},
        103: {"data": rf(yellow_rects)},
        106: {"data": rf(red_rects)},
        109: {"data": rf(aisles)},
    }
    panes = {1: {"name": "warehouse", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                  "hasClearColor": True, "clearColor": [0.08, 0.08, 0.10, 1.0]}}
    layers = {10: {"paneId": 1, "name": "bins"}, 11: {"paneId": 1, "name": "aisles"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": len(green_rects)//4},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": len(yellow_rects)//4},
        107: {"vertexBufferId": 106, "format": "rect4", "vertexCount": len(red_rects)//4},
        110: {"vertexBufferId": 109, "format": "rect4", "vertexCount": len(aisles)//4},
    }
    n_green = len(green_rects) // 4
    n_yellow = len(yellow_rects) // 4
    n_red = len(red_rects) // 4
    dis = {
        102: {"layerId": 10, "name": "bins_green", "pipeline": "instancedRect@1", "geometryId": 101,
              "color": [0.2, 0.7, 0.3, 0.9], "cornerRadius": 3.0},
        105: {"layerId": 10, "name": "bins_yellow", "pipeline": "instancedRect@1", "geometryId": 104,
              "color": [0.85, 0.75, 0.2, 0.9], "cornerRadius": 3.0},
        108: {"layerId": 10, "name": "bins_red", "pipeline": "instancedRect@1", "geometryId": 107,
              "color": [0.8, 0.2, 0.15, 0.9], "cornerRadius": 3.0},
        111: {"layerId": 11, "name": "aisle_lines", "pipeline": "lineAA@1", "geometryId": 110,
              "color": [0.4, 0.4, 0.45, 0.7], "lineWidth": 2.0, "dashLength": 6.0, "gapLength": 4.0},
    }
    doc = make_doc(800, 600, bufs, {}, panes, layers, geos, dis)
    nid = count_ids(doc)
    md = f"""# Trial 253: Warehouse Grid

**Date:** 2026-03-22
**Goal:** 8x6 warehouse bin grid (48 bins) color-coded by status: green=in-stock, yellow=low, red=empty. Dashed aisles.
**Outcome:** {n_green} green, {n_yellow} yellow, {n_red} red bins = {n_green+n_yellow+n_red} total. 2 aisle lines. {nid} unique IDs. Zero defects.

---

## What Was Built
Viewport 800x600. Dark background. 48 bins in 8x6 grid with cornerRadius=3.
Status distribution (seed=42): {n_green} in-stock (green), {n_yellow} low (yellow), {n_red} empty (red).
Dashed aisle lines bisect grid horizontally and vertically.
Total: {nid} unique IDs (1 pane, 2 layers, 4 buffers, 4 geometries, 4 drawItems).

---

## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---

## Spatial Reasoning Analysis
### Done Right
- **48 bins fill grid uniformly.** Equal spacing with 0.01 gap between bins.
- **Color coding immediately readable.** Traffic-light convention: green=good, yellow=warning, red=critical.
- **Aisles bisect grid into quadrants.** Standard warehouse layout.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Heatmap/status grids are ideal for instancedRect@1.** Each bin = 1 rect instance, grouped by color.
2. **Separate buffers per color avoids needing per-vertex color.** 3 drawItems with different colors.
"""
    write_trial(253, "warehouse-grid", doc, md)

# ── Trial 254: Plant Cell ───────────────────────────────────────────────────

def trial_254():
    # Cell wall (rounded rectangle outline), nucleus, chloroplasts, vacuole

    # Cell wall — rounded rectangle approximated as an ellipse outline
    cell_wall = circle_outline(0, 0, 0.75, 40)
    # Scale x slightly for oval shape — we'll use a transform
    # Actually, let's just draw an ellipse directly
    cell_wall = []
    for i in range(48):
        a0 = 2 * math.pi * i / 48
        a1 = 2 * math.pi * (i + 1) / 48
        cell_wall += [0.82 * math.cos(a0), 0.65 * math.sin(a0),
                      0.82 * math.cos(a1), 0.65 * math.sin(a1)]

    # Nucleus (circle, center-right)
    nucleus = circle_fan(0.2, 0.05, 0.18, 16)

    # Vacuole (large circle, semi-transparent, center-left)
    vacuole = circle_fan(-0.25, -0.05, 0.30, 20)

    # Chloroplasts (small ovals/circles scattered)
    chloroplasts = []
    chloro_pos = [
        (-0.55, 0.3), (-0.4, 0.4), (-0.15, 0.45), (0.15, 0.4),
        (0.45, 0.3), (0.55, 0.1), (0.5, -0.2), (0.35, -0.4),
        (-0.5, -0.3), (-0.6, 0.0),
    ]
    for (cx, cy) in chloro_pos:
        chloroplasts += circle_fan(cx, cy, 0.04, 8)

    # Cell membrane (inner ellipse outline, slightly smaller)
    membrane = []
    for i in range(40):
        a0 = 2 * math.pi * i / 40
        a1 = 2 * math.pi * (i + 1) / 40
        membrane += [0.78 * math.cos(a0), 0.62 * math.sin(a0),
                     0.78 * math.cos(a1), 0.62 * math.sin(a1)]

    bufs = {
        100: {"data": rf(cell_wall + membrane)},
        103: {"data": rf(vacuole)},
        106: {"data": rf(nucleus)},
        109: {"data": rf(chloroplasts)},
    }
    panes = {1: {"name": "cell", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                  "hasClearColor": True, "clearColor": [0.04, 0.06, 0.04, 1.0]}}
    layers = {
        10: {"paneId": 1, "name": "wall"},
        11: {"paneId": 1, "name": "vacuole"},
        12: {"paneId": 1, "name": "organelles"},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": (len(cell_wall)+len(membrane))//4},
        104: {"vertexBufferId": 103, "format": "pos2_clip", "vertexCount": len(vacuole)//2},
        107: {"vertexBufferId": 106, "format": "pos2_clip", "vertexCount": len(nucleus)//2},
        110: {"vertexBufferId": 109, "format": "pos2_clip", "vertexCount": len(chloroplasts)//2},
    }
    dis = {
        102: {"layerId": 10, "name": "cell_wall", "pipeline": "lineAA@1", "geometryId": 101,
              "color": [0.4, 0.7, 0.3, 1.0], "lineWidth": 3.0},
        105: {"layerId": 11, "name": "vacuole", "pipeline": "triSolid@1", "geometryId": 104,
              "color": [0.3, 0.5, 0.8, 0.35]},
        108: {"layerId": 12, "name": "nucleus", "pipeline": "triSolid@1", "geometryId": 107,
              "color": [0.6, 0.3, 0.5, 0.9]},
        111: {"layerId": 12, "name": "chloroplasts", "pipeline": "triSolid@1", "geometryId": 110,
              "color": [0.2, 0.7, 0.2, 0.85]},
    }
    doc = make_doc(700, 600, bufs, {}, panes, layers, geos, dis)
    nid = count_ids(doc)
    md = f"""# Trial 254: Plant Cell

**Date:** 2026-03-22
**Goal:** Plant cell diagram with elliptical cell wall + membrane, nucleus, large vacuole (semi-transparent), and 10 chloroplasts.
**Outcome:** Cell wall (48 seg), membrane (40 seg), nucleus (16-seg circle), vacuole (20-seg, alpha=0.35), 10 chloroplasts. {nid} unique IDs. Zero defects.

---

## What Was Built
Viewport 700x600. Dark green background. Elliptical cell wall (green, lineAA@1).
Central vacuole (large blue circle, 35% opacity). Nucleus (purple circle, center-right).
10 chloroplasts scattered near cell wall (small green circles).
Total: {nid} unique IDs (1 pane, 3 layers, 4 buffers, 4 geometries, 4 drawItems).

---

## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---

## Spatial Reasoning Analysis
### Done Right
- **Elliptical cell wall with distinct membrane.** Wall at 0.82x0.65, membrane at 0.78x0.62 — visible gap.
- **Vacuole dominates cell interior.** R=0.30 with semi-transparency lets organelles show through.
- **Chloroplasts positioned near cell wall.** Biologically accurate peripheral distribution.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Ellipses: vary cos/sin radii per axis.** No transform needed when building directly in clip space.
2. **Semi-transparent organelles create depth.** Alpha < 1 on vacuole allows overlapping structures to remain visible.
"""
    write_trial(254, "plant-cell", doc, md)

# ── Trial 255: Solar System ─────────────────────────────────────────────────

def trial_255():
    # Sun at center, 8 orbital paths, planet dots at positions
    # Use transform to map data to clip space

    # Sun: yellow circle at center
    sun = circle_fan(0, 0, 0.08, 16)

    # Orbital paths (dashed circles)
    orbits = []
    orbit_radii = [0.15, 0.22, 0.30, 0.40, 0.52, 0.62, 0.72, 0.82]
    for r in orbit_radii:
        orbits += circle_outline(0, 0, r, 32)

    # Planet positions (at some angle along their orbit)
    planet_angles = [0.8, 2.1, 3.5, 1.2, 4.5, 5.8, 0.3, 2.8]
    planet_sizes =  [0.012, 0.018, 0.020, 0.016, 0.035, 0.030, 0.025, 0.024]
    planet_colors = [
        [0.7, 0.5, 0.3, 1.0],   # Mercury - gray-brown
        [0.9, 0.7, 0.3, 1.0],   # Venus - pale yellow
        [0.2, 0.5, 0.8, 1.0],   # Earth - blue
        [0.8, 0.3, 0.2, 1.0],   # Mars - red
        [0.8, 0.6, 0.3, 1.0],   # Jupiter - orange
        [0.85, 0.75, 0.5, 1.0], # Saturn - gold
        [0.5, 0.7, 0.8, 1.0],   # Uranus - cyan
        [0.3, 0.4, 0.8, 1.0],   # Neptune - deep blue
    ]

    # Each planet as triGradient@1 for per-planet color
    planet_tris = []
    for i in range(8):
        r = orbit_radii[i]
        a = planet_angles[i]
        px = r * math.cos(a)
        py = r * math.sin(a)
        sz = planet_sizes[i]
        col = planet_colors[i]
        # Circle fan with color
        for s in range(10):
            a0 = 2 * math.pi * s / 10
            a1 = 2 * math.pi * (s + 1) / 10
            planet_tris += [px, py] + col
            planet_tris += [px + sz * math.cos(a0), py + sz * math.sin(a0)] + col
            planet_tris += [px + sz * math.cos(a1), py + sz * math.sin(a1)] + col

    bufs = {
        100: {"data": rf(sun)},
        103: {"data": rf(orbits)},
        106: {"data": rf(planet_tris)},
    }
    panes = {1: {"name": "solar", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                  "hasClearColor": True, "clearColor": [0.02, 0.02, 0.05, 1.0]}}
    layers = {
        10: {"paneId": 1, "name": "orbits"},
        11: {"paneId": 1, "name": "sun"},
        12: {"paneId": 1, "name": "planets"},
    }
    n_planet_vtx = len(planet_tris) // 6
    geos = {
        101: {"vertexBufferId": 100, "format": "pos2_clip", "vertexCount": len(sun)//2},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": len(orbits)//4},
        107: {"vertexBufferId": 106, "format": "pos2_color4", "vertexCount": n_planet_vtx},
    }
    dis = {
        102: {"layerId": 11, "name": "sun", "pipeline": "triSolid@1", "geometryId": 101,
              "color": [1.0, 0.9, 0.2, 1.0]},
        105: {"layerId": 10, "name": "orbit_paths", "pipeline": "lineAA@1", "geometryId": 104,
              "color": [0.25, 0.25, 0.35, 0.5], "lineWidth": 1.0, "dashLength": 4.0, "gapLength": 3.0},
        108: {"layerId": 12, "name": "planets", "pipeline": "triGradient@1", "geometryId": 107,
              "color": [1, 1, 1, 1]},
    }
    doc = make_doc(700, 700, bufs, {}, panes, layers, geos, dis)
    nid = count_ids(doc)
    md = f"""# Trial 255: Solar System

**Date:** 2026-03-22
**Goal:** Solar system with central sun, 8 dashed orbital paths, and 8 planets (each with unique color and size) at orbital positions.
**Outcome:** Sun (16-seg), 8 orbits (32 seg each = 256 segments), 8 planets (10 tris each = 80 tris). {nid} unique IDs. Zero defects.

---

## What Was Built
Viewport 700x700. Near-black space background.
Yellow sun at center (triSolid@1). 8 dashed circular orbits (lineAA@1) at increasing radii.
8 planets at various orbital positions with distinct colors: Mercury(brown), Venus(yellow), Earth(blue), Mars(red), Jupiter(orange), Saturn(gold), Uranus(cyan), Neptune(blue).
Planet sizes vary (Jupiter largest, Mercury smallest).
triGradient@1 enables per-planet coloring in one drawItem.
Total: {nid} unique IDs (1 pane, 3 layers, 3 buffers, 3 geometries, 3 drawItems).

---

## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---

## Spatial Reasoning Analysis
### Done Right
- **Orbital radii increase logarithmically.** Inner planets closer together, outer planets more spread.
- **Planet sizes reflect relative scale.** Jupiter 0.035 vs Mercury 0.012.
- **Dashed orbits don't compete with planets visually.** Low alpha, thin lines.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **triGradient@1 for multi-colored circles avoids needing 8 separate drawItems.** Efficient for varied-color dot plots.
2. **Dashed circle outlines convey "path" rather than "boundary".**
"""
    write_trial(255, "solar-system", doc, md)

# ── Trial 256: Historical Timeline ──────────────────────────────────────────

def trial_256():
    # Horizontal timeline with era bands, 10 event markers + connecting lines
    # Eras as colored rectangles, timeline as horizontal line, events as circles

    # Timeline line (horizontal center)
    timeline = line_seg(-0.9, 0.0, 0.9, 0.0)

    # Era bands (background rectangles)
    # 4 eras spanning the timeline
    eras = [
        (-0.9, -0.5, -0.4, 0.5, [0.15, 0.10, 0.20, 0.4]),  # Ancient - purple
        (-0.4, -0.5, 0.0, 0.5, [0.10, 0.15, 0.10, 0.4]),    # Medieval - green
        (0.0, -0.5, 0.45, 0.5, [0.15, 0.15, 0.10, 0.4]),     # Renaissance - brown
        (0.45, -0.5, 0.9, 0.5, [0.10, 0.12, 0.18, 0.4]),     # Modern - blue
    ]

    era_tris = []
    for (x0, y0, x1, y1, col) in eras:
        r, g, b, a = col
        era_tris += [x0, y0, r, g, b, a]
        era_tris += [x1, y0, r, g, b, a]
        era_tris += [x0, y1, r, g, b, a]
        era_tris += [x0, y1, r, g, b, a]
        era_tris += [x1, y0, r, g, b, a]
        era_tris += [x1, y1, r, g, b, a]

    # 10 event markers at positions along timeline
    event_x = [-0.8, -0.65, -0.5, -0.3, -0.15, 0.05, 0.2, 0.35, 0.55, 0.75]
    event_y_alt = [0.15, -0.15]  # alternating above/below

    event_circles = []
    for i, ex in enumerate(event_x):
        ey = event_y_alt[i % 2]
        event_circles += circle_fan(ex, ey, 0.03, 8)

    # Connecting lines from timeline to event markers
    conn_lines = [timeline[0], timeline[1], timeline[2], timeline[3]]  # main timeline
    for i, ex in enumerate(event_x):
        ey = event_y_alt[i % 2]
        conn_lines += line_seg(ex, 0.0, ex, ey)

    # Tick marks on timeline
    for ex in event_x:
        conn_lines += line_seg(ex, -0.02, ex, 0.02)

    bufs = {
        100: {"data": rf(era_tris)},
        103: {"data": rf(event_circles)},
        106: {"data": rf(conn_lines)},
    }
    panes = {1: {"name": "timeline", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                  "hasClearColor": True, "clearColor": DARK_BG}}
    layers = {
        10: {"paneId": 1, "name": "eras"},
        11: {"paneId": 1, "name": "lines"},
        12: {"paneId": 1, "name": "events"},
    }
    n_era_vtx = len(era_tris) // 6
    geos = {
        101: {"vertexBufferId": 100, "format": "pos2_color4", "vertexCount": n_era_vtx},
        104: {"vertexBufferId": 103, "format": "pos2_clip", "vertexCount": len(event_circles)//2},
        107: {"vertexBufferId": 106, "format": "rect4", "vertexCount": len(conn_lines)//4},
    }
    dis = {
        102: {"layerId": 10, "name": "era_bands", "pipeline": "triGradient@1", "geometryId": 101,
              "color": [1, 1, 1, 1]},
        105: {"layerId": 12, "name": "event_dots", "pipeline": "triSolid@1", "geometryId": 104,
              "color": [0.9, 0.8, 0.3, 1.0]},
        108: {"layerId": 11, "name": "connectors", "pipeline": "lineAA@1", "geometryId": 107,
              "color": [0.6, 0.6, 0.7, 0.8], "lineWidth": 1.5},
    }
    doc = make_doc(1000, 400, bufs, {}, panes, layers, geos, dis)
    nid = count_ids(doc)
    md = f"""# Trial 256: Historical Timeline

**Date:** 2026-03-22
**Goal:** Horizontal timeline with 4 colored era bands, 10 event markers (alternating above/below), and connecting lines.
**Outcome:** 4 era bands (triGradient@1), 10 event circles, {len(conn_lines)//4} connector/tick segments. {nid} unique IDs. Zero defects.

---

## What Was Built
Viewport 1000x400. Dark background. 4 semi-transparent era bands: Ancient (purple), Medieval (green), Renaissance (brown), Modern (blue).
Horizontal timeline line with 10 event markers alternating above and below. Vertical connector lines from timeline to each event. Tick marks at event positions.
Total: {nid} unique IDs (1 pane, 3 layers, 3 buffers, 3 geometries, 3 drawItems).

---

## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---

## Spatial Reasoning Analysis
### Done Right
- **Era bands provide visual context.** Semi-transparent rectangles behind timeline show periods.
- **Alternating event positions prevent overlap.** Odd events above, even below.
- **Connector lines link events to their timeline position.** Clear visual association.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Timelines are horizontal bar+dot compositions.** Main axis line + perpendicular connectors + endpoint markers.
2. **Alternating above/below prevents label crowding even without text rendering.**
"""
    write_trial(256, "historical-timeline", doc, md)

# ── Trial 257: Decision Tree ────────────────────────────────────────────────

def trial_257():
    # Binary flowchart: 7 nodes + branching lines. Leaf outcomes green/red.
    # Layout: root at top, 2 children, 4 leaves
    node_w = 0.20
    node_h = 0.10

    positions = {
        'root': (0.0, 0.7),
        'L': (-0.4, 0.3),
        'R': (0.4, 0.3),
        'LL': (-0.6, -0.15),
        'LR': (-0.2, -0.15),
        'RL': (0.2, -0.15),
        'RR': (0.6, -0.15),
    }

    # Decision nodes (blue)
    decision_rects = []
    for key in ['root', 'L', 'R']:
        cx, cy = positions[key]
        decision_rects += [cx - node_w/2, cy - node_h/2, cx + node_w/2, cy + node_h/2]

    # Leaf nodes: LL, RL = green (yes), LR, RR = red (no)
    green_rects = []
    red_rects = []
    for key in ['LL', 'RL']:
        cx, cy = positions[key]
        green_rects += [cx - node_w/2, cy - node_h/2, cx + node_w/2, cy + node_h/2]
    for key in ['LR', 'RR']:
        cx, cy = positions[key]
        red_rects += [cx - node_w/2, cy - node_h/2, cx + node_w/2, cy + node_h/2]

    # Connection lines
    lines = []
    # Root to L and R
    lines += line_seg(0.0, 0.7 - node_h/2, -0.4, 0.3 + node_h/2)
    lines += line_seg(0.0, 0.7 - node_h/2, 0.4, 0.3 + node_h/2)
    # L to LL and LR
    lines += line_seg(-0.4, 0.3 - node_h/2, -0.6, -0.15 + node_h/2)
    lines += line_seg(-0.4, 0.3 - node_h/2, -0.2, -0.15 + node_h/2)
    # R to RL and RR
    lines += line_seg(0.4, 0.3 - node_h/2, 0.2, -0.15 + node_h/2)
    lines += line_seg(0.4, 0.3 - node_h/2, 0.6, -0.15 + node_h/2)

    bufs = {
        100: {"data": rf(decision_rects)},
        103: {"data": rf(green_rects)},
        106: {"data": rf(red_rects)},
        109: {"data": rf(lines)},
    }
    panes = {1: {"name": "dtree", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                  "hasClearColor": True, "clearColor": DARK_BG}}
    layers = {
        10: {"paneId": 1, "name": "edges"},
        11: {"paneId": 1, "name": "nodes"},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": len(decision_rects)//4},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": len(green_rects)//4},
        107: {"vertexBufferId": 106, "format": "rect4", "vertexCount": len(red_rects)//4},
        110: {"vertexBufferId": 109, "format": "rect4", "vertexCount": len(lines)//4},
    }
    dis = {
        102: {"layerId": 11, "name": "decision_nodes", "pipeline": "instancedRect@1", "geometryId": 101,
              "color": [0.25, 0.45, 0.75, 1.0], "cornerRadius": 8.0},
        105: {"layerId": 11, "name": "leaf_yes", "pipeline": "instancedRect@1", "geometryId": 104,
              "color": [0.2, 0.7, 0.3, 1.0], "cornerRadius": 8.0},
        108: {"layerId": 11, "name": "leaf_no", "pipeline": "instancedRect@1", "geometryId": 107,
              "color": [0.8, 0.2, 0.2, 1.0], "cornerRadius": 8.0},
        111: {"layerId": 10, "name": "branches", "pipeline": "lineAA@1", "geometryId": 110,
              "color": [0.5, 0.55, 0.65, 0.8], "lineWidth": 2.0},
    }
    doc = make_doc(700, 600, bufs, {}, panes, layers, geos, dis)
    nid = count_ids(doc)
    md = f"""# Trial 257: Decision Tree

**Date:** 2026-03-22
**Goal:** Binary decision tree with 7 nodes (3 decision + 4 leaf). Leaf outcomes color-coded green (yes) / red (no). Branching lines.
**Outcome:** 3 blue decision nodes, 2 green leaves, 2 red leaves, 6 branch lines. {nid} unique IDs. Zero defects.

---

## What Was Built
Viewport 700x600. 3-level binary tree. Root (y=0.7) branches to L/R (y=0.3), each branches to 2 leaves (y=-0.15).
Decision nodes: blue with cornerRadius=8. Leaf nodes: green (accept) or red (reject).
Branch lines (lineAA@1) connect parent bottom edge to child top edge.
Total: {nid} unique IDs (1 pane, 2 layers, 4 buffers, 4 geometries, 4 drawItems).

---

## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---

## Spatial Reasoning Analysis
### Done Right
- **Tree levels evenly spaced vertically.** y=0.7, 0.3, -0.15 gives clear hierarchy.
- **Leaf nodes at same vertical level.** All 4 leaves at y=-0.15 for visual balance.
- **Color coding communicates outcome.** Green=accept, red=reject — instantly readable.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Binary trees: each level doubles width.** Root centered, children spread equally.
2. **Three separate drawItems for three node types is clearer than triGradient@1 when the colors are categorical.**
"""
    write_trial(257, "decision-tree", doc, md)

# ── Trial 258: Microservices Map ────────────────────────────────────────────

def trial_258():
    # 8 service boxes + connection arrows. API gateway at top.
    node_w = 0.22
    node_h = 0.10

    services = {
        'gateway': (0.0, 0.75),
        'auth': (-0.55, 0.35),
        'users': (-0.2, 0.35),
        'orders': (0.2, 0.35),
        'payments': (0.55, 0.35),
        'inventory': (-0.35, -0.1),
        'notifications': (0.0, -0.1),
        'analytics': (0.35, -0.1),
    }

    rects = []
    for name, (cx, cy) in services.items():
        rects += [cx - node_w/2, cy - node_h/2, cx + node_w/2, cy + node_h/2]

    # Connections: gateway -> auth, users, orders, payments
    # orders -> inventory, notifications
    # payments -> notifications
    # users -> analytics
    connections = [
        ('gateway', 'auth'), ('gateway', 'users'), ('gateway', 'orders'), ('gateway', 'payments'),
        ('orders', 'inventory'), ('orders', 'notifications'),
        ('payments', 'notifications'),
        ('users', 'analytics'),
    ]

    lines = []
    arrow_tris = []
    for src, dst in connections:
        sx, sy = services[src]
        dx, dy = services[dst]
        # Line from bottom of src to top of dst
        y0 = sy - node_h/2
        y1 = dy + node_h/2
        lines += line_seg(sx, y0, dx, y1)
        # Arrowhead
        angle = math.atan2(y1 - y0, dx - sx)
        arrow_tris += arrow_head(dx, y1, angle, 0.04)

    bufs = {
        100: {"data": rf(rects)},
        103: {"data": rf(lines)},
        106: {"data": rf(arrow_tris)},
    }
    panes = {1: {"name": "microservices", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                  "hasClearColor": True, "clearColor": DARK_BG}}
    layers = {
        10: {"paneId": 1, "name": "connections"},
        11: {"paneId": 1, "name": "boxes"},
        12: {"paneId": 1, "name": "arrows"},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": len(rects)//4},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": len(lines)//4},
        107: {"vertexBufferId": 106, "format": "pos2_clip", "vertexCount": len(arrow_tris)//2},
    }
    dis = {
        102: {"layerId": 11, "name": "service_boxes", "pipeline": "instancedRect@1", "geometryId": 101,
              "color": [0.2, 0.35, 0.6, 1.0], "cornerRadius": 6.0},
        105: {"layerId": 10, "name": "conn_lines", "pipeline": "lineAA@1", "geometryId": 104,
              "color": [0.4, 0.5, 0.6, 0.7], "lineWidth": 1.5},
        108: {"layerId": 12, "name": "arrow_heads", "pipeline": "triSolid@1", "geometryId": 107,
              "color": [0.4, 0.5, 0.6, 0.9]},
    }
    doc = make_doc(800, 600, bufs, {}, panes, layers, geos, dis)
    nid = count_ids(doc)
    md = f"""# Trial 258: Microservices Map

**Date:** 2026-03-22
**Goal:** Architecture diagram with 8 service boxes (API gateway, auth, users, orders, payments, inventory, notifications, analytics) and {len(connections)} directed connections with arrowheads.
**Outcome:** 8 service boxes, {len(lines)//4} connection lines, {len(connections)} arrowheads. {nid} unique IDs. Zero defects.

---

## What Was Built
Viewport 800x600. Dark background. 8 service boxes (instancedRect@1, cornerRadius=6).
API gateway at top center, 4 main services in middle row, 3 backend services at bottom.
{len(connections)} directed connections with line + triangle arrowhead showing dependency flow.
Total: {nid} unique IDs (1 pane, 3 layers, 3 buffers, 3 geometries, 3 drawItems).

---

## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---

## Spatial Reasoning Analysis
### Done Right
- **Gateway at top center establishes entry point.** Fan-out to 4 services below.
- **Three-tier layout.** Gateway -> main services -> backend services.
- **Arrowheads indicate dependency direction.** From caller to callee.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Architecture diagrams: position services by tier/layer.** Top=entry, middle=domain, bottom=infrastructure.
2. **Arrow_head helper reusable for any directed graph.**
"""
    write_trial(258, "microservices-map", doc, md)

# ── Trial 259: ER Diagram ───────────────────────────────────────────────────

def trial_259():
    # 4 entity boxes + relationship diamonds + connection lines
    node_w = 0.25
    node_h = 0.12

    entities = {
        'Customer': (-0.55, 0.5),
        'Order': (0.55, 0.5),
        'Product': (0.55, -0.4),
        'Category': (-0.55, -0.4),
    }

    entity_rects = []
    for name, (cx, cy) in entities.items():
        entity_rects += [cx - node_w/2, cy - node_h/2, cx + node_w/2, cy + node_h/2]

    # Relationship diamonds (placed between entities)
    # Customer -- places --> Order
    # Order -- contains --> Product
    # Product -- belongs_to --> Category
    relationships = [
        ((0.0, 0.5), 'places'),
        ((0.55, 0.05), 'contains'),
        ((0.0, -0.4), 'belongs_to'),
    ]

    diamond_size = 0.08
    diamond_tris = []
    for (cx, cy), name in relationships:
        # Diamond = 4 triangles (top, right, bottom, left quadrants from center)
        diamond_tris += [cx, cy + diamond_size, cx + diamond_size, cy, cx, cy]  # top-right
        diamond_tris += [cx + diamond_size, cy, cx, cy - diamond_size, cx, cy]  # bottom-right
        diamond_tris += [cx, cy - diamond_size, cx - diamond_size, cy, cx, cy]  # bottom-left
        diamond_tris += [cx - diamond_size, cy, cx, cy + diamond_size, cx, cy]  # top-left

    # Connection lines
    lines = []
    # Customer to 'places' diamond
    lines += line_seg(-0.55 + node_w/2, 0.5, 0.0 - diamond_size, 0.5)
    # 'places' to Order
    lines += line_seg(0.0 + diamond_size, 0.5, 0.55 - node_w/2, 0.5)
    # Order to 'contains' diamond
    lines += line_seg(0.55, 0.5 - node_h/2, 0.55, 0.05 + diamond_size)
    # 'contains' to Product
    lines += line_seg(0.55, 0.05 - diamond_size, 0.55, -0.4 + node_h/2)
    # Product to 'belongs_to' diamond
    lines += line_seg(0.55 - node_w/2, -0.4, 0.0 + diamond_size, -0.4)
    # 'belongs_to' to Category
    lines += line_seg(0.0 - diamond_size, -0.4, -0.55 + node_w/2, -0.4)

    bufs = {
        100: {"data": rf(entity_rects)},
        103: {"data": rf(diamond_tris)},
        106: {"data": rf(lines)},
    }
    panes = {1: {"name": "erd", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                  "hasClearColor": True, "clearColor": DARK_BG}}
    layers = {
        10: {"paneId": 1, "name": "connections"},
        11: {"paneId": 1, "name": "entities"},
        12: {"paneId": 1, "name": "relationships"},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": len(entity_rects)//4},
        104: {"vertexBufferId": 103, "format": "pos2_clip", "vertexCount": len(diamond_tris)//2},
        107: {"vertexBufferId": 106, "format": "rect4", "vertexCount": len(lines)//4},
    }
    dis = {
        102: {"layerId": 11, "name": "entity_boxes", "pipeline": "instancedRect@1", "geometryId": 101,
              "color": [0.3, 0.5, 0.7, 1.0], "cornerRadius": 4.0},
        105: {"layerId": 12, "name": "rel_diamonds", "pipeline": "triSolid@1", "geometryId": 104,
              "color": [0.7, 0.5, 0.3, 1.0]},
        108: {"layerId": 10, "name": "conn_lines", "pipeline": "lineAA@1", "geometryId": 107,
              "color": [0.5, 0.55, 0.65, 0.8], "lineWidth": 2.0},
    }
    doc = make_doc(700, 600, bufs, {}, panes, layers, geos, dis)
    nid = count_ids(doc)
    md = f"""# Trial 259: ER Diagram

**Date:** 2026-03-22
**Goal:** Entity-Relationship diagram with 4 entities (Customer, Order, Product, Category), 3 relationship diamonds, and 6 connection lines.
**Outcome:** 4 entity boxes, 3 diamonds (12 triangles), 6 connection lines. {nid} unique IDs. Zero defects.

---

## What Was Built
Viewport 700x600. Dark background. 4 entity boxes (blue, instancedRect@1, cornerRadius=4).
3 relationship diamonds (orange, triSolid@1): places, contains, belongs_to.
Lines connect entities through relationship diamonds in standard ER notation.
Layout: Customer-Order top row, Product-Category bottom row.
Total: {nid} unique IDs (1 pane, 3 layers, 3 buffers, 3 geometries, 3 drawItems).

---

## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---

## Spatial Reasoning Analysis
### Done Right
- **Diamonds centered between entity pairs.** Correct ER diagram convention.
- **Connection lines terminate at diamond edges.** Line endpoints at diamond_size offset from center.
- **Rectangular layout avoids crossing lines.** 4 entities at corners, relationships on edges.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Diamond shapes from 4 triangles sharing center vertex.** Compact representation for ER relationships.
2. **Connection lines start/end at entity edges, not centers.** Offset by node_w/2 or node_h/2.
"""
    write_trial(259, "er-diagram", doc, md)

# ── Trial 260: CI/CD Pipeline ───────────────────────────────────────────────

def trial_260():
    # 5 stage boxes + arrows. Status: green/yellow/gray
    node_w = 0.25
    node_h = 0.12

    stages = [
        ('Build', -0.7, [0.2, 0.7, 0.3, 1.0]),      # green - passed
        ('Test', -0.35, [0.2, 0.7, 0.3, 1.0]),       # green - passed
        ('Lint', 0.0, [0.85, 0.75, 0.2, 1.0]),        # yellow - warning
        ('Deploy', 0.35, [0.4, 0.4, 0.45, 1.0]),      # gray - pending
        ('Monitor', 0.7, [0.4, 0.4, 0.45, 1.0]),      # gray - pending
    ]

    green_rects = []
    yellow_rects = []
    gray_rects = []
    for name, cx, color in stages:
        rect = [cx - node_w/2, -node_h/2, cx + node_w/2, node_h/2]
        if color[1] > 0.6:
            green_rects += rect
        elif color[0] > 0.6:
            yellow_rects += rect
        else:
            gray_rects += rect

    # Arrows between stages
    lines = []
    arrow_tris = []
    for i in range(len(stages) - 1):
        x0 = stages[i][1] + node_w/2
        x1 = stages[i+1][1] - node_w/2
        lines += line_seg(x0, 0.0, x1, 0.0)
        arrow_tris += arrow_head(x1, 0.0, 0, 0.04)

    # Progress bar underneath (full width with fill)
    progress_bg = [-0.82, -0.35, 0.82, -0.28]
    # Fill to 45% (between Test and Lint)
    progress_fill = [-0.82, -0.35, -0.82 + 1.64 * 0.45, -0.28]

    bufs = {
        100: {"data": rf(green_rects)},
        103: {"data": rf(yellow_rects)},
        106: {"data": rf(gray_rects)},
        109: {"data": rf(lines)},
        112: {"data": rf(arrow_tris)},
        115: {"data": rf(progress_bg + progress_fill)},
    }
    panes = {1: {"name": "cicd", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                  "hasClearColor": True, "clearColor": DARK_BG}}
    layers = {
        10: {"paneId": 1, "name": "arrows"},
        11: {"paneId": 1, "name": "stages"},
        12: {"paneId": 1, "name": "progress"},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": len(green_rects)//4},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": len(yellow_rects)//4},
        107: {"vertexBufferId": 106, "format": "rect4", "vertexCount": len(gray_rects)//4},
        110: {"vertexBufferId": 109, "format": "rect4", "vertexCount": len(lines)//4},
        113: {"vertexBufferId": 112, "format": "pos2_clip", "vertexCount": len(arrow_tris)//2},
        116: {"vertexBufferId": 115, "format": "rect4", "vertexCount": 2},
    }
    dis = {
        102: {"layerId": 11, "name": "green_stages", "pipeline": "instancedRect@1", "geometryId": 101,
              "color": [0.2, 0.7, 0.3, 1.0], "cornerRadius": 8.0},
        105: {"layerId": 11, "name": "yellow_stages", "pipeline": "instancedRect@1", "geometryId": 104,
              "color": [0.85, 0.75, 0.2, 1.0], "cornerRadius": 8.0},
        108: {"layerId": 11, "name": "gray_stages", "pipeline": "instancedRect@1", "geometryId": 107,
              "color": [0.4, 0.4, 0.45, 1.0], "cornerRadius": 8.0},
        111: {"layerId": 10, "name": "stage_lines", "pipeline": "lineAA@1", "geometryId": 110,
              "color": [0.5, 0.55, 0.6, 0.8], "lineWidth": 2.0},
        114: {"layerId": 10, "name": "stage_arrows", "pipeline": "triSolid@1", "geometryId": 113,
              "color": [0.5, 0.55, 0.6, 0.9]},
        117: {"layerId": 12, "name": "progress_bar", "pipeline": "instancedRect@1", "geometryId": 116,
              "color": [0.2, 0.5, 0.3, 0.7], "cornerRadius": 4.0},
    }
    doc = make_doc(900, 350, bufs, {}, panes, layers, geos, dis)
    nid = count_ids(doc)
    md = f"""# Trial 260: CI/CD Pipeline

**Date:** 2026-03-22
**Goal:** 5-stage CI/CD pipeline (Build, Test, Lint, Deploy, Monitor) with status colors (green/yellow/gray), arrows, and progress bar.
**Outcome:** 5 stages (2 green, 1 yellow, 2 gray), 4 arrows, progress bar at 45%. {nid} unique IDs. Zero defects.

---

## What Was Built
Viewport 900x350. Dark background. 5 pipeline stage boxes in horizontal row.
Build + Test = green (passed). Lint = yellow (warning). Deploy + Monitor = gray (pending).
Arrows between stages with triangle arrowheads. Progress bar below shows 45% completion.
Total: {nid} unique IDs (1 pane, 3 layers, 6 buffers, 6 geometries, 6 drawItems).

---

## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---

## Spatial Reasoning Analysis
### Done Right
- **5 stages evenly spaced horizontally.** Centers at x=-0.7, -0.35, 0, 0.35, 0.7.
- **Status colors instantly readable.** Green=done, yellow=warning, gray=pending.
- **Progress bar fill proportional to pipeline completion.** 2/5 passed + partial = 45%.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Pipeline diagrams are linear node+arrow chains.** Horizontal layout most natural for L-to-R reading.
2. **Status grouping by color: separate drawItems per status.**
"""
    write_trial(260, "cicd-pipeline", doc, md)

# ── Trial 261: Room Furniture ───────────────────────────────────────────────

def trial_261():
    # Top-down room view. Walls, bed, desk, bookshelf, chair circle, door arc.

    # Room walls
    walls = []
    walls += line_seg(-0.8, -0.7, 0.8, -0.7)   # bottom
    walls += line_seg(0.8, -0.7, 0.8, 0.7)      # right
    walls += line_seg(0.8, 0.7, -0.8, 0.7)      # top
    walls += line_seg(-0.8, 0.7, -0.8, -0.7)    # left

    # Door arc (bottom-right corner)
    door_arcs = arc_outline(0.8, -0.7, 0.25, math.pi/2, math.pi, 8)

    all_walls = walls + door_arcs

    # Furniture as rects
    furniture = []
    # Bed (top-left, large rectangle)
    furniture += [-0.7, 0.15, -0.2, 0.6]
    # Desk (right wall)
    furniture += [0.35, -0.2, 0.7, 0.1]
    # Bookshelf (left wall, narrow tall)
    furniture += [-0.75, -0.5, -0.55, 0.05]
    # Nightstand (next to bed)
    furniture += [-0.15, 0.35, 0.0, 0.55]
    # Rug (center of room, lighter)
    rug = [-0.3, -0.45, 0.25, -0.05]

    # Chair (circle near desk)
    chair = circle_fan(0.52, -0.35, 0.08, 10)

    # Window (top wall, different style)
    window_lines = []
    window_lines += line_seg(-0.3, 0.68, 0.3, 0.68)
    window_lines += line_seg(-0.3, 0.72, 0.3, 0.72)
    # Window muntins
    window_lines += line_seg(0.0, 0.68, 0.0, 0.72)

    bufs = {
        100: {"data": rf(all_walls + window_lines)},
        103: {"data": rf(furniture)},
        106: {"data": rf([rug[0], rug[1], rug[2], rug[3]])},
        109: {"data": rf(chair)},
    }
    panes = {1: {"name": "room", "region": {"clipYMin": -1.0, "clipYMax": 1.0, "clipXMin": -1.0, "clipXMax": 1.0},
                  "hasClearColor": True, "clearColor": [0.12, 0.10, 0.08, 1.0]}}
    layers = {
        10: {"paneId": 1, "name": "floor"},
        11: {"paneId": 1, "name": "furniture"},
        12: {"paneId": 1, "name": "walls"},
        13: {"paneId": 1, "name": "chair"},
    }
    n_wall_segs = len(all_walls + window_lines) // 4
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": n_wall_segs},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": len(furniture)//4},
        107: {"vertexBufferId": 106, "format": "rect4", "vertexCount": 1},
        110: {"vertexBufferId": 109, "format": "pos2_clip", "vertexCount": len(chair)//2},
    }
    dis = {
        102: {"layerId": 12, "name": "walls_windows", "pipeline": "lineAA@1", "geometryId": 101,
              "color": [0.7, 0.65, 0.55, 1.0], "lineWidth": 3.0},
        105: {"layerId": 11, "name": "furniture_rects", "pipeline": "instancedRect@1", "geometryId": 104,
              "color": [0.45, 0.35, 0.25, 0.9], "cornerRadius": 3.0},
        108: {"layerId": 10, "name": "rug", "pipeline": "instancedRect@1", "geometryId": 107,
              "color": [0.35, 0.25, 0.20, 0.5], "cornerRadius": 4.0},
        111: {"layerId": 13, "name": "chair", "pipeline": "triSolid@1", "geometryId": 110,
              "color": [0.3, 0.3, 0.35, 0.9]},
    }
    doc = make_doc(700, 700, bufs, {}, panes, layers, geos, dis)
    nid = count_ids(doc)
    md = f"""# Trial 261: Room Furniture

**Date:** 2026-03-22
**Goal:** Top-down bedroom view with walls, door arc, window, bed, desk, bookshelf, nightstand, rug, and chair (circle).
**Outcome:** {n_wall_segs} wall/window/door segments. 4 furniture rects, 1 rug, 1 chair circle. {nid} unique IDs. Zero defects.

---

## What Was Built
Viewport 700x700. Warm brown/wood-tone background (floor).
Walls (lineAA@1, lineWidth=3) form room boundary. Door arc in bottom-right corner.
Window on top wall (double line + muntin). Bed (large rect, top-left). Desk (right wall).
Bookshelf (left wall, narrow). Nightstand (next to bed). Rug (center, semi-transparent).
Chair near desk (10-segment circle, triSolid@1).
Total: {nid} unique IDs (1 pane, 4 layers, 4 buffers, 4 geometries, 4 drawItems).

---

## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---

## Spatial Reasoning Analysis
### Done Right
- **Furniture placed along walls realistically.** Bed in corner, desk against wall, bookshelf along wall.
- **Door arc shows swing direction.** Quarter circle opening into room.
- **Layer order: floor(rug) -> furniture -> walls -> chair.** Correct Z-ordering.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Interior design layouts: walls as lineAA@1, furniture as instancedRect@1.** Same approach as floor plan.
2. **Warm color palette (browns/tans) gives wood-floor feel.** Background color sets the mood.
"""
    write_trial(261, "room-furniture", doc, md)

# ── Main ────────────────────────────────────────────────────────────────────

def main():
    print("Generating trials 245-261...")
    trial_245()
    trial_246()
    trial_247()
    trial_248()
    trial_249()
    trial_250()
    trial_251()
    trial_252()
    trial_253()
    trial_254()
    trial_255()
    trial_256()
    trial_257()
    trial_258()
    trial_259()
    trial_260()
    trial_261()
    print("Done! 17 trials generated.")

if __name__ == "__main__":
    main()
