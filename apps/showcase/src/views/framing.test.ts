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
import { defineView } from './registry';
import { framingFor, paneIds, paneOfLayer, resolveFraming } from './framing';
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

describe('resolveFraming — which views the plot box can frame, and why', () => {
  it('frames the single-pane views, naming the pane out of their own manifest', () => {
    const c = resolveFraming(candles);
    expect(c).toEqual({ framed: true, framing: { paneId: 10000, transformId: 10050 } });
    // …and the pane id was DERIVED, not typed here: the growth layer's pane.
    expect(paneOfLayer(candles.manifest, candles.growth.layerId)).toBe(10000);
    expect(paneIds(candles.manifest)).toEqual([10000]);

    const o = resolveFraming(ohlc as unknown as ManifestModule);
    expect(o.framed).toBe(true);
  });

  it('REFUSES the stacked two-pane view, and says which panes made it stacked', () => {
    const r = resolveFraming(overlays as unknown as ManifestModule);
    expect(r.framed).toBe(false);
    if (r.framed) throw new Error('unreachable');
    expect(r.reason).toBe('multi-pane');
    expect(r.detail).toContain('10000');
    expect(r.detail).toContain('10002');
    expect(r.detail).toContain('DC-L-1273');
    // The refusal is a fact about the manifest, not a hardcoded exception list.
    expect(paneIds(overlays.manifest)).toEqual([10000, 10002]);
  });

  it('refuses a view that measures no domain — there is nothing to fit', () => {
    const r = resolveFraming({ manifest: candles.manifest, growth: candles.growth });
    expect(r.framed).toBe(false);
    if (r.framed) throw new Error('unreachable');
    expect(r.reason).toBe('no-axis-domain');
  });

  it('refuses a view with no primary growth series', () => {
    const r = resolveFraming({ manifest: candles.manifest, axisDomain: candles.axisDomain });
    expect(r.framed).toBe(false);
    if (r.framed) throw new Error('unreachable');
    expect(r.reason).toBe('no-growth-transform');
  });

  it('answers for a whole catalog view too (the shape the app passes)', () => {
    expect(framingFor(null)).toBeNull();
    expect(framingFor(buildView('candles-aapl', candles))?.framed).toBe(true);
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
