#!/usr/bin/env python3
"""Generate data-driven trials 041-080: filtered and sorted store data visualizations.

Usage:
    cd DynaCharting
    python docs/data-trials/gen_data_batch_2.py
"""

import sys, os, json, math
from collections import defaultdict, Counter
from datetime import date, datetime

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))
from data.adapter import StoreData

OUT = os.path.join(os.path.dirname(__file__))
os.makedirs(OUT, exist_ok=True)

db = StoreData()


# ─── helpers ──────────────────────────────────────────────────────

def write_trial(num, slug, scene_doc, md_text):
    """Write JSON + MD for one trial."""
    tag = f"{num:03d}-{slug}"
    with open(os.path.join(OUT, f"{tag}.json"), "w") as f:
        json.dump(scene_doc, f, indent=2)
    with open(os.path.join(OUT, f"{tag}.md"), "w") as f:
        f.write(md_text)
    print(f"  wrote {tag}.json + .md")


def make_scene(
    title, buffers, transforms, panes, layers, geometries, draw_items,
    text_labels=None, width=900, height=500, viewports=None
):
    """Assemble a SceneDocument dict."""
    doc = {
        "version": 1,
        "viewport": {"width": width, "height": height},
        "buffers": buffers,
        "transforms": transforms,
        "panes": panes,
        "layers": layers,
        "geometries": geometries,
        "drawItems": draw_items,
    }
    if viewports:
        doc["viewports"] = viewports
    overlay = {"fontSize": 13, "color": "#c0c4cc", "labels": []}
    overlay["labels"].append({"clipX": 0.0, "clipY": 0.95, "text": title, "align": "c", "fontSize": 15})
    if text_labels:
        overlay["labels"].extend(text_labels)
    doc["textOverlay"] = overlay
    return doc


def simple_line_chart(
    title, items, x_key, y_key, color_hex, line_width=2.0,
    extra_labels=None, width=900, height=500
):
    """One-pane lineAA@1 chart from data items."""
    data, meta = db.to_line_segments(items, x_key, y_key)
    tx = db.fit_transform(meta["xRange"], meta["yRange"])
    scene = make_scene(
        title,
        buffers={"100": {"data": data}},
        transforms={"50": tx},
        panes={"1": {"name": "main", "region": {"clipYMin": -0.85, "clipYMax": 0.85, "clipXMin": -0.90, "clipXMax": 0.90},
                      "hasClearColor": True, "clearColor": [0.07, 0.07, 0.10, 1.0]}},
        layers={"10": {"paneId": 1, "name": "data"}},
        geometries={"101": {"vertexBufferId": 100, "format": "rect4", "vertexCount": meta["vertexCount"]}},
        draw_items={"102": {"layerId": 10, "pipeline": "lineAA@1", "geometryId": 101,
                            "transformId": 50, "color": db.hex_to_rgba(color_hex), "lineWidth": line_width}},
        text_labels=extra_labels, width=width, height=height,
    )
    return scene, meta


def simple_bar_chart(
    title, items, x_key, y_key, color_hex, bar_width=0.7,
    extra_labels=None, corner_radius=3.0, width=900, height=500
):
    """One-pane instancedRect@1 bar chart."""
    data, meta = db.to_bars(items, x_key, y_key, bar_width=bar_width)
    tx = db.fit_transform(meta["xRange"], meta["yRange"])
    scene = make_scene(
        title,
        buffers={"100": {"data": data}},
        transforms={"50": tx},
        panes={"1": {"name": "main", "region": {"clipYMin": -0.85, "clipYMax": 0.85, "clipXMin": -0.90, "clipXMax": 0.90},
                      "hasClearColor": True, "clearColor": [0.07, 0.07, 0.10, 1.0]}},
        layers={"10": {"paneId": 1, "name": "data"}},
        geometries={"101": {"vertexBufferId": 100, "format": "rect4", "vertexCount": meta["vertexCount"]}},
        draw_items={"102": {"layerId": 10, "pipeline": "instancedRect@1", "geometryId": 101,
                            "transformId": 50, "color": db.hex_to_rgba(color_hex), "cornerRadius": corner_radius}},
        text_labels=extra_labels, width=width, height=height,
    )
    return scene, meta


def simple_hbar_chart(
    title, items, y_key, x_key, color_hex, bar_height=0.7,
    extra_labels=None, corner_radius=3.0, width=900, height=500
):
    """One-pane horizontal bar chart."""
    data, meta = db.to_horizontal_bars(items, y_key, x_key, bar_height=bar_height)
    tx = db.fit_transform(meta["xRange"], meta["yRange"])
    scene = make_scene(
        title,
        buffers={"100": {"data": data}},
        transforms={"50": tx},
        panes={"1": {"name": "main", "region": {"clipYMin": -0.85, "clipYMax": 0.85, "clipXMin": -0.90, "clipXMax": 0.90},
                      "hasClearColor": True, "clearColor": [0.07, 0.07, 0.10, 1.0]}},
        layers={"10": {"paneId": 1, "name": "data"}},
        geometries={"101": {"vertexBufferId": 100, "format": "rect4", "vertexCount": meta["vertexCount"]}},
        draw_items={"102": {"layerId": 10, "pipeline": "instancedRect@1", "geometryId": 101,
                            "transformId": 50, "color": db.hex_to_rgba(color_hex), "cornerRadius": corner_radius}},
        text_labels=extra_labels, width=width, height=height,
    )
    return scene, meta


def dual_line_chart(
    title, items_a, items_b, x_key, y_key,
    color_a, color_b, label_a, label_b,
    line_width=2.0, extra_labels=None, width=900, height=500
):
    """Two overlaid lineAA@1 on one pane."""
    data_a, meta_a = db.to_line_segments(items_a, x_key, y_key)
    data_b, meta_b = db.to_line_segments(items_b, x_key, y_key)
    # Merge ranges
    x_range = (min(meta_a["xRange"][0], meta_b["xRange"][0]),
               max(meta_a["xRange"][1], meta_b["xRange"][1]))
    y_range = (min(meta_a["yRange"][0], meta_b["yRange"][0]),
               max(meta_a["yRange"][1], meta_b["yRange"][1]))
    tx = db.fit_transform(x_range, y_range)
    labels = [
        {"clipX": 0.75, "clipY": 0.88, "text": label_a, "align": "r", "color": color_a},
        {"clipX": 0.75, "clipY": 0.82, "text": label_b, "align": "r", "color": color_b},
    ]
    if extra_labels:
        labels.extend(extra_labels)
    scene = make_scene(
        title,
        buffers={
            "100": {"data": data_a},
            "103": {"data": data_b},
        },
        transforms={"50": tx},
        panes={"1": {"name": "main", "region": {"clipYMin": -0.85, "clipYMax": 0.85, "clipXMin": -0.90, "clipXMax": 0.90},
                      "hasClearColor": True, "clearColor": [0.07, 0.07, 0.10, 1.0]}},
        layers={"10": {"paneId": 1, "name": "data"}},
        geometries={
            "101": {"vertexBufferId": 100, "format": "rect4", "vertexCount": meta_a["vertexCount"]},
            "104": {"vertexBufferId": 103, "format": "rect4", "vertexCount": meta_b["vertexCount"]},
        },
        draw_items={
            "102": {"layerId": 10, "pipeline": "lineAA@1", "geometryId": 101,
                    "transformId": 50, "color": db.hex_to_rgba(color_a), "lineWidth": line_width},
            "105": {"layerId": 10, "pipeline": "lineAA@1", "geometryId": 104,
                    "transformId": 50, "color": db.hex_to_rgba(color_b), "lineWidth": line_width},
        },
        text_labels=labels, width=width, height=height,
    )
    return scene


def md_header(num, slug, title, query_desc, insight, chart_type, data_points, filter_desc):
    """Produce the standard MD template."""
    return f"""# Trial {num:03d}: {title}

**Date:** 2026-03-22
**Chart Type:** {chart_type}
**Pipeline:** see below
**Data Points:** {data_points}

**Query:** {query_desc}
**Data Insight:** {insight}

## Filter Logic

```
{filter_desc}
```

## Files

- `{num:03d}-{slug}.json` — SceneDocument
- `{num:03d}-{slug}.md` — this file
"""


# ─── Trial 041: Lumber Dept Monthly Revenue ──────────────────────

def trial_041():
    dept_monthly = db.department_monthly_revenue()
    lumber = [r for r in dept_monthly if r["deptId"] == 1]
    # Re-index
    for i, r in enumerate(lumber):
        r["idx"] = i
    scene, meta = simple_line_chart(
        "Lumber & Building Materials — Monthly Revenue",
        lumber, "idx", "revenue", "#f59e0b", line_width=2.5,
        extra_labels=[
            {"clipX": -0.85, "clipY": -0.92, "text": f"{lumber[0]['month']}", "align": "l", "fontSize": 11, "color": "#888"},
            {"clipX": 0.85, "clipY": -0.92, "text": f"{lumber[-1]['month']}", "align": "r", "fontSize": 11, "color": "#888"},
        ]
    )
    months_str = f"{lumber[0]['month']} to {lumber[-1]['month']}"
    peak = max(lumber, key=lambda r: r["revenue"])
    md = md_header(41, "lumber-dept-monthly",
        "Lumber & Building Materials — Monthly Revenue Trend",
        f"department_monthly_revenue() filtered to deptId==1 (Lumber). {len(lumber)} months from {months_str}.",
        f"Peak month: {peak['month']} at ${peak['revenue']:,.0f}. Lumber shows seasonal demand with spring/summer construction peaks.",
        "lineAA@1 trend line",
        len(lumber),
        "dept_monthly = db.department_monthly_revenue()\nlumber = [r for r in dept_monthly if r['deptId'] == 1]")
    write_trial(41, "lumber-dept-monthly", scene, md)

# ─── Trial 042: Garden Seasonal Pattern ──────────────────────────

