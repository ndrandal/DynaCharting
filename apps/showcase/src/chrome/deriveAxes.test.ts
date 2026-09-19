/* apps/showcase/src/chrome/deriveAxes.test.ts — ENC-1252 (chart-quality-bar D7)
 *
 * THE EVIDENCE, not a restatement of the code.
 *
 * These tests fold the views' REAL committed `records.json` — captured embassy
 * dataplane frames, 13-byte header + payload, the same bytes `enqueueData`
 * hands the engine — through the same `DomainTracker` the running showcase
 * uses, driven by the same `axisDomain` the view's own manifest exports. No
 * fixture is written here; the data is whatever the capture contains.
 *
 * What that turns up is the case for D7 in numbers. candles-aapl's x axis said
 * `INDEX 4→160`; the records run to index 270. The axis was short by 110
 * indices — 41% of the tape — and had been for six months, because a literal
 * cannot be wrong in a way anything notices.
 */
import { describe, it, expect } from 'vitest';
import { readFileSync } from 'node:fs';
import { DomainTracker } from '@repo/dc-wasm/chart';

import { axisDomain as candlesDomain } from '../../views/candles-aapl/manifest';
import { axisDomain as ohlcDomain } from '../../views/ohlc-bars/manifest';
import { axisDomain as overlaysDomain } from '../../views/candle-overlays/manifest';
import { resolveAxes, axisDomainReportJson } from './deriveAxes';
import type { AxisDomainSpec } from '../views/registry';
import type { AxisSpec } from './types';

interface Capture {
  frames: { t: number; b64: string }[];
}

function loadFrames(viewId: string): ArrayBuffer[] {
  const path = new URL(`../../views/${viewId}/records.json`, import.meta.url);
  const capture = JSON.parse(readFileSync(path, 'utf8')) as Capture;
  return capture.frames.map((f) => {
    const bytes = Buffer.from(f.b64, 'base64');
    return bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength) as ArrayBuffer;
  });
}

/** Fold a view's capture through the tracker its own manifest declares. */
function measure(viewId: string, spec: AxisDomainSpec, upTo = Infinity) {
  const tracker = new DomainTracker(spec.sources);
  const frames = loadFrames(viewId);
  for (const f of frames.slice(0, upTo === Infinity ? frames.length : upTo)) tracker.observe(f);
  return { tracker, frameCount: frames.length };
}

describe('candles-aapl — the domain measured from the real capture', () => {
  it('states the domain the records actually contain', () => {
    const { tracker } = measure('candles-aapl', candlesDomain);
    const d = tracker.domain();
    expect(tracker.records).toBe(267);

    // y: the OHLC extremes of the streamed candles.
    expect(d.y!.min).toBeCloseTo(408.0561, 3);
    expect(d.y!.max).toBeCloseTo(418.0236, 3);
    // x: record index, widened to the bar edges by candle6's halfWidth.
    expect(d.x!.min).toBeCloseTo(3.6, 4);
    expect(d.x!.max).toBeCloseTo(270.4, 4);
  });

  it('proves the literal it replaced was WRONG on three of its four bounds', () => {
    const { tracker } = measure('candles-aapl', candlesDomain);
    const d = tracker.domain();
    // view.json used to say: y 408..418, x 4..160.
    expect(d.y!.max).toBeGreaterThan(418); // the top wick was drawn ABOVE the axis
    expect(d.y!.min).toBeGreaterThan(408); // and the floor sat below any data
    expect(d.x!.max).toBeGreaterThan(160); // the tape ran 110 indices past the axis
    expect(d.x!.max - 160).toBeGreaterThan(100);
  });

  it('CHANGES when the data changes — the property a literal cannot have', () => {
    const full = measure('candles-aapl', candlesDomain).tracker.domain();
    const third = measure('candles-aapl', candlesDomain, 89).tracker.domain();

    expect(third.x!.max).toBeLessThan(full.x!.max); // a shorter tape is a shorter x domain
    expect(third.x!.min).toBeCloseTo(full.x!.min, 6); // …anchored at the same first bar
    // The y extremes of a third of the tape are strictly inside the whole tape's.
    expect(third.y!.min).toBeGreaterThanOrEqual(full.y!.min);
    expect(third.y!.max).toBeLessThanOrEqual(full.y!.max);
    expect([third.y!.min, third.y!.max]).not.toEqual([full.y!.min, full.y!.max]);
  });

  it('grows monotonically as the capture streams (no rescan, O(Δ))', () => {
    const tracker = new DomainTracker(candlesDomain.sources);
    const frames = loadFrames('candles-aapl');
    let prev = { span: 0, records: 0 };
    for (const f of frames) {
      tracker.observe(f);
      const d = tracker.raw();
      if (!d.x) continue;
      const span = d.x.max - d.x.min;
      expect(span).toBeGreaterThanOrEqual(prev.span);
      expect(d.records).toBeGreaterThan(prev.records);
      prev = { span, records: d.records };
    }
    expect(prev.records).toBe(267);
  });
});

