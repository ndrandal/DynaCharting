/* apps/showcase/src/views/registry.ts
 *
 * The typed view catalog (CONTRACT-view-catalog.md). Every showcase view lives
 * in apps/showcase/views/<id>/ as five files; this module imports each view's
 * pieces into one typed array, `VIEWS`. The gallery iterates VIEWS; the
 * switching controller loads the selected view's manifest + records and replays
 * it. Adding a view = adding its directory + a `defineView(...)` entry here
 * (never editing the components — DESIGN-showcase-ui.md §6 "data-driven").
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
import type { Records, GrowthSync, XAnchorSpec } from '../engine/useReplay';

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
  /** X-anchor framing derived from view.json (window/clip + baked Y), when xAnchor. */
  xAnchor?: XAnchorSpec;
}

/** What a view's manifest.ts module is expected to export. */
export interface ViewManifestModule {
  manifest: SceneManifest;
  growth?: GrowthSync;
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
    xAnchor,
  };
}

// --- view imports (one block per view; add a block to add a view) ---
import candlesAaplMeta from '../../views/candles-aapl/view.json';
import * as candlesAaplModule from '../../views/candles-aapl/manifest';
import candlesAaplRecords from '../../views/candles-aapl/records.json';
import candlesAaplExplainer from '../../views/candles-aapl/explainer.md?raw';

export const VIEWS: ShowcaseView[] = [
  defineView({
    meta: candlesAaplMeta as ViewMeta,
    module: candlesAaplModule,
    records: candlesAaplRecords as Records,
    explainer: candlesAaplExplainer,
  }),
];

/** Look up a view by id (used by the switching controller / deep-links). */
export function getView(id: string): ShowcaseView | undefined {
  return VIEWS.find((v) => v.id === id);
}