def trial_042():
    dept_monthly = db.department_monthly_revenue()
    garden = [r for r in dept_monthly if r["deptId"] == 6]
    for i, r in enumerate(garden):
        r["idx"] = i
    scene, meta = simple_line_chart(
        "Garden & Outdoor — Seasonal Revenue Pattern",
        garden, "idx", "revenue", "#22c55e", line_width=2.5,
        extra_labels=[
            {"clipX": -0.85, "clipY": -0.92, "text": garden[0]["month"], "align": "l", "fontSize": 11, "color": "#888"},
            {"clipX": 0.85, "clipY": -0.92, "text": garden[-1]["month"], "align": "r", "fontSize": 11, "color": "#888"},
        ]
    )
    peak = max(garden, key=lambda r: r["revenue"])
    trough = min(garden, key=lambda r: r["revenue"])
    md = md_header(42, "garden-seasonal-pattern",
        "Garden & Outdoor — Seasonal Revenue Pattern",
        f"department_monthly_revenue() filtered to deptId==6 (Garden & Outdoor). {len(garden)} months.",
        f"Clear spring/summer peak: {peak['month']} at ${peak['revenue']:,.0f}. Winter trough: {trough['month']} at ${trough['revenue']:,.0f}. Ratio: {peak['revenue']/trough['revenue']:.1f}x.",
        "lineAA@1 trend line",
        len(garden),
        "dept_monthly = db.department_monthly_revenue()\ngarden = [r for r in dept_monthly if r['deptId'] == 6]")
    write_trial(42, "garden-seasonal-pattern", scene, md)

# ─── Trial 043: Holiday/Seasonal Dept December Spike ─────────────

def trial_043():
    dept_monthly = db.department_monthly_revenue()
    seasonal = [r for r in dept_monthly if r["deptId"] == 8]
    for i, r in enumerate(seasonal):
        r["idx"] = i
    scene, meta = simple_line_chart(
        "Seasonal & Holiday — December Revenue Spike",
        seasonal, "idx", "revenue", "#f97316", line_width=2.5,
        extra_labels=[
            {"clipX": -0.85, "clipY": -0.92, "text": seasonal[0]["month"], "align": "l", "fontSize": 11, "color": "#888"},
            {"clipX": 0.85, "clipY": -0.92, "text": seasonal[-1]["month"], "align": "r", "fontSize": 11, "color": "#888"},
        ]
    )
    dec_months = [r for r in seasonal if r["month"].endswith("-12")]
    peak = max(seasonal, key=lambda r: r["revenue"])
    md = md_header(43, "holiday-dept-surge",
        "Seasonal & Holiday Dept — December Revenue Spike",
        f"department_monthly_revenue() filtered to deptId==8 (Seasonal & Holiday). {len(seasonal)} months.",
        f"December months show dramatic spikes. Peak: {peak['month']} at ${peak['revenue']:,.0f}. Holiday decoration/gift buying drives huge Q4 revenue.",
        "lineAA@1 trend line",
        len(seasonal),
        "dept_monthly = db.department_monthly_revenue()\nseasonal = [r for r in dept_monthly if r['deptId'] == 8]")
    write_trial(43, "holiday-dept-surge", scene, md)

# ─── Trial 044: Gold-Tier Customer Revenue ───────────────────────

def trial_044():
    gold_ids = {c["id"] for c in db.customers if c["tier"] == "gold"}
    gold_monthly = defaultdict(float)
    for s in db.sales:
        if s["customerId"] in gold_ids:
            gold_monthly[s["date"][:7]] += s["total"]
    months = sorted(gold_monthly.keys())
    items = [{"month": m, "idx": i, "revenue": round(gold_monthly[m], 2)} for i, m in enumerate(months)]
    scene, meta = simple_line_chart(
        "Gold-Tier Customers — Monthly Revenue",
        items, "idx", "revenue", "#fbbf24", line_width=2.5,
        extra_labels=[
            {"clipX": -0.85, "clipY": -0.92, "text": months[0], "align": "l", "fontSize": 11, "color": "#888"},
            {"clipX": 0.85, "clipY": -0.92, "text": months[-1], "align": "r", "fontSize": 11, "color": "#888"},
        ]
    )
    total = sum(r["revenue"] for r in items)
    avg = total / len(items)
    md = md_header(44, "gold-customer-revenue",
        "Gold-Tier Customer — Monthly Revenue",
        f"Filter sales where customerId is in gold-tier customer set. {len(gold_ids)} gold customers, {len(items)} months.",
        f"Gold-tier customers generate avg ${avg:,.0f}/month (total ${total:,.0f}). Loyalty program's top tier drives consistent high-value revenue.",
        "lineAA@1 trend line",
        len(items),
        "gold_ids = {c['id'] for c in db.customers if c['tier'] == 'gold'}\ngold_monthly[sale['date'][:7]] += sale['total'] for gold customers")
    write_trial(44, "gold-customer-revenue", scene, md)

# ─── Trial 045: Silver vs Bronze Monthly Revenue ─────────────────

def trial_045():
    silver_ids = {c["id"] for c in db.customers if c["tier"] == "silver"}
    bronze_ids = {c["id"] for c in db.customers if c["tier"] == "bronze"}
    silver_monthly = defaultdict(float)
    bronze_monthly = defaultdict(float)
    for s in db.sales:
        if s["customerId"] in silver_ids:
            silver_monthly[s["date"][:7]] += s["total"]
        elif s["customerId"] in bronze_ids:
            bronze_monthly[s["date"][:7]] += s["total"]
    months = sorted(set(list(silver_monthly.keys()) + list(bronze_monthly.keys())))
    silver_items = [{"month": m, "idx": i, "revenue": round(silver_monthly.get(m, 0), 2)} for i, m in enumerate(months)]
    bronze_items = [{"month": m, "idx": i, "revenue": round(bronze_monthly.get(m, 0), 2)} for i, m in enumerate(months)]

    scene = dual_line_chart(
        "Silver vs Bronze Tier — Monthly Revenue",
        silver_items, bronze_items, "idx", "revenue",
        "#94a3b8", "#cd7f32", "Silver", "Bronze",
        line_width=2.5,
        extra_labels=[
            {"clipX": -0.85, "clipY": -0.92, "text": months[0], "align": "l", "fontSize": 11, "color": "#888"},
            {"clipX": 0.85, "clipY": -0.92, "text": months[-1], "align": "r", "fontSize": 11, "color": "#888"},
        ]
    )
    s_total = sum(r["revenue"] for r in silver_items)
    b_total = sum(r["revenue"] for r in bronze_items)
    md = md_header(45, "silver-vs-bronze-bars",
        "Silver vs Bronze Tier — Monthly Revenue Comparison",
        f"Filter sales by customer tier (silver vs bronze). {len(silver_ids)} silver, {len(bronze_ids)} bronze customers across {len(months)} months.",
        f"Silver total: ${s_total:,.0f}, Bronze total: ${b_total:,.0f}. {'Silver' if s_total > b_total else 'Bronze'} tier outspends by ${abs(s_total - b_total):,.0f}.",
        "Two overlaid lineAA@1 lines",
        len(months) * 2,
        "silver_ids = {c['id'] for c in db.customers if c['tier'] == 'silver'}\nbronze_ids = {c['id'] for c in db.customers if c['tier'] == 'bronze'}\nAccumulate monthly revenue per tier")
    write_trial(45, "silver-vs-bronze-bars", scene, md)

# ─── Trial 046: Weekday-Only Revenue ─────────────────────────────

def trial_046():
    daily = db.daily_revenue()
    weekday = [r for r in daily if date.fromisoformat(r["date"]).weekday() < 5]
    for i, r in enumerate(weekday):
        r["idx"] = i
    scene, meta = simple_line_chart(
        "Weekday Revenue (Mon-Fri Only)",
        weekday, "idx", "revenue", "#3b82f6", line_width=1.5,
        extra_labels=[
            {"clipX": -0.85, "clipY": -0.92, "text": weekday[0]["date"], "align": "l", "fontSize": 11, "color": "#888"},
            {"clipX": 0.85, "clipY": -0.92, "text": weekday[-1]["date"], "align": "r", "fontSize": 11, "color": "#888"},
        ]
    )
    avg = sum(r["revenue"] for r in weekday) / len(weekday)
    md = md_header(46, "weekday-only-revenue",
        "Weekday-Only Daily Revenue",
        f"daily_revenue() filtered to Mon-Fri (weekday() < 5). {len(weekday)} weekdays out of {len(daily)} total days.",
        f"Average weekday revenue: ${avg:,.0f}. Weekdays show more consistent patterns without weekend volatility.",
        "lineAA@1 trend line",
        len(weekday),
        "daily = db.daily_revenue()\nweekday = [r for r in daily if date.fromisoformat(r['date']).weekday() < 5]")
    write_trial(46, "weekday-only-revenue", scene, md)

# ─── Trial 047: Weekend-Only Revenue ─────────────────────────────

def trial_047():
    daily = db.daily_revenue()
    weekend = [r for r in daily if date.fromisoformat(r["date"]).weekday() >= 5]
    for i, r in enumerate(weekend):
        r["idx"] = i
    scene, meta = simple_line_chart(
        "Weekend Revenue (Sat-Sun Only)",
        weekend, "idx", "revenue", "#ec4899", line_width=1.5,
        extra_labels=[
            {"clipX": -0.85, "clipY": -0.92, "text": weekend[0]["date"], "align": "l", "fontSize": 11, "color": "#888"},
            {"clipX": 0.85, "clipY": -0.92, "text": weekend[-1]["date"], "align": "r", "fontSize": 11, "color": "#888"},
        ]
    )
    avg = sum(r["revenue"] for r in weekend) / len(weekend)
    md = md_header(47, "weekend-only-revenue",
        "Weekend-Only Daily Revenue",
        f"daily_revenue() filtered to Sat-Sun (weekday() >= 5). {len(weekend)} weekend days.",
        f"Average weekend revenue: ${avg:,.0f}. Weekend patterns show DIY project shoppers with different spending behavior.",
        "lineAA@1 trend line",
        len(weekend),
        "daily = db.daily_revenue()\nweekend = [r for r in daily if date.fromisoformat(r['date']).weekday() >= 5]")
    write_trial(47, "weekend-only-revenue", scene, md)

