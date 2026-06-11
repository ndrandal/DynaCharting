/* apps/showcase/src/chrome/ChromeOverlay.tsx
 *
 * Composites a view's 'logical chart' chrome — axes (gridlines + tick labels),
 * legend, colorbar — plus the FPS HUD, as an absolutely-positioned layer that
 * exactly overlaps the engine canvas box. It measures its own box (which is
 * sized to fill the canvas region) with a ResizeObserver so ticks/gridlines
 * re-flow on resize. It is pointer-events:none so it never intercepts canvas
 * interaction. Driven entirely by the active view's `chrome` metadata + baked
 * transform — adding chrome to a view is data-only (no edit here).
 */

import { useEffect, useMemo, useRef, useState } from 'react';
import type { ShowcaseView } from '../views/registry';
import { AxisOverlay } from './AxisOverlay';
import { Legend } from './Legend';
import { Colorbar } from './Colorbar';
import { FpsHud } from './FpsHud';
import { ColorbarAxisLabels } from './ColorbarAxisLabels';
import { effectiveTransform } from './mapping';
import { firstRecordX } from './firstRecordX';
import type { FrameStatsHub } from './frameStats';

interface ChromeOverlayProps {
  view: ShowcaseView;
  /** Frame-stats hub for the FPS HUD (null when no engine). */
  statsHub: FrameStatsHub | null;
  /** Whether the FPS HUD is shown (toggled by 'F'). */
  fpsVisible: boolean;
}

export function ChromeOverlay({ view, statsHub, fpsVisible }: ChromeOverlayProps) {
  const boxRef = useRef<HTMLDivElement | null>(null);
  const [size, setSize] = useState({ w: 0, h: 0 });

  // Track the overlay box size (== canvas CSS box) for the data→pixel mapping.
  useEffect(() => {
    const el = boxRef.current;
    if (!el) return;
    const measure = () => setSize({ w: el.clientWidth, h: el.clientHeight });
    measure();
    const obs = new ResizeObserver(measure);
    obs.observe(el);
    return () => obs.disconnect();
  }, []);

  const chrome = view.chrome;

  // The effective transform reproduces the engine's runtime framing (including
  // the xAnchor X re-derivation from the first replayed record).
  const transform = useMemo(() => {
    const fx = view.xAnchor ? firstRecordX(view.records, view.growth) : null;
    return effectiveTransform(view.meta.transform, view.xAnchor, fx);
  }, [view]);

  const hasAxes = !!chrome?.axes && (!!chrome.axes.x || !!chrome.axes.y);
  const hasLegend = !!chrome?.legend?.length;
  const hasColorbar = !!chrome?.colorbar;
  const catX = chrome?.colorbar?.categories?.x;
  const catY = chrome?.colorbar?.categories?.y;

  return (
    <div className="chrome-overlay" ref={boxRef} aria-hidden={false}>
      {hasAxes && chrome?.axes && (
        <AxisOverlay axes={chrome.axes} transform={transform} width={size.w} height={size.h} />
      )}
      {/* Categorical symbol labels for heatmaps (correlation matrix etc.). */}
      {hasColorbar && (catX || catY) && (
        <ColorbarAxisLabels x={catX} y={catY} width={size.w} height={size.h} />
      )}
      {hasLegend && <Legend items={chrome!.legend!} />}
      {hasColorbar && <Colorbar spec={chrome!.colorbar!} />}
      <FpsHud hub={statsHub} visible={fpsVisible} />
    </div>
  );
}
