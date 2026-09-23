/* apps/showcase/src/chrome/axisTicks.test.ts — ENC-1254 (chart-quality-bar D1 tier 1)
 *
 * D1'S TIER-1 CHECK, on the real captures.
 *
 * SPEC D1 tier 1 ("Legible") states the falsifiable check as: *assert axis tick
 * text parses as a timestamp*. That is this file. It folds each market view's
 * committed `records.json` — captured embassy dataplane frames, the same bytes
 * `enqueueData` hands the engine — through the SAME `DomainTracker` and
 * `IndexTimeTracker` the running showcase builds, resolves the axes exactly as
 * `ChromeOverlay` does, and asserts every x tick label the chart will draw.
 *
 * It carries its own NEGATIVE CONTROL, because DC-L01's lesson is that a check
 * nobody has seen fail is not a check: the labels these very axes shipped for
 * six months (`4 … 160`, INDEX) are run through the same predicate and must
 * FAIL it. If someone reverts `format: 'timestamp'` back to `'index'`, this file
 * goes red — which is the only reason it exists.
 */
import { describe, it, expect } from 'vitest';
import { readFileSync } from 'node:fs';
import {
  DomainTracker,
  IndexTimeTracker,
  parsesAsTimestamp,
  type ObservedDomain,
  type TimeBasis,
} from '@repo/dc-wasm';

import { axisDomain as candlesDomain, growth as candlesGrowth } from '../../views/candles-aapl/manifest';
import { axisDomain as ohlcDomain, growth as ohlcGrowth } from '../../views/ohlc-bars/manifest';
import {
  axisDomain as overlaysDomain,
  growth as overlaysGrowth,
} from '../../views/candle-overlays/manifest';
import candlesView from '../../views/candles-aapl/view.json';
import ohlcView from '../../views/ohlc-bars/view.json';
import overlaysView from '../../views/candle-overlays/view.json';
import { resolveAxes } from './deriveAxes';
import { axisTicks, type AxisTick } from './axisTicks';
import type { AxisDomainSpec } from '../views/registry';
import type { ChromeSpec } from './types';
import type { GrowthSync } from '../engine/useReplay';

interface Capture {
  meta: { viewId: string; durationMs: number; frameCount: number; cadenceMs: number };
  frames: { t: number; b64: string }[];
}

function loadCapture(viewId: string): Capture {
  const path = new URL(`../../views/${viewId}/records.json`, import.meta.url);
  return JSON.parse(readFileSync(path, 'utf8')) as Capture;
}

function decode(b64: string): ArrayBuffer {
  // atob, not Buffer: the same decode the browser replay path uses.
  const bin = atob(b64);
  const bytes = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
  return bytes.buffer;
}

/**
 * Replay a view's capture the way `useViewSwitch` does: every frame's bytes into
 * both trackers, each stamped with that frame's own `t`. `upTo` truncates the
 * tape, which is how the "16 s of replay" cases below are taken.
 */
function replay(
  viewId: string,
  spec: AxisDomainSpec,
  growth: GrowthSync,
  upTo = Infinity,
): { domain: ObservedDomain; basis: TimeBasis | null; capture: Capture } {
  const capture = loadCapture(viewId);
  const tracker = new DomainTracker(spec.sources, spec.policy);
  const time = new IndexTimeTracker(
    [{ bufferId: growth.bufferId, stride: growth.stride, indexOffset: growth.xField }],
    false,
  );
  const frames = Number.isFinite(upTo) ? capture.frames.slice(0, upTo) : capture.frames;
  for (const f of frames) {
    const ab = decode(f.b64);
    tracker.observe(ab);
    time.observe(ab, f.t);
  }
  return { domain: tracker.domain(), basis: time.basis(), capture };
}

/** The x ticks the chart will draw, resolved exactly as ChromeOverlay does. */
function xTicks(chrome: ChromeSpec, domain: ObservedDomain, basis: TimeBasis | null): AxisTick[] {
  const { axes } = resolveAxes(chrome.axes, domain, basis);
  return axes.x ? axisTicks(axes.x) : [];
}