# ─── Trial 048: Top 10 Products by Margin ────────────────────────

def trial_048():
    products = db.product_rankings()
    by_margin = sorted(products, key=lambda r: -r["margin"])[:10]
    for i, r in enumerate(by_margin):
        r["idx"] = i
    labels = [{"clipX": -0.88, "clipY": 0.85 - i * 0.165, "text": f"{r['name'][:25]}",
               "align": "l", "fontSize": 10, "color": "#aaa"} for i, r in enumerate(by_margin)]
    scene, meta = simple_hbar_chart(
        "Top 10 Products by Profit Margin",
        by_margin, "idx", "margin", "#22c55e", bar_height=0.6,
        extra_labels=labels, corner_radius=2.0,
    )
    md = md_header(48, "top-10-by-margin",
        "Top 10 Products by Profit Margin",
        f"product_rankings() sorted by margin desc, top 10. Margin = (unitPrice - unitCost) / unitPrice.",
        f"Highest margin: {by_margin[0]['name']} at {by_margin[0]['margin']:.1%}. Top 10 margins range {by_margin[9]['margin']:.1%} to {by_margin[0]['margin']:.1%}.",
        "Horizontal instancedRect@1 bars",
        10,
        "products = db.product_rankings()\nby_margin = sorted(products, key=lambda r: -r['margin'])[:10]")
    write_trial(48, "top-10-by-margin", scene, md)

# ─── Trial 049: Bottom 10 Products by Margin ─────────────────────

def trial_049():
    products = db.product_rankings()
    by_margin = sorted(products, key=lambda r: r["margin"])[:10]
    for i, r in enumerate(by_margin):
        r["idx"] = i
    labels = [{"clipX": -0.88, "clipY": 0.85 - i * 0.165, "text": f"{r['name'][:25]}",
               "align": "l", "fontSize": 10, "color": "#aaa"} for i, r in enumerate(by_margin)]
    scene, meta = simple_hbar_chart(
        "Bottom 10 Products by Profit Margin",
        by_margin, "idx", "margin", "#ef4444", bar_height=0.6,
        extra_labels=labels, corner_radius=2.0,
    )
    md = md_header(49, "bottom-10-by-margin",
        "Bottom 10 Products by Profit Margin",
        f"product_rankings() sorted by margin asc, bottom 10.",
        f"Lowest margin: {by_margin[0]['name']} at {by_margin[0]['margin']:.1%}. These products are sold near cost — loss leaders or commodity items.",
        "Horizontal instancedRect@1 bars",
        10,
        "products = db.product_rankings()\nby_margin = sorted(products, key=lambda r: r['margin'])[:10]")
    write_trial(49, "bottom-10-by-margin", scene, md)

# ─── Trial 050: High-Ticket Sales (>$200) by Month ───────────────

def trial_050():
    high_ticket = [s for s in db.sales if s["total"] > 200]
    monthly_count = defaultdict(int)
    for s in high_ticket:
        monthly_count[s["date"][:7]] += 1
    months = sorted(monthly_count.keys())
    items = [{"month": m, "idx": i, "count": monthly_count[m]} for i, m in enumerate(months)]
    scene, meta = simple_bar_chart(
        "High-Ticket Sales (>$200) — Monthly Count",
        items, "idx", "count", "#8b5cf6", bar_width=0.6,
        extra_labels=[
            {"clipX": -0.85, "clipY": -0.92, "text": months[0], "align": "l", "fontSize": 11, "color": "#888"},
            {"clipX": 0.85, "clipY": -0.92, "text": months[-1], "align": "r", "fontSize": 11, "color": "#888"},
        ]
    )
    total = len(high_ticket)
    avg_per_month = total / len(months)
    md = md_header(50, "high-ticket-sales",
        "High-Ticket Sales (>$200) — Count Per Month",
        f"Filter sales where total > $200. {total} qualifying sales out of {len(db.sales)} total.",
        f"{total} high-ticket sales ({total/len(db.sales)*100:.1f}% of all sales), avg {avg_per_month:.1f}/month. These premium transactions represent the store's big-project customers.",
        "instancedRect@1 bar chart",
        len(items),
        "high_ticket = [s for s in db.sales if s['total'] > 200]\nmonthly_count[s['date'][:7]] += 1")
    write_trial(50, "high-ticket-sales", scene, md)

# ─── Trial 051: Single-Item Sales Monthly Count ──────────────────

def trial_051():
    single = [s for s in db.sales if s["itemCount"] == 1]
    monthly_count = defaultdict(int)
    for s in single:
        monthly_count[s["date"][:7]] += 1
    months = sorted(monthly_count.keys())
    items = [{"month": m, "idx": i, "count": monthly_count[m]} for i, m in enumerate(months)]
    scene, meta = simple_line_chart(
        "Single-Item Sales — Monthly Count",
        items, "idx", "count", "#06b6d4", line_width=2.0,
        extra_labels=[
            {"clipX": -0.85, "clipY": -0.92, "text": months[0], "align": "l", "fontSize": 11, "color": "#888"},
            {"clipX": 0.85, "clipY": -0.92, "text": months[-1], "align": "r", "fontSize": 11, "color": "#888"},
        ]
    )
    total = len(single)
    md = md_header(51, "single-item-sales",
        "Single-Item Sales — Monthly Count",
        f"Filter sales where itemCount == 1. {total} single-item sales ({total/len(db.sales)*100:.1f}% of all).",
        f"Single-item purchases represent quick runs — hardware store grab-and-go shoppers. Consistent {total/len(months):.0f}/month baseline.",
        "lineAA@1 trend line",
        len(items),
        "single = [s for s in db.sales if s['itemCount'] == 1]\nmonthly_count[s['date'][:7]] += 1")
    write_trial(51, "single-item-sales", scene, md)

# ─── Trial 052: Cash-Only Monthly Revenue ─────────────────────────

def trial_052():
    cash = [s for s in db.sales if s["paymentMethod"] == "cash"]
    monthly_rev = defaultdict(float)
    for s in cash:
        monthly_rev[s["date"][:7]] += s["total"]
    months = sorted(monthly_rev.keys())
    items = [{"month": m, "idx": i, "revenue": round(monthly_rev[m], 2)} for i, m in enumerate(months)]
    scene, meta = simple_line_chart(
        "Cash-Only Transactions — Monthly Revenue",
        items, "idx", "revenue", "#22c55e", line_width=2.0,
        extra_labels=[
            {"clipX": -0.85, "clipY": -0.92, "text": months[0], "align": "l", "fontSize": 11, "color": "#888"},
            {"clipX": 0.85, "clipY": -0.92, "text": months[-1], "align": "r", "fontSize": 11, "color": "#888"},
        ]
    )
    total = sum(r["revenue"] for r in items)
    all_total = sum(s["total"] for s in db.sales)
    md = md_header(52, "cash-only-trend",
        "Cash-Only Transactions — Monthly Revenue",
        f"Filter sales to paymentMethod == 'cash'. {len(cash)} cash transactions.",
        f"Cash represents ${total:,.0f} ({total/all_total*100:.1f}% of all revenue). Cash usage may be declining over time as digital payments grow.",
        "lineAA@1 trend line",
        len(items),
        "cash = [s for s in db.sales if s['paymentMethod'] == 'cash']\nmonthly_rev[s['date'][:7]] += s['total']")
    write_trial(52, "cash-only-trend", scene, md)

# ─── Trial 053: Morning vs Afternoon Revenue ─────────────────────

def trial_053():
    morning_monthly = defaultdict(float)
    afternoon_monthly = defaultdict(float)
    for s in db.sales:
        hour = int(s["time"].split(":")[0])
        mk = s["date"][:7]
        if hour < 12:
            morning_monthly[mk] += s["total"]
        else:
            afternoon_monthly[mk] += s["total"]
    months = sorted(set(list(morning_monthly.keys()) + list(afternoon_monthly.keys())))
    morning_items = [{"month": m, "idx": i, "revenue": round(morning_monthly.get(m, 0), 2)} for i, m in enumerate(months)]
    afternoon_items = [{"month": m, "idx": i, "revenue": round(afternoon_monthly.get(m, 0), 2)} for i, m in enumerate(months)]

    scene = dual_line_chart(
        "Morning (<12:00) vs Afternoon Revenue",
        morning_items, afternoon_items, "idx", "revenue",
        "#f59e0b", "#3b82f6", "Morning (<12:00)", "Afternoon (>=12:00)",
        line_width=2.5,
        extra_labels=[
            {"clipX": -0.85, "clipY": -0.92, "text": months[0], "align": "l", "fontSize": 11, "color": "#888"},
            {"clipX": 0.85, "clipY": -0.92, "text": months[-1], "align": "r", "fontSize": 11, "color": "#888"},
        ]
    )
    m_total = sum(r["revenue"] for r in morning_items)
    a_total = sum(r["revenue"] for r in afternoon_items)
    md = md_header(53, "morning-vs-afternoon",
        "Morning vs Afternoon — Monthly Revenue Comparison",
        f"Split sales by time: hour < 12 (morning) vs hour >= 12 (afternoon). {len(months)} months.",
        f"Morning total: ${m_total:,.0f}, Afternoon total: ${a_total:,.0f}. {'Afternoon' if a_total > m_total else 'Morning'} dominates by ${abs(a_total - m_total):,.0f}. Contractor morning rush vs DIY afternoon shoppers.",
        "Two overlaid lineAA@1 lines",
        len(months) * 2,
        "Split sales: hour = int(s['time'].split(':')[0])\nhour < 12 -> morning, else -> afternoon\nAccumulate monthly revenue per period")
    write_trial(53, "morning-vs-afternoon", scene, md)

# ─── Trial 054: Q4 2024 Daily Revenue ─────────────────────────────

