/* apps/showcase/src/chrome/format.ts
 *
 * Tick-value → label formatters (per AxisSpec.format) and color coercion.
 *
 * PRECISION IS DERIVED, NOT CONSTANT (ENC-1254, SPEC §2 D12). `price` used to be
 * `'$' + value.toFixed(2)`. Two is the number of decimals a US equity happens to
 * want, and feed-simulator's universe is 30 fictional equity tickers and will
 * never get crypto pairs (D12 rules §5 Q5 out entirely) — so that constant was
 * never going to be contradicted by anything this gallery replays. Crypto is
 * what the platform sells, and a pair five orders of magnitude smaller needs 4-5
 * decimals; with no axis ground truth in the frame, under-specifying is silent
 * (§1.3's whole lesson). So the caller passes the decimals it DERIVED from the
 * observed tick step (`decimalsForStep`, @repo/dc-wasm), and nothing here knows
 * what a share price costs.
 *
 * The `timestamp` format is deliberately NOT handled here: an instant's label
 * depends on the tick STEP and on whether the domain crosses a day, neither of
 * which is knowable from one value. AxisOverlay formats those through
 * `timeTicks`, which returns each tick already labelled.
 */

import type { AxisFormat, ColorInput } from './types';

/**
 * Format a tick value per the axis's declared format.
 *
 * @param decimals the precision derived from the tick step by the caller. When
 *   omitted, `price` falls back to 2 and `number` to its own adaptive rule —
 *   kept only for call sites that have no step (there are none in the overlay).
 */
export function formatTick(value: number, format: AxisFormat, decimals?: number): string {
  switch (format) {
    case 'price':
      return '$' + value.toFixed(decimals ?? 2);
    case 'time': {
      // value is seconds → m:ss. This is an ELAPSED DURATION, not an instant —
      // `parsesAsTimestamp` rejects it on purpose. Views whose x really is a
      // clock use `format: 'timestamp'`.
      const total = Math.max(0, Math.round(value));
      const m = Math.floor(total / 60);
      const s = total % 60;
      return `${m}:${String(s).padStart(2, '0')}`;
    }
    case 'index':
      return String(Math.round(value));
    case 'percent': {
      const pct = value * 100;
      const sign = pct > 0 ? '+' : '';
      return `${sign}${pct.toFixed(0)}%`;
    }
    case 'timestamp':
      // Unreachable via the overlay (see the module header). If some other
      // caller asks, the honest answer is the raw value, not a fabricated clock.
      return String(value);
    case 'number':
    default: {
      // Uniform across the axis: a tick column with mixed precision ("1" above
      // "0.2") reads as a mistake even when every value is right.
      if (decimals !== undefined) return value.toFixed(decimals);
      const abs = Math.abs(value);
      // Adaptive precision: keep small values readable, drop noise on large ones.
      if (abs !== 0 && abs < 1) return value.toFixed(2);
      if (abs < 100) return value.toFixed(2).replace(/\.00$/, '');
      return Math.round(value).toString();
    }
  }
}

/** Coerce an RGBA-float tuple or hex string to a CSS color string. */
export function toCssColor(color: ColorInput): string {
  if (typeof color === 'string') return color;
  const [r, g, b, a = 1] = color;
  const c = (v: number) => Math.max(0, Math.min(255, Math.round(v * 255)));
  return `rgba(${c(r)}, ${c(g)}, ${c(b)}, ${a})`;
}
