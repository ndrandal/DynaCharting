/* apps/showcase/src/useWebGPU.ts
 *
 * Detect WebGPU support (T5.7). The showcase renders live via WebGPU; when the
 * browser lacks `navigator.gpu` we show a designed fallback (the contact sheet
 * of stills) instead of a broken canvas, so the case study survives being
 * opened anywhere.
 */

export function isWebGPUSupported(): boolean {
  return typeof navigator !== 'undefined' && 'gpu' in navigator && navigator.gpu != null;
}