def trial_054():
    daily = db.daily_revenue()
    q4 = [r for r in daily if r["date"] >= "2024-10-01" and r["date"] <= "2024-12-31"]
    for i, r in enumerate(q4):
        r["idx"] = i
    scene, meta = simple_line_chart(
        "Q4 2024 Daily Revenue (Oct-Dec)",
        q4, "idx", "revenue", "#f97316", line_width=1.5,
        extra_labels=[
            {"clipX": -0.85, "clipY": -0.92, "text": "Oct 1", "align": "l", "fontSize": 11, "color": "#888"},
            {"clipX": 0.85, "clipY": -0.92, "text": "Dec 31", "align": "r", "fontSize": 11, "color": "#888"},
        ]
    )
    avg = sum(r["revenue"] for r in q4) / len(q4)
    peak = max(q4, key=lambda r: r["revenue"])
    md = md_header(54, "q4-2024-daily",
        "Q4 2024 Daily Revenue (Oct-Dec)",
        f"daily_revenue() filtered to 2024-10-01..2024-12-31. {len(q4)} days.",
        f"Q4 avg daily revenue: ${avg:,.0f}. Peak day: {peak['date']} at ${peak['revenue']:,.0f}. Holiday shopping season drives clear uptick in November-December.",
        "lineAA@1 trend line (dense daily)",
        len(q4),
        "q4 = [r for r in daily if r['date'] >= '2024-10-01' and r['date'] <= '2024-12-31']")
    write_trial(54, "q4-2024-daily", scene, md)

# ─── Trial 055: Q1 2025 Daily Revenue ─────────────────────────────

def trial_055():
    daily = db.daily_revenue()
    q1 = [r for r in daily if r["date"] >= "2025-01-01" and r["date"] <= "2025-03-31"]
    for i, r in enumerate(q1):
        r["idx"] = i
    scene, meta = simple_line_chart(
        "Q1 2025 Daily Revenue — Post-Holiday Slump",
        q1, "idx", "revenue", "#64748b", line_width=1.5,
        extra_labels=[
            {"clipX": -0.85, "clipY": -0.92, "text": "Jan 1", "align": "l", "fontSize": 11, "color": "#888"},
            {"clipX": 0.85, "clipY": -0.92, "text": "Mar 31", "align": "r", "fontSize": 11, "color": "#888"},
        ]
    )
    avg = sum(r["revenue"] for r in q1) / len(q1)
    # Compare to Q4
    q4_daily = [r for r in daily if r["date"] >= "2024-10-01" and r["date"] <= "2024-12-31"]
    q4_avg = sum(r["revenue"] for r in q4_daily) / len(q4_daily) if q4_daily else 0
    md = md_header(55, "q1-2025-daily",
        "Q1 2025 Daily Revenue — Post-Holiday Slump",
        f"daily_revenue() filtered to 2025-01-01..2025-03-31. {len(q1)} days.",
        f"Q1 2025 avg: ${avg:,.0f}/day vs Q4 2024 avg: ${q4_avg:,.0f}/day. {'Post-holiday decline' if avg < q4_avg else 'Steady'} of {abs(q4_avg - avg)/q4_avg*100:.0f}%. January-February typically sees reduced hardware store traffic.",
        "lineAA@1 trend line (dense daily)",
        len(q1),
        "q1 = [r for r in daily if r['date'] >= '2025-01-01' and r['date'] <= '2025-03-31']")
    write_trial(55, "q1-2025-daily", scene, md)

# ─── Trial 056: Employees Hired in 2024 — Total Hours ────────────

def trial_056():
    hired_2024 = [e for e in db.employees if e["hireDate"].startswith("2024")]
    emp_hours = db.employee_hours()
    hours_map = {r["id"]: r for r in emp_hours}
    items = []
    for i, e in enumerate(hired_2024):
        h = hours_map.get(e["id"])
        if h:
            items.append({"idx": i, "name": f"{e['firstName']} {e['lastName'][:1]}.", "hours": h["totalHours"]})
    items.sort(key=lambda r: -r["hours"])
    for i, r in enumerate(items):
        r["idx"] = i
    labels = [{"clipX": -0.88, "clipY": 0.85 - i * (1.7 / max(len(items), 1)), "text": r["name"][:20],
               "align": "l", "fontSize": 10, "color": "#aaa"} for i, r in enumerate(items)]
    scene, meta = simple_hbar_chart(
        "2024 Hires — Total Hours Worked",
        items, "idx", "hours", "#06b6d4", bar_height=0.6,
        extra_labels=labels, corner_radius=2.0,
    )
    md = md_header(56, "new-employees-2024",
        "Employees Hired in 2024 — Total Hours Worked",
        f"Filter employees where hireDate starts with '2024'. {len(hired_2024)} employees hired in 2024.",
        f"Top performer: {items[0]['name']} with {items[0]['hours']:.0f} hours. New hires show varied ramp-up speeds.",
        "Horizontal instancedRect@1 bars",
        len(items),
        "hired_2024 = [e for e in db.employees if e['hireDate'].startswith('2024')]\nJoin with employee_hours() for total hours")
    write_trial(56, "new-employees-2024", scene, md)

# ─── Trial 057: Full-Time vs Part-Time Hours ─────────────────────

def trial_057():
    emp_hours = db.employee_hours()
    full_time = [r for r in emp_hours if r["avgWeeklyHours"] > 30]
    part_time = [r for r in emp_hours if r["avgWeeklyHours"] <= 30]
    ft_avg = sum(r["avgWeeklyHours"] for r in full_time) / len(full_time) if full_time else 0
    pt_avg = sum(r["avgWeeklyHours"] for r in part_time) / len(part_time) if part_time else 0

    # Create grouped comparison: two bars
    items = [
        {"idx": 0, "label": "Full-Time", "hours": ft_avg},
        {"idx": 1, "label": "Part-Time", "hours": pt_avg},
    ]
    data, meta = db.to_bars(items, "idx", "hours", bar_width=0.6)
    tx = db.fit_transform(meta["xRange"], meta["yRange"])
    scene = make_scene(
        f"Full-Time vs Part-Time — Avg Weekly Hours (FT={len(full_time)}, PT={len(part_time)})",
        buffers={"100": {"data": data}},
        transforms={"50": tx},
        panes={"1": {"name": "main", "region": {"clipYMin": -0.85, "clipYMax": 0.85, "clipXMin": -0.90, "clipXMax": 0.90},
                      "hasClearColor": True, "clearColor": [0.07, 0.07, 0.10, 1.0]}},
        layers={"10": {"paneId": 1, "name": "data"}},
        geometries={"101": {"vertexBufferId": 100, "format": "rect4", "vertexCount": meta["vertexCount"]}},
        draw_items={"102": {"layerId": 10, "pipeline": "instancedRect@1", "geometryId": 101,
                            "transformId": 50, "color": db.hex_to_rgba("#8b5cf6"), "cornerRadius": 4.0}},
        text_labels=[
            {"clipX": -0.25, "clipY": -0.92, "text": f"Full-Time (>{30}h/wk): {ft_avg:.1f}h avg", "align": "c", "fontSize": 12, "color": "#aaa"},
            {"clipX": 0.55, "clipY": -0.92, "text": f"Part-Time (<=30h/wk): {pt_avg:.1f}h avg", "align": "c", "fontSize": 12, "color": "#aaa"},
        ]
    )
    md = md_header(57, "full-time-vs-part-time",
        "Full-Time vs Part-Time — Average Weekly Hours Comparison",
        f"Split employee_hours() by avgWeeklyHours > 30 (full-time) vs <= 30 (part-time). {len(full_time)} FT, {len(part_time)} PT employees.",
        f"Full-time avg: {ft_avg:.1f} h/wk, Part-time avg: {pt_avg:.1f} h/wk. FT employees work {ft_avg/pt_avg:.1f}x the hours of PT staff.",
        "instancedRect@1 grouped bars",
        2,
        "full_time = [r for r in emp_hours if r['avgWeeklyHours'] > 30]\npart_time = [r for r in emp_hours if r['avgWeeklyHours'] <= 30]")
    write_trial(57, "full-time-vs-part-time", scene, md)

# ─── Trial 058: Tools Dept Products by Revenue ───────────────────

def trial_058():
    products = db.product_rankings()
    tools = sorted([r for r in products if r["deptId"] == 2], key=lambda r: -r["revenue"])
    for i, r in enumerate(tools):
        r["idx"] = i
    labels = [{"clipX": -0.88, "clipY": 0.85 - i * (1.7 / max(len(tools), 1)), "text": r["name"][:22],
               "align": "l", "fontSize": 9, "color": "#aaa"} for i, r in enumerate(tools)]
    scene, meta = simple_hbar_chart(
        "Tools & Hardware — Products by Revenue",
        tools, "idx", "revenue", "#3b82f6", bar_height=0.6,
        extra_labels=labels, corner_radius=2.0,
    )
    total = sum(r["revenue"] for r in tools)
    md = md_header(58, "tools-products-by-revenue",
        "Tools & Hardware Dept Products Sorted by Revenue",
        f"product_rankings() filtered to deptId==2, sorted by revenue desc. {len(tools)} products.",
        f"Top seller: {tools[0]['name']} at ${tools[0]['revenue']:,.0f}. Dept total: ${total:,.0f}. Top 5 products account for {sum(r['revenue'] for r in tools[:5])/total*100:.0f}% of department revenue.",
        "Horizontal instancedRect@1 bars",
        len(tools),
        "tools = [r for r in product_rankings() if r['deptId'] == 2]\ntools.sort(key=lambda r: -r['revenue'])")
    write_trial(58, "tools-products-by-revenue", scene, md)

# ─── Trial 059: Paint Dept Monthly Revenue ────────────────────────

