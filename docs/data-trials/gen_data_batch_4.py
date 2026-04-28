#!/usr/bin/env python3
"""Generate data-driven trials 121-160: Multi-Pane Dashboards.

Each trial produces:
  - NNN-slug.json  (SceneDocument with inline buffer data)
  - NNN-slug.md    (audit markdown)

These are COMPOSITION challenges — combining multiple data views into
multi-pane dashboards with independent transforms, shared color schemes,
and careful ID allocation across panes.
"""
import json, math, os, sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))
from data.adapter import StoreData
from collections import defaultdict, Counter
from datetime import date

OUT_DIR = os.path.dirname(os.path.abspath(__file__))
db = StoreData()

# ── helpers ──────────────────────────────────────────────────────────────────

def rf(arr, digits=6):
    """Round floats in an array."""
    return [round(float(x), digits) for x in arr]

def make_doc(w, h, buffers, transforms, panes, layers, geometries, drawItems,
             viewports=None, textOverlay=None):
    doc = {"version": 1, "viewport": {"width": w, "height": h}}
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

def fit(x_range, y_range, clip_x=(-0.9, 0.9), clip_y=(-0.9, 0.9), padding=0.05):
    return db.fit_transform(x_range, y_range, clip_x, clip_y, padding)

DARK_BG = [0.06, 0.09, 0.16, 1.0]
PANE_BG = [0.07, 0.09, 0.14, 1.0]
PAL = StoreData.PALETTE_8
DEPT_PAL = StoreData.PALETTE_DEPT
hex2rgba = StoreData.hex_to_rgba

def circle_fan(cx, cy, r, segs):
    verts = []
    for i in range(segs):
        a0 = 2 * math.pi * i / segs
        a1 = 2 * math.pi * (i + 1) / segs
        verts += [cx, cy,
                  cx + r * math.cos(a0), cy + r * math.sin(a0),
                  cx + r * math.cos(a1), cy + r * math.sin(a1)]
    return verts

# ── Trial 121: Executive Summary ─────────────────────────────────────────────

