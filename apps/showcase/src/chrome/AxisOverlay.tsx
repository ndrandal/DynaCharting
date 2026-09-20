/* apps/showcase/src/chrome/AxisOverlay.tsx
 *
 * Renders gridlines + tick marks + tick labels for the x and y axes of a
 * cartesian view, as an SVG layer sized to the canvas box. Tick positions come
 * from the data→pixel mapping (mapping.ts) so they align with the rendered
 * geometry. Labels are formatted per AxisSpec.format. Chrome recedes (dim
 * hairlines, mono tabular numerics) so the data stays the brightest thing.
 *
 * It takes RESOLVED axes (ENC-1252): the domain has already been settled by
 * deriveAxes.ts — measured from the streamed records where the view declares an
 * `axisDomain`, falling back to a legacy literal otherwise — so nothing here
 * ever has to decide what an axis's bounds are.
 *
 * And since ENC-1254 it does not decide what an axis SAYS either: `axisTicks`
 * owns the step and the label (a time ladder for a `timestamp` axis, round marks
 * + step-derived precision for everything else), in data space and DOM-free.
 * This component is now only the projection and the SVG — which is the shape
 * ENC-1253 needs, because it replaces exactly this file and nothing above it.
 */

import { useMemo } from 'react';
import type { ResolvedAxes, ResolvedAxisSpec } from './deriveAxes';
import type { EffectiveTransform } from './mapping';
import { dataXToPx, dataYToPx } from './mapping';
import { axisTicks } from './axisTicks';

interface AxisOverlayProps {
  axes: ResolvedAxes;
  transform: EffectiveTransform;
  /** Overlay box size in CSS px (== the engine canvas CSS box). */
  width: number;
  height: number;
}

interface Tick {
  px: number;
  label: string;
}

/**
 * Project one axis's ticks into the plot box.
 *
 * The filter drops ticks the frame does not contain, which is why a view can
 * request 6 and show 4: the stated domain is wider than the x-anchored window
 * (LIMITATIONS.md **DC-L13** §2, and fitting the two together is ENC-1256).
 * Every label shown is true; there are simply fewer of them. Do NOT "fix" that
 * here by widening the window or synthesising ticks — that would put a mark
 * where the engine draws nothing, which is the tier-0 defect one level up.
 */
function buildTicks(
  spec: ResolvedAxisSpec,
  toPx: (v: number) => number,
  extent: number,
): Tick[] {
  return axisTicks(spec)
    .map((t) => ({ px: toPx(t.value), label: t.label }))
    // Keep ticks that fall within (a hair beyond) the plot box.
    .filter((t) => Number.isFinite(t.px) && t.px >= -1 && t.px <= extent + 1);
}

export function AxisOverlay({ axes, transform, width, height }: AxisOverlayProps) {
  const xTicks = useMemo(
    () => (axes.x ? buildTicks(axes.x, (v) => dataXToPx(v, transform, width), width) : []),
    [axes.x, transform, width],
  );
  const yTicks = useMemo(
    () => (axes.y ? buildTicks(axes.y, (v) => dataYToPx(v, transform, height), height) : []),
    [axes.y, transform, height],
  );

  if (width <= 0 || height <= 0) return null;
  const xGrid = axes.x?.grid ?? true;
  const yGrid = axes.y?.grid ?? true;

  return (
    <svg className="chrome-axis-svg" width={width} height={height} aria-hidden focusable="false">
      {/* Y gridlines (horizontal) + price/value labels on the left. */}
      {axes.y &&
        yTicks.map((t, i) => (
          <g key={`y${i}`}>
            {yGrid && (
              <line className="chrome-grid" x1={0} y1={t.px} x2={width} y2={t.px} />
            )}
            <line className="chrome-tick" x1={0} y1={t.px} x2={6} y2={t.px} />
            <text className="chrome-label chrome-label-y" x={9} y={t.px} dy="0.32em">
              {t.label}
            </text>
          </g>
        ))}

      {/* X gridlines (vertical) + index/time labels along the bottom. */}
      {axes.x &&
        xTicks.map((t, i) => (
          <g key={`x${i}`}>
            {xGrid && (
              <line className="chrome-grid" x1={t.px} y1={0} x2={t.px} y2={height} />
            )}
            <line className="chrome-tick" x1={t.px} y1={height} x2={t.px} y2={height - 6} />
            <text
              className="chrome-label chrome-label-x"
              x={t.px}
              y={height - 9}
              textAnchor="middle"
            >
              {t.label}
            </text>
          </g>
        ))}

      {/* Axis titles. */}
      {axes.y?.label && (
        <text className="chrome-axis-title" x={6} y={14}>
          {axes.y.label}
        </text>
      )}
      {axes.x?.label && (
        <text className="chrome-axis-title" x={width - 6} y={height - 24} textAnchor="end">
          {axes.x.label}
        </text>
      )}
    </svg>
  );
}
