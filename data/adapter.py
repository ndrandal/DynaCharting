"""Data adapter: reads Meridian Hardware DB → produces vertex-ready float arrays.

Usage:
    from data.adapter import StoreData
    db = StoreData()

    # Query
    monthly = db.monthly_revenue()  # [{month, revenue, count}, ...]

    # Convert to vertices
    data, meta = db.to_line_segments(monthly, "index", "revenue")
    # data = [x0,y0,x1,y1, ...] for lineAA@1 rect4
    # meta = {"format": "rect4", "vertexCount": N, "xRange": (min,max), "yRange": (min,max)}

    # Compute transform to map data space → clip space
    tx = db.fit_transform(meta["xRange"], meta["yRange"])
    # tx = {"sx": ..., "sy": ..., "tx": ..., "ty": ...}
"""
import json, math, os
from collections import defaultdict, Counter
from datetime import date

_DB_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "meridian_hardware.json")


class StoreData:
    def __init__(self, path=_DB_PATH):
        with open(path) as f:
            self.db = json.load(f)
        self._precompute()

    # ══════════════════════════════════════════════════════════
    # Precompute common aggregations
    # ══════════════════════════════════════════════════════════
    def _precompute(self):
        db = self.db
        self._prod_map = {p["id"]: p for p in db["products"]}
        self._dept_map = {d["id"]: d for d in db["departments"]}
        self._emp_map = {e["id"]: e for e in db["employees"]}
        self._cust_map = {c["id"]: c for c in db["customers"]}
        self._acct_map = {a["id"]: a for a in db["accounts"]}
        self._supp_map = {s["id"]: s for s in db["suppliers"]}

        # Product → total units & revenue
        self._prod_units = defaultdict(int)
        self._prod_revenue = defaultdict(float)
        for si in db["sale_items"]:
            self._prod_units[si["productId"]] += si["quantity"]
            self._prod_revenue[si["productId"]] += si["lineTotal"]

        # Monthly aggregations
        self._monthly_rev = defaultdict(float)
        self._monthly_count = defaultdict(int)
        self._monthly_items = defaultdict(int)
        for s in db["sales"]:
            mk = s["date"][:7]
            self._monthly_rev[mk] += s["total"]
            self._monthly_count[mk] += 1
        for si in db["sale_items"]:
            sale = None
            # Use sale_id → date mapping for speed
            pass
        # Build sale_id → month mapping
        self._sale_month = {}
        for s in db["sales"]:
            self._sale_month[s["id"]] = s["date"][:7]

        # Department revenue
        self._dept_rev = defaultdict(float)
        self._dept_units = defaultdict(int)
        for si in db["sale_items"]:
            p = self._prod_map.get(si["productId"])
            if p:
                self._dept_rev[p["departmentId"]] += si["lineTotal"]
                self._dept_units[p["departmentId"]] += si["quantity"]

    # ══════════════════════════════════════════════════════════
    # Raw accessors
    # ══════════════════════════════════════════════════════════
    @property
    def store(self): return self.db["store"]
    @property
    def products(self): return self.db["products"]
    @property
    def employees(self): return self.db["employees"]
    @property
    def customers(self): return self.db["customers"]
    @property
    def departments(self): return self.db["departments"]
    @property
    def suppliers(self): return self.db["suppliers"]
    @property
    def sales(self): return self.db["sales"]
    @property
    def sale_items(self): return self.db["sale_items"]
    @property
    def shifts(self): return self.db["shifts"]
    @property
    def expenses(self): return self.db["expenses"]
    @property
    def accounts(self): return self.db["accounts"]
    @property
    def inventory_snapshots(self): return self.db["inventory_snapshots"]
    @property
    def purchase_orders(self): return self.db["purchase_orders"]
    @property
    def daily_summaries(self): return self.db["daily_summaries"]

    # ══════════════════════════════════════════════════════════
    # Data Queries — return list[dict]
    # ══════════════════════════════════════════════════════════

    def monthly_revenue(self):
        """[{month, index, revenue, count, avgTicket}] sorted by month."""
        months = sorted(self._monthly_rev.keys())
        return [{"month": m, "index": i, "revenue": round(self._monthly_rev[m], 2),
                 "count": self._monthly_count[m],
                 "avgTicket": round(self._monthly_rev[m] / self._monthly_count[m], 2) if self._monthly_count[m] else 0}
                for i, m in enumerate(months)]

    def daily_revenue(self):
        """[{date, index, revenue, count, uniqueCustomers}] from daily_summaries."""
        ds = sorted(self.db["daily_summaries"], key=lambda x: x["date"])
        return [{"date": d["date"], "index": i,
                 "revenue": d["totalRevenue"], "count": d["transactionCount"],
                 "uniqueCustomers": d["uniqueCustomers"]}
                for i, d in enumerate(ds)]

    def department_revenue(self):
        """[{id, name, revenue, units}] sorted by revenue desc."""
        rows = [{"id": did, "name": self._dept_map[did]["name"],
                 "revenue": round(self._dept_rev[did], 2),
                 "units": self._dept_units[did]}
                for did in sorted(self._dept_rev, key=lambda x: -self._dept_rev[x])]
        # Add index for bar positioning
        for i, r in enumerate(rows):
            r["index"] = i
        return rows

    def department_monthly_revenue(self):
        """[{deptId, deptName, month, index, revenue}] — for multi-line or stacked area."""
        dept_month = defaultdict(lambda: defaultdict(float))
        for si in self.db["sale_items"]:
            p = self._prod_map.get(si["productId"])
            if not p: continue
            sale_month = self._sale_month.get(si["saleId"], "")
            dept_month[p["departmentId"]][sale_month] += si["lineTotal"]

        all_months = sorted(set(m for dm in dept_month.values() for m in dm))
        rows = []
        for did in sorted(dept_month):
            for i, m in enumerate(all_months):
                rows.append({"deptId": did, "deptName": self._dept_map[did]["name"],
                             "month": m, "index": i,
                             "revenue": round(dept_month[did].get(m, 0), 2)})
        return rows

    def product_rankings(self, top_n=None):
        """[{id, name, dept, units, revenue, unitPrice, margin}] sorted by revenue desc."""
        rows = []
        for p in self.products:
            pid = p["id"]
            rev = self._prod_revenue.get(pid, 0)
            units = self._prod_units.get(pid, 0)
            margin = (p["unitPrice"] - p["unitCost"]) / p["unitPrice"] if p["unitPrice"] else 0
            rows.append({"id": pid, "name": p["name"], "dept": self._dept_map[p["departmentId"]]["name"],
                         "deptId": p["departmentId"],
                         "units": units, "revenue": round(rev, 2),
                         "unitPrice": p["unitPrice"], "unitCost": p["unitCost"],
                         "margin": round(margin, 3)})
        rows.sort(key=lambda x: -x["revenue"])
        for i, r in enumerate(rows):
            r["index"] = i
        return rows[:top_n] if top_n else rows

    def hourly_distribution(self):
        """[{hour, count, revenue}] — sales by hour of day."""
        by_hour = defaultdict(lambda: {"count": 0, "revenue": 0.0})
        for s in self.sales:
            h = int(s["time"].split(":")[0])
            by_hour[h]["count"] += 1
            by_hour[h]["revenue"] += s["total"]
        return [{"hour": h, "index": h, "count": by_hour[h]["count"],
                 "revenue": round(by_hour[h]["revenue"], 2)}
                for h in range(24) if by_hour[h]["count"] > 0]

    def dow_distribution(self):
        """[{dow, name, count, revenue}] — sales by day of week."""
        names = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]
        by_dow = defaultdict(lambda: {"count": 0, "revenue": 0.0})
        for s in self.sales:
            d = date.fromisoformat(s["date"])
            dow = d.weekday()
            by_dow[dow]["count"] += 1
            by_dow[dow]["revenue"] += s["total"]
        return [{"dow": d, "name": names[d], "index": d,
                 "count": by_dow[d]["count"],
                 "revenue": round(by_dow[d]["revenue"], 2)}
                for d in range(7)]

    def payment_method_breakdown(self):
        """[{method, count, revenue, fraction}]."""
        by_method = defaultdict(lambda: {"count": 0, "revenue": 0.0})
        for s in self.sales:
            by_method[s["paymentMethod"]]["count"] += 1
            by_method[s["paymentMethod"]]["revenue"] += s["total"]
        total_rev = sum(v["revenue"] for v in by_method.values())
        rows = [{"method": m, "count": v["count"],
                 "revenue": round(v["revenue"], 2),
                 "fraction": round(v["revenue"] / total_rev, 4) if total_rev else 0}
                for m, v in sorted(by_method.items(), key=lambda x: -x[1]["revenue"])]
        for i, r in enumerate(rows):
            r["index"] = i
        return rows

    def customer_tier_breakdown(self):
        """[{tier, count, fraction}]."""
        tiers = Counter(c["tier"] for c in self.customers)
        total = len(self.customers)
        order = ["gold", "silver", "bronze"]
        return [{"tier": t, "index": i, "count": tiers.get(t, 0),
                 "fraction": round(tiers.get(t, 0) / total, 4)}
                for i, t in enumerate(order)]

    def customer_tier_revenue(self):
        """[{tier, revenue, avgSpend, count}] — revenue by customer tier."""
        cust_rev = defaultdict(float)
        cust_count = defaultdict(int)
        for s in self.sales:
            if s["customerId"]:
                c = self._cust_map.get(s["customerId"])
                if c:
                    cust_rev[c["tier"]] += s["total"]
                    cust_count[c["tier"]] += 1
        order = ["gold", "silver", "bronze"]
        return [{"tier": t, "index": i, "revenue": round(cust_rev.get(t, 0), 2),
                 "count": cust_count.get(t, 0),
                 "avgSpend": round(cust_rev[t] / cust_count[t], 2) if cust_count.get(t) else 0}
                for i, t in enumerate(order)]

    def expense_by_account(self):
        """[{accountId, name, type, total}] sorted by total desc."""
        by_acct = defaultdict(float)
        for e in self.expenses:
            by_acct[e["accountId"]] += e["amount"]
        rows = [{"accountId": aid, "name": self._acct_map[aid]["name"],
                 "type": self._acct_map[aid]["type"],
                 "total": round(by_acct[aid], 2)}
                for aid in sorted(by_acct, key=lambda x: -by_acct[x])]
        for i, r in enumerate(rows):
            r["index"] = i
        return rows

    def monthly_expenses(self):
        """[{month, index, total}]."""
        by_month = defaultdict(float)
        for e in self.expenses:
            mk = e["date"][:7]
            by_month[mk] += e["amount"]
        months = sorted(by_month.keys())
        return [{"month": m, "index": i, "total": round(by_month[m], 2)}
                for i, m in enumerate(months)]

    def monthly_profit(self):
        """[{month, index, revenue, expenses, profit, margin}]."""
        rev = {r["month"]: r["revenue"] for r in self.monthly_revenue()}
        exp = {e["month"]: e["total"] for e in self.monthly_expenses()}
        months = sorted(set(list(rev.keys()) + list(exp.keys())))
        rows = []
        for i, m in enumerate(months):
            r = rev.get(m, 0)
            e = exp.get(m, 0)
            p = round(r - e, 2)
            rows.append({"month": m, "index": i, "revenue": round(r, 2),
                         "expenses": round(e, 2), "profit": p,
                         "margin": round(p / r, 4) if r else 0})
        return rows

    def employee_hours(self, top_n=None):
        """[{id, name, role, totalHours, avgWeeklyHours}]."""
        by_emp = defaultdict(float)
        for sh in self.shifts:
            by_emp[sh["employeeId"]] += sh["hoursWorked"]
        weeks = len(set(sh["date"][:7] for sh in self.shifts)) * 4.33
        rows = []
        for eid, hrs in sorted(by_emp.items(), key=lambda x: -x[1]):
            e = self._emp_map[eid]
            rows.append({"id": eid, "name": f"{e['firstName']} {e['lastName']}",
                         "role": e["role"], "totalHours": round(hrs, 1),
                         "avgWeeklyHours": round(hrs / weeks, 1) if weeks else 0})
        for i, r in enumerate(rows):
            r["index"] = i
        return rows[:top_n] if top_n else rows

    def shift_heatmap(self):
        """[{dow, hour, count}] — shift coverage heatmap (7 days × 16 hours)."""
        grid = defaultdict(int)
        for sh in self.shifts:
            d = date.fromisoformat(sh["date"])
            dow = d.weekday()
            start_h = int(sh["startTime"].split(":")[0])
            end_h = int(sh["endTime"].split(":")[0])
            for h in range(start_h, end_h):
                grid[(dow, h)] += 1
        return [{"dow": dow, "hour": h, "count": grid.get((dow, h), 0)}
                for dow in range(7) for h in range(5, 23)]

    def inventory_trend(self, product_id):
        """[{date, index, qty, reorderPoint, sold}] for one product."""
        snaps = [s for s in self.inventory_snapshots if s["productId"] == product_id]
        snaps.sort(key=lambda x: x["date"])
        return [{"date": s["date"], "index": i, "qty": s["quantityOnHand"],
                 "reorderPoint": s["reorderPoint"], "sold": s["monthlyUnitsSold"]}
                for i, s in enumerate(snaps)]

    def supplier_performance(self):
        """[{id, name, poCount, avgLeadTime, totalCost}]."""
        by_supp = defaultdict(lambda: {"count": 0, "totalLead": 0, "cost": 0.0})
        for po in self.purchase_orders:
            sid = po["supplierId"]
            by_supp[sid]["count"] += 1
            ord_d = date.fromisoformat(po["orderDate"])
            rcv_d = date.fromisoformat(po["receivedDate"])
            by_supp[sid]["totalLead"] += (rcv_d - ord_d).days
            by_supp[sid]["cost"] += po["totalCost"]
        rows = []
        for sid in sorted(by_supp):
            s = self._supp_map[sid]
            v = by_supp[sid]
            rows.append({"id": sid, "name": s["name"],
                         "poCount": v["count"],
                         "avgLeadTime": round(v["totalLead"] / v["count"], 1) if v["count"] else 0,
                         "totalCost": round(v["cost"], 2)})
        for i, r in enumerate(rows):
            r["index"] = i
        return rows

    def items_per_sale_distribution(self):
        """[{itemCount, frequency}] — distribution of items per transaction."""
        counts = Counter(s["itemCount"] for s in self.sales)
        return [{"itemCount": n, "index": n, "frequency": counts.get(n, 0)}
                for n in range(1, max(counts.keys()) + 1)]

    def product_price_vs_volume(self):
        """[{id, name, unitPrice, unitsSold, revenue}] — for scatter plots."""
        rows = []
        for p in self.products:
            pid = p["id"]
            rows.append({"id": pid, "name": p["name"],
                         "unitPrice": p["unitPrice"],
                         "unitsSold": self._prod_units.get(pid, 0),
                         "revenue": round(self._prod_revenue.get(pid, 0), 2)})
        return rows

    # ══════════════════════════════════════════════════════════
    # Vertex Builders — convert query results → float arrays
    # ══════════════════════════════════════════════════════════

    @staticmethod
    def to_line_segments(items, x_key, y_key):
        """Convert ordered points to lineAA@1 rect4 segments.
        Returns (data, meta) where data = [x0,y0,x1,y1, ...]."""
        if len(items) < 2:
            return [], {"format": "rect4", "vertexCount": 0, "xRange": (0,0), "yRange": (0,0)}
        data = []
        xs = [item[x_key] for item in items]
        ys = [item[y_key] for item in items]
        for i in range(len(items) - 1):
            data.extend([xs[i], ys[i], xs[i+1], ys[i+1]])
        return data, {
            "format": "rect4", "vertexCount": len(items) - 1,
            "xRange": (min(xs), max(xs)), "yRange": (min(ys), max(ys)),
        }

    @staticmethod
    def to_bars(items, x_key, y_key, bar_width=0.7, baseline=0):
        """Convert categorical data to instancedRect@1 rect4.
        Returns (data, meta) where data = [x0,y0,x1,y1, ...] per bar."""
        data = []
        xs = [item[x_key] for item in items]
        ys = [item[y_key] for item in items]
        hw = bar_width / 2.0
        for item in items:
            x = item[x_key]
            y = item[y_key]
            data.extend([x - hw, baseline, x + hw, y])
        y_vals = ys + [baseline]
        return data, {
            "format": "rect4", "vertexCount": len(items),
            "xRange": (min(xs) - hw, max(xs) + hw),
            "yRange": (min(y_vals), max(y_vals)),
        }

    @staticmethod
    def to_horizontal_bars(items, y_key, x_key, bar_height=0.7, baseline=0):
        """Horizontal bars: instancedRect@1 rect4.
        x_key = the value (bar length), y_key = category position."""
        data = []
        xs = [item[x_key] for item in items]
        ys = [item[y_key] for item in items]
        hh = bar_height / 2.0
        for item in items:
            y = item[y_key]
            x = item[x_key]
            data.extend([baseline, y - hh, x, y + hh])
        x_vals = xs + [baseline]
        return data, {
            "format": "rect4", "vertexCount": len(items),
            "xRange": (min(x_vals), max(x_vals)),
            "yRange": (min(ys) - hh, max(ys) + hh),
        }

    @staticmethod
    def to_scatter(items, x_key, y_key):
        """Convert to points@1 pos2_clip.
        Returns (data, meta) where data = [x,y, x,y, ...]."""
        data = []
        xs, ys = [], []
        for item in items:
            x, y = item[x_key], item[y_key]
            data.extend([x, y])
            xs.append(x); ys.append(y)
        return data, {
            "format": "pos2_clip", "vertexCount": len(items),
            "xRange": (min(xs), max(xs)) if xs else (0, 0),
            "yRange": (min(ys), max(ys)) if ys else (0, 0),
        }

    @staticmethod
    def to_area(items, x_key, y_key, baseline=0):
        """Convert to triSolid@1 filled area under curve.
        Returns (data, meta) with 6 floats per quad (2 triangles)."""
        if len(items) < 2:
            return [], {"format": "pos2_clip", "vertexCount": 0, "xRange": (0,0), "yRange": (0,0)}
        data = []
        xs = [item[x_key] for item in items]
        ys = [item[y_key] for item in items]
        for i in range(len(items) - 1):
            x0, y0 = xs[i], ys[i]
            x1, y1 = xs[i+1], ys[i+1]
            # Triangle 1
            data.extend([x0, y0, x0, baseline, x1, y1])
            # Triangle 2
            data.extend([x1, y1, x0, baseline, x1, baseline])
        return data, {
            "format": "pos2_clip", "vertexCount": (len(items) - 1) * 6,
            "xRange": (min(xs), max(xs)),
            "yRange": (min(min(ys), baseline), max(max(ys), baseline)),
        }

    @staticmethod
    def to_pie_wedges(items, value_key, cx=0.0, cy=0.0, r=0.8, segments_per_wedge=32):
        """Convert to list of triSolid@1 wedge data.
        Returns [(data, fraction, startAngle, endAngle), ...] per wedge."""
        total = sum(item[value_key] for item in items)
        if total == 0:
            return []
        wedges = []
        angle = math.pi / 2  # Start at top (12 o'clock)
        for item in items:
            frac = item[value_key] / total
            sweep = frac * 2 * math.pi
            n_segs = max(3, int(segments_per_wedge * frac + 1))
            data = []
            for s in range(n_segs):
                a0 = angle + sweep * s / n_segs
                a1 = angle + sweep * (s + 1) / n_segs
                data.extend([cx, cy,
                             cx + r * math.cos(a0), cy + r * math.sin(a0),
                             cx + r * math.cos(a1), cy + r * math.sin(a1)])
            wedges.append((data, frac, angle, angle + sweep))
            angle += sweep
        return wedges

    @staticmethod
    def to_donut_wedges(items, value_key, cx=0.0, cy=0.0, r_outer=0.8, r_inner=0.4, segments_per_wedge=32):
        """Convert to list of triSolid@1 donut wedge data.
        Each wedge is a ring segment (2 triangles per sub-segment)."""
        total = sum(item[value_key] for item in items)
        if total == 0: return []
        wedges = []
        angle = math.pi / 2
        for item in items:
            frac = item[value_key] / total
            sweep = frac * 2 * math.pi
            n_segs = max(3, int(segments_per_wedge * frac + 1))
            data = []
            for s in range(n_segs):
                a0 = angle + sweep * s / n_segs
                a1 = angle + sweep * (s + 1) / n_segs
                # Outer edge points
                ox0, oy0 = cx + r_outer * math.cos(a0), cy + r_outer * math.sin(a0)
                ox1, oy1 = cx + r_outer * math.cos(a1), cy + r_outer * math.sin(a1)
                # Inner edge points
                ix0, iy0 = cx + r_inner * math.cos(a0), cy + r_inner * math.sin(a0)
                ix1, iy1 = cx + r_inner * math.cos(a1), cy + r_inner * math.sin(a1)
                # Two triangles per sub-segment
                data.extend([ox0, oy0, ix0, iy0, ox1, oy1])
                data.extend([ox1, oy1, ix0, iy0, ix1, iy1])
            wedges.append((data, frac, angle, angle + sweep))
            angle += sweep
        return wedges

    @staticmethod
    def to_heatmap_rects(items, row_key, col_key, value_key, cell_w=1.0, cell_h=1.0, gap=0.05):
        """Convert grid data to instancedRect@1 rect4.
        Returns (data, meta, value_range) for color mapping."""
        data = []
        vals = [item[value_key] for item in items]
        for item in items:
            r, c = item[row_key], item[col_key]
            x0 = c * cell_w + gap
            y0 = r * cell_h + gap
            x1 = (c + 1) * cell_w - gap
            y1 = (r + 1) * cell_h - gap
            data.extend([x0, y0, x1, y1])
        rows = [item[row_key] for item in items]
        cols = [item[col_key] for item in items]
        return data, {
            "format": "rect4", "vertexCount": len(items),
            "xRange": (0, (max(cols) + 1) * cell_w),
            "yRange": (0, (max(rows) + 1) * cell_h),
        }, (min(vals), max(vals))

    @staticmethod
    def to_stacked_bars(series_list, x_key, y_key, bar_width=0.7):
        """Convert multiple series to stacked instancedRect@1.
        series_list = [items_series_0, items_series_1, ...]
        Each series must have same x positions.
        Returns [(data, meta), ...] per series."""
        if not series_list: return []
        n_x = len(series_list[0])
        hw = bar_width / 2.0

        # Track cumulative baseline per x position
        baselines = [0.0] * n_x
        results = []

        for series in series_list:
            data = []
            ys = []
            for j, item in enumerate(series):
                x = item[x_key]
                y = item[y_key]
                bot = baselines[j]
                top = bot + y
                data.extend([x - hw, bot, x + hw, top])
                baselines[j] = top
                ys.append(top)
            xs = [item[x_key] for item in series]
            results.append((data, {
                "format": "rect4", "vertexCount": len(series),
                "xRange": (min(xs) - hw, max(xs) + hw),
                "yRange": (0, max(ys)),
            }))
        return results

    @staticmethod
    def to_candles(items, x_key, open_key, high_key, low_key, close_key, half_width=0.35):
        """Convert OHLC data to instancedCandle@1 candle6.
        Returns (data, meta)."""
        data = []
        for item in items:
            data.extend([item[x_key], item[open_key], item[high_key],
                         item[low_key], item[close_key], half_width])
        xs = [item[x_key] for item in items]
        all_prices = [item[k] for item in items for k in [open_key, high_key, low_key, close_key]]
        return data, {
            "format": "candle6", "vertexCount": len(items),
            "xRange": (min(xs) - half_width, max(xs) + half_width),
            "yRange": (min(all_prices), max(all_prices)),
        }

    # ══════════════════════════════════════════════════════════
    # Transform Computation
    # ══════════════════════════════════════════════════════════

    @staticmethod
    def fit_transform(x_range, y_range, clip_x=(-0.9, 0.9), clip_y=(-0.9, 0.9), padding=0.05):
        """Compute transform {sx, sy, tx, ty} to map data range → clip range.
        Adds padding to data range to avoid points on exact edges."""
        x_min, x_max = x_range
        y_min, y_max = y_range

        # Add padding
        x_span = x_max - x_min if x_max != x_min else 1.0
        y_span = y_max - y_min if y_max != y_min else 1.0
        x_min -= x_span * padding
        x_max += x_span * padding
        y_min -= y_span * padding
        y_max += y_span * padding

        cx_min, cx_max = clip_x
        cy_min, cy_max = clip_y

        sx = (cx_max - cx_min) / (x_max - x_min) if x_max != x_min else 1.0
        sy = (cy_max - cy_min) / (y_max - y_min) if y_max != y_min else 1.0
        tx = cx_min - x_min * sx
        ty = cy_min - y_min * sy

        return {"sx": round(sx, 9), "sy": round(sy, 9),
                "tx": round(tx, 9), "ty": round(ty, 9)}

    # ══════════════════════════════════════════════════════════
    # Color Helpers
    # ══════════════════════════════════════════════════════════

    @staticmethod
    def hex_to_rgba(h, a=1.0):
        h = h.lstrip('#')
        return [int(h[i:i+2], 16) / 255.0 for i in (0, 2, 4)] + [a]

    @staticmethod
    def value_to_color(val, vmin, vmax, palette="viridis"):
        """Map a value in [vmin, vmax] to an RGBA color."""
        t = (val - vmin) / (vmax - vmin) if vmax != vmin else 0.5
        t = max(0.0, min(1.0, t))
        if palette == "viridis":
            # Simplified viridis approximation
            r = max(0, min(1, 0.267 + t * (0.993 - 0.267)))
            g = max(0, min(1, 0.004 + t * 0.906))
            b = max(0, min(1, 0.329 + (1 - t) * 0.511))
            return [round(r, 4), round(g, 4), round(b, 4), 1.0]
        elif palette == "blue_red":
            return [round(t, 4), round(0.1, 4), round(1 - t, 4), 1.0]
        elif palette == "heat":
            if t < 0.5:
                return [round(t * 2, 4), round(t * 2, 4), 0.0, 1.0]
            else:
                return [1.0, round(2 - t * 2, 4), 0.0, 1.0]
        else:
            return [round(t, 4), round(t, 4), round(t, 4), 1.0]

    # Standard chart color palettes
    PALETTE_8 = [
        "#3b82f6", "#ef4444", "#22c55e", "#f59e0b",
        "#8b5cf6", "#ec4899", "#06b6d4", "#f97316",
    ]

    PALETTE_DEPT = {
        1: "#f59e0b",  # Lumber — amber
        2: "#3b82f6",  # Tools — blue
        3: "#06b6d4",  # Plumbing — cyan
        4: "#8b5cf6",  # Electrical — violet
        5: "#ef4444",  # Paint — red
        6: "#22c55e",  # Garden — green
        7: "#ec4899",  # Décor — pink
        8: "#f97316",  # Seasonal — orange
    }


