/* apps/showcase/src/App.tsx
 *
 * Minimal catalog integration (ENC-545 / T5.4): loads views from the registry
 * and renders the SELECTED view through the switching + replay path
 * (resetScene → applyManifest → bake transform → useReplay over the captured
 * records.json). A minimal selector + replay transport drive it; the full
 * designed gallery (DESIGN-showcase-ui.md) is a separate ticket — this keeps the
 * UI minimal but functional and proves the catalog/replay contract.
 *
 * The gallery defaults to REPLAY (no live backend). useAgentStream remains
 * available for a live mode (VITE_SHOWCASE_AGENT_URL) but is not wired here.
 */

import { useMemo, useState } from 'react';
import type { EngineHost } from '@repo/dc-wasm';
import { useShowcaseEngine } from './engine/ShowcaseEngine';
import { VIEWS, getView } from './views/registry';
import { useViewSwitch } from './views/useViewSwitch';

function fmtTime(fraction: number, durationMs: number): string {
  const ms = fraction * durationMs;
  const s = Math.floor(ms / 1000);
  return `0:${String(s).padStart(2, '0')}`;
}

export default function App() {
  const { canvasRef, host, status, error } = useShowcaseEngine();
  const [selectedId, setSelectedId] = useState(VIEWS[0]?.id ?? '');
  const view = useMemo(() => getView(selectedId) ?? VIEWS[0] ?? null, [selectedId]);

  const { progress, playing, setPlaying, restart } = useViewSwitch(host, view);

  const statusLabel =
    status === 'ready'
      ? 'Replay'
      : status === 'error'
        ? `Engine error: ${error ?? 'unknown'}`
        : 'Loading engine…';

  const durationMs = view?.records.meta.durationMs ?? 0;

  return (
    <div className="showcase-root">
      <header className="showcase-titlebar">
        <span className="showcase-title">
          <span className="accent">DynaCharting</span> Showcase
        </span>

        {VIEWS.length > 1 && (
          <select
            className="showcase-view-select"
            value={selectedId}
            onChange={(e) => setSelectedId(e.target.value)}
            aria-label="Select view"
          >
            {VIEWS.map((v) => (
              <option key={v.id} value={v.id}>
                {v.meta.title} · {v.meta.tier}
              </option>
            ))}
          </select>
        )}

        <span className="showcase-transport">
          <button type="button" onClick={() => setPlaying(!playing)} aria-label={playing ? 'Pause' : 'Play'}>
            {playing ? '⏸' : '▶'}
          </button>
          <button type="button" onClick={restart} aria-label="Restart">
            ↻
          </button>
          <span className="showcase-progress" aria-hidden>
            <span className="showcase-progress-fill" style={{ width: `${Math.round(progress * 100)}%` }} />
          </span>
          <span className="showcase-time">
            {fmtTime(progress, durationMs)} / {fmtTime(1, durationMs)}
          </span>
        </span>

        <span className="showcase-status">
          <span className={`dot ${status === 'ready' ? 'ready' : status === 'error' ? 'error' : ''}`} />
          {statusLabel}
        </span>
      </header>
      <div className="showcase-canvas-wrap">
        <canvas ref={canvasRef} className="showcase-canvas" />
      </div>
    </div>
  );
}
