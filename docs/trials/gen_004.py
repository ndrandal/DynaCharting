#!/usr/bin/env python3
"""Generate the 004-bollinger-band-chart.json file."""
import json
import math
import random

random.seed(42)

# ── Generate 60 OHLC candles ──────────────────────────────────
N = 60
candles = []  # list of dicts with o,h,l,c,vol,is_up

price = 100.0
for i in range(N):
    # Trend bias
    if i < 15:
        drift = 0.35  # uptrend
    elif i < 30:
        drift = 0.0   # choppy
    elif i < 45:
        drift = -0.30  # downtrend
    else:
        drift = 0.25   # recovery

    body = random.uniform(0.3, 2.0)
    direction = 1 if random.random() < (0.55 + drift * 0.4) else -1

    if direction == 1:
        o = price + random.uniform(-0.3, 0.3)
        c = o + body
    else:
        o = price + random.uniform(-0.3, 0.3)
        c = o - body

    wick_up = random.uniform(0.2, 1.5)
    wick_dn = random.uniform(0.2, 1.5)
    h = max(o, c) + wick_up
    l = min(o, c) - wick_dn

    is_up = c >= o

    # Volume: 500-3000, spikes at trend changes
    base_vol = random.uniform(500, 1800)
    if 13 <= i <= 17 or 28 <= i <= 32 or 43 <= i <= 47:
        base_vol += random.uniform(800, 1500)  # spike
    vol = min(base_vol, 3200)

    candles.append({"o": o, "h": h, "l": l, "c": c, "vol": vol, "is_up": is_up})
    price = c  # next open near this close

# ── SMA(20) and Bollinger Bands ───────────────────────────────
sma = []
upper_band = []
lower_band = []
for i in range(20, N):
    window = [candles[j]["c"] for j in range(i - 19, i + 1)]
    mean = sum(window) / 20.0
    variance = sum((v - mean) ** 2 for v in window) / 20.0
    sd = math.sqrt(variance)
    sma.append(mean)
    upper_band.append(mean + 2 * sd)
    lower_band.append(mean - 2 * sd)

# SMA has 40 points (indices 20..59), so 39 line segments

# ── Price range ───────────────────────────────────────────────
all_prices = []
for c in candles:
    all_prices.extend([c["h"], c["l"]])
all_prices.extend(upper_band)
all_prices.extend(lower_band)

price_min = min(all_prices)
price_max = max(all_prices)
price_pad = (price_max - price_min) * 0.05
data_ymin = price_min - price_pad
data_ymax = price_max + price_pad

# Volume range
vol_max = max(c["vol"] for c in candles)
vol_ymax = vol_max * 1.15

# X range: candles at x=0..59, pad by 1 on each side
data_xmin = -1.0
data_xmax = 60.0

# ── Candle buffer (candle6: x, open, high, low, close, halfWidth) ──
candle_data = []
hw = 0.35
for i, c in enumerate(candles):
    candle_data.extend([float(i), c["o"], c["h"], c["l"], c["c"], hw])

# ── SMA line buffer (lineAA rect4: x0,y0,x1,y1 per segment) ──
# 40 points → 39 segments
sma_data = []
for j in range(len(sma) - 1):
    x0 = float(20 + j)
    y0 = sma[j]
    x1 = float(20 + j + 1)
    y1 = sma[j + 1]
    sma_data.extend([x0, y0, x1, y1])

# ── Bollinger upper line (lineAA rect4: 39 segments) ──
bb_upper_data = []
for j in range(len(upper_band) - 1):
    x0 = float(20 + j)
    y0 = upper_band[j]
    x1 = float(20 + j + 1)
    y1 = upper_band[j + 1]
    bb_upper_data.extend([x0, y0, x1, y1])

# ── Bollinger lower line (lineAA rect4: 39 segments) ──
bb_lower_data = []
for j in range(len(lower_band) - 1):
    x0 = float(20 + j)
    y0 = lower_band[j]
    x1 = float(20 + j + 1)
    y1 = lower_band[j + 1]
    bb_lower_data.extend([x0, y0, x1, y1])

