/* apps/showcase/src/components/ErrorBoundary.tsx — ENC-1313
 *
 * WHY THIS FILE EXISTS, AND WHY IT IS NOT DEFENSIVE PROGRAMMING.
 *
 * Until this ticket there was no error boundary anywhere in the showcase. React
 * treats that as "unmount the whole tree": one throw from any render, lifecycle
 * or PASSIVE EFFECT below `<App>` emptied `#root` and left a white page with no
 * canvas, no app bar, and nothing on screen naming what had happened. That is
 * not a hypothetical — it is what shipped. At `1e125e3`, opening **11 of the 22
 * showcase views by direct link** did exactly that, 3/3 cold loads each, from a
 * single `PlotBoxError` thrown inside `useEngineAxis`'s passive effect while the
 * canvas was still at its 1x1 pre-layout bootstrap size (ENC-1262 found it;
 * `specs/2026-09-19-chart-quality-bar/harness/deeplink-crash.mjs` is the
 * re-check).
 *
 * ENC-1313 fixes that throw at its source — `tryPlotBox` + `engineAxisSpec`'s
 * named refusal. This file fixes the thing that made ONE throw cost the WHOLE
 * app, which is a different and larger problem: the next throw in that effect,
 * from any cause, had the same blast radius and would have been just as
 * invisible. A guard that makes one instance impossible is not the same as a
 * guard that makes the class survivable, and this is the second one.
 *
 * ── TWO BOUNDARIES, DELIBERATELY, BECAUSE THEY PROTECT DIFFERENT THINGS ─────
 *
 * 1. `ChartChromeBoundary` wraps the chrome overlay (axes, legend, colorbar, FPS
 *    HUD). The chrome is FURNITURE composited over the engine canvas; the chart
 *    is the canvas. So a chrome failure must degrade to "a chart with no
 *    furniture", which is a real and previously-shipped state (§1.3: eight views
 *    draw no axis at all), not to "no chart".
 *
 * 2. `AppBoundary` wraps `<App>` in `main.tsx`. It cannot keep anything running
 *    — by the time it fires, App's tree is gone — but it replaces a BLANK PAGE
 *    with a named, copyable diagnostic. The single worst property of the bug
 *    this ticket fixes was not that it crashed; it was that the crash and "the
 *    page has not loaded yet" were pixel-identical, so three sessions read it as
 *    a slow load.
 *
 * ── THEY DO NOT SWALLOW ANYTHING ────────────────────────────────────────────
 *
 * A boundary that hides an error is worse than the crash, because the crash at
 * least announced itself. So every catch does all three of:
 *
 *   - `console.error`s the error and the component stack;
 *   - renders something a human can SEE, in place, saying what declined;
 *   - pushes onto `window.__dcBoundaryErrors`, so a harness can ASK the page
 *     whether a boundary fired instead of inferring it from a screenshot. This
 *     is the same discipline ENC-1252/ENC-1253 applied to the domain and the
 *     axis (`window.__dcAxisDomain`, `window.__dcEngineAxis`).
 *
 * A boundary fires only for a bug. `deeplink-crash.mjs` passing is therefore the
 * weaker claim; `window.__dcBoundaryErrors` being EMPTY is the stronger one, and
 * it is the one ENC-1313 verified.
 */

import { Component, type ErrorInfo, type ReactNode } from 'react';

/** One caught error, in the shape a harness reads off the page. */
export interface BoundaryErrorRecord {
  /** Which boundary caught it — 'chrome' (furniture) or 'app' (everything). */
  boundary: 'chrome' | 'app';
  /** `error.name`, e.g. `PlotBoxError`. */
  name: string;
  /** `error.message`. */
  message: string;
  /** React's component stack, so the offending component is nameable. */
  componentStack: string;
  /** When it fired. */
  at: string;
}

declare global {
  interface Window {
    /**
     * Every error an error boundary has caught this page-load (ENC-1313).
     * ABSENT or EMPTY is the healthy state — a boundary firing always means a
     * bug, never a normal transient. `deeplink-crash.mjs` checks that the app is
     * still mounted; this is how you check that it did not merely survive.
     */
    __dcBoundaryErrors?: BoundaryErrorRecord[];
  }
}

function record(boundary: 'chrome' | 'app', error: Error, info: ErrorInfo): void {
  const rec: BoundaryErrorRecord = {
    boundary,
    name: error?.name ?? 'Error',
    message: error?.message ?? String(error),
    componentStack: info?.componentStack ?? '',
    at: new Date().toISOString(),
  };
  // Said out loud, every time. A boundary is not a place to be quiet.
  console.error(`[showcase] ${boundary} error boundary caught ${rec.name}: ${rec.message}`, error, info?.componentStack);
  if (typeof window !== 'undefined') {
    window.__dcBoundaryErrors = [...(window.__dcBoundaryErrors ?? []), rec];
  }
}

interface BoundaryState {
  error: Error | null;
}

interface BoundaryProps {
  children: ReactNode;
}

/**
 * Keeps a chrome failure inside the chrome. The engine canvas, the app bar, the
 * router and the transport all survive; the furniture is replaced by a visible
 * badge naming what failed.
 *
 * It does NOT retry. The chrome is driven by the domain, the ticks and the
 * canvas size, all of which change constantly, so a boundary that re-rendered on
 * every prop change would loop through the same throw many times a second and
 * bury the console. It resets only when the VIEW changes — `key={view.id}` at
 * the call site remounts it — which is the one boundary a person crosses on
 * purpose.
 */
export class ChartChromeBoundary extends Component<BoundaryProps, BoundaryState> {
  state: BoundaryState = { error: null };

  static getDerivedStateFromError(error: Error): BoundaryState {
    return { error };
  }

  componentDidCatch(error: Error, info: ErrorInfo): void {
    record('chrome', error, info);
  }

  render(): ReactNode {
    const { error } = this.state;
    if (!error) return this.props.children;
    return (
      <div className="chrome-overlay" data-dc-chrome-error={`${error.name}: ${error.message}`}>
        <div className="chrome-error-badge" role="status">
          <strong>chart chrome declined</strong>
          <span className="mono">
            {error.name}: {error.message}
          </span>
          <span>The chart is still rendering; its axes, legend and HUD are not.</span>
        </div>
      </div>
    );
  }
}

/**
 * The last line before a white page. Renders a named diagnostic in place of the
 * blank `#root` that a throw above every boundary used to produce.
 */
export class AppBoundary extends Component<BoundaryProps, BoundaryState> {
  state: BoundaryState = { error: null };

  static getDerivedStateFromError(error: Error): BoundaryState {
    return { error };
  }

  componentDidCatch(error: Error, info: ErrorInfo): void {
    record('app', error, info);
  }

  render(): ReactNode {
    const { error } = this.state;
    if (!error) return this.props.children;
    return (
      <div className="showcase-root" data-dc-app-error={`${error.name}: ${error.message}`}>
        <div className="page">
          <div className="state-panel">
            <span className="state-title">The showcase stopped</span>
            <span className="state-detail mono">
              {error.name}: {error.message}
            </span>
            <span className="state-detail">
              This is a bug, not a slow load. The full stack is in the console, and{' '}
              <code>window.__dcBoundaryErrors</code> carries it for a harness.
            </span>
            <button className="btn" onClick={() => window.location.reload()}>
              Reload
            </button>
          </div>
        </div>
      </div>
    );
  }
}
