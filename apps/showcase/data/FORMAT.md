# Showcase dataset format (ENC-521 / ENC-522)

The datasets here are the **mock GMA's playback tapes**. The mock GMA reads a dataset,
then replays its updates in time order as GMA value-stream emissions. embassy routes each
emission into a DynaCharting buffer by ID (see
`specs/2026-06-11-dynacharting-capabilities-showcase/CONTRACT-buffer-id.md` and
`DESIGN-buffer-binding.md`). **No computation happens at replay time** — every value
(OHLCV, TA, depth ladder, heatmap cell, waveform sample) is precomputed here. That keeps
the mock dead-simple: load file, sort by `t`, emit.

## File shape

One dataset = one JSON file. Top-level:

```jsonc
{
  "meta": {
    "datasetId": "market",          // stable id (matches view.json datasetId)
    "kind": "market|synthetic",
    "durationMs": 20000,            // total span; updates live in [0, durationMs]
    "cadenceMs": 100,               // nominal base tick spacing (informational)
    "symbols": ["AAPL", ...],       // market only; [] otherwise
    "streamKeys": ["AAPL", ...],    // every distinct streamKey present
    "fields": ["close", "vwap", ...], // every distinct field present (sorted, deduped)
    "seed": 1337,                   // RNG seed used to generate (determinism)
    "updateCount": 12345            // length of updates[]
  },
  "updates": [
    { "t": 0,   "streamKey": "AAPL", "field": "close", "value": 187.42 },
    { "t": 0,   "streamKey": "AAPL", "field": "open",  "value": 187.30 },
    ...
  ]
}
```

### The update record

`{ t, streamKey, field, value }` — that's the whole vocabulary.

- `t` — integer milliseconds **offset from start of replay**, in `[0, meta.durationMs]`.
  Updates are emitted in ascending `t`. (The file is written already-sorted; the validator
  enforces monotonicity.)
- `streamKey` — the logical instrument/trace. For market data this is the symbol (`"AAPL"`).
  For synthetic traces it's the trace id (`"heatmap"`, `"ecg"`).
- `field` — the GMA key being updated. This is the join point with the buffer contract:
  a binding's `slot` corresponds to a `(streamKey, field)` pair via the instruction's
  `subscriptions[]`. Field names follow the **GMA key vocabulary** below.
- `value` — a finite JSON number (`f32`-representable). Never `NaN`/`Infinity`/`null`.

NDJSON is **not** used — a single JSON object per file keeps `meta` and `updates` together
and is trivial to `JSON.parse`. (If a future dataset gets large enough to stream, switch
that one file to NDJSON with a leading `meta` line; the validator already reads the parsed
form so only the loader would change.)

## GMA key vocabulary (the `field` values the mock can emit)

Scalar time-series keys (one value per tick/bucket):

| field | meaning |
|---|---|
| `open` `high` `low` `close` | OHLC bucket components |
| `lastPrice` | per-tick last trade price (higher cadence than buckets) |
| `volume` | per-bucket traded volume |
| `vwap` | volume-weighted average price (cumulative over the session) |
| `sma_20` | 20-bucket simple moving average of close |
| `ema_12` `ema_26` | 12/26-bucket exponential moving averages |
| `rsi` | 14-bucket Relative Strength Index, 0..100 |
| `macd` `macdSignal` `macdHistogram` | MACD line (ema_12 − ema_26), its 9-EMA signal, and the histogram (macd − signal) |
| `bb_middle` `bb_upper` `bb_lower` | Bollinger Bands (sma_20 ± 2σ) |

Vector keys, emitted as a **scalar-fan** (one `field` per element):

| field pattern | meaning |
|---|---|
| `depth.bid.{k}.size` | size at bid level `k` (0 = top of book), k = 0..19 |
| `depth.ask.{k}.size` | size at ask level `k`, k = 0..19 |
| `field.r{row}.c{col}` | synthetic 2D scalar field cell (heatmap), row/col 0-based |
| `amp` | synthetic single-channel waveform amplitude (ECG/audio) |

