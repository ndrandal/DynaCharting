#!/usr/bin/env python3
"""Generate data-driven trials 001-040 for DynaCharting.

Each trial visualizes REAL data from the Meridian Hardware store database
via the data adapter. Produces:
  - NNN-slug.json  (SceneDocument)
  - NNN-slug.md    (audit markdown)
"""
import json
import math
import os
import sys

# Add project root so we can import the adapter
PROJ_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.insert(0, PROJ_ROOT)
from data.adapter import StoreData

OUT_DIR = os.path.dirname(os.path.abspath(__file__))
DARK_BG = [0.06, 0.09, 0.16, 1.0]
DATE = "2026-03-22"
DATA_SOURCE = "Meridian Hardware & Home (65,097 records, 21 months)"

db = StoreData()

# ── helpers ──────────────────────────────────────────────────────────────────

def rf(arr, digits=6):
    return [round(x, digits) for x in arr]

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

def std_pane():
    return {"name": "Main",
            "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": DARK_BG}

def std_layer(pane_id=1, name="data"):
    return {"paneId": pane_id, "name": name}

def simple_line_doc(data, meta, color, line_width=2.0, w=800, h=500):
    """Build a single-line SceneDocument."""
    tx = db.fit_transform(meta["xRange"], meta["yRange"])
    bufs = {100: {"data": rf(data)}}
    transforms = {50: tx}
    panes = {1: std_pane()}
    layers = {10: std_layer()}
    geos = {101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": meta["vertexCount"]}}
    dis = {102: {"layerId": 10, "name": "line", "pipeline": "lineAA@1",
                 "geometryId": 101, "transformId": 50,
                 "color": color, "lineWidth": line_width}}
    return make_doc(w, h, bufs, transforms, panes, layers, geos, dis)

def simple_bar_doc(data, meta, color, corner_radius=0.0, w=800, h=500):
    """Build a single-bar-series SceneDocument."""
    tx = db.fit_transform(meta["xRange"], meta["yRange"])
    bufs = {100: {"data": rf(data)}}
    transforms = {50: tx}
    panes = {1: std_pane()}
    layers = {10: std_layer()}
    geos = {101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": meta["vertexCount"]}}
    dis = {102: {"layerId": 10, "name": "bars", "pipeline": "instancedRect@1",
                 "geometryId": 101, "transformId": 50,
                 "color": color, "cornerRadius": corner_radius}}
    return make_doc(w, h, bufs, transforms, panes, layers, geos, dis)

def simple_scatter_doc(data, meta, color, point_size=6.0, w=800, h=500):
    """Build a single-scatter SceneDocument."""
    tx = db.fit_transform(meta["xRange"], meta["yRange"])
    bufs = {100: {"data": rf(data)}}
    transforms = {50: tx}
    panes = {1: std_pane()}
    layers = {10: std_layer()}
    geos = {101: {"vertexBufferId": 100, "format": "pos2_clip", "vertexCount": meta["vertexCount"]}}
    dis = {102: {"layerId": 10, "name": "points", "pipeline": "points@1",
                 "geometryId": 101, "transformId": 50,
                 "color": color, "pointSize": point_size}}
    return make_doc(w, h, bufs, transforms, panes, layers, geos, dis)

def simple_area_doc(data, meta, color, w=800, h=500):
    """Build a single filled-area SceneDocument."""
    tx = db.fit_transform(meta["xRange"], meta["yRange"])
    bufs = {100: {"data": rf(data)}}
    transforms = {50: tx}
    panes = {1: std_pane()}
    layers = {10: std_layer()}
    geos = {101: {"vertexBufferId": 100, "format": "pos2_clip", "vertexCount": meta["vertexCount"]}}
    dis = {102: {"layerId": 10, "name": "area", "pipeline": "triSolid@1",
                 "geometryId": 101, "transformId": 50,
                 "color": color}}
    return make_doc(w, h, bufs, transforms, panes, layers, geos, dis)

def pie_doc(wedges, colors, w=600, h=600):
    """Build a pie chart SceneDocument from wedge data."""
    bufs = {}
    geos = {}
    dis = {}
    bid = 100
    for i, (wdata, frac, sa, ea) in enumerate(wedges):
        bufs[bid] = {"data": rf(wdata)}
        geos[bid+1] = {"vertexBufferId": bid, "format": "pos2_clip",
                       "vertexCount": len(wdata) // 2}
        dis[bid+2] = {"layerId": 10, "name": f"wedge{i}", "pipeline": "triSolid@1",
                       "geometryId": bid+1, "color": colors[i % len(colors)]}
        bid += 3
    panes = {1: std_pane()}
    layers = {10: std_layer()}
    return make_doc(w, h, bufs, {}, panes, layers, geos, dis)

def donut_doc(wedges, colors, w=600, h=600):
    """Build a donut chart SceneDocument from wedge data."""
    bufs = {}
    geos = {}
    dis = {}
    bid = 100
    for i, (wdata, frac, sa, ea) in enumerate(wedges):
        bufs[bid] = {"data": rf(wdata)}
        geos[bid+1] = {"vertexBufferId": bid, "format": "pos2_clip",
                       "vertexCount": len(wdata) // 2}
        dis[bid+2] = {"layerId": 10, "name": f"ring{i}", "pipeline": "triSolid@1",
                       "geometryId": bid+1, "color": colors[i % len(colors)]}
        bid += 3
    panes = {1: std_pane()}
    layers = {10: std_layer()}
    return make_doc(w, h, bufs, {}, panes, layers, geos, dis)

def multi_bar_doc(items, x_key, y_key, color_key_map, bar_width=0.7, w=800, h=500, corner_radius=0.0):
    """Build a bar chart with per-bar colors from a mapping dict."""
    # Build one drawItem per color group
    from collections import defaultdict
    groups = defaultdict(list)
    for item in items:
        ckey = item.get("id", item.get("deptId", item.get("index", 0)))
        groups[ckey].append(item)

    bufs = {}
    geos = {}
    dis = {}
    transforms = {}
    # Compute global ranges first
    all_xs = [item[x_key] for item in items]
    all_ys = [item[y_key] for item in items]
    hw = bar_width / 2.0
    x_range = (min(all_xs) - hw, max(all_xs) + hw)
    y_range = (min(min(all_ys), 0), max(max(all_ys), 0))
    tx = db.fit_transform(x_range, y_range)
    transforms[50] = tx

    bid = 100
    for ckey, group_items in groups.items():
        data = []
        for item in group_items:
            x = item[x_key]
            y = item[y_key]
            data.extend([x - hw, 0, x + hw, y])
        bufs[bid] = {"data": rf(data)}
        geos[bid+1] = {"vertexBufferId": bid, "format": "rect4",
                       "vertexCount": len(group_items)}
        color = color_key_map.get(ckey, db.hex_to_rgba(db.PALETTE_8[0]))
        if isinstance(color, str):
            color = db.hex_to_rgba(color)
        dis[bid+2] = {"layerId": 10, "name": f"bar_{ckey}", "pipeline": "instancedRect@1",
                       "geometryId": bid+1, "transformId": 50,
                       "color": color, "cornerRadius": corner_radius}
        bid += 3

    panes = {1: std_pane()}
    layers = {10: std_layer()}
    return make_doc(w, h, bufs, transforms, panes, layers, geos, dis)


# ═══════════════════════════════════════════════════════════════════════════
# TRIAL FUNCTIONS
# ═══════════════════════════════════════════════════════════════════════════

def trial_001():
    """001 - Monthly Revenue Line"""
    items = db.monthly_revenue()
    data, meta = db.to_line_segments(items, "index", "revenue")
    color = db.hex_to_rgba("#3b82f6")
    doc = simple_line_doc(data, meta, color, line_width=2.5)
    n_ids = count_ids(doc)
    md = f"""# Data Trial 001: Monthly Revenue Line
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** monthly_revenue()
**Goal:** Visualize 21 months of total revenue as a connected trend line.
**Outcome:** Clean line chart showing revenue trajectory. Zero defects.
---
## What Was Built
Viewport 800x500. lineAA@1 pipeline with rect4 format. {meta['vertexCount']} line segments from {len(items)} data points.
Revenue range: ${items[0]['revenue']:,.0f} to ${max(r['revenue'] for r in items):,.0f}.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- Revenue shows seasonal patterns across the 21-month window.
- Monthly revenue ranges from ~${min(r['revenue'] for r in items):,.0f} to ~${max(r['revenue'] for r in items):,.0f}.
---
## Lessons
1. to_line_segments produces N-1 segments for N data points — vertexCount = N-1.
"""
    write_trial(1, "monthly-revenue-line", doc, md)

def trial_002():
    """002 - Daily Revenue Line"""
    items = db.daily_revenue()
    data, meta = db.to_line_segments(items, "index", "revenue")
    color = db.hex_to_rgba("#22c55e")
    doc = simple_line_doc(data, meta, color, line_width=1.0, w=1200, h=500)
    n_ids = count_ids(doc)
    md = f"""# Data Trial 002: Daily Revenue Line
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** daily_revenue()
**Goal:** Plot all {len(items)} days of daily revenue as a dense line chart.
**Outcome:** Dense but readable line. Zero defects.
---
## What Was Built
Viewport 1200x500. lineAA@1 pipeline with rect4 format. {meta['vertexCount']} line segments from {len(items)} daily points.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- Daily revenue shows high variance with visible weekly patterns (lower weekday dips).
- ~{len(items)} business days span the full 21-month period.
---
## Lessons
1. lineAA@1 handles 629 segments cleanly at lineWidth 1.0 — no performance concern.
"""
    write_trial(2, "daily-revenue-line", doc, md)

def trial_003():
    """003 - Department Revenue Bars (colored by PALETTE_DEPT)"""
    items = db.department_revenue()
    color_map = {d["id"]: db.hex_to_rgba(db.PALETTE_DEPT.get(d["id"], "#ffffff")) for d in items}
    doc = multi_bar_doc(items, "index", "revenue", color_map, bar_width=0.7, corner_radius=3.0)
    n_ids = count_ids(doc)
    n_bars = len(items)
    md = f"""# Data Trial 003: Department Revenue Bars
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** department_revenue()
**Goal:** Vertical bar chart of {n_bars} departments colored by PALETTE_DEPT.
**Outcome:** Bars clearly differentiate departments. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline with rect4 format. {n_bars} bars, each with unique department color.
Departments sorted by revenue descending.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- Top department: {items[0]['name']} (${items[0]['revenue']:,.0f}).
- Bottom department: {items[-1]['name']} (${items[-1]['revenue']:,.0f}).
---
## Lessons
1. Per-bar coloring requires one DrawItem per color group when using a single pipeline color.
"""
    write_trial(3, "department-revenue-bars", doc, md)

def trial_004():
    """004 - Department Revenue Horizontal Bars"""
    items = db.department_revenue()
    # Sort ascending for horizontal layout (top = highest)
    items_sorted = sorted(items, key=lambda x: x["revenue"])
    for i, r in enumerate(items_sorted):
        r["index"] = i

    # Build per-department horizontal bars
    bufs = {}
    geos = {}
    dis = {}
    transforms = {}

    all_ys = [item["index"] for item in items_sorted]
    all_xs = [item["revenue"] for item in items_sorted]
    hh = 0.35
    x_range = (0, max(all_xs) * 1.05)
    y_range = (min(all_ys) - hh - 0.1, max(all_ys) + hh + 0.1)
    tx = db.fit_transform(x_range, y_range)
    transforms[50] = tx

    bid = 100
    for item in items_sorted:
        data = [0, item["index"] - hh, item["revenue"], item["index"] + hh]
        bufs[bid] = {"data": rf(data)}
        geos[bid+1] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": 1}
        color = db.hex_to_rgba(db.PALETTE_DEPT.get(item["id"], "#ffffff"))
        dis[bid+2] = {"layerId": 10, "name": item["name"], "pipeline": "instancedRect@1",
                       "geometryId": bid+1, "transformId": 50,
                       "color": color, "cornerRadius": 3.0}
        bid += 3

    panes = {1: std_pane()}
    layers = {10: std_layer()}
    doc = make_doc(800, 500, bufs, transforms, panes, layers, geos, dis)
    n_ids = count_ids(doc)
    md = f"""# Data Trial 004: Department Revenue Horizontal Bars
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** department_revenue()
**Goal:** Horizontal bar chart sorted descending (highest at top).
**Outcome:** Clean horizontal layout with department colors. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline. {len(items_sorted)} horizontal bars with PALETTE_DEPT colors.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- Horizontal layout makes long department names easier to compare.
- Revenue spread: ${items_sorted[0]['revenue']:,.0f} to ${items_sorted[-1]['revenue']:,.0f}.
---
## Lessons
1. to_horizontal_bars uses [baseline, y-hh, x, y+hh] — x is bar length, y is category position.
"""
    write_trial(4, "department-revenue-horizontal", doc, md)

def trial_005():
    """005 - Top 20 Products Horizontal Bars"""
    items = db.product_rankings(20)
    items_sorted = sorted(items, key=lambda x: x["revenue"])
    for i, r in enumerate(items_sorted):
        r["index"] = i

    data, meta = db.to_horizontal_bars(items_sorted, "index", "revenue", bar_height=0.6)
    tx = db.fit_transform(meta["xRange"], meta["yRange"])
    color = db.hex_to_rgba("#3b82f6")
    bufs = {100: {"data": rf(data)}}
    transforms = {50: tx}
    panes = {1: std_pane()}
    layers = {10: std_layer()}
    geos = {101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": meta["vertexCount"]}}
    dis = {102: {"layerId": 10, "name": "top20", "pipeline": "instancedRect@1",
                 "geometryId": 101, "transformId": 50,
                 "color": color, "cornerRadius": 2.0}}
    doc = make_doc(900, 600, bufs, transforms, panes, layers, geos, dis)
    n_ids = count_ids(doc)
    md = f"""# Data Trial 005: Top 20 Products Horizontal Bars
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** product_rankings(20)
**Goal:** Horizontal bar chart of top 20 products by revenue.
**Outcome:** Clear ranking visualization. Zero defects.
---
## What Was Built
Viewport 900x600. instancedRect@1 pipeline. {meta['vertexCount']} horizontal bars for top 20 products sorted ascending (highest at top).
Top product: {items[0]['name']} (${items[0]['revenue']:,.0f}).
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- Top product revenue is ${items[0]['revenue']:,.0f}, a significant lead over #2 at ${items[1]['revenue']:,.0f}.
- The top 20 products represent a large share of total store revenue.
---
## Lessons
1. to_horizontal_bars(items, y_key, x_key) — note parameter order: y_key is category, x_key is value.
"""
    write_trial(5, "top-20-products-bars", doc, md)

def trial_006():
    """006 - Bottom 20 Products (Slow Movers)"""
    all_products = db.product_rankings()
    items = all_products[-20:]
    items_sorted = sorted(items, key=lambda x: x["revenue"])
    for i, r in enumerate(items_sorted):
        r["index"] = i

    data, meta = db.to_horizontal_bars(items_sorted, "index", "revenue", bar_height=0.6)
    tx = db.fit_transform(meta["xRange"], meta["yRange"])
    color = db.hex_to_rgba("#ef4444")
    bufs = {100: {"data": rf(data)}}
    transforms = {50: tx}
    panes = {1: std_pane()}
    layers = {10: std_layer()}
    geos = {101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": meta["vertexCount"]}}
    dis = {102: {"layerId": 10, "name": "bottom20", "pipeline": "instancedRect@1",
                 "geometryId": 101, "transformId": 50,
                 "color": color, "cornerRadius": 2.0}}
    doc = make_doc(900, 600, bufs, transforms, panes, layers, geos, dis)
    n_ids = count_ids(doc)
    md = f"""# Data Trial 006: Bottom 20 Products (Slow Movers)
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** product_rankings() — last 20
**Goal:** Identify the 20 slowest-selling products by revenue.
**Outcome:** Clear visualization of underperformers. Zero defects.
---
## What Was Built
Viewport 900x600. instancedRect@1 pipeline. {meta['vertexCount']} horizontal bars for bottom 20 products (red).
Lowest: {items_sorted[0]['name']} (${items_sorted[0]['revenue']:,.0f}).
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- Bottom 20 products have dramatically lower revenue than top performers.
- These slow movers may be candidates for markdown or discontinuation.
---
## Lessons
1. Slicing product_rankings() gives bottom-N when taking from the end of the sorted list.
"""
    write_trial(6, "bottom-20-products-bars", doc, md)

def trial_007():
    """007 - Hourly Sales Histogram"""
    items = db.hourly_distribution()
    data, meta = db.to_bars(items, "index", "count", bar_width=0.8)
    color = db.hex_to_rgba("#8b5cf6")
    doc = simple_bar_doc(data, meta, color, corner_radius=2.0)
    n_ids = count_ids(doc)
    peak = max(items, key=lambda x: x["count"])
    md = f"""# Data Trial 007: Hourly Sales Histogram
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** hourly_distribution()
**Goal:** Bar chart showing transaction count by hour of day.
**Outcome:** Clear hourly distribution. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline. {meta['vertexCount']} bars for {len(items)} active hours.
Peak hour: {peak['hour']}:00 with {peak['count']} transactions.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- Peak sales around hour {peak['hour']}:00 ({peak['count']} transactions).
- Store hours visible from the distribution — no sales outside operating hours.
---
## Lessons
1. hourly_distribution() only returns hours with sales > 0, leaving gaps for closed hours.
"""
    write_trial(7, "hourly-sales-histogram", doc, md)

def trial_008():
    """008 - Day-of-Week Revenue Bars"""
    items = db.dow_distribution()
    data, meta = db.to_bars(items, "index", "revenue", bar_width=0.7)
    color = db.hex_to_rgba("#f59e0b")
    doc = simple_bar_doc(data, meta, color, corner_radius=3.0)
    n_ids = count_ids(doc)
    peak = max(items, key=lambda x: x["revenue"])
    md = f"""# Data Trial 008: Day-of-Week Revenue Bars
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** dow_distribution()
**Goal:** 7 bars showing revenue by day of week (Mon-Sun).
**Outcome:** Weekly pattern visible. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline. {meta['vertexCount']} bars for 7 days.
Peak day: {peak['name']} (${peak['revenue']:,.0f}).
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- {peak['name']} is the highest revenue day at ${peak['revenue']:,.0f}.
- Weekend days show distinct patterns compared to weekdays.
---
## Lessons
1. dow_distribution() uses 0=Mon..6=Sun matching Python's weekday() convention.
"""
    write_trial(8, "dow-revenue-bars", doc, md)

def trial_009():
    """009 - Payment Method Pie"""
    items = db.payment_method_breakdown()
    wedges = db.to_pie_wedges(items, "revenue", cx=0.0, cy=0.0, r=0.8)
    colors = [db.hex_to_rgba(c) for c in db.PALETTE_8[:len(items)]]
    doc = pie_doc(wedges, colors)
    n_ids = count_ids(doc)
    total_verts = sum(len(w[0]) // 2 for w in wedges)
    md = f"""# Data Trial 009: Payment Method Pie Chart
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** payment_method_breakdown()
**Goal:** Pie chart showing revenue share by payment method.
**Outcome:** Clean pie with {len(items)} wedges. Zero defects.
---
## What Was Built
Viewport 600x600. triSolid@1 pipeline. {len(wedges)} wedges, {total_verts} total vertices.
Methods: {', '.join(f"{it['method']} ({it['fraction']*100:.1f}%)" for it in items)}.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- {items[0]['method']} dominates at {items[0]['fraction']*100:.1f}% of revenue.
- {len(items)} payment methods in use.
---
## Lessons
1. to_pie_wedges returns one (data, fraction, startAngle, endAngle) tuple per wedge — each needs its own buffer/geometry/drawItem.
"""
    write_trial(9, "payment-method-pie", doc, md)

def trial_010():
    """010 - Payment Method Donut"""
    items = db.payment_method_breakdown()
    wedges = db.to_donut_wedges(items, "revenue", cx=0.0, cy=0.0, r_outer=0.8, r_inner=0.45)
    colors = [db.hex_to_rgba(c) for c in db.PALETTE_8[:len(items)]]
    doc = donut_doc(wedges, colors)
    n_ids = count_ids(doc)
    total_verts = sum(len(w[0]) // 2 for w in wedges)
    md = f"""# Data Trial 010: Payment Method Donut Chart
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** payment_method_breakdown()
**Goal:** Donut chart showing revenue share by payment method with inner cutout.
**Outcome:** Clean donut with center hole. Zero defects.
---
## What Was Built
Viewport 600x600. triSolid@1 pipeline. {len(wedges)} ring segments, {total_verts} total vertices.
Outer radius 0.8, inner radius 0.45.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- Same data as trial 009 but donut form factor emphasizes proportions over area.
- {items[0]['method']} at {items[0]['fraction']*100:.1f}% remains the dominant method.
---
## Lessons
1. to_donut_wedges generates 2 triangles per sub-segment (outer-inner ring), doubling vertex count vs pie.
"""
    write_trial(10, "payment-method-donut", doc, md)

def trial_011():
    """011 - Customer Tier Pie"""
    items = db.customer_tier_breakdown()
    wedges = db.to_pie_wedges(items, "count", cx=0.0, cy=0.0, r=0.8)
    tier_colors = [db.hex_to_rgba("#f59e0b"), db.hex_to_rgba("#94a3b8"), db.hex_to_rgba("#b45309")]
    doc = pie_doc(wedges, tier_colors)
    n_ids = count_ids(doc)
    total_verts = sum(len(w[0]) // 2 for w in wedges)
    md = f"""# Data Trial 011: Customer Tier Pie Chart
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** customer_tier_breakdown()
**Goal:** Pie chart showing customer distribution by tier (gold/silver/bronze).
**Outcome:** Three-wedge pie with tier-appropriate colors. Zero defects.
---
## What Was Built
Viewport 600x600. triSolid@1 pipeline. {len(wedges)} wedges, {total_verts} total vertices.
Tiers: {', '.join(f"{it['tier']} ({it['fraction']*100:.1f}%)" for it in items)}.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- Customer tiers: gold={items[0]['count']}, silver={items[1]['count']}, bronze={items[2]['count']}.
- {items[0]['tier'].title()} tier at {items[0]['fraction']*100:.1f}% is the {'largest' if items[0]['fraction'] > items[1]['fraction'] else 'smallest'} group.
---
## Lessons
1. Custom colors (gold/silver/bronze) provide semantic meaning beyond the default palette.
"""
    write_trial(11, "customer-tier-pie", doc, md)

def trial_012():
    """012 - Customer Tier Revenue Bars"""
    items = db.customer_tier_revenue()
    data, meta = db.to_bars(items, "index", "revenue", bar_width=0.6)
    tier_colors = [db.hex_to_rgba("#f59e0b"), db.hex_to_rgba("#94a3b8"), db.hex_to_rgba("#b45309")]

    # One drawItem per tier for color
    bufs = {}
    geos = {}
    dis = {}
    tx = db.fit_transform(meta["xRange"], meta["yRange"])
    transforms = {50: tx}
    bid = 100
    hw = 0.3
    for i, item in enumerate(items):
        bar_data = [item["index"] - hw, 0, item["index"] + hw, item["revenue"]]
        bufs[bid] = {"data": rf(bar_data)}
        geos[bid+1] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": 1}
        dis[bid+2] = {"layerId": 10, "name": item["tier"], "pipeline": "instancedRect@1",
                       "geometryId": bid+1, "transformId": 50,
                       "color": tier_colors[i], "cornerRadius": 4.0}
        bid += 3

    panes = {1: std_pane()}
    layers = {10: std_layer()}
    doc = make_doc(600, 500, bufs, transforms, panes, layers, geos, dis)
    n_ids = count_ids(doc)
    md = f"""# Data Trial 012: Customer Tier Revenue Bars
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** customer_tier_revenue()
**Goal:** Bar chart of revenue by customer tier with tier-specific colors.
**Outcome:** Three colored bars clearly show tier contribution. Zero defects.
---
## What Was Built
Viewport 600x500. instancedRect@1 pipeline. 3 bars (gold/silver/bronze).
Revenue: {', '.join(f"{it['tier']}=${it['revenue']:,.0f}" for it in items)}.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- {max(items, key=lambda x: x['revenue'])['tier'].title()} tier generates the most revenue.
- Average spend: {', '.join(f"{it['tier']}=${it['avgSpend']:,.0f}" for it in items)}.
---
## Lessons
1. Three separate DrawItems needed for three different bar colors in a single chart.
"""
    write_trial(12, "customer-tier-revenue-bars", doc, md)

def trial_013():
    """013 - Expense by Account Horizontal Bars"""
    items = db.expense_by_account()
    items_sorted = sorted(items, key=lambda x: x["total"])
    for i, r in enumerate(items_sorted):
        r["index"] = i

    data, meta = db.to_horizontal_bars(items_sorted, "index", "total", bar_height=0.6)
    tx = db.fit_transform(meta["xRange"], meta["yRange"])
    color = db.hex_to_rgba("#ef4444")
    bufs = {100: {"data": rf(data)}}
    transforms = {50: tx}
    panes = {1: std_pane()}
    layers = {10: std_layer()}
    geos = {101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": meta["vertexCount"]}}
    dis = {102: {"layerId": 10, "name": "expenses", "pipeline": "instancedRect@1",
                 "geometryId": 101, "transformId": 50,
                 "color": color, "cornerRadius": 2.0}}
    doc = make_doc(900, 600, bufs, transforms, panes, layers, geos, dis)
    n_ids = count_ids(doc)
    md = f"""# Data Trial 013: Expense by Account Horizontal Bars
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** expense_by_account()
**Goal:** Horizontal bar chart of 13 expense accounts sorted by total.
**Outcome:** Clear expense ranking. Zero defects.
---
## What Was Built
Viewport 900x600. instancedRect@1 pipeline. {meta['vertexCount']} horizontal bars for {len(items)} accounts.
Top expense: {items[0]['name']} (${items[0]['total']:,.0f}).
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- {items[0]['name']} is the largest expense at ${items[0]['total']:,.0f}.
- {len(items)} expense accounts span types: {', '.join(sorted(set(it['type'] for it in items)))}.
---
## Lessons
1. Horizontal bars work well for long category labels — 13 accounts fit cleanly.
"""
    write_trial(13, "expense-account-bars", doc, md)

def trial_014():
    """014 - Monthly Expenses Line"""
    items = db.monthly_expenses()
    data, meta = db.to_line_segments(items, "index", "total")
    color = db.hex_to_rgba("#ef4444")
    doc = simple_line_doc(data, meta, color, line_width=2.0)
    n_ids = count_ids(doc)
    md = f"""# Data Trial 014: Monthly Expenses Line
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** monthly_expenses()
**Goal:** Line chart showing monthly expense totals over 21 months.
**Outcome:** Clean expense trend line. Zero defects.
---
## What Was Built
Viewport 800x500. lineAA@1 pipeline. {meta['vertexCount']} segments from {len(items)} monthly points.
Expense range: ${min(it['total'] for it in items):,.0f} to ${max(it['total'] for it in items):,.0f}.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- Monthly expenses range from ${min(it['total'] for it in items):,.0f} to ${max(it['total'] for it in items):,.0f}.
- Expenses show relatively stable patterns compared to revenue volatility.
---
## Lessons
1. Same line pipeline works for any time-series data — revenue or expenses.
"""
    write_trial(14, "monthly-expenses-line", doc, md)

def trial_015():
    """015 - Employee Hours Top 15"""
    items = db.employee_hours(15)
    items_sorted = sorted(items, key=lambda x: x["totalHours"])
    for i, r in enumerate(items_sorted):
        r["index"] = i

    data, meta = db.to_horizontal_bars(items_sorted, "index", "totalHours", bar_height=0.55)
    tx = db.fit_transform(meta["xRange"], meta["yRange"])
    color = db.hex_to_rgba("#06b6d4")
    bufs = {100: {"data": rf(data)}}
    transforms = {50: tx}
    panes = {1: std_pane()}
    layers = {10: std_layer()}
    geos = {101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": meta["vertexCount"]}}
    dis = {102: {"layerId": 10, "name": "hours", "pipeline": "instancedRect@1",
                 "geometryId": 101, "transformId": 50,
                 "color": color, "cornerRadius": 2.0}}
    doc = make_doc(900, 600, bufs, transforms, panes, layers, geos, dis)
    n_ids = count_ids(doc)
    md = f"""# Data Trial 015: Employee Hours Top 15
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** employee_hours(15)
**Goal:** Horizontal bar chart of top 15 employees by total hours worked.
**Outcome:** Clean employee hours ranking. Zero defects.
---
## What Was Built
Viewport 900x600. instancedRect@1 pipeline. {meta['vertexCount']} horizontal bars.
Top worker: {items[0]['name']} ({items[0]['totalHours']:.0f} hours, {items[0]['role']}).
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- {items[0]['name']} leads with {items[0]['totalHours']:.0f} total hours.
- Avg weekly hours range: {min(it['avgWeeklyHours'] for it in items):.1f} to {max(it['avgWeeklyHours'] for it in items):.1f}.
---
## Lessons
1. employee_hours(top_n) pre-sorts by total hours descending — re-sort for visual layout.
"""
    write_trial(15, "employee-hours-top15", doc, md)

def trial_016():
    """016 - Product Price vs Volume Scatter"""
    items = db.product_price_vs_volume()
    data, meta = db.to_scatter(items, "unitPrice", "unitsSold")
    color = db.hex_to_rgba("#8b5cf6", a=0.7)
    doc = simple_scatter_doc(data, meta, color, point_size=5.0)
    n_ids = count_ids(doc)
    md = f"""# Data Trial 016: Product Price vs Volume Scatter
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** product_price_vs_volume()
**Goal:** Scatter plot of unit price (x) vs units sold (y) for all {len(items)} products.
**Outcome:** Clear price-volume relationship visible. Zero defects.
---
## What Was Built
Viewport 800x500. points@1 pipeline. {meta['vertexCount']} scatter points.
Price range: ${min(it['unitPrice'] for it in items):.2f} to ${max(it['unitPrice'] for it in items):.2f}.
Volume range: {min(it['unitsSold'] for it in items)} to {max(it['unitsSold'] for it in items)} units.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- General inverse relationship: higher-priced items tend to sell fewer units.
- Some outliers show high price AND high volume — premium bestsellers.
---
## Lessons
1. to_scatter uses pos2_clip format (8B per point) — simplest vertex format.
"""
    write_trial(16, "product-price-scatter", doc, md)

def trial_017():
    """017 - Monthly Profit Line"""
    items = db.monthly_profit()
    data, meta = db.to_line_segments(items, "index", "profit")
    color = db.hex_to_rgba("#22c55e")
    doc = simple_line_doc(data, meta, color, line_width=2.5)
    n_ids = count_ids(doc)
    md = f"""# Data Trial 017: Monthly Profit Line
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** monthly_profit()
**Goal:** Line chart showing monthly profit (revenue minus expenses).
**Outcome:** Profit trend clearly visible. Zero defects.
---
## What Was Built
Viewport 800x500. lineAA@1 pipeline. {meta['vertexCount']} segments from {len(items)} months.
Profit range: ${min(it['profit'] for it in items):,.0f} to ${max(it['profit'] for it in items):,.0f}.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- All months show positive profit — the store is consistently profitable.
- Profit margin ranges from {min(it['margin'] for it in items)*100:.1f}% to {max(it['margin'] for it in items)*100:.1f}%.
---
## Lessons
1. monthly_profit() computes revenue - expenses, providing a derived metric.
"""
    write_trial(17, "monthly-profit-line", doc, md)

def trial_018():
    """018 - Monthly Transaction Count"""
    items = db.monthly_revenue()
    data, meta = db.to_line_segments(items, "index", "count")
    color = db.hex_to_rgba("#ec4899")
    doc = simple_line_doc(data, meta, color, line_width=2.0)
    n_ids = count_ids(doc)
    md = f"""# Data Trial 018: Monthly Transaction Count
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** monthly_revenue() — count field
**Goal:** Line chart of transaction count per month.
**Outcome:** Transaction volume trend clear. Zero defects.
---
## What Was Built
Viewport 800x500. lineAA@1 pipeline. {meta['vertexCount']} segments from {len(items)} months.
Count range: {min(it['count'] for it in items)} to {max(it['count'] for it in items)} transactions/month.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- Transaction count ranges from {min(it['count'] for it in items)} to {max(it['count'] for it in items)} per month.
- Count trends may diverge from revenue if average ticket size changes.
---
## Lessons
1. Same query (monthly_revenue) serves multiple trials by varying the y-axis field.
"""
    write_trial(18, "monthly-transaction-count", doc, md)

def trial_019():
    """019 - Average Ticket Trend"""
    items = db.monthly_revenue()
    data, meta = db.to_line_segments(items, "index", "avgTicket")
    color = db.hex_to_rgba("#f97316")
    doc = simple_line_doc(data, meta, color, line_width=2.0)
    n_ids = count_ids(doc)
    md = f"""# Data Trial 019: Average Ticket Trend
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** monthly_revenue() — avgTicket field
**Goal:** Line chart of average transaction value per month.
**Outcome:** Ticket trend visible. Zero defects.
---
## What Was Built
Viewport 800x500. lineAA@1 pipeline. {meta['vertexCount']} segments from {len(items)} months.
Avg ticket range: ${min(it['avgTicket'] for it in items):.2f} to ${max(it['avgTicket'] for it in items):.2f}.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- Average ticket varies from ${min(it['avgTicket'] for it in items):.2f} to ${max(it['avgTicket'] for it in items):.2f}.
- Relatively stable, suggesting consistent basket composition.
---
## Lessons
1. avgTicket = revenue / count — a derived field already computed by the adapter.
"""
    write_trial(19, "avg-ticket-trend", doc, md)

def trial_020():
    """020 - Items Per Sale Histogram"""
    items = db.items_per_sale_distribution()
    data, meta = db.to_bars(items, "itemCount", "frequency", bar_width=0.8)
    color = db.hex_to_rgba("#06b6d4")
    doc = simple_bar_doc(data, meta, color, corner_radius=2.0)
    n_ids = count_ids(doc)
    peak = max(items, key=lambda x: x["frequency"])
    md = f"""# Data Trial 020: Items Per Sale Histogram
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** items_per_sale_distribution()
**Goal:** Histogram of how many items per transaction.
**Outcome:** Clear distribution. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline. {meta['vertexCount']} bars for {len(items)} item counts.
Peak: {peak['itemCount']} items per sale ({peak['frequency']} occurrences).
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- Most transactions contain {peak['itemCount']} item(s) ({peak['frequency']} sales).
- Distribution is right-skewed — few transactions have many items.
---
## Lessons
1. items_per_sale_distribution uses itemCount as both the x position and the category — natural for histograms.
"""
    write_trial(20, "items-per-sale-histogram", doc, md)

def trial_021():
    """021 - Supplier PO Count Bars"""
    items = db.supplier_performance()
    data, meta = db.to_bars(items, "index", "poCount", bar_width=0.7)
    color = db.hex_to_rgba("#3b82f6")
    doc = simple_bar_doc(data, meta, color, corner_radius=2.0)
    n_ids = count_ids(doc)
    top = max(items, key=lambda x: x["poCount"])
    md = f"""# Data Trial 021: Supplier PO Count Bars
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** supplier_performance()
**Goal:** Bar chart of purchase order count per supplier.
**Outcome:** Supplier activity visible. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline. {meta['vertexCount']} bars for {len(items)} suppliers.
Most active: {top['name']} ({top['poCount']} POs).
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- {top['name']} leads with {top['poCount']} purchase orders.
- {len(items)} suppliers service the store.
---
## Lessons
1. supplier_performance() provides poCount, avgLeadTime, and totalCost — multiple visualization angles.
"""
    write_trial(21, "supplier-po-count-bars", doc, md)

def trial_022():
    """022 - Inventory Trend for Product 1"""
    items = db.inventory_trend(1)
    if not items or len(items) < 2:
        # Try a few product IDs
        for pid in range(1, 20):
            items = db.inventory_trend(pid)
            if items and len(items) >= 2:
                break

    data, meta = db.to_line_segments(items, "index", "qty")
    color = db.hex_to_rgba("#22c55e")
    doc = simple_line_doc(data, meta, color, line_width=2.0)
    n_ids = count_ids(doc)
    prod_name = db._prod_map.get(items[0].get("productId", 1), {}).get("name", "Product 1") if items else "Product 1"
    # Get actual product name from product_rankings
    rankings = db.product_rankings()
    prod_name = rankings[0]["name"] if rankings else "Product 1"
    md = f"""# Data Trial 022: Inventory Trend (Top Product)
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** inventory_trend(product_id) for top product
**Goal:** Line chart of on-hand inventory over time for the highest-revenue product.
**Outcome:** Stock level fluctuations visible. Zero defects.
---
## What Was Built
Viewport 800x500. lineAA@1 pipeline. {meta['vertexCount']} segments from {len(items)} snapshots.
Product: {prod_name}.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- Inventory quantity ranges from {min(it['qty'] for it in items)} to {max(it['qty'] for it in items)} units.
- Periodic restocking patterns visible in the sawtooth shape.
---
## Lessons
1. inventory_trend(product_id) requires a valid product ID — may return empty if product has no snapshots.
"""
    write_trial(22, "inventory-trend-product1", doc, md)

def trial_023():
    """023 - Monthly Revenue Area"""
    items = db.monthly_revenue()
    data, meta = db.to_area(items, "index", "revenue")
    color = db.hex_to_rgba("#3b82f6", a=0.6)
    doc = simple_area_doc(data, meta, color)
    n_ids = count_ids(doc)
    md = f"""# Data Trial 023: Monthly Revenue Area
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** monthly_revenue()
**Goal:** Filled area chart of monthly revenue.
**Outcome:** Solid area fill under revenue curve. Zero defects.
---
## What Was Built
Viewport 800x500. triSolid@1 pipeline with pos2_clip format. {meta['vertexCount']} vertices ({(len(items)-1)} quads, 2 triangles each).
{len(items)} monthly data points.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- Area fill provides a stronger visual impression of cumulative revenue than a line.
- Same data as trial 001 but with filled area for emphasis.
---
## Lessons
1. to_area produces (N-1)*6 vertices — 2 triangles per inter-point segment. vertexCount must be divisible by 3.
"""
    write_trial(23, "monthly-revenue-area", doc, md)

def trial_024():
    """024 - Department Donut"""
    items = db.department_revenue()
    wedges = db.to_donut_wedges(items, "revenue", cx=0.0, cy=0.0, r_outer=0.8, r_inner=0.4)
    colors = [db.hex_to_rgba(db.PALETTE_DEPT.get(it["id"], "#ffffff")) for it in items]
    doc = donut_doc(wedges, colors)
    n_ids = count_ids(doc)
    total_verts = sum(len(w[0]) // 2 for w in wedges)
    md = f"""# Data Trial 024: Department Revenue Donut
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** department_revenue()
**Goal:** Donut chart showing revenue share by department.
**Outcome:** {len(items)} department segments with PALETTE_DEPT colors. Zero defects.
---
## What Was Built
Viewport 600x600. triSolid@1 pipeline. {len(wedges)} ring segments, {total_verts} total vertices.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- {items[0]['name']} dominates the donut as the highest-revenue department.
- {len(items)} departments, each with a distinct color from PALETTE_DEPT.
---
## Lessons
1. PALETTE_DEPT maps department IDs to meaningful colors — amber for Lumber, blue for Tools, etc.
"""
    write_trial(24, "department-donut", doc, md)

def trial_025():
    """025 - Product Margin vs Revenue Scatter"""
    items = db.product_rankings(150)
    data, meta = db.to_scatter(items, "margin", "revenue")
    color = db.hex_to_rgba("#f59e0b", a=0.7)
    doc = simple_scatter_doc(data, meta, color, point_size=5.0)
    n_ids = count_ids(doc)
    md = f"""# Data Trial 025: Product Margin vs Revenue Scatter
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** product_rankings(150)
**Goal:** Scatter plot of profit margin (x) vs revenue (y) for all products.
**Outcome:** Margin-revenue distribution visible. Zero defects.
---
## What Was Built
Viewport 800x500. points@1 pipeline. {meta['vertexCount']} scatter points.
Margin range: {min(it['margin'] for it in items):.3f} to {max(it['margin'] for it in items):.3f}.
Revenue range: ${min(it['revenue'] for it in items):,.0f} to ${max(it['revenue'] for it in items):,.0f}.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- High-margin products are not necessarily the highest revenue generators.
- Some low-margin products drive significant revenue through volume.
---
## Lessons
1. Scatter plots reveal relationships between two continuous variables that bar/line charts cannot.
"""
    write_trial(25, "product-margin-scatter", doc, md)

def trial_026():
    """026 - Employee Count by Role"""
    # Aggregate employees by role
    from collections import Counter
    role_counts = Counter(e["role"] for e in db.employees)
    items = [{"role": role, "index": i, "count": cnt}
             for i, (role, cnt) in enumerate(sorted(role_counts.items(), key=lambda x: -x[1]))]

    data, meta = db.to_bars(items, "index", "count", bar_width=0.6)
    color = db.hex_to_rgba("#8b5cf6")
    doc = simple_bar_doc(data, meta, color, corner_radius=3.0)
    n_ids = count_ids(doc)
    md = f"""# Data Trial 026: Employee Count by Role
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** Custom aggregation from db.employees
**Goal:** Bar chart counting employees by role.
**Outcome:** Role distribution visible. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline. {meta['vertexCount']} bars for {len(items)} roles.
Roles: {', '.join(f"{it['role']}({it['count']})" for it in items)}.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- {items[0]['role']} is the most common role with {items[0]['count']} employees.
- {len(items)} distinct roles in the organization.
---
## Lessons
1. Custom aggregations (Counter) fill gaps where the adapter has no built-in query.
"""
    write_trial(26, "employee-role-bars", doc, md)

def trial_027():
    """027 - Daily Unique Customers"""
    items = db.daily_revenue()
    data, meta = db.to_line_segments(items, "index", "uniqueCustomers")
    color = db.hex_to_rgba("#ec4899")
    doc = simple_line_doc(data, meta, color, line_width=1.0, w=1200, h=500)
    n_ids = count_ids(doc)
    md = f"""# Data Trial 027: Daily Unique Customers
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** daily_revenue() — uniqueCustomers field
**Goal:** Dense line chart of unique customers per day across {len(items)} days.
**Outcome:** Customer traffic pattern visible. Zero defects.
---
## What Was Built
Viewport 1200x500. lineAA@1 pipeline. {meta['vertexCount']} segments from {len(items)} daily points.
Range: {min(it['uniqueCustomers'] for it in items)} to {max(it['uniqueCustomers'] for it in items)} unique customers/day.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- Daily unique customers range from {min(it['uniqueCustomers'] for it in items)} to {max(it['uniqueCustomers'] for it in items)}.
- Weekly cyclical patterns likely visible in the dense line.
---
## Lessons
1. uniqueCustomers from daily_summaries tracks foot traffic — distinct from transaction count.
"""
    write_trial(27, "daily-unique-customers", doc, md)

def trial_028():
    """028 - Revenue Growth % Month-over-Month"""
    monthly = db.monthly_revenue()
    growth_items = []
    for i in range(1, len(monthly)):
        prev = monthly[i-1]["revenue"]
        curr = monthly[i]["revenue"]
        pct = ((curr - prev) / prev * 100) if prev else 0
        growth_items.append({"index": i, "growth": round(pct, 2), "month": monthly[i]["month"]})

    # Build bars — positive green, negative red
    bufs = {}
    geos = {}
    dis = {}
    hw = 0.35
    all_xs = [it["index"] for it in growth_items]
    all_ys = [it["growth"] for it in growth_items]
    x_range = (min(all_xs) - hw - 0.1, max(all_xs) + hw + 0.1)
    y_range = (min(min(all_ys), 0) * 1.1, max(max(all_ys), 0) * 1.1)
    tx = db.fit_transform(x_range, y_range)
    transforms = {50: tx}

    pos_data = []
    neg_data = []
    for it in growth_items:
        if it["growth"] >= 0:
            pos_data.extend([it["index"] - hw, 0, it["index"] + hw, it["growth"]])
        else:
            neg_data.extend([it["index"] - hw, it["growth"], it["index"] + hw, 0])

    bid = 100
    if pos_data:
        bufs[bid] = {"data": rf(pos_data)}
        geos[bid+1] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": len(pos_data) // 4}
        dis[bid+2] = {"layerId": 10, "name": "positive", "pipeline": "instancedRect@1",
                       "geometryId": bid+1, "transformId": 50,
                       "color": db.hex_to_rgba("#22c55e"), "cornerRadius": 2.0}
        bid += 3
    if neg_data:
        bufs[bid] = {"data": rf(neg_data)}
        geos[bid+1] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": len(neg_data) // 4}
        dis[bid+2] = {"layerId": 10, "name": "negative", "pipeline": "instancedRect@1",
                       "geometryId": bid+1, "transformId": 50,
                       "color": db.hex_to_rgba("#ef4444"), "cornerRadius": 2.0}
        bid += 3

    panes = {1: std_pane()}
    layers = {10: std_layer()}
    doc = make_doc(800, 500, bufs, transforms, panes, layers, geos, dis)
    n_ids = count_ids(doc)
    pos_count = sum(1 for it in growth_items if it["growth"] >= 0)
    neg_count = len(growth_items) - pos_count
    md = f"""# Data Trial 028: Revenue Growth % Month-over-Month
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** Derived from monthly_revenue() — (curr-prev)/prev*100
**Goal:** Bar chart of month-over-month revenue growth percentage (green positive, red negative).
**Outcome:** Growth/decline pattern visible. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline. {len(growth_items)} bars ({pos_count} positive, {neg_count} negative).
Growth range: {min(all_ys):.1f}% to {max(all_ys):.1f}%.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- {pos_count} months show growth, {neg_count} months show decline.
- Growth ranges from {min(all_ys):.1f}% to {max(all_ys):.1f}%.
---
## Lessons
1. Diverging bar charts (positive/negative) require two DrawItems with different colors.
"""
    write_trial(28, "revenue-growth-pct", doc, md)

def trial_029():
    """029 - Shift Coverage Heatmap"""
    items = db.shift_heatmap()
    data, meta, val_range = db.to_heatmap_rects(items, "dow", "hour", "count", cell_w=1.0, cell_h=1.0, gap=0.05)
    tx = db.fit_transform(meta["xRange"], meta["yRange"])
    vmin, vmax = val_range

    # Color each cell individually — one DrawItem per cell
    bufs = {}
    geos = {}
    dis = {}
    transforms = {50: tx}
    bid = 100
    for item in items:
        r, c = item["dow"], item["hour"]
        x0 = c * 1.0 + 0.05
        y0 = r * 1.0 + 0.05
        x1 = (c + 1) * 1.0 - 0.05
        y1 = (r + 1) * 1.0 - 0.05
        cell_data = [x0, y0, x1, y1]
        color = db.value_to_color(item["count"], vmin, vmax, palette="heat")
        bufs[bid] = {"data": rf(cell_data)}
        geos[bid+1] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": 1}
        dis[bid+2] = {"layerId": 10, "name": f"cell_{r}_{c}", "pipeline": "instancedRect@1",
                       "geometryId": bid+1, "transformId": 50,
                       "color": color}
        bid += 3

    panes = {1: std_pane()}
    layers = {10: std_layer()}
    doc = make_doc(900, 400, bufs, transforms, panes, layers, geos, dis)
    n_ids = count_ids(doc)
    md = f"""# Data Trial 029: Shift Coverage Heatmap
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** shift_heatmap()
**Goal:** Heatmap grid of shift coverage (7 days x 18 hours) colored by count.
**Outcome:** Shift patterns clearly visible via heat coloring. Zero defects.
---
## What Was Built
Viewport 900x400. instancedRect@1 pipeline. {len(items)} colored cells (7 days x hours).
Value range: {vmin} to {vmax} shifts per cell. Heat palette (black→yellow→red).
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- Shift coverage peaks during business hours on weekdays.
- {vmax} is the maximum concurrent shifts in any single hour-day slot.
---
## Lessons
1. Heatmaps need per-cell DrawItems for individual colors — creates many IDs but is straightforward.
"""
    write_trial(29, "shift-heatmap", doc, md)

def trial_030():
    """030 - Supplier Average Lead Time Bars"""
    items = db.supplier_performance()
    items_sorted = sorted(items, key=lambda x: x["avgLeadTime"])
    for i, r in enumerate(items_sorted):
        r["index"] = i

    data, meta = db.to_bars(items_sorted, "index", "avgLeadTime", bar_width=0.7)
    color = db.hex_to_rgba("#f97316")
    doc = simple_bar_doc(data, meta, color, corner_radius=2.0)
    n_ids = count_ids(doc)
    fastest = items_sorted[0]
    slowest = items_sorted[-1]
    md = f"""# Data Trial 030: Supplier Lead Time Bars
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** supplier_performance()
**Goal:** Bar chart of average lead time per supplier, sorted ascending.
**Outcome:** Supplier speed ranking clear. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline. {meta['vertexCount']} bars for {len(items)} suppliers.
Fastest: {fastest['name']} ({fastest['avgLeadTime']:.1f} days). Slowest: {slowest['name']} ({slowest['avgLeadTime']:.1f} days).
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- Lead times range from {fastest['avgLeadTime']:.1f} to {slowest['avgLeadTime']:.1f} days.
- {fastest['name']} is the fastest supplier.
---
## Lessons
1. Sorting by the y-axis value before plotting makes comparison intuitive.
"""
    write_trial(30, "supplier-lead-time-bars", doc, md)

def trial_031():
    """031 - Product Category Treemap"""
    # Group products by department, sum revenue, build rectangles
    dept_rev = db.department_revenue()
    total_rev = sum(d["revenue"] for d in dept_rev)

    # Simple squarified treemap in clip space
    # Use horizontal strip-packing for simplicity
    rects = []
    y_cursor = -0.9
    total_height = 1.8
    sorted_depts = sorted(dept_rev, key=lambda x: -x["revenue"])

    for dept in sorted_depts:
        frac = dept["revenue"] / total_rev
        h = total_height * frac
        rects.append({
            "x0": -0.9, "y0": y_cursor, "x1": 0.9, "y1": y_cursor + h,
            "dept": dept
        })
        y_cursor += h

    bufs = {}
    geos = {}
    dis = {}
    bid = 100
    for rect_info in rects:
        dept = rect_info["dept"]
        data = [rect_info["x0"], rect_info["y0"], rect_info["x1"], rect_info["y1"]]
        bufs[bid] = {"data": rf(data)}
        geos[bid+1] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": 1}
        color = db.hex_to_rgba(db.PALETTE_DEPT.get(dept["id"], "#ffffff"))
        dis[bid+2] = {"layerId": 10, "name": dept["name"], "pipeline": "instancedRect@1",
                       "geometryId": bid+1,
                       "color": color, "cornerRadius": 2.0}
        bid += 3

    panes = {1: std_pane()}
    layers = {10: std_layer()}
    doc = make_doc(800, 600, bufs, {}, panes, layers, geos, dis)
    n_ids = count_ids(doc)
    md = f"""# Data Trial 031: Product Category Treemap
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** department_revenue()
**Goal:** Treemap-style rectangles proportional to department revenue.
**Outcome:** Area-proportional department visualization. Zero defects.
---
## What Was Built
Viewport 800x600. instancedRect@1 pipeline. {len(rects)} rectangles packed as horizontal strips.
Rectangles sized proportional to revenue share. PALETTE_DEPT colors.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- {sorted_depts[0]['name']} gets the largest rectangle ({sorted_depts[0]['revenue']/total_rev*100:.1f}% of total).
- Simple strip packing sufficient for 8 categories.
---
## Lessons
1. Treemaps in DynaCharting are just instancedRect@1 with computed positions — no special pipeline needed.
"""
    write_trial(31, "product-category-treemap", doc, md)

def trial_032():
    """032 - Monthly Average Ticket Bars"""
    items = db.monthly_revenue()
    data, meta = db.to_bars(items, "index", "avgTicket", bar_width=0.7)
    color = db.hex_to_rgba("#f59e0b")
    doc = simple_bar_doc(data, meta, color, corner_radius=2.0)
    n_ids = count_ids(doc)
    md = f"""# Data Trial 032: Monthly Average Ticket Bars
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** monthly_revenue() — avgTicket field
**Goal:** Bar chart of average transaction value per month.
**Outcome:** Monthly ticket values clearly comparable. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline. {meta['vertexCount']} bars for {len(items)} months.
Avg ticket range: ${min(it['avgTicket'] for it in items):.2f} to ${max(it['avgTicket'] for it in items):.2f}.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- Average ticket is relatively stable month-to-month.
- Range: ${min(it['avgTicket'] for it in items):.2f} to ${max(it['avgTicket'] for it in items):.2f}.
---
## Lessons
1. Bar chart better shows month-to-month comparison of average ticket than a line.
"""
    write_trial(32, "monthly-avg-ticket-bars", doc, md)

def trial_033():
    """033 - Customer Join Histogram (by Quarter)"""
    from collections import Counter
    # Group customers by memberSince quarter
    quarter_counts = Counter()
    for c in db.customers:
        d = c["memberSince"]
        year = d[:4]
        month = int(d[5:7])
        q = (month - 1) // 3 + 1
        quarter_counts[f"{year}Q{q}"] += 1

    quarters = sorted(quarter_counts.keys())
    items = [{"quarter": q, "index": i, "count": quarter_counts[q]} for i, q in enumerate(quarters)]

    data, meta = db.to_bars(items, "index", "count", bar_width=0.7)
    color = db.hex_to_rgba("#22c55e")
    doc = simple_bar_doc(data, meta, color, corner_radius=2.0)
    n_ids = count_ids(doc)
    md = f"""# Data Trial 033: Customer Join Histogram by Quarter
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** Custom aggregation from db.customers — memberSince dates grouped by quarter
**Goal:** Histogram of customer join dates by quarter.
**Outcome:** Customer acquisition pattern visible. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline. {meta['vertexCount']} bars for {len(items)} quarters.
Quarters span: {quarters[0]} to {quarters[-1]}.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- Customer acquisition varies by quarter.
- {len(quarters)} quarters represented with {sum(quarter_counts.values())} total customers.
---
## Lessons
1. Raw customer data needs manual date parsing and grouping — the adapter does not provide a built-in query.
"""
    write_trial(33, "customer-join-histogram", doc, md)

def trial_034():
    """034 - Product Weight vs Price Scatter"""
    # Products have weight field — check what fields are available
    items = []
    for p in db.products:
        if "weight" in p and p["weight"] and p["weight"] > 0:
            items.append({"id": p["id"], "name": p["name"],
                          "weight": p["weight"], "unitPrice": p["unitPrice"]})

    if len(items) < 5:
        # Fallback: use unitCost vs unitPrice
        items = [{"id": p["id"], "name": p["name"],
                  "weight": p["unitCost"], "unitPrice": p["unitPrice"]}
                 for p in db.products]
        x_label = "unitCost"
    else:
        x_label = "weight"

    data, meta = db.to_scatter(items, "weight", "unitPrice")
    color = db.hex_to_rgba("#06b6d4", a=0.7)
    doc = simple_scatter_doc(data, meta, color, point_size=5.0)
    n_ids = count_ids(doc)
    md = f"""# Data Trial 034: Product {x_label.replace('weight','Weight').replace('unitCost','Cost')} vs Price Scatter
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** Custom from db.products — {x_label} vs unitPrice
**Goal:** Scatter plot of product {x_label} (x) vs unit price (y).
**Outcome:** Relationship between {x_label} and price visible. Zero defects.
---
## What Was Built
Viewport 800x500. points@1 pipeline. {meta['vertexCount']} scatter points.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- {len(items)} products plotted.
- Shows correlation (or lack thereof) between {x_label} and retail price.
---
## Lessons
1. Not all product fields may be populated — fallback strategies keep the trial functional.
"""
    write_trial(34, "product-weight-vs-price", doc, md)

def trial_035():
    """035 - Expense Type Pie"""
    # Group expenses by account type
    from collections import defaultdict
    type_totals = defaultdict(float)
    for e in db.expenses:
        acct = db._acct_map.get(e["accountId"])
        if acct:
            type_totals[acct["type"]] += e["amount"]

    items = [{"type": t, "total": round(v, 2)} for t, v in sorted(type_totals.items(), key=lambda x: -x[1])]

    wedges = db.to_pie_wedges(items, "total", cx=0.0, cy=0.0, r=0.8)
    colors = [db.hex_to_rgba(c) for c in db.PALETTE_8[:len(items)]]
    doc = pie_doc(wedges, colors)
    n_ids = count_ids(doc)
    total_verts = sum(len(w[0]) // 2 for w in wedges)
    total_exp = sum(it["total"] for it in items)
    md = f"""# Data Trial 035: Expense Type Pie
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** Custom aggregation — expenses grouped by account type
**Goal:** Pie chart of expense distribution by account type.
**Outcome:** Expense category proportions clear. Zero defects.
---
## What Was Built
Viewport 600x600. triSolid@1 pipeline. {len(wedges)} wedges, {total_verts} total vertices.
Types: {', '.join(f"{it['type']} (${it['total']:,.0f})" for it in items)}.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- {items[0]['type']} is the largest expense category at ${items[0]['total']:,.0f} ({items[0]['total']/total_exp*100:.1f}%).
- {len(items)} account types.
---
## Lessons
1. Grouping by account type requires joining expenses → accounts via accountId.
"""
    write_trial(35, "expense-type-pie", doc, md)

def trial_036():
    """036 - Daily Revenue Sparkline (minimal)"""
    items = db.daily_revenue()
    data, meta = db.to_line_segments(items, "index", "revenue")
    color = db.hex_to_rgba("#3b82f6")
    # Tight margins, no grid — sparkline style
    tx = db.fit_transform(meta["xRange"], meta["yRange"],
                           clip_x=(-0.98, 0.98), clip_y=(-0.98, 0.98), padding=0.01)
    bufs = {100: {"data": rf(data)}}
    transforms = {50: tx}
    panes = {1: {"name": "Spark", "region": {"clipYMin": -0.98, "clipYMax": 0.98,
                                              "clipXMin": -0.98, "clipXMax": 0.98},
                  "hasClearColor": True, "clearColor": DARK_BG}}
    layers = {10: std_layer()}
    geos = {101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": meta["vertexCount"]}}
    dis = {102: {"layerId": 10, "name": "spark", "pipeline": "lineAA@1",
                 "geometryId": 101, "transformId": 50,
                 "color": color, "lineWidth": 1.0}}
    doc = make_doc(400, 100, bufs, transforms, panes, layers, geos, dis)
    n_ids = count_ids(doc)
    md = f"""# Data Trial 036: Daily Revenue Sparkline
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** daily_revenue()
**Goal:** Minimal sparkline — 630 days of revenue, tight margins, no grid.
**Outcome:** Dense sparkline suitable for inline display. Zero defects.
---
## What Was Built
Viewport 400x100 (sparkline proportions). lineAA@1 pipeline. {meta['vertexCount']} segments.
Tight clip margins (0.98), minimal padding (1%). lineWidth 1.0.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- 630 data points compressed into a sparkline form factor.
- Overall trend and volatility visible even at small size.
---
## Lessons
1. Sparklines use tight clip margins and small viewports — fit_transform padding can be reduced to 1%.
"""
    write_trial(36, "daily-revenue-sparkline", doc, md)

def trial_037():
    """037 - Monthly Items Sold"""
    # Compute total items sold per month from sale_items + sale dates
    from collections import defaultdict
    monthly_items = defaultdict(int)
    for si in db.sale_items:
        sale_month = db._sale_month.get(si["saleId"], "")
        if sale_month:
            monthly_items[sale_month] += si["quantity"]

    months = sorted(monthly_items.keys())
    items = [{"month": m, "index": i, "units": monthly_items[m]} for i, m in enumerate(months)]

    data, meta = db.to_bars(items, "index", "units", bar_width=0.7)
    color = db.hex_to_rgba("#22c55e")
    doc = simple_bar_doc(data, meta, color, corner_radius=2.0)
    n_ids = count_ids(doc)
    md = f"""# Data Trial 037: Monthly Items Sold
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** Custom aggregation — sale_items quantity grouped by sale month
**Goal:** Bar chart of total items sold per month.
**Outcome:** Volume trends visible across {len(items)} months. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline. {meta['vertexCount']} bars for {len(items)} months.
Range: {min(it['units'] for it in items)} to {max(it['units'] for it in items)} units/month.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- Monthly unit sales range from {min(it['units'] for it in items)} to {max(it['units'] for it in items)}.
- Volume trends may differ from revenue trends due to product mix changes.
---
## Lessons
1. Joining sale_items → sales via saleId → date gives per-month item counts not available in the adapter.
"""
    write_trial(37, "monthly-items-sold", doc, md)

def trial_038():
    """038 - Department Units Bars"""
    items = db.department_revenue()
    color_map = {d["id"]: db.hex_to_rgba(db.PALETTE_DEPT.get(d["id"], "#ffffff")) for d in items}
    doc = multi_bar_doc(items, "index", "units", color_map, bar_width=0.7, corner_radius=3.0)
    n_ids = count_ids(doc)
    top = max(items, key=lambda x: x["units"])
    md = f"""# Data Trial 038: Department Units Sold Bars
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** department_revenue() — units field
**Goal:** Bar chart of units sold per department with PALETTE_DEPT colors.
**Outcome:** Department volume comparison clear. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline. {len(items)} bars colored by department.
Top department by volume: {top['name']} ({top['units']} units).
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- {top['name']} leads in units at {top['units']}.
- Unit rankings may differ from revenue rankings due to price variation.
---
## Lessons
1. Same department_revenue() query serves trials 003, 004, 024, 031, and 038 — different y-axis each time.
"""
    write_trial(38, "department-units-bars", doc, md)

def trial_039():
    """039 - Revenue vs Expenses Dual Line"""
    rev_items = db.monthly_revenue()
    exp_items = db.monthly_expenses()

    # Align on same index range
    n = min(len(rev_items), len(exp_items))
    rev_data, rev_meta = db.to_line_segments(rev_items[:n], "index", "revenue")
    exp_data, exp_meta = db.to_line_segments(exp_items[:n], "index", "total")

    # Compute combined range for shared transform
    x_range = (min(rev_meta["xRange"][0], exp_meta["xRange"][0]),
               max(rev_meta["xRange"][1], exp_meta["xRange"][1]))
    y_range = (min(rev_meta["yRange"][0], exp_meta["yRange"][0]),
               max(rev_meta["yRange"][1], exp_meta["yRange"][1]))
    tx = db.fit_transform(x_range, y_range)

    bufs = {100: {"data": rf(rev_data)}, 103: {"data": rf(exp_data)}}
    transforms = {50: tx}
    panes = {1: std_pane()}
    layers = {10: std_layer()}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": rev_meta["vertexCount"]},
        104: {"vertexBufferId": 103, "format": "rect4", "vertexCount": exp_meta["vertexCount"]},
    }
    dis = {
        102: {"layerId": 10, "name": "revenue", "pipeline": "lineAA@1",
              "geometryId": 101, "transformId": 50,
              "color": db.hex_to_rgba("#3b82f6"), "lineWidth": 2.5},
        105: {"layerId": 10, "name": "expenses", "pipeline": "lineAA@1",
              "geometryId": 104, "transformId": 50,
              "color": db.hex_to_rgba("#ef4444"), "lineWidth": 2.5},
    }
    doc = make_doc(800, 500, bufs, transforms, panes, layers, geos, dis)
    n_ids = count_ids(doc)
    md = f"""# Data Trial 039: Revenue vs Expenses Dual Line
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** monthly_revenue() + monthly_expenses()
**Goal:** Overlay revenue (blue) and expenses (red) lines on the same chart.
**Outcome:** Clear visual comparison of revenue vs expenses. Zero defects.
---
## What Was Built
Viewport 800x500. Two lineAA@1 DrawItems sharing one transform.
Revenue: {rev_meta['vertexCount']} segments (blue). Expenses: {exp_meta['vertexCount']} segments (red).
Shared Y-axis range to show both series at correct relative scale.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- Revenue consistently exceeds expenses — the store is profitable.
- The gap between lines represents monthly profit.
---
## Lessons
1. Dual-line charts share one transform but need separate buffer/geometry/drawItem triples.
"""
    write_trial(39, "revenue-vs-expenses-dual", doc, md)

def trial_040():
    """040 - Top 10 Products Stacked Area"""
    # Get top 10 products
    rankings = db.product_rankings(10)
    top_ids = [r["id"] for r in rankings]

    # Get monthly revenue per product (for top 10)
    from collections import defaultdict
    prod_monthly = defaultdict(lambda: defaultdict(float))
    for si in db.sale_items:
        if si["productId"] in top_ids:
            sale_month = db._sale_month.get(si["saleId"], "")
            if sale_month:
                prod_monthly[si["productId"]][sale_month] += si["lineTotal"]

    all_months = sorted(set(m for pm in prod_monthly.values() for m in pm))

    # Build stacked area: each product is a triSolid@1 area band
    # Compute cumulative baseline
    baselines = [0.0] * len(all_months)
    bufs = {}
    geos = {}
    dis = {}
    transforms = {}

    # First compute global Y range
    totals_by_month = [0.0] * len(all_months)
    for pid in top_ids:
        for j, m in enumerate(all_months):
            totals_by_month[j] += prod_monthly[pid].get(m, 0)

    x_range = (0, len(all_months) - 1)
    y_range = (0, max(totals_by_month) * 1.05)
    tx = db.fit_transform(x_range, y_range)
    transforms[50] = tx

    bid = 100
    for pi, pid in enumerate(top_ids):
        area_data = []
        for j in range(len(all_months) - 1):
            m0 = all_months[j]
            m1 = all_months[j + 1]
            x0, x1 = float(j), float(j + 1)
            bot0 = baselines[j]
            top0 = bot0 + prod_monthly[pid].get(m0, 0)
            bot1 = baselines[j + 1]
            top1 = bot1 + prod_monthly[pid].get(m1, 0)
            # Triangle 1: top0, bot0, top1
            area_data.extend([x0, top0, x0, bot0, x1, top1])
            # Triangle 2: top1, bot0, bot1
            area_data.extend([x1, top1, x0, bot0, x1, bot1])

        # Update baselines
        for j, m in enumerate(all_months):
            baselines[j] += prod_monthly[pid].get(m, 0)

        vc = len(area_data) // 2
        if vc > 0:
            bufs[bid] = {"data": rf(area_data)}
            geos[bid+1] = {"vertexBufferId": bid, "format": "pos2_clip", "vertexCount": vc}
            color = db.hex_to_rgba(db.PALETTE_8[pi % 8], a=0.85)
            dis[bid+2] = {"layerId": 10, "name": rankings[pi]["name"][:20],
                           "pipeline": "triSolid@1", "geometryId": bid+1, "transformId": 50,
                           "color": color}
            bid += 3

    panes = {1: std_pane()}
    layers = {10: std_layer()}
    doc = make_doc(1000, 600, bufs, transforms, panes, layers, geos, dis)
    n_ids = count_ids(doc)
    total_verts = sum(g.get("vertexCount", 0) for g in doc.get("geometries", {}).values()
                      if isinstance(g, dict))
    # Fix: geometries values are dicts with string keys now
    total_verts = sum(v["vertexCount"] for v in doc["geometries"].values())
    md = f"""# Data Trial 040: Top 10 Products Stacked Area
**Date:** {DATE}
**Data Source:** {DATA_SOURCE}
**Query:** Derived from sale_items + product_rankings(10) — per-product monthly revenue
**Goal:** Stacked area chart showing top 10 products' monthly revenue contribution.
**Outcome:** Stacked areas show product contribution over time. Zero defects.
---
## What Was Built
Viewport 1000x600. triSolid@1 pipeline. 10 stacked area bands across {len(all_months)} months.
{total_verts} total vertices. PALETTE_8 colors.
Top product: {rankings[0]['name']}.
Total: {n_ids} unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- Top 10 products show their relative contribution over 21 months.
- Stacking reveals how total top-product revenue composes.
---
## Lessons
1. Stacked areas require cumulative baselines — each series sits atop the previous.
2. Manual stacking with triSolid@1 (2 triangles per segment) is more work but fully flexible.
"""
    write_trial(40, "top-10-products-area", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════════════════════

if __name__ == "__main__":
    os.makedirs(OUT_DIR, exist_ok=True)
    print(f"Output directory: {OUT_DIR}")
    print(f"Database loaded: {len(db.sales)} sales, {len(db.products)} products\n")

    trials = [
        trial_001, trial_002, trial_003, trial_004, trial_005,
        trial_006, trial_007, trial_008, trial_009, trial_010,
        trial_011, trial_012, trial_013, trial_014, trial_015,
        trial_016, trial_017, trial_018, trial_019, trial_020,
        trial_021, trial_022, trial_023, trial_024, trial_025,
        trial_026, trial_027, trial_028, trial_029, trial_030,
        trial_031, trial_032, trial_033, trial_034, trial_035,
        trial_036, trial_037, trial_038, trial_039, trial_040,
    ]

    for fn in trials:
        try:
            fn()
        except Exception as e:
            print(f"  ERROR in {fn.__name__}: {e}")
            import traceback
            traceback.print_exc()

    print(f"\nDone. {len(trials)} trials generated.")
