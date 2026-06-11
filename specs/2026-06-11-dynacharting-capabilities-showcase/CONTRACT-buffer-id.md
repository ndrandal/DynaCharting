# T0.1 — Buffer-ID contract (ENC-517)

The single agreement shared by **three producers** of each view: the **manifest** (browser SceneDocument, authored in the showcase), the **instruction file** (embassy routing), and the **mock GMA** (data). They agree on one thing: **buffer IDs and how data lands in them.** Builds on `DESIGN-buffer-binding.md`.

## Principle
- The **showcase owns the manifest.** embassy is a generic data pump. embassy's `scene-init` is **ignored** by the showcase (the showcase applies its own catalog manifest; if it applied embassy's too, the shared buffer IDs would double-`createBuffer`).
- Data purity preserved: embassy never mutates values; it only routes them into buffers by ID. All computation is upstream (mock GMA / datasets).

## Per-view file layout
Each view is a directory under `apps/showcase/views/<view-id>/`:
```
manifest.json      # SceneDocument: panes/layers/buffers/geometries/drawItems/transforms (browser applies via applyControl)
instruction.json   # embassy: { id, sessionId, subscriptions[], visualization(showcase-explicit-v1) }
view.md            # explainer copy: title, reference tool, "what's going on", tier, data+pipeline facts
view.json          # metadata: { id, title, tier: native|composed|walled, referenceTool, datasetId }
```
The **mock GMA** reads `datasetId` + the instruction's subscriptions to know which keys to stream.

## Buffer-ID namespacing
Per-view buffer IDs are local to that view's manifest+instruction (the engine is scene-reset between views, IDs reused). Convention within a view:
- `1xx` panes/layers/transforms (structural), `5xx` time-series data buffers, `6xx` vector/fixed buffers, `7xx` compound (packed) buffers, `8xx` texture-backed (see texture contract). Geometry/drawItem IDs `2xx`/`3xx`. (Soft convention for readability; the only hard rule is manifest buffer IDs == instruction binding bufferIds.)

## The three write modes (must match across manifest ↔ instruction)
| Mode | embassy route | Data shape | Buffer grows? | Manifest geometry reads |
|---|---|---|---|---|
| **append** | `routeSimple` (exists) | scalar time-series (price, TA, line/candle-overlay) | yes, monotonic | a growing vertex buffer (e.g. `pos2_clip` line, `candle6`) |
| **compound** | `routeCompound` (exists, **≤8 slots**) | packed multi-field record per tick/bucket (OHLC candle6) | yes, per record | one packed-format buffer (`candle6`, `rect4`) |
| **fixed** | `routeFixedUpdate` (NEW, T3.3) | current-state vector (depth ladder, profile snapshot, heatmap row) | no, overwritten in place | a fixed-size buffer read as the live snapshot |

**Rule of thumb:** time-series → append; ≤8-field packed records → compound; wide/current-state vectors → fixed (one subscription per element, the "scalar-fan").

## Instruction `visualization` schema (recipe `showcase-explicit-v1`)
```json
{ "recipe": "showcase-explicit-v1",
  "bindings": [
    {"slot":"<name>", "requestId":"<gma-req-id>", "bufferId":<u32>,
     "mode":"append|compound|fixed",
     "offset":<u32>,           // mode=fixed: byte offset of this element in the buffer
     "stride":<u32>, "intraOffset":<u32>, "slotBit":<u8>   // mode=compound
    } ],
  "buffers": [
    {"bufferId":<u32>, "stride":<u32>, "fullMask":<u8>,
     "staticFields":[{"intraOffset":<u32>,"kind":"recordIndex|recordIndexPlusConst|const","value":<f32>}] }
  ] }
```
- Every `requestId` in `bindings` must appear in `subscriptions`.
- Every `bufferId` used must be declared (with `byteLength`) in the manifest.
- `compound` bindings sharing a `bufferId` must list a matching `buffers[]` entry; `fullMask` = OR of their `(1<<slotBit)`.
- `fixed` buffers must be pre-sized in the manifest (`byteLength` = elementCount × 4) so UPDATE_RANGE offsets are valid.

## Manifest ↔ instruction agreement (the lint, T3.5)
1. Every instruction `bufferId` is declared in the manifest `buffers`.
2. Every manifest data buffer is targeted by ≥1 binding (no orphan buffers).
3. `fixed` buffer `byteLength` ≥ max(offset)+4 across its bindings.
4. `compound` buffer `byteLength` is a multiple of `stride`; all contributing strides equal.
5. Geometry `format` is compatible with how data lands (e.g. `candle6` ↔ compound stride 24; `pos2_clip` ↔ append pairs).

## Worked examples
- **Line (vwap), append:** manifest buffer `500` (pos2_clip), geometry reads `500`; instruction `{slot:"vwap",requestId:"r-vwap",bufferId:500,mode:"append"}`; mock streams `vwap` scalars. *(But pos2_clip needs (x,y) pairs — append streams y only; x comes from a paired index buffer or the producer emits (x,y) pairs. Resolved per-view in Phase 4; simplest: producer emits interleaved (recordIndex, value) via a 2-wide append.)*
- **Candle (OHLC), compound:** manifest buffer `700` (candle6, byteLength = N×24), geometry `instancedCandle@1`; 4 bindings (open/high/low/close) mode=compound stride=24 slotBit 0..3, buffers[`700`] fullMask=15 + staticFields recordIndex(x)+halfWidth; mock streams `open/high/low/close` per bucket.
- **Depth ladder (20 levels), fixed:** manifest buffer `600` (rect4 or a fixed array, byteLength = 20×4), geometry reads all 20; 20 bindings `depth.bid.k` mode=fixed offset=k×4; mock streams `depth.bid.0..19.size` each tick → overwrite in place → live snapshot.

(Texture-backed views — heatmap/spectrogram/weather — use a separate path: `setTexturePixels` from a precomputed colormap texture, see T3.6; the data buffer contract above does not apply to them.)
