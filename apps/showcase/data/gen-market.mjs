#!/usr/bin/env node
// ENC-521 (T1.1) — market dataset generator.
//
// Multi-symbol random-walk -> per-bucket OHLCV + lastPrice ticks + a TA suite
// (vwap, sma_20, ema_12/26, rsi, macd*, bb_*, volume) + an L2 order book emitted
// as a scalar-fan (depth.bid.k.size / depth.ask.k.size). Deterministic (seeded).
//
// Output: apps/showcase/data/market/<SYMBOL>.json  + market/index.json
//
//   node apps/showcase/data/gen-market.mjs

import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { writeFileSync } from "node:fs";
import { mulberry32, gaussian, round, f32, buildDataset, writeDataset } from "./lib.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const OUT_DIR = join(HERE, "market");

// ---- replay/timing parameters -------------------------------------------------
const DURATION_MS = 20_000; // total tape span
const BUCKET_MS = 500; // one OHLC bucket every 500ms -> 40 buckets
const TICK_MS = 100; // one lastPrice tick every 100ms -> 200 ticks
const DEPTH_LEVELS = 20; // L2 depth: 20 levels per side (scalar-fan)
const SEED = 1337;

const N_BUCKETS = DURATION_MS / BUCKET_MS;

// Each symbol gets its own start price, volatility, and seed stream so they
// random-walk independently but reproducibly.
const SYMBOLS = [
  { symbol: "AAPL", start: 187.5, vol: 0.0009, drift: 0.00002 },
  { symbol: "MSFT", start: 412.0, vol: 0.0008, drift: 0.00001 },
  { symbol: "NVDA", start: 122.3, vol: 0.0016, drift: 0.00004 },
  { symbol: "TSLA", start: 178.9, vol: 0.0021, drift: -0.00002 },
];

// ---- TA helpers ---------------------------------------------------------------
function ema(prev, value, period) {
  const k = 2 / (period + 1);
  return prev == null ? value : value * k + prev * (1 - k);
}

function rsiFromGains(avgGain, avgLoss) {
  if (avgLoss === 0) return avgGain === 0 ? 50 : 100;
  const rs = avgGain / avgLoss;
  return 100 - 100 / (1 + rs);
}