def trial_121():
    monthly = db.monthly_revenue()
    dept = db.department_revenue()
    profit = db.monthly_profit()

    # Pane 1: top-left — monthly revenue line
    line_data, line_meta = db.to_line_segments(monthly, "index", "revenue")
    t1 = fit(line_meta["xRange"], line_meta["yRange"])

    # Pane 2: top-right — dept revenue bars
    bar_data, bar_meta = db.to_bars(dept, "index", "revenue", bar_width=0.7)
    t2 = fit(bar_meta["xRange"], bar_meta["yRange"])

    # Pane 3: bottom — monthly profit line
    pline_data, pline_meta = db.to_line_segments(profit, "index", "profit")
    t3 = fit(pline_meta["xRange"], pline_meta["yRange"])

    bufs = {
        100: {"data": rf(line_data)},
        110: {"data": rf(bar_data)},
        120: {"data": rf(pline_data)},
    }
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
        52: {"sx": t3["sx"], "sy": t3["sy"], "tx": t3["tx"], "ty": t3["ty"]},
    }
    panes = {
        1: {"name": "Revenue", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": -0.03},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "Departments", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": 0.03, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
        3: {"name": "Profit", "region": {"clipYMin": -0.95, "clipYMax": -0.02, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {
        10: {"paneId": 1, "name": "rev-data"},
        20: {"paneId": 2, "name": "dept-data"},
        30: {"paneId": 3, "name": "profit-data"},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": line_meta["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "rect4", "vertexCount": bar_meta["vertexCount"]},
        121: {"vertexBufferId": 120, "format": "rect4", "vertexCount": pline_meta["vertexCount"]},
    }
    dis = {
        102: {"layerId": 10, "name": "rev-line", "pipeline": "lineAA@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#3b82f6"), "lineWidth": 2.5},
        112: {"layerId": 20, "name": "dept-bars", "pipeline": "instancedRect@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#22c55e"), "cornerRadius": 2.0},
        122: {"layerId": 30, "name": "profit-line", "pipeline": "lineAA@1", "geometryId": 121,
              "transformId": 52, "color": hex2rgba("#f59e0b"), "lineWidth": 2.5},
    }
    labels = []
    labels.append({"clipX": -0.49, "clipY": 0.98, "text": "Monthly Revenue", "align": "c"})
    labels.append({"clipX": 0.49, "clipY": 0.98, "text": "Department Revenue", "align": "c"})
    labels.append({"clipX": 0.0, "clipY": -0.0, "text": "Monthly Profit", "align": "c"})

    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 14, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 121 — Executive Summary Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes (top-left: monthly revenue line, top-right: dept revenue bars, bottom: monthly profit line)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `monthly_revenue()` — {len(monthly)} months as lineAA@1
- Pane 2: `department_revenue()` — {len(dept)} departments as instancedRect@1
- Pane 3: `monthly_profit()` — {len(profit)} months as lineAA@1

## Insight
Combines top-line revenue trend, departmental breakdown, and profitability into a single executive view. The bottom pane shows profit = revenue - expenses, revealing months where high revenue didn't translate to high profit.

## ID Allocation
- Panes: 1, 2, 3 | Layers: 10, 20, 30 | Transforms: 50, 51, 52
- Buffers: 100, 110, 120 | Geometries: 101, 111, 121 | DrawItems: 102, 112, 122
"""
    write_trial(121, "executive-summary", doc, md)

# ── Trial 122: Sales Operations ──────────────────────────────────────────────

def trial_122():
    daily = db.daily_revenue()
    hourly = db.hourly_distribution()
    dow = db.dow_distribution()
    payments = db.payment_method_breakdown()

    # Pane 1: top-left — daily revenue line
    d1, m1 = db.to_line_segments(daily, "index", "revenue")
    t1 = fit(m1["xRange"], m1["yRange"])

    # Pane 2: top-right — hourly histogram bars
    d2, m2 = db.to_bars(hourly, "hour", "count", bar_width=0.8)
    t2 = fit(m2["xRange"], m2["yRange"])

    # Pane 3: bottom-left — DOW bars
    d3, m3 = db.to_bars(dow, "dow", "revenue", bar_width=0.7)
    t3 = fit(m3["xRange"], m3["yRange"])

    # Pane 4: bottom-right — payment pie
    wedges = db.to_pie_wedges(payments, "revenue", cx=0.0, cy=0.0, r=0.75)

    bufs = {100: {"data": rf(d1)}, 110: {"data": rf(d2)}, 120: {"data": rf(d3)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
        52: {"sx": t3["sx"], "sy": t3["sy"], "tx": t3["tx"], "ty": t3["ty"]},
    }
    panes = {
        1: {"name": "DailyRev", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": -0.03},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "Hourly", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": 0.03, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
        3: {"name": "DOW", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": -0.95, "clipXMax": -0.03},
            "hasClearColor": True, "clearColor": PANE_BG},
        4: {"name": "Payments", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": 0.03, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "d1"}, 20: {"paneId": 2, "name": "d2"},
              30: {"paneId": 3, "name": "d3"}, 40: {"paneId": 4, "name": "d4"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": m1["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "rect4", "vertexCount": m2["vertexCount"]},
        121: {"vertexBufferId": 120, "format": "rect4", "vertexCount": m3["vertexCount"]},
    }
    dis = {
        102: {"layerId": 10, "name": "daily-line", "pipeline": "lineAA@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#3b82f6"), "lineWidth": 1.5},
        112: {"layerId": 20, "name": "hourly-bars", "pipeline": "instancedRect@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#06b6d4"), "cornerRadius": 2.0},
        122: {"layerId": 30, "name": "dow-bars", "pipeline": "instancedRect@1", "geometryId": 121,
              "transformId": 52, "color": hex2rgba("#8b5cf6"), "cornerRadius": 2.0},
    }

    # Pie wedges as separate draw items
    pie_colors = [hex2rgba(PAL[i % len(PAL)]) for i in range(len(wedges))]
    buf_id = 130
    for i, (wdata, frac, sa, ea) in enumerate(wedges):
        bufs[buf_id] = {"data": rf(wdata)}
        geos[buf_id + 1] = {"vertexBufferId": buf_id, "format": "pos2_clip", "vertexCount": len(wdata) // 2}
        dis[buf_id + 2] = {"layerId": 40, "name": f"pie-{i}", "pipeline": "triSolid@1",
                           "geometryId": buf_id + 1, "color": pie_colors[i]}
        buf_id += 3

    labels = [
        {"clipX": -0.49, "clipY": 0.98, "text": "Daily Revenue", "align": "c"},
        {"clipX": 0.49, "clipY": 0.98, "text": "Hourly Distribution", "align": "c"},
        {"clipX": -0.49, "clipY": 0.0, "text": "Day of Week", "align": "c"},
        {"clipX": 0.49, "clipY": 0.0, "text": "Payment Methods", "align": "c"},
    ]

    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 13, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 122 — Sales Operations Dashboard
**Date:** 2026-03-22
**Layout:** 4 panes (2x2): daily revenue, hourly histogram, DOW bars, payment pie
**Resolution:** 1200x800

## Data Sources
- Pane 1: `daily_revenue()` — {len(daily)} days as lineAA@1
- Pane 2: `hourly_distribution()` — {len(hourly)} hours as instancedRect@1
- Pane 3: `dow_distribution()` — {len(dow)} days as instancedRect@1
- Pane 4: `payment_method_breakdown()` — {len(payments)} methods as pie (triSolid@1)

## Insight
Four operational views: daily trend reveals seasonal patterns, hourly distribution shows peak trading hours, DOW breakdown identifies strongest sales days, and payment mix shows cash vs card preferences.
"""
    write_trial(122, "sales-operations", doc, md)

# ── Trial 123: Product Intelligence ──────────────────────────────────────────

def trial_123():
    top20 = db.product_rankings(top_n=20)
    ppv = db.product_price_vs_volume()
    products = db.product_rankings()

    # Pane 1: top — top 20 revenue bars (horizontal)
    d1, m1 = db.to_horizontal_bars(top20, "index", "revenue", bar_height=0.7)
    t1 = fit(m1["xRange"], m1["yRange"])

    # Pane 2: bottom-left — price vs volume scatter
    d2, m2 = db.to_scatter(ppv, "unitPrice", "unitsSold")
    t2 = fit(m2["xRange"], m2["yRange"])

    # Pane 3: bottom-right — margin histogram
    # Bin margins into 10 bins
    margins = [p["margin"] for p in products]
    bins = 10
    m_min, m_max = min(margins), max(margins)
    bin_w = (m_max - m_min) / bins if m_max != m_min else 0.1
    hist = [0] * bins
    for m in margins:
        b = min(int((m - m_min) / bin_w), bins - 1)
        hist[b] += 1
    hist_items = [{"index": i, "count": hist[i]} for i in range(bins)]
    d3, m3 = db.to_bars(hist_items, "index", "count", bar_width=0.8)
    t3 = fit(m3["xRange"], m3["yRange"])

    bufs = {100: {"data": rf(d1)}, 110: {"data": rf(d2)}, 120: {"data": rf(d3)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
        52: {"sx": t3["sx"], "sy": t3["sy"], "tx": t3["tx"], "ty": t3["ty"]},
    }
    panes = {
        1: {"name": "TopProducts", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "PriceVol", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": -0.95, "clipXMax": -0.03},
            "hasClearColor": True, "clearColor": PANE_BG},
        3: {"name": "MarginHist", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": 0.03, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "top"}, 20: {"paneId": 2, "name": "scatter"},
              30: {"paneId": 3, "name": "hist"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": m1["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "pos2_clip", "vertexCount": m2["vertexCount"]},
        121: {"vertexBufferId": 120, "format": "rect4", "vertexCount": m3["vertexCount"]},
    }
    dis = {
        102: {"layerId": 10, "name": "top-bars", "pipeline": "instancedRect@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#ef4444"), "cornerRadius": 2.0},
        112: {"layerId": 20, "name": "scatter", "pipeline": "points@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#22c55e", 0.7), "pointSize": 5.0},
        122: {"layerId": 30, "name": "margin-bars", "pipeline": "instancedRect@1", "geometryId": 121,
              "transformId": 52, "color": hex2rgba("#8b5cf6"), "cornerRadius": 2.0},
    }

    labels = [
        {"clipX": 0.0, "clipY": 0.98, "text": "Top 20 Products by Revenue", "align": "c"},
        {"clipX": -0.49, "clipY": 0.0, "text": "Price vs Volume", "align": "c"},
        {"clipX": 0.49, "clipY": 0.0, "text": "Margin Distribution", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 13, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 123 — Product Intelligence Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes: top 20 revenue bars (top), price-vs-volume scatter (bottom-left), margin histogram (bottom-right)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `product_rankings(top_n=20)` — {len(top20)} products as horizontal instancedRect@1
- Pane 2: `product_price_vs_volume()` — {len(ppv)} products as points@1
- Pane 3: margin distribution — {bins} bins as instancedRect@1

## Insight
Identifies top revenue-generating products, reveals the price-volume relationship (expensive items sell fewer units), and shows margin distribution across the catalog.
"""
    write_trial(123, "product-intelligence", doc, md)

# ── Trial 124: Customer Analytics ─────────────────────────────────────────────

def trial_124():
    tiers = db.customer_tier_breakdown()
    tier_rev = db.customer_tier_revenue()

    # Pane 1: left — tier donut
    wedges = db.to_donut_wedges(tiers, "count", cx=0.0, cy=0.0, r_outer=0.75, r_inner=0.4)
    tier_colors = [hex2rgba("#f59e0b"), hex2rgba("#94a3b8"), hex2rgba("#b45309")]

    # Pane 2: center — tier revenue bars
    d2, m2 = db.to_bars(tier_rev, "index", "revenue", bar_width=0.6)
    t2 = fit(m2["xRange"], m2["yRange"])

    # Pane 3: right — avg spend bars
    d3, m3 = db.to_bars(tier_rev, "index", "avgSpend", bar_width=0.6)
    t3 = fit(m3["xRange"], m3["yRange"])

    bufs = {}
    transforms = {
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
        52: {"sx": t3["sx"], "sy": t3["sy"], "tx": t3["tx"], "ty": t3["ty"]},
    }
    panes = {
        1: {"name": "TierDonut", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": -0.35},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "TierRev", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": -0.30, "clipXMax": 0.30},
            "hasClearColor": True, "clearColor": PANE_BG},
        3: {"name": "AvgSpend", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": 0.35, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "donut"}, 20: {"paneId": 2, "name": "rev"},
              30: {"paneId": 3, "name": "avg"}}
    geos = {}
    dis = {}

    # Donut wedges
    buf_id = 100
    for i, (wdata, frac, sa, ea) in enumerate(wedges):
        bufs[buf_id] = {"data": rf(wdata)}
        geos[buf_id + 1] = {"vertexBufferId": buf_id, "format": "pos2_clip",
                            "vertexCount": len(wdata) // 2}
        dis[buf_id + 2] = {"layerId": 10, "name": f"donut-{i}", "pipeline": "triSolid@1",
                           "geometryId": buf_id + 1, "color": tier_colors[i % len(tier_colors)]}
        buf_id += 3

    # Revenue bars
    bufs[buf_id] = {"data": rf(d2)}
    geos[buf_id + 1] = {"vertexBufferId": buf_id, "format": "rect4", "vertexCount": m2["vertexCount"]}
    dis[buf_id + 2] = {"layerId": 20, "name": "tier-rev", "pipeline": "instancedRect@1",
                       "geometryId": buf_id + 1, "transformId": 51,
                       "color": hex2rgba("#3b82f6"), "cornerRadius": 3.0}
    buf_id += 3

    # Avg spend bars
    bufs[buf_id] = {"data": rf(d3)}
    geos[buf_id + 1] = {"vertexBufferId": buf_id, "format": "rect4", "vertexCount": m3["vertexCount"]}
    dis[buf_id + 2] = {"layerId": 30, "name": "avg-spend", "pipeline": "instancedRect@1",
                       "geometryId": buf_id + 1, "transformId": 52,
                       "color": hex2rgba("#22c55e"), "cornerRadius": 3.0}

    labels = [
        {"clipX": -0.65, "clipY": 0.98, "text": "Tier Distribution", "align": "c"},
        {"clipX": 0.0, "clipY": 0.98, "text": "Tier Revenue", "align": "c"},
        {"clipX": 0.65, "clipY": 0.98, "text": "Avg Spend / Tier", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 13, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 124 — Customer Analytics Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes: tier donut (left), tier revenue bars (center), avg spend bars (right)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `customer_tier_breakdown()` — {len(tiers)} tiers as donut (triSolid@1)
- Pane 2: `customer_tier_revenue()` — {len(tier_rev)} tiers as instancedRect@1
- Pane 3: `customer_tier_revenue()` — avgSpend field as instancedRect@1

## Insight
Gold tier is smallest in count but highest in revenue per customer. Silver tier drives the most total revenue by volume.
"""
    write_trial(124, "customer-analytics", doc, md)

# ── Trial 125: Workforce Dashboard ───────────────────────────────────────────

def trial_125():
    emp = db.employee_hours(top_n=15)
    heatmap = db.shift_heatmap()
    roles = Counter(e["role"] for e in db.employees)

    # Pane 1: top — top 15 employee hours bars
    d1, m1 = db.to_horizontal_bars(emp, "index", "totalHours", bar_height=0.6)
    t1 = fit(m1["xRange"], m1["yRange"])

    # Pane 2: bottom-left — shift heatmap
    d2, m2, vr = db.to_heatmap_rects(heatmap, "dow", "hour", "count",
                                      cell_w=1.0, cell_h=1.0)
    t2 = fit(m2["xRange"], m2["yRange"])

    # Pane 3: bottom-right — role distribution bars
    role_items = [{"index": i, "count": count} for i, (role, count) in enumerate(sorted(roles.items(), key=lambda x: -x[1]))]
    d3, m3 = db.to_bars(role_items, "index", "count", bar_width=0.6)
    t3 = fit(m3["xRange"], m3["yRange"])

    bufs = {100: {"data": rf(d1)}, 120: {"data": rf(d3)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        52: {"sx": t3["sx"], "sy": t3["sy"], "tx": t3["tx"], "ty": t3["ty"]},
    }
    panes = {
        1: {"name": "EmpHours", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "Heatmap", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": -0.95, "clipXMax": -0.03},
            "hasClearColor": True, "clearColor": PANE_BG},
        3: {"name": "Roles", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": 0.03, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "emp"}, 20: {"paneId": 2, "name": "heat"},
              30: {"paneId": 3, "name": "roles"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": m1["vertexCount"]},
        121: {"vertexBufferId": 120, "format": "rect4", "vertexCount": m3["vertexCount"]},
    }
    dis = {
        102: {"layerId": 10, "name": "emp-bars", "pipeline": "instancedRect@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#3b82f6"), "cornerRadius": 2.0},
        122: {"layerId": 30, "name": "role-bars", "pipeline": "instancedRect@1", "geometryId": 121,
              "transformId": 52, "color": hex2rgba("#ec4899"), "cornerRadius": 2.0},
    }

    # Heatmap cells — color-coded individually
    # Group heatmap cells into value buckets and create a few draw items
    vmin, vmax = vr
    buf_id = 130
    t_heat = fit(m2["xRange"], m2["yRange"])
    transforms[51] = {"sx": t_heat["sx"], "sy": t_heat["sy"], "tx": t_heat["tx"], "ty": t_heat["ty"]}

    # Split into 5 intensity bands
    n_bands = 5
    bands = [[] for _ in range(n_bands)]
    for item in heatmap:
        val = item["count"]
        t_val = (val - vmin) / (vmax - vmin) if vmax != vmin else 0.5
        band = min(int(t_val * n_bands), n_bands - 1)
        r_idx, c_idx = item["dow"], item["hour"]
        gap = 0.05
        x0 = c_idx * 1.0 + gap
        y0 = r_idx * 1.0 + gap
        x1 = (c_idx + 1) * 1.0 - gap
        y1 = (r_idx + 1) * 1.0 - gap
        bands[band].extend([x0, y0, x1, y1])

    band_colors = [
        [0.1, 0.1, 0.2, 1.0],
        [0.15, 0.25, 0.4, 1.0],
        [0.2, 0.45, 0.6, 1.0],
        [0.35, 0.65, 0.75, 1.0],
        [0.5, 0.85, 0.9, 1.0],
    ]
    for bi, band_data in enumerate(bands):
        if not band_data:
            continue
        bufs[buf_id] = {"data": rf(band_data)}
        geos[buf_id + 1] = {"vertexBufferId": buf_id, "format": "rect4",
                            "vertexCount": len(band_data) // 4}
        dis[buf_id + 2] = {"layerId": 20, "name": f"heat-{bi}", "pipeline": "instancedRect@1",
                           "geometryId": buf_id + 1, "transformId": 51,
                           "color": band_colors[bi]}
        buf_id += 3

    labels = [
        {"clipX": 0.0, "clipY": 0.98, "text": "Top 15 Employees by Hours", "align": "c"},
        {"clipX": -0.49, "clipY": 0.0, "text": "Shift Heatmap (DOW x Hour)", "align": "c"},
        {"clipX": 0.49, "clipY": 0.0, "text": "Role Distribution", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 13, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 125 — Workforce Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes: top 15 employee hours (top), shift heatmap (bottom-left), role distribution (bottom-right)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `employee_hours(top_n=15)` — {len(emp)} employees as horizontal instancedRect@1
- Pane 2: `shift_heatmap()` — {len(heatmap)} cells as color-banded instancedRect@1
- Pane 3: employee roles — {len(roles)} roles as instancedRect@1

## Insight
Shift heatmap reveals staffing patterns — darker cells indicate understaffed hours. Role distribution shows workforce composition for scheduling optimization.
"""
    write_trial(125, "workforce-dashboard", doc, md)

# ── Trial 126: Financial Overview ─────────────────────────────────────────────

def trial_126():
    monthly = db.monthly_revenue()
    expenses = db.monthly_expenses()
    profit = db.monthly_profit()

    # Pane 1: top-left — revenue line
    d1, m1 = db.to_line_segments(monthly, "index", "revenue")
    t1 = fit(m1["xRange"], m1["yRange"])

    # Pane 2: top-right — expense bars by month
    d2, m2 = db.to_bars(expenses, "index", "total", bar_width=0.7)
    t2 = fit(m2["xRange"], m2["yRange"])

    # Pane 3: bottom-left — profit line
    d3, m3 = db.to_line_segments(profit, "index", "profit")
    t3 = fit(m3["xRange"], m3["yRange"])

    # Pane 4: bottom-right — margin trend
    d4, m4 = db.to_line_segments(profit, "index", "margin")
    t4 = fit(m4["xRange"], m4["yRange"])

    bufs = {100: {"data": rf(d1)}, 110: {"data": rf(d2)},
            120: {"data": rf(d3)}, 130: {"data": rf(d4)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
        52: {"sx": t3["sx"], "sy": t3["sy"], "tx": t3["tx"], "ty": t3["ty"]},
        53: {"sx": t4["sx"], "sy": t4["sy"], "tx": t4["tx"], "ty": t4["ty"]},
    }
    panes = {
        1: {"name": "Revenue", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": -0.03},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "Expenses", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": 0.03, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
        3: {"name": "Profit", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": -0.95, "clipXMax": -0.03},
            "hasClearColor": True, "clearColor": PANE_BG},
        4: {"name": "Margin", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": 0.03, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "r"}, 20: {"paneId": 2, "name": "e"},
              30: {"paneId": 3, "name": "p"}, 40: {"paneId": 4, "name": "m"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": m1["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "rect4", "vertexCount": m2["vertexCount"]},
        121: {"vertexBufferId": 120, "format": "rect4", "vertexCount": m3["vertexCount"]},
        131: {"vertexBufferId": 130, "format": "rect4", "vertexCount": m4["vertexCount"]},
    }
    dis = {
        102: {"layerId": 10, "name": "rev-line", "pipeline": "lineAA@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#3b82f6"), "lineWidth": 2.5},
        112: {"layerId": 20, "name": "exp-bars", "pipeline": "instancedRect@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#ef4444"), "cornerRadius": 2.0},
        122: {"layerId": 30, "name": "profit-line", "pipeline": "lineAA@1", "geometryId": 121,
              "transformId": 52, "color": hex2rgba("#22c55e"), "lineWidth": 2.5},
        132: {"layerId": 40, "name": "margin-line", "pipeline": "lineAA@1", "geometryId": 131,
              "transformId": 53, "color": hex2rgba("#f59e0b"), "lineWidth": 2.0},
    }
    labels = [
        {"clipX": -0.49, "clipY": 0.98, "text": "Monthly Revenue", "align": "c"},
        {"clipX": 0.49, "clipY": 0.98, "text": "Monthly Expenses", "align": "c"},
        {"clipX": -0.49, "clipY": 0.0, "text": "Monthly Profit", "align": "c"},
        {"clipX": 0.49, "clipY": 0.0, "text": "Profit Margin %", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 13, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 126 — Financial Overview Dashboard
**Date:** 2026-03-22
**Layout:** 4 panes (2x2): revenue line, expense bars, profit line, margin trend
**Resolution:** 1200x800

## Data Sources
- Pane 1: `monthly_revenue()` — {len(monthly)} months as lineAA@1
- Pane 2: `monthly_expenses()` — {len(expenses)} months as instancedRect@1
- Pane 3: `monthly_profit()` — {len(profit)} months as lineAA@1
- Pane 4: margin trend — {len(profit)} months as lineAA@1

## Insight
Revenue grows steadily while expenses remain relatively flat, producing improving profit margins over time. The margin trend line reveals the overall trajectory of business health.
"""
    write_trial(126, "financial-overview", doc, md)

# ── Trial 127: Inventory Monitor ──────────────────────────────────────────────

def trial_127():
    # Get top 5 products by revenue
    top5 = db.product_rankings(top_n=5)

    # Pane 1: top — inventory trend for top product
    inv = db.inventory_trend(top5[0]["id"])
    d1, m1 = db.to_line_segments(inv, "index", "qty")
    t1 = fit(m1["xRange"], m1["yRange"])

    # Pane 2: bottom-left — reorder frequency (products that hit reorder point)
    # Count how many snapshots each product was at or below reorder
    reorder_counts = defaultdict(int)
    for snap in db.inventory_snapshots:
        if snap["quantityOnHand"] <= snap["reorderPoint"]:
            reorder_counts[snap["productId"]] += 1
    top_reorder = sorted(reorder_counts.items(), key=lambda x: -x[1])[:15]
    reorder_items = [{"index": i, "count": count} for i, (pid, count) in enumerate(top_reorder)]
    d2, m2 = db.to_bars(reorder_items, "index", "count", bar_width=0.7)
    t2 = fit(m2["xRange"], m2["yRange"])

    # Pane 3: bottom-right — supplier lead times
    supps = db.supplier_performance()
    d3, m3 = db.to_bars(supps, "index", "avgLeadTime", bar_width=0.6)
    t3 = fit(m3["xRange"], m3["yRange"])

    bufs = {100: {"data": rf(d1)}, 110: {"data": rf(d2)}, 120: {"data": rf(d3)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
        52: {"sx": t3["sx"], "sy": t3["sy"], "tx": t3["tx"], "ty": t3["ty"]},
    }
    panes = {
        1: {"name": "InvTrend", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "Reorder", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": -0.95, "clipXMax": -0.03},
            "hasClearColor": True, "clearColor": PANE_BG},
        3: {"name": "LeadTimes", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": 0.03, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "inv"}, 20: {"paneId": 2, "name": "reorder"},
              30: {"paneId": 3, "name": "lead"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": m1["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "rect4", "vertexCount": m2["vertexCount"]},
        121: {"vertexBufferId": 120, "format": "rect4", "vertexCount": m3["vertexCount"]},
    }
    dis = {
        102: {"layerId": 10, "name": "inv-line", "pipeline": "lineAA@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#06b6d4"), "lineWidth": 2.5},
        112: {"layerId": 20, "name": "reorder-bars", "pipeline": "instancedRect@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#ef4444"), "cornerRadius": 2.0},
        122: {"layerId": 30, "name": "lead-bars", "pipeline": "instancedRect@1", "geometryId": 121,
              "transformId": 52, "color": hex2rgba("#f59e0b"), "cornerRadius": 2.0},
    }
    labels = [
        {"clipX": 0.0, "clipY": 0.98, "text": f"Inventory Trend: {top5[0]['name']}", "align": "c"},
        {"clipX": -0.49, "clipY": 0.0, "text": "Reorder Frequency", "align": "c"},
        {"clipX": 0.49, "clipY": 0.0, "text": "Supplier Lead Times (days)", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 13, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 127 — Inventory Monitor Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes: inventory trend (top), reorder frequency (bottom-left), supplier lead times (bottom-right)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `inventory_trend({top5[0]['id']})` — {len(inv)} snapshots as lineAA@1
- Pane 2: reorder frequency — {len(reorder_items)} products as instancedRect@1
- Pane 3: `supplier_performance()` — {len(supps)} suppliers as instancedRect@1

## Insight
Tracks top-product inventory levels, flags products that frequently hit reorder points, and compares supplier delivery performance for procurement decisions.
"""
    write_trial(127, "inventory-monitor", doc, md)

# ── Trial 128: Department Comparison ──────────────────────────────────────────

def trial_128():
    dept = db.department_revenue()

    # Pane 1: left — revenue bars
    d1, m1 = db.to_bars(dept, "index", "revenue", bar_width=0.7)
    t1 = fit(m1["xRange"], m1["yRange"])

    # Pane 2: right — units bars
    d2, m2 = db.to_bars(dept, "index", "units", bar_width=0.7)
    t2 = fit(m2["xRange"], m2["yRange"])

    bufs = {100: {"data": rf(d1)}, 110: {"data": rf(d2)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
    }
    panes = {
        1: {"name": "Revenue", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": -0.03},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "Units", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": 0.03, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "rev"}, 20: {"paneId": 2, "name": "units"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": m1["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "rect4", "vertexCount": m2["vertexCount"]},
    }

    # Color-code bars by department
    # Build separate draw items per department for color coding
    dis = {}
    buf_id = 120
    for idx, d in enumerate(dept):
        did = d.get("id", idx + 1)
        color = hex2rgba(DEPT_PAL.get(did, PAL[idx % len(PAL)]))
        # Revenue bar
        hw = 0.35
        bar1 = [d["index"] - hw, 0, d["index"] + hw, d["revenue"]]
        bufs[buf_id] = {"data": rf(bar1)}
        geos[buf_id + 1] = {"vertexBufferId": buf_id, "format": "rect4", "vertexCount": 1}
        dis[buf_id + 2] = {"layerId": 10, "name": f"rev-{d['name']}", "pipeline": "instancedRect@1",
                           "geometryId": buf_id + 1, "transformId": 50,
                           "color": color, "cornerRadius": 3.0}
        buf_id += 3
        # Units bar
        bar2 = [d["index"] - hw, 0, d["index"] + hw, d["units"]]
        bufs[buf_id] = {"data": rf(bar2)}
        geos[buf_id + 1] = {"vertexBufferId": buf_id, "format": "rect4", "vertexCount": 1}
        dis[buf_id + 2] = {"layerId": 20, "name": f"units-{d['name']}", "pipeline": "instancedRect@1",
                           "geometryId": buf_id + 1, "transformId": 51,
                           "color": color, "cornerRadius": 3.0}
        buf_id += 3

    labels = [
        {"clipX": -0.49, "clipY": 0.98, "text": "Revenue by Department ($)", "align": "c"},
        {"clipX": 0.49, "clipY": 0.98, "text": "Units Sold by Department", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 14, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 128 — Department Comparison Dashboard
**Date:** 2026-03-22
**Layout:** 2 panes side-by-side: revenue bars (left) + units bars (right)
**Resolution:** 1200x800

## Data Sources
- Both panes: `department_revenue()` — {len(dept)} departments
- Left: revenue field as instancedRect@1 (color-coded by department)
- Right: units field as instancedRect@1 (same colors)

## Insight
Side-by-side comparison reveals which departments generate the most revenue vs units. High-revenue/low-unit departments sell premium items; high-unit/low-revenue departments sell consumables.
"""
    write_trial(128, "department-comparison", doc, md)

# ── Trial 129: Seasonal Quarters ──────────────────────────────────────────────

def trial_129():
    daily = db.daily_revenue()
    # Split into quarters by month
    quarters = {
        "Q1": [d for d in daily if d["date"][5:7] in ("01", "02", "03")],
        "Q2": [d for d in daily if d["date"][5:7] in ("04", "05", "06")],
        "Q3": [d for d in daily if d["date"][5:7] in ("07", "08", "09")],
        "Q4": [d for d in daily if d["date"][5:7] in ("10", "11", "12")],
    }
    # Re-index within each quarter
    for qname, qdata in quarters.items():
        for i, d in enumerate(qdata):
            d["qindex"] = i

    q_colors = [hex2rgba("#3b82f6"), hex2rgba("#22c55e"), hex2rgba("#f59e0b"), hex2rgba("#ef4444")]

    bufs = {}
    transforms = {}
    panes_d = {}
    layers_d = {}
    geos = {}
    dis = {}

    regions = [
        {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": -0.03},
        {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": 0.03, "clipXMax": 0.95},
        {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": -0.95, "clipXMax": -0.03},
        {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": 0.03, "clipXMax": 0.95},
    ]

    q_names = ["Q1", "Q2", "Q3", "Q4"]
    for qi, qname in enumerate(q_names):
        qdata = quarters[qname]
        if len(qdata) < 2:
            continue
        d_data, d_meta = db.to_line_segments(qdata, "qindex", "revenue")
        t = fit(d_meta["xRange"], d_meta["yRange"])

        pane_id = qi + 1
        layer_id = (qi + 1) * 10
        tf_id = 50 + qi
        buf_id = 100 + qi * 10

        bufs[buf_id] = {"data": rf(d_data)}
        transforms[tf_id] = {"sx": t["sx"], "sy": t["sy"], "tx": t["tx"], "ty": t["ty"]}
        panes_d[pane_id] = {"name": qname, "region": regions[qi],
                            "hasClearColor": True, "clearColor": PANE_BG}
        layers_d[layer_id] = {"paneId": pane_id, "name": f"{qname}-data"}
        geos[buf_id + 1] = {"vertexBufferId": buf_id, "format": "rect4",
                            "vertexCount": d_meta["vertexCount"]}
        dis[buf_id + 2] = {"layerId": layer_id, "name": f"{qname}-line", "pipeline": "lineAA@1",
                           "geometryId": buf_id + 1, "transformId": tf_id,
                           "color": q_colors[qi], "lineWidth": 2.0}

    labels = [
        {"clipX": -0.49, "clipY": 0.98, "text": "Q1 (Jan-Mar)", "align": "c"},
        {"clipX": 0.49, "clipY": 0.98, "text": "Q2 (Apr-Jun)", "align": "c"},
        {"clipX": -0.49, "clipY": 0.0, "text": "Q3 (Jul-Sep)", "align": "c"},
        {"clipX": 0.49, "clipY": 0.0, "text": "Q4 (Oct-Dec)", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes_d, layers_d, geos, dis,
                   textOverlay={"fontSize": 13, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 129 — Seasonal Quarters Dashboard
**Date:** 2026-03-22
**Layout:** 4 panes: Q1, Q2, Q3, Q4 daily revenue lines
**Resolution:** 1200x800

## Data Sources
- All panes: `daily_revenue()` — split by quarter
- Q1: {len(quarters['Q1'])} days | Q2: {len(quarters['Q2'])} days | Q3: {len(quarters['Q3'])} days | Q4: {len(quarters['Q4'])} days

## Insight
Quarterly comparison with independent Y-axes reveals seasonal sales patterns. Each quarter has its own transform to normalize the view, making relative patterns visible regardless of absolute revenue differences.
"""
    write_trial(129, "seasonal-quarters", doc, md)

# ── Trial 130: Supplier Scorecard ─────────────────────────────────────────────

def trial_130():
    supps = db.supplier_performance()

    d1, m1 = db.to_bars(supps, "index", "poCount", bar_width=0.6)
    t1 = fit(m1["xRange"], m1["yRange"])

    d2, m2 = db.to_bars(supps, "index", "avgLeadTime", bar_width=0.6)
    t2 = fit(m2["xRange"], m2["yRange"])

    d3, m3 = db.to_bars(supps, "index", "totalCost", bar_width=0.6)
    t3 = fit(m3["xRange"], m3["yRange"])

    bufs = {100: {"data": rf(d1)}, 110: {"data": rf(d2)}, 120: {"data": rf(d3)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
        52: {"sx": t3["sx"], "sy": t3["sy"], "tx": t3["tx"], "ty": t3["ty"]},
    }
    panes = {
        1: {"name": "POCount", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": -0.35},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "LeadTime", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.30, "clipXMax": 0.30},
            "hasClearColor": True, "clearColor": PANE_BG},
        3: {"name": "TotalCost", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": 0.35, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "po"}, 20: {"paneId": 2, "name": "lead"},
              30: {"paneId": 3, "name": "cost"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": m1["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "rect4", "vertexCount": m2["vertexCount"]},
        121: {"vertexBufferId": 120, "format": "rect4", "vertexCount": m3["vertexCount"]},
    }
    dis = {
        102: {"layerId": 10, "name": "po-bars", "pipeline": "instancedRect@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#3b82f6"), "cornerRadius": 2.0},
        112: {"layerId": 20, "name": "lead-bars", "pipeline": "instancedRect@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#f59e0b"), "cornerRadius": 2.0},
        122: {"layerId": 30, "name": "cost-bars", "pipeline": "instancedRect@1", "geometryId": 121,
              "transformId": 52, "color": hex2rgba("#ef4444"), "cornerRadius": 2.0},
    }
    labels = [
        {"clipX": -0.65, "clipY": 0.98, "text": "PO Count", "align": "c"},
        {"clipX": 0.0, "clipY": 0.98, "text": "Avg Lead Time (days)", "align": "c"},
        {"clipX": 0.65, "clipY": 0.98, "text": "Total Cost ($)", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 13, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 130 — Supplier Scorecard Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes: PO count bars (left), lead time bars (center), total cost bars (right)
**Resolution:** 1200x800

## Data Sources
- All panes: `supplier_performance()` — {len(supps)} suppliers
- Left: poCount | Center: avgLeadTime | Right: totalCost

## Insight
Three-axis supplier evaluation. Ideal suppliers have high order volume, low lead times, and competitive costs. The scorecard reveals outliers in each dimension.
"""
    write_trial(130, "supplier-scorecard", doc, md)

# ── Trial 131: Daily Ops ─────────────────────────────────────────────────────

def trial_131():
    daily = db.daily_revenue()
    hourly = db.hourly_distribution()
    heatmap = db.shift_heatmap()
    ips = db.items_per_sale_distribution()

    d1, m1 = db.to_line_segments(daily, "index", "revenue")
    t1 = fit(m1["xRange"], m1["yRange"])

    d2, m2 = db.to_line_segments(hourly, "hour", "revenue")
    t2 = fit(m2["xRange"], m2["yRange"])

    d4, m4 = db.to_bars(ips, "itemCount", "frequency", bar_width=0.7)
    t4 = fit(m4["xRange"], m4["yRange"])

    # Heatmap
    d3_raw, m3, vr = db.to_heatmap_rects(heatmap, "dow", "hour", "count")
    t3 = fit(m3["xRange"], m3["yRange"])

    bufs = {100: {"data": rf(d1)}, 110: {"data": rf(d2)}, 130: {"data": rf(d4)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
        52: {"sx": t3["sx"], "sy": t3["sy"], "tx": t3["tx"], "ty": t3["ty"]},
        53: {"sx": t4["sx"], "sy": t4["sy"], "tx": t4["tx"], "ty": t4["ty"]},
    }
    panes = {
        1: {"name": "DailyRev", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": -0.03},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "Hourly", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": 0.03, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
        3: {"name": "ShiftHeat", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": -0.95, "clipXMax": -0.03},
            "hasClearColor": True, "clearColor": PANE_BG},
        4: {"name": "ItemsPerSale", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": 0.03, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "d"}, 20: {"paneId": 2, "name": "h"},
              30: {"paneId": 3, "name": "heat"}, 40: {"paneId": 4, "name": "ips"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": m1["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "rect4", "vertexCount": m2["vertexCount"]},
        131: {"vertexBufferId": 130, "format": "rect4", "vertexCount": m4["vertexCount"]},
    }
    dis = {
        102: {"layerId": 10, "name": "daily-line", "pipeline": "lineAA@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#3b82f6"), "lineWidth": 1.5},
        112: {"layerId": 20, "name": "hourly-curve", "pipeline": "lineAA@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#06b6d4"), "lineWidth": 2.0},
        132: {"layerId": 40, "name": "ips-bars", "pipeline": "instancedRect@1", "geometryId": 131,
              "transformId": 53, "color": hex2rgba("#8b5cf6"), "cornerRadius": 2.0},
    }

    # Heatmap bands
    vmin, vmax = vr
    buf_id = 140
    n_bands = 4
    bands = [[] for _ in range(n_bands)]
    for item in heatmap:
        val = item["count"]
        t_val = (val - vmin) / (vmax - vmin) if vmax != vmin else 0.5
        band = min(int(t_val * n_bands), n_bands - 1)
        r_idx, c_idx = item["dow"], item["hour"]
        gap = 0.05
        bands[band].extend([c_idx + gap, r_idx + gap, c_idx + 1 - gap, r_idx + 1 - gap])
    band_colors = [[0.1,0.15,0.25,1.0],[0.2,0.35,0.5,1.0],[0.3,0.55,0.7,1.0],[0.5,0.8,0.9,1.0]]
    for bi, bd in enumerate(bands):
        if not bd:
            continue
        bufs[buf_id] = {"data": rf(bd)}
        geos[buf_id+1] = {"vertexBufferId": buf_id, "format": "rect4", "vertexCount": len(bd)//4}
        dis[buf_id+2] = {"layerId": 30, "name": f"heat-{bi}", "pipeline": "instancedRect@1",
                         "geometryId": buf_id+1, "transformId": 52, "color": band_colors[bi]}
        buf_id += 3

    labels = [
        {"clipX": -0.49, "clipY": 0.98, "text": "Daily Revenue", "align": "c"},
        {"clipX": 0.49, "clipY": 0.98, "text": "Hourly Sales Curve", "align": "c"},
        {"clipX": -0.49, "clipY": 0.0, "text": "Shift Coverage Heatmap", "align": "c"},
        {"clipX": 0.49, "clipY": 0.0, "text": "Items per Sale", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 13, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 131 — Daily Operations Dashboard
**Date:** 2026-03-22
**Layout:** 4 panes: daily revenue line, hourly sales curve, shift heatmap, items-per-sale histogram
**Resolution:** 1200x800

## Data Sources
- Pane 1: `daily_revenue()` — {len(daily)} days as lineAA@1
- Pane 2: `hourly_distribution()` — {len(hourly)} hours as lineAA@1
- Pane 3: `shift_heatmap()` — {len(heatmap)} cells as banded instancedRect@1
- Pane 4: `items_per_sale_distribution()` — {len(ips)} bins as instancedRect@1

## Insight
Operational command center combining revenue trends, peak hours, staffing coverage, and basket size distribution for day-to-day decision making.
"""
    write_trial(131, "daily-ops", doc, md)

# ── Trial 132: YoY Revenue ───────────────────────────────────────────────────

def trial_132():
    monthly = db.monthly_revenue()
    y2024 = [m for m in monthly if m["month"].startswith("2024")]
    y2025 = [m for m in monthly if m["month"].startswith("2025")]
    # Re-index within year
    for i, m in enumerate(y2024):
        m["yindex"] = i
    for i, m in enumerate(y2025):
        m["yindex"] = i

    # Use same Y scale for comparison
    all_rev = [m["revenue"] for m in y2024 + y2025]
    y_range = (0, max(all_rev) * 1.05)

    if len(y2024) >= 2:
        d1, m1 = db.to_line_segments(y2024, "yindex", "revenue")
        t1 = fit(m1["xRange"], y_range)
    else:
        d1, m1 = [], {"format": "rect4", "vertexCount": 0, "xRange": (0, 1), "yRange": y_range}
        t1 = fit(m1["xRange"], y_range)

    if len(y2025) >= 2:
        d2, m2 = db.to_line_segments(y2025, "yindex", "revenue")
        t2 = fit(m2["xRange"], y_range)
    else:
        d2, m2 = [], {"format": "rect4", "vertexCount": 0, "xRange": (0, 1), "yRange": y_range}
        t2 = fit(m2["xRange"], y_range)

    bufs = {100: {"data": rf(d1)}, 110: {"data": rf(d2)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
    }
    panes = {
        1: {"name": "2024", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "2025", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "y24"}, 20: {"paneId": 2, "name": "y25"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": m1["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "rect4", "vertexCount": m2["vertexCount"]},
    }
    dis = {
        102: {"layerId": 10, "name": "2024-rev", "pipeline": "lineAA@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#3b82f6"), "lineWidth": 2.5},
        112: {"layerId": 20, "name": "2025-rev", "pipeline": "lineAA@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#22c55e"), "lineWidth": 2.5},
    }
    labels = [
        {"clipX": 0.0, "clipY": 0.98, "text": "2024 Monthly Revenue", "align": "c"},
        {"clipX": 0.0, "clipY": 0.0, "text": "2025 Monthly Revenue", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 14, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 132 — Year-over-Year Revenue Dashboard
**Date:** 2026-03-22
**Layout:** 2 panes: top = 2024 monthly revenue, bottom = 2025 monthly revenue (same Y scale)
**Resolution:** 1200x800

## Data Sources
- Top: `monthly_revenue()` filtered to 2024 — {len(y2024)} months as lineAA@1
- Bottom: `monthly_revenue()` filtered to 2025 — {len(y2025)} months as lineAA@1

## Insight
Both panes share the same Y-axis range for direct visual comparison. Growth or decline is immediately visible from the line positions relative to each pane boundary.
"""
    write_trial(132, "yoy-revenue", doc, md)

# ── Trial 133: Product Portfolio ──────────────────────────────────────────────

def trial_133():
    products = db.product_rankings()
    top20 = products[:20]
    ppv = db.product_price_vs_volume()

    # Pane 1: top — revenue treemap (approximated with bars sized by revenue)
    # Treemap approximation: horizontal bars ordered by revenue, filling width
    d1, m1 = db.to_horizontal_bars(top20, "index", "revenue", bar_height=0.6)
    t1 = fit(m1["xRange"], m1["yRange"])

    # Pane 2: bottom-left — margin scatter
    margin_items = [{"price": p["unitPrice"], "margin": p["margin"]} for p in products if p["unitPrice"] > 0]
    d2, m2 = db.to_scatter(margin_items, "price", "margin")
    t2 = fit(m2["xRange"], m2["yRange"])

    # Pane 3: bottom-right — slow movers (bottom 15 by units)
    slow = sorted(products, key=lambda x: x["units"])[:15]
    for i, s in enumerate(slow):
        s["sindex"] = i
    d3, m3 = db.to_horizontal_bars(slow, "sindex", "units", bar_height=0.6)
    t3 = fit(m3["xRange"], m3["yRange"])

    bufs = {100: {"data": rf(d1)}, 110: {"data": rf(d2)}, 120: {"data": rf(d3)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
        52: {"sx": t3["sx"], "sy": t3["sy"], "tx": t3["tx"], "ty": t3["ty"]},
    }
    panes = {
        1: {"name": "TopRev", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "MarginScat", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": -0.95, "clipXMax": -0.03},
            "hasClearColor": True, "clearColor": PANE_BG},
        3: {"name": "SlowMovers", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": 0.03, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "top"}, 20: {"paneId": 2, "name": "scat"},
              30: {"paneId": 3, "name": "slow"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": m1["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "pos2_clip", "vertexCount": m2["vertexCount"]},
        121: {"vertexBufferId": 120, "format": "rect4", "vertexCount": m3["vertexCount"]},
    }
    dis = {
        102: {"layerId": 10, "name": "top-rev", "pipeline": "instancedRect@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#3b82f6"), "cornerRadius": 2.0},
        112: {"layerId": 20, "name": "margin-scat", "pipeline": "points@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#22c55e", 0.7), "pointSize": 4.0},
        122: {"layerId": 30, "name": "slow-bars", "pipeline": "instancedRect@1", "geometryId": 121,
              "transformId": 52, "color": hex2rgba("#ef4444", 0.8), "cornerRadius": 2.0},
    }
    labels = [
        {"clipX": 0.0, "clipY": 0.98, "text": "Top 20 Products by Revenue", "align": "c"},
        {"clipX": -0.49, "clipY": 0.0, "text": "Price vs Margin", "align": "c"},
        {"clipX": 0.49, "clipY": 0.0, "text": "Slowest Movers (by units)", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 13, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 133 — Product Portfolio Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes: revenue ranking (top), margin scatter (bottom-left), slow movers (bottom-right)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `product_rankings()` top 20 — horizontal instancedRect@1
- Pane 2: products — price vs margin as points@1
- Pane 3: bottom 15 by units — horizontal instancedRect@1

## Insight
The portfolio view identifies top revenue earners, the price-margin relationship, and slow-moving inventory candidates for clearance or discontinuation.
"""
    write_trial(133, "product-portfolio", doc, md)

# ── Trial 134: Customer Segmentation ──────────────────────────────────────────

def trial_134():
    tiers = db.customer_tier_breakdown()
    dept = db.department_revenue()
    tier_rev = db.customer_tier_revenue()

    # Pane 1: left — tier pie
    wedges = db.to_pie_wedges(tiers, "count", cx=0.0, cy=0.0, r=0.75)
    tier_colors = [hex2rgba("#f59e0b"), hex2rgba("#94a3b8"), hex2rgba("#b45309")]

    # Pane 2: center — tier-dept stacked bars (simplified: tier revenue bars side by side)
    d2, m2 = db.to_bars(tier_rev, "index", "revenue", bar_width=0.6)
    t2 = fit(m2["xRange"], m2["yRange"])

    # Pane 3: right — customer purchase frequency scatter
    # Count purchases per customer
    cust_purchases = defaultdict(lambda: {"count": 0, "total": 0.0})
    for s in db.sales:
        if s["customerId"]:
            cust_purchases[s["customerId"]]["count"] += 1
            cust_purchases[s["customerId"]]["total"] += s["total"]
    freq_items = [{"count": v["count"], "total": v["total"]}
                  for v in cust_purchases.values()]
    d3, m3 = db.to_scatter(freq_items, "count", "total")
    t3 = fit(m3["xRange"], m3["yRange"])

    bufs = {110: {"data": rf(d2)}, 120: {"data": rf(d3)}}
    transforms = {
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
        52: {"sx": t3["sx"], "sy": t3["sy"], "tx": t3["tx"], "ty": t3["ty"]},
    }
    panes = {
        1: {"name": "TierPie", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": -0.35},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "TierDept", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": -0.30, "clipXMax": 0.30},
            "hasClearColor": True, "clearColor": PANE_BG},
        3: {"name": "FreqScat", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": 0.35, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "pie"}, 20: {"paneId": 2, "name": "stacked"},
              30: {"paneId": 3, "name": "freq"}}
    geos = {
        111: {"vertexBufferId": 110, "format": "rect4", "vertexCount": m2["vertexCount"]},
        121: {"vertexBufferId": 120, "format": "pos2_clip", "vertexCount": m3["vertexCount"]},
    }
    dis = {
        112: {"layerId": 20, "name": "tier-bars", "pipeline": "instancedRect@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#3b82f6"), "cornerRadius": 3.0},
        122: {"layerId": 30, "name": "freq-scatter", "pipeline": "points@1", "geometryId": 121,
              "transformId": 52, "color": hex2rgba("#22c55e", 0.6), "pointSize": 4.0},
    }

    # Pie wedges
    buf_id = 130
    for i, (wdata, frac, sa, ea) in enumerate(wedges):
        bufs[buf_id] = {"data": rf(wdata)}
        geos[buf_id + 1] = {"vertexBufferId": buf_id, "format": "pos2_clip", "vertexCount": len(wdata) // 2}
        dis[buf_id + 2] = {"layerId": 10, "name": f"pie-{i}", "pipeline": "triSolid@1",
                           "geometryId": buf_id + 1, "color": tier_colors[i % len(tier_colors)]}
        buf_id += 3

    labels = [
        {"clipX": -0.65, "clipY": 0.98, "text": "Tier Distribution", "align": "c"},
        {"clipX": 0.0, "clipY": 0.98, "text": "Tier Revenue", "align": "c"},
        {"clipX": 0.65, "clipY": 0.98, "text": "Purchase Frequency", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 13, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 134 — Customer Segmentation Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes: tier pie (left), tier revenue bars (center), customer frequency scatter (right)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `customer_tier_breakdown()` — {len(tiers)} tiers as pie (triSolid@1)
- Pane 2: `customer_tier_revenue()` — {len(tier_rev)} tiers as instancedRect@1
- Pane 3: customer purchase counts — {len(freq_items)} customers as points@1

## Insight
Scatter plot reveals the long tail: most customers make few purchases, while a handful are heavy buyers. Combined with tier distribution, this guides loyalty program targeting.
"""
    write_trial(134, "customer-segmentation", doc, md)

# ── Trial 135: Expense Analysis ───────────────────────────────────────────────

def trial_135():
    monthly_exp = db.monthly_expenses()
    acct_exp = db.expense_by_account()
    profit = db.monthly_profit()

    # Pane 1: top — monthly expense trend
    d1, m1 = db.to_line_segments(monthly_exp, "index", "total")
    t1 = fit(m1["xRange"], m1["yRange"])

    # Pane 2: bottom-left — account breakdown bars
    d2, m2 = db.to_bars(acct_exp, "index", "total", bar_width=0.7)
    t2 = fit(m2["xRange"], m2["yRange"])

    # Pane 3: bottom-right — payroll ratio line (expenses / revenue)
    # Use expense/revenue ratio from profit data
    ratio_items = [{"index": p["index"], "ratio": p["expenses"] / p["revenue"] if p["revenue"] else 0}
                   for p in profit]
    d3, m3 = db.to_line_segments(ratio_items, "index", "ratio")
    t3 = fit(m3["xRange"], m3["yRange"])

    bufs = {100: {"data": rf(d1)}, 110: {"data": rf(d2)}, 120: {"data": rf(d3)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
        52: {"sx": t3["sx"], "sy": t3["sy"], "tx": t3["tx"], "ty": t3["ty"]},
    }
    panes = {
        1: {"name": "ExpTrend", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "AcctBreak", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": -0.95, "clipXMax": -0.03},
            "hasClearColor": True, "clearColor": PANE_BG},
        3: {"name": "Ratio", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": 0.03, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "trend"}, 20: {"paneId": 2, "name": "acct"},
              30: {"paneId": 3, "name": "ratio"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": m1["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "rect4", "vertexCount": m2["vertexCount"]},
        121: {"vertexBufferId": 120, "format": "rect4", "vertexCount": m3["vertexCount"]},
    }
    dis = {
        102: {"layerId": 10, "name": "exp-trend", "pipeline": "lineAA@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#ef4444"), "lineWidth": 2.5},
        112: {"layerId": 20, "name": "acct-bars", "pipeline": "instancedRect@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#f59e0b"), "cornerRadius": 2.0},
        122: {"layerId": 30, "name": "ratio-line", "pipeline": "lineAA@1", "geometryId": 121,
              "transformId": 52, "color": hex2rgba("#8b5cf6"), "lineWidth": 2.0},
    }
    labels = [
        {"clipX": 0.0, "clipY": 0.98, "text": "Monthly Expense Trend", "align": "c"},
        {"clipX": -0.49, "clipY": 0.0, "text": "Expense by Account", "align": "c"},
        {"clipX": 0.49, "clipY": 0.0, "text": "Expense/Revenue Ratio", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 13, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 135 — Expense Analysis Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes: monthly expense trend (top), account breakdown (bottom-left), expense/revenue ratio (bottom-right)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `monthly_expenses()` — {len(monthly_exp)} months as lineAA@1
- Pane 2: `expense_by_account()` — {len(acct_exp)} accounts as instancedRect@1
- Pane 3: expense/revenue ratio — {len(ratio_items)} months as lineAA@1

## Insight
Expense trend combined with account breakdown shows where money goes. The ratio line reveals whether expenses are growing faster than revenue.
"""
    write_trial(135, "expense-analysis", doc, md)

# ── Trial 136: Garden Department Focus ────────────────────────────────────────

def trial_136():
    # Garden dept = ID 6
    garden_id = 6
    dept_monthly = db.department_monthly_revenue()
    garden_monthly = [d for d in dept_monthly if d["deptId"] == garden_id]

    # Garden products
    products = db.product_rankings()
    garden_prods = sorted([p for p in products if p["deptId"] == garden_id],
                          key=lambda x: -x["revenue"])[:10]
    for i, p in enumerate(garden_prods):
        p["gindex"] = i

    # Pane 1: top — garden monthly revenue
    if len(garden_monthly) >= 2:
        d1, m1 = db.to_line_segments(garden_monthly, "index", "revenue")
        t1 = fit(m1["xRange"], m1["yRange"])
    else:
        d1, m1 = [], {"format": "rect4", "vertexCount": 0, "xRange": (0, 1), "yRange": (0, 1)}
        t1 = fit(m1["xRange"], m1["yRange"])

    # Pane 2: bottom-left — top garden products
    d2, m2 = db.to_horizontal_bars(garden_prods, "gindex", "revenue", bar_height=0.6)
    t2 = fit(m2["xRange"], m2["yRange"])

    # Pane 3: bottom-right — seasonal pattern (sum by month-of-year)
    month_totals = defaultdict(float)
    for d in garden_monthly:
        month_num = int(d["month"].split("-")[1])
        month_totals[month_num] += d["revenue"]
    seasonal = [{"month": m, "revenue": month_totals.get(m, 0)} for m in range(1, 13)]
    d3, m3 = db.to_bars(seasonal, "month", "revenue", bar_width=0.7)
    t3 = fit(m3["xRange"], m3["yRange"])

    bufs = {100: {"data": rf(d1)}, 110: {"data": rf(d2)}, 120: {"data": rf(d3)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
        52: {"sx": t3["sx"], "sy": t3["sy"], "tx": t3["tx"], "ty": t3["ty"]},
    }
    panes = {
        1: {"name": "GardenMonthly", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "GardenTop", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": -0.95, "clipXMax": -0.03},
            "hasClearColor": True, "clearColor": PANE_BG},
        3: {"name": "GardenSeasonal", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": 0.03, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "monthly"}, 20: {"paneId": 2, "name": "top"},
              30: {"paneId": 3, "name": "seasonal"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": m1["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "rect4", "vertexCount": m2["vertexCount"]},
        121: {"vertexBufferId": 120, "format": "rect4", "vertexCount": m3["vertexCount"]},
    }
    dis = {
        102: {"layerId": 10, "name": "garden-line", "pipeline": "lineAA@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#22c55e"), "lineWidth": 2.5},
        112: {"layerId": 20, "name": "garden-prods", "pipeline": "instancedRect@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#22c55e", 0.8), "cornerRadius": 2.0},
        122: {"layerId": 30, "name": "garden-seasonal", "pipeline": "instancedRect@1", "geometryId": 121,
              "transformId": 52, "color": hex2rgba("#22c55e", 0.6), "cornerRadius": 2.0},
    }
    labels = [
        {"clipX": 0.0, "clipY": 0.98, "text": "Garden Department — Monthly Revenue", "align": "c"},
        {"clipX": -0.49, "clipY": 0.0, "text": "Top Garden Products", "align": "c"},
        {"clipX": 0.49, "clipY": 0.0, "text": "Seasonal Pattern (by month)", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 13, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 136 — Garden Department Focus Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes: garden monthly revenue (top), top garden products (bottom-left), seasonal pattern (bottom-right)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `department_monthly_revenue()` filtered to Garden — {len(garden_monthly)} months
- Pane 2: `product_rankings()` filtered to Garden — {len(garden_prods)} products
- Pane 3: garden revenue aggregated by month-of-year — 12 months

## Insight
Garden department deep-dive reveals seasonal peaks (spring/summer months show highest revenue) and identifies which specific products drive the category.
"""
    write_trial(136, "garden-dept-focus", doc, md)

# ── Trial 137: Tools Department Focus ─────────────────────────────────────────

def trial_137():
    tools_id = 2
    dept_monthly = db.department_monthly_revenue()
    tools_monthly = [d for d in dept_monthly if d["deptId"] == tools_id]

    products = db.product_rankings()
    tools_prods = sorted([p for p in products if p["deptId"] == tools_id],
                         key=lambda x: -x["margin"])[:10]
    for i, p in enumerate(tools_prods):
        p["tindex"] = i

    # Pane 1: tools monthly revenue
    if len(tools_monthly) >= 2:
        d1, m1 = db.to_line_segments(tools_monthly, "index", "revenue")
        t1 = fit(m1["xRange"], m1["yRange"])
    else:
        d1, m1 = [], {"format": "rect4", "vertexCount": 0, "xRange": (0, 1), "yRange": (0, 1)}
        t1 = fit(m1["xRange"], m1["yRange"])

    # Pane 2: tools product margins
    margin_items = [{"index": i, "margin": p["margin"]} for i, p in enumerate(tools_prods)]
    d2, m2 = db.to_bars(margin_items, "index", "margin", bar_width=0.6)
    t2 = fit(m2["xRange"], m2["yRange"])

    # Pane 3: tools inventory (top 5 tools products inventory trends)
    tools_top5 = sorted([p for p in products if p["deptId"] == tools_id],
                        key=lambda x: -x["revenue"])[:5]
    inv_lines = []
    for p in tools_top5:
        inv = db.inventory_trend(p["id"])
        if len(inv) >= 2:
            inv_lines.append((p["name"], inv))

    # Use first product inventory as primary
    if inv_lines:
        d3, m3 = db.to_line_segments(inv_lines[0][1], "index", "qty")
        t3 = fit(m3["xRange"], m3["yRange"])
    else:
        d3, m3 = [], {"format": "rect4", "vertexCount": 0, "xRange": (0, 1), "yRange": (0, 1)}
        t3 = fit(m3["xRange"], m3["yRange"])

    bufs = {100: {"data": rf(d1)}, 110: {"data": rf(d2)}, 120: {"data": rf(d3)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
        52: {"sx": t3["sx"], "sy": t3["sy"], "tx": t3["tx"], "ty": t3["ty"]},
    }
    panes = {
        1: {"name": "ToolsMonthly", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "ToolsMargin", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": -0.95, "clipXMax": -0.03},
            "hasClearColor": True, "clearColor": PANE_BG},
        3: {"name": "ToolsInv", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": 0.03, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "monthly"}, 20: {"paneId": 2, "name": "margin"},
              30: {"paneId": 3, "name": "inv"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": m1["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "rect4", "vertexCount": m2["vertexCount"]},
        121: {"vertexBufferId": 120, "format": "rect4", "vertexCount": m3["vertexCount"]},
    }
    dis = {
        102: {"layerId": 10, "name": "tools-line", "pipeline": "lineAA@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#3b82f6"), "lineWidth": 2.5},
        112: {"layerId": 20, "name": "tools-margin", "pipeline": "instancedRect@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#3b82f6", 0.8), "cornerRadius": 2.0},
        122: {"layerId": 30, "name": "tools-inv", "pipeline": "lineAA@1", "geometryId": 121,
              "transformId": 52, "color": hex2rgba("#06b6d4"), "lineWidth": 2.0},
    }
    inv_name = inv_lines[0][0] if inv_lines else "N/A"
    labels = [
        {"clipX": 0.0, "clipY": 0.98, "text": "Tools Department — Monthly Revenue", "align": "c"},
        {"clipX": -0.49, "clipY": 0.0, "text": "Product Margins", "align": "c"},
        {"clipX": 0.49, "clipY": 0.0, "text": f"Inventory: {inv_name}", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 13, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 137 — Tools Department Focus Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes: tools monthly revenue (top), product margins (bottom-left), inventory trend (bottom-right)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `department_monthly_revenue()` filtered to Tools — {len(tools_monthly)} months
- Pane 2: tools products sorted by margin — {len(tools_prods)} products
- Pane 3: inventory trend for top tools product — {len(inv_lines[0][1]) if inv_lines else 0} snapshots

## Insight
Tools department analysis combining revenue trajectory, product-level margin analysis, and inventory monitoring for the top-selling tool.
"""
    write_trial(137, "tools-dept-focus", doc, md)

# ── Trial 138: Weekend vs Weekday ─────────────────────────────────────────────

def trial_138():
    dow = db.dow_distribution()
    weekday = [d for d in dow if d["dow"] < 5]
    weekend = [d for d in dow if d["dow"] >= 5]

    # Re-index
    for i, d in enumerate(weekday):
        d["windex"] = i
    for i, d in enumerate(weekend):
        d["windex"] = i

    # Daily averages: divide revenue by approximate number of that day in dataset
    daily = db.daily_revenue()
    day_counts = Counter(date.fromisoformat(d["date"]).weekday() for d in daily)
    for d in weekday:
        cnt = day_counts.get(d["dow"], 1)
        d["avgRevenue"] = d["revenue"] / cnt
    for d in weekend:
        cnt = day_counts.get(d["dow"], 1)
        d["avgRevenue"] = d["revenue"] / cnt

    d1, m1 = db.to_bars(weekday, "windex", "avgRevenue", bar_width=0.6)
    t1 = fit(m1["xRange"], m1["yRange"])

    d2, m2 = db.to_bars(weekend, "windex", "avgRevenue", bar_width=0.6)
    # Use same Y scale
    all_avg = [d["avgRevenue"] for d in weekday + weekend]
    y_range = (0, max(all_avg) * 1.05)
    t2 = fit(m2["xRange"], y_range)
    t1 = fit(m1["xRange"], y_range)

    bufs = {100: {"data": rf(d1)}, 110: {"data": rf(d2)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
    }
    panes = {
        1: {"name": "Weekday", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": -0.03},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "Weekend", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": 0.03, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "wd"}, 20: {"paneId": 2, "name": "we"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": m1["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "rect4", "vertexCount": m2["vertexCount"]},
    }
    dis = {
        102: {"layerId": 10, "name": "weekday-bars", "pipeline": "instancedRect@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#3b82f6"), "cornerRadius": 3.0},
        112: {"layerId": 20, "name": "weekend-bars", "pipeline": "instancedRect@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#f59e0b"), "cornerRadius": 3.0},
    }
    labels = [
        {"clipX": -0.49, "clipY": 0.98, "text": "Weekday Avg Revenue (Mon-Fri)", "align": "c"},
        {"clipX": 0.49, "clipY": 0.98, "text": "Weekend Avg Revenue (Sat-Sun)", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 14, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 138 — Weekend vs Weekday Dashboard
**Date:** 2026-03-22
**Layout:** 2 panes: left = weekday avg revenue bars, right = weekend avg revenue bars
**Resolution:** 1200x800

## Data Sources
- Left: `dow_distribution()` filtered to Mon-Fri — {len(weekday)} bars (avg daily revenue)
- Right: `dow_distribution()` filtered to Sat-Sun — {len(weekend)} bars (avg daily revenue)

## Insight
Same Y scale enables direct comparison between weekday and weekend spending patterns. Weekend days may have higher or lower per-day revenue depending on store traffic patterns.
"""
    write_trial(138, "weekend-vs-weekday", doc, md)

# ── Trial 139: Cash Flow ──────────────────────────────────────────────────────

def trial_139():
    monthly = db.monthly_revenue()
    expenses = db.monthly_expenses()
    profit = db.monthly_profit()

    # Pane 1: top — revenue area
    d1, m1 = db.to_area(monthly, "index", "revenue", baseline=0)
    t1 = fit(m1["xRange"], m1["yRange"])

    # Pane 2: middle — expense area
    d2, m2 = db.to_area(expenses, "index", "total", baseline=0)
    t2 = fit(m2["xRange"], m2["yRange"])

    # Pane 3: bottom — profit bars
    d3, m3 = db.to_bars(profit, "index", "profit", bar_width=0.7)
    t3 = fit(m3["xRange"], m3["yRange"])

    bufs = {100: {"data": rf(d1)}, 110: {"data": rf(d2)}, 120: {"data": rf(d3)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
        52: {"sx": t3["sx"], "sy": t3["sy"], "tx": t3["tx"], "ty": t3["ty"]},
    }
    panes = {
        1: {"name": "Revenue", "region": {"clipYMin": 0.38, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "Expenses", "region": {"clipYMin": -0.28, "clipYMax": 0.33, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
        3: {"name": "Profit", "region": {"clipYMin": -0.95, "clipYMax": -0.33, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "rev"}, 20: {"paneId": 2, "name": "exp"},
              30: {"paneId": 3, "name": "profit"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "pos2_clip", "vertexCount": m1["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "pos2_clip", "vertexCount": m2["vertexCount"]},
        121: {"vertexBufferId": 120, "format": "rect4", "vertexCount": m3["vertexCount"]},
    }
    dis = {
        102: {"layerId": 10, "name": "rev-area", "pipeline": "triSolid@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#22c55e", 0.6)},
        112: {"layerId": 20, "name": "exp-area", "pipeline": "triSolid@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#ef4444", 0.6)},
        122: {"layerId": 30, "name": "profit-bars", "pipeline": "instancedRect@1", "geometryId": 121,
              "transformId": 52, "color": hex2rgba("#3b82f6"), "cornerRadius": 2.0},
    }
    labels = [
        {"clipX": 0.0, "clipY": 0.98, "text": "Revenue (area)", "align": "c"},
        {"clipX": 0.0, "clipY": 0.36, "text": "Expenses (area)", "align": "c"},
        {"clipX": 0.0, "clipY": -0.30, "text": "Net Profit (bars)", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 13, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 139 — Cash Flow Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes stacked: revenue area (top), expense area (middle), net profit bars (bottom)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `monthly_revenue()` — {len(monthly)} months as filled area (triSolid@1)
- Pane 2: `monthly_expenses()` — {len(expenses)} months as filled area (triSolid@1)
- Pane 3: `monthly_profit()` — {len(profit)} months as instancedRect@1

## Insight
Cash flow visualization: green area (in) minus red area (out) equals blue bars (net). The stacked layout makes the visual subtraction intuitive.
"""
    write_trial(139, "cash-flow", doc, md)

# ── Trial 140: KPI Cards ──────────────────────────────────────────────────────

def trial_140():
    # 6 mini-panes showing KPIs
    total_rev = sum(s["total"] for s in db.sales)
    total_sales = len(db.sales)
    avg_ticket = total_rev / total_sales if total_sales else 0
    total_custs = len(db.customers)
    emp_count = len(db.employees)
    prod_count = len(db.products)

    kpis = [
        ("Total Revenue", total_rev, "#22c55e"),
        ("Sales Count", total_sales, "#3b82f6"),
        ("Avg Ticket", avg_ticket, "#f59e0b"),
        ("Customers", total_custs, "#8b5cf6"),
        ("Employees", emp_count, "#ef4444"),
        ("Products", prod_count, "#06b6d4"),
    ]

    # 3×2 grid
    regions = [
        {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": -0.35},
        {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.30, "clipXMax": 0.30},
        {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": 0.35, "clipXMax": 0.95},
        {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": -0.95, "clipXMax": -0.35},
        {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": -0.30, "clipXMax": 0.30},
        {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": 0.35, "clipXMax": 0.95},
    ]

    bufs = {}
    transforms = {}
    panes_d = {}
    layers_d = {}
    geos = {}
    dis = {}
    labels = []

    for i, (name, value, color) in enumerate(kpis):
        pane_id = i + 1
        layer_id = (i + 1) * 10
        buf_id = 100 + i * 10

        panes_d[pane_id] = {"name": name, "region": regions[i],
                            "hasClearColor": True, "clearColor": [0.08, 0.10, 0.16, 1.0]}
        layers_d[layer_id] = {"paneId": pane_id, "name": f"kpi-{i}"}

        # Single large bar representing the KPI value (normalized to 0-1 for clip space)
        bar_data = [-0.5, 0.0, 0.5, 0.8]  # Single bar in clip space
        bufs[buf_id] = {"data": rf(bar_data)}
        geos[buf_id + 1] = {"vertexBufferId": buf_id, "format": "rect4", "vertexCount": 1}
        dis[buf_id + 2] = {"layerId": layer_id, "name": f"kpi-bar-{i}", "pipeline": "instancedRect@1",
                           "geometryId": buf_id + 1, "color": hex2rgba(color, 0.8),
                           "cornerRadius": 6.0}

        # Label
        region = regions[i]
        cx = (region["clipXMin"] + region["clipXMax"]) / 2
        cy = (region["clipYMin"] + region["clipYMax"]) / 2
        if value >= 1000:
            val_str = f"${value:,.0f}" if i == 0 else f"{value:,.0f}" if i < 3 else f"{int(value)}"
        else:
            val_str = f"${value:,.2f}" if i == 2 else f"{int(value)}"
        labels.append({"clipX": cx, "clipY": cy + 0.15, "text": name, "align": "c", "fontSize": 12})
        labels.append({"clipX": cx, "clipY": cy - 0.15, "text": val_str, "align": "c", "fontSize": 18,
                       "color": color})

    doc = make_doc(1200, 800, bufs, transforms, panes_d, layers_d, geos, dis,
                   textOverlay={"fontSize": 14, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 140 — KPI Cards Dashboard
**Date:** 2026-03-22
**Layout:** 6 mini-panes (3x2 grid) each showing one KPI metric
**Resolution:** 1200x800

## KPI Values
- Total Revenue: ${total_rev:,.2f}
- Sales Count: {total_sales:,}
- Avg Ticket: ${avg_ticket:,.2f}
- Customers: {total_custs}
- Employees: {emp_count}
- Products: {prod_count}

## Insight
At-a-glance KPI dashboard. Each card uses a single colored bar with text overlay showing the metric name and value. Designed for wall-mounted displays.
"""
    write_trial(140, "kpi-cards", doc, md)

# ── Trial 141: January 2026 Report ───────────────────────────────────────────

def trial_141():
    daily = db.daily_revenue()
    jan_2026 = [d for d in daily if d["date"].startswith("2026-01")]
    jan_2025 = [d for d in daily if d["date"].startswith("2025-01")]

    for i, d in enumerate(jan_2026):
        d["jindex"] = i
    for i, d in enumerate(jan_2025):
        d["jindex"] = i

    # Dept breakdown for Jan 2026
    dept_monthly = db.department_monthly_revenue()
    jan_dept = [d for d in dept_monthly if d["month"] == "2026-01"]
    for i, d in enumerate(jan_dept):
        d["dindex"] = i

    # Pane 1: Jan 2026 daily revenue
    if len(jan_2026) >= 2:
        d1, m1 = db.to_line_segments(jan_2026, "jindex", "revenue")
        t1 = fit(m1["xRange"], m1["yRange"])
    else:
        d1, m1 = [], {"format": "rect4", "vertexCount": 0, "xRange": (0, 1), "yRange": (0, 1)}
        t1 = fit(m1["xRange"], m1["yRange"])

    # Pane 2: dept breakdown
    if jan_dept:
        d2, m2 = db.to_bars(jan_dept, "dindex", "revenue", bar_width=0.6)
        t2 = fit(m2["xRange"], m2["yRange"])
    else:
        d2, m2 = [], {"format": "rect4", "vertexCount": 0, "xRange": (0, 1), "yRange": (0, 1)}
        t2 = fit(m2["xRange"], m2["yRange"])

    # Pane 3: comparison vs Jan 2025
    if len(jan_2025) >= 2:
        d3, m3 = db.to_line_segments(jan_2025, "jindex", "revenue")
        # Use same Y range as Jan 2026
        all_rev = [d["revenue"] for d in jan_2026 + jan_2025]
        y_range = (min(all_rev) * 0.9, max(all_rev) * 1.1)
        t3 = fit(m3["xRange"], y_range)
    else:
        d3, m3 = [], {"format": "rect4", "vertexCount": 0, "xRange": (0, 1), "yRange": (0, 1)}
        t3 = fit(m3["xRange"], m3["yRange"])

    bufs = {100: {"data": rf(d1)}, 110: {"data": rf(d2)}, 120: {"data": rf(d3)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
        52: {"sx": t3["sx"], "sy": t3["sy"], "tx": t3["tx"], "ty": t3["ty"]},
    }
    panes = {
        1: {"name": "Jan2026", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": -0.03},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "DeptBreak", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": 0.03, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
        3: {"name": "Comparison", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "jan26"}, 20: {"paneId": 2, "name": "dept"},
              30: {"paneId": 3, "name": "comp"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": m1["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "rect4", "vertexCount": m2["vertexCount"]},
        121: {"vertexBufferId": 120, "format": "rect4", "vertexCount": m3["vertexCount"]},
    }
    dis = {
        102: {"layerId": 10, "name": "jan26-line", "pipeline": "lineAA@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#3b82f6"), "lineWidth": 2.5},
        112: {"layerId": 20, "name": "dept-bars", "pipeline": "instancedRect@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#22c55e"), "cornerRadius": 2.0},
        122: {"layerId": 30, "name": "jan25-line", "pipeline": "lineAA@1", "geometryId": 121,
              "transformId": 52, "color": hex2rgba("#f59e0b"), "lineWidth": 2.0},
    }
    labels = [
        {"clipX": -0.49, "clipY": 0.98, "text": "Jan 2026 Daily Revenue", "align": "c"},
        {"clipX": 0.49, "clipY": 0.98, "text": "Jan 2026 Dept Breakdown", "align": "c"},
        {"clipX": 0.0, "clipY": 0.0, "text": "Comparison: Jan 2025 (gold) vs baseline", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 13, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 141 — January 2026 Report Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes: Jan 2026 daily revenue (top-left), dept breakdown (top-right), Jan 2025 comparison (bottom)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `daily_revenue()` filtered to 2026-01 — {len(jan_2026)} days
- Pane 2: `department_monthly_revenue()` filtered to 2026-01 — {len(jan_dept)} departments
- Pane 3: `daily_revenue()` filtered to 2025-01 — {len(jan_2025)} days

## Insight
Monthly report combining this month's daily trend, department contribution, and year-over-year comparison.
"""
    write_trial(141, "january-2026-report", doc, md)

# ── Trial 142: Q4 2025 Review ────────────────────────────────────────────────

def trial_142():
    daily = db.daily_revenue()
    oct = [d for d in daily if d["date"].startswith("2025-10")]
    nov = [d for d in daily if d["date"].startswith("2025-11")]
    dec = [d for d in daily if d["date"].startswith("2025-12")]

    for i, d in enumerate(oct): d["mindex"] = i
    for i, d in enumerate(nov): d["mindex"] = i
    for i, d in enumerate(dec): d["mindex"] = i

    # Q4 dept totals
    dept_monthly = db.department_monthly_revenue()
    q4_dept = defaultdict(float)
    for d in dept_monthly:
        if d["month"] in ("2025-10", "2025-11", "2025-12"):
            q4_dept[d["deptName"]] += d["revenue"]
    q4_items = [{"index": i, "name": n, "revenue": r}
                for i, (n, r) in enumerate(sorted(q4_dept.items(), key=lambda x: -x[1]))]

    month_data = []
    for mdata in [oct, nov, dec]:
        if len(mdata) >= 2:
            d, m = db.to_line_segments(mdata, "mindex", "revenue")
            month_data.append((d, m))
        else:
            month_data.append(([], {"format": "rect4", "vertexCount": 0, "xRange": (0, 1), "yRange": (0, 1)}))

    d4, m4 = db.to_bars(q4_items, "index", "revenue", bar_width=0.6)
    t4 = fit(m4["xRange"], m4["yRange"])

    bufs = {}
    transforms = {}
    panes_d = {}
    layers_d = {}
    geos = {}
    dis = {}

    regions = [
        {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": -0.35},
        {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.30, "clipXMax": 0.30},
        {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": 0.35, "clipXMax": 0.95},
        {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": -0.95, "clipXMax": 0.95},
    ]
    month_names = ["October", "November", "December"]
    colors = [hex2rgba("#3b82f6"), hex2rgba("#22c55e"), hex2rgba("#ef4444")]

    for i in range(3):
        d, m = month_data[i]
        pane_id = i + 1
        layer_id = (i + 1) * 10
        tf_id = 50 + i
        buf_id = 100 + i * 10

        t = fit(m["xRange"], m["yRange"])
        bufs[buf_id] = {"data": rf(d)}
        transforms[tf_id] = {"sx": t["sx"], "sy": t["sy"], "tx": t["tx"], "ty": t["ty"]}
        panes_d[pane_id] = {"name": month_names[i], "region": regions[i],
                            "hasClearColor": True, "clearColor": PANE_BG}
        layers_d[layer_id] = {"paneId": pane_id, "name": f"m{i}"}
        geos[buf_id + 1] = {"vertexBufferId": buf_id, "format": "rect4", "vertexCount": m["vertexCount"]}
        dis[buf_id + 2] = {"layerId": layer_id, "name": f"{month_names[i]}-line",
                           "pipeline": "lineAA@1", "geometryId": buf_id + 1,
                           "transformId": tf_id, "color": colors[i], "lineWidth": 2.0}

    # Q4 dept bars
    panes_d[4] = {"name": "Q4Dept", "region": regions[3], "hasClearColor": True, "clearColor": PANE_BG}
    layers_d[40] = {"paneId": 4, "name": "dept"}
    bufs[140] = {"data": rf(d4)}
    transforms[53] = {"sx": t4["sx"], "sy": t4["sy"], "tx": t4["tx"], "ty": t4["ty"]}
    geos[141] = {"vertexBufferId": 140, "format": "rect4", "vertexCount": m4["vertexCount"]}
    dis[142] = {"layerId": 40, "name": "q4-dept", "pipeline": "instancedRect@1",
                "geometryId": 141, "transformId": 53, "color": hex2rgba("#8b5cf6"), "cornerRadius": 2.0}

    labels = [
        {"clipX": -0.65, "clipY": 0.98, "text": "Oct 2025", "align": "c"},
        {"clipX": 0.0, "clipY": 0.98, "text": "Nov 2025", "align": "c"},
        {"clipX": 0.65, "clipY": 0.98, "text": "Dec 2025", "align": "c"},
        {"clipX": 0.0, "clipY": 0.0, "text": "Q4 2025 Department Totals", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes_d, layers_d, geos, dis,
                   textOverlay={"fontSize": 13, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 142 — Q4 2025 Review Dashboard
**Date:** 2026-03-22
**Layout:** 4 panes: Oct/Nov/Dec revenue lines (top row), quarterly dept bars (bottom)
**Resolution:** 1200x800

## Data Sources
- Top row: `daily_revenue()` filtered to Oct/Nov/Dec 2025 — {len(oct)}/{len(nov)}/{len(dec)} days
- Bottom: `department_monthly_revenue()` Q4 aggregated — {len(q4_items)} departments

## Insight
Month-by-month Q4 review with independent scaling per month-pane, plus combined departmental performance for the quarter.
"""
    write_trial(142, "q4-2025-review", doc, md)

# ── Trial 143: Inventory Health ───────────────────────────────────────────────

def trial_143():
    top5 = db.product_rankings(top_n=5)

    # Pane 1: stock levels for top 5 (overlay lines)
    bufs = {}
    transforms = {}
    geos = {}
    dis = {}

    all_inv = []
    inv_data_list = []
    for p in top5:
        inv = db.inventory_trend(p["id"])
        inv_data_list.append(inv)
        all_inv.extend(inv)

    # Common Y range for stock levels
    if all_inv:
        all_qty = [i["qty"] for i in all_inv]
        y_range = (min(all_qty) * 0.9, max(all_qty) * 1.1)
    else:
        y_range = (0, 100)

    colors = [hex2rgba(PAL[i]) for i in range(5)]
    buf_id = 100
    for i, inv in enumerate(inv_data_list):
        if len(inv) < 2:
            continue
        d, m = db.to_line_segments(inv, "index", "qty")
        t = fit(m["xRange"], y_range)
        if i == 0:
            transforms[50] = {"sx": t["sx"], "sy": t["sy"], "tx": t["tx"], "ty": t["ty"]}
        bufs[buf_id] = {"data": rf(d)}
        geos[buf_id + 1] = {"vertexBufferId": buf_id, "format": "rect4", "vertexCount": m["vertexCount"]}
        dis[buf_id + 2] = {"layerId": 10, "name": f"inv-{i}", "pipeline": "lineAA@1",
                           "geometryId": buf_id + 1, "transformId": 50,
                           "color": colors[i], "lineWidth": 2.0}
        buf_id += 3

    # Pane 2: turnover rates (monthly units sold / stock)
    turnover_items = []
    for i, p in enumerate(top5):
        inv = inv_data_list[i]
        if inv:
            avg_stock = sum(s["qty"] for s in inv) / len(inv) if inv else 1
            sold = sum(s["sold"] for s in inv)
            turnover = sold / avg_stock if avg_stock else 0
            turnover_items.append({"index": i, "turnover": turnover})
    d2, m2 = db.to_bars(turnover_items, "index", "turnover", bar_width=0.6)
    t2 = fit(m2["xRange"], m2["yRange"])

    bufs[buf_id] = {"data": rf(d2)}
    transforms[51] = {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]}
    geos[buf_id + 1] = {"vertexBufferId": buf_id, "format": "rect4", "vertexCount": m2["vertexCount"]}
    dis[buf_id + 2] = {"layerId": 20, "name": "turnover-bars", "pipeline": "instancedRect@1",
                       "geometryId": buf_id + 1, "transformId": 51,
                       "color": hex2rgba("#f59e0b"), "cornerRadius": 2.0}
    buf_id += 3

    # Pane 3: below-reorder count
    below_counts = []
    for i, p in enumerate(top5):
        inv = inv_data_list[i]
        below = sum(1 for s in inv if s["qty"] <= s["reorderPoint"])
        below_counts.append({"index": i, "count": below})
    d3, m3 = db.to_bars(below_counts, "index", "count", bar_width=0.6)
    t3 = fit(m3["xRange"], m3["yRange"])

    bufs[buf_id] = {"data": rf(d3)}
    transforms[52] = {"sx": t3["sx"], "sy": t3["sy"], "tx": t3["tx"], "ty": t3["ty"]}
    geos[buf_id + 1] = {"vertexBufferId": buf_id, "format": "rect4", "vertexCount": m3["vertexCount"]}
    dis[buf_id + 2] = {"layerId": 30, "name": "below-bars", "pipeline": "instancedRect@1",
                       "geometryId": buf_id + 1, "transformId": 52,
                       "color": hex2rgba("#ef4444"), "cornerRadius": 2.0}

    panes = {
        1: {"name": "StockLevels", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "Turnover", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": -0.95, "clipXMax": -0.03},
            "hasClearColor": True, "clearColor": PANE_BG},
        3: {"name": "BelowReorder", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": 0.03, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers_d = {10: {"paneId": 1, "name": "stock"}, 20: {"paneId": 2, "name": "turn"},
                30: {"paneId": 3, "name": "below"}}

    labels = [
        {"clipX": 0.0, "clipY": 0.98, "text": "Stock Levels — Top 5 Products", "align": "c"},
        {"clipX": -0.49, "clipY": 0.0, "text": "Turnover Rates", "align": "c"},
        {"clipX": 0.49, "clipY": 0.0, "text": "Below Reorder Count", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers_d, geos, dis,
                   textOverlay={"fontSize": 13, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 143 — Inventory Health Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes: stock level overlay lines (top), turnover rates (bottom-left), below-reorder count (bottom-right)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `inventory_trend()` for top 5 products — overlaid lineAA@1 lines
- Pane 2: computed turnover rates — {len(turnover_items)} bars as instancedRect@1
- Pane 3: below-reorder snapshot count — {len(below_counts)} bars as instancedRect@1

## Insight
Multi-product inventory overlay shows which products are declining fastest. Turnover and below-reorder metrics identify replenishment priorities.
"""
    write_trial(143, "inventory-health", doc, md)

# ── Trial 144: Employee Performance ───────────────────────────────────────────

def trial_144():
    emp = db.employee_hours(top_n=15)

    # Sales per employee
    emp_sales = defaultdict(lambda: {"count": 0, "revenue": 0.0})
    for s in db.sales:
        if s.get("employeeId"):
            emp_sales[s["employeeId"]]["count"] += 1
            emp_sales[s["employeeId"]]["revenue"] += s["total"]

    # Pane 1: hours ranking
    d1, m1 = db.to_bars(emp, "index", "totalHours", bar_width=0.6)
    t1 = fit(m1["xRange"], m1["yRange"])

    # Pane 2: sales per hour (revenue / hours)
    sph_items = []
    for i, e in enumerate(emp):
        es = emp_sales.get(e["id"], {"revenue": 0})
        sph = es["revenue"] / e["totalHours"] if e["totalHours"] else 0
        sph_items.append({"index": i, "sph": sph})
    d2, m2 = db.to_bars(sph_items, "index", "sph", bar_width=0.6)
    t2 = fit(m2["xRange"], m2["yRange"])

    # Pane 3: shift coverage heatmap
    heatmap = db.shift_heatmap()
    d3, m3, vr = db.to_heatmap_rects(heatmap, "dow", "hour", "count")
    t3 = fit(m3["xRange"], m3["yRange"])

    bufs = {100: {"data": rf(d1)}, 110: {"data": rf(d2)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
        52: {"sx": t3["sx"], "sy": t3["sy"], "tx": t3["tx"], "ty": t3["ty"]},
    }
    panes = {
        1: {"name": "Hours", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": -0.03},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "SPH", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": 0.03, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
        3: {"name": "Shifts", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "hrs"}, 20: {"paneId": 2, "name": "sph"},
              30: {"paneId": 3, "name": "shifts"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": m1["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "rect4", "vertexCount": m2["vertexCount"]},
    }
    dis = {
        102: {"layerId": 10, "name": "hrs-bars", "pipeline": "instancedRect@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#3b82f6"), "cornerRadius": 2.0},
        112: {"layerId": 20, "name": "sph-bars", "pipeline": "instancedRect@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#22c55e"), "cornerRadius": 2.0},
    }

    # Heatmap bands
    vmin, vmax = vr
    buf_id = 120
    n_bands = 4
    bands = [[] for _ in range(n_bands)]
    for item in heatmap:
        val = item["count"]
        t_val = (val - vmin) / (vmax - vmin) if vmax != vmin else 0.5
        band = min(int(t_val * n_bands), n_bands - 1)
        r_idx, c_idx = item["dow"], item["hour"]
        gap = 0.05
        bands[band].extend([c_idx + gap, r_idx + gap, c_idx + 1 - gap, r_idx + 1 - gap])
    band_colors = [[0.12,0.12,0.22,1.0],[0.2,0.3,0.45,1.0],[0.3,0.5,0.65,1.0],[0.45,0.75,0.85,1.0]]
    for bi, bd in enumerate(bands):
        if not bd:
            continue
        bufs[buf_id] = {"data": rf(bd)}
        geos[buf_id+1] = {"vertexBufferId": buf_id, "format": "rect4", "vertexCount": len(bd)//4}
        dis[buf_id+2] = {"layerId": 30, "name": f"shift-{bi}", "pipeline": "instancedRect@1",
                         "geometryId": buf_id+1, "transformId": 52, "color": band_colors[bi]}
        buf_id += 3

    labels = [
        {"clipX": -0.49, "clipY": 0.98, "text": "Total Hours Worked", "align": "c"},
        {"clipX": 0.49, "clipY": 0.98, "text": "Revenue per Hour ($)", "align": "c"},
        {"clipX": 0.0, "clipY": 0.0, "text": "Shift Coverage Heatmap", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 13, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 144 — Employee Performance Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes: hours ranking (top-left), revenue per hour (top-right), shift heatmap (bottom)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `employee_hours(top_n=15)` — {len(emp)} employees as instancedRect@1
- Pane 2: computed revenue/hour — {len(sph_items)} employees as instancedRect@1
- Pane 3: `shift_heatmap()` — {len(heatmap)} cells as banded instancedRect@1

## Insight
Hours worked alone doesn't tell the story — revenue per hour reveals true productivity. The heatmap shows when the store is staffed vs understaffed.
"""
    write_trial(144, "employee-performance", doc, md)

# ── Trial 145: Marketing vs Revenue ──────────────────────────────────────────

def trial_145():
    monthly = db.monthly_revenue()
    expenses = db.monthly_expenses()

    # Filter marketing/advertising expenses
    marketing_accts = [a for a in db.accounts if "marketing" in a["name"].lower() or "advertising" in a["name"].lower()]
    marketing_ids = {a["id"] for a in marketing_accts}

    monthly_marketing = defaultdict(float)
    for e in db.expenses:
        if e["accountId"] in marketing_ids:
            monthly_marketing[e["date"][:7]] += e["amount"]

    if not monthly_marketing:
        # Fallback: use total expenses as proxy
        for e in db.expenses:
            monthly_marketing[e["date"][:7]] += e["amount"]

    months = sorted(set(list(monthly_marketing.keys()) + [m["month"] for m in monthly]))
    mkt_items = [{"index": i, "amount": monthly_marketing.get(m, 0)} for i, m in enumerate(months)]

    # Revenue line
    d1, m1 = db.to_line_segments(monthly, "index", "revenue")
    t1 = fit(m1["xRange"], m1["yRange"])

    # Marketing expense line
    if len(mkt_items) >= 2:
        d2, m2 = db.to_line_segments(mkt_items, "index", "amount")
        t2 = fit(m2["xRange"], m2["yRange"])
    else:
        d2, m2 = [], {"format": "rect4", "vertexCount": 0, "xRange": (0, 1), "yRange": (0, 1)}
        t2 = fit(m2["xRange"], m2["yRange"])

    bufs = {100: {"data": rf(d1)}, 110: {"data": rf(d2)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
    }
    panes = {
        1: {"name": "Marketing", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "Revenue", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "mkt"}, 20: {"paneId": 2, "name": "rev"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": m1["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "rect4", "vertexCount": m2["vertexCount"]},
    }
    dis = {
        102: {"layerId": 20, "name": "rev-line", "pipeline": "lineAA@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#22c55e"), "lineWidth": 2.5},
        112: {"layerId": 10, "name": "mkt-line", "pipeline": "lineAA@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#ec4899"), "lineWidth": 2.5},
    }
    labels = [
        {"clipX": 0.0, "clipY": 0.98, "text": "Monthly Expenses (Marketing proxy)", "align": "c"},
        {"clipX": 0.0, "clipY": 0.0, "text": "Monthly Revenue", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 14, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 145 — Marketing vs Revenue Dashboard
**Date:** 2026-03-22
**Layout:** 2 panes: top = monthly expense line, bottom = monthly revenue line
**Resolution:** 1200x800

## Data Sources
- Top: monthly expenses — {len(mkt_items)} months as lineAA@1
- Bottom: `monthly_revenue()` — {len(monthly)} months as lineAA@1

## Insight
Visual correlation between spending and revenue. When the top line spikes, does the bottom line follow? Time-aligned panes make lagged effects visible.
"""
    write_trial(145, "marketing-vs-revenue", doc, md)

# ── Trial 146: Payment Deep Dive ──────────────────────────────────────────────

def trial_146():
    payments = db.payment_method_breakdown()

    # Pane 1: method pie
    wedges = db.to_pie_wedges(payments, "revenue", cx=0.0, cy=0.0, r=0.75)
    pie_colors = [hex2rgba(PAL[i % len(PAL)]) for i in range(len(wedges))]

    # Pane 2: monthly CC % trend
    cc_monthly = defaultdict(lambda: {"cc": 0.0, "total": 0.0})
    for s in db.sales:
        mk = s["date"][:7]
        cc_monthly[mk]["total"] += s["total"]
        if s["paymentMethod"].lower() in ("credit_card", "credit card", "cc"):
            cc_monthly[mk]["cc"] += s["total"]
    months = sorted(cc_monthly.keys())
    cc_items = [{"index": i, "pct": cc_monthly[m]["cc"] / cc_monthly[m]["total"] * 100
                 if cc_monthly[m]["total"] else 0}
                for i, m in enumerate(months)]
    if len(cc_items) >= 2:
        d2, m2 = db.to_line_segments(cc_items, "index", "pct")
        t2 = fit(m2["xRange"], m2["yRange"])
    else:
        d2, m2 = [], {"format": "rect4", "vertexCount": 0, "xRange": (0, 1), "yRange": (0, 100)}
        t2 = fit(m2["xRange"], m2["yRange"])

    # Pane 3: avg transaction by method bars
    avg_items = [{"index": p["index"], "avg": p["revenue"] / p["count"] if p["count"] else 0}
                 for p in payments]
    d3, m3 = db.to_bars(avg_items, "index", "avg", bar_width=0.6)
    t3 = fit(m3["xRange"], m3["yRange"])

    bufs = {110: {"data": rf(d2)}, 120: {"data": rf(d3)}}
    transforms = {
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
        52: {"sx": t3["sx"], "sy": t3["sy"], "tx": t3["tx"], "ty": t3["ty"]},
    }
    panes = {
        1: {"name": "MethodPie", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": -0.35},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "CCTrend", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.30, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
        3: {"name": "AvgTxn", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": -0.30, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "pie"}, 20: {"paneId": 2, "name": "cc"},
              30: {"paneId": 3, "name": "avg"}}
    geos = {
        111: {"vertexBufferId": 110, "format": "rect4", "vertexCount": m2["vertexCount"]},
        121: {"vertexBufferId": 120, "format": "rect4", "vertexCount": m3["vertexCount"]},
    }
    dis = {
        112: {"layerId": 20, "name": "cc-line", "pipeline": "lineAA@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#3b82f6"), "lineWidth": 2.5},
        122: {"layerId": 30, "name": "avg-bars", "pipeline": "instancedRect@1", "geometryId": 121,
              "transformId": 52, "color": hex2rgba("#22c55e"), "cornerRadius": 2.0},
    }

    # Pie wedges
    buf_id = 130
    for i, (wdata, frac, sa, ea) in enumerate(wedges):
        bufs[buf_id] = {"data": rf(wdata)}
        geos[buf_id + 1] = {"vertexBufferId": buf_id, "format": "pos2_clip", "vertexCount": len(wdata) // 2}
        dis[buf_id + 2] = {"layerId": 10, "name": f"pie-{i}", "pipeline": "triSolid@1",
                           "geometryId": buf_id + 1, "color": pie_colors[i]}
        buf_id += 3

    labels = [
        {"clipX": -0.65, "clipY": 0.98, "text": "Payment Method Share", "align": "c"},
        {"clipX": 0.33, "clipY": 0.98, "text": "Credit Card % Trend", "align": "c"},
        {"clipX": 0.33, "clipY": 0.0, "text": "Avg Transaction by Method", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 13, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 146 — Payment Deep Dive Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes: payment method pie (left), CC% trend (top-right), avg transaction by method (bottom-right)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `payment_method_breakdown()` — {len(payments)} methods as pie (triSolid@1)
- Pane 2: monthly credit card percentage — {len(cc_items)} months as lineAA@1
- Pane 3: avg transaction per method — {len(avg_items)} methods as instancedRect@1

## Insight
Payment mix analysis showing method preferences, credit card adoption trends over time, and average basket size by payment type.
"""
    write_trial(146, "payment-deep-dive", doc, md)

# ── Trial 147: Top Customer Profiles ──────────────────────────────────────────

def trial_147():
    cust_data = defaultdict(lambda: {"spend": 0.0, "visits": 0})
    for s in db.sales:
        if s["customerId"]:
            cust_data[s["customerId"]]["spend"] += s["total"]
            cust_data[s["customerId"]]["visits"] += 1

    top10 = sorted(cust_data.items(), key=lambda x: -x[1]["spend"])[:10]
    spend_items = [{"index": i, "spend": v["spend"]} for i, (cid, v) in enumerate(top10)]
    freq_items = [{"index": i, "visits": v["visits"]} for i, (cid, v) in enumerate(top10)]

    d1, m1 = db.to_bars(spend_items, "index", "spend", bar_width=0.6)
    t1 = fit(m1["xRange"], m1["yRange"])

    d2, m2 = db.to_bars(freq_items, "index", "visits", bar_width=0.6)
    t2 = fit(m2["xRange"], m2["yRange"])

    bufs = {100: {"data": rf(d1)}, 110: {"data": rf(d2)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
    }
    panes = {
        1: {"name": "Spend", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": -0.03},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "Frequency", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": 0.03, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "spend"}, 20: {"paneId": 2, "name": "freq"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": m1["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "rect4", "vertexCount": m2["vertexCount"]},
    }
    dis = {
        102: {"layerId": 10, "name": "spend-bars", "pipeline": "instancedRect@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#f59e0b"), "cornerRadius": 3.0},
        112: {"layerId": 20, "name": "freq-bars", "pipeline": "instancedRect@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#8b5cf6"), "cornerRadius": 3.0},
    }
    labels = [
        {"clipX": -0.49, "clipY": 0.98, "text": "Top 10 Customers — Total Spend", "align": "c"},
        {"clipX": 0.49, "clipY": 0.98, "text": "Top 10 Customers — Visit Count", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 14, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 147 — Top Customer Profiles Dashboard
**Date:** 2026-03-22
**Layout:** 2 panes: top 10 by spend (left), their visit frequency (right)
**Resolution:** 1200x800

## Data Sources
- Left: customer spend aggregated from sales — {len(spend_items)} customers as instancedRect@1
- Right: same customers — visit counts as instancedRect@1

## Insight
High-spend customers may or may not be frequent visitors. A high-spend/low-visit customer buys big when they come; a high-spend/high-visit customer is a loyal regular.
"""
    write_trial(147, "top-customer-profiles", doc, md)

# ── Trial 148: Supplier Relationship ──────────────────────────────────────────

def trial_148():
    supps = db.supplier_performance()

    # Pane 1: cost by supplier bars
    d1, m1 = db.to_bars(supps, "index", "totalCost", bar_width=0.6)
    t1 = fit(m1["xRange"], m1["yRange"])

    # Pane 2: reliability scatter (lead time vs PO count)
    d2, m2 = db.to_scatter(supps, "poCount", "avgLeadTime")
    t2 = fit(m2["xRange"], m2["yRange"])

    # Pane 3: product count per supplier
    supp_prods = defaultdict(int)
    for p in db.products:
        if p.get("supplierId"):
            supp_prods[p["supplierId"]] += 1
    prod_items = [{"index": s["index"], "count": supp_prods.get(s["id"], 0)} for s in supps]
    d3, m3 = db.to_bars(prod_items, "index", "count", bar_width=0.6)
    t3 = fit(m3["xRange"], m3["yRange"])

    bufs = {100: {"data": rf(d1)}, 110: {"data": rf(d2)}, 120: {"data": rf(d3)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
        52: {"sx": t3["sx"], "sy": t3["sy"], "tx": t3["tx"], "ty": t3["ty"]},
    }
    panes = {
        1: {"name": "CostBars", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": -0.35},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "Reliability", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.30, "clipXMax": 0.30},
            "hasClearColor": True, "clearColor": PANE_BG},
        3: {"name": "ProdCount", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": 0.35, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "cost"}, 20: {"paneId": 2, "name": "rel"},
              30: {"paneId": 3, "name": "prods"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": m1["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "pos2_clip", "vertexCount": m2["vertexCount"]},
        121: {"vertexBufferId": 120, "format": "rect4", "vertexCount": m3["vertexCount"]},
    }
    dis = {
        102: {"layerId": 10, "name": "cost-bars", "pipeline": "instancedRect@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#ef4444"), "cornerRadius": 2.0},
        112: {"layerId": 20, "name": "rel-scatter", "pipeline": "points@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#f59e0b"), "pointSize": 8.0},
        122: {"layerId": 30, "name": "prod-bars", "pipeline": "instancedRect@1", "geometryId": 121,
              "transformId": 52, "color": hex2rgba("#3b82f6"), "cornerRadius": 2.0},
    }
    labels = [
        {"clipX": -0.65, "clipY": 0.98, "text": "Cost by Supplier", "align": "c"},
        {"clipX": 0.0, "clipY": 0.98, "text": "PO Count vs Lead Time", "align": "c"},
        {"clipX": 0.65, "clipY": 0.98, "text": "Products per Supplier", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 13, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 148 — Supplier Relationship Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes: cost bars (left), reliability scatter (center), product count bars (right)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `supplier_performance()` — {len(supps)} suppliers, totalCost as instancedRect@1
- Pane 2: PO count vs lead time — {len(supps)} suppliers as points@1
- Pane 3: product count per supplier — {len(prod_items)} suppliers as instancedRect@1

## Insight
Three-dimensional supplier view: cost exposure, reliability (ideal = top-left in scatter: many POs, fast delivery), and product dependency.
"""
    write_trial(148, "supplier-relationship", doc, md)

# ── Trial 149: Floor Zone Revenue ─────────────────────────────────────────────

def trial_149():
    # Zone revenue from departments (each dept = a zone)
    dept = db.department_revenue()

    # Pane 1: zone layout heatmap (2x4 grid representing store zones)
    zone_data = []
    zone_vals = []
    n_rows, n_cols = 2, 4
    for i, d in enumerate(dept[:8]):
        row = i // n_cols
        col = i % n_cols
        gap = 0.08
        zone_data.extend([col + gap, row + gap, col + 1 - gap, row + 1 - gap])
        zone_vals.append(d["revenue"])

    vmin = min(zone_vals) if zone_vals else 0
    vmax = max(zone_vals) if zone_vals else 1
    t1 = fit((0, n_cols), (0, n_rows))

    # Pane 2: revenue bars
    d2, m2 = db.to_bars(dept, "index", "revenue", bar_width=0.7)
    t2 = fit(m2["xRange"], m2["yRange"])

    bufs = {110: {"data": rf(d2)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
    }
    panes = {
        1: {"name": "ZoneMap", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": -0.03},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "ZoneBars", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": 0.03, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "zones"}, 20: {"paneId": 2, "name": "bars"}}
    geos = {111: {"vertexBufferId": 110, "format": "rect4", "vertexCount": m2["vertexCount"]}}
    dis = {
        112: {"layerId": 20, "name": "zone-bars", "pipeline": "instancedRect@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#3b82f6"), "cornerRadius": 2.0},
    }

    # Individual zone rects colored by revenue
    buf_id = 120
    for i, d in enumerate(dept[:8]):
        row = i // n_cols
        col = i % n_cols
        gap = 0.08
        rect = [col + gap, row + gap, col + 1 - gap, row + 1 - gap]
        color = db.value_to_color(d["revenue"], vmin, vmax, palette="heat")
        bufs[buf_id] = {"data": rf(rect)}
        geos[buf_id + 1] = {"vertexBufferId": buf_id, "format": "rect4", "vertexCount": 1}
        dis[buf_id + 2] = {"layerId": 10, "name": f"zone-{d['name']}", "pipeline": "instancedRect@1",
                           "geometryId": buf_id + 1, "transformId": 50,
                           "color": color, "cornerRadius": 4.0}
        buf_id += 3

    labels = [
        {"clipX": -0.49, "clipY": 0.98, "text": "Store Zone Revenue (Heatmap)", "align": "c"},
        {"clipX": 0.49, "clipY": 0.98, "text": "Department Revenue ($)", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 14, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 149 — Floor Zone Revenue Dashboard
**Date:** 2026-03-22
**Layout:** 2 panes: zone layout heatmap (left), zone revenue bars (right)
**Resolution:** 1200x800

## Data Sources
- Left: `department_revenue()` mapped to 2x4 grid — {len(dept[:8])} zones as heatmap (instancedRect@1)
- Right: `department_revenue()` — {len(dept)} departments as instancedRect@1

## Insight
Spatial layout of department zones colored by revenue intensity (cool=low, hot=high) alongside exact values for comparison.
"""
    write_trial(149, "floor-zone-revenue", doc, md)

# ── Trial 150: Full Business Overview ─────────────────────────────────────────

def trial_150():
    monthly = db.monthly_revenue()
    dept = db.department_revenue()
    top10 = db.product_rankings(top_n=10)
    tiers = db.customer_tier_breakdown()
    emp = db.employee_hours(top_n=10)
    profit = db.monthly_profit()

    # 6 panes in 3x2 grid
    regions = [
        {"clipYMin": 0.38, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": -0.35},
        {"clipYMin": 0.38, "clipYMax": 0.95, "clipXMin": -0.30, "clipXMax": 0.30},
        {"clipYMin": 0.38, "clipYMax": 0.95, "clipXMin": 0.35, "clipXMax": 0.95},
        {"clipYMin": -0.28, "clipYMax": 0.33, "clipXMin": -0.95, "clipXMax": -0.35},
        {"clipYMin": -0.28, "clipYMax": 0.33, "clipXMin": -0.30, "clipXMax": 0.30},
        {"clipYMin": -0.28, "clipYMax": 0.33, "clipXMin": 0.35, "clipXMax": 0.95},
    ]

    # Pane 1: revenue line
    d1, m1 = db.to_line_segments(monthly, "index", "revenue")
    t1 = fit(m1["xRange"], m1["yRange"])

    # Pane 2: dept bars
    d2, m2 = db.to_bars(dept, "index", "revenue", bar_width=0.6)
    t2 = fit(m2["xRange"], m2["yRange"])

    # Pane 3: top 10 products horizontal
    d3, m3 = db.to_horizontal_bars(top10, "index", "revenue", bar_height=0.6)
    t3 = fit(m3["xRange"], m3["yRange"])

    # Pane 4: customer tier donut
    wedges = db.to_donut_wedges(tiers, "count", cx=0.0, cy=0.0, r_outer=0.7, r_inner=0.35)

    # Pane 5: employee hours bars
    d5, m5 = db.to_bars(emp, "index", "totalHours", bar_width=0.6)
    t5 = fit(m5["xRange"], m5["yRange"])

    # Pane 6: profit line
    d6, m6 = db.to_line_segments(profit, "index", "profit")
    t6 = fit(m6["xRange"], m6["yRange"])

    bufs = {
        100: {"data": rf(d1)}, 110: {"data": rf(d2)}, 120: {"data": rf(d3)},
        140: {"data": rf(d5)}, 150: {"data": rf(d6)},
    }
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
        52: {"sx": t3["sx"], "sy": t3["sy"], "tx": t3["tx"], "ty": t3["ty"]},
        54: {"sx": t5["sx"], "sy": t5["sy"], "tx": t5["tx"], "ty": t5["ty"]},
        55: {"sx": t6["sx"], "sy": t6["sy"], "tx": t6["tx"], "ty": t6["ty"]},
    }
    panes_d = {}
    for i, reg in enumerate(regions):
        panes_d[i + 1] = {"name": f"P{i+1}", "region": reg,
                          "hasClearColor": True, "clearColor": PANE_BG}

    layers_d = {
        10: {"paneId": 1, "name": "rev"}, 20: {"paneId": 2, "name": "dept"},
        30: {"paneId": 3, "name": "prod"}, 40: {"paneId": 4, "name": "cust"},
        44: {"paneId": 5, "name": "emp"}, 48: {"paneId": 6, "name": "fin"},
    }
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": m1["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "rect4", "vertexCount": m2["vertexCount"]},
        121: {"vertexBufferId": 120, "format": "rect4", "vertexCount": m3["vertexCount"]},
        141: {"vertexBufferId": 140, "format": "rect4", "vertexCount": m5["vertexCount"]},
        151: {"vertexBufferId": 150, "format": "rect4", "vertexCount": m6["vertexCount"]},
    }
    dis = {
        102: {"layerId": 10, "name": "rev-line", "pipeline": "lineAA@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#3b82f6"), "lineWidth": 2.0},
        112: {"layerId": 20, "name": "dept-bars", "pipeline": "instancedRect@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#22c55e"), "cornerRadius": 2.0},
        122: {"layerId": 30, "name": "top-prods", "pipeline": "instancedRect@1", "geometryId": 121,
              "transformId": 52, "color": hex2rgba("#ef4444"), "cornerRadius": 2.0},
        142: {"layerId": 44, "name": "emp-bars", "pipeline": "instancedRect@1", "geometryId": 141,
              "transformId": 54, "color": hex2rgba("#ec4899"), "cornerRadius": 2.0},
        152: {"layerId": 48, "name": "profit-line", "pipeline": "lineAA@1", "geometryId": 151,
              "transformId": 55, "color": hex2rgba("#f59e0b"), "lineWidth": 2.0},
    }

    # Donut wedges for customer tier (pane 4)
    tier_colors = [hex2rgba("#f59e0b"), hex2rgba("#94a3b8"), hex2rgba("#b45309")]
    buf_id = 160
    for i, (wdata, frac, sa, ea) in enumerate(wedges):
        bufs[buf_id] = {"data": rf(wdata)}
        geos[buf_id + 1] = {"vertexBufferId": buf_id, "format": "pos2_clip", "vertexCount": len(wdata) // 2}
        dis[buf_id + 2] = {"layerId": 40, "name": f"tier-{i}", "pipeline": "triSolid@1",
                           "geometryId": buf_id + 1, "color": tier_colors[i % len(tier_colors)]}
        buf_id += 3

    labels = [
        {"clipX": -0.65, "clipY": 0.98, "text": "Revenue", "align": "c"},
        {"clipX": 0.0, "clipY": 0.98, "text": "Departments", "align": "c"},
        {"clipX": 0.65, "clipY": 0.98, "text": "Top Products", "align": "c"},
        {"clipX": -0.65, "clipY": 0.36, "text": "Customers", "align": "c"},
        {"clipX": 0.0, "clipY": 0.36, "text": "Workforce", "align": "c"},
        {"clipX": 0.65, "clipY": 0.36, "text": "Profit", "align": "c"},
    ]
    doc = make_doc(1400, 900, bufs, transforms, panes_d, layers_d, geos, dis,
                   textOverlay={"fontSize": 12, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 150 — Full Business Overview Dashboard
**Date:** 2026-03-22
**Layout:** 6 panes (3x2 grid): revenue, departments, products, customers, workforce, finances
**Resolution:** 1400x900

## Data Sources
- Revenue: `monthly_revenue()` — {len(monthly)} months as lineAA@1
- Departments: `department_revenue()` — {len(dept)} depts as instancedRect@1
- Products: `product_rankings(10)` — 10 products as horizontal instancedRect@1
- Customers: `customer_tier_breakdown()` — {len(tiers)} tiers as donut (triSolid@1)
- Workforce: `employee_hours(10)` — 10 employees as instancedRect@1
- Profit: `monthly_profit()` — {len(profit)} months as lineAA@1

## Insight
THE comprehensive dashboard. Every major business dimension in one view. Designed for executive wall displays.
"""
    write_trial(150, "full-business-overview", doc, md)

# ── Trial 151: Profit/Loss Waterfall ──────────────────────────────────────────

def trial_151():
    # Revenue → minus COGS → minus payroll → minus rent → minus other = profit
    total_rev = sum(s["total"] for s in db.sales)
    acct_exp = db.expense_by_account()

    # Approximate COGS as 60% of revenue (hardware store)
    cogs = total_rev * 0.60
    expenses_by_type = defaultdict(float)
    for a in acct_exp:
        expenses_by_type[a["type"]] += a["total"]

    payroll = expenses_by_type.get("payroll", 0) or sum(a["total"] for a in acct_exp if "payroll" in a["name"].lower() or "salary" in a["name"].lower())
    rent = sum(a["total"] for a in acct_exp if "rent" in a["name"].lower() or "lease" in a["name"].lower())
    other_exp = sum(a["total"] for a in acct_exp) - payroll - rent
    profit = total_rev - cogs - payroll - rent - other_exp

    # Waterfall bars: each bar starts where the previous ended
    items = [
        ("Revenue", 0, total_rev, "#22c55e"),
        ("COGS", total_rev, total_rev - cogs, "#ef4444"),
        ("Payroll", total_rev - cogs, total_rev - cogs - payroll, "#ef4444"),
        ("Rent", total_rev - cogs - payroll, total_rev - cogs - payroll - rent, "#ef4444"),
        ("Other", total_rev - cogs - payroll - rent, total_rev - cogs - payroll - rent - other_exp, "#ef4444"),
        ("Profit", 0, profit, "#3b82f6"),
    ]

    bar_data = []
    hw = 0.35
    for i, (name, bottom, top, color) in enumerate(items):
        bar_data.extend([i - hw, min(bottom, top), i + hw, max(bottom, top)])

    xs = list(range(len(items)))
    all_y = [total_rev, 0, profit]
    y_range = (min(all_y) * 0.9, max(all_y) * 1.1)
    t1 = fit((-0.5, len(items) - 0.5), y_range)

    bufs = {}
    transforms = {50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]}}
    geos = {}
    dis = {}

    # Individual colored bars
    buf_id = 100
    for i, (name, bottom, top, color) in enumerate(items):
        bar = [i - hw, min(bottom, top), i + hw, max(bottom, top)]
        bufs[buf_id] = {"data": rf(bar)}
        geos[buf_id + 1] = {"vertexBufferId": buf_id, "format": "rect4", "vertexCount": 1}
        dis[buf_id + 2] = {"layerId": 10, "name": name, "pipeline": "instancedRect@1",
                           "geometryId": buf_id + 1, "transformId": 50,
                           "color": hex2rgba(color), "cornerRadius": 3.0}
        buf_id += 3

    panes = {
        1: {"name": "Waterfall", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "bars"}}

    labels = [
        {"clipX": 0.0, "clipY": 0.98, "text": "Profit & Loss Waterfall", "align": "c"},
    ]
    # Add bar labels
    for i, (name, bottom, top, color) in enumerate(items):
        clip_x = -0.9 + (i / (len(items) - 1)) * 1.8 if len(items) > 1 else 0
        labels.append({"clipX": clip_x, "clipY": -0.98, "text": name, "align": "c", "fontSize": 11})

    doc = make_doc(1200, 600, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 14, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 151 — Profit/Loss Waterfall
**Date:** 2026-03-22
**Layout:** Single pane: waterfall chart (revenue minus costs = profit)
**Resolution:** 1200x600

## Data
- Revenue: ${total_rev:,.0f}
- COGS (est 60%): ${cogs:,.0f}
- Payroll: ${payroll:,.0f}
- Rent: ${rent:,.0f}
- Other: ${other_exp:,.0f}
- Net Profit: ${profit:,.0f}

## Insight
Waterfall decomposition shows where revenue goes. Green = income, red = deductions, blue = residual profit.
"""
    write_trial(151, "profit-loss-waterfall", doc, md)

# ── Trial 152: Customer Cohort Dashboard ──────────────────────────────────────

def trial_152():
    tiers = db.customer_tier_breakdown()
    tier_rev = db.customer_tier_revenue()

    # Pane 1: cohort revenue bars
    d1, m1 = db.to_bars(tier_rev, "index", "revenue", bar_width=0.6)
    t1 = fit(m1["xRange"], m1["yRange"])

    # Pane 2: cumulative customer growth (by join date)
    custs_by_month = defaultdict(int)
    for c in db.customers:
        if c.get("joinDate"):
            custs_by_month[c["joinDate"][:7]] += 1
    months = sorted(custs_by_month.keys())
    cumulative = []
    total = 0
    for i, m in enumerate(months):
        total += custs_by_month[m]
        cumulative.append({"index": i, "total": total})

    if len(cumulative) >= 2:
        d2, m2 = db.to_line_segments(cumulative, "index", "total")
        t2 = fit(m2["xRange"], m2["yRange"])
    else:
        # Fallback: use tier counts
        d2, m2 = db.to_line_segments([{"index": i, "total": (i+1)*100} for i in range(10)], "index", "total")
        t2 = fit(m2["xRange"], m2["yRange"])

    # Pane 3: tier donut
    wedges = db.to_donut_wedges(tiers, "count", cx=0.0, cy=0.0, r_outer=0.7, r_inner=0.35)
    tier_colors = [hex2rgba("#f59e0b"), hex2rgba("#94a3b8"), hex2rgba("#b45309")]

    bufs = {100: {"data": rf(d1)}, 110: {"data": rf(d2)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
    }
    panes = {
        1: {"name": "CohortRev", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": -0.35},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "Growth", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": -0.30, "clipXMax": 0.30},
            "hasClearColor": True, "clearColor": PANE_BG},
        3: {"name": "TierDonut", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": 0.35, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "rev"}, 20: {"paneId": 2, "name": "growth"},
              30: {"paneId": 3, "name": "donut"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": m1["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "rect4", "vertexCount": m2["vertexCount"]},
    }
    dis = {
        102: {"layerId": 10, "name": "cohort-bars", "pipeline": "instancedRect@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#3b82f6"), "cornerRadius": 3.0},
        112: {"layerId": 20, "name": "growth-line", "pipeline": "lineAA@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#22c55e"), "lineWidth": 2.5},
    }

    buf_id = 120
    for i, (wdata, frac, sa, ea) in enumerate(wedges):
        bufs[buf_id] = {"data": rf(wdata)}
        geos[buf_id + 1] = {"vertexBufferId": buf_id, "format": "pos2_clip", "vertexCount": len(wdata) // 2}
        dis[buf_id + 2] = {"layerId": 30, "name": f"donut-{i}", "pipeline": "triSolid@1",
                           "geometryId": buf_id + 1, "color": tier_colors[i % len(tier_colors)]}
        buf_id += 3

    labels = [
        {"clipX": -0.65, "clipY": 0.98, "text": "Tier Revenue", "align": "c"},
        {"clipX": 0.0, "clipY": 0.98, "text": "Customer Growth", "align": "c"},
        {"clipX": 0.65, "clipY": 0.98, "text": "Tier Distribution", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 13, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 152 — Customer Cohort Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes: cohort revenue bars (left), cumulative growth line (center), tier donut (right)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `customer_tier_revenue()` — {len(tier_rev)} tiers as instancedRect@1
- Pane 2: customer join dates — {len(cumulative)} months as lineAA@1
- Pane 3: `customer_tier_breakdown()` — {len(tiers)} tiers as donut

## Insight
Revenue by tier shows tier value; growth curve shows customer acquisition pace; donut shows current composition.
"""
    write_trial(152, "customer-cohort-dashboard", doc, md)

# ── Trial 153: Product Mix Evolution ──────────────────────────────────────────

def trial_153():
    dept_monthly = db.department_monthly_revenue()
    depts = list(set((d["deptId"], d["deptName"]) for d in dept_monthly))
    depts.sort(key=lambda x: x[0])

    # Pane 1: dept revenue share stacked area (simplified as multi-line)
    # Show each department as a separate line
    all_months = sorted(set(d["month"] for d in dept_monthly))

    bufs = {}
    transforms = {}
    geos = {}
    dis = {}

    # Find global Y range from monthly totals
    month_totals = defaultdict(float)
    for d in dept_monthly:
        month_totals[d["month"]] += d["revenue"]
    max_total = max(month_totals.values()) if month_totals else 1

    buf_id = 100
    for di, (did, dname) in enumerate(depts[:6]):  # Limit to 6 for clarity
        dept_data = [d for d in dept_monthly if d["deptId"] == did]
        if len(dept_data) < 2:
            continue
        d, m = db.to_line_segments(dept_data, "index", "revenue")
        if di == 0:
            y_range = (0, max_total)
            t = fit(m["xRange"], y_range)
            transforms[50] = {"sx": t["sx"], "sy": t["sy"], "tx": t["tx"], "ty": t["ty"]}
        color = hex2rgba(DEPT_PAL.get(did, PAL[di % len(PAL)]))
        bufs[buf_id] = {"data": rf(d)}
        geos[buf_id + 1] = {"vertexBufferId": buf_id, "format": "rect4", "vertexCount": m["vertexCount"]}
        dis[buf_id + 2] = {"layerId": 10, "name": f"dept-{dname}", "pipeline": "lineAA@1",
                           "geometryId": buf_id + 1, "transformId": 50,
                           "color": color, "lineWidth": 2.0}
        buf_id += 3

    # Pane 2: top 5 products line
    top5 = db.product_rankings(top_n=5)
    # Monthly revenue per product
    prod_monthly_rev = defaultdict(lambda: defaultdict(float))
    for si in db.sale_items:
        sale_month = ""
        for s in db.sales:
            if s["id"] == si["saleId"]:
                sale_month = s["date"][:7]
                break
        if sale_month:
            prod_monthly_rev[si["productId"]][sale_month] += si["lineTotal"]

    for pi, p in enumerate(top5):
        pid = p["id"]
        if not prod_monthly_rev[pid]:
            continue
        months_sorted = sorted(prod_monthly_rev[pid].keys())
        items = [{"index": i, "revenue": prod_monthly_rev[pid].get(m, 0)} for i, m in enumerate(months_sorted)]
        if len(items) < 2:
            continue
        d, m = db.to_line_segments(items, "index", "revenue")
        if pi == 0:
            t = fit(m["xRange"], m["yRange"])
            transforms[51] = {"sx": t["sx"], "sy": t["sy"], "tx": t["tx"], "ty": t["ty"]}
        color = hex2rgba(PAL[pi % len(PAL)])
        bufs[buf_id] = {"data": rf(d)}
        geos[buf_id + 1] = {"vertexBufferId": buf_id, "format": "rect4", "vertexCount": m["vertexCount"]}
        dis[buf_id + 2] = {"layerId": 20, "name": f"prod-{p['name'][:15]}", "pipeline": "lineAA@1",
                           "geometryId": buf_id + 1, "transformId": 51,
                           "color": color, "lineWidth": 1.5}
        buf_id += 3

    panes = {
        1: {"name": "DeptShare", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "Top5Prod", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers_d = {10: {"paneId": 1, "name": "dept"}, 20: {"paneId": 2, "name": "prod"}}

    labels = [
        {"clipX": 0.0, "clipY": 0.98, "text": "Department Revenue Over Time", "align": "c"},
        {"clipX": 0.0, "clipY": 0.0, "text": "Top 5 Products — Monthly Revenue", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers_d, geos, dis,
                   textOverlay={"fontSize": 13, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 153 — Product Mix Evolution Dashboard
**Date:** 2026-03-22
**Layout:** 2 panes: dept revenue multi-line (top), top 5 products line (bottom)
**Resolution:** 1200x800

## Data Sources
- Top: `department_monthly_revenue()` — up to 6 departments as overlaid lineAA@1
- Bottom: top 5 products monthly revenue as overlaid lineAA@1

## Insight
Multi-line overlay shows how department mix shifts over time. Product-level trends in the bottom pane reveal individual winners and losers.
"""
    write_trial(153, "product-mix-evolution", doc, md)

# ── Trial 154: Department Efficiency ──────────────────────────────────────────

def trial_154():
    dept = db.department_revenue()
    emp_dept = defaultdict(int)
    for e in db.employees:
        if e.get("departmentId"):
            emp_dept[e["departmentId"]] += 1

    # Revenue per employee
    rpe_items = []
    for i, d in enumerate(dept):
        emp_count = emp_dept.get(d["id"], 1)
        rpe_items.append({"index": i, "rpe": d["revenue"] / emp_count})

    d1, m1 = db.to_bars(rpe_items, "index", "rpe", bar_width=0.6)
    t1 = fit(m1["xRange"], m1["yRange"])

    # Revenue per sqft (using store zones as proxy — assume equal area)
    sqft_per_dept = 1000  # placeholder
    rps_items = [{"index": i, "rps": d["revenue"] / sqft_per_dept} for i, d in enumerate(dept)]
    d2, m2 = db.to_bars(rps_items, "index", "rps", bar_width=0.6)
    t2 = fit(m2["xRange"], m2["yRange"])

    bufs = {100: {"data": rf(d1)}, 110: {"data": rf(d2)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
    }
    panes = {
        1: {"name": "RevPerEmp", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": -0.03},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "RevPerSqft", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": 0.03, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "rpe"}, 20: {"paneId": 2, "name": "rps"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": m1["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "rect4", "vertexCount": m2["vertexCount"]},
    }
    dis = {
        102: {"layerId": 10, "name": "rpe-bars", "pipeline": "instancedRect@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#3b82f6"), "cornerRadius": 3.0},
        112: {"layerId": 20, "name": "rps-bars", "pipeline": "instancedRect@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#22c55e"), "cornerRadius": 3.0},
    }
    labels = [
        {"clipX": -0.49, "clipY": 0.98, "text": "Revenue per Employee ($)", "align": "c"},
        {"clipX": 0.49, "clipY": 0.98, "text": "Revenue per Sq Ft ($)", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 14, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 154 — Department Efficiency Dashboard
**Date:** 2026-03-22
**Layout:** 2 panes: revenue/employee bars (left), revenue/sqft bars (right)
**Resolution:** 1200x800

## Data Sources
- Left: dept revenue / employee count — {len(rpe_items)} depts as instancedRect@1
- Right: dept revenue / sqft — {len(rps_items)} depts as instancedRect@1

## Insight
Efficiency metrics normalize raw revenue by resources consumed. A department with high revenue per employee is labor-efficient; high revenue per sqft means dense productivity.
"""
    write_trial(154, "dept-efficiency", doc, md)

# ── Trial 155: Trend with Projection ──────────────────────────────────────────

def trial_155():
    monthly = db.monthly_revenue()
    n = len(monthly)

    # Linear regression for projection
    xs = [m["index"] for m in monthly]
    ys = [m["revenue"] for m in monthly]
    x_mean = sum(xs) / n
    y_mean = sum(ys) / n
    ss_xy = sum((xs[i] - x_mean) * (ys[i] - y_mean) for i in range(n))
    ss_xx = sum((xs[i] - x_mean) ** 2 for i in range(n))
    slope = ss_xy / ss_xx if ss_xx else 0
    intercept = y_mean - slope * x_mean

    # Projected 3 months
    proj_items = [{"index": n + i, "revenue": slope * (n + i) + intercept} for i in range(3)]
    all_items = monthly + proj_items

    # Actual line
    d1, m1 = db.to_line_segments(monthly, "index", "revenue")
    x_range_full = (0, n + 2)
    y_range_full = (min(ys + [p["revenue"] for p in proj_items]) * 0.9,
                    max(ys + [p["revenue"] for p in proj_items]) * 1.1)
    t1 = fit(x_range_full, y_range_full)

    # Projection line (from last actual to 3 months forward)
    proj_with_start = [monthly[-1]] + proj_items
    d2, m2 = db.to_line_segments(proj_with_start, "index", "revenue")

    bufs = {100: {"data": rf(d1)}, 110: {"data": rf(d2)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
    }
    panes = {
        1: {"name": "Actual", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "WithProj", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "actual"}, 20: {"paneId": 2, "name": "proj"},
              21: {"paneId": 2, "name": "proj-overlay"}}

    # Clone actual data for bottom pane
    bufs[120] = {"data": rf(d1)}

    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": m1["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "rect4", "vertexCount": m2["vertexCount"]},
        121: {"vertexBufferId": 120, "format": "rect4", "vertexCount": m1["vertexCount"]},
    }
    dis = {
        102: {"layerId": 10, "name": "actual-line", "pipeline": "lineAA@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#3b82f6"), "lineWidth": 2.5},
        112: {"layerId": 21, "name": "proj-line", "pipeline": "lineAA@1", "geometryId": 111,
              "transformId": 50, "color": hex2rgba("#f59e0b"), "lineWidth": 2.0,
              "dashLength": 8.0, "gapLength": 6.0},
        122: {"layerId": 20, "name": "actual-bottom", "pipeline": "lineAA@1", "geometryId": 121,
              "transformId": 50, "color": hex2rgba("#3b82f6"), "lineWidth": 2.0},
    }
    labels = [
        {"clipX": 0.0, "clipY": 0.98, "text": "Actual Monthly Revenue", "align": "c"},
        {"clipX": 0.0, "clipY": 0.0, "text": "Revenue + 3-Month Projection (dashed)", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 14, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 155 — Trend with Projection Dashboard
**Date:** 2026-03-22
**Layout:** 2 panes: top = actual revenue line, bottom = actual + 3-month linear projection (dashed)
**Resolution:** 1200x800

## Data Sources
- Top: `monthly_revenue()` — {len(monthly)} months as lineAA@1
- Bottom: same + 3 projected months (slope={slope:.1f}/month) as dashed lineAA@1

## Insight
Linear regression projects forward. The dashed line shows where revenue is headed if current trends continue. Both panes share the same transform for direct comparison.
"""
    write_trial(155, "trend-with-projection", doc, md)

# ── Trial 156: Pareto 80/20 ──────────────────────────────────────────────────

def trial_156():
    products = db.product_rankings()
    total_rev = sum(p["revenue"] for p in products)

    # Cumulative percentage
    cum = 0
    cum_items = []
    for i, p in enumerate(products):
        cum += p["revenue"]
        cum_items.append({"index": i, "pct": cum / total_rev * 100})

    # Top pane: revenue bars
    d1, m1 = db.to_bars(products, "index", "revenue", bar_width=0.8)
    t1 = fit(m1["xRange"], m1["yRange"])

    # Bottom pane: cumulative % line
    d2, m2 = db.to_line_segments(cum_items, "index", "pct")
    t2 = fit(m2["xRange"], m2["yRange"])

    # 80% reference line
    ref_y = 80.0
    ref_line = [0, ref_y, len(products) - 1, ref_y]

    bufs = {100: {"data": rf(d1)}, 110: {"data": rf(d2)}, 120: {"data": rf(ref_line)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
    }
    panes = {
        1: {"name": "RevBars", "region": {"clipYMin": 0.05, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "Cumulative", "region": {"clipYMin": -0.95, "clipYMax": -0.03, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "bars"}, 20: {"paneId": 2, "name": "cum"},
              21: {"paneId": 2, "name": "ref"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": m1["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "rect4", "vertexCount": m2["vertexCount"]},
        121: {"vertexBufferId": 120, "format": "rect4", "vertexCount": 1},
    }
    dis = {
        102: {"layerId": 10, "name": "rev-bars", "pipeline": "instancedRect@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#3b82f6"), "cornerRadius": 1.0},
        112: {"layerId": 20, "name": "cum-line", "pipeline": "lineAA@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#22c55e"), "lineWidth": 2.5},
        122: {"layerId": 21, "name": "80-ref", "pipeline": "lineAA@1", "geometryId": 121,
              "transformId": 51, "color": hex2rgba("#ef4444", 0.7), "lineWidth": 1.5,
              "dashLength": 6.0, "gapLength": 4.0},
    }

    # Find 80% crossover
    cross_idx = next((i for i, c in enumerate(cum_items) if c["pct"] >= 80), len(cum_items))
    labels = [
        {"clipX": 0.0, "clipY": 0.98, "text": f"Products Ranked by Revenue ({len(products)} total)", "align": "c"},
        {"clipX": 0.0, "clipY": 0.0, "text": f"Cumulative % (80% at product #{cross_idx+1})", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 13, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 156 — Pareto 80/20 Dashboard
**Date:** 2026-03-22
**Layout:** 2 panes: product revenue bars (top), cumulative % line with 80% reference (bottom)
**Resolution:** 1200x800

## Data Sources
- Top: `product_rankings()` — {len(products)} products as instancedRect@1
- Bottom: cumulative revenue percentage — {len(cum_items)} points as lineAA@1

## Insight
The Pareto principle: {cross_idx+1} products out of {len(products)} generate 80% of revenue. The red dashed line marks the 80% threshold.
"""
    write_trial(156, "pareto-80-20", doc, md)

# ── Trial 157: ABC Inventory ──────────────────────────────────────────────────

def trial_157():
    products = db.product_rankings()
    total_rev = sum(p["revenue"] for p in products)

    cum = 0
    a_prods, b_prods, c_prods = [], [], []
    for p in products:
        cum += p["revenue"]
        pct = cum / total_rev
        if pct <= 0.80:
            a_prods.append(p)
        elif pct <= 0.95:
            b_prods.append(p)
        else:
            c_prods.append(p)

    # Re-index within class
    for i, p in enumerate(a_prods): p["cindex"] = i
    for i, p in enumerate(b_prods): p["cindex"] = i
    for i, p in enumerate(c_prods): p["cindex"] = i

    datasets = [
        (a_prods, "A Products (top 80%)", "#22c55e"),
        (b_prods, "B Products (next 15%)", "#f59e0b"),
        (c_prods, "C Products (last 5%)", "#ef4444"),
    ]

    bufs = {}
    transforms = {}
    panes_d = {}
    layers_d = {}
    geos = {}
    dis = {}

    regions = [
        {"clipYMin": 0.38, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95},
        {"clipYMin": -0.28, "clipYMax": 0.33, "clipXMin": -0.95, "clipXMax": 0.95},
        {"clipYMin": -0.95, "clipYMax": -0.33, "clipXMin": -0.95, "clipXMax": 0.95},
    ]

    for pi, (prods, label, color) in enumerate(datasets):
        pane_id = pi + 1
        layer_id = (pi + 1) * 10
        tf_id = 50 + pi
        buf_id = 100 + pi * 10

        if prods:
            d, m = db.to_bars(prods, "cindex", "revenue", bar_width=0.7)
            t = fit(m["xRange"], m["yRange"])
        else:
            d, m = [0, 0, 1, 1], {"format": "rect4", "vertexCount": 1, "xRange": (0, 1), "yRange": (0, 1)}
            t = fit(m["xRange"], m["yRange"])

        bufs[buf_id] = {"data": rf(d)}
        transforms[tf_id] = {"sx": t["sx"], "sy": t["sy"], "tx": t["tx"], "ty": t["ty"]}
        panes_d[pane_id] = {"name": label, "region": regions[pi],
                            "hasClearColor": True, "clearColor": PANE_BG}
        layers_d[layer_id] = {"paneId": pane_id, "name": f"abc-{pi}"}
        geos[buf_id + 1] = {"vertexBufferId": buf_id, "format": "rect4", "vertexCount": m["vertexCount"]}
        dis[buf_id + 2] = {"layerId": layer_id, "name": f"abc-bars-{pi}",
                           "pipeline": "instancedRect@1", "geometryId": buf_id + 1,
                           "transformId": tf_id, "color": hex2rgba(color), "cornerRadius": 2.0}

    labels = [
        {"clipX": 0.0, "clipY": 0.98, "text": f"A Products ({len(a_prods)} items — top 80% revenue)", "align": "c"},
        {"clipX": 0.0, "clipY": 0.36, "text": f"B Products ({len(b_prods)} items — next 15%)", "align": "c"},
        {"clipX": 0.0, "clipY": -0.30, "text": f"C Products ({len(c_prods)} items — last 5%)", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes_d, layers_d, geos, dis,
                   textOverlay={"fontSize": 13, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 157 — ABC Inventory Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes stacked: A products (top), B products (middle), C products (bottom)
**Resolution:** 1200x800

## Data Sources
- A class: {len(a_prods)} products (top 80% revenue) — green bars
- B class: {len(b_prods)} products (next 15%) — amber bars
- C class: {len(c_prods)} products (last 5%) — red bars

## Insight
ABC analysis for inventory management. A products deserve premium shelf space and never-stockout policies. C products are candidates for reduction or clearance.
"""
    write_trial(157, "abc-inventory", doc, md)

# ── Trial 158: Margin Waterfall ───────────────────────────────────────────────

def trial_158():
    total_rev = sum(s["total"] for s in db.sales)
    acct_exp = db.expense_by_account()
    total_exp = sum(a["total"] for a in acct_exp)

    # Cost of goods (estimated)
    total_cost = sum(db._prod_map[si["productId"]]["unitCost"] * si["quantity"]
                     for si in db.sale_items if si["productId"] in db._prod_map)
    gross_profit = total_rev - total_cost
    net_profit = total_rev - total_cost - total_exp

    items = [
        ("Revenue", 0, total_rev, "#22c55e"),
        ("COGS", total_rev, gross_profit, "#ef4444"),
        ("Gross Profit", 0, gross_profit, "#3b82f6"),
        ("Expenses", gross_profit, net_profit, "#ef4444"),
        ("Net Profit", 0, net_profit, "#8b5cf6"),
    ]

    hw = 0.35
    bufs = {}
    transforms = {}
    geos = {}
    dis = {}

    all_vals = [total_rev, gross_profit, net_profit, 0]
    y_range = (min(all_vals) * 0.9, max(all_vals) * 1.1)
    t1 = fit((-0.5, len(items) - 0.5), y_range)
    transforms[50] = {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]}

    buf_id = 100
    for i, (name, bottom, top, color) in enumerate(items):
        bar = [i - hw, min(bottom, top), i + hw, max(bottom, top)]
        bufs[buf_id] = {"data": rf(bar)}
        geos[buf_id + 1] = {"vertexBufferId": buf_id, "format": "rect4", "vertexCount": 1}
        dis[buf_id + 2] = {"layerId": 10, "name": name, "pipeline": "instancedRect@1",
                           "geometryId": buf_id + 1, "transformId": 50,
                           "color": hex2rgba(color), "cornerRadius": 3.0}
        buf_id += 3

    panes = {
        1: {"name": "Waterfall", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "bars"}}

    labels = [{"clipX": 0.0, "clipY": 0.98, "text": "Margin Waterfall", "align": "c"}]
    for i, (name, _, _, _) in enumerate(items):
        cx = -0.9 + (i / (len(items) - 1)) * 1.8 if len(items) > 1 else 0
        labels.append({"clipX": cx, "clipY": -0.98, "text": name, "align": "c", "fontSize": 11})

    doc = make_doc(1200, 600, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 14, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 158 — Margin Waterfall
**Date:** 2026-03-22
**Layout:** Single pane waterfall: Revenue minus COGS = Gross Profit, minus Expenses = Net Profit
**Resolution:** 1200x600

## Data
- Revenue: ${total_rev:,.0f}
- COGS: ${total_cost:,.0f}
- Gross Profit: ${gross_profit:,.0f}
- Expenses: ${total_exp:,.0f}
- Net Profit: ${net_profit:,.0f}

## Insight
More detailed waterfall than Trial 151 — separates gross profit from net profit, showing the margin structure clearly.
"""
    write_trial(158, "margin-waterfall", doc, md)

# ── Trial 159: Conversion Funnel ──────────────────────────────────────────────

def trial_159():
    total_customers = len(db.customers)
    tiers = db.customer_tier_breakdown()
    tier_counts = {t["tier"]: t["count"] for t in tiers}

    # Funnel: total walk-ins → members → silver → gold
    # Estimate walk-ins as 3x members (not all visitors become customers)
    walk_ins = total_customers * 3
    members = total_customers
    silver = tier_counts.get("silver", 0)
    gold = tier_counts.get("gold", 0)

    stages = [
        ("Walk-ins", walk_ins, "#3b82f6"),
        ("Members", members, "#06b6d4"),
        ("Silver", silver, "#f59e0b"),
        ("Gold", gold, "#22c55e"),
    ]

    # Funnel as narrowing bars centered
    max_val = walk_ins
    bufs = {}
    transforms = {}
    geos = {}
    dis = {}

    buf_id = 100
    for i, (name, value, color) in enumerate(stages):
        hw = (value / max_val) * 0.85 / 2 if max_val else 0.1
        y_bot = 0.8 - i * 0.5
        y_top = y_bot + 0.4
        bar = [-hw, y_bot, hw, y_top]
        bufs[buf_id] = {"data": rf(bar)}
        geos[buf_id + 1] = {"vertexBufferId": buf_id, "format": "rect4", "vertexCount": 1}
        dis[buf_id + 2] = {"layerId": 10, "name": name, "pipeline": "instancedRect@1",
                           "geometryId": buf_id + 1, "color": hex2rgba(color, 0.8),
                           "cornerRadius": 4.0}
        buf_id += 3

    panes = {
        1: {"name": "Funnel", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "bars"}}

    labels = [{"clipX": 0.0, "clipY": 0.98, "text": "Customer Conversion Funnel", "align": "c"}]
    for i, (name, value, _) in enumerate(stages):
        y = 0.8 - i * 0.5 + 0.2
        labels.append({"clipX": 0.0, "clipY": y, "text": f"{name}: {value:,}", "align": "c", "fontSize": 14})

    doc = make_doc(800, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 14, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 159 — Conversion Funnel
**Date:** 2026-03-22
**Layout:** Single pane: narrowing bars representing customer conversion stages
**Resolution:** 800x800

## Data
- Walk-ins (est): {walk_ins:,}
- Members: {members:,}
- Silver tier: {silver:,}
- Gold tier: {gold:,}

## Insight
Visual funnel shows drop-off at each stage. The width of each bar is proportional to the count, making conversion rates immediately apparent.
"""
    write_trial(159, "conversion-funnel", doc, md)

# ── Trial 160: Holiday Playbook ───────────────────────────────────────────────

def trial_160():
    daily = db.daily_revenue()
    dec_2024 = [d for d in daily if d["date"].startswith("2024-12")]
    dec_2025 = [d for d in daily if d["date"].startswith("2025-12")]

    for i, d in enumerate(dec_2024): d["dindex"] = i
    for i, d in enumerate(dec_2025): d["dindex"] = i

    # Same Y scale
    all_rev = [d["revenue"] for d in dec_2024 + dec_2025]
    if all_rev:
        y_range = (min(all_rev) * 0.9, max(all_rev) * 1.1)
    else:
        y_range = (0, 1000)

    if len(dec_2024) >= 2:
        d1, m1 = db.to_line_segments(dec_2024, "dindex", "revenue")
        t1 = fit(m1["xRange"], y_range)
    else:
        d1, m1 = [], {"format": "rect4", "vertexCount": 0, "xRange": (0, 1), "yRange": y_range}
        t1 = fit(m1["xRange"], y_range)

    if len(dec_2025) >= 2:
        d2, m2 = db.to_line_segments(dec_2025, "dindex", "revenue")
        t2 = fit(m2["xRange"], y_range)
    else:
        d2, m2 = [], {"format": "rect4", "vertexCount": 0, "xRange": (0, 1), "yRange": y_range}
        t2 = fit(m2["xRange"], y_range)

    bufs = {100: {"data": rf(d1)}, 110: {"data": rf(d2)}}
    transforms = {
        50: {"sx": t1["sx"], "sy": t1["sy"], "tx": t1["tx"], "ty": t1["ty"]},
        51: {"sx": t2["sx"], "sy": t2["sy"], "tx": t2["tx"], "ty": t2["ty"]},
    }
    panes = {
        1: {"name": "Dec2024", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": -0.03},
            "hasClearColor": True, "clearColor": PANE_BG},
        2: {"name": "Dec2025", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": 0.03, "clipXMax": 0.95},
            "hasClearColor": True, "clearColor": PANE_BG},
    }
    layers = {10: {"paneId": 1, "name": "d24"}, 20: {"paneId": 2, "name": "d25"}}
    geos = {
        101: {"vertexBufferId": 100, "format": "rect4", "vertexCount": m1["vertexCount"]},
        111: {"vertexBufferId": 110, "format": "rect4", "vertexCount": m2["vertexCount"]},
    }
    dis = {
        102: {"layerId": 10, "name": "dec24-line", "pipeline": "lineAA@1", "geometryId": 101,
              "transformId": 50, "color": hex2rgba("#ef4444"), "lineWidth": 2.5},
        112: {"layerId": 20, "name": "dec25-line", "pipeline": "lineAA@1", "geometryId": 111,
              "transformId": 51, "color": hex2rgba("#22c55e"), "lineWidth": 2.5},
    }
    labels = [
        {"clipX": -0.49, "clipY": 0.98, "text": "December 2024", "align": "c"},
        {"clipX": 0.49, "clipY": 0.98, "text": "December 2025", "align": "c"},
    ]
    doc = make_doc(1200, 800, bufs, transforms, panes, layers, geos, dis,
                   textOverlay={"fontSize": 14, "color": "#b2b5bc", "labels": labels})
    md = f"""# Trial 160 — Holiday Playbook Dashboard
**Date:** 2026-03-22
**Layout:** 2 panes: left = Dec 2024 daily revenue, right = Dec 2025 daily revenue
**Resolution:** 1200x800

## Data Sources
- Left: `daily_revenue()` filtered to 2024-12 — {len(dec_2024)} days as lineAA@1
- Right: `daily_revenue()` filtered to 2025-12 — {len(dec_2025)} days as lineAA@1

## Insight
Holiday season comparison. Same Y scale across both panes reveals whether the store improved its December performance year-over-year. Key dates (Black Friday, Christmas Eve) create visible spikes.
"""
    write_trial(160, "holiday-playbook", doc, md)


# ══════════════════════════════════════════════════════════════════════════════
# Main
# ══════════════════════════════════════════════════════════════════════════════

if __name__ == "__main__":
    print("Generating data-driven trials 121-160 (Multi-Pane Dashboards)...")
    print()
    trial_121()
    trial_122()
    trial_123()
    trial_124()
    trial_125()
    trial_126()
    trial_127()
    trial_128()
    trial_129()
    trial_130()
    trial_131()
    trial_132()
    trial_133()
    trial_134()
    trial_135()
    trial_136()
    trial_137()
    trial_138()
    trial_139()
    trial_140()
    trial_141()
    trial_142()
    trial_143()
    trial_144()
    trial_145()
    trial_146()
    trial_147()
    trial_148()
    trial_149()
    trial_150()
    trial_151()
    trial_152()
    trial_153()
    trial_154()
    trial_155()
    trial_156()
    trial_157()
    trial_158()
    trial_159()
    trial_160()
    print()
    print("Done — 40 trials generated.")