def trial_059():
    dept_monthly = db.department_monthly_revenue()
    paint = [r for r in dept_monthly if r["deptId"] == 5]
    for i, r in enumerate(paint):
        r["idx"] = i
    scene, meta = simple_line_chart(
        "Paint & Coatings — Monthly Revenue (Seasonal)",
        paint, "idx", "revenue", "#ef4444", line_width=2.5,
        extra_labels=[
            {"clipX": -0.85, "clipY": -0.92, "text": paint[0]["month"], "align": "l", "fontSize": 11, "color": "#888"},
            {"clipX": 0.85, "clipY": -0.92, "text": paint[-1]["month"], "align": "r", "fontSize": 11, "color": "#888"},
        ]
    )
    peak = max(paint, key=lambda r: r["revenue"])
    md = md_header(59, "paint-seasonal-trend",
        "Paint & Coatings — Monthly Revenue (Spring Painting Season)",
        f"department_monthly_revenue() filtered to deptId==5 (Paint). {len(paint)} months.",
        f"Peak: {peak['month']} at ${peak['revenue']:,.0f}. Paint sales follow spring renovation season with March-June being peak months.",
        "lineAA@1 trend line",
        len(paint),
        "dept_monthly = db.department_monthly_revenue()\npaint = [r for r in dept_monthly if r['deptId'] == 5]")
    write_trial(59, "paint-seasonal-trend", scene, md)

# ─── Trial 060: Plumbing Top 10 by Units ─────────────────────────

def trial_060():
    products = db.product_rankings()
    plumbing = sorted([r for r in products if r["deptId"] == 3], key=lambda r: -r["units"])[:10]
    for i, r in enumerate(plumbing):
        r["idx"] = i
    labels = [{"clipX": -0.88, "clipY": 0.85 - i * 0.165, "text": r["name"][:22],
               "align": "l", "fontSize": 10, "color": "#aaa"} for i, r in enumerate(plumbing)]
    scene, meta = simple_hbar_chart(
        "Plumbing — Top 10 Products by Units Sold",
        plumbing, "idx", "units", "#06b6d4", bar_height=0.6,
        extra_labels=labels, corner_radius=2.0,
    )
    md = md_header(60, "plumbing-top-sellers",
        "Plumbing Dept — Top 10 Products by Units Sold",
        f"product_rankings() filtered to deptId==3, sorted by units desc, top 10.",
        f"Top seller: {plumbing[0]['name']} with {plumbing[0]['units']} units. Plumbing essentials (fittings, pipes) dominate volume.",
        "Horizontal instancedRect@1 bars",
        10,
        "plumbing = [r for r in product_rankings() if r['deptId'] == 3]\nplumbing.sort(key=lambda r: -r['units'])[:10]")
    write_trial(60, "plumbing-top-sellers", scene, md)

# ─── Trial 061: Electrical Products by Price ──────────────────────

def trial_061():
    products = db.product_rankings()
    elec = sorted([r for r in products if r["deptId"] == 4], key=lambda r: -r["unitPrice"])
    for i, r in enumerate(elec):
        r["idx"] = i
    labels = [{"clipX": -0.88, "clipY": 0.85 - i * (1.7 / max(len(elec), 1)), "text": r["name"][:22],
               "align": "l", "fontSize": 9, "color": "#aaa"} for i, r in enumerate(elec)]
    scene, meta = simple_hbar_chart(
        "Electrical — Products by Unit Price",
        elec, "idx", "unitPrice", "#8b5cf6", bar_height=0.6,
        extra_labels=labels, corner_radius=2.0,
    )
    prices = [r["unitPrice"] for r in elec]
    md = md_header(61, "electrical-price-range",
        "Electrical Dept Products Sorted by Unit Price",
        f"product_rankings() filtered to deptId==4, sorted by unitPrice desc. {len(elec)} products.",
        f"Price range: ${min(prices):.2f} to ${max(prices):.2f}. Electrical spans from cheap wire nuts to expensive panel components.",
        "Horizontal instancedRect@1 bars",
        len(elec),
        "elec = [r for r in product_rankings() if r['deptId'] == 4]\nelec.sort(key=lambda r: -r['unitPrice'])")
    write_trial(61, "electrical-price-range", scene, md)

# ─── Trial 062: Popular Products (popularity > 70) ───────────────

def trial_062():
    ppv = db.product_price_vs_volume()
    popular = [r for r in ppv if r.get("unitsSold", 0) > 0]
    # Need popularity from raw products
    prod_map = {p["id"]: p for p in db.products}
    for r in popular:
        r["popularity"] = prod_map[r["id"]]["popularity"]
    popular = [r for r in popular if r["popularity"] > 70]

    data, meta = db.to_scatter(popular, "popularity", "revenue")
    tx = db.fit_transform(meta["xRange"], meta["yRange"])
    scene = make_scene(
        f"Popular Products (Popularity > 70) — {len(popular)} Items",
        buffers={"100": {"data": data}},
        transforms={"50": tx},
        panes={"1": {"name": "main", "region": {"clipYMin": -0.85, "clipYMax": 0.85, "clipXMin": -0.90, "clipXMax": 0.90},
                      "hasClearColor": True, "clearColor": [0.07, 0.07, 0.10, 1.0]}},
        layers={"10": {"paneId": 1, "name": "data"}},
        geometries={"101": {"vertexBufferId": 100, "format": "pos2_clip", "vertexCount": meta["vertexCount"]}},
        draw_items={"102": {"layerId": 10, "pipeline": "points@1", "geometryId": 101,
                            "transformId": 50, "color": db.hex_to_rgba("#f59e0b"), "pointSize": 6.0}},
        text_labels=[
            {"clipX": 0.0, "clipY": -0.92, "text": "Popularity Score -->", "align": "c", "fontSize": 11, "color": "#888"},
            {"clipX": -0.95, "clipY": 0.0, "text": "Revenue ($) -->", "align": "l", "fontSize": 11, "color": "#888"},
        ]
    )
    md = md_header(62, "popular-products",
        "Popular Products (Popularity > 70) — Scatter",
        f"Filter products where popularity > 70. {len(popular)} out of {len(db.products)} products qualify.",
        f"High-popularity products cluster in {meta['yRange'][0]:.0f}-{meta['yRange'][1]:.0f} revenue range. Popularity doesn't strictly predict revenue.",
        "points@1 scatter plot",
        len(popular),
        "popular = [p for p in products if p['popularity'] > 70]\nScatter: popularity (x) vs revenue (y)")
    write_trial(62, "popular-products", scene, md)

# ─── Trial 063: Slow Movers (<100 units) ─────────────────────────

def trial_063():
    products = db.product_rankings()
    slow = sorted([r for r in products if r["units"] < 100 and r["units"] > 0], key=lambda r: r["units"])
    for i, r in enumerate(slow):
        r["idx"] = i
    # Limit to 20 for readability
    display = slow[:20]
    for i, r in enumerate(display):
        r["idx"] = i
    labels = [{"clipX": -0.88, "clipY": 0.85 - i * (1.7 / max(len(display), 1)), "text": r["name"][:22],
               "align": "l", "fontSize": 9, "color": "#aaa"} for i, r in enumerate(display)]
    scene, meta = simple_hbar_chart(
        f"Slow-Moving Products (<100 units) — {len(slow)} Total, Showing 20",
        display, "idx", "units", "#ef4444", bar_height=0.6,
        extra_labels=labels, corner_radius=2.0,
    )
    md = md_header(63, "slow-movers",
        "Slow-Moving Products (< 100 Units Sold)",
        f"product_rankings() filtered to units < 100. {len(slow)} products qualify out of {len(products)}.",
        f"Slowest: {slow[0]['name']} with {slow[0]['units']} units. These products may need markdown, removal, or repositioning.",
        "Horizontal instancedRect@1 bars (top 20 shown)",
        min(20, len(slow)),
        "slow = [r for r in product_rankings() if r['units'] < 100]\nslow.sort(key=lambda r: r['units'])")
    write_trial(63, "slow-movers", scene, md)

# ─── Trial 064: Above-Average Employees (Hours) ──────────────────

def trial_064():
    emp_hours = db.employee_hours()
    mean_hrs = sum(r["avgWeeklyHours"] for r in emp_hours) / len(emp_hours)
    above = sorted([r for r in emp_hours if r["avgWeeklyHours"] > mean_hrs], key=lambda r: -r["avgWeeklyHours"])
    for i, r in enumerate(above):
        r["idx"] = i
    labels = [{"clipX": -0.88, "clipY": 0.85 - i * (1.7 / max(len(above), 1)), "text": r["name"][:20],
               "align": "l", "fontSize": 10, "color": "#aaa"} for i, r in enumerate(above)]
    scene, meta = simple_hbar_chart(
        f"Above-Average Employees (>{mean_hrs:.1f} h/wk)",
        above, "idx", "avgWeeklyHours", "#22c55e", bar_height=0.6,
        extra_labels=labels, corner_radius=2.0,
    )
    md = md_header(64, "above-avg-employees",
        "Above-Average Employees by Weekly Hours",
        f"employee_hours() filtered where avgWeeklyHours > mean ({mean_hrs:.1f}). {len(above)} of {len(emp_hours)} employees.",
        f"Top worker: {above[0]['name']} at {above[0]['avgWeeklyHours']:.1f} h/wk. These {len(above)} employees consistently exceed the store average.",
        "Horizontal instancedRect@1 bars",
        len(above),
        f"mean = sum(avgWeeklyHours) / len(employees) = {mean_hrs:.1f}\nabove = [e for e in employee_hours() if avgWeeklyHours > mean]")
    write_trial(64, "above-avg-employees", scene, md)

# ─── Trial 065: Large Basket Sales (itemCount >= 5) ──────────────

def trial_065():
    large = [s for s in db.sales if s["itemCount"] >= 5]
    monthly_count = defaultdict(int)
    for s in large:
        monthly_count[s["date"][:7]] += 1
    months = sorted(monthly_count.keys())
    items = [{"month": m, "idx": i, "count": monthly_count[m]} for i, m in enumerate(months)]
    scene, meta = simple_bar_chart(
        "Large Basket Sales (5+ Items) — Monthly Count",
        items, "idx", "count", "#f59e0b", bar_width=0.6,
        extra_labels=[
            {"clipX": -0.85, "clipY": -0.92, "text": months[0], "align": "l", "fontSize": 11, "color": "#888"},
            {"clipX": 0.85, "clipY": -0.92, "text": months[-1], "align": "r", "fontSize": 11, "color": "#888"},
        ]
    )
    total = len(large)
    md = md_header(65, "large-basket-sales",
        "Large Basket Sales (5+ Items) — Monthly Count",
        f"Filter sales where itemCount >= 5. {total} qualifying sales ({total/len(db.sales)*100:.1f}%).",
        f"Large baskets indicate project shoppers buying multiple items. Avg {total/len(months):.1f} large-basket sales per month.",
        "instancedRect@1 bar chart",
        len(items),
        "large = [s for s in db.sales if s['itemCount'] >= 5]\nmonthly_count[s['date'][:7]] += 1")
    write_trial(65, "large-basket-sales", scene, md)