function genSymbol(cfg, seedOffset) {
  const rng = mulberry32(SEED + seedOffset);
  const updates = [];

  // --- 1) generate the per-tick lastPrice random walk -------------------------
  const ticks = []; // { t, price, vol }
  let price = cfg.start;
  const nTicks = DURATION_MS / TICK_MS;
  for (let i = 0; i < nTicks; i++) {
    const t = i * TICK_MS;
    const ret = cfg.drift + cfg.vol * gaussian(rng);
    price = Math.max(0.01, price * (1 + ret));
    const tickVol = Math.max(1, Math.round(200 + 800 * rng()));
    ticks.push({ t, price, vol: tickVol });
    updates.push({ t, streamKey: cfg.symbol, field: "lastPrice", value: round(price, 4) });
  }

  // --- 2) aggregate ticks into OHLCV buckets ---------------------------------
  const buckets = []; // { t, open, high, low, close, volume, vwapNum, vwapDen }
  let cumPV = 0;
  let cumV = 0;
  for (let b = 0; b < N_BUCKETS; b++) {
    const t0 = b * BUCKET_MS;
    const t1 = t0 + BUCKET_MS;
    const inBucket = ticks.filter((tk) => tk.t >= t0 && tk.t < t1);
    const closeT = t1 - TICK_MS; // emit bucket-derived fields at the bucket's last tick
    const prices = inBucket.map((tk) => tk.price);
    const open = prices[0];
    const close = prices[prices.length - 1];
    const high = Math.max(...prices);
    const low = Math.min(...prices);
    const volume = inBucket.reduce((s, tk) => s + tk.vol, 0);
    const typical = (high + low + close) / 3;
    cumPV += typical * volume;
    cumV += volume;
    const vwap = cumV > 0 ? cumPV / cumV : close;
    buckets.push({ t: closeT, open, high, low, close, volume, vwap });

    // OHLC -> compound candle (slots open/high/low/close)
    updates.push({ t: closeT, streamKey: cfg.symbol, field: "open", value: round(open, 4) });
    updates.push({ t: closeT, streamKey: cfg.symbol, field: "high", value: round(high, 4) });
    updates.push({ t: closeT, streamKey: cfg.symbol, field: "low", value: round(low, 4) });
    updates.push({ t: closeT, streamKey: cfg.symbol, field: "close", value: round(close, 4) });
    updates.push({ t: closeT, streamKey: cfg.symbol, field: "volume", value: f32(volume) });
    updates.push({ t: closeT, streamKey: cfg.symbol, field: "vwap", value: round(vwap, 4) });
  }

  // --- 3) TA suite over the bucket closes ------------------------------------
  const closes = buckets.map((b) => b.close);
  let ema12 = null;
  let ema26 = null;
  let macdSignal = null;
  let avgGain = null;
  let avgLoss = null;
  const RSI_PERIOD = 14;
  const SMA_PERIOD = 20;
  const BB_PERIOD = 20;

  for (let i = 0; i < buckets.length; i++) {
    const t = buckets[i].t;
    const close = closes[i];

    // SMA(20) — once we have a full window
    if (i >= SMA_PERIOD - 1) {
      const win = closes.slice(i - SMA_PERIOD + 1, i + 1);
      const sma = win.reduce((s, v) => s + v, 0) / SMA_PERIOD;
      updates.push({ t, streamKey: cfg.symbol, field: "sma_20", value: round(sma, 4) });

      // Bollinger Bands (sma ± 2σ) share the 20-window
      const mean = sma;
      const variance = win.reduce((s, v) => s + (v - mean) ** 2, 0) / SMA_PERIOD;
      const sd = Math.sqrt(variance);
      updates.push({ t, streamKey: cfg.symbol, field: "bb_middle", value: round(mean, 4) });
      updates.push({ t, streamKey: cfg.symbol, field: "bb_upper", value: round(mean + 2 * sd, 4) });
      updates.push({ t, streamKey: cfg.symbol, field: "bb_lower", value: round(mean - 2 * sd, 4) });
    }
    void BB_PERIOD;

    // EMA(12)/EMA(26) -> MACD / signal / histogram
    ema12 = ema(ema12, close, 12);
    ema26 = ema(ema26, close, 26);
    updates.push({ t, streamKey: cfg.symbol, field: "ema_12", value: round(ema12, 4) });
    updates.push({ t, streamKey: cfg.symbol, field: "ema_26", value: round(ema26, 4) });
    const macd = ema12 - ema26;
    macdSignal = ema(macdSignal, macd, 9);
    updates.push({ t, streamKey: cfg.symbol, field: "macd", value: round(macd, 4) });
    updates.push({ t, streamKey: cfg.symbol, field: "macdSignal", value: round(macdSignal, 4) });
    updates.push({ t, streamKey: cfg.symbol, field: "macdHistogram", value: round(macd - macdSignal, 4) });

    // RSI(14) — Wilder smoothing
    if (i > 0) {
      const change = closes[i] - closes[i - 1];
      const gain = Math.max(0, change);
      const loss = Math.max(0, -change);
      if (i <= RSI_PERIOD) {
        avgGain = (avgGain ?? 0) + gain / RSI_PERIOD;
        avgLoss = (avgLoss ?? 0) + loss / RSI_PERIOD;
      } else {
        avgGain = (avgGain * (RSI_PERIOD - 1) + gain) / RSI_PERIOD;
        avgLoss = (avgLoss * (RSI_PERIOD - 1) + loss) / RSI_PERIOD;
      }
      if (i >= RSI_PERIOD) {
        const rsi = rsiFromGains(avgGain, avgLoss);
        updates.push({ t, streamKey: cfg.symbol, field: "rsi", value: round(rsi, 2) });
      }
    }
  }

  // --- 4) L2 order book as a scalar-fan (fixed-mode snapshot per tick) --------
  // Re-walk the per-tick prices; at each tick emit a fresh depth ladder centered
  // on the current price. 20 levels/side -> 40 scalar updates per tick.
  for (const tk of ticks) {
    const mid = tk.price;
    const tickSize = Math.max(0.01, mid * 0.0002);
    for (let k = 0; k < DEPTH_LEVELS; k++) {
      // size decays with depth, with a little deterministic noise per level
      const base = 1200 / (k + 1);
      const noiseBid = 0.6 + 0.8 * rng();
      const noiseAsk = 0.6 + 0.8 * rng();
      const bidSize = Math.round(base * noiseBid);
      const askSize = Math.round(base * noiseAsk);
      updates.push({ t: tk.t, streamKey: cfg.symbol, field: `depth.bid.${k}.size`, value: f32(bidSize) });
      updates.push({ t: tk.t, streamKey: cfg.symbol, field: `depth.ask.${k}.size`, value: f32(askSize) });
    }
    void tickSize;
  }

  return buildDataset(
    {
      datasetId: cfg.symbol,
      kind: "market",
      durationMs: DURATION_MS,
      cadenceMs: TICK_MS,
      bucketMs: BUCKET_MS,
      symbols: [cfg.symbol],
      depthLevels: DEPTH_LEVELS,
      seed: SEED + seedOffset,
    },
    updates,
  );
}

// ---- main ---------------------------------------------------------------------
const index = [];
let totalUpdates = 0;
SYMBOLS.forEach((cfg, i) => {
  const dataset = genSymbol(cfg, i * 101);
  const path = join(OUT_DIR, `${cfg.symbol}.json`);
  const n = writeDataset(path, dataset);
  totalUpdates += n;
  index.push({ symbol: cfg.symbol, datasetId: cfg.symbol, file: `${cfg.symbol}.json`, updates: n, fields: dataset.meta.fields.length });
  console.log(`  market/${cfg.symbol}.json  ${n} updates, ${dataset.meta.fields.length} fields`);
});

writeFileSync(
  join(OUT_DIR, "index.json"),
  JSON.stringify({ kind: "market", durationMs: DURATION_MS, symbols: index }, null, 2) + "\n",
);
console.log(`market: ${SYMBOLS.length} symbols, ${totalUpdates} total updates -> market/index.json`);
