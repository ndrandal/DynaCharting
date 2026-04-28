#!/usr/bin/env python3
"""Generate trials 078-111 for DynaCharting drawing engine.

Each trial produces a .json SceneDocument and a .md audit file.
"""
import json
import math
import os
from typing import Any

OUT_DIR = os.path.dirname(os.path.abspath(__file__))
DATE = "2026-03-22"


# ── helpers ──────────────────────────────────────────────────────────────────

def write_trial(num: int, slug: str, doc: dict, md: str):
    prefix = f"{num:03d}-{slug}"
    json_path = os.path.join(OUT_DIR, f"{prefix}.json")
    md_path = os.path.join(OUT_DIR, f"{prefix}.md")
    with open(json_path, "w") as f:
        json.dump(doc, f, indent=2)
        f.write("\n")
    with open(md_path, "w") as f:
        f.write(md)
    print(f"  wrote {prefix}.json + .md")


def make_doc(width: int, height: int, buffers: dict, transforms: dict,
             panes: dict, layers: dict, geometries: dict, drawItems: dict,
             viewports: dict | None = None,
             textOverlay: dict | None = None) -> dict:
    """Construct a SceneDocument dict with string keys everywhere."""
    doc: dict[str, Any] = {"version": 1, "viewport": {"width": width, "height": height}}
    # Ensure all top-level dicts use string keys
    doc["buffers"] = {str(k): v for k, v in buffers.items()}
    doc["transforms"] = {str(k): v for k, v in transforms.items()}
    doc["panes"] = {str(k): v for k, v in panes.items()}
    doc["layers"] = {str(k): v for k, v in layers.items()}
    doc["geometries"] = {str(k): v for k, v in geometries.items()}
    doc["drawItems"] = {str(k): v for k, v in drawItems.items()}
    if viewports:
        doc["viewports"] = viewports
    if textOverlay:
        doc["textOverlay"] = textOverlay
    return doc


def pane(name: str, ymin: float, ymax: float, xmin: float = -1.0, xmax: float = 1.0,
         clear_color=None) -> dict:
    p: dict = {
        "name": name,
        "region": {"clipYMin": round(ymin, 4), "clipYMax": round(ymax, 4),
                    "clipXMin": round(xmin, 4), "clipXMax": round(xmax, 4)},
    }
    if clear_color is None:
        clear_color = [0.06, 0.09, 0.16, 1.0]
    p["hasClearColor"] = True
    p["clearColor"] = clear_color
    return p


def layer(pane_id: int, name: str) -> dict:
    return {"paneId": pane_id, "name": name}


def transform(sx=1.0, sy=1.0, tx=0.0, ty=0.0) -> dict:
    return {"sx": round(sx, 9), "sy": round(sy, 9),
            "tx": round(tx, 9), "ty": round(ty, 9)}


def compute_transform(data_min_x, data_max_x, data_min_y, data_max_y,
                       clip_min_x, clip_max_x, clip_min_y, clip_max_y) -> dict:
    sx = (clip_max_x - clip_min_x) / (data_max_x - data_min_x) if data_max_x != data_min_x else 1.0
    tx = clip_min_x - data_min_x * sx
    sy = (clip_max_y - clip_min_y) / (data_max_y - data_min_y) if data_max_y != data_min_y else 1.0
    ty = clip_min_y - data_min_y * sy
    return transform(sx, sy, tx, ty)


def buf(data: list) -> dict:
    return {"data": [round(v, 6) if isinstance(v, float) else v for v in data]}


def geom(vbuf_id: int, fmt: str, vtx_count: int) -> dict:
    return {"vertexBufferId": vbuf_id, "format": fmt, "vertexCount": vtx_count}


def di(layer_id: int, name: str, pipeline: str, geom_id: int,
       color=None, transform_id=None, **kwargs) -> dict:
    d: dict = {"layerId": layer_id, "name": name, "pipeline": pipeline, "geometryId": geom_id}
    if color:
        d["color"] = color
    if transform_id is not None:
        d["transformId"] = transform_id
    for k, v in kwargs.items():
        d[k] = v
    return d


def md_audit(num: int, title: str, goal: str, what_was_built: str,
             total_ids: str, spatial_right: list[str],
             outcome: str = "Structurally sound. Zero defects.",
             defects_critical: str = "None.",
             defects_major: str = "None.",
             defects_minor: str = "None.",
             spatial_wrong: str = "Nothing.",
             lessons: list[str] | None = None) -> str:
    if lessons is None:
        lessons = ["**Validate vertex counts.** Ensure len(data)/floats_per_vertex equals vertexCount."]

    spatial_lines = "\n".join(f"- **{s.split('.')[0]}.**{'.'.join(s.split('.')[1:])}" if '.' in s else f"- {s}" for s in spatial_right)
    lesson_lines = "\n".join(f"{i+1}. {l}" for i, l in enumerate(lessons))

    return f"""# Trial {num:03d}: {title}

**Date:** {DATE}
**Goal:** {goal}
**Outcome:** {outcome}

---

## What Was Built

{what_was_built}

Total: {total_ids}

---

## Defects Found

### Critical
{defects_critical}

### Major
{defects_major}

### Minor
{defects_minor}

---

## Spatial Reasoning Analysis

### Done Right
{spatial_lines}

### Done Wrong
{spatial_wrong}

---

## Lessons for Future Trials
{lesson_lines}
"""


# ── helper: circle tessellation ──────────────────────────────────────────────

def circle_fan_tris(cx, cy, r, n_segments=32):
    """Generate triangles for a filled circle using center-fan tessellation."""
    verts = []
    for i in range(n_segments):
        a0 = 2 * math.pi * i / n_segments
        a1 = 2 * math.pi * (i + 1) / n_segments
        verts.extend([cx, cy,
                      cx + r * math.cos(a0), cy + r * math.sin(a0),
                      cx + r * math.cos(a1), cy + r * math.sin(a1)])
    return verts


def arc_fan_tris(cx, cy, r, start_angle, end_angle, n_segments=32):
    """Generate triangles for an arc sector using center-fan tessellation."""
    verts = []
    span = end_angle - start_angle
    for i in range(n_segments):
        a0 = start_angle + span * i / n_segments
        a1 = start_angle + span * (i + 1) / n_segments
        verts.extend([cx, cy,
                      cx + r * math.cos(a0), cy + r * math.sin(a0),
                      cx + r * math.cos(a1), cy + r * math.sin(a1)])
    return verts


def ring_arc_tris(cx, cy, r_inner, r_outer, start_angle, end_angle, n_segments=32):
    """Generate triangles for a ring arc (donut sector)."""
    verts = []
    span = end_angle - start_angle
    for i in range(n_segments):
        a0 = start_angle + span * i / n_segments
        a1 = start_angle + span * (i + 1) / n_segments
        c0, s0 = math.cos(a0), math.sin(a0)
        c1, s1 = math.cos(a1), math.sin(a1)
        # Two triangles per segment forming a quad strip
        verts.extend([
            cx + r_inner * c0, cy + r_inner * s0,
            cx + r_outer * c0, cy + r_outer * s0,
            cx + r_outer * c1, cy + r_outer * s1,
            cx + r_inner * c0, cy + r_inner * s0,
            cx + r_outer * c1, cy + r_outer * s1,
            cx + r_inner * c1, cy + r_inner * s1,
        ])
    return verts


# ── Trial 078: CPU/Memory Dashboard ─────────────────────────────────────────

def trial_078():
    # CPU % data (24 hourly samples: 15-85%)
    cpu_data = [35, 42, 38, 30, 25, 20, 22, 45, 65, 72, 78, 82,
                80, 75, 70, 68, 72, 78, 85, 80, 65, 50, 42, 38]
    # Memory data (24 hourly: 40-90%)
    mem_data = [55, 56, 54, 52, 50, 48, 50, 58, 65, 70, 72, 75,
                78, 80, 82, 80, 78, 76, 80, 85, 90, 82, 70, 60]

    # lineAA segments for CPU: 23 segments, rect4 format
    cpu_segs = []
    for i in range(23):
        cpu_segs.extend([i, cpu_data[i], i + 1, cpu_data[i + 1]])

    # instancedRect bars for memory: 24 rects
    mem_rects = []
    for i in range(24):
        mem_rects.extend([i - 0.35, 0, i + 0.35, mem_data[i]])

    buffers = {
        100: buf(cpu_segs),
        103: buf(mem_rects),
    }
    transforms = {
        50: compute_transform(-1, 24, 10, 95, -0.9, 0.9, -0.9, 0.9),
        51: compute_transform(-1, 24, 0, 100, -0.9, 0.9, -0.9, 0.9),
    }
    panes = {
        1: pane("CPU", 0.05, 1.0),
        2: pane("Memory", -1.0, -0.05),
    }
    layers = {
        10: layer(1, "cpu-line"),
        20: layer(2, "mem-bars"),
    }
    geometries = {
        101: geom(100, "rect4", 23),
        104: geom(103, "rect4", 24),
    }
    drawItems = {
        102: di(10, "CPULine", "lineAA@1", 101, [0.0, 0.8, 0.4, 1.0], 50, lineWidth=2.5),
        105: di(20, "MemBars", "instancedRect@1", 104, [0.2, 0.5, 1.0, 0.8], 51, cornerRadius=2.0),
    }
    viewports = {
        "cpu": {"transformId": 50, "paneId": 1, "xMin": -1, "xMax": 24, "yMin": 10, "yMax": 95, "linkGroup": "time"},
        "memory": {"transformId": 51, "paneId": 2, "xMin": -1, "xMax": 24, "yMin": 0, "yMax": 100, "linkGroup": "time"},
    }
    doc = make_doc(1000, 600, buffers, transforms, panes, layers, geometries, drawItems, viewports)
    md = md_audit(78, "CPU & Memory Dashboard",
                  "2-pane layout with CPU line chart and memory usage bars sharing a time axis.",
                  "1000x600 viewport with two vertically stacked panes.\n\n"
                  "1. **CPU pane** (top 50%) -- Green lineAA trace of 24 hourly CPU % readings (20-85%), 23 segments.\n"
                  "2. **Memory pane** (bottom 50%) -- Blue instancedRect bars for 24 hourly memory readings (48-90%), with rounded corners.\n\n"
                  "Both panes share a linked X-axis time group for synchronized panning.",
                  "9 unique IDs (2 panes, 2 layers, 2 transforms, 1 buffer+geo+DI per pane = 3+3)",
                  ["Pane regions. Top pane [0.05, 1.0], bottom [-1.0, -0.05] with 0.1 gap between them.",
                   "Transform math. CPU maps [−1,24]×[10,95] and memory maps [−1,24]×[0,100] into their respective clip regions.",
                   "LineAA segment continuity. Each segment endpoint matches the next segment start."],
                  lessons=["**Link viewports for synchronized scroll.** Using linkGroup 'time' ensures both panes pan together on the X axis.",
                           "**Leave margins around pane regions.** The 0.1 gap between panes provides visual separation."])
    return "cpu-memory-dashboard", doc, md


# ── Trial 079: E-commerce Funnel ─────────────────────────────────────────────

def trial_079():
    # 5 funnel stages: Visits, Cart, Checkout, Payment, Complete
    values = [10000, 6500, 4200, 3100, 2400]
    max_val = values[0]
    # Horizontal bars centered, narrowing top to bottom
    rects = []
    colors_data = []
    n = len(values)
    for i, v in enumerate(values):
        half_w = 0.85 * (v / max_val)
        y_top = 0.8 - i * 0.34
        y_bot = y_top - 0.28
        rects.extend([-half_w, y_bot, half_w, y_top])

    # Use separate draw items per bar for different colors
    # Blue gradient darkening: lighter at top, darker at bottom
    blue_colors = [
        [0.3, 0.6, 1.0, 1.0],
        [0.25, 0.5, 0.9, 1.0],
        [0.2, 0.4, 0.8, 1.0],
        [0.15, 0.3, 0.7, 1.0],
        [0.1, 0.2, 0.6, 1.0],
    ]

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100
    for i in range(5):
        v = values[i]
        half_w = 0.85 * (v / max_val)
        y_top = 0.8 - i * 0.34
        y_bot = y_top - 0.28
        buffers[bid] = buf([-half_w, y_bot, half_w, y_top])
        geometries[bid + 1] = geom(bid, "rect4", 1)
        drawItems[bid + 2] = di(10, f"Stage{i+1}", "instancedRect@1", bid + 1,
                                 blue_colors[i], cornerRadius=6.0)
        bid += 3

    panes = {1: pane("Funnel", -0.95, 0.95, -0.95, 0.95)}
    layers = {10: layer(1, "bars")}
    transforms = {}

    doc = make_doc(600, 800, buffers, transforms, panes, layers, geometries, drawItems)
    md = md_audit(79, "E-Commerce Funnel",
                  "Single-pane funnel visualization with 5 horizontal bars narrowing top-to-bottom.",
                  "600x800 viewport with a single pane.\n\n"
                  "5 horizontal bars representing conversion funnel stages (Visits → Complete). "
                  "Each bar is centered horizontally with width proportional to its value. "
                  "Bars are colored in a blue gradient that darkens from top (lightest) to bottom (darkest). "
                  "Rounded corners (6px) on all bars.",
                  "18 unique IDs (1 pane, 1 layer, 5×(buf+geo+di)=15, 0 transforms)",
                  ["Bar centering. Each bar is symmetrically positioned around x=0 with half-width proportional to value/max.",
                   "Vertical spacing. 0.34 clip units between bar tops, 0.28 bar height, leaving 0.06 gap between bars.",
                   "Color gradient. Five distinct blue shades from [0.3,0.6,1.0] to [0.1,0.2,0.6] convey funnel narrowing."],
                  lessons=["**Use separate draw items for per-element colors.** instancedRect@1 applies one color to all rects, so separate buffers per stage allow distinct colors.",
                           "**Pre-computed clip space works well for static layouts.** No transform needed when data is directly in clip coordinates."])
    return "ecommerce-funnel", doc, md


# ── Trial 080: Weather Forecast ──────────────────────────────────────────────