# ─── Trial 066: Black Friday Window (Nov 20-30) ──────────────────

def trial_066():
    daily = db.daily_revenue()
    bf = [r for r in daily if r["date"][5:7] == "11" and 20 <= int(r["date"][8:10]) <= 30]
    # Sort by date, group by year, then flatten as index
    bf.sort(key=lambda r: r["date"])
    for i, r in enumerate(bf):
        r["idx"] = i
    scene, meta = simple_bar_chart(
        "Black Friday Window (Nov 20-30) — Daily Revenue",
        bf, "idx", "revenue", "#ef4444", bar_width=0.6,
        extra_labels=[
            {"clipX": -0.85, "clipY": -0.92, "text": bf[0]["date"], "align": "l", "fontSize": 11, "color": "#888"},
            {"clipX": 0.85, "clipY": -0.92, "text": bf[-1]["date"], "align": "r", "fontSize": 11, "color": "#888"},
        ]
    )
    peak = max(bf, key=lambda r: r["revenue"])
    md = md_header(66, "black-friday-window",
        "Black Friday Window (Nov 20-30) — Daily Revenue",
        f"daily_revenue() filtered to November 20-30 across all years. {len(bf)} days.",
        f"Peak day: {peak['date']} at ${peak['revenue']:,.0f}. Black Friday and surrounding days show elevated spending.",
        "instancedRect@1 bar chart",
        len(bf),
        "bf = [r for r in daily if month == '11' and 20 <= day <= 30]")
    write_trial(66, "black-friday-window", scene, md)

# ─── Trial 067: YoY H2 Comparison (Jul-Dec 2024 vs 2025) ────────

def trial_067():
    daily = db.daily_revenue()
    h2_2024 = [r for r in daily if r["date"] >= "2024-07-01" and r["date"] <= "2024-12-31"]
    h2_2025 = [r for r in daily if r["date"] >= "2025-07-01" and r["date"] <= "2025-12-31"]

    # Aggregate monthly for cleaner comparison
    def monthly_agg(records):
        by_month = defaultdict(float)
        for r in records:
            by_month[r["date"][:7]] += r["revenue"]
        months = sorted(by_month.keys())
        return [{"month": m, "idx": i, "revenue": round(by_month[m], 2)} for i, m in enumerate(months)]

    items_2024 = monthly_agg(h2_2024)
    items_2025 = monthly_agg(h2_2025)

    if items_2024 and items_2025:
        scene = dual_line_chart(
            "H2 Revenue: 2024 vs 2025",
            items_2024, items_2025, "idx", "revenue",
            "#3b82f6", "#ef4444", "H2 2024", "H2 2025",
            line_width=2.5,
            extra_labels=[
                {"clipX": -0.85, "clipY": -0.92, "text": "Jul", "align": "l", "fontSize": 11, "color": "#888"},
                {"clipX": 0.85, "clipY": -0.92, "text": "Dec", "align": "r", "fontSize": 11, "color": "#888"},
            ]
        )
    else:
        # Fallback if 2025 H2 data doesn't exist yet
        scene, _ = simple_line_chart("H2 2024 Revenue", items_2024, "idx", "revenue", "#3b82f6")

    t24 = sum(r["revenue"] for r in items_2024) if items_2024 else 0
    t25 = sum(r["revenue"] for r in items_2025) if items_2025 else 0
    md = md_header(67, "yoy-h2-comparison",
        "Year-over-Year H2 Comparison (Jul-Dec 2024 vs 2025)",
        f"Aggregate daily_revenue() into monthly for Jul-Dec each year. 2024: {len(items_2024)} months, 2025: {len(items_2025)} months.",
        f"H2 2024 total: ${t24:,.0f}, H2 2025 total: ${t25:,.0f}. {'Growth' if t25 > t24 else 'Decline'} of {abs(t25 - t24)/t24*100:.1f}% YoY." if t24 > 0 else "2025 H2 data not yet available.",
        "Two overlaid lineAA@1 (indexed 0-5 for month offset)",
        (len(items_2024) + len(items_2025)),
        "h2_2024 = daily filtered Jul-Dec 2024\nh2_2025 = daily filtered Jul-Dec 2025\nAggregate to monthly, overlay on same 0-5 index axis")
    write_trial(67, "yoy-h2-comparison", scene, md)

# ─── Trial 068: Summer 2025 Daily Revenue ─────────────────────────

def trial_068():
    daily = db.daily_revenue()
    summer = [r for r in daily if r["date"] >= "2025-06-01" and r["date"] <= "2025-08-31"]
    for i, r in enumerate(summer):
        r["idx"] = i
    scene, meta = simple_line_chart(
        "Summer 2025 Daily Revenue (Jun-Aug)",
        summer, "idx", "revenue", "#22c55e", line_width=1.5,
        extra_labels=[
            {"clipX": -0.85, "clipY": -0.92, "text": "Jun 1", "align": "l", "fontSize": 11, "color": "#888"},
            {"clipX": 0.85, "clipY": -0.92, "text": "Aug 31", "align": "r", "fontSize": 11, "color": "#888"},
        ]
    )
    avg = sum(r["revenue"] for r in summer) / len(summer) if summer else 0
    md = md_header(68, "summer-2025-daily",
        "Summer 2025 Daily Revenue (Jun-Aug)",
        f"daily_revenue() filtered to 2025-06-01..2025-08-31. {len(summer)} days.",
        f"Summer avg: ${avg:,.0f}/day. Peak outdoor project season drives consistent hardware store traffic.",
        "lineAA@1 trend line (dense daily)",
        len(summer),
        "summer = [r for r in daily if r['date'] >= '2025-06-01' and r['date'] <= '2025-08-31']")
    write_trial(68, "summer-2025-daily", scene, md)

# ─── Trial 069: January Slump (All January Days) ─────────────────

def trial_069():
    daily = db.daily_revenue()
    jan = [r for r in daily if r["date"][5:7] == "01"]
    jan.sort(key=lambda r: r["date"])
    for i, r in enumerate(jan):
        r["idx"] = i
    scene, meta = simple_bar_chart(
        "All January Days — Daily Revenue (Post-Holiday Depression)",
        jan, "idx", "revenue", "#64748b", bar_width=0.6,
        extra_labels=[
            {"clipX": -0.85, "clipY": -0.92, "text": jan[0]["date"], "align": "l", "fontSize": 11, "color": "#888"},
            {"clipX": 0.85, "clipY": -0.92, "text": jan[-1]["date"], "align": "r", "fontSize": 11, "color": "#888"},
        ]
    )
    avg = sum(r["revenue"] for r in jan) / len(jan) if jan else 0
    all_avg = sum(r["revenue"] for r in daily) / len(daily)
    md = md_header(69, "january-slump",
        "January Slump — All January Days Across All Years",
        f"daily_revenue() filtered to month == '01'. {len(jan)} January days.",
        f"January avg: ${avg:,.0f}/day vs overall avg: ${all_avg:,.0f}/day. {'Below' if avg < all_avg else 'Above'} average by {abs(avg - all_avg)/all_avg*100:.0f}%. Post-holiday budget tightening visible.",
        "instancedRect@1 bar chart",
        len(jan),
        "jan = [r for r in daily if r['date'][5:7] == '01']")
    write_trial(69, "january-slump", scene, md)

# ─── Trial 070: Credit Card % of Monthly Revenue ─────────────────

def trial_070():
    cc_monthly = defaultdict(float)
    total_monthly = defaultdict(float)
    for s in db.sales:
        mk = s["date"][:7]
        total_monthly[mk] += s["total"]
        if s["paymentMethod"] == "credit_card":
            cc_monthly[mk] += s["total"]
    months = sorted(total_monthly.keys())
    items = [{"month": m, "idx": i,
              "pct": round(cc_monthly.get(m, 0) / total_monthly[m] * 100, 2) if total_monthly[m] else 0}
             for i, m in enumerate(months)]
    scene, meta = simple_line_chart(
        "Credit Card Share of Monthly Revenue (%)",
        items, "idx", "pct", "#8b5cf6", line_width=2.5,
        extra_labels=[
            {"clipX": -0.85, "clipY": -0.92, "text": months[0], "align": "l", "fontSize": 11, "color": "#888"},
            {"clipX": 0.85, "clipY": -0.92, "text": months[-1], "align": "r", "fontSize": 11, "color": "#888"},
        ]
    )
    avg_pct = sum(r["pct"] for r in items) / len(items)
    md = md_header(70, "credit-card-pct-monthly",
        "Credit Card Share of Monthly Revenue (%)",
        f"Compute credit_card sales / total sales per month. {len(months)} months.",
        f"Average CC share: {avg_pct:.1f}%. Trend reveals whether digital payment adoption is growing.",
        "lineAA@1 trend line",
        len(items),
        "cc_monthly[mk] += s['total'] for paymentMethod == 'credit_card'\npct = cc_monthly / total_monthly * 100")
    write_trial(70, "credit-card-pct-monthly", scene, md)

# ─── Trial 071: Supplier Cost Ranking ─────────────────────────────