`ohlc` (combined) and `bb_*` aliases are not separately emitted — the candle is fed as the
four scalar keys `open/high/low/close`, which the manifest packs via the **compound** mode.

## How each `field` maps to a DynaCharting buffer write mode

The three write modes are defined in CONTRACT-buffer-id.md. The mock doesn't pick the mode
(the per-view manifest + instruction do); this table documents the **intended** mapping so
the data shape lines up with the buffer model.

| field(s) | write mode | rationale |
|---|---|---|
| `lastPrice`, `vwap`, `sma_20`, `ema_12`, `ema_26`, `rsi`, `macd`, `macdSignal`, `macdHistogram`, `bb_*`, `volume` | **append** | scalar time-series → growing vertex/line buffer; one value appended per tick |
| `open` `high` `low` `close` (as a 4-tuple per bucket) | **compound** | a packed `candle6` record per bucket (≤8 slots, slotBit 0..3) |
| `depth.bid.{k}.size` / `depth.ask.{k}.size` | **fixed** | current-state depth ladder; each level is one binding at a fixed byte offset, overwritten in place each tick (the scalar-fan) |
| `field.r{row}.c{col}` (heatmap) | **fixed** *(scalar-fan)* OR **texture** | small grids ride the scalar-fan as a fixed snapshot (one binding per cell, `offset=(row*cols+col)*4`); large grids should instead be a precomputed colormap texture via `setTexturePixels` (the texture path, outside the buffer contract — see CONTRACT §"Texture-backed views"). The generated `heatmap` dataset here is **small (16×16)** and uses the fixed scalar-fan so it round-trips through the same replay path as everything else. |
| `amp` (waveform) | **append** | single high-cadence scalar series → growing line buffer (`pos2_clip`); x from the paired record index |

### Scalar-fan detail (vector → many scalars)

A depth ladder of 20 levels per side is **not** one vector update; it is 40 scalar updates
per tick (`depth.bid.0.size` … `depth.bid.19.size`, `depth.ask.0.size` … `.19.size`). Each
maps to one `fixed`-mode binding whose `offset = k*4`, all into one pre-sized buffer. The
geometry reads the whole buffer as the live snapshot. Same technique for the heatmap grid
(`field.r{row}.c{col}` → `offset=(row*cols+col)*4`).

## Datasets in this directory

- `market/<SYMBOL>.json` — one file per symbol, plus `market/index.json` listing them. Each
  is a full market tape: OHLCV buckets + `lastPrice` ticks + TA suite + a 20×2 depth ladder.
  See `gen-market.mjs`.
- `synthetic/heatmap.json` — a 16×16 scalar field animated over 20 s, emitted as a
  `field.r{row}.c{col}` fixed scalar-fan. See `gen-synthetic.mjs`.
- `synthetic/ecg.json` — a single high-cadence `amp` waveform (synthetic ECG, ~250 Hz). See
  `gen-synthetic.mjs`.

## Generators & validation

All generators are plain Node ESM (`.mjs`), **zero dependencies**, deterministic (seeded
mulberry32 PRNG). No build step, no app coupling.

```bash
node apps/showcase/data/gen-market.mjs      # writes market/*.json
node apps/showcase/data/gen-synthetic.mjs   # writes synthetic/*.json
node apps/showcase/data/validate.mjs        # validates every dataset; non-zero exit on failure
```

The validator checks, per dataset: `t` monotonic non-decreasing and within
`[0, durationMs]`; every value finite (no `NaN`/`Inf`/`null`) and `f32`-representable;
per-field value ranges sane (e.g. `rsi` in `[0,100]`, prices > 0, sizes ≥ 0); and that every
`field` declared in `meta.fields` actually appears in `updates` (and vice versa).
