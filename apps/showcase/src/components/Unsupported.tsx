/* apps/showcase/src/components/Unsupported.tsx
 *
 * WebGPU-unsupported fallback (DESIGN §3.7 / T5.7): a designed panel, not a
 * broken canvas. The case study must survive being opened in any browser — so
 * we keep the explainers + frontier-map argument legible by listing every view
 * as a still-card (the T6.2 contact sheet stands in until stills are captured;
 * each card carries the title, tier, reference tool, and blurb). The explainers
 * and frontier map remain fully functional without a GPU.
 */

import { VIEWS } from '../views/registry';
import { TierBadge, RefChip } from './TierBadge';

export function Unsupported() {
  return (
    <div className="page">
      <div className="unsupported">
        <h1>This showcase renders live via WebGPU.</h1>
        <p>
          Your browser doesn't support it yet. The case study is still legible — below is every view with its data,
          technique, and tier placement. Open in a WebGPU-capable browser (recent Chrome / Edge) to see them render
          live.
        </p>

        <div className="contact-sheet">
          {VIEWS.map((v) => (
            <div className="still" key={v.id}>
              <div className="still-title">{v.meta.title}</div>
              <p style={{ fontSize: '0.78rem', color: 'var(--text-dim)', margin: '0 0 var(--s-3)', lineHeight: 1.45 }}>
                {v.meta.blurb}
              </p>
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', gap: 'var(--s-2)' }}>
                <TierBadge tier={v.meta.tier} />
                <RefChip tool={v.meta.referenceTool} />
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}
