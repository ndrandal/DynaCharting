/* apps/showcase/src/chrome/mapping.ts
 *
 * The data→clip→pixel mapping the chrome overlay uses to place ticks/gridlines.
 * It mirrors EXACTLY how the engine projects geometry:
 *
 *   data → clip:   clipX = x·sx + tx        clipY = y·sy + ty     (the transform)
 *   clip → pixel:  px = (clipX + 1)·0.5·W   py = (1 − clipY)·0.5·H
 *
 * The clip→pixel half is the engine's own convention (core SvgExporter
 * clipToSvg{X,Y}): clip space [-1,1] fills the whole canvas; SVG/HTML Y is
 * top-down so clip +1 (top) maps to pixel 0. The overlay canvas is the SAME CSS
 * box as the engine <canvas>, so these pixels line up with the rendered frame.
 *
 * For xAnchor views the engine re-derives the transform's X (sx,tx) at replay
 * from the first record's index; we reproduce that derivation here so the X axis
 * tracks the live framing rather than the (possibly stale) baked X.
 */

import type { ViewTransform } from '../views/registry';
import type { XAnchorSpec } from '../engine/useReplay';

/** Effective data→clip transform, applying the xAnchor X re-derivation. */
export interface EffectiveTransform {
  sx: number;
  sy: number;
  tx: number;
  ty: number;
}

/**
 * Resolve the transform the overlay should map through. When `xAnchor` is set,
 * the X part is re-derived from `firstRecordX` the same way useReplay does
 * (sx = (clipMax−clipMin)/xWindow; tx = clipMin − sx·firstX), and Y is the baked
 * sy/ty. Otherwise the baked transform is used as-is. Falls back to identity.
 */
export function effectiveTransform(
  transform: ViewTransform | undefined,
  xAnchor: XAnchorSpec | undefined,
  firstRecordX: number | null,
): EffectiveTransform {
  const base: EffectiveTransform = transform
    ? { sx: transform.sx, sy: transform.sy, tx: transform.tx, ty: transform.ty }
    : { sx: 1, sy: 1, tx: 0, ty: 0 };
  if (xAnchor && firstRecordX !== null) {
    const sx = (xAnchor.clipMax - xAnchor.clipMin) / xAnchor.xWindow;
    const tx = xAnchor.clipMin - sx * firstRecordX;
    return { sx, tx, sy: xAnchor.sy, ty: xAnchor.ty };
  }
  return base;
}

/** Map a data-space X value to a pixel X within a W-wide overlay box. */
export function dataXToPx(x: number, t: EffectiveTransform, w: number): number {
  const clipX = x * t.sx + t.tx;
  return (clipX + 1) * 0.5 * w;
}

/** Map a data-space Y value to a pixel Y within an H-tall overlay box. */
export function dataYToPx(y: number, t: EffectiveTransform, h: number): number {
  const clipY = y * t.sy + t.ty;
  return (1 - clipY) * 0.5 * h;
}

/** Map a clip-space X (already in [-1,1]) to a pixel X. */
export function clipXToPx(clipX: number, w: number): number {
  return (clipX + 1) * 0.5 * w;
}

/** Map a clip-space Y (already in [-1,1]) to a pixel Y. */
export function clipYToPx(clipY: number, h: number): number {
  return (1 - clipY) * 0.5 * h;
}

/** Evenly-spaced tick VALUES across [min,max] (inclusive), `count` intervals. */
export function tickValues(min: number, max: number, count: number): number[] {
  const n = Math.max(1, Math.floor(count));
  const out: number[] = [];
  for (let i = 0; i <= n; i++) out.push(min + ((max - min) * i) / n);
  return out;
}
