/* apps/showcase/src/components/SingleView.tsx
 *
 * Single-view mode (DESIGN §4.2, `/view/:id`): the live canvas dominates
 * (≥65% width), the explainer panel docked right, an inline replay scrubber
 * under the canvas, prev/next sibling nav, and a filmstrip of sibling views
 * along the bottom for lateral nav (keeps "breadth" present while deep). The
 * canvas is the shared engine portaled into `slotRef` by App.
 */

import { useEffect, useRef, type ReactNode } from 'react';
import { VIEWS, type ShowcaseView } from '../views/registry';
import { ExplainerPanel } from './ExplainerPanel';
import { Transport, type TransportProps } from './Transport';

interface SingleViewProps {
  view: ShowcaseView;
  onSelect: (id: string) => void;
  onSlot: (el: HTMLElement | null) => void;
  transport: TransportProps;
  /** Logical-chart chrome overlay, composited over the canvas region. */
  chrome?: ReactNode;
}

export function SingleView({ view, onSelect, onSlot, transport, chrome }: SingleViewProps) {
  const slotRef = useRef<HTMLDivElement | null>(null);
  useEffect(() => {
    onSlot(slotRef.current);
    return () => onSlot(null);
  }, [onSlot]);

  const idx = VIEWS.findIndex((v) => v.id === view.id);
  const prev = idx > 0 ? VIEWS[idx - 1] : null;
  const next = idx >= 0 && idx < VIEWS.length - 1 ? VIEWS[idx + 1] : null;

  return (
    <div className="single">
      <div className="single-canvas-region" ref={slotRef} aria-label={`${view.meta.title} live view`}>
        {chrome}
      </div>

      <aside className="single-explainer" aria-label="Explainer">
        <ExplainerPanel view={view} />
      </aside>

      <div className="single-transport">
        <Transport {...transport} variant="full" />
        <div className="nav-siblings">
          <button onClick={() => prev && onSelect(prev.id)} disabled={!prev} aria-label="Previous view">
            ← Prev
          </button>
          <button onClick={() => next && onSelect(next.id)} disabled={!next} aria-label="Next view">
            Next →
          </button>
        </div>
      </div>

      <nav className="filmstrip" aria-label="All views">
        {VIEWS.map((v) => (
          <button
            key={v.id}
            className={v.id === view.id ? 'current' : ''}
            aria-current={v.id === view.id ? 'true' : undefined}
            onClick={() => onSelect(v.id)}
            title={`${v.meta.title} — ${v.meta.tier}`}
          >
            <span className="strip-dot" style={{ background: `var(--tier-${v.meta.tier})` }} aria-hidden />
            {v.meta.title}
          </button>
        ))}
      </nav>
    </div>
  );
}