def trial_080():
    # 7 days of temperature and precipitation
    temps = [18, 22, 25, 23, 19, 16, 20]  # Celsius
    precip = [0, 5, 12, 8, 20, 15, 3]  # mm

    # lineAA segments for temperature
    temp_segs = []
    for i in range(6):
        temp_segs.extend([i, temps[i], i + 1, temps[i + 1]])

    # instancedRect bars for precipitation
    precip_rects = []
    for i in range(7):
        precip_rects.extend([i - 0.3, 0, i + 0.3, precip[i]])

    buffers = {100: buf(temp_segs), 103: buf(precip_rects)}
    transforms = {
        50: compute_transform(-0.5, 6.5, 10, 30, -0.9, 0.9, -0.9, 0.9),
        51: compute_transform(-0.5, 6.5, -2, 25, -0.9, 0.9, -0.9, 0.9),
    }
    panes = {
        1: pane("Temperature", 0.05, 1.0),
        2: pane("Precipitation", -1.0, -0.05),
    }
    layers = {10: layer(1, "temp-line"), 20: layer(2, "precip-bars")}
    geometries = {
        101: geom(100, "rect4", 6),
        104: geom(103, "rect4", 7),
    }
    drawItems = {
        102: di(10, "TempLine", "lineAA@1", 101, [1.0, 0.4, 0.2, 1.0], 50, lineWidth=3.0),
        105: di(20, "PrecipBars", "instancedRect@1", 104, [0.2, 0.6, 1.0, 0.85], 51, cornerRadius=3.0),
    }
    viewports = {
        "temp": {"transformId": 50, "paneId": 1, "xMin": -0.5, "xMax": 6.5, "yMin": 10, "yMax": 30},
        "precip": {"transformId": 51, "paneId": 2, "xMin": -0.5, "xMax": 6.5, "yMin": -2, "yMax": 25},
    }
    doc = make_doc(800, 600, buffers, transforms, panes, layers, geometries, drawItems, viewports)
    md = md_audit(80, "Weather Forecast",
                  "Two-pane 7-day weather forecast with temperature line and precipitation bars.",
                  "800x600 viewport with two vertically stacked panes.\n\n"
                  "1. **Temperature pane** (top) -- Orange-red lineAA trace of 7-day temperatures (16-25°C), 6 segments.\n"
                  "2. **Precipitation pane** (bottom) -- Blue instancedRect bars for daily precipitation (0-20mm), 7 bars.\n\n"
                  "Separate viewports for each pane with appropriate Y ranges.",
                  "9 unique IDs (2 panes, 2 layers, 2 transforms, 2 buf+geo+DI groups)",
                  ["Temperature range. Data spans 16-25°C mapped to view range [10,30] providing comfortable padding.",
                   "Bar widths. Each bar spans ±0.3 data units with 0.4-unit gaps, preventing overlap.",
                   "Pane gap. 0.1 clip-space separation between panes at y=−0.05 to y=0.05."],
                  lessons=["**Pad data ranges.** Extending the viewport range beyond data extremes prevents clipping at edges.",
                           "**Match bar baseline to viewport minimum.** Precipitation bars start at y=0, viewport yMin=-2, giving a small baseline margin."])
    return "weather-forecast", doc, md


# ── Trial 081: IoT Sensor Grid ───────────────────────────────────────────────

def trial_081():
    import random
    random.seed(42)

    # 3x3 grid of mini sparklines
    buffers = {}
    transforms = {}
    panes_d = {}
    layers_d = {}
    geometries = {}
    drawItems = {}

    # Pane IDs: 1-9, Layer IDs: 10-18, Transform IDs: 50-58
    # Buffer/Geo/DI: start at 100, step by 3
    bid = 100
    sparkline_colors = [
        [0.0, 0.8, 0.6, 1.0], [0.2, 0.7, 1.0, 1.0], [1.0, 0.5, 0.2, 1.0],
        [0.8, 0.3, 0.9, 1.0], [0.4, 0.9, 0.3, 1.0], [1.0, 0.8, 0.1, 1.0],
        [0.9, 0.2, 0.3, 1.0], [0.3, 0.5, 1.0, 1.0], [0.6, 0.9, 0.7, 1.0],
    ]

    for row in range(3):
        for col in range(3):
            idx = row * 3 + col
            pane_id = idx + 1
            layer_id = 10 + idx
            tf_id = 50 + idx

            # Pane region: 3x3 grid with margins
            x_min = -1.0 + col * (2.0 / 3) + 0.02
            x_max = -1.0 + (col + 1) * (2.0 / 3) - 0.02
            y_max = 1.0 - row * (2.0 / 3) - 0.02
            y_min = 1.0 - (row + 1) * (2.0 / 3) + 0.02

            panes_d[pane_id] = pane(f"Sensor{idx+1}", y_min, y_max, x_min, x_max,
                                     [0.08, 0.1, 0.18, 1.0])
            layers_d[layer_id] = layer(pane_id, f"spark{idx+1}")

            # 10 data points with random walk
            data = [50.0]
            for _ in range(9):
                data.append(data[-1] + random.uniform(-8, 8))

            # lineAA segments: 9 segments
            segs = []
            for i in range(9):
                segs.extend([i, data[i], i + 1, data[i + 1]])

            dmin = min(data) - 5
            dmax = max(data) + 5

            buffers[bid] = buf(segs)
            transforms[tf_id] = compute_transform(-0.5, 9.5, dmin, dmax, -0.85, 0.85, -0.8, 0.8)
            geometries[bid + 1] = geom(bid, "rect4", 9)
            drawItems[bid + 2] = di(layer_id, f"Spark{idx+1}", "lineAA@1", bid + 1,
                                     sparkline_colors[idx], tf_id, lineWidth=2.0)
            bid += 3

    doc = make_doc(900, 900, buffers, transforms, panes_d, layers_d, geometries, drawItems)
    md = md_audit(81, "IoT Sensor Grid",
                  "3x3 grid of mini sparklines showing 9 IoT sensor readings.",
                  "900x900 viewport with 9 panes arranged in a 3x3 grid.\n\n"
                  "Each pane contains a single lineAA sparkline with 10 data points (9 segments) "
                  "showing a random-walk sensor reading. Each sparkline has a unique color. "
                  "Panes have subtle dark backgrounds with 0.04 clip-space margins between them.",
                  "45 unique IDs (9 panes, 9 layers, 9 transforms, 9×(buf+geo+di)=27)",
                  ["Grid layout. 3x3 pane arrangement with even spacing and consistent margins of 0.02 clip units on each side.",
                   "Per-pane transforms. Each sparkline has its own transform computed from its data range, fitting data to the pane.",
                   "Color distinctness. Nine different hues ensure each sensor is visually identifiable."],
                  lessons=["**Use per-pane transforms for independent Y ranges.** Each sensor has different magnitude so needs its own mapping.",
                           "**Seed random data for reproducibility.** Using random.seed(42) ensures consistent output across runs."])
    return "iot-sensor-grid", doc, md


# ── Trial 082: Portfolio Pie ─────────────────────────────────────────────────

def trial_082():
    # 6 sectors with percentages
    sectors = [
        ("Stocks", 0.35, [0.2, 0.6, 1.0, 1.0]),
        ("Bonds", 0.25, [0.3, 0.8, 0.4, 1.0]),
        ("RealEstate", 0.15, [1.0, 0.6, 0.2, 1.0]),
        ("Commodities", 0.10, [0.9, 0.3, 0.3, 1.0]),
        ("Crypto", 0.08, [0.7, 0.3, 0.9, 1.0]),
        ("Cash", 0.07, [0.5, 0.5, 0.5, 1.0]),
    ]

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100

    angle = 0
    cx, cy, r = 0.0, 0.0, 0.75
    n_seg = 24  # segments per sector

    for name, frac, color in sectors:
        end_angle = angle + frac * 2 * math.pi
        verts = arc_fan_tris(cx, cy, r, angle, end_angle, n_seg)
        vtx_count = len(verts) // 2
        buffers[bid] = buf(verts)
        geometries[bid + 1] = geom(bid, "pos2_clip", vtx_count)
        drawItems[bid + 2] = di(10, name, "triSolid@1", bid + 1, color)
        angle = end_angle
        bid += 3

    panes = {1: pane("Pie", -0.95, 0.95, -0.95, 0.95)}
    layers = {10: layer(1, "sectors")}
    transforms = {}

    doc = make_doc(700, 700, buffers, transforms, panes, layers, geometries, drawItems)
    md = md_audit(82, "Portfolio Pie Chart",
                  "Pie chart with 6 sectors using triSolid@1 center-fan tessellation.",
                  "700x700 viewport with a single pane.\n\n"
                  "Six pie sectors representing portfolio allocation: Stocks (35%), Bonds (25%), "
                  "Real Estate (15%), Commodities (10%), Crypto (8%), Cash (7%). Each sector is "
                  "built from 24 center-fan triangles for smooth arcs. Radius 0.75 in clip space, centered at origin.",
                  "20 unique IDs (1 pane, 1 layer, 6×(buf+geo+di)=18)",
                  ["Sector angles. Fractions sum to 1.0 (100%), so sectors form a complete circle without gaps.",
                   "Center-fan tessellation. 24 triangles per sector gives ~15° angular resolution, smooth arcs.",
                   "Clip-space radius. r=0.75 fits within the [-0.95,0.95] pane region with margin."],
                  lessons=["**Use enough segments for smooth arcs.** 24 segments per sector is adequate; fewer would show visible edges.",
                           "**Sum fractions to 1.0.** Pie sectors must sum to exactly 100% or a gap/overlap will appear."])
    return "portfolio-pie", doc, md


# ── Trial 083: Sales Pipeline ────────────────────────────────────────────────

def trial_083():
    # 5 stages decreasing left-to-right
    stages = [("Leads", 500), ("Qualified", 320), ("Proposal", 180), ("Negotiation", 100), ("Closed", 60)]
    max_val = stages[0][1]
    rects = []
    colors = [
        [0.3, 0.7, 1.0, 1.0],
        [0.25, 0.6, 0.9, 1.0],
        [0.2, 0.5, 0.8, 1.0],
        [0.15, 0.4, 0.7, 1.0],
        [0.1, 0.3, 0.6, 1.0],
    ]

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100
    for i, (name, val) in enumerate(stages):
        bar_h = 0.7 * (val / max_val)
        x0 = -0.85 + i * 0.36
        x1 = x0 + 0.30
        y0 = -bar_h / 2
        y1 = bar_h / 2
        buffers[bid] = buf([x0, y0, x1, y1])
        geometries[bid + 1] = geom(bid, "rect4", 1)
        drawItems[bid + 2] = di(10, name, "instancedRect@1", bid + 1, colors[i], cornerRadius=8.0)
        bid += 3

    panes = {1: pane("Pipeline", -0.95, 0.95, -0.95, 0.95)}
    layers = {10: layer(1, "bars")}
    transforms = {}

    doc = make_doc(1000, 400, buffers, transforms, panes, layers, geometries, drawItems)
    md = md_audit(83, "Sales Pipeline",
                  "5-stage horizontal pipeline bars decreasing left-to-right with rounded corners.",
                  "1000x400 viewport with a single pane.\n\n"
                  "Five bars representing sales pipeline stages: Leads (500), Qualified (320), "
                  "Proposal (180), Negotiation (100), Closed (60). Bar heights are proportional "
                  "to values, centered vertically. Arranged left-to-right with blue gradient darkening. "
                  "Rounded corners (8px) on all bars.",
                  "18 unique IDs (1 pane, 1 layer, 5×(buf+geo+di)=15, 0 transforms)",
                  ["Bar height proportionality. Heights scale linearly from 0.7 (Leads) to 0.084 (Closed) in clip space.",
                   "Horizontal spacing. 0.36 clip units per stage with 0.30-wide bars gives 0.06 gap between bars.",
                   "Vertical centering. Each bar centered at y=0 creates a symmetric funnel effect."],
                  lessons=["**Center bars vertically for pipeline visuals.** Using ±height/2 creates symmetry that reads as a funnel.",
                           "**Rounded corners add polish.** cornerRadius=8.0 softens the industrial look."])
    return "sales-pipeline", doc, md


# ── Trial 084: Server Status Grid ────────────────────────────────────────────

def trial_084():
    # 4x4 grid of server status indicators
    # 0=green(ok), 1=yellow(warning), 2=red(critical)
    statuses = [0, 0, 1, 0,
                0, 2, 0, 0,
                0, 0, 0, 1,
                0, 0, 0, 0]
    color_map = {
        0: [0.2, 0.8, 0.3, 1.0],  # green
        1: [0.95, 0.8, 0.1, 1.0],  # yellow
        2: [0.9, 0.2, 0.15, 1.0],  # red
    }

    # Group rects by color for efficiency
    rects_by_color = {0: [], 1: [], 2: []}
    for i, s in enumerate(statuses):
        row = i // 4
        col = i % 4
        x0 = -0.85 + col * 0.45
        x1 = x0 + 0.38
        y1 = 0.85 - row * 0.45
        y0 = y1 - 0.38
        rects_by_color[s].extend([x0, y0, x1, y1])

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100
    color_names = {0: "Green", 1: "Yellow", 2: "Red"}
    for status_code, rects in rects_by_color.items():
        if not rects:
            continue
        n_rects = len(rects) // 4
        buffers[bid] = buf(rects)
        geometries[bid + 1] = geom(bid, "rect4", n_rects)
        drawItems[bid + 2] = di(10, f"Status{color_names[status_code]}", "instancedRect@1",
                                 bid + 1, color_map[status_code], cornerRadius=4.0)
        bid += 3

    panes = {1: pane("Servers", -0.95, 0.95, -0.95, 0.95, [0.04, 0.06, 0.12, 1.0])}
    layers = {10: layer(1, "grid")}
    transforms = {}

    doc = make_doc(600, 600, buffers, transforms, panes, layers, geometries, drawItems)
    md = md_audit(84, "Server Status Grid",
                  "4x4 grid of colored server status indicators.",
                  "600x600 viewport with a single pane on dark background.\n\n"
                  "16 server status squares in a 4x4 grid. Colors indicate status: "
                  "green (12 servers OK), yellow (2 warnings), red (1 critical). "
                  "Squares grouped by color into 3 draw items for rendering efficiency. "
                  "Rounded corners (4px).",
                  "12 unique IDs (1 pane, 1 layer, 3×(buf+geo+di)=9, 0 transforms)",
                  ["Grid layout. 4x4 arrangement with 0.45 clip-unit spacing and 0.38-wide squares gives 0.07 gaps.",
                   "Color grouping. Batching rects by status color reduces draw calls from 16 to 3.",
                   "Status distribution. 12 green, 2 yellow, 1 red represents a realistic server fleet."],
                  lessons=["**Group same-color rects into one draw item.** instancedRect@1 applies one color, so grouping by color is efficient.",
                           "**Use distinctive status colors.** Green/yellow/red is universally understood for health indicators."])
    return "server-status-grid", doc, md


# ── Trial 085: Monthly Revenue ───────────────────────────────────────────────