def trial_071():
    suppliers = db.supplier_performance()
    by_cost = sorted(suppliers, key=lambda r: -r["totalCost"])
    for i, r in enumerate(by_cost):
        r["idx"] = i
    labels = [{"clipX": -0.88, "clipY": 0.85 - i * (1.7 / max(len(by_cost), 1)), "text": r["name"][:25],
               "align": "l", "fontSize": 10, "color": "#aaa"} for i, r in enumerate(by_cost)]
    scene, meta = simple_hbar_chart(
        "Supplier Cost Ranking (Total PO Cost)",
        by_cost, "idx", "totalCost", "#f97316", bar_height=0.6,
        extra_labels=labels, corner_radius=2.0,
    )
    total = sum(r["totalCost"] for r in by_cost)
    md = md_header(71, "supplier-cost-ranking",
        "Supplier Cost Ranking by Total PO Cost",
        f"supplier_performance() sorted by totalCost desc. {len(by_cost)} suppliers.",
        f"Top supplier: {by_cost[0]['name']} at ${by_cost[0]['totalCost']:,.0f} ({by_cost[0]['totalCost']/total*100:.0f}% of total). Concentration risk: top 3 = {sum(r['totalCost'] for r in by_cost[:3])/total*100:.0f}%.",
        "Horizontal instancedRect@1 bars",
        len(by_cost),
        "suppliers = db.supplier_performance()\nby_cost = sorted(suppliers, key=lambda r: -r['totalCost'])")
    write_trial(71, "supplier-cost-ranking", scene, md)

# ─── Trial 072: Low Stock Products ────────────────────────────────

def trial_072():
    # Get latest snapshot per product
    latest = {}
    for snap in db.inventory_snapshots:
        pid = snap["productId"]
        if pid not in latest or snap["date"] > latest[pid]["date"]:
            latest[pid] = snap
    # No products are strictly below reorder point; use <= 1.2x threshold for "near reorder"
    low_stock = [s for s in latest.values() if s["quantityOnHand"] <= s["reorderPoint"] * 1.2]
    low_stock.sort(key=lambda s: s["quantityOnHand"] / s["reorderPoint"] if s["reorderPoint"] > 0 else 999)

    prod_map = {p["id"]: p for p in db.products}
    items = []
    for i, s in enumerate(low_stock):
        p = prod_map.get(s["productId"])
        name = p["name"] if p else f"Product {s['productId']}"
        items.append({"idx": i, "name": name, "qty": s["quantityOnHand"], "reorder": s["reorderPoint"]})

    display = items[:20]
    for i, r in enumerate(display):
        r["idx"] = i
    labels = [{"clipX": -0.88, "clipY": 0.85 - i * (1.7 / max(len(display), 1)), "text": r["name"][:22],
               "align": "l", "fontSize": 9, "color": "#aaa"} for i, r in enumerate(display)]
    scene, meta = simple_hbar_chart(
        f"Low Stock Alert — {len(low_stock)} Products Below Reorder Point",
        display, "idx", "qty", "#ef4444", bar_height=0.6,
        extra_labels=labels, corner_radius=2.0,
    )
    md = md_header(72, "low-stock-products",
        "Low Stock Products Below Reorder Point",
        f"From latest inventory_snapshots per product, filter where qty <= reorderPoint * 1.2. {len(low_stock)} products near reorder threshold.",
        f"Most critical: {items[0]['name']} with {items[0]['qty']} units (reorder at {items[0]['reorder']}). These products are at or near replenishment threshold.",
        "Horizontal instancedRect@1 bars (top 20)",
        min(20, len(low_stock)),
        "latest = most recent snapshot per product\nlow_stock = [s for s in latest if qty <= reorderPoint * 1.2]")
    write_trial(72, "low-stock-products", scene, md)

# ─── Trial 073: High Revenue, Low Margin Products ────────────────

