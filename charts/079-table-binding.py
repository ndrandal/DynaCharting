#!/usr/bin/env python3
"""Generate a Table → Chart binding dashboard.

A data table at the top with clickable rows.
Clicking a row shows that stock's price history in the chart below.
"""
import json, math

W, H = 900, 600

# ---- Stock data ----
stocks = [
    {"ticker": "AAPL",  "name": "Apple Inc.",       "price": 189.84, "change": +2.31, "sector": "Technology",
     "color": [0.30, 0.69, 0.31, 1.0]},
    {"ticker": "MSFT",  "name": "Microsoft Corp.",  "price": 417.52, "change": +1.87, "sector": "Technology",
     "color": [0.13, 0.59, 0.95, 1.0]},
    {"ticker": "AMZN",  "name": "Amazon.com Inc.",  "price": 186.49, "change": -0.42, "sector": "Consumer",
     "color": [1.00, 0.60, 0.00, 1.0]},
    {"ticker": "GOOGL", "name": "Alphabet Inc.",    "price": 153.71, "change": +0.95, "sector": "Technology",
     "color": [0.91, 0.30, 0.24, 1.0]},
    {"ticker": "TSLA",  "name": "Tesla Inc.",       "price": 248.23, "change": -1.53, "sector": "Automotive",
     "color": [0.61, 0.15, 0.69, 1.0]},
    {"ticker": "NVDA",  "name": "NVIDIA Corp.",     "price": 875.28, "change": +4.12, "sector": "Technology",
     "color": [0.47, 0.84, 0.09, 1.0]},
]

N = len(stocks)

# ---- Layout constants (clip space) ----
# Table area
TABLE_LEFT   = -0.94
TABLE_RIGHT  =  0.94
TABLE_TOP    =  0.92
ROW_HEIGHT   =  0.105
ROW_GAP      =  0.02
HEADER_HEIGHT = 0.08

# Chart area
CHART_LEFT   = -0.85
CHART_RIGHT  =  0.90
CHART_TOP    =  TABLE_TOP - HEADER_HEIGHT - N * (ROW_HEIGHT + ROW_GAP) - 0.08
CHART_BOTTOM = -0.92

# Column positions (clip X)
COL_TICKER = TABLE_LEFT + 0.04
COL_NAME   = TABLE_LEFT + 0.22
COL_PRICE  = TABLE_LEFT + 0.95
COL_CHANGE = TABLE_LEFT + 1.28
COL_SECTOR = TABLE_LEFT + 1.58

# ---- Generate price history per stock ----
NUM_POINTS = 80

def gen_price_line(seed, base_price, volatility, trend):
    pts = []
    for i in range(NUM_POINTS):
        t = i / (NUM_POINTS - 1)
        x = CHART_LEFT + t * (CHART_RIGHT - CHART_LEFT)
        # Normalize price to chart Y range
        price_variation = (
            trend * t +
            volatility * math.sin(2.3 * t * 2 * math.pi + seed) +
            volatility * 0.4 * math.sin(5.7 * t * 2 * math.pi + seed * 1.3) +
            volatility * 0.2 * math.sin(11.1 * t * 2 * math.pi + seed * 0.5)
        )
        y_norm = 0.5 + price_variation  # Center around 0.5
        y = CHART_BOTTOM + y_norm * (CHART_TOP - CHART_BOTTOM)
        y = max(CHART_BOTTOM, min(CHART_TOP, y))
        pts.extend([x, y])
    return pts

price_lines = [
    gen_price_line(seed=0.0, base_price=189, volatility=0.12, trend=0.15),   # AAPL
    gen_price_line(seed=1.2, base_price=417, volatility=0.10, trend=0.20),   # MSFT
    gen_price_line(seed=2.4, base_price=186, volatility=0.18, trend=-0.05),  # AMZN
    gen_price_line(seed=3.6, base_price=153, volatility=0.08, trend=0.10),   # GOOGL
    gen_price_line(seed=4.8, base_price=248, volatility=0.22, trend=-0.12),  # TSLA
    gen_price_line(seed=6.0, base_price=875, volatility=0.14, trend=0.25),   # NVDA
]

# ---- Chart grid ----
grid_verts = []
# Vertical lines
for i in range(9):
    t = i / 8
    x = CHART_LEFT + t * (CHART_RIGHT - CHART_LEFT)
    grid_verts.extend([x, CHART_BOTTOM, x, CHART_TOP])
