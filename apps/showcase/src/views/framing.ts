/* apps/showcase/src/views/framing.ts — ENC-1273 (chart-quality-bar SPEC D1 tier 2, §1.0)
 *
 * WHERE THE PLOT BOX MEETS A RENDER PATH.
 *
 * ENC-1256 built `plotbox.ts` — a tested, exported, pure frame-fitting module —
 * and its own author logged **DC-L14** against it, because nothing that renders
 * called any of it. Every showcase view baked a literal `transform` into
 * `view.json`, `useViewSwitch` wrote that literal into the engine, and the
 * framing of the product was therefore exactly what a human typed six months
 * ago. ENC-1253 then laid the AXIS out against `plotBox(canvas)` while the DATA
 * stayed on the literal, which is why the leftmost bars were drawn to the left
 * of the box's left edge, under the price labels.
 *
 * This module is the missing half: given a view, it says WHICH pane and WHICH
 * transform the fitted frame belongs to, and whether the view is one the plot
 * box can frame at all. `useViewSwitch` then calls `frameSeries(measuredDomain,
 * canvas)` and applies BOTH halves it returns — `setPaneRegion` and
 * `setTransform` (plotbox.ts contract note 7: they must describe ONE rectangle,
 * or the scissor and the projection drift).
 *
 * ── WHY A VIEW CAN BE REFUSED, AND WHY THAT IS NOT A SHRUG ──────────────────
 *
 * `plotBox()` returns ONE rectangle, and `frameSeries` fits ONE domain into it.
 * A view that stacks panes — `candle-overlays` puts price in clip y
 * [-0.20, 0.95] and a volume sub-pane in [-0.95, -0.30] — needs a LAYOUT: two
 * boxes carved out of one, two transforms, and axis furniture that knows which
 * band it belongs to. None of that exists (the plot box has no concept of a
 * second pane, and `DomainTracker` is not tracking the volume buffer's y at all
 * — `manifest.ts` registers it `axes: 'x'` on purpose). Fitting the price
 * series to the WHOLE box would paint it straight over the volume pane.
 *
 * So a multi-pane view is refused here, by counting its `createPane` commands,
 * and the refusal is reported rather than silently skipped — it is
 * LIMITATIONS.md **DC-L-1273**, not an oversight. The two single-pane views
 * (`candles-aapl`, `ohlc-bars`) are framed.
 *
 * ── TWO FITS, BECAUSE THERE ARE TWO KINDS OF VIEW (ENC-1316) ────────────────
 *
 * ENC-1273 reached 2 of 22 views, because a `frameSeries` fit needs a MEASURED
 * domain (`axisDomain`, ENC-1252) and ONE transform to write it to. Eleven more
 * views draw an axis and have neither: they author their geometry directly in
 * clip space (`audio-waveform`, `ridgeline`, `spectrogram`, `streamgraph`), or
 * bake a data→clip literal across two mirrored transforms (`footprint`,
 * `depth-ladder`, `volume-profile`), or carry one transform but no axis group
 * (`ecg`, `renko`, `scatter`, `price-line-area`). For all eleven the axis
 * furniture was laid out against `plotBox(canvas)` while the data was not, so
 * the two shared pixels: `audio-waveform`'s `'0.0'` tick label scored **1.57:1**
 * against the cyan waveform running behind it INSIDE the 64px label gutter.
 *
 * What every one of them DOES declare is the clip rectangle it draws inside —
 * its pane's `setPaneRegion`, the `±0.95` in its own manifest. So the second fit
 * is a change of RECTANGLE rather than of domain: `fitRegionToBox` maps that
 * rectangle onto the plot box and the result is COMPOSED onto whatever
 * transform the view already authored (`composeTransform`), one per transform
 * the manifest creates, plus one attached to the draw items that have none.
 * Everything is read out of the view's own manifest; no view file gains a
 * literal and nothing here names a view.
 *
 * It is the weaker fit and it is labelled as such: `kind: 'pane'` preserves
 * whatever dead margin the author left inside their own rectangle, where
 * `kind: 'series'` fits the ink itself. What it does guarantee is the property
 * ENC-1316 is about — the data is inside the box and the gutters are the
 * furniture's alone.
 *
 * ── THE OTHER THING THIS TURNS OFF ──────────────────────────────────────────
 *
 * `useReplay`'s `xAnchor` writes its OWN `setTransform` on the first record of
 * every replay pass (`sx = (clipMax-clipMin)/xWindow`, from `view.json`'s
 * hand-typed 150-index window over clip ±0.85). That is the ad-hoc plot box
 * this supersedes, and it is not a fallback that can be left armed: it would
 * overwrite the fitted transform once per loop. `useViewSwitch` therefore stops
 * passing `xAnchor` for a framed view. Nothing is lost — anchoring X to the
 * first record is precisely what fitting the MEASURED x domain does, except
 * measured rather than assumed.
 */

