# Showcase stills — canvas-only, hardware-rendered, recaptured 2026-09-20

Every PNG in this directory is one frame of the running showcase, read off the engine's own
canvas. They were recaptured by **ENC-1288**; what was here before was captured at `537c995`
(2026-06-11) and was **vertically mirrored**, which is a story worth keeping — see
[Why this directory has a README](#why-this-directory-has-a-readme) below, and LIMITATIONS.md
**DC-L15** (now retired) and **§C6**.

Regenerate with:

```bash
pnpm --filter @repo/showcase dev --port 5650 --strictPort    # in one shell
node apps/showcase/tools/recapture-stills.mjs                # in another
```

## What each file is

| File | What it is |
|---|---|
| `<view-id>.png` | The engine canvas, `canvas.engine-canvas` → `toDataURL('image/png')`. **No DOM or SVG is composited in.** |
| `<view-id>.png.capture.json` | That shot's manifest: adapter, `sha256`, backing store, capture mode, URL, timestamp. |
| `<view-id>.png.probe.json` | What the page said it had drawn, evaluated **in the same eval as the shot** — engine axis, measured domain, leftover chrome DOM counts, canvas sample. |
| `capture-manifest.json` | One row per still: the same facts, plus the DynaCharting commit each was taken against and the replay position it was taken at. |
| `render-tally.json` | Per-view render verdict (`full` / `partial` / `none`), computed from the in-capture canvas sample. |
| `contact-sheet.html` | The gallery page. |
| `contact-sheet.png` | A **page screenshot of that HTML page** — a photo of a document, not evidence about any renderer. It is stamped `captureMode=page`, `tier1Scorable:false`. |

Provenance also lives **inside** each PNG, as uncompressed `tEXt` chunks, so a file separated
from its sidecar can still answer for itself:

```bash
grep -a captureMode apps/showcase/stills/price-line-area.png     # -> captureMode canvas
```

## The three properties these have and the old ones did not

1. **Canvas-only.** The old stills were screenshots of `.single-canvas-region`: the engine
   canvas *plus* the hand-typed chrome overlay composited over it. A DOM overlay in a still can
   pass a tier-1 check on the engine's behalf, and did — the engine drew no axis at all for six
   months while the stills showed one (SPEC §1.3, D10).
2. **A named hardware adapter.** Every frame here was rendered on `vendor: nvidia,
   architecture: ampere`, with `adapter.info.isFallbackAdapter: false` — the property that
   actually discriminates; `GPUAdapter.isFallbackAdapter` does not exist on Chromium 1228 and
   reads `undefined` on software *and* hardware alike (ENC-1263). A software frame looks exactly
   like a hardware one, so `--allow-software` is never passed and the capture aborts rather than
   guessing. Per-still record: `capture-manifest.json` (SPEC D8).
3. **A known point in the replay.** The capture waits for one full pass of the view's own
   transport scrubber — progress seen at or below 5%, then at or above 92% — rather than a fixed
   wait into a ~20s loop. The old tool waited 10s and hoped. Measured 2026-09-20, a *40s* fixed
   dwell landed `price-line-area` thirty pixel-columns into a fresh loop: 30 of 267 records
   drawn, and the orientation fit could not be made at all.

## What you still may not do with them

A still is **one frame of a loop, and the loop moves**. Tier-1 judgements are made on a fresh
canvas-only capture scored against the scene the page declared in the same eval, never on a
committed PNG (SPEC D10). These are a gallery and a coverage record; they are not a scorecard.

Nothing here is a claim about framing quality either — see LIMITATIONS.md **DC-L-1316**, which
is about what the plot-box fit does and does not promise.

## Whether one is upright

You cannot tell by looking, and that is the whole lesson of this directory. Measure it:

```bash
python3 ../tools/still-orientation.py price-line-area      # exit 0 == upright, 1 == mirrored
# -> value edge: top of the fill run   (baseline edge bottom spans 0 px)
# -> fit       : slope -29.24 px/unit   r = -1.0000   median|resid| = 0.2 px
# -> VERDICT   : UPRIGHT
```

and satisfy yourself the instrument can still say no:

```bash
d=$(mktemp -d)
git show 537c995:apps/showcase/stills/price-line-area.png > "$d/old.png"
python3 ../tools/still-orientation.py price-line-area --still "$d/old.png"
# -> VERDICT   : MIRRORED  (exit 1)
```

## Why this directory has a README

Every PNG here used to be upside down, and nothing said so for three months.

They were all captured in one commit — `537c995`, **2026-06-11**. The `EngineHost` blit fix that
put the Y axis the right way up landed at `d6b5acd`, **2026-06-21**, **ten days later**. So every
committed still was a vertically **mirrored** picture of a render that was itself correct. Not a
renderer bug: a stale artifact that nothing labelled as stale.

Nobody caught it because **a mirrored random walk is still a random walk**, and every axis number
beside the geometry was a hand-typed DOM/SVG overlay that flipped with the image. Nothing inside
the frame contradicted the mirror. That is how `price-line-area.png` was read for three months as
*"the area is filled on the wrong side of the line"* and scored a tier-0 (Truthful) failure, when
the fill was correct and the frame was upside down.

**A rendered artifact carries the date of the code that drew it, not the date you look at it.**
`git log -1 -- <image>` against `git log -1 -S<the fix> -- <source>` is the whole test, and it
cost three months here.

The recapture is **ENC-1288**; the SPEC correction was **ENC-1276**; the diagnosis was
**ENC-1250**. See LIMITATIONS.md **DC-L15** (§R) and **§C6**.
