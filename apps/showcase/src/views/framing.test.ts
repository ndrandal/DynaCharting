/* apps/showcase/src/views/framing.test.ts — ENC-1273 (chart-quality-bar D1 tier 2)
 *
 * THE BEFORE AND THE AFTER, ON THE REAL CAPTURES.
 *
 * Like `deriveAxes.test.ts`, nothing here is a fixture: every number is folded
 * out of the views' committed `records.json` — the captured embassy dataplane
 * frames — through the same `DomainTracker` the running showcase uses, driven
 * by the same `axisDomain` each view's own manifest exports. The "before"
 * transform is not a straw man either: it is `effectiveTransform` applied to the
 * view's real `view.json` literal and its real `xAnchor`, i.e. exactly the
 * framing the app shipped with, reconstructed from the files that produced it.
 *
 * What it demonstrates:
 *
 *   1. The shipped framing FAILS `checkTier2Framing`, and the ink escapes the
 *      plot box on the left — which is the visible symptom ENC-1253 measured,
 *      the leftmost bars drawn under the price labels.
 *   2. `frameSeries(measuredDomain, canvas)` passes tier 2 on every canvas from
 *      a phone to a 4K panel, with the ink inside the box on all four sides.
 *   3. The pane region and the transform describe ONE rectangle (plotbox.ts
 *      contract note 7) — the half a caller is most likely to forget.
 *   4. A stacked multi-pane view is REFUSED with a reason, not silently
 *      mis-framed (DC-L-1273).
 */
import { describe, it, expect } from 'vitest';
import { readFileSync } from 'node:fs';
import {
  DEFAULT_PLOT_INSETS,
  DomainTracker,
  checkTier2Framing,
  fitRegionToBox,
  frameSeries,
  framingMetrics,
  plotBox,
  type CanvasSize,
  type ObservedDomain,
} from '@repo/dc-wasm';

import * as candles from '../../views/candles-aapl/manifest';
import * as ohlc from '../../views/ohlc-bars/manifest';
import * as overlays from '../../views/candle-overlays/manifest';
import { effectiveTransform } from '../chrome/mapping';
import { firstRecordX } from '../chrome/firstRecordX';
import { VIEWS, defineView } from './registry';
import { framingFor, paneIds, paneOfLayer, resolveFraming } from './framing';
import { SHOWCASE_FIT_TRANSFORM_ID } from './useViewSwitch';
import type { Records } from '../engine/useReplay';
import type { ViewMeta } from './registry';

/** The committed capture, decoded the way the browser replay path decodes it. */
function loadRecords(viewId: string): Records {
  const path = new URL(`../../views/${viewId}/records.json`, import.meta.url);
  return JSON.parse(readFileSync(path, 'utf8')) as Records;
}

function loadMeta(viewId: string): ViewMeta {
  const path = new URL(`../../views/${viewId}/view.json`, import.meta.url);
  return JSON.parse(readFileSync(path, 'utf8')) as ViewMeta;
}

function framesOf(records: Records): ArrayBuffer[] {
  return records.frames.map((f) => {
    const bin = atob(f.b64);
    const bytes = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
    return bytes.buffer;
  });
}

type ManifestModule = typeof candles;

/** A real catalog view, assembled exactly as `registry.ts` assembles it. */
function buildView(viewId: string, mod: ManifestModule) {
  const records = loadRecords(viewId);
  return defineView({ meta: loadMeta(viewId), module: mod, records, explainer: '' });
}

/** The domain the capture actually contains, per the view's own axis group. */
function measuredDomain(viewId: string, mod: ManifestModule): ObservedDomain {
  const tracker = new DomainTracker(mod.axisDomain.sources);
  for (const f of framesOf(loadRecords(viewId))) tracker.observe(f);
  return tracker.domain();
}

/**
 * The framing the app SHIPPED with: the view.json literal, X re-anchored from
 * the first replayed record the way `useReplay`/`mapping.ts` did it.
 */