describe('ohlc-bars — the same measurement, a different mark', () => {
  it('shares the price domain and differs on x by the bar half-width', () => {
    const candles = measure('candles-aapl', candlesDomain).tracker.domain();
    const bars = measure('ohlc-bars', ohlcDomain).tracker.domain();
    expect(bars.y!.min).toBeCloseTo(candles.y!.min, 6); // same underlying AAPL tape
    expect(bars.y!.max).toBeCloseTo(candles.y!.max, 6);
    expect(bars.x!.min).toBeGreaterThan(candles.x!.min); // narrower bars, narrower x
    expect(bars.x!.max).toBeLessThan(candles.x!.max);
  });
});

describe('candle-overlays — why an axis group is declared, not inferred', () => {
  it('keeps the PRICE axis a price axis with a volume pane on the wire', () => {
    const { tracker } = measure('candle-overlays', overlaysDomain);
    const d = tracker.domain();
    // Candles + SMA feed y; the volume buffer (0..161456) is registered x-only.
    expect(d.y!.min).toBeCloseTo(408.0561, 3);
    expect(d.y!.max).toBeCloseTo(418.0236, 3);
    expect(d.y!.max).toBeLessThan(1000);
  });

  it('would have stated 0..161456 had the volume buffer fed y', () => {
    const wrong = new DomainTracker(
      overlaysDomain.sources.map((s) => ({ ...s, axes: 'xy' as const })),
    );
    for (const f of loadFrames('candle-overlays')) wrong.observe(f);
    expect(wrong.domain().y!.max).toBeGreaterThan(160000);
  });

  it('folds all three series and counts every record', () => {
    const { tracker } = measure('candle-overlays', overlaysDomain);
    expect(tracker.records).toBe(267 * 3);
  });
});

describe('resolveAxes', () => {
  const y: AxisSpec = { label: 'Price', format: 'price', ticks: 5, grid: true };
  const x: AxisSpec = { label: 'Index', format: 'index', ticks: 6, grid: true };

  it('derives from the measurement and says so', () => {
    const { axes, report } = resolveAxes(
      { x, y },
      { x: { min: 3.6, max: 270.4 }, y: { min: 408.05, max: 418.02 }, records: 267, observations: 1335 },
    );
    expect(axes.y).toMatchObject({ label: 'Price', min: 408.05, max: 418.02 });
    expect(report.y).toEqual({ min: 408.05, max: 418.02, source: 'derived' });
    expect(report.x!.source).toBe('derived');
    expect(report.records).toBe(267);
  });

  it('falls back to a legacy literal, and labels it a literal', () => {
    const { axes, report } = resolveAxes({ y: { ...y, min: 406, max: 418 } }, null);
    expect(axes.y).toMatchObject({ min: 406, max: 418 });
    expect(report.y).toEqual({ min: 406, max: 418, source: 'literal' });
    expect(report.x).toBeNull();
  });

  it('prefers a measurement over a stale literal', () => {
    const { report } = resolveAxes(
      { y: { ...y, min: 406, max: 418 } },
      { x: null, y: { min: 100, max: 200 }, records: 4, observations: 16 },
    );
    expect(report.y).toEqual({ min: 100, max: 200, source: 'derived' });
  });

  it('drops an axis with neither a measurement nor a literal — never invents one', () => {
    const { axes, report } = resolveAxes({ x, y }, { x: null, y: null, records: 0, observations: 0 });
    expect(axes.x).toBeUndefined();
    expect(axes.y).toBeUndefined();
    expect(report).toEqual({ x: null, y: null, records: 0 });
  });

  it('ignores a half-specified or non-finite literal', () => {
    expect(resolveAxes({ y: { ...y, min: 406 } }, null).axes.y).toBeUndefined();
    expect(resolveAxes({ y: { ...y, min: NaN, max: 418 } }, null).axes.y).toBeUndefined();
  });

  it('reports itself as compact JSON for the data attribute / harness read', () => {
    const { report } = resolveAxes(
      { x, y },
      { x: { min: 3.5999999940395355, max: 270.4 }, y: { min: 408.05612, max: 418.02359 }, records: 267, observations: 1 },
    );
    expect(JSON.parse(axisDomainReportJson(report))).toEqual({
      x: { min: 3.6, max: 270.4, source: 'derived' },
      y: { min: 408.05612, max: 418.02359, source: 'derived' },
      records: 267,
    });
  });
});