/** Just the label text, for the assertions that are about the strings. */
function xLabels(chrome: ChromeSpec, domain: ObservedDomain, basis: TimeBasis | null): string[] {
  return xTicks(chrome, domain, basis).map((t) => t.label);
}

/**
 * The tier-1 question, asked the way a caller with the tick in hand asks it:
 * the label AND the instant it was rendered from. Two of the seven label shapes
 * ("2026", "14:32") are equally a record index and an elapsed duration, so the
 * predicate takes the instant as evidence rather than guessing (ENC-1390) — and
 * the axis has it, because `axisTicks` now carries it.
 */
function tier1(tick: AxisTick): boolean {
  return parsesAsTimestamp(tick.label, { instantMs: tick.instantMs });
}

const MARKET_VIEWS = [
  { id: 'candles-aapl', spec: candlesDomain, growth: candlesGrowth, meta: candlesView },
  { id: 'ohlc-bars', spec: ohlcDomain, growth: ohlcGrowth, meta: ohlcView },
  { id: 'candle-overlays', spec: overlaysDomain, growth: overlaysGrowth, meta: overlaysView },
] as const;

describe('D1 tier 1 — every x tick label parses as a timestamp', () => {
  for (const v of MARKET_VIEWS) {
    it(`${v.id}: the whole tape`, () => {
      const { domain, basis } = replay(v.id, v.spec, v.growth as GrowthSync);
      expect(basis, 'no time basis was fitted from the capture').not.toBeNull();
      const ticks = xTicks(v.meta.chrome as ChromeSpec, domain, basis);
      expect(ticks.length, 'the axis stated no ticks at all').toBeGreaterThan(0);
      for (const tick of ticks) {
        expect(tick.instantMs, `${v.id}: ${JSON.stringify(tick.label)} carried no instant`).toBeTypeOf('number');
        expect(tier1(tick), `${v.id}: ${JSON.stringify(tick.label)}`).toBe(true);
      }
    });

    it(`${v.id}: and at every point along it, not just at the end`, () => {
      // A tier-1 pass that only holds once the tape has finished is not a pass:
      // the axis is on screen the whole time, and the domain (and therefore the
      // chosen step) grows under it.
      const total = loadCapture(v.id).frames.length;
      for (const upTo of [3, 10, 40, Math.floor(total / 2), total]) {
        const { domain, basis } = replay(v.id, v.spec, v.growth as GrowthSync, upTo);
        for (const tick of xTicks(v.meta.chrome as ChromeSpec, domain, basis)) {
          expect(tier1(tick), `${v.id} @${upTo} frames: ${tick.label}`).toBe(true);
        }
      }
    });
  }
});

