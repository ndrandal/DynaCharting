/* apps/showcase/src/components/Hero.tsx
 *
 * Act I — Hero / frame (DESIGN §1.1, `/`). Three jobs in ~5s: assert the thesis
 * (H1), prove it's live (the flagship view renders in the canvas slot beside
 * the headline, already streaming the replay), show the spine (the data path
 * diagram). Two CTAs: Explore the gallery (primary) + See the frontier
 * (secondary — plant the climax early).
 *
 * The live canvas is the shared engine portaled into `slotRef` by App.
 */

import { useEffect, useRef, type ReactNode } from 'react';
import type { Route } from '../router';

interface HeroProps {
  onNavigate: (r: Route) => void;
  onSlot: (el: HTMLElement | null) => void;
  /** Logical-chart chrome overlay (FPS HUD + flagship chrome), over the canvas. */
  chrome?: ReactNode;
}

const SPINE = ['Mock GMA', 'embassy', 'dataplane WS', 'dc-wasm / WebGPU'];

export function Hero({ onNavigate, onSlot, chrome }: HeroProps) {
  const slotRef = useRef<HTMLDivElement | null>(null);
  useEffect(() => {
    onSlot(slotRef.current);
    return () => onSlot(null);
  }, [onSlot]);

  return (
    <div className="page">
      <div className="page-inner">
        <div className="hero">
          <div className="hero-copy">
            <h1>
              A data-aware <span className="accent">GPU charting engine</span>, driven entirely by JSON manifests —
              pushed until exactly one wall remains.
            </h1>
            <p className="sub">
              Every chart in this gallery was produced the same way: Claude wrote a <strong>JSON manifest</strong> — a
              declaration of panes, buffers, and render pipelines — paired it with a <strong>synthetic test data feed</strong>,
              and handed both to DynaCharting. There&rsquo;s no per-chart code; no two views share a line of rendering logic.
              The engine reads the manifest and renders the feed — that&rsquo;s the whole pipeline, streaming live in your
              browser via WebGPU.
            </p>

            <div className="spine" aria-label="Data path">
              {SPINE.map((n, i) => (
                <span key={n} style={{ display: 'inline-flex', alignItems: 'center', gap: 'var(--s-2)' }}>
                  <span className="node">{n}</span>
                  {i < SPINE.length - 1 && (
                    <span className="arrow" aria-hidden>
                      →
                    </span>
                  )}
                </span>
              ))}
            </div>

            <div className="cta-row">
              <button className="btn primary" onClick={() => onNavigate({ name: 'gallery' })}>
                Explore the gallery →
              </button>
              <button className="btn" onClick={() => onNavigate({ name: 'frontier' })}>
                See the frontier
              </button>
            </div>
          </div>

          <div className="hero-canvas" ref={slotRef} aria-label="Live engine view">
            {chrome}
          </div>
        </div>
      </div>
    </div>
  );
}
