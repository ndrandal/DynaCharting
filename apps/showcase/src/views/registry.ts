/* apps/showcase/src/views/registry.ts
 *
 * The typed view catalog (CONTRACT-view-catalog.md), assembled by AUTO-DISCOVERY.
 *
 * Every showcase view lives in apps/showcase/views/<id>/ as a directory of
 * files; this module discovers them with Vite `import.meta.glob` and assembles
 * each into a `ShowcaseView`. **Adding a view = dropping a `views/<id>/` dir —
 * no edit to this file.** This is what lets the parallel view agents each ship
 * a view by adding files only (DESIGN-showcase-ui.md §6 "data-driven, no
 * hardcoding"; T6.5 "how to add a view").
 *
 * Per-view directory (apps/showcase/views/<id>/):
 *   view.json       — metadata: { id, title, tier, referenceTool, blurb,
 *                     datasetId, transform?, xAnchor? }  (ViewMeta below)
 *   manifest.ts     — exports `manifest: SceneManifest` (+ optional `growth`,
 *                     `xAnchor`) — the ordered SceneDocument commands, no uploads
 *   instruction.json — embassy showcase-explicit-v1 instruction (capture only)
 *   records.json    — captured dataplane frames (Records) the replay engine plays
 *   explainer.md    — front-matter + DATA+TECHNIQUE copy + fact block
 */

import type { SceneManifest } from '../scene/commands';
import type { Records, GrowthSync, GrowthSeries, XAnchorSpec } from '../engine/useReplay';
import type { ChromeSpec } from '../chrome/types';

export type { ChromeSpec, AxisSpec, LegendItem, ColorbarSpec, AxisFormat, LegendKind, RGBA } from '../chrome/types';

/** Tier semantics + colors per DESIGN-showcase-ui.md (native green / composed blue / walled amber). */
export type ViewTier = 'native' | 'composed' | 'walled';

/** Baked data→clip transform. The showcase-explicit path has no RangeTracker,
 *  so framing is baked per view (CONTRACT-view-catalog.md "Transform framing"). */
export interface ViewTransform {
  sx: number;
  sy: number;
  tx: number;
  ty: number;
}

/** The contents of a view's `view.json`. */
export interface ViewMeta {
  id: string;
  title: string;
  tier: ViewTier;
  /** The familiar tool this view is "≈ in the spirit of" (orientation, never competition). */
  referenceTool: string;
  /** One-line gallery/card blurb. */
  blurb: string;
  /** Dataset key the mock GMA streams (e.g. "AAPL"). */
  datasetId: string;
  /** Baked data→clip transform (optional; absent ⇒ manifest sets its own). */
  transform?: ViewTransform;
  /** Re-anchor the transform's X to the first replayed record's recordIndex. */
  xAnchor?: boolean;
  /**
   * Optional 'logical chart' chrome — axes/gridlines/tick labels, a legend, and
   * (for heatmaps) a colorbar — rendered as a crisp HTML/SVG overlay over the
   * WebGPU canvas (NOT in-engine text). The overlay maps data→clip→pixel the
   * SAME way the engine does, driven by this view's baked `transform` + the
   * static data ranges declared here, so ticks/gridlines align with the rendered
   * geometry. All sub-blocks are optional; a view sets whichever apply
   * (cartesian charts → axes + legend; heatmaps → colorbar; treemap → legend
   * only). See apps/showcase/src/chrome/types.ts for the full field shapes and
   * candles-aapl / correlation-heatmap for reference usage.
   */
  chrome?: ChromeSpec;
}

/**
 * One fully-resolved catalog view: its metadata + the SceneManifest to apply +
 * the captured records to replay + the explainer copy. This is the contract the
 * gallery, the explainer panel, the frontier map, and the replay controller all
 * consume — and the shape the 6 remaining view agents produce per view.
 */
export interface ShowcaseView {
  /** Stable view id (matches the directory name + view.json.id). */
  id: string;
  /** Parsed view.json metadata. */
  meta: ViewMeta;
  /** The SceneManifest applied via applyManifest() on view-select. */
  manifest: SceneManifest;
  /** Captured dataplane frames the replay engine plays (records.json). */
  records: Records;
  /** Raw explainer markdown (front-matter + body), rendered by the UI. */
  explainer: string;
  /** Instanced-geometry growth descriptor, when the view's manifest exports one. */
  growth?: GrowthSync;
  /**
   * Every growing series in the view (candles + volume + SMA …). The replay
   * advances each geometry's vertexCount as its buffer grows (ENC-568 multi-
   * buffer growth). Absent for single-series / fixed-size views.
   */
  growthSeries?: GrowthSeries[];
  /** X-anchor framing derived from view.json (window/clip + baked Y), when xAnchor. */
  xAnchor?: XAnchorSpec;
  /** Logical-chart chrome (axes/legend/colorbar) from view.json, when present. */
  chrome?: ChromeSpec;
}

/** What a view's manifest.ts module is expected to export. */
export interface ViewManifestModule {
  manifest: SceneManifest;
  growth?: GrowthSync;
  /** Every growing series in the view (ENC-568 multi-buffer growth). */
  growthSeries?: GrowthSeries[];
}