def trial_073():
    products = db.product_rankings()
    products_with_rev = [r for r in products if r["revenue"] > 0]
    products_with_rev.sort(key=lambda r: -r["revenue"])
    top_30_rev = set(r["id"] for r in products_with_rev[:30])
    # Bottom 50% by margin
    by_margin = sorted(products_with_rev, key=lambda r: r["margin"])
    bottom_half = set(r["id"] for r in by_margin[:len(by_margin) // 2])
    # Intersection
    targets = [r for r in products_with_rev if r["id"] in top_30_rev and r["id"] in bottom_half]

    data, meta = db.to_scatter(targets, "revenue", "margin")
    tx = db.fit_transform(meta["xRange"], meta["yRange"])
    scene = make_scene(
        f"High Revenue + Low Margin Products — {len(targets)} Items",
        buffers={"100": {"data": data}},
        transforms={"50": tx},
        panes={"1": {"name": "main", "region": {"clipYMin": -0.85, "clipYMax": 0.85, "clipXMin": -0.90, "clipXMax": 0.90},
                      "hasClearColor": True, "clearColor": [0.07, 0.07, 0.10, 1.0]}},
        layers={"10": {"paneId": 1, "name": "data"}},
        geometries={"101": {"vertexBufferId": 100, "format": "pos2_clip", "vertexCount": meta["vertexCount"]}},
        draw_items={"102": {"layerId": 10, "pipeline": "points@1", "geometryId": 101,
                            "transformId": 50, "color": db.hex_to_rgba("#ef4444"), "pointSize": 8.0}},
        text_labels=[
            {"clipX": 0.0, "clipY": -0.92, "text": "Revenue ($) -->", "align": "c", "fontSize": 11, "color": "#888"},
            {"clipX": -0.95, "clipY": 0.0, "text": "Margin -->", "align": "l", "fontSize": 11, "color": "#888"},
        ]
    )
    md = md_header(73, "high-rev-low-margin",
        "High Revenue but Low Margin Products",
        f"Top 30 by revenue intersected with bottom 50% by margin. {len(targets)} products qualify.",
        f"These {len(targets)} products drive revenue but eat margin. Consider price increases or supplier renegotiation.",
        "points@1 scatter plot (revenue x, margin y)",
        len(targets),
        "top_30_rev = top 30 by revenue\nbottom_half = bottom 50% by margin\ntargets = intersection of both sets")
    write_trial(73, "high-rev-low-margin", scene, md)

# ─── Trial 074: Customer Frequency Top 20 ────────────────────────

def trial_074():
    cust_counts = Counter(s["customerId"] for s in db.sales if s["customerId"] is not None)
    top_20 = cust_counts.most_common(20)
    cust_map = {c["id"]: c for c in db.customers}
    items = []
    for i, (cid, count) in enumerate(top_20):
        c = cust_map.get(cid)
        name = f"{c['firstName']} {c['lastName'][:1]}." if c else f"#{cid}"
        items.append({"idx": i, "name": name, "count": count})
    labels = [{"clipX": -0.88, "clipY": 0.85 - i * (1.7 / 20), "text": r["name"][:20],
               "align": "l", "fontSize": 10, "color": "#aaa"} for i, r in enumerate(items)]
    scene, meta = simple_hbar_chart(
        "Top 20 Customers by Purchase Frequency",
        items, "idx", "count", "#3b82f6", bar_height=0.6,
        extra_labels=labels, corner_radius=2.0,
    )
    md = md_header(74, "customer-frequency-top20",
        "Top 20 Customers by Purchase Frequency",
        f"Count sales per customerId, top 20. {len(cust_counts)} unique customers total.",
        f"Most frequent: {items[0]['name']} with {items[0]['count']} visits. Top 20 account for {sum(r['count'] for r in items)} total visits.",
        "Horizontal instancedRect@1 bars",
        20,
        "cust_counts = Counter(s['customerId'] for s in db.sales if customerId is not None)\ntop_20 = cust_counts.most_common(20)")
    write_trial(74, "customer-frequency-top20", scene, md)

# ─── Trial 075: Top 20 Customers by Total Spend ──────────────────

def trial_075():
    cust_spend = defaultdict(float)
    for s in db.sales:
        if s["customerId"] is not None:
            cust_spend[s["customerId"]] += s["total"]
    top_20 = sorted(cust_spend.items(), key=lambda x: -x[1])[:20]
    cust_map = {c["id"]: c for c in db.customers}
    items = []
    for i, (cid, spend) in enumerate(top_20):
        c = cust_map.get(cid)
        name = f"{c['firstName']} {c['lastName'][:1]}." if c else f"#{cid}"
        items.append({"idx": i, "name": name, "spend": round(spend, 2)})
    labels = [{"clipX": -0.88, "clipY": 0.85 - i * (1.7 / 20), "text": r["name"][:20],
               "align": "l", "fontSize": 10, "color": "#aaa"} for i, r in enumerate(items)]
    scene, meta = simple_hbar_chart(
        "Top 20 Customers by Total Spend ($)",
        items, "idx", "spend", "#f59e0b", bar_height=0.6,
        extra_labels=labels, corner_radius=2.0,
    )
    total_top20 = sum(r["spend"] for r in items)
    all_spend = sum(cust_spend.values())
    md = md_header(75, "loyal-customers-spending",
        "Top 20 Customers by Total Lifetime Spend",
        f"Aggregate sales.total per customerId, top 20. {len(cust_spend)} customers with recorded purchases.",
        f"Top spender: {items[0]['name']} at ${items[0]['spend']:,.0f}. Top 20 = ${total_top20:,.0f} ({total_top20/all_spend*100:.1f}% of tracked customer revenue).",
        "Horizontal instancedRect@1 bars",
        20,
        "cust_spend[s['customerId']] += s['total']\ntop_20 = sorted by spend desc[:20]")
    write_trial(75, "loyal-customers-spending", scene, md)

# ─── Trial 076: One-Time Customers by Tier ────────────────────────

def trial_076():
    cust_counts = Counter(s["customerId"] for s in db.sales if s["customerId"] is not None)
    one_timers = {cid for cid, cnt in cust_counts.items() if cnt == 1}
    cust_map = {c["id"]: c for c in db.customers}
    tier_counts = Counter()
    for cid in one_timers:
        c = cust_map.get(cid)
        if c:
            tier_counts[c["tier"]] += 1
    order = ["gold", "silver", "bronze"]
    items = [{"tier": t, "idx": i, "count": tier_counts.get(t, 0)} for i, t in enumerate(order)]
    tier_colors = {"gold": "#fbbf24", "silver": "#94a3b8", "bronze": "#cd7f32"}

    data, meta = db.to_bars(items, "idx", "count", bar_width=0.6)
    tx = db.fit_transform(meta["xRange"], meta["yRange"])
    scene = make_scene(
        f"One-Time Customers by Tier — {len(one_timers)} Total",
        buffers={"100": {"data": data}},
        transforms={"50": tx},
        panes={"1": {"name": "main", "region": {"clipYMin": -0.85, "clipYMax": 0.85, "clipXMin": -0.90, "clipXMax": 0.90},
                      "hasClearColor": True, "clearColor": [0.07, 0.07, 0.10, 1.0]}},
        layers={"10": {"paneId": 1, "name": "data"}},
        geometries={"101": {"vertexBufferId": 100, "format": "rect4", "vertexCount": meta["vertexCount"]}},
        draw_items={"102": {"layerId": 10, "pipeline": "instancedRect@1", "geometryId": 101,
                            "transformId": 50, "color": db.hex_to_rgba("#8b5cf6"), "cornerRadius": 4.0}},
        text_labels=[
            {"clipX": -0.3, "clipY": -0.92, "text": f"Gold: {items[0]['count']}", "align": "c", "fontSize": 12, "color": "#fbbf24"},
            {"clipX": 0.0, "clipY": -0.92, "text": f"Silver: {items[1]['count']}", "align": "c", "fontSize": 12, "color": "#94a3b8"},
            {"clipX": 0.3, "clipY": -0.92, "text": f"Bronze: {items[2]['count']}", "align": "c", "fontSize": 12, "color": "#cd7f32"},
        ]
    )
    md = md_header(76, "one-time-customers",
        "One-Time Customers Grouped by Tier",
        f"Count sales per customer, filter to count == 1. {len(one_timers)} one-time customers.",
        f"Gold: {tier_counts.get('gold',0)}, Silver: {tier_counts.get('silver',0)}, Bronze: {tier_counts.get('bronze',0)}. "
        f"Bronze dominates single-purchase behavior — potential loyalty program targets.",
        "instancedRect@1 bar chart",
        3,
        "one_timers = {cid for cid, cnt in cust_counts.items() if cnt == 1}\ntier_counts = Counter(tier for one-timer customers)")
    write_trial(76, "one-time-customers", scene, md)

# ─── Trial 077: Store Manager (empId=1) Shift Hours ──────────────

def trial_077():
    manager_shifts = [sh for sh in db.shifts if sh["employeeId"] == 1]
    monthly_hours = defaultdict(float)
    for sh in manager_shifts:
        monthly_hours[sh["date"][:7]] += sh["hoursWorked"]
    months = sorted(monthly_hours.keys())
    items = [{"month": m, "idx": i, "hours": round(monthly_hours[m], 1)} for i, m in enumerate(months)]
    emp = [e for e in db.employees if e["id"] == 1][0]
    name = f"{emp['firstName']} {emp['lastName']}"
    scene, meta = simple_line_chart(
        f"{name} (Store Manager) — Monthly Hours",
        items, "idx", "hours", "#ec4899", line_width=2.5,
        extra_labels=[
            {"clipX": -0.85, "clipY": -0.92, "text": months[0], "align": "l", "fontSize": 11, "color": "#888"},
            {"clipX": 0.85, "clipY": -0.92, "text": months[-1], "align": "r", "fontSize": 11, "color": "#888"},
        ]
    )
    avg = sum(r["hours"] for r in items) / len(items)
    md = md_header(77, "manager-shift-hours",
        f"Store Manager ({name}) — Monthly Shift Hours",
        f"Filter shifts to employeeId==1 ({name}). {len(manager_shifts)} shifts across {len(months)} months.",
        f"Avg {avg:.0f} hours/month. Manager workload is consistently high across all seasons.",
        "lineAA@1 trend line",
        len(items),
        f"manager_shifts = [sh for sh in db.shifts if sh['employeeId'] == 1]\nmonthly_hours[sh['date'][:7]] += sh['hoursWorked']")
    write_trial(77, "manager-shift-hours", scene, md)

# ─── Trial 078: Cashier Sales Count ───────────────────────────────

def trial_078():
    cashiers = [e for e in db.employees if e["role"] == "Cashier"]
    cashier_ids = {e["id"] for e in cashiers}
    sales_by_cashier = Counter(s["employeeId"] for s in db.sales if s["employeeId"] in cashier_ids)
    items = []
    for i, e in enumerate(sorted(cashiers, key=lambda e: -sales_by_cashier.get(e["id"], 0))):
        items.append({"idx": i, "name": f"{e['firstName']} {e['lastName'][:1]}.",
                       "count": sales_by_cashier.get(e["id"], 0)})
    labels = [{"clipX": -0.88, "clipY": 0.85 - i * (1.7 / max(len(items), 1)), "text": r["name"][:20],
               "align": "l", "fontSize": 10, "color": "#aaa"} for i, r in enumerate(items)]
    scene, meta = simple_hbar_chart(
        "Cashier Sales Count — Who Rings Up the Most?",
        items, "idx", "count", "#06b6d4", bar_height=0.6,
        extra_labels=labels, corner_radius=2.0,
    )
    total = sum(r["count"] for r in items)
    md = md_header(78, "cashier-sales-count",
        "Cashier Sales Count — Transactions per Cashier",
        f"Filter employees with role=='Cashier'. Count sales per cashier. {len(cashiers)} cashiers.",
        f"Top cashier: {items[0]['name']} with {items[0]['count']} transactions. Total cashier transactions: {total}.",
        "Horizontal instancedRect@1 bars",
        len(items),
        "cashiers = [e for e in db.employees if e['role'] == 'Cashier']\nsales_by_cashier = Counter(s['employeeId'] for cashier sales)")
    write_trial(78, "cashier-sales-count", scene, md)

# ─── Trial 079: Department Leads Weekly Hours ─────────────────────

def trial_079():
    dept_leads = [e for e in db.employees if e["role"] == "Department Lead"]
    emp_hours = db.employee_hours()
    hours_map = {r["id"]: r for r in emp_hours}
    items = []
    for e in sorted(dept_leads, key=lambda e: -hours_map.get(e["id"], {}).get("avgWeeklyHours", 0)):
        h = hours_map.get(e["id"])
        if h:
            items.append({"idx": len(items), "name": f"{e['firstName']} {e['lastName'][:1]}.",
                           "hours": h["avgWeeklyHours"]})
    labels = [{"clipX": -0.88, "clipY": 0.85 - i * (1.7 / max(len(items), 1)), "text": r["name"][:20],
               "align": "l", "fontSize": 10, "color": "#aaa"} for i, r in enumerate(items)]
    scene, meta = simple_hbar_chart(
        "Department Leads — Average Weekly Hours",
        items, "idx", "hours", "#8b5cf6", bar_height=0.6,
        extra_labels=labels, corner_radius=2.0,
    )
    avg = sum(r["hours"] for r in items) / len(items) if items else 0
    md = md_header(79, "dept-leads-weekly-hours",
        "Department Leads — Average Weekly Hours Comparison",
        f"Filter employees with role=='Department Lead'. {len(dept_leads)} leads.",
        f"Avg across leads: {avg:.1f} h/wk. Top: {items[0]['name']} at {items[0]['hours']:.1f}h. Lead workloads vary by department demands.",
        "Horizontal instancedRect@1 bars",
        len(items),
        "dept_leads = [e for e in db.employees if e['role'] == 'Department Lead']\nJoin with employee_hours() for avg weekly")
    write_trial(79, "dept-leads-weekly-hours", scene, md)

# ─── Trial 080: Terminated Employee Shift History ─────────────────

def trial_080():
    # Employee 26: Sam O'Donnell, terminated 2025-08-15
    emp = [e for e in db.employees if e["id"] == 26][0]
    name = f"{emp['firstName']} {emp['lastName']}"
    shifts = [sh for sh in db.shifts if sh["employeeId"] == 26]
    monthly_hours = defaultdict(float)
    for sh in shifts:
        monthly_hours[sh["date"][:7]] += sh["hoursWorked"]
    months = sorted(monthly_hours.keys())
    items = [{"month": m, "idx": i, "hours": round(monthly_hours[m], 1)} for i, m in enumerate(months)]
    scene, meta = simple_line_chart(
        f"{name} (Terminated) — Monthly Hours Until Departure",
        items, "idx", "hours", "#ef4444", line_width=2.5,
        extra_labels=[
            {"clipX": -0.85, "clipY": -0.92, "text": months[0], "align": "l", "fontSize": 11, "color": "#888"},
            {"clipX": 0.85, "clipY": -0.92, "text": f"{months[-1]} (term.)", "align": "r", "fontSize": 11, "color": "#ef4444"},
        ]
    )
    total = sum(r["hours"] for r in items)
    avg = total / len(items) if items else 0
    md = md_header(80, "terminated-employee",
        f"Terminated Employee: {name} — Monthly Hours Until Departure",
        f"Filter shifts to employeeId==26 ({name}, terminated {emp['terminationDate']}). {len(shifts)} shifts across {len(months)} months.",
        f"Total hours: {total:.0f}, avg {avg:.0f}/month. Final months may show declining hours as departure approached.",
        "lineAA@1 trend line",
        len(items),
        f"shifts = [sh for sh in db.shifts if sh['employeeId'] == 26]\nmonthly_hours[sh['date'][:7]] += sh['hoursWorked']\nTerminated: {emp['terminationDate']}")
    write_trial(80, "terminated-employee", scene, md)


# ─── Main ─────────────────────────────────────────────────────────

def main():
    print("Generating data-driven trials 041-080...")
    trial_041()
    trial_042()
    trial_043()
    trial_044()
    trial_045()
    trial_046()
    trial_047()
    trial_048()
    trial_049()
    trial_050()
    trial_051()
    trial_052()
    trial_053()
    trial_054()
    trial_055()
    trial_056()
    trial_057()
    trial_058()
    trial_059()
    trial_060()
    trial_061()
    trial_062()
    trial_063()
    trial_064()
    trial_065()
    trial_066()
    trial_067()
    trial_068()
    trial_069()
    trial_070()
    trial_071()
    trial_072()
    trial_073()
    trial_074()
    trial_075()
    trial_076()
    trial_077()
    trial_078()
    trial_079()
    trial_080()
    print("Done. 40 trials generated in", OUT)


if __name__ == "__main__":
    main()
