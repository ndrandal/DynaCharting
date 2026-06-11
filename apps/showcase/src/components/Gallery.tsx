/* apps/showcase/src/components/Gallery.tsx
 *
 * The gallery (DESIGN §4.1 / T5.2): views grouped by tier (native → composed →
 * walled), each tier a section with its badge, one-line meaning, and count;
 * each view a card. The tier grouping is load-bearing narrative (§2). Clicking
 * a card enters single-view mode. Keyboard arrow-key navigation across the flat
 * card order + ARIA (`role=list`/`listitem`) satisfy T5.7. Fully data-driven
 * from VIEWS — adding a view's dir makes a card appear with zero edits here.
 */

import { useEffect, useRef } from 'react';
import { viewsByTier, TIER_INFO, type ShowcaseView } from '../views/registry';
import { TierBadge, RefChip } from './TierBadge';

interface GalleryProps {
  currentId?: string;
  onOpen: (id: string) => void;
}

export function Gallery({ currentId, onOpen }: GalleryProps) {
  const groups = viewsByTier();
  const flatIds = groups.flatMap((g) => g.views.map((v) => v.id));
  const gridRef = useRef<HTMLDivElement | null>(null);

  // Arrow-key navigation across the flat card order (T5.7).
  const onKeyNav = (e: React.KeyboardEvent) => {
    const cards = gridRef.current?.querySelectorAll<HTMLElement>('.view-card');
    if (!cards || cards.length === 0) return;
    const active = document.activeElement as HTMLElement | null;
    const idx = Array.from(cards).findIndex((c) => c === active);
    let next = -1;
    if (e.key === 'ArrowRight' || e.key === 'ArrowDown') next = idx < 0 ? 0 : Math.min(idx + 1, cards.length - 1);
    else if (e.key === 'ArrowLeft' || e.key === 'ArrowUp') next = idx < 0 ? 0 : Math.max(idx - 1, 0);
    else if (e.key === 'Home') next = 0;
    else if (e.key === 'End') next = cards.length - 1;
    if (next >= 0) {
      e.preventDefault();
      cards[next].focus();
    }
  };

  const totalViews = flatIds.length;

  return (
    <div className="page">
      <div className="page-inner">
        <div className="page-head">
          <h1>The Gallery</h1>
          <p className="lead">
            <span className="mono">{totalViews}</span> live {totalViews === 1 ? 'view' : 'views'} · JSON manifests
            over a faithful market-data path. Grouped by how far each rides on plain JSON — native, composed, then the
            honest walled edge.
          </p>
        </div>

        <div ref={gridRef} onKeyDown={onKeyNav}>
          {groups.map(({ tier, views }) =>
            views.length === 0 ? null : (
              <section className="tier-section" key={tier} aria-label={`${TIER_INFO[tier].label} views`}>
                <div className="tier-section-head">
                  <TierBadge tier={tier} />
                  <span className="meaning">{TIER_INFO[tier].meaning}</span>
                  <span className="count mono">({views.length})</span>
                </div>
                <div className="card-grid" role="list">
                  {views.map((v) => (
                    <ViewCard key={v.id} view={v} current={v.id === currentId} onOpen={onOpen} />
                  ))}
                </div>
              </section>
            ),
          )}

          {totalViews === 0 && (
            <div className="state-panel">
              <span className="state-title">No views yet</span>
              <span className="state-detail">
                Drop a <span className="mono">views/&lt;id&gt;/</span> directory to register one — the catalog
                auto-discovers it.
              </span>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}

function ViewCard({ view, current, onOpen }: { view: ShowcaseView; current: boolean; onOpen: (id: string) => void }) {
  const ref = useRef<HTMLButtonElement | null>(null);
  // Keep the current card scrolled into view when navigating from single mode.
  useEffect(() => {
    if (current) ref.current?.scrollIntoView({ block: 'nearest' });
  }, [current]);

  return (
    <div role="listitem">
      <button
        ref={ref}
        className={`view-card${current ? ' current' : ''}`}
        onClick={() => onOpen(view.id)}
        aria-label={`${view.meta.title} — ${view.meta.tier} tier, in the spirit of ${view.meta.referenceTool}`}
      >
        <div className="thumb">
          <div className="thumb-placeholder mono">live · {view.meta.datasetId}</div>
          <span className="view-affordance mono">View →</span>
        </div>
        <div className="card-body">
          <h3 className="card-title">{view.meta.title}</h3>
          <p className="card-blurb">{view.meta.blurb}</p>
          <div className="card-meta">
            <TierBadge tier={view.meta.tier} />
            <RefChip tool={view.meta.referenceTool} />
          </div>
        </div>
      </button>
    </div>
  );
}
