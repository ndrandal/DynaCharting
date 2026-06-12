---
title: Renko — fixed-brick (live)
referenceTool: Renko / Point & Figure
tier: composed
---

A time-less Renko chart of AAPL that forms **live**: price is quantised into fixed-size bricks — a brick is drawn for every brick-sized close move, advancing one column per brick — so the x-axis tracks *movement*, not the clock. The brick walk (`brickSize = priceRange / 14`, closes walked boundary-by-boundary into axis-aligned rects) is reproduced offline by a generator, but instead of one static upload each brick is emitted as a `rect4` **APPEND** frame stamped with the timestamp of the close that produced it. The replay engine streams those frames over a ~20s timeline, so the wall **builds up progressively** as the simulated price crosses brick thresholds.

| | |
|---|---|
| **DATA** | AAPL · close series (40 buckets) · quantised to ~25 bricks |
| **TECHNIQUE** | offline brick-walk → streamed `rect4` APPEND timeline (replay) |
| **PIPELINE** | `instancedRect@1` (one rect4 per brick) |
| **GEOMETRY** | `rect4` (x0,y0,x1,y1) appended one brick at a time |
| **BUFFERS** | `601` rect4 brick buffer — starts empty, grows live |
| **COLOR** | single green accent (instancedRect = one uniform color per draw item) |

**What's going on (the technique).** This view used to be the *static* counterpart to a streaming candle chart — its entire shape was resolved once at build time and baked as static `uploads`. It is now **live**: `.gen.mjs` walks the AAPL closes exactly as the static build did, but emits each brick as a 16-byte `rect4` APPEND record at the timestamp of its producing close, written into `records.json` as a `{t, b64}` timeline. At view-select the manifest declares an **empty** brick buffer (`byteLength 0`) and an `instancedRect@1` geometry whose `vertexCount` the replay engine advances as records land (the `growth` descriptor — a fresh-geometry rebind per ENC-558, so the backend re-reads the grown buffer). A single baked data→clip transform frames the full column/price range up front, so the first bricks appear on-screen and the wall fills left-to-right: a few bricks by ~2s, most by ~10s, the full ~25-brick wall by ~20s — the same fixed-brick technique, now animating.