function shippedTransform(viewId: string, mod: ManifestModule) {
  const view = buildView(viewId, mod);
  const fx = view.xAnchor ? firstRecordX(view.records, view.growth) : null;
  return effectiveTransform(view.meta.transform, view.xAnchor, fx);
}

/** The canvas sizes every framing claim below is made over. */
const CANVASES: { name: string; canvas: CanvasSize }[] = [
  { name: 'small (400x300)', canvas: { width: 400, height: 300 } },
  { name: 'laptop (1280x800)', canvas: { width: 1280, height: 800 } },
  { name: 'the showcase capture (1280x800 @dpr2)', canvas: { width: 2560, height: 1600 } },
  { name: 'wide (3840x1200)', canvas: { width: 3840, height: 1200 } },
];

const SINGLE_PANE: { id: string; mod: ManifestModule }[] = [
  { id: 'candles-aapl', mod: candles },
  { id: 'ohlc-bars', mod: ohlc as unknown as ManifestModule },
];

/** The `chrome.axes` a view declares, as `resolveFraming` takes them. */
function axesOf(viewId: string) {
  return loadMeta(viewId).chrome?.axes;
}

describe('resolveFraming — which views the plot box can frame, and why', () => {
  it('SERIES-fits the two views that measure a domain, from their own manifest', () => {
    const c = resolveFraming({ ...candles, axes: axesOf('candles-aapl') });
    expect(c).toEqual({
      framed: true,
      kind: 'series',
      framing: { paneId: 10000, transformId: 10050 },
    });
    // …and the pane id was DERIVED, not typed here: the growth layer's pane.
    expect(paneOfLayer(candles.manifest, candles.growth.layerId)).toBe(10000);
    expect(paneIds(candles.manifest)).toEqual([10000]);

    const o = resolveFraming({
      ...(ohlc as unknown as ManifestModule),
      axes: axesOf('ohlc-bars'),
    });
    expect(o.framed).toBe(true);
    if (!o.framed) throw new Error('unreachable');
    expect(o.kind).toBe('series');
  });

  it('REFUSES the stacked two-pane view, and says which panes made it stacked', () => {
    const r = resolveFraming({
      ...(overlays as unknown as ManifestModule),
      axes: axesOf('candle-overlays'),
    });
    expect(r.framed).toBe(false);
    if (r.framed) throw new Error('unreachable');
    expect(r.reason).toBe('multi-pane');
    expect(r.detail).toContain('10000');
    expect(r.detail).toContain('10002');
    expect(r.detail).toContain('DC-L-1273');
    // The refusal is a fact about the manifest, not a hardcoded exception list.
    expect(paneIds(overlays.manifest)).toEqual([10000, 10002]);
  });

  it('refuses a view that declares no axes — a box with nothing in its gutters', () => {
    const r = resolveFraming({ manifest: candles.manifest, growth: candles.growth });
    expect(r.framed).toBe(false);
    if (r.framed) throw new Error('unreachable');
    expect(r.reason).toBe('no-axes');
  });

  it('PANE-fits a view that draws an axis but measures no domain (ENC-1316)', () => {
    // The same manifest with its `axisDomain` withheld: there is nothing to fit
    // a domain to, but there is still a rectangle to re-frame — and before this
    // ticket that case was refused outright, which is how eleven views that
    // draw an axis ended up with their data in the label gutter.
    const r = resolveFraming({
      manifest: candles.manifest,
      growth: candles.growth,
      axes: axesOf('candles-aapl'),
      transform: loadMeta('candles-aapl').transform,
    });
    expect(r.framed).toBe(true);
    if (!r.framed || r.kind !== 'pane') throw new Error('expected a pane fit');
    expect(r.paneFraming.paneId).toBe(10000);
    expect(r.paneFraming.region).toEqual({
      clipXMin: -0.95,
      clipXMax: 0.95,
      clipYMin: -0.95,
      clipYMax: 0.95,
    });
  });

  it('answers for a whole catalog view too (the shape the app passes)', () => {
    expect(framingFor(null)).toBeNull();
    expect(framingFor(buildView('candles-aapl', candles))?.framed).toBe(true);
  });
});

