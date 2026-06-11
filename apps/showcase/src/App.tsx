/* apps/showcase/src/App.tsx
 *
 * The DynaCharting · Frontier showcase shell (T5.1) — the designed public
 * gallery. Composes the four-act narrative (DESIGN §1) over a single shared
 * engine instance:
 *
 *   Act I   /          — Hero: thesis + live flagship view + data-path spine
 *   Act II  /gallery   — tier-grouped gallery; /view/:id single-view + explainer
 *   Act III /frontier  — the frontier map (the payoff)
 *   Act IV  /report    — the plain close + "how to add a view"
 *
 * One EngineHost (ShowcaseEngine) drives one <canvas> that is PORTALED
 * (EngineCanvas) into whichever route slot is active — the engine survives
 * route changes, no churn (§6 "one engine"). useViewSwitch applies the selected
 * view's manifest and loop-replays its captured records (T5.6). When WebGPU is
 * absent we render the designed still-image fallback (T5.7). Keyboard:
 * space=play/pause, r=restart, ←/→ in single-view = prev/next.
 */

import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { useShowcaseEngine } from './engine/ShowcaseEngine';
import { VIEWS, getView } from './views/registry';
import { useViewSwitch } from './views/useViewSwitch';
import { useRouter, type Route } from './router';
import { isWebGPUSupported } from './useWebGPU';
import { AppBar, type StatusKind } from './components/AppBar';
import { Hero } from './components/Hero';
import { Gallery } from './components/Gallery';
import { SingleView } from './components/SingleView';
import { FrontierMap } from './components/FrontierMap';
import { Report } from './components/Report';
import { Unsupported } from './components/Unsupported';
import { EngineCanvas } from './components/EngineCanvas';
import type { TransportProps } from './components/Transport';

const FLAGSHIP_ID = VIEWS.find((v) => v.meta.tier === 'native')?.id ?? VIEWS[0]?.id ?? '';

export default function App() {
  const webgpu = useMemo(() => isWebGPUSupported(), []);
  const { route, navigate } = useRouter();
  const { canvasRef, host, status, error } = useShowcaseEngine();

  // Which view is live in the shared engine. On the hero we render the flagship;
  // in single-view the routed view; elsewhere we keep the last view warm.
  const routeViewId = route.name === 'view' ? route.id : undefined;
  const [lastViewId, setLastViewId] = useState(FLAGSHIP_ID);
  const activeViewId = routeViewId ?? (route.name === 'home' ? FLAGSHIP_ID : lastViewId);
  const view = useMemo(() => getView(activeViewId) ?? VIEWS[0] ?? null, [activeViewId]);

  useEffect(() => {
    if (routeViewId) setLastViewId(routeViewId);
  }, [routeViewId]);

  const { progress, playing, setPlaying, restart, loop, setLoop } = useViewSwitch(webgpu ? host : null, view);

  // --- canvas slot routing (portal target for the one shared canvas) ---
  const [slot, setSlot] = useState<HTMLElement | null>(null);
  const keepAliveRef = useRef<HTMLDivElement | null>(null);
  const ready = status === 'ready';

  // --- transport props (shared by app bar + single-view inline scrubber) ---
  const durationMs = view?.records.meta.durationMs ?? 0;
  const transport: TransportProps = {
    progress,
    playing,
    onPlayToggle: () => setPlaying(!playing),
    onRestart: restart,
    durationMs,
    loop,
    onLoopToggle: () => setLoop(!loop),
    variant: 'compact',
  };

  // --- keyboard: space=play/pause, r=restart, ←/→ prev/next in single-view ---
  const onSelectView = useCallback((id: string) => navigate({ name: 'view', id }), [navigate]);
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      const tag = (e.target as HTMLElement)?.tagName;
      if (tag === 'INPUT' || tag === 'SELECT' || tag === 'TEXTAREA') return;
      if (e.code === 'Space') {
        e.preventDefault();
        setPlaying(!playing);
      } else if (e.key === 'r' || e.key === 'R') {
        restart();
      } else if (route.name === 'view' && (e.key === 'ArrowRight' || e.key === 'ArrowLeft')) {
        const idx = VIEWS.findIndex((v) => v.id === route.id);
        const next = e.key === 'ArrowRight' ? idx + 1 : idx - 1;
        if (next >= 0 && next < VIEWS.length) navigate({ name: 'view', id: VIEWS[next].id });
      }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [playing, setPlaying, restart, route, navigate]);

  // --- status pill ---
  const statusKind: StatusKind = !webgpu ? 'error' : status === 'error' ? 'error' : ready ? 'ok' : 'connecting';
  const statusLabel = !webgpu
    ? 'No WebGPU'
    : status === 'error'
      ? 'Error'
      : ready
        ? 'Live'
        : 'Connecting';

  // The global transport shows in single-view + frontier (per §3.1).
  const showTransport = webgpu && (route.name === 'view' || route.name === 'frontier');

  if (!webgpu) {
    return (
      <div className="showcase-root">
        <AppBar route={route} onNavigate={navigate} status="error" statusLabel="No WebGPU" />
        {route.name === 'frontier' ? (
          <FrontierMap onNavigate={navigate} />
        ) : route.name === 'report' ? (
          <Report onNavigate={navigate} />
        ) : (
          <Unsupported />
        )}
      </div>
    );
  }

  return (
    <div className="showcase-root">
      <AppBar
        route={route}
        onNavigate={navigate}
        status={statusKind}
        statusLabel={statusLabel}
        transport={showTransport ? transport : undefined}
      />

      {/* The one shared engine canvas, portaled into the active slot. */}
      <EngineCanvas
        canvasRef={canvasRef}
        status={status}
        error={error}
        ready={ready}
        slot={route.name === 'home' || route.name === 'view' ? slot : null}
        keepAlive={keepAliveRef.current ?? document.body}
      />
      {/* Hidden keep-alive host so the canvas stays mounted off-route. */}
      <div ref={keepAliveRef} style={{ position: 'fixed', width: 1, height: 1, left: -9999, top: -9999, overflow: 'hidden' }} aria-hidden />

      {route.name === 'home' && <Hero onNavigate={navigate} onSlot={setSlot} />}

      {route.name === 'gallery' && <Gallery currentId={routeViewId} onOpen={onSelectView} />}

      {route.name === 'view' &&
        (view ? (
          <SingleView
            view={view}
            onSelect={onSelectView}
            onSlot={setSlot}
            transport={{ ...transport, variant: 'full', onScrub: undefined }}
          />
        ) : (
          <div className="page">
            <div className="state-panel">
              <span className="state-title">View not found</span>
              <span className="state-detail mono">{route.id}</span>
              <button className="btn" onClick={() => navigate({ name: 'gallery' })}>
                Back to the gallery
              </button>
            </div>
          </div>
        ))}

      {route.name === 'frontier' && <FrontierMap onNavigate={navigate} />}

      {route.name === 'report' && <Report onNavigate={navigate} />}
    </div>
  );
}
