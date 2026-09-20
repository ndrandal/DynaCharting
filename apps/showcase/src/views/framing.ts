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

import type { PlotInsets } from '@repo/dc-wasm';
import type { SceneManifest } from '../scene/commands';
import type { GrowthSync } from '../engine/useReplay';
import type { AxisDomainSpec, ShowcaseView } from './registry';

/** The ids a fitted frame is applied to, once a view is known to be framable. */
export interface ViewFraming {
  /** The pane whose region becomes `paneRegionFor(box)`. */
  paneId: number;
  /** The transform the fit is written to (the view's primary growth transform). */
  transformId: number;
  /** Per-view gutters. `undefined` ⇒ `DEFAULT_PLOT_INSETS`. */
  insets?: PlotInsets;
}

/** Why a view is NOT framed by the plot box. Reported, never silent. */
export type FramingRefusal =
  | 'no-axis-domain' // declares no ENC-1252 axis group: nothing measures its domain
  | 'no-growth-transform' // no primary growth series, so no transform to fit
  | 'multi-pane' // stacked panes: one box cannot lay out two (DC-L-1273)
  | 'no-pane'; // the growth layer's pane is not in the manifest

/** `resolveFraming`'s answer: the ids, or the reason there are none. */
export type FramingResolution =
  | { framed: true; framing: ViewFraming }
  | { framed: false; reason: FramingRefusal; detail: string };

/** The slice of a view this decision needs (so tests can pass manifest modules). */
export interface FramableView {
  manifest: SceneManifest;
  growth?: GrowthSync;
  axisDomain?: AxisDomainSpec;
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

/**
 * Decide whether `view`'s data can be framed by the plot box, and to which ids.
 *
 * Everything here is read out of the view's OWN manifest — the pane comes from
 * the `createLayer` the growth descriptor already names, the transform from the
 * growth descriptor itself. No view file gains a literal, and adding a view
 * needs no edit here, which is the property `registry.ts` exists to preserve.
 */
export function resolveFraming(view: FramableView): FramingResolution {
  if (!view.axisDomain) {
    return {
      framed: false,
      reason: 'no-axis-domain',
      detail:
        'the view declares no `axisDomain`, so nothing MEASURES its domain — ' +
        'framing it would mean fitting to the literal this replaces (ENC-1252)',
    };
  }
  const growth: GrowthSync | undefined = view.growth;
  if (!growth || typeof growth.transformId !== 'number') {
    return {
      framed: false,
      reason: 'no-growth-transform',
      detail: 'the view declares no primary `growth` series, so there is no transform to fit',
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
  const paneId = paneOfLayer(view.manifest, growth.layerId);
  if (paneId === null) {
    return {
      framed: false,
      reason: 'no-pane',
      detail: `no \`createLayer\` with id ${growth.layerId} in the manifest, so its pane is unknown`,
    };
  }
  return { framed: true, framing: { paneId, transformId: growth.transformId } };
}

/** `resolveFraming` for a catalog `ShowcaseView` (the running app's shape). */
export function framingFor(view: ShowcaseView | null): FramingResolution | null {
  if (!view) return null;
  return resolveFraming({ manifest: view.manifest, growth: view.growth, axisDomain: view.axisDomain });
}