# ── Bollinger fill (triSolid pos2_clip, area between upper and lower) ──
# 40 points → 39 segments, each segment = 2 triangles = 6 vertices
# For each segment i:
#   tri1: (x[i], upper[i]), (x[i], lower[i]), (x[i+1], upper[i+1])
#   tri2: (x[i+1], upper[i+1]), (x[i], lower[i]), (x[i+1], lower[i+1])
bb_fill_data = []
for j in range(len(upper_band) - 1):
    x0 = float(20 + j)
    x1 = float(20 + j + 1)
    u0 = upper_band[j]
    u1 = upper_band[j + 1]
    l0 = lower_band[j]
    l1 = lower_band[j + 1]
    # tri1
    bb_fill_data.extend([x0, u0, x0, l0, x1, u1])
    # tri2
    bb_fill_data.extend([x1, u1, x0, l0, x1, l1])

# ── Volume bars (instancedRect rect4: x0, y0, x1, y1) ──
# Split into up and down
vol_up_data = []
vol_down_data = []
bar_hw = 0.35
for i, c in enumerate(candles):
    x0 = float(i) - bar_hw
    x1 = float(i) + bar_hw
    y0 = 0.0
    y1 = c["vol"]
    if c["is_up"]:
        vol_up_data.extend([x0, y0, x1, y1])
    else:
        vol_down_data.extend([x0, y0, x1, y1])

# ── Vertex counts ─────────────────────────────────────────────
n_candles = 60
n_sma_segments = len(sma) - 1  # 39
n_bb_fill_verts = (len(upper_band) - 1) * 6  # 39 * 6 = 234
n_bb_upper_segs = len(upper_band) - 1  # 39
n_bb_lower_segs = len(lower_band) - 1  # 39
n_vol_up = len(vol_up_data) // 4
n_vol_down = len(vol_down_data) // 4

# Sanity checks
assert len(candle_data) == n_candles * 6, f"candle: {len(candle_data)} vs {n_candles*6}"
assert len(sma_data) == n_sma_segments * 4, f"sma: {len(sma_data)} vs {n_sma_segments*4}"
assert len(bb_fill_data) == n_bb_fill_verts * 2, f"fill: {len(bb_fill_data)} vs {n_bb_fill_verts*2}"
assert len(bb_upper_data) == n_bb_upper_segs * 4
assert len(bb_lower_data) == n_bb_lower_segs * 4
assert len(vol_up_data) == n_vol_up * 4
assert len(vol_down_data) == n_vol_down * 4

# ── Text overlay labels ──────────────────────────────────────
# Price range for Y-axis labels
price_step = (data_ymax - data_ymin) / 5
price_labels = []
for i in range(1, 5):
    val = data_ymin + price_step * i
    # Map to clip Y within price pane
    # Pane 1 region: clipYMin=0.08, clipYMax=0.98
    # data range: data_ymin..data_ymax
    frac = (val - data_ymin) / (data_ymax - data_ymin)
    clipY = 0.08 + frac * (0.98 - 0.08)
    price_labels.append({
        "clipX": 0.85,
        "clipY": round(clipY, 3),
        "text": f"{val:.1f}",
        "align": "l",
        "color": "#888888",
        "fontSize": 10
    })

# Volume Y-axis labels
vol_step = vol_ymax / 3
vol_labels = []
for i in range(1, 3):
    val = vol_step * i
    frac = val / vol_ymax
    clipY = -0.98 + frac * (-0.34 - (-0.98))
    vol_labels.append({
        "clipX": 0.85,
        "clipY": round(clipY, 3),
        "text": f"{val:.0f}",
        "align": "l",
        "color": "#888888",
        "fontSize": 10
    })

text_labels = [
    {"clipX": -0.93, "clipY": 0.93, "text": "BTC/USDT", "align": "l", "color": "#ffffff", "fontSize": 14},
    {"clipX": -0.93, "clipY": 0.87, "text": "Bollinger Bands (20, 2)", "align": "l", "color": "#888888", "fontSize": 11},
    {"clipX": -0.93, "clipY": -0.38, "text": "Volume", "align": "l", "color": "#888888", "fontSize": 11},
] + price_labels + vol_labels

# ── Round floats ──────────────────────────────────────────────
def rf(arr, decimals=4):
    return [round(v, decimals) for v in arr]