describe('ENC-1316 — every view that draws an axis is framed, and nothing else is', () => {
  /** Views whose `chrome.axes` declare at least one axis. */
  const withAxes = VIEWS.filter((v) => v.chrome?.axes?.x || v.chrome?.axes?.y);
  const withoutAxes = VIEWS.filter((v) => !(v.chrome?.axes?.x || v.chrome?.axes?.y));

  it('the catalog splits the way the scorecard measured it', () => {
    // 22 views; 14 draw an axis (SCORECARD.md Table D), 8 declare none.
    expect(VIEWS).toHaveLength(22);
    expect(withAxes).toHaveLength(14);
    expect(withoutAxes).toHaveLength(8);
  });

  it('frames all 14 axis-drawing views except the stacked one', () => {
    const framedIds: string[] = [];
    const refused: { id: string; reason: string }[] = [];
    for (const v of withAxes) {
      const r = framingFor(v)!;
      if (r.framed) framedIds.push(v.id);
      else refused.push({ id: v.id, reason: r.reason });
    }
    expect(framedIds).toHaveLength(13);
    // `candle-overlays` is the ONE refusal, and it is refused by DC-L-1273's
    // rule (two panes need a layout, not a fit) rather than by name.
    expect(refused).toEqual([{ id: 'candle-overlays', reason: 'multi-pane' }]);
  });

  it('frames NO view that declares no axes — no gutters for an absent axis', () => {
    for (const v of withoutAxes) {
      const r = framingFor(v)!;
      expect(r.framed).toBe(false);
      if (r.framed) throw new Error('unreachable');
      expect(r.reason).toBe('no-axes');
    }
  });

  it('every framed view lands its data inside the plot box, on every canvas', () => {
    for (const v of withAxes) {
      const r = framingFor(v)!;
      if (!r.framed || r.kind !== 'pane') continue;
      for (const { canvas } of CANVASES) {
        const box = plotBox(canvas, DEFAULT_PLOT_INSETS);
        const remap = fitRegionToBox(r.paneFraming.region, box);
        const region = r.paneFraming.region;
        // The view's own rectangle, carried through the remap, IS the box.
        expect(region.clipXMin * remap.sx + remap.tx).toBeCloseTo(box.x.min, 9);
        expect(region.clipXMax * remap.sx + remap.tx).toBeCloseTo(box.x.max, 9);
        expect(region.clipYMin * remap.sy + remap.ty).toBeCloseTo(box.y.min, 9);
        expect(region.clipYMax * remap.sy + remap.ty).toBeCloseTo(box.y.max, 9);
        // …and the gutters are then empty of data by construction: nothing the
        // view drew inside its region can reach the label band.
        expect(box.x.min).toBeGreaterThan(-1);
      }
    }
  });

  it('gives every bound draw item a transform to be re-framed by', () => {
    // The failure this rules out is silent: a draw item with no transform
    // ignores the remap, so the axis moves and the data does not.
    for (const v of withAxes) {
      const r = framingFor(v)!;
      if (!r.framed || r.kind !== 'pane') continue;
      const pf = r.paneFraming;
      const covered = new Set<number>([...pf.untransformedDrawItems]);
      for (const c of v.manifest.commands) {
        if (c.cmd === 'attachTransform' && typeof c.drawItemId === 'number') {
          covered.add(c.drawItemId);
        }
      }
      const bound = v.manifest.commands
        .filter((c) => c.cmd === 'bindDrawItem' && typeof c.drawItemId === 'number')
        .map((c) => c.drawItemId as number);
      expect(bound.length).toBeGreaterThan(0);
      for (const id of bound) expect(covered.has(id)).toBe(true);
    }
  });

  it("reads each transform's AUTHORED value, so the remap composes onto it", () => {
    // `ecg` sets its transform in the manifest; `price-line-area` creates one
    // and leaves the values to `view.json`. Composing onto the wrong base is
    // the one way this is silently off, so both bases are asserted.
    const ecg = framingFor(VIEWS.find((v) => v.id === 'ecg')!)!;
    if (!ecg.framed || ecg.kind !== 'pane') throw new Error('expected a pane fit');
    expect(ecg.paneFraming.transforms).toHaveLength(1);
    expect(ecg.paneFraming.transforms[0].authored.tx).toBeCloseTo(-0.92, 9);

    const pla = framingFor(VIEWS.find((v) => v.id === 'price-line-area')!)!;
    if (!pla.framed || pla.kind !== 'pane') throw new Error('expected a pane fit');
    const baked = VIEWS.find((v) => v.id === 'price-line-area')!.meta.transform!;
    expect(pla.paneFraming.transforms[0].authored).toEqual({
      sx: baked.sx,
      sy: baked.sy,
      tx: baked.tx,
      ty: baked.ty,
    });
  });

  it('the synthesised fit transform can collide with nothing in the catalog', () => {
    // Above every hand-picked manifest id and below the engine axis's allocator
    // base (900000), so a stray id in a scene dump is attributable at a glance.
    const ids = VIEWS.flatMap((v) =>
      v.manifest.commands.filter((c) => typeof c.id === 'number').map((c) => c.id as number),
    );
    expect(ids.length).toBeGreaterThan(100);
    expect(Math.max(...ids)).toBeLessThan(SHOWCASE_FIT_TRANSFORM_ID);
    expect(SHOWCASE_FIT_TRANSFORM_ID).toBeLessThan(900000);
  });
});