# Horizontal lines
for i in range(7):
    t = i / 6
    y = CHART_BOTTOM + t * (CHART_TOP - CHART_BOTTOM)
    grid_verts.extend([CHART_LEFT, y, CHART_RIGHT, y])

# Chart border
border_verts = [
    CHART_LEFT, CHART_BOTTOM, CHART_RIGHT, CHART_BOTTOM,
    CHART_RIGHT, CHART_BOTTOM, CHART_RIGHT, CHART_TOP,
    CHART_RIGHT, CHART_TOP, CHART_LEFT, CHART_TOP,
    CHART_LEFT, CHART_TOP, CHART_LEFT, CHART_BOTTOM,
]

# ---- Table row backgrounds ----
row_rects = []
header_y1 = TABLE_TOP
header_y0 = TABLE_TOP - HEADER_HEIGHT

for i in range(N):
    y1 = header_y0 - ROW_GAP - i * (ROW_HEIGHT + ROW_GAP)
    y0 = y1 - ROW_HEIGHT
    row_rects.append([TABLE_LEFT, y0, TABLE_RIGHT, y1])

# Row selection highlight (slightly brighter rect, same position)
highlight_rects = []
for i in range(N):
    y1 = header_y0 - ROW_GAP - i * (ROW_HEIGHT + ROW_GAP)
    y0 = y1 - ROW_HEIGHT
    highlight_rects.append([TABLE_LEFT - 0.005, y0 - 0.005, TABLE_RIGHT + 0.005, y1 + 0.005])

# Header background
header_rect = [TABLE_LEFT, header_y0, TABLE_RIGHT, header_y1]

# Table separator line under header
separator_verts = [TABLE_LEFT, header_y0, TABLE_RIGHT, header_y0]

# ---- Column separator lines ----
col_sep_verts = []
col_x_positions = [-0.36, 0.20, 0.52]
for cx in col_x_positions:
    for i in range(N):
        y1 = header_y0 - ROW_GAP - i * (ROW_HEIGHT + ROW_GAP)
        y0 = y1 - ROW_HEIGHT
        col_sep_verts.extend([cx, y0 + 0.015, cx, y1 - 0.015])
    # Also in header
    col_sep_verts.extend([cx, header_y0 + 0.01, cx, header_y1 - 0.01])

# ---- Build document ----
# ID plan:
# Buffers:  100 (grid), 101 (border), 102 (separator), 103 (header bg), 104 (col seps)
#           110-115 (row bgs), 120-125 (highlights), 130-135 (price lines)
# Transforms: 50
# Panes: 1, Layers: 10 (bg), 11 (table), 12 (chart-data), 13 (highlights)
# Geometries: 200 (grid), 201 (border), 202 (sep), 203 (header), 204 (col seps)
#             210-215 (row bgs), 220-225 (highlights), 230-235 (price lines)
# DrawItems: 500 (grid), 501 (border), 502 (sep), 503 (header bg), 504 (col seps)
#            300-305 (row bgs), 310-315 (highlights), 400-405 (price lines)
# Bindings:  1001+

doc = {
    "version": 1,
    "viewport": {"width": W, "height": H},
    "buffers": {},
    "transforms": {"50": {"tx": 0, "ty": 0, "sx": 1, "sy": 1}},
    "panes": {
        "1": {"name": "main", "hasClearColor": True, "clearColor": [0.08, 0.08, 0.11, 1.0]}
    },
    "layers": {
        "10": {"paneId": 1, "name": "background"},
        "11": {"paneId": 1, "name": "table"},
        "12": {"paneId": 1, "name": "chart-data"},
        "13": {"paneId": 1, "name": "highlights"},
    },
    "geometries": {},
    "drawItems": {},
    "bindings": {},
    "textOverlay": {"fontSize": 13, "color": "#9a9da3", "labels": []}
}

T = 50  # transform ID