import { FULL_CLIP_REGION, type PaneRegion, type PlotInsets, type Transform2D } from '@repo/dc-wasm';
import type { SceneManifest, SceneCommand } from '../scene/commands';
import type { GrowthSync } from '../engine/useReplay';
import type { AxisSpec } from '../chrome/types';
import type { AxisDomainSpec, ShowcaseView, ViewTransform } from './registry';

/** The ids a fitted frame is applied to, once a view is known to be framable. */
export interface ViewFraming {
  /** The pane whose region becomes `paneRegionFor(box)`. */
  paneId: number;
  /** The transform the fit is written to (the view's primary growth transform). */
  transformId: number;
  /** Per-view gutters. `undefined` ⇒ `DEFAULT_PLOT_INSETS`. */
  insets?: PlotInsets;
}

/**
 * The clip-space re-frame for a view that has no measured domain to fit — the
 * ENC-1316 path. Everything in it is read out of the view's own manifest.
 */
export interface PaneFraming {
  /** The view's single pane. Its region becomes `paneRegionFor(box)`. */
  paneId: number;
  /** The clip rectangle the view authored inside — its own `setPaneRegion`. */
  region: PaneRegion;
  /** Every transform the manifest creates, with the value it authors for it. */
  transforms: { id: number; authored: Transform2D }[];
  /** Draw items the manifest binds with no transform of their own. */
  untransformedDrawItems: number[];
  /** Per-view gutters. `undefined` ⇒ `DEFAULT_PLOT_INSETS`. */
  insets?: PlotInsets;
}

/** Why a view is NOT framed by the plot box. Reported, never silent. */
export type FramingRefusal =
  | 'no-axes' // declares no `chrome.axes`: no furniture, so no box to share
  | 'multi-pane' // stacked panes: one box cannot lay out two (DC-L-1273)
  | 'no-pane'; // the manifest creates no pane to frame

/** `resolveFraming`'s answer: the ids, or the reason there are none. */
export type FramingResolution =
  | { framed: true; kind: 'series'; framing: ViewFraming }
  | { framed: true; kind: 'pane'; paneFraming: PaneFraming }
  | { framed: false; reason: FramingRefusal; detail: string };

/** The slice of a view this decision needs (so tests can pass manifest modules). */
export interface FramableView {
  manifest: SceneManifest;
  growth?: GrowthSync;
  axisDomain?: AxisDomainSpec;
  /** The view's declared axes — a view with none gets no plot box (ENC-1316). */
  axes?: { x?: AxisSpec; y?: AxisSpec };
  /** `view.json`'s baked transform: the value `bakeTransform` would have written. */
  transform?: ViewTransform;
}

/** Every `createPane` id in a manifest, in scene order. */
export function paneIds(manifest: SceneManifest): number[] {
  return manifest.commands
    .filter((c) => c.cmd === 'createPane' && typeof c.id === 'number')
    .map((c) => c.id as number);
}

/** The pane a layer was created on, or null when the layer is not in the manifest. */
export function paneOfLayer(manifest: SceneManifest, layerId: number): number | null {
  for (const c of manifest.commands) {
    if (c.cmd === 'createLayer' && c.id === layerId && typeof c.paneId === 'number') {
      return c.paneId;
    }
  }
  return null;
}

/** The clip rectangle a manifest declares for `paneId` — its LAST `setPaneRegion`. */
export function paneRegionOf(manifest: SceneManifest, paneId: number): PaneRegion {
  let region: PaneRegion = { ...FULL_CLIP_REGION };
  for (const c of manifest.commands) {
    if (c.cmd !== 'setPaneRegion' || c.id !== paneId) continue;
    region = {
      clipXMin: numOr(c.clipXMin, region.clipXMin),
      clipXMax: numOr(c.clipXMax, region.clipXMax),
      clipYMin: numOr(c.clipYMin, region.clipYMin),
      clipYMax: numOr(c.clipYMax, region.clipYMax),
    };
  }
  return region;
}

function numOr(v: unknown, fallback: number): number {
  return typeof v === 'number' && Number.isFinite(v) ? v : fallback;
}

/**
 * Every transform the manifest creates, paired with the value it will hold once
 * the manifest has been applied.
 *
 * Three sources, in the order the engine sees them: the manifest's own
 * `setTransform`, else `view.json`'s baked literal (what `bakeTransform` writes
 * for a view that creates a transform and leaves the values to the app), else
 * identity. Composing onto the WRONG base is the one way this can be silently
 * off, so the base is derived rather than assumed.
 */
export function authoredTransforms(
  manifest: SceneManifest,
  baked?: ViewTransform,
): { id: number; authored: Transform2D }[] {
  const IDENTITY: Transform2D = { sx: 1, sy: 1, tx: 0, ty: 0 };
  const base: Transform2D = baked
    ? { sx: baked.sx, sy: baked.sy, tx: baked.tx, ty: baked.ty }
    : IDENTITY;
  const order: number[] = [];
  const byId = new Map<number, Transform2D>();
  for (const c of manifest.commands) {
    if (typeof c.id !== 'number') continue;
    if (c.cmd === 'createTransform') {
      if (!byId.has(c.id)) {
        order.push(c.id);
        byId.set(c.id, { ...base });
      }
    } else if (c.cmd === 'setTransform' && byId.has(c.id)) {
      byId.set(c.id, {
        sx: numOr(c.sx, 1),
        sy: numOr(c.sy, 1),
        tx: numOr(c.tx, 0),
        ty: numOr(c.ty, 0),
      });
    }
  }
  return order.map((id) => ({ id, authored: byId.get(id)! }));
}