describe('the negative control — the axis these views used to ship', () => {
  it('FAILS the same predicate, so the check above can go red', () => {
    // candles-aapl's literal x axis, verbatim: "min": 4, "max": 160, format
    // "index", 6 ticks. These are the labels that sat on a market time series
    // for six months.
    const legacy = axisTicks({ label: 'Index', format: 'index', ticks: 6, min: 4, max: 160 });
    expect(legacy.length).toBeGreaterThan(0);
    const passing = legacy.map((t) => t.label).filter((l) => parsesAsTimestamp(l));
    expect(passing, `INDEX labels ${JSON.stringify(legacy.map((t) => t.label))} must NOT parse`).toEqual([]);
  });

  it('and so does elapsed m:ss, which is a duration rather than an instant', () => {
    const elapsed = axisTicks({ label: 'Time', format: 'time', ticks: 4, min: 0, max: 4 });
    expect(elapsed.map((t) => t.label)).toEqual(['0:00', '0:01', '0:02', '0:03', '0:04']);
    expect(elapsed.map((t) => t.label).filter((l) => parsesAsTimestamp(l))).toEqual([]);
  });

  it('holds PAST 999 RECORDS, which is where the old control stopped (ENC-1390 H1)', () => {
    // `min: 4, max: 160` above is candles-aapl's literal axis, and it is three
    // digits wide — so it could not see that `^(\d{4})$` passed EVERY 4-digit
    // index as a year. `footprint` / `depth-ladder` / `volume-profile` already
    // ship 42720 / 10680 / 6408 records, so this is the axis those views get.
    const big = axisTicks({ label: 'Index', format: 'index', ticks: 6, min: 1000, max: 6408 });
    expect(big.length).toBeGreaterThan(0);
    expect(big.map((t) => t.label).some((l) => /^\d{4}$/.test(l)), JSON.stringify(big.map((t) => t.label))).toBe(true);
    // And they carry no instant, because an index axis has none to carry — so
    // there is nothing that could settle them either.
    expect(big.every((t) => t.instantMs === undefined)).toBe(true);
    const passing = big.filter(tier1).map((t) => t.label);
    expect(passing, `INDEX labels ${JSON.stringify(big.map((t) => t.label))} must NOT parse`).toEqual([]);
  });

  it('and elapsed m:ss holds AT AND ABOVE TEN MINUTES, where it gains a zero pad (ENC-1390 H2)', () => {
    // Under 600 s the duration formatter emits `0:12` — rejected for the missing
    // zero pad rather than for being a duration. At and above ten minutes it
    // emits `10:00`, `22:05`: well-formed clock times, and the control above
    // never reached them.
    const long = axisTicks({ label: 'Time', format: 'time', ticks: 4, min: 600, max: 1325 });
    expect(long.map((t) => t.label)).toEqual(['10:00', '13:20', '16:40', '20:00']);
    expect(long.filter(tier1).map((t) => t.label)).toEqual([]);
  });
});

describe('what the measurement actually says about candles-aapl', () => {
  it('fits the capture cadence from the tape, and states it as tape-relative', () => {
    const { basis, capture } = replay('candles-aapl', candlesDomain, candlesGrowth);
    expect(basis!.source).toBe('observed');
    // The capture's own meta says 267 frames over 19 950 ms. The fit is taken
    // from the frame timestamps alone and must land on the same cadence.
    expect(basis!.samples).toBe(267);
    expect(basis!.msPerIndex).toBeCloseTo(capture.meta.durationMs / (capture.meta.frameCount - 1), 1);
    expect(basis!.msPerIndex).toBeCloseTo(75, 0);
    // epochKnown is FALSE: the tape's zero is the start of the capture, not a
    // wall clock. This is the flag that stops a reader mistaking the labels for
    // the market time the bar closed at — which the stream does not carry.
    expect(basis!.epochKnown).toBe(false);
  });

  it('picks a 5-second step for a 20-second tape, and labels whole seconds', () => {
    const { domain, basis } = replay('candles-aapl', candlesDomain, candlesGrowth);
    const labels = xLabels(candlesView.chrome as ChromeSpec, domain, basis);
    // Four ticks, not five, and that is the axis being HONEST: the tape's last
    // frame is at t = 19 950 ms and the domain's right edge lands at ≈19 980 ms,
    // so there is no 00:00:20 on it. An evenly-divided axis would have printed a
    // sixth label at the frame edge regardless of whether any record was there —
    // which is the whole failure mode this ticket exists to remove.
    expect(labels).toEqual(['00:00:00', '00:00:05', '00:00:10', '00:00:15']);
    const last = domain.x!.max;
    expect(basis!.originMs + last * basis!.msPerIndex).toBeLessThan(20_000);
  });

  it('promotes the step as the tape grows — the axis is a function of the data', () => {
    // This is the property a hand-typed axis cannot have (SPEC §1.3). At 3
    // frames the visible span is a fraction of a second; at the end it is 20 s.
    const early = replay('candles-aapl', candlesDomain, candlesGrowth, 6);
    const late = replay('candles-aapl', candlesDomain, candlesGrowth);
    const earlyLabels = xLabels(candlesView.chrome as ChromeSpec, early.domain, early.basis);
    const lateLabels = xLabels(candlesView.chrome as ChromeSpec, late.domain, late.basis);
    expect(earlyLabels).not.toEqual(lateLabels);
    // Sub-second early: the step ladder went below a second rather than
    // collapsing every tick onto the same label.
    expect(new Set(earlyLabels).size).toBe(earlyLabels.length);
    expect(new Set(lateLabels).size).toBe(lateLabels.length);
    const earlyTicks = xTicks(candlesView.chrome as ChromeSpec, early.domain, early.basis);
    const lateTicks = xTicks(candlesView.chrome as ChromeSpec, late.domain, late.basis);
    for (const t of [...earlyTicks, ...lateTicks]) expect(tier1(t), t.label).toBe(true);
  });

  it('DROPS the x axis while no basis has been fitted — it never captions it', () => {
    const { domain } = replay('candles-aapl', candlesDomain, candlesGrowth, 1);
    const { axes, report } = resolveAxes(candlesView.chrome!.axes as never, domain, null);
    expect(axes.x).toBeUndefined();
    expect(report.x).toBeNull();
    // ...while the y axis, which needs no basis, is unaffected.
    expect(axes.y).toBeDefined();
  });

  it('publishes the basis alongside the domain, so provenance survives into the report', () => {
    const { domain, basis } = replay('candles-aapl', candlesDomain, candlesGrowth);
    const { report } = resolveAxes(candlesView.chrome!.axes as never, domain, basis);
    expect(report.x!.source).toBe('derived');
    expect(report.x!.time).toBeDefined();
    expect(report.x!.time!.epochKnown).toBe(false);
    expect(report.x!.time!.samples).toBe(267);
    expect(report.y!.time).toBeUndefined();
  });
});