# ---- Background elements ----
# Chart grid
doc["buffers"]["100"] = {"data": grid_verts}
doc["geometries"]["200"] = {"vertexBufferId": 100, "format": "pos2_clip", "vertexCount": len(grid_verts) // 2}
doc["drawItems"]["500"] = {
    "layerId": 10, "pipeline": "line2d@1", "geometryId": 200,
    "transformId": T, "color": [0.15, 0.15, 0.19, 0.6], "lineWidth": 1.0
}

# Chart border
doc["buffers"]["101"] = {"data": border_verts}
doc["geometries"]["201"] = {"vertexBufferId": 101, "format": "pos2_clip", "vertexCount": len(border_verts) // 2}
doc["drawItems"]["501"] = {
    "layerId": 10, "pipeline": "line2d@1", "geometryId": 201,
    "transformId": T, "color": [0.28, 0.28, 0.34, 0.9], "lineWidth": 1.5
}

# Table header separator
doc["buffers"]["102"] = {"data": separator_verts}
doc["geometries"]["202"] = {"vertexBufferId": 102, "format": "pos2_clip", "vertexCount": 2}
doc["drawItems"]["502"] = {
    "layerId": 11, "pipeline": "line2d@1", "geometryId": 202,
    "transformId": T, "color": [0.30, 0.30, 0.38, 1.0], "lineWidth": 1.5
}

# Header background
doc["buffers"]["103"] = {"data": header_rect}
doc["geometries"]["203"] = {"vertexBufferId": 103, "format": "rect4", "vertexCount": 1}
doc["drawItems"]["503"] = {
    "layerId": 10, "pipeline": "instancedRect@1", "geometryId": 203,
    "transformId": T, "color": [0.12, 0.12, 0.16, 1.0]
}

# Column separators
doc["buffers"]["104"] = {"data": col_sep_verts}
doc["geometries"]["204"] = {"vertexBufferId": 104, "format": "pos2_clip", "vertexCount": len(col_sep_verts) // 2}
doc["drawItems"]["504"] = {
    "layerId": 11, "pipeline": "line2d@1", "geometryId": 204,
    "transformId": T, "color": [0.18, 0.18, 0.24, 0.6], "lineWidth": 1.0
}

# ---- Table row backgrounds (clickable) ----
for i in range(N):
    buf_id = 110 + i
    geom_id = 210 + i
    di_id = 300 + i

    doc["buffers"][str(buf_id)] = {"data": row_rects[i]}
    doc["geometries"][str(geom_id)] = {"vertexBufferId": buf_id, "format": "rect4", "vertexCount": 1}
    even = (i % 2 == 0)
    bg = [0.11, 0.11, 0.15, 1.0] if even else [0.095, 0.095, 0.13, 1.0]
    doc["drawItems"][str(di_id)] = {
        "layerId": 11, "pipeline": "instancedRect@1", "geometryId": geom_id,
        "transformId": T, "color": bg
    }

# ---- Row highlight overlays (initially invisible) ----
for i in range(N):
    buf_id = 120 + i
    geom_id = 220 + i
    di_id = 310 + i

    doc["buffers"][str(buf_id)] = {"data": highlight_rects[i]}
    doc["geometries"][str(geom_id)] = {"vertexBufferId": buf_id, "format": "rect4", "vertexCount": 1}
    doc["drawItems"][str(di_id)] = {
        "layerId": 13, "pipeline": "instancedRect@1", "geometryId": geom_id,
        "transformId": T,
        "color": [stocks[i]["color"][0], stocks[i]["color"][1], stocks[i]["color"][2], 0.18],
        "visible": False
    }

# ---- Price line DrawItems (initially invisible) ----
for i in range(N):
    buf_id = 130 + i
    geom_id = 230 + i
    di_id = 400 + i

    doc["buffers"][str(buf_id)] = {"data": price_lines[i]}
    doc["geometries"][str(geom_id)] = {
        "vertexBufferId": buf_id, "format": "pos2_clip",
        "vertexCount": len(price_lines[i]) // 2
    }
    doc["drawItems"][str(di_id)] = {
        "layerId": 12, "pipeline": "line2d@1", "geometryId": geom_id,
        "transformId": T,
        "color": stocks[i]["color"],
        "lineWidth": 2.5,
        "visible": False
    }

# ---- "Select a row" placeholder ----
placeholder_y = (CHART_TOP + CHART_BOTTOM) / 2
doc["buffers"]["140"] = {"data": [CHART_LEFT, placeholder_y, CHART_RIGHT, placeholder_y]}
doc["geometries"]["240"] = {"vertexBufferId": 140, "format": "pos2_clip", "vertexCount": 2}
doc["drawItems"]["410"] = {
    "layerId": 12, "pipeline": "line2d@1", "geometryId": 240,
    "transformId": T, "color": [0.25, 0.25, 0.30, 0.4],
    "lineWidth": 1.0, "dashLength": 10.0, "gapLength": 8.0
}

# ---- Bindings ----
binding_id = 1001
for i in range(N):
    row_di = 300 + i
    line_di = 400 + i
    highlight_di = 310 + i

    # Show line
    doc["bindings"][str(binding_id)] = {
        "trigger": {"type": "selection", "drawItemId": row_di},
        "effect": {"type": "setVisible", "drawItemId": line_di,
                   "visible": True, "defaultVisible": False}
    }
    binding_id += 1

    # Show row highlight
    doc["bindings"][str(binding_id)] = {
        "trigger": {"type": "selection", "drawItemId": row_di},
        "effect": {"type": "setVisible", "drawItemId": highlight_di,
                   "visible": True, "defaultVisible": False}
    }
    binding_id += 1

    # Hide placeholder
    doc["bindings"][str(binding_id)] = {
        "trigger": {"type": "selection", "drawItemId": row_di},
        "effect": {"type": "setVisible", "drawItemId": 410,
                   "visible": False, "defaultVisible": True}
    }
    binding_id += 1

# ---- Text overlay labels ----
labels = doc["textOverlay"]["labels"]

# Table header
labels.append({"clipX": COL_TICKER, "clipY": (header_y0 + header_y1) / 2,
               "text": "TICKER", "align": "l", "color": "#6b6e76", "fontSize": 11})
labels.append({"clipX": COL_NAME, "clipY": (header_y0 + header_y1) / 2,
               "text": "COMPANY", "align": "l", "color": "#6b6e76", "fontSize": 11})
labels.append({"clipX": COL_PRICE, "clipY": (header_y0 + header_y1) / 2,
               "text": "PRICE", "align": "r", "color": "#6b6e76", "fontSize": 11})
labels.append({"clipX": COL_CHANGE, "clipY": (header_y0 + header_y1) / 2,
               "text": "CHG %", "align": "r", "color": "#6b6e76", "fontSize": 11})
labels.append({"clipX": COL_SECTOR, "clipY": (header_y0 + header_y1) / 2,
               "text": "SECTOR", "align": "l", "color": "#6b6e76", "fontSize": 11})

# Table rows
for i, stock in enumerate(stocks):
    y1 = header_y0 - ROW_GAP - i * (ROW_HEIGHT + ROW_GAP)
    y0 = y1 - ROW_HEIGHT
    y_center = (y0 + y1) / 2

    # Color indicator dot (using unicode block)
    c = stock["color"]
    hex_color = "#{:02x}{:02x}{:02x}".format(
        int(c[0]*255), int(c[1]*255), int(c[2]*255))

    labels.append({"clipX": TABLE_LEFT + 0.015, "clipY": y_center,
                   "text": "\u25cf", "align": "l", "color": hex_color, "fontSize": 14})

    labels.append({"clipX": COL_TICKER, "clipY": y_center,
                   "text": stock["ticker"], "align": "l", "color": "#e0e2e8", "fontSize": 13})
    labels.append({"clipX": COL_NAME, "clipY": y_center,
                   "text": stock["name"], "align": "l", "color": "#a0a3ab", "fontSize": 12})

    labels.append({"clipX": COL_PRICE, "clipY": y_center,
                   "text": f"${stock['price']:.2f}", "align": "r", "color": "#e0e2e8", "fontSize": 13})

    chg = stock["change"]
    chg_color = "#4caf50" if chg >= 0 else "#ef5350"
    chg_text = f"+{chg:.2f}%" if chg >= 0 else f"{chg:.2f}%"
    labels.append({"clipX": COL_CHANGE, "clipY": y_center,
                   "text": chg_text, "align": "r", "color": chg_color, "fontSize": 13})

    labels.append({"clipX": COL_SECTOR, "clipY": y_center,
                   "text": stock["sector"], "align": "l", "color": "#787b83", "fontSize": 12})

# Chart title (shows when nothing selected)
labels.append({"clipX": (CHART_LEFT + CHART_RIGHT) / 2, "clipY": CHART_TOP + 0.04,
               "text": "PRICE HISTORY  \u2014  Click a row above to view", "align": "c",
               "color": "#555560", "fontSize": 13})

# Month labels on chart X axis
months = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug"]
for j, m in enumerate(months):
    t = j / (len(months) - 1)
    x = CHART_LEFT + t * (CHART_RIGHT - CHART_LEFT)
    labels.append({"clipX": x, "clipY": CHART_BOTTOM - 0.04,
                   "text": m, "align": "c", "color": "#444450", "fontSize": 10})

# ---- Write ----
output_path = "charts/079-table-binding.json"
with open(output_path, "w") as f:
    json.dump(doc, f, indent=2)

print(f"Generated {output_path}")
print(f"  {N} stocks, {NUM_POINTS} price points each")
print(f"  {len(doc['buffers'])} buffers, {len(doc['drawItems'])} drawItems, {len(doc['bindings'])} bindings")
print(f"  {len(labels)} text labels")