/**
 * Draw items the manifest binds to a pipeline and never gives a transform.
 *
 * Their geometry is authored directly in CLIP space — `audio-waveform`'s
 * envelope, `spectrogram`'s quad — which is exactly why they need one to be
 * re-framed: the clip→clip remap has nowhere else to live. Every Dawn backend
 * resolves `di.transformId`, so attaching one is uniform across pipelines.
 */
export function untransformedDrawItems(manifest: SceneManifest): number[] {
  const created: number[] = [];
  const transformed = new Set<number>();
  for (const c of manifest.commands) {
    if (c.cmd === 'createDrawItem' && typeof c.id === 'number') created.push(c.id);
    if (c.cmd === 'attachTransform' && typeof c.drawItemId === 'number') {
      transformed.add(c.drawItemId);
    }
  }
  return created.filter((id) => !transformed.has(id));
}

/** Does this command bind a draw item to a pipeline? (unbound items draw nothing) */
function isBind(c: SceneCommand): boolean {
  return c.cmd === 'bindDrawItem' && typeof c.drawItemId === 'number';
}

/**
 * Decide whether `view`'s data can be framed by the plot box, and how.
 *
 * Everything here is read out of the view's OWN manifest — the pane comes from
 * the `createLayer` the growth descriptor already names (or from the single
 * `createPane`), the transforms from the manifest's own commands. No view file
 * gains a literal, and adding a view needs no edit here, which is the property
 * `registry.ts` exists to preserve.
 *
 * The order of the questions is the whole decision:
 *
 *   1. no `chrome.axes` at all  -> nothing is laid out against a plot box, so
 *      introducing one would only buy dead margin. REFUSED.
 *   2. more than one pane       -> REFUSED, DC-L-1273 (a layout, not a fit).
 *   3. a measured `axisDomain` AND a growth transform -> the `series` fit.
 *   4. otherwise                -> the `pane` fit (ENC-1316).
 */
export function resolveFraming(view: FramableView): FramingResolution {
  if (!view.axes?.x && !view.axes?.y) {
    return {
      framed: false,
      reason: 'no-axes',
      detail:
        'the view declares no `chrome.axes`, so no furniture is laid out against a ' +
        'plot box and nothing shares pixels with the data; framing it would reserve ' +
        'gutters for an axis that is never drawn',
    };
  }
  const panes = paneIds(view.manifest);
  if (panes.length > 1) {
    return {
      framed: false,
      reason: 'multi-pane',
      detail:
        `the manifest creates ${panes.length} panes (${panes.join(', ')}); ` +
        '`plotBox()` is ONE rectangle and `frameSeries` fits ONE domain into it, so ' +
        'fitting the primary series to the whole box would paint it over the other ' +
        'pane. Stacked layout is LIMITATIONS.md DC-L-1273.',
    };
  }
  if (panes.length === 0) {
    return {
      framed: false,
      reason: 'no-pane',
      detail: 'the manifest creates no pane, so there is no region to frame',
    };
  }

  const growth: GrowthSync | undefined = view.growth;
  if (view.axisDomain && growth && typeof growth.transformId === 'number') {
    const paneId = paneOfLayer(view.manifest, growth.layerId);
    if (paneId === null) {
      return {
        framed: false,
        reason: 'no-pane',
        detail: `no \`createLayer\` with id ${growth.layerId} in the manifest, so its pane is unknown`,
      };
    }
    return { framed: true, kind: 'series', framing: { paneId, transformId: growth.transformId } };
  }

  const paneId = panes[0];
  const bound = new Set(view.manifest.commands.filter(isBind).map((c) => c.drawItemId as number));
  return {
    framed: true,
    kind: 'pane',
    paneFraming: {
      paneId,
      region: paneRegionOf(view.manifest, paneId),
      transforms: authoredTransforms(view.manifest, view.transform),
      // Only BOUND draw items are worth a transform: an unbound one draws
      // nothing, and `attachTransform` on it is a command with no effect.
      untransformedDrawItems: untransformedDrawItems(view.manifest).filter((id) => bound.has(id)),
    },
  };
}

/** `resolveFraming` for a catalog `ShowcaseView` (the running app's shape). */
export function framingFor(view: ShowcaseView | null): FramingResolution | null {
  if (!view) return null;
  return resolveFraming({
    manifest: view.manifest,
    growth: view.growth,
    axisDomain: view.axisDomain,
    axes: view.chrome?.axes,
    transform: view.meta.transform,
  });
}
