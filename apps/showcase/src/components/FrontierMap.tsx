/* apps/showcase/src/components/FrontierMap.tsx
 *
 * Act III — The frontier map (DESIGN §3.5 / §4.3 / T5.5), THE PAYOFF. A map,
 * not a table: three horizontal bands (native → composed → walled), left→right
 * = increasing distance from "native". The composed band is wide and full; the
 * walled band narrow and singular — the SHAPE carries the argument. Each node
 * shows the *reason* it lands in its band (the COMPOSED VIA / THE WALL fact,
 * derived from the view's explainer), so placement comes with justification.
 * The walled band gets the pinned amber wall-callout — the visual terminus.
 *
 * Data-driven from VIEWS — adding a view's dir places a new node automatically.
 */

import { useMemo } from 'react';
import { viewsByTier, TIER_INFO, type ShowcaseView, type ViewTier } from '../views/registry';
import { parseExplainer } from '../views/explainer';
import { TierBadge } from './TierBadge';
import type { Route } from '../router';

const BAND_PHRASE: Record<ViewTier, string> = {
  native: '"directly"',
  composed: '"JSON manifest + upstream precompute"',
  walled: '"shown precomputed"',
};

/** Derive the one-line "why it lands here" reason for a view's node. */
function reasonFor(view: ShowcaseView): string {
  const { facts } = parseExplainer(view.explainer);
  const wall = facts.find((f) => f.emphasis === 'wall');
  if (wall) return wall.value;
  const composed = facts.find((f) => f.emphasis === 'composed');
  if (composed) return composed.value;
  return view.meta.blurb;
}

interface FrontierMapProps {
  onNavigate: (r: Route) => void;
}

export function FrontierMap({ onNavigate }: FrontierMapProps) {
  const groups = useMemo(() => viewsByTier(), []);
  const total = groups.reduce((n, g) => n + g.views.length, 0);

  return (
    <div className="page">
      <div className="frontier">
        <div className="frontier-head">
          <h1>The Frontier</h1>
          <p className="lead">
            How far a JSON manifest reaches — and the one wall left. Everything to the left is a manifest over a pure
            data path; the wall on the right is the same shape every time.
          </p>
        </div>

        <div className="frontier-bands">
          {groups.map(({ tier, views }) => (
            <section className="frontier-band" data-tier={tier} key={tier} aria-label={`${TIER_INFO[tier].label} band`}>
              <div className="band-head">
                <TierBadge tier={tier} />
                <span className="count mono" style={{ color: 'var(--text-faint)' }}>
                  ({views.length})
                </span>
              </div>
              <p className="band-phrase">{BAND_PHRASE[tier]}</p>
              <div className="frontier-nodes">
                {views.map((v) => (
                  <button
                    key={v.id}
                    className="frontier-node"
                    data-tier={tier}
                    onClick={() => onNavigate({ name: 'view', id: v.id })}
                    aria-label={`${v.meta.title} — ${TIER_INFO[tier].label}. ${reasonFor(v)}`}
                  >
                    <span className={`node-dot${tier === 'walled' ? ' precomputed' : ''}`} aria-hidden />
                    <span className="node-text">
                      <span className="node-title">{v.meta.title}</span>
                      <span className="node-reason">{reasonFor(v)}</span>
                    </span>
                  </button>
                ))}
                {views.length === 0 && (
                  <span className="node-reason" style={{ padding: 'var(--s-2)' }}>
                    No views in this band yet.
                  </span>
                )}
              </div>
            </section>
          ))}
        </div>

        <div className="wall-callout" role="note">
          <p className="micro-label">◀ The wall</p>
          <p>
            Everything to the left is a JSON manifest over a pure data path. The wall on the right is the same shape
            every time: <strong>computation that must happen on the GPU, per-pixel, in real time</strong> — live FFT,
            live KDE density, live marching-squares, glow. We render a <em>precomputed-but-animated</em> version
            (texture-fed, the field swapped frame-by-frame over the loop, purity preserved) and mark the{' '}
            <em>live-GPU</em> gap — every view here animates, so the wall isn't "static vs. live", it's the field being
            <em> computed</em> live on the GPU vs. replayed. <strong>That gap is the custom-WGSL-pipeline-from-JSON
            decision</strong> — the one thing the manifest model can't currently buy.
          </p>
        </div>

        <div className="frontier-legend">
          <span className="legend-arrow">◀── what the manifest expresses ──▶</span>
          <span>·</span>
          <span className="legend-arrow">the gap ──▶</span>
          <span>·</span>
          <span>
            <span className="node-dot" style={{ display: 'inline-block', background: 'var(--text)', width: 9, height: 9, borderRadius: '50%', verticalAlign: 'middle' }} /> live
            {'   '}
            <span className="node-dot precomputed" style={{ display: 'inline-block', borderColor: 'var(--text-dim)', width: 9, height: 9, borderRadius: '50%', borderWidth: 2, borderStyle: 'solid', verticalAlign: 'middle' }} /> precomputed
          </span>
        </div>

        <div className="frontier-close">
          <p>
            <span className="mono">{total}</span> live {total === 1 ? 'view' : 'views'} prove the breadth; one named
            wall marks the edge. That's the case: manifest-driven reach, with a documented frontier.
          </p>
          <div className="links">
            <button className="btn" onClick={() => onNavigate({ name: 'gallery' })}>
              Back to the gallery
            </button>
            <button className="btn" onClick={() => onNavigate({ name: 'report' })}>
              Read the evidence →
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}
