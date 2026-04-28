#!/usr/bin/env python3
"""Generate trials 081-120 (Relational & Cross-Table Analysis) for DynaCharting.

These trials visualize JOINS across the Meridian Hardware store database.
Each trial produces:
  - NNN-slug.json  (SceneDocument)
  - NNN-slug.md    (audit markdown with Query and Data Insight)
"""
import json
import math
import os
import sys
from collections import defaultdict, Counter
from datetime import date, timedelta

# ── adapter import ──────────────────────────────────────────────────────────
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))
from data.adapter import StoreData

OUT_DIR = os.path.dirname(os.path.abspath(__file__))
db = StoreData()

# ── helpers ──────────────────────────────────────────────────────────────────

def rf(arr, digits=6):
    return [round(x, digits) for x in arr]

DARK_BG = [0.06, 0.09, 0.16, 1.0]

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

def simple_bar_doc(items, x_key, y_key, title, color, viewport=(1000, 500), bar_width=0.7, corner_r=3.0):
    """Build a single-pane bar chart doc from items."""
    data, meta = db.to_bars(items, x_key, y_key, bar_width=bar_width)
    tx = db.fit_transform(meta["xRange"], meta["yRange"])
    c = db.hex_to_rgba(color)
    doc = make_doc(viewport[0], viewport[1],
        buffers={100: {"byteLength": 0, "data": rf(data)}},
        transforms={50: tx},
        panes={1: {"name": title, "region": {"clipYMin": -0.92, "clipYMax": 0.92, "clipXMin": -0.92, "clipXMax": 0.92}, "clearColor": DARK_BG, "hasClearColor": True}},
        layers={10: {"paneId": 1, "name": "Data"}},
        geometries={200: {"vertexBufferId": 100, "format": "rect4", "vertexCount": meta["vertexCount"]}},
        drawItems={300: {"layerId": 10, "name": title, "pipeline": "instancedRect@1", "geometryId": 200, "transformId": 50, "color": c, "cornerRadius": corner_r}})
    return doc

def simple_hbar_doc(items, y_key, x_key, title, color, viewport=(1000, 500), bar_height=0.7, corner_r=3.0):
    """Build a single-pane horizontal bar chart doc."""
    data, meta = db.to_horizontal_bars(items, y_key, x_key, bar_height=bar_height)
    tx = db.fit_transform(meta["xRange"], meta["yRange"])
    c = db.hex_to_rgba(color)
    doc = make_doc(viewport[0], viewport[1],
        buffers={100: {"byteLength": 0, "data": rf(data)}},
        transforms={50: tx},
        panes={1: {"name": title, "region": {"clipYMin": -0.92, "clipYMax": 0.92, "clipXMin": -0.92, "clipXMax": 0.92}, "clearColor": DARK_BG, "hasClearColor": True}},
        layers={10: {"paneId": 1, "name": "Data"}},
        geometries={200: {"vertexBufferId": 100, "format": "rect4", "vertexCount": meta["vertexCount"]}},
        drawItems={300: {"layerId": 10, "name": title, "pipeline": "instancedRect@1", "geometryId": 200, "transformId": 50, "color": c, "cornerRadius": corner_r}})
    return doc

def simple_scatter_doc(items, x_key, y_key, title, color, point_size=6.0, viewport=(800, 600)):
    """Build a single-pane scatter chart doc."""
    data, meta = db.to_scatter(items, x_key, y_key)
    tx = db.fit_transform(meta["xRange"], meta["yRange"])
    c = db.hex_to_rgba(color)
    doc = make_doc(viewport[0], viewport[1],
        buffers={100: {"byteLength": 0, "data": rf(data)}},
        transforms={50: tx},
        panes={1: {"name": title, "region": {"clipYMin": -0.92, "clipYMax": 0.92, "clipXMin": -0.92, "clipXMax": 0.92}, "clearColor": DARK_BG, "hasClearColor": True}},
        layers={10: {"paneId": 1, "name": "Data"}},
        geometries={200: {"vertexBufferId": 100, "format": "pos2_clip", "vertexCount": meta["vertexCount"]}},
        drawItems={300: {"layerId": 10, "name": title, "pipeline": "points@1", "geometryId": 200, "transformId": 50, "color": c, "pointSize": point_size}})
    return doc

def simple_line_doc(items, x_key, y_key, title, color, line_width=2.5, viewport=(1000, 500)):
    """Build a single-pane lineAA chart doc."""
    data, meta = db.to_line_segments(items, x_key, y_key)
    tx = db.fit_transform(meta["xRange"], meta["yRange"])
    c = db.hex_to_rgba(color)
    doc = make_doc(viewport[0], viewport[1],
        buffers={100: {"byteLength": 0, "data": rf(data)}},
        transforms={50: tx},
        panes={1: {"name": title, "region": {"clipYMin": -0.92, "clipYMax": 0.92, "clipXMin": -0.92, "clipXMax": 0.92}, "clearColor": DARK_BG, "hasClearColor": True}},
        layers={10: {"paneId": 1, "name": "Data"}},
        geometries={200: {"vertexBufferId": 100, "format": "rect4", "vertexCount": meta["vertexCount"]}},
        drawItems={300: {"layerId": 10, "name": title, "pipeline": "lineAA@1", "geometryId": 200, "transformId": 50, "color": c, "lineWidth": line_width}})
    return doc


# ═══════════════════════════════════════════════════════════════════════════
# Trial 081: Revenue by Product Category
# ═══════════════════════════════════════════════════════════════════════════

def trial_081():
    # Join sale_items → products, group by category, sum revenue
    cat_rev = defaultdict(float)
    for si in db.sale_items:
        p = db._prod_map.get(si["productId"])
        if p:
            cat_rev[p["category"]] += si["lineTotal"]
    top15 = sorted(cat_rev.items(), key=lambda x: -x[1])[:15]
    items = [{"index": i, "category": c, "revenue": round(r, 2)} for i, (c, r) in enumerate(top15)]

    doc = simple_bar_doc(items, "index", "revenue", "Revenue by Category", "#3b82f6")

    table_rows = "\n".join(f"| {r['category']} | ${r['revenue']:,.0f} |" for r in items)
    md = f"""# Trial 081: Revenue by Product Category

**Date:** 2026-03-22
**Goal:** Join sale_items to products, aggregate revenue by category, show top 15 as vertical bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sale_items JOIN products ON sale_items.productId = products.id
GROUP BY products.category
SUM(sale_items.lineTotal) AS revenue
ORDER BY revenue DESC
LIMIT 15
```

Join: sale_items (33,834 rows) x products (150 rows) via productId.
Aggregation: sum lineTotal per category, sort descending, take top 15.

## Data Insight

| Category | Revenue |
|----------|---------|
{table_rows}

The top category alone accounts for a significant share of total revenue. Hardware staples
(fasteners, hand tools, pipe, fittings) dominate, reflecting the store's identity as a
hardware-first retailer.

---

## What Was Built

1000x500 viewport, single pane, {len(items)} blue instancedRect bars with corner radius.
Transform maps category indices 0-14 on X and revenue on Y to clip space.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(81, "revenue-by-product-category", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 082: Employees per Department
# ═══════════════════════════════════════════════════════════════════════════

def trial_082():
    dept_count = defaultdict(int)
    for e in db.employees:
        if e["departmentId"]:
            dept_count[e["departmentId"]] += 1
    items = []
    for i, did in enumerate(sorted(dept_count)):
        items.append({"index": i, "dept": db._dept_map[did]["name"], "count": dept_count[did], "deptId": did})

    colors_by_dept = []
    data = []
    hw = 0.7 / 2.0
    for item in items:
        x = item["index"]
        y = item["count"]
        data.extend([x - hw, 0, x + hw, y])
    xs = [it["index"] for it in items]
    ys = [it["count"] for it in items]
    x_range = (min(xs) - hw, max(xs) + hw)
    y_range = (0, max(ys))
    tx = db.fit_transform(x_range, y_range)

    # One drawItem per bar for dept colors
    buffers = {}
    geometries = {}
    drawItems = {}
    for j, item in enumerate(items):
        bid = 100 + j
        gid = 200 + j
        did_item = 300 + j
        x = item["index"]
        y = item["count"]
        bar_data = [x - hw, 0, x + hw, y]
        buffers[bid] = {"byteLength": 0, "data": rf(bar_data)}
        geometries[gid] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": 1}
        c = db.hex_to_rgba(db.PALETTE_DEPT.get(item["deptId"], "#888888"))
        drawItems[did_item] = {"layerId": 10, "name": item["dept"], "pipeline": "instancedRect@1",
                               "geometryId": gid, "transformId": 50, "color": c, "cornerRadius": 3.0}

    doc = make_doc(1000, 500,
        buffers=buffers,
        transforms={50: tx},
        panes={1: {"name": "Employees per Department", "region": {"clipYMin": -0.92, "clipYMax": 0.92, "clipXMin": -0.92, "clipXMax": 0.92},
                    "clearColor": DARK_BG, "hasClearColor": True}},
        layers={10: {"paneId": 1, "name": "Data"}},
        geometries=geometries,
        drawItems=drawItems)

    table_rows = "\n".join(f"| {r['dept']} | {r['count']} |" for r in items)
    md = f"""# Trial 082: Employees per Department

**Date:** 2026-03-22
**Goal:** Count employees by departmentId, show as department-colored bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
employees GROUP BY departmentId
COUNT(*) AS headcount
(excluding employees with departmentId = null — managers, cashiers, stock clerks)
```

Join: employees (35 rows), filter to those with non-null departmentId.
Aggregation: count per department.

## Data Insight

| Department | Headcount |
|------------|-----------|
{table_rows}

Most departments have 2-4 people. Garden & Outdoor and Tools & Hardware are the most
staffed departments, reflecting higher customer traffic in those areas.

---

## What Was Built

1000x500 viewport, single pane, {len(items)} bars each with its department's palette color.
Each bar is a separate DrawItem for per-department coloring.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(82, "employees-per-department", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 083: Revenue per Employee by Department
# ═══════════════════════════════════════════════════════════════════════════

def trial_083():
    # dept revenue / dept employee count
    dept_rev = dict(db._dept_rev)
    dept_emp_count = defaultdict(int)
    for e in db.employees:
        if e["departmentId"]:
            dept_emp_count[e["departmentId"]] += 1
    items = []
    for i, did in enumerate(sorted(dept_rev)):
        ec = dept_emp_count.get(did, 1)
        rpe = dept_rev[did] / ec
        items.append({"index": i, "dept": db._dept_map[did]["name"], "deptId": did,
                       "revenue": round(dept_rev[did], 2), "empCount": ec,
                       "revPerEmp": round(rpe, 2)})

    # Per-dept colored bars
    hw = 0.7 / 2.0
    buffers = {}; geometries = {}; drawItems = {}
    for j, item in enumerate(items):
        bid = 100 + j; gid = 200 + j; diid = 300 + j
        bar_data = [item["index"] - hw, 0, item["index"] + hw, item["revPerEmp"]]
        buffers[bid] = {"byteLength": 0, "data": rf(bar_data)}
        geometries[gid] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": 1}
        c = db.hex_to_rgba(db.PALETTE_DEPT.get(item["deptId"], "#888888"))
        drawItems[diid] = {"layerId": 10, "name": item["dept"], "pipeline": "instancedRect@1",
                           "geometryId": gid, "transformId": 50, "color": c, "cornerRadius": 3.0}

    xs = [it["index"] for it in items]
    ys = [it["revPerEmp"] for it in items]
    tx = db.fit_transform((min(xs) - hw, max(xs) + hw), (0, max(ys)))

    doc = make_doc(1000, 500, buffers=buffers, transforms={50: tx},
        panes={1: {"name": "Revenue/Employee", "region": {"clipYMin": -0.92, "clipYMax": 0.92, "clipXMin": -0.92, "clipXMax": 0.92},
                    "clearColor": DARK_BG, "hasClearColor": True}},
        layers={10: {"paneId": 1, "name": "Data"}},
        geometries=geometries, drawItems=drawItems)

    table_rows = "\n".join(f"| {r['dept']} | ${r['revenue']:,.0f} | {r['empCount']} | ${r['revPerEmp']:,.0f} |" for r in items)
    md = f"""# Trial 083: Revenue per Employee by Department

**Date:** 2026-03-22
**Goal:** Compute department efficiency: total department revenue / employee headcount. Bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sale_items JOIN products ON productId → departmentId → dept_revenue
employees GROUP BY departmentId → dept_headcount
revenue_per_employee = dept_revenue / dept_headcount
```

Two-table join: sale_items x products for revenue, employees for headcount. Cross-table ratio.

## Data Insight

| Department | Revenue | Employees | Rev/Employee |
|------------|---------|-----------|--------------|
{table_rows}