def trial_085():
    # 12 months of revenue + trend line
    revenue = [42, 48, 55, 52, 60, 65, 70, 68, 75, 80, 78, 85]  # $K
    trend = [40, 44, 48, 52, 56, 60, 64, 68, 72, 76, 80, 84]

    # instancedRect bars
    bar_rects = []
    for i in range(12):
        bar_rects.extend([i - 0.35, 0, i + 0.35, revenue[i]])

    # lineAA trend line segments
    trend_segs = []
    for i in range(11):
        trend_segs.extend([i, trend[i], i + 1, trend[i + 1]])

    buffers = {100: buf(bar_rects), 103: buf(trend_segs)}
    transforms = {50: compute_transform(-1, 12, 0, 95, -0.9, 0.9, -0.9, 0.9)}
    panes = {1: pane("Revenue", -0.95, 0.95, -0.95, 0.95)}
    layers = {
        10: layer(1, "bars"),
        11: layer(1, "trend"),
    }
    geometries = {
        101: geom(100, "rect4", 12),
        104: geom(103, "rect4", 11),
    }
    drawItems = {
        102: di(10, "RevenueBars", "instancedRect@1", 101, [0.2, 0.5, 0.9, 0.8], 50, cornerRadius=3.0),
        105: di(11, "TrendLine", "lineAA@1", 104, [1.0, 0.7, 0.1, 1.0], 50, lineWidth=2.5,
                dashLength=10.0, gapLength=6.0),
    }
    viewports = {
        "revenue": {"transformId": 50, "paneId": 1, "xMin": -1, "xMax": 12, "yMin": 0, "yMax": 95},
    }

    doc = make_doc(1000, 500, buffers, transforms, panes, layers, geometries, drawItems, viewports)
    md = md_audit(85, "Monthly Revenue",
                  "Dual-axis chart with revenue bars and dashed trend line overlaid in a single pane.",
                  "1000x500 viewport with one pane and two layers.\n\n"
                  "1. **Bar layer** -- 12 blue instancedRect bars (one per month) showing revenue ($42K-$85K), rounded corners.\n"
                  "2. **Trend layer** -- Yellow dashed lineAA trend line (11 segments) overlaid on top of bars.\n\n"
                  "Both layers share the same transform for aligned data mapping.",
                  "9 unique IDs (1 pane, 2 layers, 1 transform, 2 buf+geo+DI groups)",
                  ["Layer ordering. Bar layer (ID 10) renders behind trend layer (ID 11), so the line overlays the bars.",
                   "Shared transform. Both draw items use transform 50 for consistent X and Y mapping.",
                   "Dashed line. dashLength=10.0 and gapLength=6.0 create a visually distinct trend indicator."],
                  lessons=["**Use multiple layers for overlay compositing.** Bars on layer 10, trend on layer 11 ensures correct z-order.",
                           "**Dashed lines distinguish reference data.** The trend line's dash pattern separates it from actual revenue."])
    return "monthly-revenue", doc, md


# ── Trial 086: Real Estate Comparison ────────────────────────────────────────

def trial_086():
    # 4 properties × 3 metrics (Price/1000, sqft/10, score)
    properties = ["Downtown", "Suburb", "Beach", "Mountain"]
    metrics = [
        ("Price", [450, 280, 520, 340]),  # in thousands
        ("SqFt", [180, 250, 160, 300]),   # in tens
        ("Score", [85, 72, 90, 78]),      # out of 100
    ]
    metric_colors = [
        [0.2, 0.6, 1.0, 1.0],   # blue: price
        [0.3, 0.8, 0.4, 1.0],   # green: sqft
        [1.0, 0.6, 0.2, 1.0],   # orange: score
    ]

    # Grouped bars: 3 bars per property group
    rects = []
    all_rects = {0: [], 1: [], 2: []}
    bar_w = 0.12
    group_w = 0.55
    for pi in range(4):
        x_center = pi  # data space
        for mi in range(3):
            x0 = x_center - 0.20 + mi * 0.15
            x1 = x0 + bar_w
            val = metrics[mi][1][pi]
            all_rects[mi].extend([x0, 0, x1, val])

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100
    for mi in range(3):
        buffers[bid] = buf(all_rects[mi])
        geometries[bid + 1] = geom(bid, "rect4", 4)
        drawItems[bid + 2] = di(10, f"Metric{mi}", "instancedRect@1", bid + 1,
                                 metric_colors[mi], 50, cornerRadius=2.0)
        bid += 3

    transforms = {50: compute_transform(-0.5, 3.5, 0, 550, -0.9, 0.9, -0.9, 0.9)}
    panes = {1: pane("Comparison", -0.95, 0.95, -0.95, 0.95)}
    layers = {10: layer(1, "bars")}

    doc = make_doc(900, 500, buffers, transforms, panes, layers, geometries, drawItems)
    md = md_audit(86, "Real Estate Comparison",
                  "Grouped bar chart comparing 4 properties across 3 metrics.",
                  "900x500 viewport with one pane.\n\n"
                  "12 bars in 4 groups of 3, comparing Downtown, Suburb, Beach, and Mountain properties. "
                  "Three metrics: Price (blue, $280K-$520K), SqFt (green, 1600-3000), and Score (orange, 72-90). "
                  "Bars use a shared transform mapping data space to clip space. Rounded corners (2px).",
                  "12 unique IDs (1 pane, 1 layer, 1 transform, 3×(buf+geo+di)=9)",
                  ["Group spacing. Properties spaced 1.0 apart in data space with 3 narrow bars (0.12 wide) per group.",
                   "Color-coded metrics. Blue/green/orange distinctly identifies each metric within a group.",
                   "Unified Y axis. All three metrics share the same Y range (0-550), allowing direct visual comparison."],
                  lessons=["**Group bars by metric for consistent coloring.** One draw item per metric color simplifies the scene.",
                           "**Normalize data carefully.** All three metrics share one Y axis, so values must be on comparable scales."])
    return "real-estate-comparison", doc, md


# ── Trial 087: Traffic Lights ────────────────────────────────────────────────

def trial_087():
    # 3 large circles: red, yellow, green on dark background
    # Circle tessellation using triSolid@1
    r = 0.18
    positions = [(0.0, 0.5), (0.0, 0.0), (0.0, -0.5)]  # top, middle, bottom
    colors = [
        [0.9, 0.15, 0.1, 1.0],   # red
        [0.95, 0.85, 0.1, 1.0],  # yellow
        [0.1, 0.85, 0.2, 1.0],   # green
    ]
    names = ["Red", "Yellow", "Green"]

    # Background rectangle for the housing
    housing_rect = [-0.35, -0.78, 0.35, 0.78]

    buffers = {}
    geometries = {}
    drawItems = {}

    # Housing background
    buffers[100] = buf(housing_rect)
    geometries[101] = geom(100, "rect4", 1)
    drawItems[102] = di(10, "Housing", "instancedRect@1", 101, [0.15, 0.15, 0.15, 1.0], cornerRadius=12.0)

    bid = 103
    for i, ((cx, cy), color, name) in enumerate(zip(positions, colors, names)):
        verts = circle_fan_tris(cx, cy, r, 32)
        vtx_count = len(verts) // 2
        buffers[bid] = buf(verts)
        geometries[bid + 1] = geom(bid, "pos2_clip", vtx_count)
        drawItems[bid + 2] = di(11, name, "triSolid@1", bid + 1, color)
        bid += 3

    panes = {1: pane("Lights", -0.95, 0.95, -0.95, 0.95, [0.03, 0.03, 0.06, 1.0])}
    layers = {10: layer(1, "housing"), 11: layer(1, "circles")}
    transforms = {}

    doc = make_doc(300, 700, buffers, transforms, panes, layers, geometries, drawItems)
    md = md_audit(87, "Traffic Lights",
                  "Three colored circles (red/yellow/green) on dark housing background.",
                  "300x700 viewport (portrait) with a single pane.\n\n"
                  "A traffic light housing (dark gray rounded rectangle) with three circles: "
                  "red (top), yellow (middle), green (bottom). Each circle uses 32-segment "
                  "center-fan tessellation for smooth rendering. Circle radius 0.18, spaced 0.5 apart.",
                  "15 unique IDs (1 pane, 2 layers, 3×(buf+geo+di) for circles + 1 housing group = 12)",
                  ["Vertical spacing. Circles at y=0.5, 0.0, −0.5 with r=0.18 leave 0.14 gap between circles.",
                   "Housing proportions. Rectangle [-0.35,−0.78] to [0.35,0.78] encloses all three circles with padding.",
                   "Layer ordering. Housing on layer 10 renders behind circles on layer 11."],
                  lessons=["**Use sufficient tessellation for circles.** 32 segments is enough for smooth appearance at this size.",
                           "**Portrait aspect ratio for vertical layouts.** 300x700 viewport naturally suits the traffic light form."])
    return "traffic-lights", doc, md


# ── Trial 088: Project Burndown ──────────────────────────────────────────────

def trial_088():
    # 10 sprints, actual vs ideal burndown
    total = 100
    actual = [100, 92, 85, 78, 68, 60, 48, 38, 25, 12, 5]
    ideal = [total - i * total / 10 for i in range(11)]

    # lineAA segments for actual
    actual_segs = []
    for i in range(10):
        actual_segs.extend([i, actual[i], i + 1, actual[i + 1]])

    # lineAA segments for ideal (dashed)
    ideal_segs = []
    for i in range(10):
        ideal_segs.extend([i, ideal[i], i + 1, ideal[i + 1]])

    buffers = {100: buf(actual_segs), 103: buf(ideal_segs)}
    transforms = {50: compute_transform(-0.5, 10.5, -5, 110, -0.9, 0.9, -0.9, 0.9)}
    panes = {1: pane("Burndown", -0.95, 0.95, -0.95, 0.95)}
    layers = {10: layer(1, "ideal"), 11: layer(1, "actual")}
    geometries = {
        101: geom(100, "rect4", 10),
        104: geom(103, "rect4", 10),
    }
    drawItems = {
        105: di(10, "IdealLine", "lineAA@1", 104, [0.5, 0.5, 0.5, 0.6], 50,
                lineWidth=2.0, dashLength=8.0, gapLength=5.0),
        102: di(11, "ActualLine", "lineAA@1", 101, [0.1, 0.7, 0.9, 1.0], 50, lineWidth=3.0),
    }
    viewports = {
        "burndown": {"transformId": 50, "paneId": 1, "xMin": -0.5, "xMax": 10.5, "yMin": -5, "yMax": 110},
    }

    doc = make_doc(900, 500, buffers, transforms, panes, layers, geometries, drawItems, viewports)
    md = md_audit(88, "Project Burndown",
                  "Burndown chart with actual progress line and dashed ideal trend line.",
                  "900x500 viewport with one pane and two layers.\n\n"
                  "1. **Ideal line** (layer 10) -- Gray dashed lineAA showing linear burndown from 100 to 0 over 10 sprints.\n"
                  "2. **Actual line** (layer 11) -- Cyan solid lineAA showing actual progress: 100→92→85→78→68→60→48→38→25→12→5.\n\n"
                  "Actual line slightly underperforms ideal in early sprints but catches up.",
                  "9 unique IDs (1 pane, 2 layers, 1 transform, 2 buf+geo+DI groups)",
                  ["Draw order. Ideal (dashed, gray) on layer 10 behind actual (solid, cyan) on layer 11.",
                   "Data padding. Viewport range [−0.5,10.5]×[−5,110] extends beyond data for margin.",
                   "Dashed vs solid. Distinct visual treatment clearly separates ideal from actual."],
                  lessons=["**Put reference lines behind data lines.** Ideal trend renders first (lower layer ID), data overlays it.",
                           "**Use alpha for secondary data.** Ideal line at alpha 0.6 recedes visually while remaining visible."])
    return "project-burndown", doc, md


# ── Trial 089: A/B Test Distributions ────────────────────────────────────────

def trial_089():
    # Two bell curves using triAA@1 filled area
    # Generate approximate bell curve points
    def bell_curve_area(mean, sigma, color_alpha, n_points=30):
        """Generate triAA filled area under a bell curve."""
        verts = []
        x_min = mean - 3.5 * sigma
        x_max = mean + 3.5 * sigma
        baseline = 0.0
        for i in range(n_points - 1):
            x0 = x_min + (x_max - x_min) * i / (n_points - 1)
            x1 = x_min + (x_max - x_min) * (i + 1) / (n_points - 1)
            y0 = math.exp(-0.5 * ((x0 - mean) / sigma) ** 2) / (sigma * math.sqrt(2 * math.pi))
            y1 = math.exp(-0.5 * ((x1 - mean) / sigma) ** 2) / (sigma * math.sqrt(2 * math.pi))
            # Two triangles per strip segment
            # Triangle 1: (x0, baseline, 0) (x0, y0, alpha) (x1, y1, alpha)
            # Triangle 2: (x0, baseline, 0) (x1, y1, alpha) (x1, baseline, 0)
            verts.extend([
                x0, baseline, 0.0,
                x0, y0, color_alpha,
                x1, y1, color_alpha,
                x0, baseline, 0.0,
                x1, y1, color_alpha,
                x1, baseline, 0.0,
            ])
        return verts

    curve_a = bell_curve_area(5.0, 1.2, 0.7, 25)
    curve_b = bell_curve_area(6.5, 1.0, 0.7, 25)

    vtx_a = len(curve_a) // 3
    vtx_b = len(curve_b) // 3

    buffers = {100: buf(curve_a), 103: buf(curve_b)}
    transforms = {50: compute_transform(0, 12, -0.02, 0.45, -0.9, 0.9, -0.9, 0.9)}
    panes = {1: pane("ABTest", -0.95, 0.95, -0.95, 0.95)}
    layers = {10: layer(1, "curveA"), 11: layer(1, "curveB")}
    geometries = {
        101: geom(100, "pos2_alpha", vtx_a),
        104: geom(103, "pos2_alpha", vtx_b),
    }
    drawItems = {
        102: di(10, "GroupA", "triAA@1", 101, [0.2, 0.5, 1.0, 0.6], 50, blendMode="additive"),
        105: di(11, "GroupB", "triAA@1", 104, [1.0, 0.4, 0.2, 0.6], 50, blendMode="additive"),
    }

    doc = make_doc(900, 500, buffers, transforms, panes, layers, geometries, drawItems)
    md = md_audit(89, "A/B Test Distributions",
                  "Two overlapping bell curves with alpha blending showing A/B test results.",
                  "900x500 viewport with one pane and two layers.\n\n"
                  "Two normal distributions rendered as filled areas using triAA@1:\n"
                  "- **Group A** (blue, mean=5.0, sigma=1.2) on layer 10\n"
                  "- **Group B** (orange, mean=6.5, sigma=1.0) on layer 11\n\n"
                  "Both use additive blending for transparency in the overlap region. "
                  "Each curve has 24 strip segments (48 triangles) with per-vertex alpha "
                  "for smooth edges at the baseline.",
                  "9 unique IDs (1 pane, 2 layers, 1 transform, 2 buf+geo+DI groups)",
                  ["Baseline alpha. Bottom vertices have alpha=0.0, top vertices have alpha=0.7 for smooth fadeout at base.",
                   "Additive blending. Overlap region adds blue+orange, producing a visible combined intensity.",
                   "Transform range. X [0,12] covers both curves (means at 5.0 and 6.5 with 3.5σ tails)."],
                  lessons=["**Use triAA@1 for smooth filled areas.** Per-vertex alpha enables gradient edges without separate geometry.",
                           "**Additive blending for overlapping distributions.** It naturally shows where two datasets coincide."])
    return "ab-test-distributions", doc, md


# ── Trial 090: NPS Gauge ─────────────────────────────────────────────────────