describe('the y axis — precision derived from the step, not from an equity (D12)', () => {
  it('candles-aapl: round marks, and exactly the decimals its own step needs', () => {
    const { domain, basis } = replay('candles-aapl', candlesDomain, candlesGrowth);
    const { axes } = resolveAxes(candlesView.chrome!.axes as never, domain, basis);
    const ticks = axisTicks(axes.y!);
    // Measured domain 408.056…418.024 → a $2 step → 0 decimals, and every label
    // is EXACT for the mark it sits on (the old evenly-divided ticks put a
    // gridline at 410.049 and then printed "$410.05" for it).
    expect(ticks.map((t) => t.label)).toEqual(['$410', '$412', '$414', '$416', '$418']);
    for (const t of ticks) expect(Number(t.label.slice(1))).toBe(t.value);
  });

  it('the same code gives a sub-dollar pair five decimals, with no constant changed', () => {
    // D12's case, which feed-simulator will never produce: a crypto pair three
    // orders of magnitude below a share price. Nothing in the formatter knows.
    const ticks = axisTicks({ label: 'Price', format: 'price', ticks: 5, min: 0.00012, max: 0.00013 });
    expect(ticks.length).toBeGreaterThan(0);
    for (const t of ticks) {
      // 6 decimals, because the step is 2e-6 — not because anyone said "crypto".
      expect(t.label).toMatch(/^\$0\.\d{6}$/);
      expect(Number(t.label.slice(1))).toBeCloseTo(t.value, 9);
    }
  });

  it('...and a ~$185 fictional equity two, from the same rule', () => {
    // NEXO-shaped (SPEC §1.0's live reference symbol), a tight intraday band.
    const ticks = axisTicks({ label: 'Price', format: 'price', ticks: 5, min: 185.02, max: 185.31 });
    expect(ticks.map((t) => t.label)).toEqual([
      '$185.05',
      '$185.10',
      '$185.15',
      '$185.20',
      '$185.25',
      '$185.30',
    ]);
  });
});