/**
 * Assemble a ShowcaseView from its directory's loaded files. Derives the
 * XAnchorSpec from view.json.transform when `xAnchor` is set (window defaults to
 * 150 record-indices over clipX [-0.85,0.85], the proven slice framing; the Y
 * mapping is taken from the baked transform so it survives the X re-anchor).
 */
export function defineView(parts: {
  meta: ViewMeta;
  module: ViewManifestModule;
  records: Records;
  explainer: string;
}): ShowcaseView {
  const { meta, module, records, explainer } = parts;
  let xAnchor: XAnchorSpec | undefined;
  if (meta.xAnchor && meta.transform) {
    xAnchor = {
      xWindow: 150,
      clipMin: -0.85,
      clipMax: 0.85,
      sy: meta.transform.sy,
      ty: meta.transform.ty,
    };
  }
  return {
    id: meta.id,
    meta,
    manifest: module.manifest,
    records,
    explainer,
    growth: module.growth,
    growthSeries: module.growthSeries,
    xAnchor,
    chrome: meta.chrome,
  };
}

// --- AUTO-DISCOVERY (Vite import.meta.glob, eager) ---------------------------
// Each glob keys by the view directory's file path; the id is the <id> segment.
// Adding `views/<id>/{view.json,manifest.ts,records.json,explainer.md}` makes
// the view appear in VIEWS with zero edits here.

const metaModules = import.meta.glob<{ default: ViewMeta }>('../../views/*/view.json', { eager: true });
const manifestModules = import.meta.glob<ViewManifestModule>('../../views/*/manifest.ts', { eager: true });
const recordModules = import.meta.glob<{ default: Records }>('../../views/*/records.json', { eager: true });
const explainerModules = import.meta.glob<string>('../../views/*/explainer.md', {
  eager: true,
  query: '?raw',
  import: 'default',
});

/** Extract the `<id>` directory segment from a `../../views/<id>/<file>` path. */
function viewIdFromPath(path: string): string {
  const m = path.match(/\/views\/([^/]+)\//);
  return m ? m[1] : path;
}

/** Build a path→value index keyed by the view id for one glob result. */
function indexById<T>(modules: Record<string, T>, pick: (m: T) => unknown): Map<string, unknown> {
  const out = new Map<string, unknown>();
  for (const [path, mod] of Object.entries(modules)) {
    out.set(viewIdFromPath(path), pick(mod));
  }
  return out;
}

const metaById = indexById(metaModules, (m) => m.default);
const recordsById = indexById(recordModules, (m) => m.default);
const explainerById = indexById(explainerModules, (m) => m);
// manifest modules are kept whole (we need `manifest` + optional `growth`).
const manifestById = new Map<string, ViewManifestModule>();
for (const [path, mod] of Object.entries(manifestModules)) {
  manifestById.set(viewIdFromPath(path), mod);
}

const tierRank: Record<ViewTier, number> = { native: 0, composed: 1, walled: 2 };

/**
 * The discovered catalog. A directory is included only when it has all four
 * required files (a half-added view in flight is skipped rather than crashing
 * the gallery — so view agents can land files incrementally). Sorted by tier
 * (native → composed → walled) then title.
 */
export const VIEWS: ShowcaseView[] = Array.from(metaById.keys())
  .map((id): ShowcaseView | null => {
    const meta = metaById.get(id) as ViewMeta | undefined;
    const module = manifestById.get(id);
    const records = recordsById.get(id) as Records | undefined;
    const explainer = explainerById.get(id) as string | undefined;
    if (!meta || !module || !records || explainer === undefined) {
      if (import.meta.env?.DEV) {
        // eslint-disable-next-line no-console
        console.warn(`[showcase] view "${id}" is missing files; skipping until complete.`);
      }
      return null;
    }
    return defineView({ meta, module, records, explainer });
  })
  .filter((v): v is ShowcaseView => v !== null)
  .sort((a, b) => {
    const t = tierRank[a.meta.tier] - tierRank[b.meta.tier];
    return t !== 0 ? t : a.meta.title.localeCompare(b.meta.title);
  });

/** Look up a view by id (used by the switching controller / deep-links). */
export function getView(id: string): ShowcaseView | undefined {
  return VIEWS.find((v) => v.id === id);
}

/** Human-readable tier metadata (label + one-line meaning), shared everywhere. */
export const TIER_INFO: Record<ViewTier, { label: string; meaning: string }> = {
  native: { label: 'Native', meaning: 'the engine renders this directly from the manifest' },
  composed: { label: 'Composed', meaning: 'JSON manifest + upstream precompute (scalar-fan / texture-feed)' },
  walled: { label: 'Walled', meaning: 'rendered from precomputed data; the live-GPU version is the frontier' },
};

/** Stable tier order for grouping (native → composed → walled). */
export const TIER_ORDER: ViewTier[] = ['native', 'composed', 'walled'];

/** Views grouped by tier in canonical order (used by gallery + frontier map). */
export function viewsByTier(): { tier: ViewTier; views: ShowcaseView[] }[] {
  return TIER_ORDER.map((tier) => ({ tier, views: VIEWS.filter((v) => v.meta.tier === tier) }));
}