# ══════════════════════════════════════════════════════════════
# Quick self-test
# ══════════════════════════════════════════════════════════════
if __name__ == "__main__":
    db = StoreData()
    print("Adapter loaded OK")
    print(f"  Monthly revenue points: {len(db.monthly_revenue())}")
    print(f"  Daily revenue points:   {len(db.daily_revenue())}")
    print(f"  Departments:            {len(db.department_revenue())}")
    print(f"  Products ranked:        {len(db.product_rankings())}")
    print(f"  Hourly distribution:    {len(db.hourly_distribution())}")
    print(f"  DOW distribution:       {len(db.dow_distribution())}")
    print(f"  Payment methods:        {len(db.payment_method_breakdown())}")
    print(f"  Customer tiers:         {len(db.customer_tier_breakdown())}")
    print(f"  Expense accounts:       {len(db.expense_by_account())}")
    print(f"  Employee hours:         {len(db.employee_hours())}")
    print(f"  Shift heatmap cells:    {len(db.shift_heatmap())}")
    print(f"  Supplier performance:   {len(db.supplier_performance())}")
    print(f"  Items/sale dist:        {len(db.items_per_sale_distribution())}")

    # Test vertex builders
    mr = db.monthly_revenue()
    data, meta = db.to_line_segments(mr, "index", "revenue")
    tx = db.fit_transform(meta["xRange"], meta["yRange"])
    print(f"\n  Monthly revenue line: {meta['vertexCount']} segments, transform: sx={tx['sx']:.6f}")

    dr = db.department_revenue()
    data, meta = db.to_bars(dr, "index", "revenue")
    print(f"  Dept revenue bars: {meta['vertexCount']} bars")

    pp = db.product_price_vs_volume()
    data, meta = db.to_scatter(pp, "unitPrice", "unitsSold")
    print(f"  Product scatter: {meta['vertexCount']} points")

    pm = db.payment_method_breakdown()
    wedges = db.to_pie_wedges(pm, "revenue")
    print(f"  Payment pie: {len(wedges)} wedges, {sum(len(w[0])//2 for w in wedges)} vertices")

    print("\nAll OK.")