describe('the framing the showcase SHIPPED — the state DC-L14 describes', () => {
  for (const { id, mod } of SINGLE_PANE) {
    it(`${id}: the baked literal fails tier 2 on the domain the capture contains`, () => {
      const canvas = { width: 1280, height: 800 };
      const domain = measuredDomain(id, mod);
      const box = plotBox(canvas, DEFAULT_PLOT_INSETS);
      const m = framingMetrics(
        { x: domain.x!, y: domain.y! },
        shippedTransform(id, mod),
        box,
        canvas,
      );
      const verdict = checkTier2Framing(m);
      expect(verdict.pass).toBe(false);
      expect(verdict.failures.join(' | ')).toMatch(/fill ratio|dead margin|edge clearance/);
    });

    it(`${id}: THE SYMPTOM — the tape runs off the right of the canvas entirely`, () => {
      const canvas = { width: 1280, height: 800 };
      const domain = measuredDomain(id, mod);
      const m = framingMetrics(
        { x: domain.x!, y: domain.y! },
        shippedTransform(id, mod),
        plotBox(canvas, DEFAULT_PLOT_INSETS),
        canvas,
      );
      // The literal's X window is 150 record-indices (view.json's `xAnchor`
      // default over clip ±0.85); the capture runs to index 270. So the ink
      // ends at clip ~2.17 — off the render target, not merely off the box —
      // and what you see is the pane scissor cutting it at ±0.95. DC-L13
      // part (2)'s "~44% of the stated domain is off-frame", in clip units.
      expect(m.ink.x.max).toBeGreaterThan(1);
      expect(m.edgeClearancePx.right).toBeLessThan(0);
      // Vertically the opposite failure: the price band covers about half the
      // height, which is the dead margin tier 2 measures.
      expect(m.ink.y.max - m.ink.y.min).toBeLessThan(1.2);
    });

    it(`${id}: and on a narrow canvas its leftmost ink IS under the price labels`, () => {
      // The left symptom ENC-1253 reported is canvas-width dependent, because
      // the gutter is a fixed 64 CSS px while the literal's left edge is a
      // fixed clip -0.85. They cross at a canvas width of ~853px: narrower
      // than that and the first bar is drawn inside the y-tick-label band.
      const narrow = { width: 700, height: 500 };
      const wide = { width: 1280, height: 800 };
      const domain = measuredDomain(id, mod);
      const t = shippedTransform(id, mod);
      const inkXMin = domain.x!.min * t.sx + t.tx;
      expect(inkXMin).toBeLessThan(plotBox(narrow, DEFAULT_PLOT_INSETS).x.min);
      expect(inkXMin).toBeGreaterThan(plotBox(wide, DEFAULT_PLOT_INSETS).x.min);
      // Fitted, it is inside the box at BOTH sizes — the width-dependence goes
      // away because the fit is a function of the canvas, not a constant.
      for (const canvas of [narrow, wide]) {
        const framed = frameSeries(domain, canvas);
        expect(framed.metrics!.ink.x.min).toBeGreaterThanOrEqual(framed.box.x.min - 1e-9);
      }
    });
  }
});

