/* apps/showcase/src/components/AppBar.tsx
 *
 * The persistent slim top bar (DESIGN §3.1): wordmark `DynaCharting · Frontier`,
 * route nav (Gallery / Frontier / Report), the global replay transport (shown
 * in single-view + frontier), and the connection/replay status pill. 48px,
 * --bg-2, bottom hairline, sticky. Tokens-first; no raw hex.
 */

import type { Route } from '../router';
import { Transport, type TransportProps } from './Transport';

export type StatusKind = 'connecting' | 'ok' | 'error' | 'settled';

export interface AppBarProps {
  route: Route;
  onNavigate: (route: Route) => void;
  status: StatusKind;
  statusLabel: string;
  /** When present, the global transport is shown (single-view + frontier). */
  transport?: TransportProps;
}

const NAV: { label: string; route: Route['name'] }[] = [
  { label: 'Gallery', route: 'gallery' },
  { label: 'Frontier', route: 'frontier' },
  { label: 'Report', route: 'report' },
];

export function AppBar({ route, onNavigate, status, statusLabel, transport }: AppBarProps) {
  const pillClass = status === 'error' ? 'error' : status === 'connecting' ? 'connecting' : 'ok';
  return (
    <header className="app-bar">
      <button className="wordmark" onClick={() => onNavigate({ name: 'home' })} aria-label="DynaCharting Frontier — home">
        <span className="diamond" aria-hidden>
          ◆
        </span>
        DynaCharting
        <span className="sep">·</span>
        <span className="frontier">Frontier</span>
      </button>

      <nav className="app-nav" aria-label="Primary">
        {NAV.map((n) => (
          <button
            key={n.route}
            className={route.name === n.route || (n.route === 'gallery' && route.name === 'view') ? 'active' : ''}
            aria-current={route.name === n.route ? 'page' : undefined}
            onClick={() => onNavigate({ name: n.route } as Route)}
          >
            {n.label}
          </button>
        ))}
      </nav>

      <span className="spacer" />

      {transport && <Transport {...transport} />}

      <span className={`status-pill ${pillClass}`} role="status" aria-live="polite">
        <span className="dot" aria-hidden />
        {statusLabel}
      </span>
    </header>
  );
}
