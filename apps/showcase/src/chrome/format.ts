/* apps/showcase/src/chrome/format.ts
 *
 * Tick-value → label formatters (per AxisSpec.format) and color coercion.
 */

import type { AxisFormat, ColorInput } from './types';

/** Format a tick value per the axis's declared format. */
export function formatTick(value: number, format: AxisFormat): string {
  switch (format) {
    case 'price':
      return '$' + value.toFixed(2);
    case 'time': {
      // value is seconds → m:ss
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
    case 'number':
    default: {
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