describe('frameSeries on the render path — the framing after ENC-1273', () => {
  for (const { id, mod } of SINGLE_PANE) {
    for (const { name, canvas } of CANVASES) {
      it(`${id} @ ${name}: tier 2 is green`, () => {
        const domain = measuredDomain(id, mod);
        const framed = frameSeries(domain, canvas);
        expect(framed.transform).not.toBeNull();
        expect(framed.metrics).not.toBeNull();
        const verdict = checkTier2Framing(framed.metrics!);
        expect(verdict.failures).toEqual([]);
        expect(verdict.pass).toBe(true);
      });
    }

    it(`${id}: every side of the ink is INSIDE the box — the symptom is gone`, () => {
      const canvas = { width: 1280, height: 800 };
      const framed = frameSeries(measuredDomain(id, mod), canvas);
      const box = framed.box;
      const ink = framed.metrics!.ink;
      expect(ink.x.min).toBeGreaterThanOrEqual(box.x.min - 1e-9);
      expect(ink.x.max).toBeLessThanOrEqual(box.x.max + 1e-9);
      expect(ink.y.min).toBeGreaterThanOrEqual(box.y.min - 1e-9);
      expect(ink.y.max).toBeLessThanOrEqual(box.y.max + 1e-9);
      // …and the left clearance is now the whole price-label gutter.
      expect(framed.metrics!.edgeClearancePx.left).toBeCloseTo(DEFAULT_PLOT_INSETS.left, 6);
    });

    it(`${id}: the pane region and the transform describe ONE rectangle (note 7)`, () => {
      const canvas = { width: 1280, height: 800 };
      const framed = frameSeries(measuredDomain(id, mod), canvas);
      expect(framed.paneRegion).toEqual({
        clipXMin: framed.box.x.min,
        clipXMax: framed.box.x.max,
        clipYMin: framed.box.y.min,
        clipYMax: framed.box.y.max,
      });
      // The scissor cannot crop the fit: the ink is inside the region it clips to.
      const ink = framed.metrics!.ink;
      expect(ink.x.min).toBeGreaterThanOrEqual(framed.paneRegion.clipXMin - 1e-9);
      expect(ink.x.max).toBeLessThanOrEqual(framed.paneRegion.clipXMax + 1e-9);
    });

    it(`${id}: the fit is a MEASUREMENT — a third of the tape frames differently`, () => {
      const canvas = { width: 1280, height: 800 };
      const tracker = new DomainTracker(mod.axisDomain.sources);
      const frames = framesOf(loadRecords(id));
      for (const f of frames.slice(0, Math.floor(frames.length / 3))) tracker.observe(f);
      const partial = frameSeries(tracker.domain(), canvas);
      const full = frameSeries(measuredDomain(id, mod), canvas);
      expect(partial.transform!.sx).not.toBeCloseTo(full.transform!.sx, 6);
      // …and BOTH are correctly framed; it reframes, it does not drift.
      expect(checkTier2Framing(partial.metrics!).pass).toBe(true);
      expect(checkTier2Framing(full.metrics!).pass).toBe(true);
    });
  }

  it('states no frame for a chart that has measured nothing', () => {
    const framed = frameSeries({ x: null, y: null }, { width: 1280, height: 800 });
    expect(framed.transform).toBeNull();
    expect(framed.metrics).toBeNull();
    // …but the box and the pane region still exist: the gutters are a layout
    // decision, not a data one, so the axis can be drawn before a record lands.
    expect(framed.paneRegion.clipXMin).toBeGreaterThan(-1);
  });
});
