/* apps/showcase/src/App.tsx
 *
 * The DynaCharting Showcase render shell (ENC-519, T0.3): a full-bleed WebGPU
 * canvas + a title bar. On engine-ready it resets any prior scene and applies
 * ONE hardcoded sample manifest, proving the WASM engine loads and renders a
 * visible SceneDocument in the browser. No auth, no forum, no tenant.
 *
 * Live WS data is a later phase; useAgentStream is wired but inert unless
 * VITE_SHOWCASE_AGENT_URL is set.
 */

import { useCallback, useRef } from 'react';
import type { EngineHost } from '@repo/dc-wasm';
import { useShowcaseEngine } from './engine/ShowcaseEngine';
import { useAgentStream } from './engine/useAgentStream';
import { applyManifest, resetScene } from './scene/sceneController';
import { SAMPLE_MANIFEST } from './scene/sampleManifest';
import type { SceneManifest } from './scene/commands';

export default function App() {
  // The last manifest we applied — used to tear down before applying the next.
  const appliedRef = useRef<SceneManifest | null>(null);

  const onReady = useCallback((host: EngineHost) => {
    resetScene(host, appliedRef.current);
    appliedRef.current = applyManifest(host, SAMPLE_MANIFEST);
  }, []);

  const { canvasRef, host, status, error } = useShowcaseEngine(onReady);

  // Inert unless VITE_SHOWCASE_AGENT_URL is configured.
  useAgentStream(host);

  const statusLabel =
    status === 'ready'
      ? 'Engine ready'
      : status === 'error'
        ? `Engine error: ${error ?? 'unknown'}`
        : 'Loading engine…';

  return (
    <div className="showcase-root">
      <header className="showcase-titlebar">
        <span className="showcase-title">
          <span className="accent">DynaCharting</span> Showcase
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
