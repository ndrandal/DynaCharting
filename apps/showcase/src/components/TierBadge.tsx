/* apps/showcase/src/components/TierBadge.tsx
 *
 * The tier badge (DESIGN §3.3): a small uppercase mono label with a 2px left
 * color bar + dot, one component used everywhere (card, explainer, frontier
 * map) so the color→meaning mapping is learned once. Color is NEVER the only
 * signal — the text label always rides along (T5.7 color-blind safety). The
 * `title` tooltip carries the one-line meaning.
 */

import type { ViewTier } from '../views/registry';
import { TIER_INFO } from '../views/registry';

export function TierBadge({ tier }: { tier: ViewTier }) {
  const info = TIER_INFO[tier];
  return (
    <span className="tier-badge" data-tier={tier} title={`${info.label} — ${info.meaning}`}>
      <span className="tier-dot" aria-hidden />
      {info.label}
    </span>
  );
}

/** Reference-tool chip: `≈ <tool>` — "in the spirit of", never competition (§3.2). */
export function RefChip({ tool }: { tool: string }) {
  return (
    <span className="ref-chip mono" title={`In the spirit of ${tool}`}>
      ≈ {tool}
    </span>
  );
}
