/* apps/showcase/src/chrome/ColorbarAxisLabels.tsx
 *
 * Categorical axis labels for a heatmap / matrix view (e.g. the symbol names of
 * a correlation matrix). The matrix quad covers clip [-Q, Q] in both axes; the
 * N categories are the cell CENTERS evenly spaced across that span. X labels run
 * along the bottom, Y labels down the left, mapped clip→pixel the engine's way.
 * Row 0 (image top) is the first Y category — matching the texture's row order.
 *
 * Q is the matrix half-extent in clip space (the correlation-heatmap quad uses
 * 0.85; kept as the default so per-view agents only supply category names).
 */

import { clipXToPx, clipYToPx } from './mapping';

interface ColorbarAxisLabelsProps {
  x?: string[];
  y?: string[];
  width: number;
  height: number;
  /** Matrix half-extent in clip space (the quad spans [-q, q]). */
  q?: number;
}

/** Cell-center clip coordinate for category `i` of `n`, across [-q, q]. */
function cellCenterClip(i: number, n: number, q: number): number {
  // span [-q, q]; cell width = 2q/n; center of cell i = -q + (i+0.5)*cellW.
  return -q + ((i + 0.5) * 2 * q) / n;
}

export function ColorbarAxisLabels({ x, y, width, height, q = 0.85 }: ColorbarAxisLabelsProps) {
  if (width <= 0 || height <= 0) return null;
  return (
    <svg className="chrome-axis-svg" width={width} height={height} aria-hidden focusable="false">
      {y &&
        y.map((label, i) => {
          // Image row 0 is at the TOP (clip +q); descending categories go down.
          const clipY = q - ((i + 0.5) * 2 * q) / y.length;
          const py = clipYToPx(clipY, height);
          return (
            <text key={`cy${i}`} className="chrome-label chrome-cat-y" x={9} y={py} dy="0.32em">
              {label}
            </text>
          );
        })}
      {x &&
        x.map((label, i) => {
          const px = clipXToPx(cellCenterClip(i, x.length, q), width);
          return (
            <text
              key={`cx${i}`}
              className="chrome-label chrome-cat-x"
              x={px}
              y={height - 9}
              textAnchor="middle"
            >
              {label}
            </text>
          );
        })}
    </svg>
  );
}
