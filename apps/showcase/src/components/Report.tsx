/* apps/showcase/src/components/Report.tsx
 *
 * Act IV — CTA / close (DESIGN §1.4). A short, plain close: what this proves
 * (manifest-driven breadth + a documented edge), a link to the frontier report
 * (the empirical artifact, T6.3), and a "how to add a view" pointer (T6.5). No
 * newsletter, no fake urgency — "read the evidence", not "sign up".
 */

import type { Route } from '../router';

export function Report({ onNavigate }: { onNavigate: (r: Route) => void }) {
  return (
    <div className="page">
      <div className="page-inner">
        <div className="page-head">
          <h1>What this proves</h1>
          <p className="lead">
            A data-aware GPU charting engine, driven entirely by JSON manifests, over a faithful market-data path —
            pushed until exactly one wall remains. The gallery shows the breadth; the frontier map shows the edge.
          </p>
        </div>

        <section className="tier-section">
          <div className="tier-section-head">
            <span className="micro-label" style={{ margin: 0 }}>
              The evidence
            </span>
          </div>
          <p style={{ color: 'var(--text)', lineHeight: 1.65, maxWidth: '68ch' }}>
            Every view here is a JSON manifest applied to one engine instance, fed by captured frames from the real
            faithful pipeline (Mock GMA → embassy → dataplane WS → dc-wasm). Native views render directly; composed
            views route upstream precompute (scalar-fan / texture-feed) into fixed buffers; walled views render the
            precomputed version and name the live-GPU gap. The one remaining wall is{' '}
            <strong style={{ color: 'var(--tier-walled)' }}>real-time GPU per-pixel compute / custom shaders</strong> —
            the custom-WGSL-pipeline-from-JSON decision.
          </p>
          <div className="links" style={{ display: 'flex', gap: 'var(--s-3)', marginTop: 'var(--s-6)' }}>
            <button className="btn primary" onClick={() => onNavigate({ name: 'frontier' })}>
              See the frontier map →
            </button>
            <button className="btn" onClick={() => onNavigate({ name: 'gallery' })}>
              Browse the gallery
            </button>
          </div>
        </section>

        <section className="tier-section">
          <div className="tier-section-head">
            <span className="micro-label" style={{ margin: 0 }}>
              Add a view
            </span>
          </div>
          <p style={{ color: 'var(--text-dim)', lineHeight: 1.65, maxWidth: '68ch' }}>
            Adding a view is dropping a directory — no component edits. Create{' '}
            <span className="mono">apps/showcase/views/&lt;id&gt;/</span> with{' '}
            <span className="mono">view.json</span>, <span className="mono">manifest.ts</span>,{' '}
            <span className="mono">records.json</span>, and <span className="mono">explainer.md</span>; the catalog
            auto-discovers it and it appears in the gallery, the explainer panel, and the frontier map automatically.
          </p>
        </section>
      </div>
    </div>
  );
}
