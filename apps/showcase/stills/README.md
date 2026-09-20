# ⚠ These stills are UPSIDE DOWN. Do not score them.

Every PNG in this directory was captured in one commit — `537c995`, **2026-06-11**. The
`EngineHost` blit fix that put the Y axis the right way up landed in `d6b5acd`, **2026-06-21**,
**ten days later** (LIMITATIONS.md §R L2, DC-L05). So all 23 view stills — and the contact sheet
built from them — are vertically **mirrored** pictures of renders that were themselves correct.

This is not a renderer bug. It is a stale artifact that nothing labelled as stale.

```bash
git log -1 --format='%h %ad' --date=short -- .                        # 537c995 2026-06-11
git log -1 --format='%h %ad' --date=short -S 'fbH - 1 - y' \
  -- ../../../packages/dc-wasm/src/EngineHost.ts                      # d6b5acd 2026-06-21
```

## Why you cannot just look at them and tell

A mirrored random walk is still a random walk, and every axis number beside the geometry is a
hand-typed DOM/SVG overlay (`specs/2026-09-19-chart-quality-bar/SPEC.md` §1.3) that flips with
the image. **Nothing inside the frame contradicts the mirror.** That is how `price-line-area.png`
was read for three months as *"the area is filled on the wrong side of the line"* and scored a
tier-0 (Truthful) failure, when the fill was correct and the frame was upside down
(LIMITATIONS.md **§C6**, ENC-1250).

Orientation has to be **measured** against the data the capture replayed:

```bash
python3 ../tools/still-orientation.py price-line-area     # exit 1 == mirrored
# -> fit       : slope +36.40 px/unit   r = +0.9895   median|resid| = 0.3 px
# -> predicted : upright -36.43   mirrored +36.43
# -> VERDICT   : MIRRORED
```

## What to use instead

| You want to know | Use |
|---|---|
| Does a mark depict its data (tier 0)? | `bash ../../../scripts/tier0.sh` — asserts on the **presented** raster and ships the mirror as a negative control |
| What the renderer draws today | Re-render. These files are a 2026-06 record of *what* was drawn, not of *how it looked*. |
| Whether one still is mirrored | `python3 ../tools/still-orientation.py <view>` |

Recapturing the gallery needs `../tools/capture.mjs`, whose `EMBASSY_REPO` path does not exist on
this machine. Until that happens, treat everything here as evidence of pipeline coverage only —
never of orientation, fill direction, or anything else with a vertical sense.

See **LIMITATIONS.md DC-L14**.