# ── Build document ────────────────────────────────────────────
doc = {
    "version": 1,
    "viewport": {"width": 1000, "height": 700},
    "buffers": {
        "100": {"data": rf(bb_fill_data)},
        "103": {"data": rf(bb_upper_data)},
        "106": {"data": rf(bb_lower_data)},
        "109": {"data": rf(candle_data)},
        "112": {"data": rf(sma_data)},
        "115": {"data": rf(vol_up_data)},
        "118": {"data": rf(vol_down_data)},
    },
    "transforms": {
        "50": {"tx": 0, "ty": 0, "sx": 1, "sy": 1},
        "51": {"tx": 0, "ty": 0, "sx": 1, "sy": 1},
    },
    "panes": {
        "1": {
            "name": "Price",
            "region": {"clipYMin": 0.08, "clipYMax": 0.98, "clipXMin": -0.98, "clipXMax": 0.82},
            "hasClearColor": True,
            "clearColor": [0.09, 0.09, 0.14, 1.0],
        },
        "2": {
            "name": "Volume",
            "region": {"clipYMin": -0.98, "clipYMax": -0.34, "clipXMin": -0.98, "clipXMax": 0.82},
            "hasClearColor": True,
            "clearColor": [0.08, 0.08, 0.12, 1.0],
        },
    },
    "layers": {
        "10": {"paneId": 1, "name": "BollingerFill"},
        "11": {"paneId": 1, "name": "BollingerLines"},
        "12": {"paneId": 1, "name": "Candles"},
        "13": {"paneId": 1, "name": "SMA"},
        "14": {"paneId": 2, "name": "VolumeBars"},
    },
    "geometries": {
        "101": {"vertexBufferId": 100, "format": "pos2_clip", "vertexCount": n_bb_fill_verts},
        "104": {"vertexBufferId": 103, "format": "rect4", "vertexCount": n_bb_upper_segs},
        "107": {"vertexBufferId": 106, "format": "rect4", "vertexCount": n_bb_lower_segs},
        "110": {"vertexBufferId": 109, "format": "candle6", "vertexCount": n_candles},
        "113": {"vertexBufferId": 112, "format": "rect4", "vertexCount": n_sma_segments},
        "116": {"vertexBufferId": 115, "format": "rect4", "vertexCount": n_vol_up},
        "119": {"vertexBufferId": 118, "format": "rect4", "vertexCount": n_vol_down},
    },
    "drawItems": {
        "102": {
            "layerId": 10,
            "name": "BollingerFill",
            "pipeline": "triSolid@1",
            "geometryId": 101,
            "transformId": 50,
            "color": [0.129, 0.588, 0.953, 0.12],
        },
        "105": {
            "layerId": 11,
            "name": "BollingerUpper",
            "pipeline": "lineAA@1",
            "geometryId": 104,
            "transformId": 50,
            "color": [0.129, 0.588, 0.953, 1.0],
            "lineWidth": 1.0,
        },
        "108": {
            "layerId": 11,
            "name": "BollingerLower",
            "pipeline": "lineAA@1",
            "geometryId": 107,
            "transformId": 50,
            "color": [0.129, 0.588, 0.953, 1.0],
            "lineWidth": 1.0,
        },
        "111": {
            "layerId": 12,
            "name": "Candles",
            "pipeline": "instancedCandle@1",
            "geometryId": 110,
            "transformId": 50,
            "colorUp": [0.149, 0.651, 0.604, 1.0],
            "colorDown": [0.937, 0.325, 0.314, 1.0],
        },
        "114": {
            "layerId": 13,
            "name": "SMA20",
            "pipeline": "lineAA@1",
            "geometryId": 113,
            "transformId": 50,
            "color": [1.0, 0.655, 0.149, 1.0],
            "lineWidth": 1.5,
        },
        "117": {
            "layerId": 14,
            "name": "VolumeUp",
            "pipeline": "instancedRect@1",
            "geometryId": 116,
            "transformId": 51,
            "color": [0.149, 0.651, 0.604, 0.7],
        },
        "120": {
            "layerId": 14,
            "name": "VolumeDown",
            "pipeline": "instancedRect@1",
            "geometryId": 119,
            "transformId": 51,
            "color": [0.937, 0.325, 0.314, 0.7],
        },
    },
    "viewports": {
        "price": {
            "transformId": 50,
            "paneId": 1,
            "xMin": data_xmin,
            "xMax": data_xmax,
            "yMin": round(data_ymin, 2),
            "yMax": round(data_ymax, 2),
            "linkGroup": "price",
        },
        "volume": {
            "transformId": 51,
            "paneId": 2,
            "xMin": data_xmin,
            "xMax": data_xmax,
            "yMin": 0,
            "yMax": round(vol_ymax, 2),
            "linkGroup": "price",
            "panY": False,
            "zoomY": False,
        },
    },
    "textOverlay": {
        "fontSize": 12,
        "color": "#b2b5bc",
        "labels": text_labels,
    },
}

# Write JSON
out = json.dumps(doc, indent=2)
print(out)
