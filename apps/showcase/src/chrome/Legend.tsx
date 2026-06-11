/* apps/showcase/src/chrome/Legend.tsx
 *
 * Series legend: a stack of colored swatches + labels, pinned top-right over the
 * canvas. Swatch glyph varies by `kind` (line/area/candle/bar/point/swatch).
 * Styled per the design tokens (bg-2 panel, hairline border, mono micro labels).
 */

import type { LegendItem } from './types';
import { toCssColor } from './format';

function Swatch({ item }: { item: LegendItem }) {
  const color = toCssColor(item.color);
  const kind = item.kind ?? 'swatch';
  switch (kind) {
    case 'line':
      return <span className="chrome-swatch chrome-swatch-line" style={{ background: color }} />;
    case 'point':
      return <span className="chrome-swatch chrome-swatch-point" style={{ background: color }} />;
    case 'candle':
      return <span className="chrome-swatch chrome-swatch-candle" style={{ background: color }} />;
    case 'bar':
    case 'area':
    case 'swatch':
    default:
      return <span className="chrome-swatch chrome-swatch-fill" style={{ background: color }} />;
  }
}

export function Legend({ items }: { items: LegendItem[] }) {
  if (!items.length) return null;
  return (
    <div className="chrome-legend" role="list" aria-label="Legend">
      {items.map((item, i) => (
        <div className="chrome-legend-row" role="listitem" key={`${item.label}-${i}`}>
          <Swatch item={item} />
          <span className="chrome-legend-label">{item.label}</span>
        </div>
      ))}
    </div>
  );
}
