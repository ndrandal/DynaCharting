/* apps/showcase/src/components/EngineCanvas.tsx
 *
 * The single live engine canvas (DESIGN §6 "one engine instance"). It is
 * mounted ONCE in App and portaled into whichever route slot is active (hero
 * canvas, single-view canvas). Portaling moves the DOM node without unmounting
 * the React element, so the EngineHost lifecycle (create-on-mount in
 * ShowcaseEngine's callback ref) survives route changes — no engine churn.
 *
 * When no slot is active the canvas portals into a hidden keep-alive node so
 * the engine stays warm. Loading + error overlays render on top of whatever
 * slot the canvas currently fills.
 */

import { createPortal } from 'react-dom';
import type { EngineStatus } from '../engine/ShowcaseEngine';

interface EngineCanvasProps {
  canvasRef: (el: HTMLCanvasElement | null) => void;
  status: EngineStatus;
  error: string | null;
  ready: boolean;
  /** The DOM node to render the canvas into; null → hidden keep-alive host. */
  slot: HTMLElement | null;
  keepAlive: HTMLElement;
}

export function EngineCanvas({ canvasRef, status, error, ready, slot, keepAlive }: EngineCanvasProps) {
  const target = slot ?? keepAlive;
  return createPortal(
    <>
      <canvas ref={canvasRef} className="engine-canvas" />
      {slot && !ready && status !== 'error' && (
        <div className="canvas-overlay">
          <div className="skeleton-shimmer" aria-hidden />
          <span className="label">{status === 'init' ? 'Compiling manifest…' : 'Subscribing to data path…'}</span>
        </div>
      )}
      {slot && status === 'error' && (
        <div className="canvas-overlay">
          <span className="state-title">Couldn't start the renderer.</span>
          <span className="state-detail">{error ?? 'unknown error'}</span>
        </div>
      )}
    </>,
    target,
  );
}