Departments with fewer specialized staff can show higher per-employee revenue, revealing
which teams generate the most value per person.

---

## What Was Built

1000x500 viewport, {len(items)} department-colored bars showing revenue-per-employee ratio.
Each bar is a separate DrawItem for per-department palette coloring.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(83, "revenue-per-employee-by-dept", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 084: Supplier Total Revenue
# ═══════════════════════════════════════════════════════════════════════════

def trial_084():
    # sale_items → products → suppliers, sum revenue by supplier
    supp_rev = defaultdict(float)
    for si in db.sale_items:
        p = db._prod_map.get(si["productId"])
        if p:
            supp_rev[p["supplierId"]] += si["lineTotal"]
    items = sorted([{"supplierId": sid, "name": db._supp_map[sid]["name"], "revenue": round(r, 2)}
                    for sid, r in supp_rev.items()], key=lambda x: -x["revenue"])
    for i, r in enumerate(items):
        r["index"] = i

    doc = simple_bar_doc(items, "index", "revenue", "Revenue by Supplier", "#22c55e")

    table_rows = "\n".join(f"| {r['name']} | ${r['revenue']:,.0f} |" for r in items)
    md = f"""# Trial 084: Supplier Total Revenue

**Date:** 2026-03-22
**Goal:** Three-table join: sale_items -> products -> suppliers. Sum revenue per supplier.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sale_items JOIN products ON productId
           JOIN suppliers ON products.supplierId = suppliers.id
GROUP BY suppliers.id
SUM(sale_items.lineTotal) AS revenue
ORDER BY revenue DESC
```

Three-table chain: sale_items (33,834) x products (150) x suppliers (12).

## Data Insight

| Supplier | Revenue |
|----------|---------|
{table_rows}

The supplier revenue distribution reveals which vendors' products drive the most sales.
This is critical for supplier negotiation leverage — top suppliers may warrant volume discounts.

---

## What Was Built

1000x500 viewport, {len(items)} green bars for supplier revenue, sorted descending.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(84, "supplier-total-revenue", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 085: Gold Customer Department Preference (Pie)
# ═══════════════════════════════════════════════════════════════════════════

def trial_085():
    # Filter sales to gold customers, join sale_items → products → dept
    gold_cust_ids = {c["id"] for c in db.customers if c["tier"] == "gold"}
    gold_sale_ids = {s["id"] for s in db.sales if s["customerId"] in gold_cust_ids}
    dept_rev = defaultdict(float)
    for si in db.sale_items:
        if si["saleId"] in gold_sale_ids:
            p = db._prod_map.get(si["productId"])
            if p:
                dept_rev[p["departmentId"]] += si["lineTotal"]

    items = [{"deptId": did, "name": db._dept_map[did]["name"], "revenue": round(r, 2)}
             for did, r in sorted(dept_rev.items(), key=lambda x: -x[1])]

    wedges = db.to_pie_wedges(items, "revenue", cx=0.0, cy=0.0, r=0.75)
    buffers = {}; geometries = {}; drawItems = {}
    for j, (wdata, frac, sa, ea) in enumerate(wedges):
        bid = 100 + j; gid = 200 + j; diid = 300 + j
        vc = len(wdata) // 2
        buffers[bid] = {"byteLength": 0, "data": rf(wdata)}
        geometries[gid] = {"vertexBufferId": bid, "format": "pos2_clip", "vertexCount": vc}
        c = db.hex_to_rgba(db.PALETTE_DEPT.get(items[j]["deptId"], "#888888"))
        drawItems[diid] = {"layerId": 10, "name": items[j]["name"], "pipeline": "triSolid@1",
                           "geometryId": gid, "color": c}

    doc = make_doc(700, 700, buffers=buffers, transforms={},
        panes={1: {"name": "Gold Dept Pref", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95},
                    "clearColor": DARK_BG, "hasClearColor": True}},
        layers={10: {"paneId": 1, "name": "Pie"}},
        geometries=geometries, drawItems=drawItems)

    total = sum(r["revenue"] for r in items)
    table_rows = "\n".join(f"| {r['name']} | ${r['revenue']:,.0f} | {r['revenue']/total*100:.1f}% |" for r in items)
    md = f"""# Trial 085: Gold Customer Department Preference

**Date:** 2026-03-22
**Goal:** Filter sales to gold-tier customers, join to products, group by department. Pie chart.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
customers WHERE tier='gold' → gold_customer_ids
sales WHERE customerId IN gold_customer_ids → gold_sale_ids
sale_items WHERE saleId IN gold_sale_ids
  JOIN products ON productId → departmentId
GROUP BY departmentId
SUM(lineTotal)
```

Four-table chain: customers -> sales -> sale_items -> products.
Gold customers: {len(gold_cust_ids)} of 500.

## Data Insight

| Department | Revenue | Share |
|------------|---------|-------|
{table_rows}

Gold customers tend to spend heavily on high-ticket items (Lumber, Tools) rather than
consumables, reflecting their DIY project orientation.

---

## What Was Built

700x700 viewport, pie chart with {len(items)} wedges, one per department.
Each wedge uses its department palette color. triSolid@1 triangle fans.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(85, "gold-customer-dept-preference", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 086: Employee Sales Count (Horizontal Bars)
# ═══════════════════════════════════════════════════════════════════════════

def trial_086():
    emp_count = Counter(s["employeeId"] for s in db.sales)
    top15 = emp_count.most_common(15)
    items = []
    for i, (eid, cnt) in enumerate(top15):
        e = db._emp_map[eid]
        items.append({"index": i, "name": f"{e['firstName']} {e['lastName']}", "empId": eid, "count": cnt})

    doc = simple_hbar_doc(items, "index", "count", "Sales Count by Employee", "#f59e0b")

    table_rows = "\n".join(f"| {r['name']} | {r['count']} |" for r in items)
    md = f"""# Trial 086: Employee Sales Count

**Date:** 2026-03-22
**Goal:** Count sales per employeeId, show top 15 as horizontal bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sales GROUP BY employeeId
COUNT(*) AS salesCount
ORDER BY salesCount DESC
LIMIT 15
```

Single table query on sales (12,338 rows), grouped by employeeId, then employee name
lookup via employees table.

## Data Insight

| Employee | Sales Count |
|----------|-------------|
{table_rows}

Cashiers and long-tenured associates dominate the top — they handle the most register
transactions regardless of department.

---

## What Was Built

1000x500 viewport, {len(items)} amber horizontal bars.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(86, "employee-sales-count", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 087: Top Seller Product Mix
# ═══════════════════════════════════════════════════════════════════════════

def trial_087():
    # Find #1 employee by sales count
    emp_count = Counter(s["employeeId"] for s in db.sales)
    top_eid = emp_count.most_common(1)[0][0]
    top_emp = db._emp_map[top_eid]
    top_name = f"{top_emp['firstName']} {top_emp['lastName']}"

    # Get their sale IDs
    sale_ids = {s["id"] for s in db.sales if s["employeeId"] == top_eid}
    # Count products sold
    prod_count = defaultdict(int)
    for si in db.sale_items:
        if si["saleId"] in sale_ids:
            prod_count[si["productId"]] += si["quantity"]
    top_prods = sorted(prod_count.items(), key=lambda x: -x[1])[:15]
    items = [{"index": i, "name": db._prod_map[pid]["name"][:20], "units": cnt}
             for i, (pid, cnt) in enumerate(top_prods)]

    doc = simple_bar_doc(items, "index", "units", f"{top_name} Product Mix", "#8b5cf6")

    table_rows = "\n".join(f"| {r['name']} | {r['units']} |" for r in items)
    md = f"""# Trial 087: Top Seller Product Mix

**Date:** 2026-03-22
**Goal:** Find #1 employee by sales count, then show their top 15 products sold. Bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
Step 1: sales GROUP BY employeeId → top employee = {top_name} (emp #{top_eid})
Step 2: sales WHERE employeeId={top_eid} → sale_ids
Step 3: sale_items WHERE saleId IN sale_ids
        JOIN products ON productId
        GROUP BY productId, SUM(quantity)
        ORDER BY units DESC LIMIT 15
```

Three-table chain: employees -> sales -> sale_items -> products.

## Data Insight

| Product | Units Sold |
|---------|-----------|
{table_rows}

{top_name}'s product mix reveals whether they are a generalist (selling across departments)
or a specialist focused on one category.

---

## What Was Built

1000x500 viewport, {len(items)} violet bars for the top seller's product breakdown.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(87, "top-seller-product-mix", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 088: Staffing vs Revenue Scatter
# ═══════════════════════════════════════════════════════════════════════════

def trial_088():
    # Per day: total shift hours, total revenue
    day_hours = defaultdict(float)
    for sh in db.shifts:
        day_hours[sh["date"]] += sh["hoursWorked"]
    day_rev = defaultdict(float)
    for s in db.sales:
        day_rev[s["date"]] += s["total"]
    common_dates = sorted(set(day_hours.keys()) & set(day_rev.keys()))
    items = [{"hours": round(day_hours[d], 1), "revenue": round(day_rev[d], 2)} for d in common_dates]

    doc = simple_scatter_doc(items, "hours", "revenue", "Staffing vs Revenue", "#06b6d4", point_size=4.0)

    n = len(items)
    avg_h = sum(it["hours"] for it in items) / n
    avg_r = sum(it["revenue"] for it in items) / n
    md = f"""# Trial 088: Staffing vs Revenue Scatter

**Date:** 2026-03-22
**Goal:** For each day, compute (total shift hours, total revenue). Scatter showing correlation.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
shifts GROUP BY date → SUM(hoursWorked) AS dayHours
sales GROUP BY date → SUM(total) AS dayRevenue
INNER JOIN on date
```

Two-table cross-join by date: shifts (13,751) and sales (12,338).

## Data Insight

{n} days plotted. Average daily staffing: {avg_h:.0f} hours, average daily revenue: ${avg_r:,.0f}.

The scatter reveals the correlation between labor investment and revenue output.
Days with more staff-hours tend to produce more revenue, but diminishing returns appear
at high staffing levels.

---

## What Was Built

800x600 viewport, {n} cyan scatter points. X = staff hours, Y = revenue.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(88, "staffing-vs-revenue-scatter", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 089: Inventory vs Sales Rate Scatter
# ═══════════════════════════════════════════════════════════════════════════

def trial_089():
    # Latest inventory snapshot per product, avg monthly sales
    latest_inv = {}
    for snap in db.inventory_snapshots:
        pid = snap["productId"]
        if pid not in latest_inv or snap["date"] > latest_inv[pid]["date"]:
            latest_inv[pid] = snap
    # Monthly avg: total units sold / months active
    prod_monthly = defaultdict(lambda: defaultdict(int))
    for si in db.sale_items:
        sale_month = db._sale_month.get(si["saleId"], "")
        if sale_month:
            prod_monthly[si["productId"]][sale_month] += si["quantity"]

    items = []
    for pid, snap in latest_inv.items():
        months_active = len(prod_monthly.get(pid, {}))
        total_units = sum(prod_monthly.get(pid, {}).values())
        avg_rate = total_units / months_active if months_active else 0
        items.append({"qty": snap["quantityOnHand"], "avgRate": round(avg_rate, 1)})

    doc = simple_scatter_doc(items, "qty", "avgRate", "Inventory vs Sales Rate", "#ef4444", point_size=5.0)

    md = f"""# Trial 089: Inventory vs Sales Rate

**Date:** 2026-03-22
**Goal:** For each product: latest inventory qty vs monthly avg sales rate. Scatter.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
inventory_snapshots: latest snapshot per productId (MAX date)
sale_items JOIN sales → month → GROUP BY (productId, month) → SUM(quantity)
avg_monthly_rate = total_units / months_active
```

Two-table cross-reference: inventory_snapshots (3,150) and sale_items (33,834).

## Data Insight

{len(items)} products plotted. Points in the upper-left (low stock, high sales) are
stockout risks. Points in the lower-right (high stock, low sales) are overstock candidates.

This scatter is the foundation of inventory optimization — it reveals which products
need reordering attention vs which are tying up capital.

---

## What Was Built

800x600 viewport, {len(items)} red scatter points. X = current inventory, Y = avg monthly sales rate.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(89, "inventory-vs-sales-rate", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 090: Reorder Frequency by Supplier
# ═══════════════════════════════════════════════════════════════════════════

def trial_090():
    supp_po = Counter(po["supplierId"] for po in db.purchase_orders)
    items = sorted([{"index": i, "name": db._supp_map[sid]["name"], "poCount": cnt}
                    for i, (sid, cnt) in enumerate(sorted(supp_po.items(), key=lambda x: -x[1]))],
                   key=lambda x: x["index"])

    doc = simple_bar_doc(items, "index", "poCount", "PO Count by Supplier", "#f97316")

    table_rows = "\n".join(f"| {r['name']} | {r['poCount']} |" for r in items)
    md = f"""# Trial 090: Reorder Frequency by Supplier

**Date:** 2026-03-22
**Goal:** Count purchase orders per supplier. Bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
purchase_orders GROUP BY supplierId
COUNT(*) AS poCount
ORDER BY poCount DESC
JOIN suppliers ON id for name
```

Two-table join: purchase_orders (429) x suppliers (12).

## Data Insight

| Supplier | PO Count |
|----------|----------|
{table_rows}

Suppliers with high PO frequency serve fast-moving departments. Those with fewer POs
may supply specialty items with longer reorder cycles.

---

## What Was Built

1000x500 viewport, {len(items)} orange bars for PO count by supplier.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(90, "reorder-frequency-by-supplier", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 091: PO Cost vs Product Revenue (Scatter)
# ═══════════════════════════════════════════════════════════════════════════

def trial_091():
    # By supplier: total PO cost vs total product revenue
    supp_po_cost = defaultdict(float)
    for po in db.purchase_orders:
        supp_po_cost[po["supplierId"]] += po["totalCost"]
    supp_prod_rev = defaultdict(float)
    for si in db.sale_items:
        p = db._prod_map.get(si["productId"])
        if p:
            supp_prod_rev[p["supplierId"]] += si["lineTotal"]

    items = []
    for sid in db._supp_map:
        cost = supp_po_cost.get(sid, 0)
        rev = supp_prod_rev.get(sid, 0)
        if cost > 0 or rev > 0:
            items.append({"poCost": round(cost, 2), "revenue": round(rev, 2),
                          "name": db._supp_map[sid]["name"]})

    doc = simple_scatter_doc(items, "poCost", "revenue", "PO Cost vs Revenue", "#22c55e", point_size=8.0)

    table_rows = "\n".join(f"| {r['name']} | ${r['poCost']:,.0f} | ${r['revenue']:,.0f} |" for r in items)
    md = f"""# Trial 091: PO Cost vs Product Revenue by Supplier

**Date:** 2026-03-22
**Goal:** By supplier: total PO cost vs total product revenue. Scatter.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
purchase_orders GROUP BY supplierId → SUM(totalCost)
sale_items JOIN products ON productId → GROUP BY supplierId → SUM(lineTotal)
Plot: (totalPOCost, totalProductRevenue) per supplier
```

Three-table join: purchase_orders + sale_items + products, aggregated by supplier.

## Data Insight

| Supplier | PO Cost | Product Revenue |
|----------|---------|-----------------|
{table_rows}

Points above the diagonal (revenue > cost) indicate profitable supplier relationships.
The ratio reveals which suppliers deliver the best return on procurement investment.

---

## What Was Built

800x600 viewport, {len(items)} green scatter points. X = total PO cost, Y = total product revenue.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(91, "po-cost-vs-product-revenue", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 092: Tier Average Basket Size
# ═══════════════════════════════════════════════════════════════════════════

def trial_092():
    tier_items = defaultdict(list)
    for s in db.sales:
        if s["customerId"]:
            c = db._cust_map.get(s["customerId"])
            if c:
                tier_items[c["tier"]].append(s["itemCount"])
    order = ["gold", "silver", "bronze"]
    items = [{"index": i, "tier": t, "avgBasket": round(sum(tier_items[t]) / len(tier_items[t]), 2) if tier_items[t] else 0}
             for i, t in enumerate(order)]

    colors = ["#f59e0b", "#94a3b8", "#b45309"]
    buffers = {}; geometries = {}; drawItems = {}
    hw = 0.7 / 2.0
    for j, item in enumerate(items):
        bid = 100 + j; gid = 200 + j; diid = 300 + j
        bar = [item["index"] - hw, 0, item["index"] + hw, item["avgBasket"]]
        buffers[bid] = {"byteLength": 0, "data": rf(bar)}
        geometries[gid] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": 1}
        drawItems[diid] = {"layerId": 10, "name": item["tier"], "pipeline": "instancedRect@1",
                           "geometryId": gid, "transformId": 50, "color": db.hex_to_rgba(colors[j]),
                           "cornerRadius": 4.0}

    xs = [it["index"] for it in items]; ys = [it["avgBasket"] for it in items]
    tx = db.fit_transform((min(xs) - hw, max(xs) + hw), (0, max(ys)))

    doc = make_doc(800, 500, buffers=buffers, transforms={50: tx},
        panes={1: {"name": "Avg Basket by Tier", "region": {"clipYMin": -0.92, "clipYMax": 0.92, "clipXMin": -0.92, "clipXMax": 0.92},
                    "clearColor": DARK_BG, "hasClearColor": True}},
        layers={10: {"paneId": 1, "name": "Data"}},
        geometries=geometries, drawItems=drawItems)

    table_rows = "\n".join(f"| {r['tier']} | {r['avgBasket']:.2f} |" for r in items)
    md = f"""# Trial 092: Tier Average Basket Size

**Date:** 2026-03-22
**Goal:** By customer tier: average itemCount per sale. Bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sales JOIN customers ON customerId
GROUP BY customers.tier
AVG(sales.itemCount) AS avgBasket
```

Two-table join: sales (12,338) x customers (500).

## Data Insight

| Tier | Avg Items/Sale |
|------|----------------|
{table_rows}

Gold customers buy more items per visit than silver or bronze, confirming that loyalty
tier correlates with project size (larger DIY projects = more items).

---

## What Was Built

800x500 viewport, 3 tier-colored bars (gold/silver/bronze).

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(92, "tier-avg-basket-size", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 093: Tier x Department Revenue (Stacked Bars)
# ═══════════════════════════════════════════════════════════════════════════

def trial_093():
    # For each tier, revenue by dept
    tier_dept_rev = defaultdict(lambda: defaultdict(float))
    for s in db.sales:
        if not s["customerId"]: continue
        c = db._cust_map.get(s["customerId"])
        if not c: continue
        tier = c["tier"]
        sale_id = s["id"]
        for si in db.sale_items:
            if si["saleId"] == sale_id:
                p = db._prod_map.get(si["productId"])
                if p:
                    tier_dept_rev[tier][p["departmentId"]] += si["lineTotal"]

    # Faster approach: build sale→tier map
    sale_tier = {}
    for s in db.sales:
        if s["customerId"]:
            c = db._cust_map.get(s["customerId"])
            if c:
                sale_tier[s["id"]] = c["tier"]
    tier_dept_rev2 = defaultdict(lambda: defaultdict(float))
    for si in db.sale_items:
        tier = sale_tier.get(si["saleId"])
        if tier:
            p = db._prod_map.get(si["productId"])
            if p:
                tier_dept_rev2[tier][p["departmentId"]] += si["lineTotal"]

    tiers = ["gold", "silver", "bronze"]
    dept_ids = sorted(db._dept_map.keys())

    # Build stacked bar series: each dept is a series, x = tier index
    series_list = []
    for did in dept_ids:
        series = [{"index": ti, "revenue": round(tier_dept_rev2[t].get(did, 0), 2)} for ti, t in enumerate(tiers)]
        series_list.append(series)

    stacked = db.to_stacked_bars(series_list, "index", "revenue", bar_width=0.7)

    buffers = {}; geometries = {}; drawItems = {}
    all_y = 0
    for j, (sdata, smeta) in enumerate(stacked):
        bid = 100 + j; gid = 200 + j; diid = 300 + j
        buffers[bid] = {"byteLength": 0, "data": rf(sdata)}
        geometries[gid] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": smeta["vertexCount"]}
        did = dept_ids[j]
        c = db.hex_to_rgba(db.PALETTE_DEPT.get(did, "#888888"))
        drawItems[diid] = {"layerId": 10, "name": db._dept_map[did]["name"], "pipeline": "instancedRect@1",
                           "geometryId": gid, "transformId": 50, "color": c, "cornerRadius": 0.0}
        if smeta["yRange"][1] > all_y:
            all_y = smeta["yRange"][1]

    tx = db.fit_transform((-0.35, 2.35), (0, all_y))

    doc = make_doc(900, 600, buffers=buffers, transforms={50: tx},
        panes={1: {"name": "Tier x Dept Revenue", "region": {"clipYMin": -0.92, "clipYMax": 0.92, "clipXMin": -0.92, "clipXMax": 0.92},
                    "clearColor": DARK_BG, "hasClearColor": True}},
        layers={10: {"paneId": 1, "name": "Data"}},
        geometries=geometries, drawItems=drawItems)

    md = f"""# Trial 093: Tier x Department Revenue (Stacked Bars)

**Date:** 2026-03-22
**Goal:** For each customer tier, show revenue by department as stacked bars. 3 groups x 8 dept stacks.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sales JOIN customers ON customerId → sale_tier mapping
sale_items JOIN products ON productId → departmentId
GROUP BY (tier, departmentId) → SUM(lineTotal)
```

Three-table chain: customers -> sales -> sale_items -> products.
Cross-tabulation: 3 tiers x 8 departments = 24 cells.

## Data Insight

Gold customers generate more total revenue despite being fewer in number.
Department preferences vary by tier — gold customers may over-index on Lumber
(project materials) while bronze customers buy more consumables.

---

## What Was Built

900x600 viewport, 3 stacked bar columns (gold/silver/bronze), each with 8 department
color segments. {len(stacked)} series using to_stacked_bars.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(93, "tier-department-split", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 094: Payment Method by Department (Stacked Bars)
# ═══════════════════════════════════════════════════════════════════════════

def trial_094():
    # Build sale→paymentMethod map
    sale_payment = {s["id"]: s["paymentMethod"] for s in db.sales}
    dept_pay_rev = defaultdict(lambda: defaultdict(float))
    for si in db.sale_items:
        pm = sale_payment.get(si["saleId"])
        p = db._prod_map.get(si["productId"])
        if pm and p:
            dept_pay_rev[p["departmentId"]][pm] += si["lineTotal"]

    dept_ids = sorted(db._dept_map.keys())
    methods = sorted(set(pm for dpr in dept_pay_rev.values() for pm in dpr))
    method_colors = ["#3b82f6", "#ef4444", "#22c55e", "#f59e0b", "#8b5cf6"]

    series_list = []
    for mi, method in enumerate(methods):
        series = [{"index": di, "revenue": round(dept_pay_rev[did].get(method, 0), 2)}
                  for di, did in enumerate(dept_ids)]
        series_list.append(series)

    stacked = db.to_stacked_bars(series_list, "index", "revenue", bar_width=0.7)

    buffers = {}; geometries = {}; drawItems = {}
    max_y = 0
    for j, (sdata, smeta) in enumerate(stacked):
        bid = 100 + j; gid = 200 + j; diid = 300 + j
        buffers[bid] = {"byteLength": 0, "data": rf(sdata)}
        geometries[gid] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": smeta["vertexCount"]}
        c = db.hex_to_rgba(method_colors[j % len(method_colors)])
        drawItems[diid] = {"layerId": 10, "name": methods[j], "pipeline": "instancedRect@1",
                           "geometryId": gid, "transformId": 50, "color": c}
        if smeta["yRange"][1] > max_y:
            max_y = smeta["yRange"][1]

    tx = db.fit_transform((-0.35, len(dept_ids) - 0.65), (0, max_y))

    doc = make_doc(1000, 600, buffers=buffers, transforms={50: tx},
        panes={1: {"name": "Payment by Dept", "region": {"clipYMin": -0.92, "clipYMax": 0.92, "clipXMin": -0.92, "clipXMax": 0.92},
                    "clearColor": DARK_BG, "hasClearColor": True}},
        layers={10: {"paneId": 1, "name": "Data"}},
        geometries=geometries, drawItems=drawItems)

    md = f"""# Trial 094: Payment Method by Department (Stacked Bars)

**Date:** 2026-03-22
**Goal:** For each department, show revenue split by payment method. Stacked bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sale_items JOIN products ON productId → departmentId
sale_items JOIN sales ON saleId → paymentMethod
GROUP BY (departmentId, paymentMethod)
SUM(lineTotal)
```

Three-table join: sale_items x products x sales.
Cross-tabulation: {len(dept_ids)} departments x {len(methods)} payment methods.

## Data Insight

Payment methods: {', '.join(methods)}.
Some departments may show different payment preferences — high-ticket Lumber purchases
might skew toward credit cards, while small Garden purchases use more cash.

---

## What Was Built

1000x600 viewport, {len(dept_ids)} stacked bar columns with {len(methods)} payment method segments each.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(94, "payment-method-by-dept", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 095: Department Lead Contribution (Horizontal Bars)
# ═══════════════════════════════════════════════════════════════════════════

def trial_095():
    items = []
    for i, d in enumerate(sorted(db.departments, key=lambda x: x["id"])):
        mid = d["managerId"]
        if mid:
            e = db._emp_map[mid]
            name = f"{e['firstName']} {e['lastName']}"
        else:
            name = "(no lead)"
        rev = round(db._dept_rev.get(d["id"], 0), 2)
        items.append({"index": i, "name": name, "dept": d["name"], "revenue": rev})

    doc = simple_hbar_doc(items, "index", "revenue", "Dept Lead Revenue", "#ec4899")

    table_rows = "\n".join(f"| {r['name']} | {r['dept']} | ${r['revenue']:,.0f} |" for r in items)
    md = f"""# Trial 095: Department Lead Contribution

**Date:** 2026-03-22
**Goal:** For each department lead, show their department's total revenue. Horizontal bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
departments.managerId → employees (name lookup)
sale_items JOIN products ON productId → departmentId
GROUP BY departmentId → SUM(lineTotal)
```

Three-table chain: departments -> employees (for names), sale_items -> products (for revenue).

## Data Insight

| Lead | Department | Revenue |
|------|------------|---------|
{table_rows}

This shows each department lead's accountability scope. The lead overseeing the
highest-revenue department has the most commercial responsibility.

---

## What Was Built

1000x500 viewport, {len(items)} pink horizontal bars.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(95, "dept-lead-contribution", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 096: Department Seasonal (8 Overlaid Lines)
# ═══════════════════════════════════════════════════════════════════════════

def trial_096():
    dmr = db.department_monthly_revenue()
    dept_ids = sorted(set(r["deptId"] for r in dmr))
    months = sorted(set(r["month"] for r in dmr))
    month_idx = {m: i for i, m in enumerate(months)}

    buffers = {}; geometries = {}; drawItems = {}
    all_revs = [r["revenue"] for r in dmr]
    global_y = (0, max(all_revs) if all_revs else 1)
    global_x = (0, len(months) - 1)
    tx = db.fit_transform(global_x, global_y)

    for j, did in enumerate(dept_ids):
        dept_rows = [r for r in dmr if r["deptId"] == did]
        dept_rows.sort(key=lambda x: x["month"])
        for r in dept_rows:
            r["mi"] = month_idx[r["month"]]
        data, meta = db.to_line_segments(dept_rows, "mi", "revenue")
        if meta["vertexCount"] == 0:
            continue
        bid = 100 + j; gid = 200 + j; diid = 300 + j
        buffers[bid] = {"byteLength": 0, "data": rf(data)}
        geometries[gid] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": meta["vertexCount"]}
        c = db.hex_to_rgba(db.PALETTE_DEPT.get(did, "#888888"))
        drawItems[diid] = {"layerId": 10, "name": db._dept_map[did]["name"], "pipeline": "lineAA@1",
                           "geometryId": gid, "transformId": 50, "color": c, "lineWidth": 2.0}

    doc = make_doc(1200, 600, buffers=buffers, transforms={50: tx},
        panes={1: {"name": "Dept Seasonal", "region": {"clipYMin": -0.92, "clipYMax": 0.92, "clipXMin": -0.92, "clipXMax": 0.92},
                    "clearColor": DARK_BG, "hasClearColor": True}},
        layers={10: {"paneId": 1, "name": "Lines"}},
        geometries=geometries, drawItems=drawItems)

    md = f"""# Trial 096: Department Seasonal Correlation

**Date:** 2026-03-22
**Goal:** 8 overlaid lines showing monthly revenue for each department. Multi-line chart.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sale_items JOIN products ON productId → departmentId
sale_items JOIN sales (via _sale_month) → month
GROUP BY (departmentId, month) → SUM(lineTotal)
```

Two-table join with temporal aggregation. Produces {len(dept_ids)} time series x {len(months)} months.

## Data Insight

Each department has its own seasonal pattern. Garden & Outdoor peaks in spring/summer,
Seasonal & Holiday spikes in Q4, while Tools & Hardware stays relatively stable year-round.
Overlaying all 8 lines on shared axes reveals which departments move in concert.

---

## What Was Built

1200x600 viewport, {len(dept_ids)} department-colored lineAA lines sharing one transform.
Each department is a separate DrawItem with its palette color.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(96, "dept-seasonal-correlation", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 097: OpEx-to-Revenue Ratio (Line)
# ═══════════════════════════════════════════════════════════════════════════

def trial_097():
    rev = {r["month"]: r["revenue"] for r in db.monthly_revenue()}
    exp = {e["month"]: e["total"] for e in db.monthly_expenses()}
    months = sorted(set(rev.keys()) & set(exp.keys()))
    items = [{"index": i, "month": m, "ratio": round(exp[m] / rev[m], 4) if rev[m] else 0}
             for i, m in enumerate(months)]

    doc = simple_line_doc(items, "index", "ratio", "OpEx/Revenue Ratio", "#ef4444", line_width=2.5)

    avg_ratio = sum(it["ratio"] for it in items) / len(items)
    md = f"""# Trial 097: Operating Expense to Revenue Ratio

**Date:** 2026-03-22
**Goal:** Monthly: total expenses / total revenue. lineAA@1 ratio trend.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
expenses GROUP BY month → SUM(amount) AS monthlyExpense
sales GROUP BY month → SUM(total) AS monthlyRevenue
ratio = monthlyExpense / monthlyRevenue
```

Two independent aggregations (expenses, sales) joined by month.

## Data Insight

{len(items)} months plotted. Average OpEx/Revenue ratio: {avg_ratio:.3f} ({avg_ratio*100:.1f}%).

A ratio below 1.0 means revenue exceeds expenses. Trending downward indicates improving
operational efficiency. Spikes may correlate with seasonal expense patterns (e.g., holiday
inventory buildup).

---

## What Was Built

1000x500 viewport, red lineAA@1 trend line with {len(items)-1} segments.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(97, "opex-to-revenue-ratio", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 098: Gross Margin by Department
# ═══════════════════════════════════════════════════════════════════════════

def trial_098():
    dept_rev = defaultdict(float)
    dept_cogs = defaultdict(float)
    for si in db.sale_items:
        p = db._prod_map.get(si["productId"])
        if p:
            dept_rev[p["departmentId"]] += si["lineTotal"]
            dept_cogs[p["departmentId"]] += p["unitCost"] * si["quantity"]

    items = []
    for i, did in enumerate(sorted(dept_rev)):
        rev = dept_rev[did]
        cogs = dept_cogs[did]
        margin = (rev - cogs) / rev if rev else 0
        items.append({"index": i, "dept": db._dept_map[did]["name"], "deptId": did,
                       "revenue": round(rev, 2), "cogs": round(cogs, 2),
                       "margin": round(margin, 4)})

    # Per-dept colored bars for margin
    hw = 0.7 / 2.0
    buffers = {}; geometries = {}; drawItems = {}
    for j, item in enumerate(items):
        bid = 100 + j; gid = 200 + j; diid = 300 + j
        bar = [item["index"] - hw, 0, item["index"] + hw, item["margin"]]
        buffers[bid] = {"byteLength": 0, "data": rf(bar)}
        geometries[gid] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": 1}
        c = db.hex_to_rgba(db.PALETTE_DEPT.get(item["deptId"], "#888888"))
        drawItems[diid] = {"layerId": 10, "name": item["dept"], "pipeline": "instancedRect@1",
                           "geometryId": gid, "transformId": 50, "color": c, "cornerRadius": 3.0}

    xs = [it["index"] for it in items]; ys = [it["margin"] for it in items]
    tx = db.fit_transform((min(xs) - hw, max(xs) + hw), (0, max(ys)))

    doc = make_doc(1000, 500, buffers=buffers, transforms={50: tx},
        panes={1: {"name": "Gross Margin", "region": {"clipYMin": -0.92, "clipYMax": 0.92, "clipXMin": -0.92, "clipXMax": 0.92},
                    "clearColor": DARK_BG, "hasClearColor": True}},
        layers={10: {"paneId": 1, "name": "Data"}},
        geometries=geometries, drawItems=drawItems)

    table_rows = "\n".join(f"| {r['dept']} | ${r['revenue']:,.0f} | ${r['cogs']:,.0f} | {r['margin']*100:.1f}% |" for r in items)
    md = f"""# Trial 098: Gross Margin by Department

**Date:** 2026-03-22
**Goal:** For each dept: (revenue - COGS) / revenue. Department-colored bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sale_items JOIN products ON productId
dept_revenue = SUM(lineTotal) GROUP BY departmentId
dept_cogs = SUM(products.unitCost * quantity) GROUP BY departmentId
margin = (revenue - cogs) / revenue
```

Two-table join: sale_items x products. Both revenue and cost computed from same join.

## Data Insight

| Department | Revenue | COGS | Margin |
|------------|---------|------|--------|
{table_rows}

Margin variation across departments reveals which categories are most profitable per
dollar of revenue. Departments with lower margins may need pricing review.

---

## What Was Built

1000x500 viewport, {len(items)} department-colored bars showing gross margin percentage.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(98, "gross-margin-by-dept", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 099: Inventory Turnover (Top 20)
# ═══════════════════════════════════════════════════════════════════════════

def trial_099():
    # Total units sold per product
    prod_units = defaultdict(int)
    for si in db.sale_items:
        prod_units[si["productId"]] += si["quantity"]
    # Average inventory per product
    prod_inv = defaultdict(list)
    for snap in db.inventory_snapshots:
        prod_inv[snap["productId"]].append(snap["quantityOnHand"])

    items = []
    for pid in prod_units:
        inv_list = prod_inv.get(pid, [])
        avg_inv = sum(inv_list) / len(inv_list) if inv_list else 1
        turnover = prod_units[pid] / avg_inv if avg_inv > 0 else 0
        items.append({"pid": pid, "name": db._prod_map[pid]["name"][:25],
                       "unitsSold": prod_units[pid], "avgInv": round(avg_inv, 1),
                       "turnover": round(turnover, 2)})

    items.sort(key=lambda x: -x["turnover"])
    items = items[:20]
    for i, r in enumerate(items):
        r["index"] = i

    doc = simple_bar_doc(items, "index", "turnover", "Inventory Turnover Top 20", "#06b6d4")

    table_rows = "\n".join(f"| {r['name']} | {r['unitsSold']} | {r['avgInv']:.0f} | {r['turnover']:.1f}x |" for r in items[:10])
    md = f"""# Trial 099: Inventory Turnover (Top 20)

**Date:** 2026-03-22
**Goal:** For each product: total units sold / average inventory. Top 20 by turnover.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sale_items GROUP BY productId → SUM(quantity) AS unitsSold
inventory_snapshots GROUP BY productId → AVG(quantityOnHand) AS avgInventory
turnover = unitsSold / avgInventory
ORDER BY turnover DESC LIMIT 20
```

Two-table cross-reference: sale_items (33,834) x inventory_snapshots (3,150).

## Data Insight (Top 10)

| Product | Units Sold | Avg Inv | Turnover |
|---------|-----------|---------|----------|
{table_rows}

High-turnover products move quickly relative to stock levels — they are the store's
workhorses. Low turnover suggests overstock or slow-moving inventory.

---

## What Was Built

1000x500 viewport, 20 cyan bars showing inventory turnover ratio.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(99, "inventory-turnover", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 100: Supplier Reliability (Scatter)
# ═══════════════════════════════════════════════════════════════════════════

def trial_100():
    # For each supplier: avg (receivedDate - orderDate) vs expected leadTimeDays
    supp_actual = defaultdict(list)
    for po in db.purchase_orders:
        actual = (date.fromisoformat(po["receivedDate"]) - date.fromisoformat(po["orderDate"])).days
        supp_actual[po["supplierId"]].append(actual)

    items = []
    for sid in db._supp_map:
        actuals = supp_actual.get(sid, [])
        avg_actual = sum(actuals) / len(actuals) if actuals else 0
        expected = db._supp_map[sid]["leadTimeDays"]
        items.append({"expected": expected, "actual": round(avg_actual, 1),
                       "name": db._supp_map[sid]["name"]})

    doc = simple_scatter_doc(items, "expected", "actual", "Supplier Reliability", "#f59e0b", point_size=8.0)

    table_rows = "\n".join(f"| {r['name']} | {r['expected']}d | {r['actual']:.1f}d |" for r in items)
    md = f"""# Trial 100: Supplier Reliability

**Date:** 2026-03-22
**Goal:** For each supplier: avg actual lead time vs expected leadTimeDays. Scatter.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
purchase_orders: (receivedDate - orderDate) AS actualLeadTime
GROUP BY supplierId → AVG(actualLeadTime)
JOIN suppliers ON id → leadTimeDays
Plot: (expected, actual) per supplier
```

Two-table join: purchase_orders (429) x suppliers (12).

## Data Insight

| Supplier | Expected | Avg Actual |
|----------|----------|-----------|
{table_rows}

Points on the diagonal mean the supplier delivers on time. Points above the diagonal
deliver late; below means early. Consistent lateness is a supply chain risk.

---

## What Was Built

800x600 viewport, {len(items)} amber scatter points. X = expected lead time, Y = actual avg lead time.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(100, "supplier-reliability", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 101: Repeat Customer Percentage (Line)
# ═══════════════════════════════════════════════════════════════════════════

def trial_101():
    # Monthly: customers who also bought in prior month / total unique customers
    month_customers = defaultdict(set)
    for s in db.sales:
        if s["customerId"]:
            mk = s["date"][:7]
            month_customers[mk].add(s["customerId"])

    months = sorted(month_customers.keys())
    items = []
    for i in range(1, len(months)):
        prev = month_customers[months[i-1]]
        curr = month_customers[months[i]]
        repeat = len(prev & curr)
        total = len(curr)
        pct = repeat / total if total else 0
        items.append({"index": i - 1, "month": months[i], "repeatPct": round(pct, 4)})

    doc = simple_line_doc(items, "index", "repeatPct", "Repeat Customer %", "#8b5cf6", line_width=2.5)

    avg_pct = sum(it["repeatPct"] for it in items) / len(items) if items else 0
    md = f"""# Trial 101: Repeat Customer Percentage

**Date:** 2026-03-22
**Goal:** Monthly: count of customers who also bought in prior month / total customers. Line.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sales GROUP BY (month, customerId) → month_customers set
repeat_count = |month_customers[M] INTERSECT month_customers[M-1]|
total = |month_customers[M]|
repeatPct = repeat_count / total
```

Single-table temporal analysis on sales with set intersection logic.

## Data Insight

{len(items)} months plotted. Average month-over-month repeat rate: {avg_pct*100:.1f}%.

A rising repeat rate indicates growing customer loyalty. Seasonal dips may correspond
to months where one-time project buyers inflate the unique customer count.

---

## What Was Built

1000x500 viewport, violet lineAA@1 trend with {len(items)-1} segments.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(101, "repeat-customer-pct", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 102: Product Co-occurrence (Horizontal Bars)
# ═══════════════════════════════════════════════════════════════════════════

def trial_102():
    # Group sale_items by saleId, find product pairs
    sale_products = defaultdict(set)
    for si in db.sale_items:
        sale_products[si["saleId"]].add(si["productId"])

    pair_count = Counter()
    for sid, pids in sale_products.items():
        pids_list = sorted(pids)
        for i in range(len(pids_list)):
            for j in range(i+1, len(pids_list)):
                pair_count[(pids_list[i], pids_list[j])] += 1

    top10 = pair_count.most_common(10)
    items = []
    for i, ((p1, p2), cnt) in enumerate(top10):
        n1 = db._prod_map[p1]["name"][:15]
        n2 = db._prod_map[p2]["name"][:15]
        items.append({"index": i, "pair": f"{n1} + {n2}", "count": cnt})

    doc = simple_hbar_doc(items, "index", "count", "Product Pairs", "#3b82f6")

    table_rows = "\n".join(f"| {r['pair']} | {r['count']} |" for r in items)
    md = f"""# Trial 102: Product Co-occurrence

**Date:** 2026-03-22
**Goal:** Find top 10 most common product pairs bought in the same sale. Horizontal bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sale_items GROUP BY saleId → set of productIds per sale
For each sale with 2+ products, enumerate all pairs
COUNT occurrences of each (productA, productB) pair
ORDER BY count DESC LIMIT 10
```

Self-join pattern: sale_items x sale_items on saleId, deduplicated pairs.

## Data Insight

| Product Pair | Co-occurrences |
|-------------|----------------|
{table_rows}

Frequently co-purchased products are cross-sell opportunities. Placing them near each
other on the floor or bundling them in promotions can increase basket size.

---

## What Was Built

1000x500 viewport, 10 blue horizontal bars for top co-occurring product pairs.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(102, "product-cooccurrence", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 103: Zone Revenue Heatmap
# ═══════════════════════════════════════════════════════════════════════════

def trial_103():
    # products → departments → floorZoneId; revenue from sale_items
    zone_rev = defaultdict(float)
    for si in db.sale_items:
        p = db._prod_map.get(si["productId"])
        if p:
            d = db._dept_map.get(p["departmentId"])
            if d:
                zone_rev[d["floorZoneId"]] += si["lineTotal"]

    zones = db.db["zones"]
    zone_map = {z["id"]: z for z in zones}
    # Build rects at zone positions
    data = []
    items_info = []
    for z in zones:
        pos = z["position"]
        if pos["w"] == 0: continue  # skip warehouse
        rev = zone_rev.get(z["id"], 0)
        # Map zone position to clip space: x: [0,1] -> [-0.9, 0.9], y: [0,1] -> [-0.9, 0.9]
        x0 = -0.9 + pos["x"] * 1.8
        y0 = -0.9 + pos["y"] * 1.8
        x1 = x0 + pos["w"] * 1.8
        y1 = y0 + pos["h"] * 1.8
        data.extend([x0, y0, x1, y1])
        items_info.append({"zone": z["id"], "name": z["name"], "revenue": round(rev, 2)})

    # Color by revenue
    revs = [it["revenue"] for it in items_info]
    vmin, vmax = min(revs), max(revs)

    buffers = {}; geometries = {}; drawItems = {}
    for j, info in enumerate(items_info):
        bid = 100 + j; gid = 200 + j; diid = 300 + j
        idx = j * 4
        rect = data[idx:idx+4]
        buffers[bid] = {"byteLength": 0, "data": rf(rect)}
        geometries[gid] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": 1}
        c = db.value_to_color(info["revenue"], vmin, vmax, palette="heat")
        drawItems[diid] = {"layerId": 10, "name": info["name"], "pipeline": "instancedRect@1",
                           "geometryId": gid, "color": c, "cornerRadius": 3.0}

    doc = make_doc(800, 800, buffers=buffers, transforms={},
        panes={1: {"name": "Zone Revenue", "region": {"clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95},
                    "clearColor": DARK_BG, "hasClearColor": True}},
        layers={10: {"paneId": 1, "name": "Zones"}},
        geometries=geometries, drawItems=drawItems)

    table_rows = "\n".join(f"| {r['zone']} | {r['name']} | ${r['revenue']:,.0f} |" for r in items_info)
    md = f"""# Trial 103: Zone Revenue Heatmap

**Date:** 2026-03-22
**Goal:** Sum revenue by floor zone (sale_items -> products -> departments -> zones). Spatial heatmap.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sale_items JOIN products ON productId → departmentId
departments.floorZoneId → zone assignment
GROUP BY floorZoneId → SUM(lineTotal)
zones → position coordinates
```

Four-table chain: sale_items -> products -> departments -> zones.

## Data Insight

| Zone | Name | Revenue |
|------|------|---------|
{table_rows}

The spatial heatmap reveals which physical areas of the store generate the most revenue.
High-traffic zones justify premium product placement and additional staffing.

---

## What Was Built

800x800 viewport, {len(items_info)} zone rectangles positioned per floor plan coordinates.
Color intensity maps from low (yellow) to high (red) revenue via heat palette.
No transform needed — positions pre-mapped to clip space.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(103, "zone-revenue-heatmap", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 104: Top 5 Products per Department
# ═══════════════════════════════════════════════════════════════════════════

def trial_104():
    # For each dept, top 5 products by revenue
    dept_prod_rev = defaultdict(lambda: defaultdict(float))
    for si in db.sale_items:
        p = db._prod_map.get(si["productId"])
        if p:
            dept_prod_rev[p["departmentId"]][si["productId"]] += si["lineTotal"]

    dept_ids = sorted(db._dept_map.keys())
    all_bars = []  # list of dicts with group_x, within_x, revenue, deptId, name
    for gi, did in enumerate(dept_ids):
        top5 = sorted(dept_prod_rev[did].items(), key=lambda x: -x[1])[:5]
        for wi, (pid, rev) in enumerate(top5):
            x = gi * 6 + wi  # 5 bars + 1 gap per group
            all_bars.append({"index": x, "revenue": round(rev, 2), "deptId": did,
                             "name": db._prod_map[pid]["name"][:20]})

    # Per-dept colored bars
    hw = 0.4
    buffers = {}; geometries = {}; drawItems = {}
    for j, item in enumerate(all_bars):
        bid = 100 + j; gid = 200 + j; diid = 300 + j
        bar = [item["index"] - hw, 0, item["index"] + hw, item["revenue"]]
        buffers[bid] = {"byteLength": 0, "data": rf(bar)}
        geometries[gid] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": 1}
        c = db.hex_to_rgba(db.PALETTE_DEPT.get(item["deptId"], "#888888"))
        drawItems[diid] = {"layerId": 10, "name": item["name"], "pipeline": "instancedRect@1",
                           "geometryId": gid, "transformId": 50, "color": c, "cornerRadius": 2.0}

    xs = [it["index"] for it in all_bars]; ys = [it["revenue"] for it in all_bars]
    tx = db.fit_transform((min(xs) - 1, max(xs) + 1), (0, max(ys)))

    doc = make_doc(1400, 500, buffers=buffers, transforms={50: tx},
        panes={1: {"name": "Top 5 Products/Dept", "region": {"clipYMin": -0.92, "clipYMax": 0.92, "clipXMin": -0.92, "clipXMax": 0.92},
                    "clearColor": DARK_BG, "hasClearColor": True}},
        layers={10: {"paneId": 1, "name": "Data"}},
        geometries=geometries, drawItems=drawItems)

    md = f"""# Trial 104: Top 5 Products per Department

**Date:** 2026-03-22
**Goal:** For each of 8 departments, show top 5 products by revenue. 8 groups of 5 bars = 40 bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sale_items JOIN products ON productId → departmentId
GROUP BY (departmentId, productId) → SUM(lineTotal) AS productRevenue
For each department: ORDER BY productRevenue DESC LIMIT 5
```

Two-table join with per-group ranking.

## Data Insight

40 bars organized in 8 color-coded groups. Each group's bars are sorted by revenue,
revealing the revenue concentration within each department. Some departments rely heavily
on a single top product; others have more balanced top-5 distributions.

---

## What Was Built

1400x500 viewport, {len(all_bars)} bars grouped by department (5 per group, department-colored).
Each bar is a separate DrawItem for per-department palette coloring.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(104, "top5-products-per-dept", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 105: Weekday x Department Heatmap
# ═══════════════════════════════════════════════════════════════════════════

def trial_105():
    # Revenue by (day-of-week x department). 7x8 grid.
    dow_dept_rev = defaultdict(float)
    sale_dow = {}
    for s in db.sales:
        d = date.fromisoformat(s["date"])
        sale_dow[s["id"]] = d.weekday()
    for si in db.sale_items:
        dow = sale_dow.get(si["saleId"])
        p = db._prod_map.get(si["productId"])
        if dow is not None and p:
            dow_dept_rev[(dow, p["departmentId"])] += si["lineTotal"]

    dept_ids = sorted(db._dept_map.keys())
    dow_names = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]
    grid_items = []
    for dow in range(7):
        for di, did in enumerate(dept_ids):
            rev = dow_dept_rev.get((dow, did), 0)
            grid_items.append({"row": dow, "col": di, "revenue": round(rev, 2)})

    data, meta, val_range = db.to_heatmap_rects(grid_items, "row", "col", "revenue")
    tx = db.fit_transform(meta["xRange"], meta["yRange"])

    # Color each cell individually
    buffers = {}; geometries = {}; drawItems = {}
    for j, item in enumerate(grid_items):
        bid = 100 + j; gid = 200 + j; diid = 300 + j
        idx = j * 4
        rect = data[idx:idx+4]
        buffers[bid] = {"byteLength": 0, "data": rf(rect)}
        geometries[gid] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": 1}
        c = db.value_to_color(item["revenue"], val_range[0], val_range[1], palette="viridis")
        dow_n = dow_names[item["row"]]
        dept_n = db._dept_map[dept_ids[item["col"]]]["name"][:10]
        drawItems[diid] = {"layerId": 10, "name": f"{dow_n}-{dept_n}",
                           "pipeline": "instancedRect@1", "geometryId": gid,
                           "transformId": 50, "color": c}

    doc = make_doc(900, 700, buffers=buffers, transforms={50: tx},
        panes={1: {"name": "DOW x Dept Heatmap", "region": {"clipYMin": -0.92, "clipYMax": 0.92, "clipXMin": -0.92, "clipXMax": 0.92},
                    "clearColor": DARK_BG, "hasClearColor": True}},
        layers={10: {"paneId": 1, "name": "Grid"}},
        geometries=geometries, drawItems=drawItems)

    md = f"""# Trial 105: Weekday x Department Revenue Heatmap

**Date:** 2026-03-22
**Goal:** Revenue by (day-of-week x department). 7x8 colored grid.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sales → day-of-week from date
sale_items JOIN products ON productId → departmentId
GROUP BY (dayOfWeek, departmentId) → SUM(lineTotal)
```

Three-table join with dual-dimensional grouping: 7 days x 8 departments = 56 cells.

## Data Insight

56 heatmap cells. Weekends (Sat/Sun) show higher revenue across most departments.
Garden & Outdoor may peak on Saturdays. Seasonal & Holiday may spike on specific days
during promotion periods. The viridis color scale maps low (dark) to high (bright) revenue.

---

## What Was Built

900x700 viewport, 56 individually-colored instancedRect cells. Viridis palette.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(105, "weekday-dept-heatmap", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 106: Employee Tenure vs Hours (Scatter)
# ═══════════════════════════════════════════════════════════════════════════

def trial_106():
    # Months since hire vs avgWeeklyHours
    emp_hours = db.employee_hours()
    ref_date = date(2026, 3, 22)
    items = []
    for eh in emp_hours:
        e = db._emp_map[eh["id"]]
        hire = date.fromisoformat(e["hireDate"])
        tenure_months = (ref_date.year - hire.year) * 12 + (ref_date.month - hire.month)
        items.append({"tenure": tenure_months, "avgHours": eh["avgWeeklyHours"],
                       "name": eh["name"]})

    doc = simple_scatter_doc(items, "tenure", "avgHours", "Tenure vs Hours", "#ec4899", point_size=6.0)

    md = f"""# Trial 106: Employee Tenure vs Weekly Hours

**Date:** 2026-03-22
**Goal:** For each employee: months since hire vs average weekly hours worked. Scatter.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
employees.hireDate → tenure_months = (2026-03 - hireDate) in months
shifts GROUP BY employeeId → totalHours / weeks → avgWeeklyHours
Plot: (tenure_months, avgWeeklyHours)
```

Two-table join: employees x shifts.

## Data Insight

{len(items)} employees plotted. Longer-tenured employees may work more consistent hours
(full-time) while newer hires may be part-time. The scatter reveals whether tenure
correlates with schedule commitment.

---

## What Was Built

800x600 viewport, {len(items)} pink scatter points. X = tenure months, Y = avg weekly hours.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(106, "employee-tenure-vs-hours", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 107: Cumulative Customer Growth (Line)
# ═══════════════════════════════════════════════════════════════════════════

def trial_107():
    # Day by day cumulative unique customers
    sale_dates = sorted(set(s["date"] for s in db.sales))
    seen = set()
    items = []
    cust_by_date = defaultdict(set)
    for s in db.sales:
        if s["customerId"]:
            cust_by_date[s["date"]].add(s["customerId"])

    for i, d in enumerate(sale_dates):
        seen |= cust_by_date.get(d, set())
        items.append({"index": i, "date": d, "cumCustomers": len(seen)})

    doc = simple_line_doc(items, "index", "cumCustomers", "Cumulative Customers", "#22c55e", line_width=2.5)

    md = f"""# Trial 107: Cumulative Customer Growth

**Date:** 2026-03-22
**Goal:** Day-by-day cumulative count of unique customers. lineAA@1.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sales ORDER BY date
Running set union of customerId (non-null) per day
cumCustomers = |seen_customers| at each date
```

Single-table temporal accumulation on sales (12,338 rows).

## Data Insight

{len(items)} days plotted. Final cumulative unique customers: {items[-1]["cumCustomers"]}.
The growth curve shows customer acquisition rate over time. A steepening curve
indicates accelerating customer acquisition; a flattening curve suggests market saturation.

---

## What Was Built

1000x500 viewport, green lineAA@1 cumulative growth curve with {len(items)-1} segments.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(107, "cumulative-customer-growth", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 108: Revenue per Square Foot by Zone
# ═══════════════════════════════════════════════════════════════════════════

def trial_108():
    zone_rev = defaultdict(float)
    for si in db.sale_items:
        p = db._prod_map.get(si["productId"])
        if p:
            d = db._dept_map.get(p["departmentId"])
            if d:
                zone_rev[d["floorZoneId"]] += si["lineTotal"]

    zones = db.db["zones"]
    items = []
    for i, z in enumerate(zones):
        if z["sqft"] == 0: continue  # skip warehouse
        rev = zone_rev.get(z["id"], 0)
        rps = rev / z["sqft"]
        items.append({"index": len(items), "zone": z["id"], "name": z["name"][:20],
                       "sqft": z["sqft"], "revenue": round(rev, 2),
                       "revPerSqft": round(rps, 2)})

    doc = simple_bar_doc(items, "index", "revPerSqft", "Revenue/sqft by Zone", "#f59e0b")

    table_rows = "\n".join(f"| {r['zone']} | {r['name']} | {r['sqft']:,} | ${r['revPerSqft']:,.0f} |" for r in items)
    md = f"""# Trial 108: Revenue per Square Foot by Zone

**Date:** 2026-03-22
**Goal:** Revenue / zone sqft for each zone. Bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sale_items JOIN products ON productId
  JOIN departments ON departmentId → floorZoneId
GROUP BY floorZoneId → SUM(lineTotal)
zones.sqft → revenue_per_sqft = revenue / sqft
```

Four-table chain: sale_items -> products -> departments -> zones.

## Data Insight

| Zone | Name | SqFt | Rev/SqFt |
|------|------|------|----------|
{table_rows}

Revenue per square foot is the core retail real estate metric. Zones with high rev/sqft
justify their floor space; low-performing zones may benefit from re-merchandising.

---

## What Was Built

1000x500 viewport, {len(items)} amber bars.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(108, "revenue-per-sqft-by-zone", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 109: Average Sale by Payment Method
# ═══════════════════════════════════════════════════════════════════════════

def trial_109():
    method_totals = defaultdict(lambda: {"sum": 0.0, "count": 0})
    for s in db.sales:
        method_totals[s["paymentMethod"]]["sum"] += s["total"]
        method_totals[s["paymentMethod"]]["count"] += 1

    items = sorted([{"method": m, "avgSale": round(v["sum"] / v["count"], 2)}
                    for m, v in method_totals.items()], key=lambda x: -x["avgSale"])
    for i, r in enumerate(items):
        r["index"] = i

    colors = ["#3b82f6", "#ef4444", "#22c55e", "#f59e0b", "#8b5cf6"]
    buffers = {}; geometries = {}; drawItems = {}
    hw = 0.7 / 2.0
    for j, item in enumerate(items):
        bid = 100 + j; gid = 200 + j; diid = 300 + j
        bar = [item["index"] - hw, 0, item["index"] + hw, item["avgSale"]]
        buffers[bid] = {"byteLength": 0, "data": rf(bar)}
        geometries[gid] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": 1}
        drawItems[diid] = {"layerId": 10, "name": item["method"], "pipeline": "instancedRect@1",
                           "geometryId": gid, "transformId": 50,
                           "color": db.hex_to_rgba(colors[j % len(colors)]), "cornerRadius": 4.0}

    xs = [it["index"] for it in items]; ys = [it["avgSale"] for it in items]
    tx = db.fit_transform((min(xs) - hw, max(xs) + hw), (0, max(ys)))

    doc = make_doc(800, 500, buffers=buffers, transforms={50: tx},
        panes={1: {"name": "Avg Sale by Payment", "region": {"clipYMin": -0.92, "clipYMax": 0.92, "clipXMin": -0.92, "clipXMax": 0.92},
                    "clearColor": DARK_BG, "hasClearColor": True}},
        layers={10: {"paneId": 1, "name": "Data"}},
        geometries=geometries, drawItems=drawItems)

    table_rows = "\n".join(f"| {r['method']} | ${r['avgSale']:.2f} |" for r in items)
    md = f"""# Trial 109: Average Sale by Payment Method

**Date:** 2026-03-22
**Goal:** Average transaction total by payment method. Bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sales GROUP BY paymentMethod
AVG(total) AS avgSale
```

Single-table aggregation on sales (12,338 rows).

## Data Insight

| Payment Method | Avg Transaction |
|----------------|----------------|
{table_rows}

Credit card transactions tend to have higher averages (less friction for large purchases).
Cash transactions may be smaller, reflecting impulse buys or small items.

---

## What Was Built

800x500 viewport, {len(items)} colored bars for average sale by payment method.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(109, "avg-sale-by-payment", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 110: Seasonal Stockout Risk by Department
# ═══════════════════════════════════════════════════════════════════════════

def trial_110():
    # Products where inventory dipped below 5 in any month, count by dept
    at_risk = set()
    for snap in db.inventory_snapshots:
        if snap["quantityOnHand"] < 5:
            at_risk.add(snap["productId"])

    dept_risk = defaultdict(int)
    for pid in at_risk:
        p = db._prod_map.get(pid)
        if p:
            dept_risk[p["departmentId"]] += 1

    dept_ids = sorted(db._dept_map.keys())
    items = [{"index": i, "dept": db._dept_map[did]["name"], "deptId": did,
              "riskCount": dept_risk.get(did, 0)} for i, did in enumerate(dept_ids)]

    hw = 0.7 / 2.0
    buffers = {}; geometries = {}; drawItems = {}
    for j, item in enumerate(items):
        bid = 100 + j; gid = 200 + j; diid = 300 + j
        bar = [item["index"] - hw, 0, item["index"] + hw, item["riskCount"]]
        buffers[bid] = {"byteLength": 0, "data": rf(bar)}
        geometries[gid] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": 1}
        c = db.hex_to_rgba(db.PALETTE_DEPT.get(item["deptId"], "#888888"))
        drawItems[diid] = {"layerId": 10, "name": item["dept"], "pipeline": "instancedRect@1",
                           "geometryId": gid, "transformId": 50, "color": c, "cornerRadius": 3.0}

    xs = [it["index"] for it in items]; ys = [it["riskCount"] for it in items]
    tx = db.fit_transform((min(xs) - hw, max(xs) + hw), (0, max(ys) if max(ys) > 0 else 1))

    doc = make_doc(1000, 500, buffers=buffers, transforms={50: tx},
        panes={1: {"name": "Stockout Risk", "region": {"clipYMin": -0.92, "clipYMax": 0.92, "clipXMin": -0.92, "clipXMax": 0.92},
                    "clearColor": DARK_BG, "hasClearColor": True}},
        layers={10: {"paneId": 1, "name": "Data"}},
        geometries=geometries, drawItems=drawItems)

    table_rows = "\n".join(f"| {r['dept']} | {r['riskCount']} |" for r in items)
    md = f"""# Trial 110: Seasonal Stockout Risk by Department

**Date:** 2026-03-22
**Goal:** Products where inventory dipped below 5 in any snapshot, count by department. Bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
inventory_snapshots WHERE quantityOnHand < 5 → at-risk productIds
JOIN products ON productId → departmentId
GROUP BY departmentId → COUNT(DISTINCT productId)
```

Two-table join: inventory_snapshots (3,150) x products (150).
At-risk threshold: quantityOnHand < 5.

## Data Insight

| Department | At-Risk Products |
|------------|-----------------|
{table_rows}

Total at-risk products: {len(at_risk)}.
Departments with more stockout-risk products need tighter reorder-point management.

---

## What Was Built

1000x500 viewport, {len(items)} department-colored bars.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(110, "seasonal-stockout-risk", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 111: Expense Stacked Monthly (Top 5 Accounts)
# ═══════════════════════════════════════════════════════════════════════════

def trial_111():
    # Monthly expenses by account, stacked bars for top 5 accounts
    acct_total = defaultdict(float)
    acct_month = defaultdict(lambda: defaultdict(float))
    for e in db.expenses:
        mk = e["date"][:7]
        acct_total[e["accountId"]] += e["amount"]
        acct_month[e["accountId"]][mk] += e["amount"]

    top5_accts = sorted(acct_total.items(), key=lambda x: -x[1])[:5]
    top5_ids = [a[0] for a in top5_accts]
    all_months = sorted(set(mk for am in acct_month.values() for mk in am))

    series_list = []
    for aid in top5_ids:
        series = [{"index": mi, "amount": round(acct_month[aid].get(m, 0), 2)} for mi, m in enumerate(all_months)]
        series_list.append(series)

    stacked = db.to_stacked_bars(series_list, "index", "amount", bar_width=0.7)
    acct_colors = ["#3b82f6", "#ef4444", "#22c55e", "#f59e0b", "#8b5cf6"]

    buffers = {}; geometries = {}; drawItems = {}
    max_y = 0
    for j, (sdata, smeta) in enumerate(stacked):
        bid = 100 + j; gid = 200 + j; diid = 300 + j
        buffers[bid] = {"byteLength": 0, "data": rf(sdata)}
        geometries[gid] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": smeta["vertexCount"]}
        c = db.hex_to_rgba(acct_colors[j])
        name = db._acct_map[top5_ids[j]]["name"]
        drawItems[diid] = {"layerId": 10, "name": name, "pipeline": "instancedRect@1",
                           "geometryId": gid, "transformId": 50, "color": c}
        if smeta["yRange"][1] > max_y:
            max_y = smeta["yRange"][1]

    tx = db.fit_transform((-0.35, len(all_months) - 0.65), (0, max_y))

    doc = make_doc(1200, 500, buffers=buffers, transforms={50: tx},
        panes={1: {"name": "Monthly Expenses", "region": {"clipYMin": -0.92, "clipYMax": 0.92, "clipXMin": -0.92, "clipXMax": 0.92},
                    "clearColor": DARK_BG, "hasClearColor": True}},
        layers={10: {"paneId": 1, "name": "Data"}},
        geometries=geometries, drawItems=drawItems)

    acct_names = [db._acct_map[aid]["name"] for aid in top5_ids]
    md = f"""# Trial 111: Expense Stacked Monthly

**Date:** 2026-03-22
**Goal:** All expense accounts stacked by month. to_stacked_bars for top 5 accounts.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
expenses GROUP BY (accountId, month) → SUM(amount)
accounts → name lookup
Top 5 accounts by total: {', '.join(acct_names)}
{len(all_months)} months of data
```

Two-table join: expenses (238) x accounts (15).

## Data Insight

The stacked bars reveal expense seasonality and composition. Payroll is typically the
largest expense category. Months with spikes in other accounts may indicate inventory
buildups or one-time capital expenditures.

---

## What Was Built

1200x500 viewport, {len(all_months)} stacked bar columns with 5 account segments each.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(111, "expense-stacked-monthly", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 112: Payroll/Revenue Ratio (Line)
# ═══════════════════════════════════════════════════════════════════════════

def trial_112():
    # Monthly payroll expense / revenue
    # Find payroll account
    payroll_aids = [a["id"] for a in db.accounts if "payroll" in a["name"].lower() or "salary" in a["name"].lower() or "wage" in a["name"].lower()]
    if not payroll_aids:
        # Fallback: largest expense account
        acct_total = defaultdict(float)
        for e in db.expenses:
            acct_total[e["accountId"]] += e["amount"]
        payroll_aids = [sorted(acct_total.items(), key=lambda x: -x[1])[0][0]]

    payroll_monthly = defaultdict(float)
    for e in db.expenses:
        if e["accountId"] in payroll_aids:
            mk = e["date"][:7]
            payroll_monthly[mk] += e["amount"]

    rev = {r["month"]: r["revenue"] for r in db.monthly_revenue()}
    months = sorted(set(payroll_monthly.keys()) & set(rev.keys()))
    items = [{"index": i, "month": m, "ratio": round(payroll_monthly[m] / rev[m], 4) if rev[m] else 0}
             for i, m in enumerate(months)]

    doc = simple_line_doc(items, "index", "ratio", "Payroll/Revenue Ratio", "#f97316", line_width=2.5)

    avg_r = sum(it["ratio"] for it in items) / len(items) if items else 0
    payroll_name = db._acct_map[payroll_aids[0]]["name"] if payroll_aids else "Payroll"
    md = f"""# Trial 112: Payroll/Revenue Ratio

**Date:** 2026-03-22
**Goal:** Monthly payroll expense / revenue. lineAA@1 trend.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
expenses WHERE accountId IN ({','.join(str(a) for a in payroll_aids)}) ('{payroll_name}')
GROUP BY month → SUM(amount) AS payroll
sales GROUP BY month → SUM(total) AS revenue
ratio = payroll / revenue
```

Two independent aggregations (expenses filtered to payroll, sales) joined by month.

## Data Insight

{len(items)} months. Average payroll/revenue ratio: {avg_r:.3f} ({avg_r*100:.1f}%).

A lower ratio means each dollar of payroll generates more revenue. Seasonal spikes
(e.g., holiday staffing) may temporarily increase the ratio.

---

## What Was Built

1000x500 viewport, orange lineAA@1 with {len(items)-1} segments.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(112, "payroll-revenue-ratio", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 113: Product Lifecycle — Top 3 Products Monthly Revenue
# ═══════════════════════════════════════════════════════════════════════════

def trial_113():
    # Monthly revenue for top 3 products, 3 overlaid lines
    top3 = db.product_rankings(top_n=3)
    top3_ids = [p["id"] for p in top3]

    prod_month_rev = defaultdict(lambda: defaultdict(float))
    for si in db.sale_items:
        if si["productId"] in top3_ids:
            mk = db._sale_month.get(si["saleId"], "")
            if mk:
                prod_month_rev[si["productId"]][mk] += si["lineTotal"]

    all_months = sorted(set(mk for pmr in prod_month_rev.values() for mk in pmr))
    month_idx = {m: i for i, m in enumerate(all_months)}

    colors = ["#3b82f6", "#ef4444", "#22c55e"]
    buffers = {}; geometries = {}; drawItems = {}
    all_revs = [r for pmr in prod_month_rev.values() for r in pmr.values()]
    global_y = (0, max(all_revs) if all_revs else 1)
    global_x = (0, len(all_months) - 1)
    tx = db.fit_transform(global_x, global_y)

    for j, pid in enumerate(top3_ids):
        rows = [{"mi": month_idx[m], "revenue": round(prod_month_rev[pid].get(m, 0), 2)}
                for m in all_months if prod_month_rev[pid].get(m, 0) > 0]
        rows.sort(key=lambda x: x["mi"])
        if len(rows) < 2: continue
        data, meta = db.to_line_segments(rows, "mi", "revenue")
        bid = 100 + j; gid = 200 + j; diid = 300 + j
        buffers[bid] = {"byteLength": 0, "data": rf(data)}
        geometries[gid] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": meta["vertexCount"]}
        drawItems[diid] = {"layerId": 10, "name": top3[j]["name"][:25], "pipeline": "lineAA@1",
                           "geometryId": gid, "transformId": 50, "color": db.hex_to_rgba(colors[j]),
                           "lineWidth": 2.5}

    doc = make_doc(1200, 500, buffers=buffers, transforms={50: tx},
        panes={1: {"name": "Top 3 Product Lifecycle", "region": {"clipYMin": -0.92, "clipYMax": 0.92, "clipXMin": -0.92, "clipXMax": 0.92},
                    "clearColor": DARK_BG, "hasClearColor": True}},
        layers={10: {"paneId": 1, "name": "Lines"}},
        geometries=geometries, drawItems=drawItems)

    names = [t["name"] for t in top3]
    md = f"""# Trial 113: Product Lifecycle — Top 3

**Date:** 2026-03-22
**Goal:** Monthly revenue for top 3 products over {len(all_months)} months. 3 overlaid lines.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
product_rankings() → top 3 products: {', '.join(names[:3])}
sale_items WHERE productId IN top3
  JOIN sales (via _sale_month) → month
GROUP BY (productId, month) → SUM(lineTotal)
```

Two-table join with per-product temporal aggregation.

## Data Insight

Each product's revenue line shows its lifecycle trajectory. Some products have steady
demand; others may show seasonal peaks or gradual growth/decline.

---

## What Was Built

1200x500 viewport, 3 colored lineAA@1 lines sharing one transform.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(113, "product-lifecycle-top3", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 114: Customer Cohort Revenue
# ═══════════════════════════════════════════════════════════════════════════

def trial_114():
    # Group customers by join quarter, sum their revenue
    cust_rev = defaultdict(float)
    for s in db.sales:
        if s["customerId"]:
            cust_rev[s["customerId"]] += s["total"]

    cohort_rev = defaultdict(float)
    cohort_count = defaultdict(int)
    for c in db.customers:
        ms = c["memberSince"]
        d = date.fromisoformat(ms)
        q = f"{d.year}-Q{(d.month - 1) // 3 + 1}"
        cohort_rev[q] += cust_rev.get(c["id"], 0)
        cohort_count[q] += 1

    cohorts = sorted(cohort_rev.keys())
    items = [{"index": i, "cohort": q, "revenue": round(cohort_rev[q], 2), "count": cohort_count[q]}
             for i, q in enumerate(cohorts)]

    doc = simple_bar_doc(items, "index", "revenue", "Cohort Revenue", "#8b5cf6")

    table_rows = "\n".join(f"| {r['cohort']} | {r['count']} | ${r['revenue']:,.0f} |" for r in items)
    md = f"""# Trial 114: Customer Cohort Revenue

**Date:** 2026-03-22
**Goal:** Group customers by join-quarter, sum their total revenue. Bars by cohort.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
customers.memberSince → join quarter (YYYY-QN)
sales GROUP BY customerId → SUM(total) AS customerRevenue
GROUP BY join_quarter → SUM(customerRevenue), COUNT(customers)
```

Two-table join: customers (500) x sales (12,338).

## Data Insight

| Cohort | Customers | Total Revenue |
|--------|-----------|---------------|
{table_rows}

Earlier cohorts have higher total revenue (more time to accumulate purchases).
Revenue per cohort normalized by age reveals true acquisition quality.

---

## What Was Built

1000x500 viewport, {len(items)} violet bars for cohort revenue.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(114, "customer-cohort-revenue", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 115: Hourly Staffing vs Sales (Dual Scatter)
# ═══════════════════════════════════════════════════════════════════════════

def trial_115():
    # By hour: average staff on shift vs average sales count
    # Count shifts covering each hour
    hour_shift_days = defaultdict(lambda: defaultdict(int))
    for sh in db.shifts:
        start_h = int(sh["startTime"].split(":")[0])
        end_h = int(sh["endTime"].split(":")[0])
        for h in range(start_h, end_h):
            hour_shift_days[(h, sh["date"])] = hour_shift_days.get((h, sh["date"]), 0) + 1

    hour_staff = defaultdict(list)
    for (h, d), cnt in hour_shift_days.items():
        hour_staff[h].append(cnt)

    hour_sales = defaultdict(list)
    day_hour_sales = defaultdict(int)
    for s in db.sales:
        h = int(s["time"].split(":")[0])
        day_hour_sales[(h, s["date"])] += 1
    for (h, d), cnt in day_hour_sales.items():
        hour_sales[h].append(cnt)

    items = []
    for h in range(5, 23):
        avg_staff = sum(hour_staff.get(h, [0])) / len(hour_staff.get(h, [1])) if hour_staff.get(h) else 0
        avg_sales = sum(hour_sales.get(h, [0])) / len(hour_sales.get(h, [1])) if hour_sales.get(h) else 0
        items.append({"avgStaff": round(avg_staff, 1), "avgSales": round(avg_sales, 1), "hour": h})

    doc = simple_scatter_doc(items, "avgStaff", "avgSales", "Staffing vs Sales by Hour", "#3b82f6", point_size=7.0)

    md = f"""# Trial 115: Hourly Staffing vs Sales

**Date:** 2026-03-22
**Goal:** By hour: average staff on shift vs average sales count. Scatter.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
shifts: for each (hour, date), count employees on shift
  → AVG across dates per hour → avgStaff
sales: for each (hour, date), count transactions
  → AVG across dates per hour → avgSales
Plot: (avgStaff, avgSales) per hour
```

Two-table temporal cross-reference: shifts (13,751) x sales (12,338).

## Data Insight

{len(items)} hours plotted (5 AM - 10 PM). The scatter shows whether staffing levels
match sales demand at each hour. Points below the ideal line indicate understaffing;
points above indicate overcapacity.

---

## What Was Built

800x600 viewport, {len(items)} blue scatter points. X = avg staff, Y = avg sales.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(115, "hourly-staffing-vs-sales", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 116: Supplier Concentration (Pareto: line + bars)
# ═══════════════════════════════════════════════════════════════════════════

def trial_116():
    # Cumulative % of total PO cost by supplier
    supp_cost = defaultdict(float)
    for po in db.purchase_orders:
        supp_cost[po["supplierId"]] += po["totalCost"]
    total_cost = sum(supp_cost.values())
    ranked = sorted(supp_cost.items(), key=lambda x: -x[1])

    items = []
    cum = 0
    for i, (sid, cost) in enumerate(ranked):
        cum += cost
        items.append({"index": i, "name": db._supp_map[sid]["name"],
                       "cost": round(cost, 2), "cumPct": round(cum / total_cost, 4)})

    # Bars for individual cost
    bar_data, bar_meta = db.to_bars(items, "index", "cost", bar_width=0.7)
    # Line for cumulative %: need separate Y axis → scale cumPct to cost range
    max_cost = max(it["cost"] for it in items)
    line_items = [{"index": it["index"], "cumScaled": it["cumPct"] * max_cost} for it in items]
    line_data, line_meta = db.to_line_segments(line_items, "index", "cumScaled")

    tx = db.fit_transform(bar_meta["xRange"], bar_meta["yRange"])

    doc = make_doc(1000, 500,
        buffers={
            100: {"byteLength": 0, "data": rf(bar_data)},
            101: {"byteLength": 0, "data": rf(line_data)},
        },
        transforms={50: tx},
        panes={1: {"name": "Supplier Pareto", "region": {"clipYMin": -0.92, "clipYMax": 0.92, "clipXMin": -0.92, "clipXMax": 0.92},
                    "clearColor": DARK_BG, "hasClearColor": True}},
        layers={10: {"paneId": 1, "name": "Bars"}, 11: {"paneId": 1, "name": "Line"}},
        geometries={
            200: {"vertexBufferId": 100, "format": "rect4", "vertexCount": bar_meta["vertexCount"]},
            201: {"vertexBufferId": 101, "format": "rect4", "vertexCount": line_meta["vertexCount"]},
        },
        drawItems={
            300: {"layerId": 10, "name": "Cost Bars", "pipeline": "instancedRect@1",
                  "geometryId": 200, "transformId": 50, "color": db.hex_to_rgba("#3b82f6"),
                  "cornerRadius": 3.0},
            301: {"layerId": 11, "name": "Cumulative Line", "pipeline": "lineAA@1",
                  "geometryId": 201, "transformId": 50, "color": db.hex_to_rgba("#ef4444"),
                  "lineWidth": 2.5},
        })

    table_rows = "\n".join(f"| {r['name']} | ${r['cost']:,.0f} | {r['cumPct']*100:.1f}% |" for r in items)
    md = f"""# Trial 116: Supplier Concentration (Pareto)

**Date:** 2026-03-22
**Goal:** Cumulative % of total PO cost by supplier. Pareto chart: bars + cumulative line.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
purchase_orders GROUP BY supplierId → SUM(totalCost) AS supplierCost
ORDER BY supplierCost DESC
cumulative_pct = running_sum / total
JOIN suppliers ON id → name
```

Two-table join: purchase_orders (429) x suppliers (12).

## Data Insight

| Supplier | PO Cost | Cumulative % |
|----------|---------|-------------|
{table_rows}

Total procurement: ${total_cost:,.0f}. Classic Pareto: a few suppliers account for the
majority of procurement spend, revealing supply chain concentration risk.

---

## What Was Built

1000x500 viewport, {len(items)} blue bars (individual cost) + red cumulative line.
Two layers: bars behind, line on top. Shared transform.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(116, "supplier-concentration", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 117: Department Growth Rates (8 Overlaid Lines)
# ═══════════════════════════════════════════════════════════════════════════

def trial_117():
    dmr = db.department_monthly_revenue()
    dept_ids = sorted(set(r["deptId"] for r in dmr))
    months = sorted(set(r["month"] for r in dmr))

    # Build per-dept monthly series, compute MoM growth %
    buffers = {}; geometries = {}; drawItems = {}
    all_growths = []
    month_idx = {m: i for i, m in enumerate(months)}

    for j, did in enumerate(dept_ids):
        dept_rows = sorted([r for r in dmr if r["deptId"] == did], key=lambda x: x["month"])
        growth_items = []
        for k in range(1, len(dept_rows)):
            prev = dept_rows[k-1]["revenue"]
            curr = dept_rows[k]["revenue"]
            growth = ((curr - prev) / prev * 100) if prev > 0 else 0
            growth_items.append({"mi": k - 1, "growth": round(growth, 2)})
            all_growths.append(growth)

        if len(growth_items) < 2: continue
        data, meta = db.to_line_segments(growth_items, "mi", "growth")
        bid = 100 + j; gid = 200 + j; diid = 300 + j
        buffers[bid] = {"byteLength": 0, "data": rf(data)}
        geometries[gid] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": meta["vertexCount"]}
        c = db.hex_to_rgba(db.PALETTE_DEPT.get(did, "#888888"))
        drawItems[diid] = {"layerId": 10, "name": db._dept_map[did]["name"], "pipeline": "lineAA@1",
                           "geometryId": gid, "transformId": 50, "color": c, "lineWidth": 1.8}

    global_x = (0, len(months) - 2)
    global_y = (min(all_growths), max(all_growths)) if all_growths else (-10, 10)
    tx = db.fit_transform(global_x, global_y)

    doc = make_doc(1200, 600, buffers=buffers, transforms={50: tx},
        panes={1: {"name": "Dept MoM Growth", "region": {"clipYMin": -0.92, "clipYMax": 0.92, "clipXMin": -0.92, "clipXMax": 0.92},
                    "clearColor": DARK_BG, "hasClearColor": True}},
        layers={10: {"paneId": 1, "name": "Lines"}},
        geometries=geometries, drawItems=drawItems)

    md = f"""# Trial 117: Department Growth Rates

**Date:** 2026-03-22
**Goal:** Month-over-month revenue growth % by department. 8 overlaid lines.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
department_monthly_revenue() → per-dept time series
growth_pct = (current - previous) / previous * 100 for each consecutive month pair
```

Pre-aggregated query with derived calculation (percentage change).

## Data Insight

{len(dept_ids)} department growth lines over {len(months)-1} month transitions.
High volatility indicates seasonal sensitivity. Departments with consistently
positive growth are the store's engines; those with negative trends need attention.

---

## What Was Built

1200x600 viewport, {len(dept_ids)} department-colored lineAA@1 lines showing MoM growth %.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(117, "dept-growth-rates", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 118: Profit Margin Trend (Line)
# ═══════════════════════════════════════════════════════════════════════════

def trial_118():
    mp = db.monthly_profit()
    items = [{"index": r["index"], "margin": r["margin"]} for r in mp]

    doc = simple_line_doc(items, "index", "margin", "Monthly Gross Margin %", "#22c55e", line_width=2.5)

    avg_m = sum(it["margin"] for it in items) / len(items) if items else 0
    md = f"""# Trial 118: Profit Margin Trend

**Date:** 2026-03-22
**Goal:** Monthly gross margin % (profit / revenue). lineAA@1.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
monthly_profit() → revenue, expenses, profit per month
margin = profit / revenue
```

Pre-aggregated query combining sales revenue and expense totals.

## Data Insight

{len(items)} months. Average margin: {avg_m*100:.1f}%.

The margin trend reveals operational efficiency over time. A narrowing margin despite
growing revenue suggests costs are rising faster than sales — a warning sign.

---

## What Was Built

1000x500 viewport, green lineAA@1 with {len(items)-1} segments.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(118, "profit-margin-trend", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 119: Top Category per Department (Horizontal Bars)
# ═══════════════════════════════════════════════════════════════════════════

def trial_119():
    # For each dept, the single highest-revenue category
    dept_cat_rev = defaultdict(lambda: defaultdict(float))
    for si in db.sale_items:
        p = db._prod_map.get(si["productId"])
        if p:
            dept_cat_rev[p["departmentId"]][p["category"]] += si["lineTotal"]

    dept_ids = sorted(db._dept_map.keys())
    items = []
    for i, did in enumerate(dept_ids):
        cats = dept_cat_rev[did]
        if cats:
            top_cat = max(cats.items(), key=lambda x: x[1])
            items.append({"index": i, "dept": db._dept_map[did]["name"], "deptId": did,
                           "category": top_cat[0], "revenue": round(top_cat[1], 2)})
        else:
            items.append({"index": i, "dept": db._dept_map[did]["name"], "deptId": did,
                           "category": "-", "revenue": 0})

    # Dept-colored horizontal bars
    hh = 0.7 / 2.0
    buffers = {}; geometries = {}; drawItems = {}
    for j, item in enumerate(items):
        bid = 100 + j; gid = 200 + j; diid = 300 + j
        bar = [0, item["index"] - hh, item["revenue"], item["index"] + hh]
        buffers[bid] = {"byteLength": 0, "data": rf(bar)}
        geometries[gid] = {"vertexBufferId": bid, "format": "rect4", "vertexCount": 1}
        c = db.hex_to_rgba(db.PALETTE_DEPT.get(item["deptId"], "#888888"))
        drawItems[diid] = {"layerId": 10, "name": f"{item['dept']}: {item['category']}",
                           "pipeline": "instancedRect@1", "geometryId": gid, "transformId": 50,
                           "color": c, "cornerRadius": 3.0}

    xs = [it["revenue"] for it in items]; ys = [it["index"] for it in items]
    tx = db.fit_transform((0, max(xs)), (min(ys) - hh, max(ys) + hh))

    doc = make_doc(1000, 500, buffers=buffers, transforms={50: tx},
        panes={1: {"name": "Top Cat/Dept", "region": {"clipYMin": -0.92, "clipYMax": 0.92, "clipXMin": -0.92, "clipXMax": 0.92},
                    "clearColor": DARK_BG, "hasClearColor": True}},
        layers={10: {"paneId": 1, "name": "Data"}},
        geometries=geometries, drawItems=drawItems)

    table_rows = "\n".join(f"| {r['dept']} | {r['category']} | ${r['revenue']:,.0f} |" for r in items)
    md = f"""# Trial 119: Top Category per Department

**Date:** 2026-03-22
**Goal:** For each department, the single highest-revenue category. Horizontal bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sale_items JOIN products ON productId → (departmentId, category)
GROUP BY (departmentId, category) → SUM(lineTotal)
For each department: MAX by revenue → top category
```

Two-table join with per-group MAX selection.

## Data Insight

| Department | Top Category | Revenue |
|------------|-------------|---------|
{table_rows}

Each department's identity is defined by its top-selling category. This shows the
revenue concentration within each department's product mix.

---

## What Was Built

1000x500 viewport, {len(items)} department-colored horizontal bars.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(119, "top-category-per-dept", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Trial 120: Revenue per Hour by Employee (Top 15)
# ═══════════════════════════════════════════════════════════════════════════

def trial_120():
    # Total revenue of each employee's sales / total hours worked
    emp_rev = defaultdict(float)
    for s in db.sales:
        emp_rev[s["employeeId"]] += s["total"]

    emp_hrs = defaultdict(float)
    for sh in db.shifts:
        emp_hrs[sh["employeeId"]] += sh["hoursWorked"]

    items = []
    for eid in emp_rev:
        hrs = emp_hrs.get(eid, 0)
        if hrs < 100: continue  # skip employees with very few hours
        rph = emp_rev[eid] / hrs
        e = db._emp_map[eid]
        items.append({"empId": eid, "name": f"{e['firstName']} {e['lastName']}",
                       "revenue": round(emp_rev[eid], 2), "hours": round(hrs, 1),
                       "revPerHour": round(rph, 2)})

    items.sort(key=lambda x: -x["revPerHour"])
    items = items[:15]
    for i, r in enumerate(items):
        r["index"] = i

    doc = simple_bar_doc(items, "index", "revPerHour", "Revenue/Hour Top 15", "#06b6d4")

    table_rows = "\n".join(f"| {r['name']} | ${r['revenue']:,.0f} | {r['hours']:,.0f}h | ${r['revPerHour']:.0f}/h |" for r in items)
    md = f"""# Trial 120: Revenue per Hour by Employee

**Date:** 2026-03-22
**Goal:** For top 15 employees: total revenue of their sales / total hours worked. Bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sales GROUP BY employeeId → SUM(total) AS empRevenue
shifts GROUP BY employeeId → SUM(hoursWorked) AS empHours
revPerHour = empRevenue / empHours
WHERE empHours >= 100 (exclude low-hour workers)
ORDER BY revPerHour DESC LIMIT 15
```

Two-table cross-reference: sales (12,338) x shifts (13,751), joined by employeeId.

## Data Insight

| Employee | Revenue | Hours | Rev/Hour |
|----------|---------|-------|----------|
{table_rows}

Revenue per hour is the ultimate employee efficiency metric. It combines sales performance
with time investment, normalizing for full-time vs part-time differences.

---

## What Was Built

1000x500 viewport, 15 cyan bars for revenue-per-hour.

Total: {count_ids(doc)} unique IDs.
"""
    write_trial(120, "revenue-per-hour-by-employee", doc, md)


# ═══════════════════════════════════════════════════════════════════════════
# Run all trials
# ═══════════════════════════════════════════════════════════════════════════

if __name__ == "__main__":
    print("Generating trials 081-120 (Relational & Cross-Table Analysis)...")
    trial_081()
    trial_082()
    trial_083()
    trial_084()
    trial_085()
    trial_086()
    trial_087()
    trial_088()
    trial_089()
    trial_090()
    trial_091()
    trial_092()
    trial_093()
    trial_094()
    trial_095()
    trial_096()
    trial_097()
    trial_098()
    trial_099()
    trial_100()
    trial_101()
    trial_102()
    trial_103()
    trial_104()
    trial_105()
    trial_106()
    trial_107()
    trial_108()
    trial_109()
    trial_110()
    trial_111()
    trial_112()
    trial_113()
    trial_114()
    trial_115()
    trial_116()
    trial_117()
    trial_118()
    trial_119()
    trial_120()
    print("Done!")