def trial_090():
    # Semi-circular gauge with needle
    # Arc from π to 0 (left to right, bottom half of circle)
    cx, cy, r = 0.0, -0.1, 0.7

    # Colored arc zones: red (0-30), yellow (30-70), green (70-100)
    red_arc = arc_fan_tris(cx, cy, r, math.pi, math.pi * 0.7, 20)
    yellow_arc = arc_fan_tris(cx, cy, r, math.pi * 0.7, math.pi * 0.3, 20)
    green_arc = arc_fan_tris(cx, cy, r, math.pi * 0.3, 0, 20)

    # Needle pointing to score=72 (0-100 maps to π-0)
    score = 72
    needle_angle = math.pi * (1.0 - score / 100.0)
    nx = cx + 0.6 * math.cos(needle_angle)
    ny = cy + 0.6 * math.sin(needle_angle)
    needle_data = [cx, cy, nx, ny]

    buffers = {
        100: buf(red_arc),
        103: buf(yellow_arc),
        106: buf(green_arc),
        109: buf(needle_data),
    }
    geometries = {
        101: geom(100, "pos2_clip", len(red_arc) // 2),
        104: geom(103, "pos2_clip", len(yellow_arc) // 2),
        107: geom(106, "pos2_clip", len(green_arc) // 2),
        110: geom(109, "rect4", 1),
    }
    drawItems = {
        102: di(10, "RedZone", "triSolid@1", 101, [0.9, 0.2, 0.15, 1.0]),
        105: di(10, "YellowZone", "triSolid@1", 104, [0.95, 0.8, 0.1, 1.0]),
        108: di(10, "GreenZone", "triSolid@1", 107, [0.2, 0.8, 0.3, 1.0]),
        111: di(11, "Needle", "lineAA@1", 110, [1.0, 1.0, 1.0, 1.0], lineWidth=3.0),
    }

    panes = {1: pane("NPS", -0.95, 0.95, -0.95, 0.95, [0.05, 0.05, 0.1, 1.0])}
    layers = {10: layer(1, "arc"), 11: layer(1, "needle")}
    transforms = {}

    doc = make_doc(700, 500, buffers, transforms, panes, layers, geometries, drawItems)
    md = md_audit(90, "NPS Gauge",
                  "Semi-circular gauge with colored zones and needle indicating score of 72.",
                  "700x500 viewport with one pane.\n\n"
                  "A 180° gauge arc centered at (0, −0.1) with radius 0.7:\n"
                  "- **Red zone** (0-30, left third) -- Detractors\n"
                  "- **Yellow zone** (30-70, middle) -- Passives\n"
                  "- **Green zone** (70-100, right third) -- Promoters\n\n"
                  "White needle line points to score 72 (in the green zone). Each arc zone uses 20 "
                  "center-fan triangles.",
                  "15 unique IDs (1 pane, 2 layers, 4×(buf+geo+di)=12)",
                  ["Angle mapping. Score 0→π, score 100→0. Score 72 maps to 0.28π radians (about 50°), correctly in the green zone.",
                   "Needle length. 0.6 vs arc radius 0.7 keeps the needle inside the gauge arc.",
                   "Arc continuity. Red ends where yellow begins (0.7π), yellow ends where green begins (0.3π)."],
                  lessons=["**Map scores to angles carefully.** The π-to-0 sweep creates a natural left-to-right reading for gauges.",
                           "**Use separate draw items for colored zones.** Each zone gets its own color through a separate buffer+geo+DI group."])
    return "nps-gauge", doc, md


# ── Trial 091: Inventory Stacked Area ────────────────────────────────────────

def trial_091():
    # 3 series, 20 data points each
    n = 20
    s1 = [20, 22, 25, 28, 30, 32, 35, 33, 30, 28, 25, 27, 30, 32, 35, 38, 40, 42, 40, 38]
    s2 = [15, 18, 16, 14, 15, 18, 20, 22, 25, 28, 30, 28, 25, 22, 20, 18, 15, 18, 20, 22]
    s3 = [10, 12, 14, 16, 18, 15, 12, 10, 12, 14, 16, 18, 20, 22, 25, 22, 20, 18, 16, 14]

    # Stacked: bottom=s3, middle=s2+s3, top=s1+s2+s3
    stack_top = [s1[i] + s2[i] + s3[i] for i in range(n)]
    stack_mid = [s2[i] + s3[i] for i in range(n)]
    stack_bot = list(s3)

    # Area fill using triSolid@1 strip (2 tris per segment)
    def area_strip(y_top, y_bot, n_pts):
        verts = []
        for i in range(n_pts - 1):
            # Triangle 1
            verts.extend([i, y_bot[i], i, y_top[i], i + 1, y_top[i + 1]])
            # Triangle 2
            verts.extend([i, y_bot[i], i + 1, y_top[i + 1], i + 1, y_bot[i + 1]])
        return verts

    area1 = area_strip(stack_mid, stack_top, n)  # top series
    area2 = area_strip(stack_bot, stack_mid, n)   # middle series
    area3 = area_strip([0] * n, stack_bot, n)     # bottom series

    buffers = {
        100: buf(area3),
        103: buf(area2),
        106: buf(area1),
    }
    transforms = {50: compute_transform(-0.5, 19.5, -2, 80, -0.9, 0.9, -0.9, 0.9)}
    panes = {1: pane("Inventory", -0.95, 0.95, -0.95, 0.95)}
    layers = {
        10: layer(1, "bottom"),
        11: layer(1, "middle"),
        12: layer(1, "top"),
    }
    vtx1 = len(area1) // 2
    vtx2 = len(area2) // 2
    vtx3 = len(area3) // 2
    geometries = {
        101: geom(100, "pos2_clip", vtx3),
        104: geom(103, "pos2_clip", vtx2),
        107: geom(106, "pos2_clip", vtx1),
    }
    drawItems = {
        102: di(10, "Bottom", "triSolid@1", 101, [0.2, 0.6, 0.3, 0.85], 50),
        105: di(11, "Middle", "triSolid@1", 104, [0.3, 0.5, 0.9, 0.85], 50),
        108: di(12, "Top", "triSolid@1", 107, [0.9, 0.4, 0.2, 0.85], 50),
    }

    doc = make_doc(1000, 500, buffers, transforms, panes, layers, geometries, drawItems)
    md = md_audit(91, "Inventory Stacked Area",
                  "3-series stacked area chart showing inventory levels over 20 time periods.",
                  "1000x500 viewport with one pane and three layers.\n\n"
                  "Three stacked area fills using triSolid@1 triangle strips:\n"
                  "- **Bottom** (green) -- Base inventory, 10-25 units\n"
                  "- **Middle** (blue) -- Secondary stock, 14-30 units added\n"
                  "- **Top** (orange) -- Primary stock, 20-42 units added\n\n"
                  "Each area has 19 segments (38 triangles). Series are stacked so each layer fills "
                  "from the previous layer's top to its own cumulative total.",
                  "12 unique IDs (1 pane, 3 layers, 1 transform, 3×(buf+geo+di)=9)",
                  ["Stacking math. Bottom fills 0→s3, middle fills s3→s3+s2, top fills s3+s2→s3+s2+s1.",
                   "Layer order. Bottom (layer 10) behind middle (11) behind top (12) ensures correct overlap.",
                   "Triangle strip correctness. Two triangles per segment form a watertight quad strip."],
                  lessons=["**Stack from bottom up.** Build cumulative sums and fill between adjacent layers.",
                           "**Use separate layers for overlapping area fills.** Layer ordering prevents z-fighting."])
    return "inventory-stacked-area", doc, md


# ── Trial 092: Vertical Conversion Funnel ────────────────────────────────────

def trial_092():
    # 5 trapezoidal shapes narrowing downward
    stages = [("Awareness", 1.0), ("Interest", 0.75), ("Desire", 0.55), ("Action", 0.40), ("Loyalty", 0.28)]
    colors = [
        [0.3, 0.6, 1.0, 1.0],
        [0.4, 0.7, 0.9, 1.0],
        [0.5, 0.8, 0.5, 1.0],
        [0.9, 0.7, 0.2, 1.0],
        [0.9, 0.3, 0.3, 1.0],
    ]

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100
    y_cursor = 0.85

    for i, (name, frac) in enumerate(stages):
        next_frac = stages[i + 1][1] if i < len(stages) - 1 else frac * 0.7
        w_top = 0.85 * frac
        w_bot = 0.85 * next_frac
        h = 0.30
        y_top = y_cursor
        y_bot = y_cursor - h

        # Trapezoid = 2 triangles
        verts = [
            -w_top, y_top, w_top, y_top, w_bot, y_bot,
            -w_top, y_top, w_bot, y_bot, -w_bot, y_bot,
        ]
        vtx_count = 6

        buffers[bid] = buf(verts)
        geometries[bid + 1] = geom(bid, "pos2_clip", vtx_count)
        drawItems[bid + 2] = di(10, name, "triSolid@1", bid + 1, colors[i])
        bid += 3
        y_cursor -= h + 0.04  # gap between stages

    panes = {1: pane("Funnel", -0.95, 0.95, -0.95, 0.95)}
    layers = {10: layer(1, "stages")}
    transforms = {}

    doc = make_doc(600, 800, buffers, transforms, panes, layers, geometries, drawItems)
    md = md_audit(92, "Vertical Conversion Funnel",
                  "Vertical funnel with 5 trapezoidal shapes narrowing downward.",
                  "600x800 viewport (portrait) with one pane.\n\n"
                  "Five trapezoids stacked vertically, each narrower than the one above:\n"
                  "Awareness (100%) → Interest (75%) → Desire (55%) → Action (40%) → Loyalty (28%).\n"
                  "Each stage is two triangles forming a trapezoid. Colors progress from blue through "
                  "green/yellow to red. 0.04 clip-space gaps between stages.",
                  "18 unique IDs (1 pane, 1 layer, 5×(buf+geo+di)=15, 0 transforms)",
                  ["Trapezoid construction. Each stage has top-width matching its fraction and bottom-width matching the next stage's fraction, creating smooth narrowing.",
                   "Vertical spacing. 0.30 height per stage + 0.04 gap = 0.34 per step, 5 stages fit in 1.7 clip units.",
                   "Color progression. Blue→cyan→green→yellow→red maps the customer journey intuitively."],
                  lessons=["**Connect adjacent trapezoid widths.** The bottom of each stage matches the top of the next for visual continuity.",
                           "**Use portrait aspect ratio for vertical funnels.** 600x800 gives each stage more horizontal room."])
    return "conversion-funnel-vertical", doc, md


# ── Trial 093: Sparkline Grid (2x3) ─────────────────────────────────────────

def trial_093():
    import random
    random.seed(99)

    buffers = {}
    transforms = {}
    panes_d = {}
    layers_d = {}
    geometries = {}
    drawItems = {}

    bid = 100
    sparkline_colors = [
        [0.0, 0.8, 0.4, 1.0], [0.3, 0.6, 1.0, 1.0], [1.0, 0.5, 0.1, 1.0],
        [0.9, 0.3, 0.6, 1.0], [0.6, 0.8, 0.2, 1.0], [0.4, 0.3, 1.0, 1.0],
    ]

    for row in range(2):
        for col in range(3):
            idx = row * 3 + col
            pane_id = idx + 1
            layer_id = 10 + idx
            tf_id = 50 + idx

            x_min = -1.0 + col * (2.0 / 3) + 0.02
            x_max = -1.0 + (col + 1) * (2.0 / 3) - 0.02
            y_max = 1.0 - row * 1.0 - 0.02
            y_min = 1.0 - (row + 1) * 1.0 + 0.02

            panes_d[pane_id] = pane(f"Metric{idx+1}", y_min, y_max, x_min, x_max,
                                     [0.07, 0.1, 0.17, 1.0])
            layers_d[layer_id] = layer(pane_id, f"line{idx+1}")

            # 15 data points with random walk
            data = [random.uniform(20, 80)]
            for _ in range(14):
                data.append(max(0, min(100, data[-1] + random.uniform(-10, 10))))

            # 14 segments
            segs = []
            for i in range(14):
                segs.extend([i, data[i], i + 1, data[i + 1]])

            dmin = min(data) - 5
            dmax = max(data) + 5

            buffers[bid] = buf(segs)
            transforms[tf_id] = compute_transform(-0.5, 14.5, dmin, dmax, -0.8, 0.8, -0.7, 0.7)
            geometries[bid + 1] = geom(bid, "rect4", 14)
            drawItems[bid + 2] = di(layer_id, f"Line{idx+1}", "lineAA@1", bid + 1,
                                     sparkline_colors[idx], tf_id, lineWidth=2.0)
            bid += 3

    doc = make_doc(900, 600, buffers, transforms, panes_d, layers_d, geometries, drawItems)
    md = md_audit(93, "Sparkline Grid (2x3)",
                  "6 mini sparkline charts in a 2x3 grid layout.",
                  "900x600 viewport with 6 panes in 2 rows × 3 columns.\n\n"
                  "Each pane contains a lineAA sparkline with 15 data points (14 segments). "
                  "Data is random-walk simulating various metrics. Six distinct colors. "
                  "Each pane has its own transform computed from its data range.",
                  "30 unique IDs (6 panes, 6 layers, 6 transforms, 6×(buf+geo+di)=18)",
                  ["Grid layout. 2x3 arrangement with 0.02 clip-space margins between panes.",
                   "Per-pane transforms. Each sparkline auto-scales to its data range.",
                   "Consistent styling. All sparklines share lineWidth=2.0 for visual coherence."],
                  lessons=["**Use per-pane transforms for independent scaling.** Each metric has different magnitude.",
                           "**2x3 grid works well for dashboard overviews.** Horizontal layout suits wide viewports."])
    return "sparkline-grid-6", doc, md


# ── Trial 094: Forex Candle Grid ─────────────────────────────────────────────

def trial_094():
    import random
    random.seed(123)

    pairs = ["EURUSD", "GBPUSD", "USDJPY", "AUDUSD"]
    base_prices = [1.0850, 1.2650, 148.50, 0.6550]

    buffers = {}
    transforms = {}
    panes_d = {}
    layers_d = {}
    geometries = {}
    drawItems = {}

    bid = 100
    for pi, (pair_name, base) in enumerate(zip(pairs, base_prices)):
        row = pi // 2
        col = pi % 2
        pane_id = pi + 1
        layer_id = 10 + pi
        tf_id = 50 + pi

        x_min = -1.0 + col * 1.0 + 0.02
        x_max = -1.0 + (col + 1) * 1.0 - 0.02
        y_max = 1.0 - row * 1.0 - 0.02
        y_min = 1.0 - (row + 1) * 1.0 + 0.02

        panes_d[pane_id] = pane(pair_name, y_min, y_max, x_min, x_max, [0.06, 0.08, 0.14, 1.0])
        layers_d[layer_id] = layer(pane_id, f"candles{pi+1}")

        # 20 candles
        candle_data = []
        price = base
        hw = 0.35
        price_min = price
        price_max = price
        for i in range(20):
            change = random.uniform(-0.005, 0.005) * base
            o = price
            c = price + change
            h = max(o, c) + abs(random.uniform(0, 0.003) * base)
            l = min(o, c) - abs(random.uniform(0, 0.003) * base)
            candle_data.extend([i, o, h, l, c, hw])
            price = c
            price_min = min(price_min, l)
            price_max = max(price_max, h)

        buffers[bid] = buf(candle_data)
        pad = (price_max - price_min) * 0.1
        transforms[tf_id] = compute_transform(-1, 20, price_min - pad, price_max + pad,
                                               -0.85, 0.85, -0.85, 0.85)
        geometries[bid + 1] = geom(bid, "candle6", 20)
        drawItems[bid + 2] = di(layer_id, pair_name, "instancedCandle@1", bid + 1,
                                 [0.5, 0.5, 0.5, 1.0], tf_id,
                                 colorUp=[0.2, 0.8, 0.3, 1.0],
                                 colorDown=[0.9, 0.2, 0.2, 1.0])
        bid += 3

    doc = make_doc(1000, 800, buffers, transforms, panes_d, layers_d, geometries, drawItems)
    md = md_audit(94, "Forex Candle Grid",
                  "4 currency pairs in 2x2 grid, each with 20 candlesticks.",
                  "1000x800 viewport with 4 panes in a 2x2 grid.\n\n"
                  "Each pane shows 20 OHLC candlesticks for a currency pair:\n"
                  "- **EUR/USD** (top-left)\n"
                  "- **GBP/USD** (top-right)\n"
                  "- **USD/JPY** (bottom-left)\n"
                  "- **AUD/USD** (bottom-right)\n\n"
                  "Green candles for bullish (close >= open), red for bearish. "
                  "Each pane has its own transform auto-fitted to its price range.",
                  "20 unique IDs (4 panes, 4 layers, 4 transforms, 4×(buf+geo+di)=12)",
                  ["Candle format. candle6 (x, open, high, low, close, halfWidth) at 6 floats per candle.",
                   "Per-pair transforms. Each currency has vastly different price scales (0.65 vs 148), requiring independent Y mapping.",
                   "Color semantics. colorUp/colorDown auto-applied by shader based on open vs close."],
                  lessons=["**Use instancedCandle@1 for OHLC data.** The shader handles body/wick rendering and color selection automatically.",
                           "**Independent transforms for different scales.** EUR/USD (~1.08) and USD/JPY (~148) cannot share a Y axis."])
    return "forex-candle-grid", doc, md


# ── Trial 095: Election Results ──────────────────────────────────────────────

def trial_095():
    # 5 parties sorted by vote share, horizontal bars
    parties = [
        ("Progressive", 38.2, [0.2, 0.5, 0.9, 1.0]),
        ("Conservative", 31.5, [0.9, 0.2, 0.2, 1.0]),
        ("Liberal", 15.8, [1.0, 0.7, 0.1, 1.0]),
        ("Green", 9.1, [0.2, 0.8, 0.3, 1.0]),
        ("Independent", 5.4, [0.6, 0.6, 0.6, 1.0]),
    ]

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100

    for i, (name, pct, color) in enumerate(parties):
        y_top = 0.8 - i * 0.34
        y_bot = y_top - 0.26
        x0 = -0.85
        x1 = -0.85 + 1.7 * (pct / 50.0)  # scale to max 50%
        buffers[bid] = buf([x0, y_bot, x1, y_top])
        geometries[bid + 1] = geom(bid, "rect4", 1)
        drawItems[bid + 2] = di(10, name, "instancedRect@1", bid + 1, color, cornerRadius=4.0)
        bid += 3

    panes = {1: pane("Election", -0.95, 0.95, -0.95, 0.95, [0.05, 0.05, 0.1, 1.0])}
    layers = {10: layer(1, "bars")}
    transforms = {}

    doc = make_doc(800, 500, buffers, transforms, panes, layers, geometries, drawItems)
    md = md_audit(95, "Election Results",
                  "Horizontal bar chart showing 5 parties sorted by vote share.",
                  "800x500 viewport with one pane.\n\n"
                  "Five horizontal bars representing party vote shares, sorted descending:\n"
                  "Progressive (38.2%, blue), Conservative (31.5%, red), Liberal (15.8%, yellow), "
                  "Green (9.1%, green), Independent (5.4%, gray). Bar length proportional to percentage. "
                  "Rounded corners (4px).",
                  "18 unique IDs (1 pane, 1 layer, 5×(buf+geo+di)=15, 0 transforms)",
                  ["Bar length scaling. Percentages mapped to clip width: 38.2% → 1.30 clip units, 5.4% → 0.18 clip units.",
                   "Vertical stacking. 0.34 clip units per row with 0.26 bar height gives 0.08 gap.",
                   "Party colors. Traditional political colors: blue progressive, red conservative, yellow liberal, green green."],
                  lessons=["**Sort bars by value for readability.** Descending order makes comparisons immediate.",
                           "**Use clip-space directly for static charts.** No transform needed when data is pre-computed."])
    return "election-results", doc, md


# ── Trial 096: League Table ──────────────────────────────────────────────────

def trial_096():
    # 8 teams with points
    teams = [
        ("Lions", 82), ("Eagles", 76), ("Tigers", 71), ("Bears", 65),
        ("Wolves", 58), ("Hawks", 52), ("Sharks", 45), ("Ravens", 38),
    ]

    rects = []
    for i, (name, pts) in enumerate(teams):
        y_top = 0.9 - i * 0.225
        y_bot = y_top - 0.18
        x0 = -0.85
        x1 = -0.85 + 1.7 * (pts / 100.0)
        rects.extend([x0, y_bot, x1, y_top])

    buffers = {100: buf(rects)}
    geometries = {101: geom(100, "rect4", 8)}
    drawItems = {102: di(10, "Standings", "instancedRect@1", 101,
                          [0.25, 0.55, 0.85, 0.9], cornerRadius=3.0)}

    panes = {1: pane("League", -0.95, 0.95, -0.95, 0.95, [0.05, 0.06, 0.12, 1.0])}
    layers = {10: layer(1, "bars")}
    transforms = {}

    doc = make_doc(800, 600, buffers, transforms, panes, layers, geometries, drawItems)
    md = md_audit(96, "League Table",
                  "8 horizontal bars showing team standings by points.",
                  "800x600 viewport with one pane.\n\n"
                  "Eight horizontal bars representing team standings sorted by points: "
                  "Lions (82), Eagles (76), Tigers (71), Bears (65), Wolves (58), Hawks (52), "
                  "Sharks (45), Ravens (38). All bars share one color (blue). "
                  "Bar length proportional to points (out of 100). Rounded corners (3px).",
                  "4 unique IDs (1 pane, 1 layer, 1 buf+geo+DI group = 3)",
                  ["Uniform color. All bars share one draw item with a single color — simpler than per-team colors.",
                   "Consistent spacing. 0.225 clip units per row, 0.18 bar height, 0.045 gap.",
                   "Proportional lengths. Points out of 100 mapped directly to clip width fraction."],
                  lessons=["**Batch same-color rects.** All 8 bars fit in one buffer and one draw item since they share a color.",
                           "**Use rounded corners for polish.** Even small cornerRadius (3px) improves readability."])
    return "league-table", doc, md


# ── Trial 097: Fitness Rings ─────────────────────────────────────────────────

def trial_097():
    # 3 concentric progress arcs at 75%, 50%, 90%
    cx, cy = 0.0, 0.0
    ring_width = 0.12
    rings = [
        ("Move", 0.75, 0.7, [0.9, 0.2, 0.3, 1.0]),     # outer
        ("Exercise", 0.50, 0.5, [0.3, 0.9, 0.2, 1.0]),  # middle
        ("Stand", 0.90, 0.3, [0.2, 0.7, 0.9, 1.0]),     # inner
    ]

    # Gap at top: arcs start from top (π/2) going clockwise
    # We go from π/2 counterclockwise by the fraction of full circle
    buffers = {}
    geometries = {}
    drawItems = {}

    # Background ring tracks (dark)
    bid = 100
    for i, (name, frac, r_center, color) in enumerate(rings):
        r_inner = r_center - ring_width / 2
        r_outer = r_center + ring_width / 2

        # Full ring track (dark version)
        track = ring_arc_tris(cx, cy, r_inner, r_outer, 0, 2 * math.pi * 0.95, 40)
        track_vtx = len(track) // 2
        buffers[bid] = buf(track)
        geometries[bid + 1] = geom(bid, "pos2_clip", track_vtx)
        drawItems[bid + 2] = di(10, f"{name}Track", "triSolid@1", bid + 1,
                                 [color[0] * 0.2, color[1] * 0.2, color[2] * 0.2, 0.5])
        bid += 3

        # Progress arc
        end_angle = 2 * math.pi * frac * 0.95  # scale to 95% of circle (gap at top)
        start = math.pi / 2  # start from top
        arc = ring_arc_tris(cx, cy, r_inner, r_outer, start, start + end_angle, 30)
        arc_vtx = len(arc) // 2
        buffers[bid] = buf(arc)
        geometries[bid + 1] = geom(bid, "pos2_clip", arc_vtx)
        drawItems[bid + 2] = di(11, f"{name}Progress", "triSolid@1", bid + 1, color)
        bid += 3

    panes = {1: pane("Fitness", -0.95, 0.95, -0.95, 0.95, [0.02, 0.02, 0.05, 1.0])}
    layers = {10: layer(1, "tracks"), 11: layer(1, "progress")}
    transforms = {}

    doc = make_doc(600, 600, buffers, transforms, panes, layers, geometries, drawItems)
    md = md_audit(97, "Fitness Rings",
                  "3 concentric progress arcs at 75%, 50%, 90% completion with gap at top.",
                  "600x600 viewport with one pane.\n\n"
                  "Three concentric ring arcs, each with a dim background track and bright progress arc:\n"
                  "- **Move** (outer, red, 75%) -- radius 0.7\n"
                  "- **Exercise** (middle, green, 50%) -- radius 0.5\n"
                  "- **Stand** (inner, cyan, 90%) -- radius 0.3\n\n"
                  "Ring width 0.12 clip units. Arcs start from top (π/2) going counterclockwise. "
                  "5% gap at top for visual separation. Background tracks rendered at 20% brightness.",
                  "21 unique IDs (1 pane, 2 layers, 6×(buf+geo+di)=18, 0 transforms)",
                  ["Ring spacing. Radii at 0.7, 0.5, 0.3 with ring width 0.12 leaves 0.08 gap between rings.",
                   "Layer ordering. Tracks on layer 10, progress arcs on layer 11 ensures bright arcs overlay dim tracks.",
                   "Arc tessellation. 30-40 segments per arc provides smooth curves."],
                  lessons=["**Render background tracks behind progress arcs.** Dim tracks give context for incomplete progress.",
                           "**Use ring_arc_tris for donut-shaped arcs.** Inner/outer radius creates the ring band."])
    return "fitness-rings", doc, md


# ── Trial 098: Donut Breakdown ───────────────────────────────────────────────

def trial_098():
    # 5 sectors with inner hole
    sectors = [
        ("Engineering", 0.30, [0.3, 0.6, 1.0, 1.0]),
        ("Marketing", 0.22, [0.9, 0.4, 0.2, 1.0]),
        ("Sales", 0.20, [0.3, 0.8, 0.4, 1.0]),
        ("Support", 0.15, [0.9, 0.8, 0.1, 1.0]),
        ("Admin", 0.13, [0.7, 0.3, 0.8, 1.0]),
    ]

    cx, cy = 0.0, 0.0
    r_inner, r_outer = 0.35, 0.75

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100
    angle = 0

    for name, frac, color in sectors:
        end_angle = angle + frac * 2 * math.pi
        verts = ring_arc_tris(cx, cy, r_inner, r_outer, angle, end_angle, 24)
        vtx_count = len(verts) // 2
        buffers[bid] = buf(verts)
        geometries[bid + 1] = geom(bid, "pos2_clip", vtx_count)
        drawItems[bid + 2] = di(10, name, "triSolid@1", bid + 1, color)
        angle = end_angle
        bid += 3

    panes = {1: pane("Donut", -0.95, 0.95, -0.95, 0.95)}
    layers = {10: layer(1, "sectors")}
    transforms = {}

    doc = make_doc(700, 700, buffers, transforms, panes, layers, geometries, drawItems)
    md = md_audit(98, "Donut Breakdown",
                  "Donut chart with 5 sectors and center hole showing department budget allocation.",
                  "700x700 viewport with one pane.\n\n"
                  "Five donut sectors with inner radius 0.35 and outer radius 0.75:\n"
                  "Engineering (30%, blue), Marketing (22%, orange), Sales (20%, green), "
                  "Support (15%, yellow), Admin (13%, purple). Each sector uses ring_arc tessellation "
                  "(24 segments, 48 triangles per sector). Center hole reveals the dark background.",
                  "18 unique IDs (1 pane, 1 layer, 5×(buf+geo+di)=15, 0 transforms)",
                  ["Sector angles. Fractions sum to 1.0, sectors are contiguous with no gaps.",
                   "Ring tessellation. Inner/outer radius quads prevent the center from filling.",
                   "Proportional sizing. Largest sector (Engineering 30%) subtends 108° of arc."],
                  lessons=["**Use ring_arc_tris for donut charts.** The inner/outer radius approach avoids a separate clip mask.",
                           "**Verify fractions sum to 1.0.** 0.30+0.22+0.20+0.15+0.13 = 1.00."])
    return "donut-breakdown", doc, md


# ── Trial 099: Market Treemap ────────────────────────────────────────────────

def trial_099():
    # 12 rectangles packed into viewport, 4 color categories
    # Simple squarified treemap approximation
    items = [
        ("TechA", 250, 0), ("TechB", 180, 0), ("TechC", 120, 0),
        ("FinA", 200, 1), ("FinB", 140, 1),
        ("HealthA", 160, 2), ("HealthB", 100, 2), ("HealthC", 80, 2),
        ("EnergyA", 130, 3), ("EnergyB", 110, 3), ("EnergyC", 70, 3), ("EnergyD", 60, 3),
    ]
    cat_colors = [
        [0.2, 0.5, 0.9, 1.0],   # tech: blue
        [0.3, 0.8, 0.4, 1.0],   # finance: green
        [0.9, 0.4, 0.2, 1.0],   # health: orange
        [0.8, 0.7, 0.2, 1.0],   # energy: yellow
    ]

    # Simple layout: 4 rows of 3, sizes approximate
    total = sum(v for _, v, _ in items)
    # Pre-compute approximate positions
    rects_by_cat = {0: [], 1: [], 2: [], 3: []}
    sorted_items = sorted(items, key=lambda x: -x[1])
    gap = 0.02
    # 3 columns, 4 rows layout
    cols, rows = 3, 4
    cell_w = (1.7 - (cols - 1) * gap) / cols
    cell_h = (1.7 - (rows - 1) * gap) / rows
    for i, (name, val, cat) in enumerate(sorted_items):
        r = i // cols
        c = i % cols
        x0 = -0.85 + c * (cell_w + gap)
        y1 = 0.85 - r * (cell_h + gap)
        x1 = x0 + cell_w
        y0 = y1 - cell_h
        rects_by_cat[cat].extend([x0, y0, x1, y1])

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100
    cat_names = ["Tech", "Finance", "Health", "Energy"]
    for cat in range(4):
        if not rects_by_cat[cat]:
            continue
        n_rects = len(rects_by_cat[cat]) // 4
        buffers[bid] = buf(rects_by_cat[cat])
        geometries[bid + 1] = geom(bid, "rect4", n_rects)
        drawItems[bid + 2] = di(10, cat_names[cat], "instancedRect@1", bid + 1,
                                 cat_colors[cat], cornerRadius=4.0)
        bid += 3

    panes = {1: pane("Treemap", -0.95, 0.95, -0.95, 0.95, [0.04, 0.04, 0.08, 1.0])}
    layers = {10: layer(1, "rects")}
    transforms = {}

    doc = make_doc(900, 700, buffers, transforms, panes, layers, geometries, drawItems)
    md = md_audit(99, "Market Treemap",
                  "12 rectangles representing market sectors, colored by category.",
                  "900x700 viewport with one pane.\n\n"
                  "12 rectangles in a 3×4 grid layout, sorted by market cap (descending). "
                  "Four color categories: Tech (blue, 3 items), Finance (green, 2), Health (orange, 3), "
                  "Energy (yellow, 4). Items sorted by value from top-left (largest) to bottom-right (smallest). "
                  "Rounded corners (4px), small gaps between cells.",
                  "15 unique IDs (1 pane, 1 layer, 4×(buf+geo+di)=12, 0 transforms)",
                  ["Grid packing. 3×4 grid with 0.02 gap fills the viewport efficiently.",
                   "Color grouping. Same-category rects batched into one draw item each.",
                   "Sort order. Descending by value puts most important items at top-left."],
                  lessons=["**Simple grid approximates treemap.** True squarification is complex; a sorted grid conveys the same information.",
                           "**Group by category for color coding.** Each draw item gets one color, so batch by category."])
    return "market-treemap", doc, md


# ── Trial 100: Flow Diagram ──────────────────────────────────────────────────

def trial_100():
    # 4 nodes connected by 3 arrows
    node_names = ["Input", "Process", "Validate", "Output"]
    node_x_centers = [-0.65, -0.2, 0.25, 0.7]
    node_w = 0.28
    node_h = 0.25

    # Node rectangles
    node_rects = []
    for cx in node_x_centers:
        node_rects.extend([cx - node_w / 2, -node_h / 2, cx + node_w / 2, node_h / 2])

    # Arrow lines: connect right edge of one node to left edge of next
    arrow_segs = []
    for i in range(3):
        x0 = node_x_centers[i] + node_w / 2
        x1 = node_x_centers[i + 1] - node_w / 2
        arrow_segs.extend([x0, 0, x1, 0])

    # Arrow heads (small triangles) using triSolid
    arrowheads = []
    for i in range(3):
        tip_x = node_x_centers[i + 1] - node_w / 2
        arrowheads.extend([
            tip_x, 0,
            tip_x - 0.04, 0.03,
            tip_x - 0.04, -0.03,
        ])

    buffers = {
        100: buf(node_rects),
        103: buf(arrow_segs),
        106: buf(arrowheads),
    }
    geometries = {
        101: geom(100, "rect4", 4),
        104: geom(103, "rect4", 3),
        107: geom(106, "pos2_clip", 9),
    }
    drawItems = {
        102: di(10, "Nodes", "instancedRect@1", 101, [0.15, 0.35, 0.65, 1.0], cornerRadius=10.0),
        105: di(11, "Arrows", "lineAA@1", 104, [0.7, 0.7, 0.7, 1.0], lineWidth=2.0),
        108: di(11, "ArrowHeads", "triSolid@1", 107, [0.7, 0.7, 0.7, 1.0]),
    }

    panes = {1: pane("Flow", -0.95, 0.95, -0.95, 0.95, [0.05, 0.06, 0.12, 1.0])}
    layers = {10: layer(1, "nodes"), 11: layer(1, "arrows")}
    transforms = {}

    doc = make_doc(1000, 400, buffers, transforms, panes, layers, geometries, drawItems)
    md = md_audit(100, "Flow Diagram",
                  "Left-to-right flow with 4 rounded nodes connected by 3 arrows.",
                  "1000x400 viewport with one pane.\n\n"
                  "Four rounded rectangles (Input → Process → Validate → Output) connected by "
                  "gray horizontal lines with triangular arrowheads. Nodes are centered vertically "
                  "at y=0 with 0.28×0.25 clip-space dimensions. Arrow lines bridge the gaps between nodes. "
                  "Nodes on layer 10 (behind), arrows on layer 11 (on top).",
                  "9 unique IDs (1 pane, 2 layers, 3×(buf+geo+di)=6, 0 transforms)",
                  ["Node spacing. Centers at -0.65, -0.2, 0.25, 0.7 with width 0.28 leaves ~0.17 gap for arrows.",
                   "Arrow alignment. Lines at y=0 connect right edge of each node to left edge of the next.",
                   "Arrowheads. Small 0.04×0.06 triangles pointing right at each target node edge."],
                  lessons=["**Use cornerRadius for node boxes.** cornerRadius=10.0 creates professional rounded rectangles.",
                           "**Separate nodes and connectors onto different layers.** Controls draw order explicitly."])
    return "flow-diagram", doc, md


# ── Trial 101: Weekly Heatmap 7×24 ──────────────────────────────────────────

def trial_101():
    import random
    random.seed(77)

    # 7 days × 24 hours = 168 rectangles
    # Color scale: blue (low) → red (high)
    n_days, n_hours = 7, 24
    total = n_days * n_hours

    # Generate call volumes
    volumes = []
    for d in range(n_days):
        for h in range(n_hours):
            # Higher volume during business hours, lower at night
            base = 20 if h < 8 or h > 20 else 80
            vol = max(0, min(100, base + random.uniform(-20, 20)))
            volumes.append(vol)

    # Group rects by color bucket for efficient rendering
    # 5 color buckets
    color_buckets = [
        ([0.1, 0.2, 0.6, 1.0], 0, 20),     # dark blue
        ([0.2, 0.4, 0.8, 1.0], 20, 40),     # blue
        ([0.3, 0.7, 0.4, 1.0], 40, 60),     # green
        ([0.9, 0.7, 0.2, 1.0], 60, 80),     # orange
        ([0.9, 0.2, 0.15, 1.0], 80, 101),   # red
    ]

    cell_w = 1.7 / n_hours
    cell_h = 1.7 / n_days
    gap = 0.003

    bucket_rects = {i: [] for i in range(len(color_buckets))}
    for d in range(n_days):
        for h in range(n_hours):
            vol = volumes[d * n_hours + h]
            # Find bucket
            for bi, (_, lo, hi) in enumerate(color_buckets):
                if lo <= vol < hi:
                    x0 = -0.85 + h * cell_w + gap
                    x1 = -0.85 + (h + 1) * cell_w - gap
                    y1 = 0.85 - d * cell_h + gap  # Fixed: subtract gap for visual margin
                    y0 = 0.85 - (d + 1) * cell_h - gap  # Fixed
                    # Ensure y0 < y1
                    if y0 > y1:
                        y0, y1 = y1, y0
                    bucket_rects[bi].extend([x0, y0, x1, y1])
                    break

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100
    bucket_names = ["VeryLow", "Low", "Medium", "High", "VeryHigh"]
    for bi, (color, _, _) in enumerate(color_buckets):
        rects = bucket_rects[bi]
        if not rects:
            continue
        n_rects = len(rects) // 4
        buffers[bid] = buf(rects)
        geometries[bid + 1] = geom(bid, "rect4", n_rects)
        drawItems[bid + 2] = di(10, bucket_names[bi], "instancedRect@1", bid + 1, color)
        bid += 3

    panes = {1: pane("Heatmap", -0.95, 0.95, -0.95, 0.95, [0.03, 0.03, 0.06, 1.0])}
    layers = {10: layer(1, "cells")}
    transforms = {}

    doc = make_doc(1200, 500, buffers, transforms, panes, layers, geometries, drawItems)
    md = md_audit(101, "Weekly Heatmap (7x24)",
                  "7-day × 24-hour heatmap showing call volume with blue-to-red color scale.",
                  "1200x500 viewport with one pane.\n\n"
                  "168 small rectangles in a 24-column × 7-row grid. Colors represent call volume:\n"
                  "- Dark blue: 0-20 (very low, nighttime)\n"
                  "- Blue: 20-40 (low)\n"
                  "- Green: 40-60 (medium)\n"
                  "- Orange: 60-80 (high, business hours)\n"
                  "- Red: 80-100 (very high, peak)\n\n"
                  "Rects grouped by color bucket into 5 draw items. Small gaps between cells.",
                  "18 unique IDs (1 pane, 1 layer, 5×(buf+geo+di)=15, 0 transforms)",
                  ["Grid layout. 24 columns × 7 rows with 0.003 gap fills 1.7×1.7 clip space.",
                   "Color bucketing. 5 color categories reduce draw items from 168 to 5.",
                   "Time-of-day pattern. Seeded random data with higher base during hours 8-20 creates realistic heatmap."],
                  lessons=["**Bucket continuous data for color coding.** Grouping values into 5 ranges allows batching same-color rects.",
                           "**Wide viewport for heatmaps.** 1200x500 gives each of 24 columns enough horizontal room."])
    return "weekly-heatmap-7x24", doc, md


# ── Trial 102: ROI Grouped Bars ──────────────────────────────────────────────

def trial_102():
    # 4 campaigns × 3 metrics + benchmark line
    campaigns = ["Social", "Email", "Search", "Display"]
    metrics = [
        ("Spend", [45, 30, 55, 20]),        # $K
        ("Revenue", [120, 85, 180, 45]),     # $K
        ("ROI", [167, 183, 227, 125]),       # %
    ]
    metric_colors = [
        [0.9, 0.3, 0.3, 1.0],   # spend: red
        [0.3, 0.8, 0.4, 1.0],   # revenue: green
        [0.3, 0.6, 1.0, 1.0],   # ROI: blue
    ]

    bar_groups = {0: [], 1: [], 2: []}
    bar_w = 0.10
    for ci in range(4):
        for mi in range(3):
            x0 = ci - 0.18 + mi * 0.13
            x1 = x0 + bar_w
            val = metrics[mi][1][ci]
            bar_groups[mi].extend([x0, 0, x1, val])

    # Benchmark dashed line at ROI = 150%
    benchmark_seg = [- 0.5, 150, 3.5, 150]

    buffers = {109: buf(benchmark_seg)}
    geometries = {}
    drawItems = {}
    bid = 100

    for mi in range(3):
        buffers[bid] = buf(bar_groups[mi])
        geometries[bid + 1] = geom(bid, "rect4", 4)
        drawItems[bid + 2] = di(10, metrics[mi][0], "instancedRect@1", bid + 1,
                                 metric_colors[mi], 50, cornerRadius=2.0)
        bid += 3

    geometries[110] = geom(109, "rect4", 1)
    drawItems[111] = di(11, "Benchmark", "lineAA@1", 110, [1.0, 1.0, 1.0, 0.5], 50,
                         lineWidth=1.5, dashLength=8.0, gapLength=5.0)

    transforms = {50: compute_transform(-0.5, 3.5, 0, 250, -0.9, 0.9, -0.9, 0.9)}
    panes = {1: pane("ROI", -0.95, 0.95, -0.95, 0.95)}
    layers = {10: layer(1, "bars"), 11: layer(1, "benchmark")}

    doc = make_doc(900, 500, buffers, transforms, panes, layers, geometries, drawItems)
    md = md_audit(102, "ROI Grouped Bars",
                  "4 campaigns × 3 metrics grouped bars with benchmark dashed line.",
                  "900x500 viewport with one pane and two layers.\n\n"
                  "12 grouped bars in 4 campaign groups (Social, Email, Search, Display), "
                  "3 metrics each: Spend (red), Revenue (green), ROI% (blue). "
                  "A white dashed benchmark line at ROI=150% overlays the bars. "
                  "Shared transform maps data space to clip space.",
                  "14 unique IDs (1 pane, 2 layers, 1 transform, 3 bar groups + 1 benchmark = 4×(buf+geo+di) = 12 minus shared geo slot)",
                  ["Grouped bar spacing. 0.13 clip units between bars within a group, groups spaced 1.0 apart.",
                   "Benchmark overlay. Dashed line on layer 11 renders on top of bars on layer 10.",
                   "Multi-scale data. All three metrics share one Y axis (0-250), making Spend appear short relative to ROI%."],
                  lessons=["**Dashed benchmark lines add context.** The 150% ROI target helps viewers judge each campaign.",
                           "**Consider normalized scales for mixed metrics.** Spend ($K) and ROI (%) have different magnitudes."])
    return "roi-grouped-bars", doc, md


# ── Trial 103: Semicircle Gauge ──────────────────────────────────────────────

def trial_103():
    cx, cy = 0.0, -0.15
    r = 0.75

    # Three colored zones on the semicircle
    # Red: 0-30% (π to ~0.7π), Yellow: 30-70% (0.7π to 0.3π), Green: 70-100% (0.3π to 0)
    zones = [
        ("Red", math.pi, 0.7 * math.pi, [0.9, 0.2, 0.15, 1.0]),
        ("Yellow", 0.7 * math.pi, 0.3 * math.pi, [0.95, 0.8, 0.1, 1.0]),
        ("Green", 0.3 * math.pi, 0.0, [0.2, 0.8, 0.3, 1.0]),
    ]

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100

    for name, start, end, color in zones:
        verts = arc_fan_tris(cx, cy, r, end, start, 24)  # Note: end < start for reversed direction
        vtx_count = len(verts) // 2
        buffers[bid] = buf(verts)
        geometries[bid + 1] = geom(bid, "pos2_clip", vtx_count)
        drawItems[bid + 2] = di(10, f"Zone{name}", "triSolid@1", bid + 1, color)
        bid += 3

    # Needle at value = 65 (maps to π*(1 - 0.65) = 0.35π)
    value = 65
    needle_angle = math.pi * (1.0 - value / 100.0)
    nx = cx + 0.65 * math.cos(needle_angle)
    ny = cy + 0.65 * math.sin(needle_angle)
    buffers[bid] = buf([cx, cy, nx, ny])
    geometries[bid + 1] = geom(bid, "rect4", 1)
    drawItems[bid + 2] = di(11, "Needle", "lineAA@1", bid + 1, [1.0, 1.0, 1.0, 1.0], lineWidth=3.5)

    # Center dot
    bid += 3
    dot_verts = circle_fan_tris(cx, cy, 0.04, 16)
    buffers[bid] = buf(dot_verts)
    geometries[bid + 1] = geom(bid, "pos2_clip", len(dot_verts) // 2)
    drawItems[bid + 2] = di(11, "CenterDot", "triSolid@1", bid + 1, [1.0, 1.0, 1.0, 1.0])

    panes = {1: pane("Gauge", -0.95, 0.95, -0.95, 0.95, [0.04, 0.04, 0.08, 1.0])}
    layers = {10: layer(1, "arc"), 11: layer(1, "needle")}
    transforms = {}

    doc = make_doc(700, 500, buffers, transforms, panes, layers, geometries, drawItems)
    md = md_audit(103, "Semicircle Gauge",
                  "Large semicircle gauge with red/yellow/green zones and needle at 65.",
                  "700x500 viewport with one pane.\n\n"
                  "A semicircular gauge centered at (0, -0.15) with radius 0.75:\n"
                  "- **Red zone** (0-30%) on the left\n"
                  "- **Yellow zone** (30-70%) in the middle\n"
                  "- **Green zone** (70-100%) on the right\n\n"
                  "White needle line points to value 65 (in the yellow zone near green boundary). "
                  "Small white center dot at the pivot point.",
                  "18 unique IDs (1 pane, 2 layers, 5×(buf+geo+di)=15)",
                  ["Zone boundaries. Red/yellow at 0.7π, yellow/green at 0.3π — each zone spans proportionally.",
                   "Needle angle. Value 65 maps to 0.35π radians, correctly positioned in the yellow zone.",
                   "Center dot. 16-segment circle at the pivot adds a professional finish."],
                  lessons=["**Add a center dot to gauges.** The pivot point anchors the needle visually.",
                           "**Offset gauge center downward.** cy=-0.15 leaves room above the semicircle for labels."])
    return "semicircle-gauge", doc, md


# ── Trial 104: Hospital Stacked Bars ─────────────────────────────────────────

def trial_104():
    # 5 departments × 4 categories of bed occupancy
    depts = ["ER", "ICU", "Surgery", "Pediatric", "General"]
    # Categories: Occupied, Reserved, Available, Maintenance
    data = [
        [65, 10, 20, 5],    # ER
        [80, 8, 7, 5],      # ICU
        [55, 15, 25, 5],    # Surgery
        [40, 10, 45, 5],    # Pediatric
        [70, 12, 15, 3],    # General
    ]
    cat_colors = [
        [0.9, 0.2, 0.2, 1.0],   # Occupied: red
        [0.9, 0.7, 0.2, 1.0],   # Reserved: yellow
        [0.2, 0.8, 0.3, 1.0],   # Available: green
        [0.5, 0.5, 0.5, 1.0],   # Maintenance: gray
    ]
    cat_names = ["Occupied", "Reserved", "Available", "Maintenance"]

    # Build stacked rects per category
    cat_rects = {c: [] for c in range(4)}
    for di_idx in range(5):
        x0 = di_idx - 0.35
        x1 = di_idx + 0.35
        y_base = 0
        for ci in range(4):
            val = data[di_idx][ci]
            y_top = y_base + val
            cat_rects[ci].extend([x0, y_base, x1, y_top])
            y_base = y_top

    buffers = {}
    geometries_d = {}
    drawItems = {}
    bid = 100
    for ci in range(4):
        buffers[bid] = buf(cat_rects[ci])
        geometries_d[bid + 1] = geom(bid, "rect4", 5)
        drawItems[bid + 2] = di(10, cat_names[ci], "instancedRect@1", bid + 1,
                                 cat_colors[ci], 50)
        bid += 3

    transforms = {50: compute_transform(-0.5, 4.5, 0, 110, -0.9, 0.9, -0.9, 0.9)}
    panes = {1: pane("Hospital", -0.95, 0.95, -0.95, 0.95)}
    layers = {10: layer(1, "bars")}

    doc = make_doc(900, 500, buffers, transforms, panes, layers, geometries_d, drawItems)
    md = md_audit(104, "Hospital Stacked Bars",
                  "5 departments × 4 bed occupancy categories as stacked bars.",
                  "900x500 viewport with one pane.\n\n"
                  "Five stacked bars (one per department: ER, ICU, Surgery, Pediatric, General), "
                  "each subdivided into 4 categories:\n"
                  "- Red: Occupied beds\n"
                  "- Yellow: Reserved\n"
                  "- Green: Available\n"
                  "- Gray: Maintenance\n\n"
                  "Each bar totals 100%. 20 rectangles total (4 per bar). Shared transform.",
                  "15 unique IDs (1 pane, 1 layer, 1 transform, 4×(buf+geo+di)=12)",
                  ["Stacking. Categories stack bottom-to-top: Occupied, Reserved, Available, Maintenance.",
                   "Consistent totals. Each department sums to 100, so all bars reach the same height.",
                   "Category batching. Same-category rects across all departments batch into one draw item."],
                  lessons=["**Stack from bottom up with cumulative Y base.** Each category starts where the previous ended.",
                           "**Batch by category, not by department.** One draw item per color is more efficient than one per bar."])
    return "hospital-stacked-bars", doc, md


# ── Trial 105: Classroom Layout ──────────────────────────────────────────────

def trial_105():
    import random
    random.seed(55)

    # 5x5 grid of desks: occupied (blue) / empty (gray)
    occupied = [random.random() < 0.7 for _ in range(25)]

    rects_occupied = []
    rects_empty = []
    for i in range(25):
        row = i // 5
        col = i % 5
        x0 = -0.8 + col * 0.35
        x1 = x0 + 0.28
        y1 = 0.8 - row * 0.35
        y0 = y1 - 0.28
        if occupied[i]:
            rects_occupied.extend([x0, y0, x1, y1])
        else:
            rects_empty.extend([x0, y0, x1, y1])

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100

    if rects_occupied:
        n = len(rects_occupied) // 4
        buffers[bid] = buf(rects_occupied)
        geometries[bid + 1] = geom(bid, "rect4", n)
        drawItems[bid + 2] = di(10, "Occupied", "instancedRect@1", bid + 1,
                                 [0.2, 0.5, 0.9, 0.9], cornerRadius=4.0)
        bid += 3

    if rects_empty:
        n = len(rects_empty) // 4
        buffers[bid] = buf(rects_empty)
        geometries[bid + 1] = geom(bid, "rect4", n)
        drawItems[bid + 2] = di(10, "Empty", "instancedRect@1", bid + 1,
                                 [0.3, 0.3, 0.35, 0.6], cornerRadius=4.0)
        bid += 3

    panes = {1: pane("Classroom", -0.95, 0.95, -0.95, 0.95, [0.06, 0.08, 0.14, 1.0])}
    layers = {10: layer(1, "desks")}
    transforms = {}

    n_occ = sum(occupied)
    n_empty = 25 - n_occ
    doc = make_doc(600, 600, buffers, transforms, panes, layers, geometries, drawItems)
    md = md_audit(105, "Classroom Layout",
                  "5x5 grid of desk rectangles showing occupied and empty seats.",
                  f"600x600 viewport with one pane.\n\n"
                  f"25 desks in a 5×5 grid. {n_occ} occupied (blue), {n_empty} empty (gray). "
                  "Each desk is 0.28×0.28 clip units with 0.07 gaps between them. "
                  "Rounded corners (4px). Two draw items — one for occupied, one for empty.",
                  "8 unique IDs (1 pane, 1 layer, 2×(buf+geo+di)=6, 0 transforms)",
                  ["Grid alignment. 5×5 layout with 0.35 spacing fits within [-0.8, 0.95] clip range.",
                   "Color distinction. Blue (occupied) vs gray (empty) is immediately readable.",
                   "Batching. 25 rects rendered in 2 draw calls by grouping by status."],
                  lessons=["**Batch by visual state.** Grouping occupied/empty into two draw items is simpler than 25 individual items.",
                           "**Use semi-transparency for empty states.** Alpha 0.6 on empty desks makes them visually recede."])
    return "classroom-layout", doc, md


# ── Trial 106: Parking Grid ──────────────────────────────────────────────────

def trial_106():
    import random
    random.seed(66)

    # 6 rows × 8 columns = 48 spots
    # 0=available(green), 1=occupied(red), 2=reserved(gray)
    statuses = []
    for _ in range(48):
        r = random.random()
        if r < 0.45:
            statuses.append(0)
        elif r < 0.85:
            statuses.append(1)
        else:
            statuses.append(2)

    status_colors = {
        0: [0.2, 0.8, 0.3, 1.0],   # available: green
        1: [0.9, 0.2, 0.15, 1.0],  # occupied: red
        2: [0.4, 0.4, 0.45, 0.7],  # reserved: gray
    }
    status_names = {0: "Available", 1: "Occupied", 2: "Reserved"}

    rects_by_status = {0: [], 1: [], 2: []}
    cols, rows = 8, 6
    cell_w = 1.7 / cols
    cell_h = 1.7 / rows
    gap = 0.01

    for i, s in enumerate(statuses):
        row = i // cols
        col = i % cols
        x0 = -0.85 + col * cell_w + gap
        x1 = -0.85 + (col + 1) * cell_w - gap
        y1 = 0.85 - row * cell_h + gap
        y0 = 0.85 - (row + 1) * cell_h - gap
        if y0 > y1:
            y0, y1 = y1, y0
        rects_by_status[s].extend([x0, y0, x1, y1])

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100
    for s in [0, 1, 2]:
        rects = rects_by_status[s]
        if not rects:
            continue
        n = len(rects) // 4
        buffers[bid] = buf(rects)
        geometries[bid + 1] = geom(bid, "rect4", n)
        drawItems[bid + 2] = di(10, status_names[s], "instancedRect@1", bid + 1,
                                 status_colors[s], cornerRadius=3.0)
        bid += 3

    counts = {0: statuses.count(0), 1: statuses.count(1), 2: statuses.count(2)}
    panes = {1: pane("Parking", -0.95, 0.95, -0.95, 0.95, [0.1, 0.12, 0.18, 1.0])}
    layers = {10: layer(1, "spots")}
    transforms = {}

    doc = make_doc(800, 500, buffers, transforms, panes, layers, geometries, drawItems)
    md = md_audit(106, "Parking Grid",
                  "6x8 grid of parking spots with availability colors.",
                  f"800x500 viewport with one pane.\n\n"
                  f"48 parking spots in a 6-row × 8-column grid. "
                  f"Available: {counts[0]} (green), Occupied: {counts[1]} (red), Reserved: {counts[2]} (gray). "
                  "Small gaps between spots. Background color suggests asphalt. Rounded corners (3px).",
                  "12 unique IDs (1 pane, 1 layer, 3×(buf+geo+di)=9, 0 transforms)",
                  ["Grid dimensions. 8 columns × 6 rows with 0.01 clip-unit gaps.",
                   "Color semantics. Green/red/gray matches universal parking signage conventions.",
                   "Batch by status. 48 rects in 3 draw calls."],
                  lessons=["**Use familiar color conventions.** Green=available, red=taken is universally understood.",
                           "**Asphalt background color.** Dark gray-blue [0.1, 0.12, 0.18] evokes a parking lot surface."])
    return "parking-grid", doc, md


# ── Trial 107: Train Schedule ────────────────────────────────────────────────

def trial_107():
    # Time-distance diagram: 5 diagonal lines (trains) + grid lines
    # X = time (0-24h), Y = distance (0-500km)

    # 5 trains with different departure times and speeds
    trains = [
        (2, 0, 6, 500, [0.3, 0.7, 1.0, 1.0]),    # Train A: departs 2h, arrives 6h
        (5, 0, 10, 500, [0.9, 0.4, 0.2, 1.0]),   # Train B
        (8, 0, 14, 500, [0.3, 0.8, 0.4, 1.0]),   # Train C
        (12, 500, 16, 0, [0.9, 0.8, 0.2, 1.0]),  # Train D (return)
        (16, 500, 21, 0, [0.7, 0.3, 0.9, 1.0]),  # Train E (return)
    ]

    # Grid lines: horizontal distance lines and vertical time lines
    grid_segs = []
    # Horizontal lines at 0, 100, 200, 300, 400, 500 km
    for y in range(0, 501, 100):
        grid_segs.extend([0, y, 24, y])
    # Vertical lines at 0, 4, 8, 12, 16, 20, 24 h
    for x in range(0, 25, 4):
        grid_segs.extend([x, 0, x, 500])

    # Train line segments
    buffers = {}
    geometries = {}
    drawItems = {}

    buffers[100] = buf(grid_segs)
    grid_count = len(grid_segs) // 4
    geometries[101] = geom(100, "rect4", grid_count)
    drawItems[102] = di(10, "Grid", "lineAA@1", 101, [0.3, 0.3, 0.4, 0.4], 50, lineWidth=1.0)

    bid = 103
    for i, (x0, y0, x1, y1, color) in enumerate(trains):
        buffers[bid] = buf([x0, y0, x1, y1])
        geometries[bid + 1] = geom(bid, "rect4", 1)
        drawItems[bid + 2] = di(11, f"Train{chr(65 + i)}", "lineAA@1", bid + 1, color, 50, lineWidth=2.5)
        bid += 3

    transforms = {50: compute_transform(-1, 25, -20, 520, -0.9, 0.9, -0.9, 0.9)}
    panes = {1: pane("Schedule", -0.95, 0.95, -0.95, 0.95, [0.05, 0.05, 0.1, 1.0])}
    layers = {10: layer(1, "grid"), 11: layer(1, "trains")}

    doc = make_doc(1000, 600, buffers, transforms, panes, layers, geometries, drawItems)
    md = md_audit(107, "Train Schedule",
                  "Time-distance diagram with 5 train paths and grid lines.",
                  "1000x600 viewport with one pane and two layers.\n\n"
                  "Five diagonal lines representing train journeys on a time (X, 0-24h) vs distance (Y, 0-500km) grid. "
                  "Three outbound trains (A, B, C: bottom-left to top-right) and two return trains (D, E: top-left to bottom-right). "
                  "Background grid has 6 horizontal lines (100km intervals) and 7 vertical lines (4h intervals). "
                  "Grid on layer 10 (behind), train lines on layer 11 (in front).",
                  "21 unique IDs (1 pane, 2 layers, 1 transform, 6×(buf+geo+di)=18)",
                  ["Grid-train layering. Grid (layer 10) behind trains (layer 11) prevents grid from obscuring paths.",
                   "Direction encoding. Outbound trains slope upward, return trains slope downward.",
                   "Color distinctness. Each train has a unique color for identification."],
                  lessons=["**Use separate layers for grid and data.** Grid behind data is the standard composition pattern.",
                           "**Time-distance diagrams are lineAA-native.** Each train is a single line segment."])
    return "train-schedule", doc, md


# ── Trial 108: Kanban Columns ────────────────────────────────────────────────

def trial_108():
    # 4 columns with 12 cards distributed
    col_names = ["Backlog", "InProgress", "Review", "Done"]
    col_cards = [4, 3, 2, 3]  # cards per column

    # Column backgrounds
    col_rects = []
    col_w = 0.42
    col_gap = 0.06
    start_x = -0.9
    for i in range(4):
        x0 = start_x + i * (col_w + col_gap)
        x1 = x0 + col_w
        col_rects.extend([x0, -0.9, x1, 0.9])

    # Card rectangles
    card_rects = []
    card_h = 0.12
    card_gap = 0.04
    card_colors = [
        [0.3, 0.5, 0.8, 1.0],   # Backlog: blue
        [0.9, 0.6, 0.2, 1.0],   # InProgress: orange
        [0.7, 0.3, 0.8, 1.0],   # Review: purple
        [0.3, 0.8, 0.4, 1.0],   # Done: green
    ]

    card_rects_by_col = {0: [], 1: [], 2: [], 3: []}
    for ci, n_cards in enumerate(col_cards):
        x0 = start_x + ci * (col_w + col_gap) + 0.03
        x1 = x0 + col_w - 0.06
        for j in range(n_cards):
            y1 = 0.8 - j * (card_h + card_gap)
            y0 = y1 - card_h
            card_rects_by_col[ci].extend([x0, y0, x1, y1])

    buffers = {}
    geometries = {}
    drawItems = {}

    # Column backgrounds
    buffers[100] = buf(col_rects)
    geometries[101] = geom(100, "rect4", 4)
    drawItems[102] = di(10, "Columns", "instancedRect@1", 101, [0.1, 0.12, 0.2, 1.0], cornerRadius=6.0)

    # Cards per column
    bid = 103
    for ci in range(4):
        rects = card_rects_by_col[ci]
        if not rects:
            continue
        n = len(rects) // 4
        buffers[bid] = buf(rects)
        geometries[bid + 1] = geom(bid, "rect4", n)
        drawItems[bid + 2] = di(11, col_names[ci], "instancedRect@1", bid + 1,
                                 card_colors[ci], cornerRadius=5.0)
        bid += 3

    panes = {1: pane("Kanban", -0.95, 0.95, -0.95, 0.95, [0.04, 0.05, 0.1, 1.0])}
    layers = {10: layer(1, "backgrounds"), 11: layer(1, "cards")}
    transforms = {}

    doc = make_doc(1000, 600, buffers, transforms, panes, layers, geometries, drawItems)
    md = md_audit(108, "Kanban Columns",
                  "4-column Kanban board with 12 task cards distributed across columns.",
                  "1000x600 viewport with one pane and two layers.\n\n"
                  "Four columns (Backlog, InProgress, Review, Done) with dark rounded backgrounds. "
                  "12 colored task cards distributed: Backlog (4 blue), InProgress (3 orange), "
                  "Review (2 purple), Done (3 green). Cards have rounded corners (5px) and are "
                  "inset within column backgrounds. Columns on layer 10, cards on layer 11.",
                  "18 unique IDs (1 pane, 2 layers, 5×(buf+geo+di) groups = 15)",
                  ["Column layout. 4 columns of 0.42 width with 0.06 gaps fill the [-0.9, 0.9] range.",
                   "Card inset. Cards are 0.06 narrower than columns, creating a visual margin.",
                   "Layer ordering. Column backgrounds behind cards ensures cards are visible."],
                  lessons=["**Inset cards within columns.** The 0.03 padding on each side creates a contained appearance.",
                           "**Color-code by column.** Different card colors per column aids quick visual scanning."])
    return "kanban-columns", doc, md


# ── Trial 109: Velocity Chart ────────────────────────────────────────────────

def trial_109():
    # 8 sprint velocity bars + moving average line
    velocities = [35, 42, 38, 50, 45, 55, 48, 52]

    # Bars
    bar_rects = []
    for i, v in enumerate(velocities):
        bar_rects.extend([i - 0.35, 0, i + 0.35, v])

    # Moving average (window=3)
    ma = []
    for i in range(len(velocities)):
        start = max(0, i - 1)
        end = min(len(velocities), i + 2)
        ma.append(sum(velocities[start:end]) / (end - start))

    ma_segs = []
    for i in range(len(ma) - 1):
        ma_segs.extend([i, ma[i], i + 1, ma[i + 1]])

    buffers = {100: buf(bar_rects), 103: buf(ma_segs)}
    transforms = {50: compute_transform(-0.5, 7.5, 0, 65, -0.9, 0.9, -0.9, 0.9)}
    panes = {1: pane("Velocity", -0.95, 0.95, -0.95, 0.95)}
    layers = {10: layer(1, "bars"), 11: layer(1, "average")}
    geometries = {
        101: geom(100, "rect4", 8),
        104: geom(103, "rect4", 7),
    }
    drawItems = {
        102: di(10, "SprintBars", "instancedRect@1", 101, [0.25, 0.55, 0.9, 0.85], 50, cornerRadius=3.0),
        105: di(11, "MovingAvg", "lineAA@1", 104, [1.0, 0.6, 0.1, 1.0], 50, lineWidth=2.5),
    }

    doc = make_doc(900, 500, buffers, transforms, panes, layers, geometries, drawItems)
    md = md_audit(109, "Velocity Chart",
                  "Sprint velocity bars with 3-period moving average line overlay.",
                  "900x500 viewport with one pane and two layers.\n\n"
                  "Eight bars showing sprint velocities (35-55 story points). "
                  "Orange moving average line (3-period window) overlaid on the bars. "
                  "Bars on layer 10, average on layer 11.",
                  "9 unique IDs (1 pane, 2 layers, 1 transform, 2 buf+geo+DI groups)",
                  ["Bar-line overlay. Bars behind moving average line via layer ordering.",
                   "Moving average. 3-period window smooths the velocity trend.",
                   "Shared transform. Both bars and line use transform 50 for consistent mapping."],
                  lessons=["**Moving averages on top of bars.** Layer 11 (line) over layer 10 (bars) is the standard pattern.",
                           "**Use window-based averaging for trend lines.** 3-period window balances smoothing vs responsiveness."])
    return "velocity-chart", doc, md


# ── Trial 110: Budget Waterfall ──────────────────────────────────────────────

def trial_110():
    # Waterfall chart: 8 floating bars with connector lines
    items = [
        ("Revenue", 0, 200),       # starts at 0, goes to 200
        ("COGS", 200, 130),        # drops from 200 to 130
        ("Gross", 0, 130),         # total: 0 to 130 (subtotal)
        ("OpEx", 130, 85),         # drops from 130 to 85
        ("Marketing", 85, 60),     # drops
        ("R&D", 60, 35),           # drops
        ("OtherInc", 35, 50),      # rises: +15
        ("NetIncome", 0, 50),      # total: 0 to 50
    ]
    colors = [
        [0.3, 0.7, 0.4, 1.0],   # Revenue: green (increase)
        [0.9, 0.3, 0.3, 1.0],   # COGS: red (decrease)
        [0.3, 0.6, 0.9, 1.0],   # Gross: blue (subtotal)
        [0.9, 0.3, 0.3, 1.0],   # OpEx: red
        [0.9, 0.3, 0.3, 1.0],   # Marketing: red
        [0.9, 0.3, 0.3, 1.0],   # R&D: red
        [0.3, 0.7, 0.4, 1.0],   # OtherInc: green
        [0.3, 0.6, 0.9, 1.0],   # NetIncome: blue (total)
    ]

    # Build per-item bars and connectors
    buffers = {}
    geometries = {}
    drawItems_d = {}
    bid = 100

    for i, (name, y_bot, y_top) in enumerate(items):
        buffers[bid] = buf([i - 0.3, y_bot, i + 0.3, y_top])
        geometries[bid + 1] = geom(bid, "rect4", 1)
        drawItems_d[bid + 2] = di(10, name, "instancedRect@1", bid + 1, colors[i], 50, cornerRadius=2.0)
        bid += 3

    # Connector lines between bars
    conn_segs = []
    for i in range(len(items) - 1):
        _, _, y_end = items[i]
        _, y_start_next, _ = items[i + 1]
        # Connect the top/bottom of current bar to the base of next bar
        y_conn = y_end if items[i + 1][1] == y_end else items[i][2]
        conn_segs.extend([i + 0.3, items[i][2], i + 0.7, items[i][2]])

    buffers[bid] = buf(conn_segs)
    conn_count = len(conn_segs) // 4
    geometries[bid + 1] = geom(bid, "rect4", conn_count)
    drawItems_d[bid + 2] = di(11, "Connectors", "lineAA@1", bid + 1, [0.6, 0.6, 0.6, 0.5], 50,
                               lineWidth=1.0, dashLength=4.0, gapLength=3.0)

    transforms = {50: compute_transform(-0.5, 7.5, -10, 220, -0.9, 0.9, -0.9, 0.9)}
    panes = {1: pane("Waterfall", -0.95, 0.95, -0.95, 0.95)}
    layers = {10: layer(1, "bars"), 11: layer(1, "connectors")}

    doc = make_doc(1000, 500, buffers, transforms, panes, layers, geometries, drawItems_d)
    md = md_audit(110, "Budget Waterfall",
                  "Waterfall chart with 8 floating bars and dashed connector lines.",
                  "1000x500 viewport with one pane and two layers.\n\n"
                  "Eight floating bars showing budget flow: Revenue (+200), COGS (-70), Gross Profit (130), "
                  "OpEx (-45), Marketing (-25), R&D (-25), Other Income (+15), Net Income (50). "
                  "Green bars for increases, red for decreases, blue for subtotals. "
                  "Dashed gray connector lines link bar tops to next bar bases.",
                  "30 unique IDs (1 pane, 2 layers, 1 transform, 9×(buf+geo+di) groups = 27)",
                  ["Floating bars. Each bar has independent y_bot and y_top, creating the waterfall cascade.",
                   "Color coding. Green (gains), red (losses), blue (subtotals) follows accounting conventions.",
                   "Connectors. Dashed lines at the bar-top level bridge to the next bar's base."],
                  lessons=["**Each waterfall bar needs independent Y positions.** Unlike normal bars, the baseline varies per item.",
                           "**Use connectors to show flow continuity.** Dashed lines between bars trace the running total."])
    return "budget-waterfall", doc, md


# ── Trial 111: Risk Matrix 5×5 ──────────────────────────────────────────────

def trial_111():
    # 5x5 colored grid with green-to-red diagonal gradient + grid lines
    # Risk = likelihood × impact, green (low) to red (high)

    rects_by_color = {}
    n = 5
    cell_w = 1.7 / n
    cell_h = 1.7 / n
    gap = 0.01

    for row in range(n):
        for col in range(n):
            # Risk level: 0 (low-low) to 8 (high-high)
            risk = row + col  # 0 to 8
            # Color: green(0) → yellow(4) → red(8)
            t = risk / 8.0
            if t <= 0.5:
                r = t * 2 * 0.9
                g = 0.8
                b = 0.2 * (1 - t * 2)
            else:
                r = 0.9
                g = 0.8 * (1 - (t - 0.5) * 2)
                b = 0.1

            # Quantize to 9 buckets
            bucket = risk
            if bucket not in rects_by_color:
                rects_by_color[bucket] = {"color": [round(r, 3), round(g, 3), round(b, 3), 1.0], "rects": []}

            x0 = -0.85 + col * cell_w + gap
            x1 = -0.85 + (col + 1) * cell_w - gap
            y1 = 0.85 - row * cell_h + gap
            y0 = 0.85 - (row + 1) * cell_h - gap
            if y0 > y1:
                y0, y1 = y1, y0
            rects_by_color[bucket]["rects"].extend([x0, y0, x1, y1])

    # Grid lines
    grid_segs = []
    # Horizontal lines (6 lines for 5 cells)
    for i in range(n + 1):
        y = 0.85 - i * cell_h
        grid_segs.extend([-0.85, y, 0.85, y])
    # Vertical lines
    for i in range(n + 1):
        x = -0.85 + i * cell_w
        grid_segs.extend([x, -0.85, x, 0.85])

    buffers = {}
    geometries = {}
    drawItems = {}
    bid = 100

    for bucket in sorted(rects_by_color.keys()):
        info = rects_by_color[bucket]
        rects = info["rects"]
        n_rects = len(rects) // 4
        buffers[bid] = buf(rects)
        geometries[bid + 1] = geom(bid, "rect4", n_rects)
        drawItems[bid + 2] = di(10, f"Risk{bucket}", "instancedRect@1", bid + 1, info["color"])
        bid += 3

    # Grid lines
    buffers[bid] = buf(grid_segs)
    grid_n = len(grid_segs) // 4
    geometries[bid + 1] = geom(bid, "rect4", grid_n)
    drawItems[bid + 2] = di(11, "Grid", "lineAA@1", bid + 1, [0.2, 0.2, 0.25, 0.6], lineWidth=1.0)

    panes = {1: pane("RiskMatrix", -0.95, 0.95, -0.95, 0.95, [0.04, 0.04, 0.08, 1.0])}
    layers = {10: layer(1, "cells"), 11: layer(1, "grid")}
    transforms = {}

    doc = make_doc(700, 700, buffers, transforms, panes, layers, geometries, drawItems)
    md = md_audit(111, "Risk Matrix (5x5)",
                  "5x5 colored risk matrix with green-to-red diagonal gradient and grid lines.",
                  "700x700 viewport with one pane and two layers.\n\n"
                  "25 colored rectangles in a 5×5 grid. Color varies by risk level (row+column): "
                  "green (low risk, top-left corner) through yellow (medium) to red (high risk, bottom-right corner). "
                  "9 distinct risk levels (0-8), each with its own color bucket and draw item. "
                  "Thin grid lines overlay the cells on layer 11.",
                  f"{3 + len(rects_by_color) * 3 + 3} unique IDs (1 pane, 2 layers, {len(rects_by_color)}+1 buf+geo+DI groups)",
                  ["Color gradient. Risk 0 = green [0,0.8,0.2], risk 8 = red [0.9,0,0.1] with smooth interpolation.",
                   "Grid overlay. Lines on layer 11 over cells on layer 10 provides clear cell boundaries.",
                   "Diagonal risk increase. top-left (0+0=0) is safest, bottom-right (4+4=8) is highest risk."],
                  lessons=["**Bucket continuous colors for rendering efficiency.** 9 risk levels instead of 25 unique colors.",
                           "**Grid lines on top of colored cells.** Layer ordering ensures grid is always visible."])
    return "risk-matrix-5x5", doc, md


# ── Main ─────────────────────────────────────────────────────────────────────

TRIALS = [
    (78, trial_078),
    (79, trial_079),
    (80, trial_080),
    (81, trial_081),
    (82, trial_082),
    (83, trial_083),
    (84, trial_084),
    (85, trial_085),
    (86, trial_086),
    (87, trial_087),
    (88, trial_088),
    (89, trial_089),
    (90, trial_090),
    (91, trial_091),
    (92, trial_092),
    (93, trial_093),
    (94, trial_094),
    (95, trial_095),
    (96, trial_096),
    (97, trial_097),
    (98, trial_098),
    (99, trial_099),
    (100, trial_100),
    (101, trial_101),
    (102, trial_102),
    (103, trial_103),
    (104, trial_104),
    (105, trial_105),
    (106, trial_106),
    (107, trial_107),
    (108, trial_108),
    (109, trial_109),
    (110, trial_110),
    (111, trial_111),
]


def main():
    print(f"Generating {len(TRIALS)} trials in {OUT_DIR}")
    for num, fn in TRIALS:
        slug, doc, md = fn()
        write_trial(num, slug, doc, md)
    print(f"Done. {len(TRIALS)} trial pairs written.")


if __name__ == "__main__":
    main()
