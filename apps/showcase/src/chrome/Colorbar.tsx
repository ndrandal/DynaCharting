/* apps/showcase/src/chrome/Colorbar.tsx
 *
 * Colorbar for heatmap / textured-quad views: a vertical gradient strip with a
 * max label at top, min at bottom, and a midpoint, plus an optional title. The
 * gradient is built from the view's declared `stops` (matching the colormap
 * baked into the texture). Pinned top-right over the canvas.
 */

import type { ColorbarSpec } from './types';
import { toCssColor } from './format';
import { formatTick } from './format';

export function Colorbar({ spec }: { spec: ColorbarSpec }) {
  // CSS gradient bottom→top: stop.at=0 is the min (bottom), at=1 the max (top).
  const stops = [...spec.stops].sort((a, b) => a.at - b.at);
  const gradient = `linear-gradient(to top, ${stops
    .map((s) => `${toCssColor(s.color)} ${(s.at * 100).toFixed(1)}%`)
    .join(', ')})`;

  const mid = (spec.min + spec.max) / 2;
  const fmt = (v: number) => formatTick(v, 'number');

  return (
    <div className="chrome-colorbar" aria-label={`Colorbar ${spec.label ?? ''}`}>
      {spec.label && <div className="chrome-colorbar-title">{spec.label}</div>}
      <div className="chrome-colorbar-body">
        <div className="chrome-colorbar-strip" style={{ background: gradient }} aria-hidden />
        <div className="chrome-colorbar-scale">
          <span className="chrome-colorbar-tick">{fmt(spec.max)}</span>
          <span className="chrome-colorbar-tick">{fmt(mid)}</span>
          <span className="chrome-colorbar-tick">{fmt(spec.min)}</span>
        </div>
      </div>
    </div>
  );
}
