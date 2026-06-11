/* apps/showcase/src/components/ExplainerPanel.tsx
 *
 * The explainer panel (DESIGN §3.4 / T5.3) — the "self-explanatory" workhorse.
 * A tightening funnel: title + tier badge + reference-tool chip → the
 * one-sentence DATA+TECHNIQUE "WHAT'S GOING ON" → the mono, tabular fact block
 * (buffer/pipeline receipts). Entirely data-driven from the view's
 * explainer.md (parsed front-matter + body + fact table) and view.json — no
 * copy lives in this component (§6 "data-driven, no hardcoding").
 */

import { Fragment, useMemo } from 'react';
import type { ShowcaseView } from '../views/registry';
import { parseExplainer } from '../views/explainer';
import { TierBadge, RefChip } from './TierBadge';

export function ExplainerPanel({ view }: { view: ShowcaseView }) {
  const parsed = useMemo(() => parseExplainer(view.explainer), [view.explainer]);
  const title = parsed.frontMatter.title || view.meta.title;
  const referenceTool = parsed.frontMatter.referenceTool || view.meta.referenceTool;
  const body = parsed.body || view.meta.blurb;

  return (
    <div className="explainer">
      <div className="ex-head">
        <h2>{title}</h2>
        <div className="ex-tags">
          <TierBadge tier={view.meta.tier} />
          <RefChip tool={referenceTool} />
        </div>
      </div>

      <section className="ex-section">
        <p className="micro-label">What's going on</p>
        <p className="ex-body">{body}</p>
      </section>

      {parsed.facts.length > 0 && (
        <section className="ex-section">
          <dl className="fact-block">
            {parsed.facts.map((f, i) => {
              const cls = f.emphasis === 'wall' ? 'fact-wall' : f.emphasis === 'composed' ? 'fact-composed' : undefined;
              return (
                <Fragment key={i}>
                  <dt className={cls}>{f.label}</dt>
                  <dd className={cls}>{f.value}</dd>
                </Fragment>
              );
            })}
          </dl>
        </section>
      )}
    </div>
  );
}
