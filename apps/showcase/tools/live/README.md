# Live fixtures — driving the showcase off a real data plane (ENC-1282)

The 22 views in `apps/showcase/views/` replay a **captured tape**, and a tape can only be timed
by when this client observed each frame: its basis is fitted and reports `epochKnown: false`.
These two fixtures exist so the **live** path — the one that carries the producer's own clock —
can be stood up and re-measured without reconstructing it from a transcript.

They are embassy `EMBASSY_INSTRUCTION_FILE` inputs, not showcase view files. The showcase owns
its manifest (`CONTRACT-buffer-id.md`); what it takes off the socket is the records and the
per-buffer `timeBasis`. Buffer `10100` and `stride: 24` are what tie them to
`views/candles-aapl/manifest.ts` — change one and change the other.

| File | Declares | What the axis does |
|---|---|---|
| `candles-live.instruction.json` | `TumblingWindow periodMs 1000` → `VectorReducer first/max/min/last` | a basis with `epochKnown: true`; x tick labels are wall-clock instants |
| `candles-live-nobasis.instruction.json` | the same four subscriptions with **no pipeline** | no basis at all (never `periodMs: 0`) — the time axis is **dropped**, not captioned |

The second is the negative control, and it is one field's difference from the first.

## Standing it up

Four processes. Pick ports nothing else on the box holds — several sessions run stacks here —
and never reuse a bare temp path.

```bash
# 1. the engine. NOTE: gma_server ignores argv[3] for the ingress port (ENC-1331),
#    so the feed port has to be set in a copy of the conf, not on the command line.
sed 's/^feedPort = 9001/feedPort = 19191/' GMA_V3/src/util/gma.conf > /tmp/<ENC>-gma.conf
GMA_V3/build/gma_server 14090 /tmp/<ENC>-gma.conf 19191 &

# 2. a feed into it — GMA_V3's own injector, which carries NEXO/VALT/BLITZ/QBIT/FLUX
node GMA_V3/tools/smoke-test/feed-inject.js localhost 19191 100 &

# 3. embassy, forum-less, on this fixture
EMBASSY_GMA_WS_URL=ws://localhost:14090 EMBASSY_FORUM_URL= \
EMBASSY_INSTRUCTION_FILE=$PWD/apps/showcase/tools/live/candles-live.instruction.json \
EMBASSY_DATA_ADDR=127.0.0.1:14091 EMBASSY_ADMIN_ADDR=127.0.0.1:14092 \
EMBASSY_DATA_ALLOWED_ORIGINS=http://localhost:15554 embassy &

# 4. the showcase, pointed at it
VITE_SHOWCASE_AGENT_URL=ws://localhost:14091/data VITE_SHOWCASE_AGENT_SESSION=showcase \
  pnpm --filter @repo/showcase dev --port 15554 --strictPort
```

Then read `window.__dcAxisDomain['candles-aapl'].x.time` — or shoot it canvas-only with
`specs/2026-09-19-chart-quality-bar/harness/shoot-live.mjs --canvas 'canvas.engine-canvas'`.

## Four things that will waste an hour if you do not know them

1. **embassy sends a connection NOTHING until it names a session.** The `/data` socket's only
   inbound message is `{"type":"subscribe","sessionId":"showcase"}`; the scene-init envelope and
   every buffer snapshot are enqueued inside that call. A connection that never subscribes stays
   open and silent, which looks exactly like a feed with no data.
2. **`EMBASSY_DATA_ADDR` must be loopback** unless you set `EMBASSY_DATA_TICKET_PUBLIC_KEY`;
   embassy refuses to bind a non-loopback data plane unauthenticated, and says so.
3. **This feed carries `lastPrice`, not `highPrice`/`lowPrice`.** Both fixtures build all four
   OHLC slots out of `lastPrice` with different `VectorReducer` functions, which is a real bar
   from the real tick stream. Subscribing to a field the feed does not produce yields a slot that
   never fires, and a compound record completes only when every slot has — so the dataplane goes
   silent with no error anywhere.
4. **`baseMs` is learned from the producer's FIRST bucket stamp**, and the corrected envelope is
   given only to *future* subscribers (embassy `UpdateSceneSnapshot`, deliberately silent —
   pushing it would zero the buffers it just learned from, ENC-1101). Let embassy run a few
   seconds before you open the browser, or the first frame you get is
   `{"baseMs":0,"epochKnown":false}` and it will not correct itself until you reload.
