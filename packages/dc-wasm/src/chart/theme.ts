/* packages/dc-wasm/src/chart/theme.ts — ENC-1253 (chart-quality-bar SPEC D1 tier 3, D4)
 *
 * THE AXIS FURNITURE'S COLOURS, AND WHERE THEY COME FROM.
 *
 * SPEC D1's tier-3 row does not ask whether a chart looks nice. It asks for
 * PROVENANCE: "the render's colours are drawn from the active `Theme`, not
 * literals." ENC-1261 scored the live reference chart against that question and
 * it FAILED — the candle colours are hardcoded floats in a third repo
 * (`embassy/internal/pipeline/recipe_candles_with_overlays_v1.go:415-416`), they
 * match no `Theme` preset, and the theme layer
 * (`core/include/dc/style/Theme.hpp`, six presets, `ThemeManager`, five test
 * files) has **zero non-test callers** (SPEC §1.2).
 *
 * ── WHY THIS FILE EXISTS, AND WHY IT IS NOT A FOURTH COLOUR SOURCE ──────────
 *
 * The axis furniture ENC-1253 draws is authored in TypeScript, in the browser,
 * against the WASM host's `applyControl` surface. `dc::Theme` is C++ and is
 * **not bound into the WASM module** — the browser surface is the single Embind
 * `DcEngineHost` class, so `darkTheme()` and friends are dead-stripped exactly
 * as the `dc::*Recipe` family is (CLAUDE.md, "Unbound means dead-stripped").
 * Binding the theme layer through to the browser is ENC-1259's either/or (D4),
 * not this ticket.
 *
 * So the choice here was: invent axis colours (a fourth literal source, the
 * precise thing the SPEC is about), or TRANSCRIBE the existing one. This is the
 * transcription. Every number below is copied from `core/src/style/Theme.cpp`
 * and `core/include/dc/style/Theme.hpp`, and **`theme.test.ts` parses those two
 * C++ files and asserts every value here still matches them**. It is therefore
 * a mirror that cannot drift silently, rather than a second opinion:
 *
 *   - change a preset in C++ and forget this file  → the test fails
 *   - change this file and not the C++             → the test fails
 *   - ENC-1259 binds `dc::Theme` into the host     → delete this file, keep the
 *                                                     `AxisTheme` shape, and the
 *                                                     call sites do not move
 *
 * Only the ENGINE-GENERIC axis knobs are mirrored (`gridColor`, `tickColor`,
 * `labelColor`, `textColor`, `backgroundColor`, the line widths and the grid
 * dash/opacity). The indexed palette — which is where candle up/down lives — is
 * deliberately NOT mirrored: that is the half ENC-1259 has to settle across
 * repos, and duplicating it here would be picking a side of that decision in a
 * file whose job is the axis.
 */

import type { Rgba } from "./SceneBuilder";

/** An RGBA as the C++ `float[4]` carries it — components in [0,1]. */
export type Rgba4 = readonly [number, number, number, number];

/**
 * The axis-relevant subset of `dc::Theme` (`core/include/dc/style/Theme.hpp`).
 * Field names and units match the C++ struct one-for-one so the mapping is a
 * rename-free copy when ENC-1259 makes the real theme reachable.
 */
export interface AxisTheme {
  /** `Theme::name` — "Dark", "Light", "Midnight", "Neon", "Pastel", "Bloomberg". */
  name: string;
  /** `Theme::backgroundColor` — the pane clear colour. */
  backgroundColor: Rgba4;
  /** `Theme::textColor` — axis titles and free text. */
  textColor: Rgba4;
  /** `Theme::gridColor` — gridlines. D11 puts a CEILING on its contrast. */
  gridColor: Rgba4;
  /** `Theme::tickColor` — tick marks and the spine. D11 floor: >= 3:1. */
  tickColor: Rgba4;
  /** `Theme::labelColor` — tick labels. D11 floor: >= 4.5:1. */
  labelColor: Rgba4;
  /** `Theme::gridLineWidth`, in pixels. */
  gridLineWidth: number;
  /** `Theme::tickLineWidth`, in pixels — also the spine's width. */
  tickLineWidth: number;
  /** `Theme::gridDashLength`, in pixels. 0 = solid. */
  gridDashLength: number;
  /** `Theme::gridGapLength`, in pixels. */
  gridGapLength: number;
  /** `Theme::gridOpacity` in [0,1] — MULTIPLIED into `gridColor`'s alpha. */
  gridOpacity: number;
}

/** `Theme.hpp`'s in-struct defaults — what `darkTheme()` returns unmodified. */
const DEFAULTS = {
  backgroundColor: [0.1, 0.1, 0.12, 1.0],
  textColor: [0.8, 0.8, 0.85, 1.0],
  // ENC-1316: alpha 0.4, mirrored from `Theme.hpp`, where the derivation is
  // written out. Short form: the grid is drawn OVER the data, so D11 band 3 is
  // measured against what it crosses; an opaque grid across a bright mark is a
  // mark (6.25 : 1 on `audio-waveform`). 0.4 is bounded above by the 2.0 : 1
  // ceiling over the brightest mark and below by the 10/255 visibility floor
  // over the darkest pane.
  gridColor: [0.2, 0.2, 0.25, 0.4],
  tickColor: [0.4, 0.4, 0.45, 1.0],
  labelColor: [0.7, 0.7, 0.75, 1.0],
  gridLineWidth: 1.0,
  tickLineWidth: 1.0,
  gridDashLength: 0.0,
  gridGapLength: 0.0,
  gridOpacity: 1.0,
} as const;

/** `dc::darkTheme()` — the struct defaults, unmodified. */
export const darkAxisTheme: Readonly<AxisTheme> = { name: "Dark", ...DEFAULTS };

/** `dc::lightTheme()`. */
export const lightAxisTheme: Readonly<AxisTheme> = {
  ...DEFAULTS,
  name: "Light",
  backgroundColor: [0.95, 0.95, 0.96, 1.0],
  textColor: [0.15, 0.15, 0.2, 1.0],
  gridColor: [0.85, 0.85, 0.87, 1.0],
  tickColor: [0.5, 0.5, 0.55, 1.0],
  labelColor: [0.2, 0.2, 0.25, 1.0],
};

/** `dc::midnightTheme()`. */
export const midnightAxisTheme: Readonly<AxisTheme> = {
  ...DEFAULTS,
  name: "Midnight",
  backgroundColor: [0.04, 0.055, 0.1, 1.0],
  textColor: [0.6, 0.65, 0.75, 1.0],
  gridColor: [0.12, 0.14, 0.2, 0.6],
  tickColor: [0.25, 0.3, 0.4, 1.0],
  labelColor: [0.5, 0.55, 0.65, 1.0],
};

/** `dc::neonTheme()`. */
export const neonAxisTheme: Readonly<AxisTheme> = {
  ...DEFAULTS,
  name: "Neon",
  backgroundColor: [0.02, 0.02, 0.04, 1.0],
  textColor: [0.5, 0.9, 1.0, 1.0],
  gridColor: [0.0, 0.15, 0.2, 0.4],
  tickColor: [0.0, 0.5, 0.6, 1.0],
  labelColor: [0.4, 0.8, 0.9, 1.0],
  gridDashLength: 4.0,
  gridGapLength: 4.0,
};

/** `dc::pastelTheme()`. */
export const pastelAxisTheme: Readonly<AxisTheme> = {
  ...DEFAULTS,
  name: "Pastel",
  backgroundColor: [0.97, 0.95, 0.92, 1.0],
  textColor: [0.35, 0.32, 0.3, 1.0],
  gridColor: [0.85, 0.82, 0.78, 0.5],
  tickColor: [0.6, 0.55, 0.5, 1.0],
  labelColor: [0.4, 0.37, 0.35, 1.0],
  gridOpacity: 0.5,
};

/** `dc::bloombergTheme()`. */
export const bloombergAxisTheme: Readonly<AxisTheme> = {
  ...DEFAULTS,
  name: "Bloomberg",
  backgroundColor: [0.0, 0.0, 0.0, 1.0],
  textColor: [1.0, 0.6, 0.0, 1.0],
  gridColor: [0.2, 0.2, 0.2, 0.8],
  tickColor: [0.45, 0.45, 0.45, 1.0],
  labelColor: [1.0, 0.6, 0.0, 1.0],
  gridDashLength: 3.0,
  gridGapLength: 3.0,
};

/** Every mirrored preset, by the name the C++ preset sets on `Theme::name`. */
export const AXIS_THEMES: Readonly<Record<string, Readonly<AxisTheme>>> = {
  Dark: darkAxisTheme,
  Light: lightAxisTheme,
  Midnight: midnightAxisTheme,
  Neon: neonAxisTheme,
  Pastel: pastelAxisTheme,
  Bloomberg: bloombergAxisTheme,
};

/**
 * The default for a dark surface — `darkTheme()`, and the choice is MEASURED
 * rather than tasteful.
 *
 * SPEC §1.2 praises `midnightTheme()`'s grid ("0.12, 0.14, 0.20 @ α0.6 — muted,
 * professional") and it does clear D11's gridline ceiling. But D11 has three
 * bands, and `midnightTheme()` fails the second one on its own surface: its
 * `tickColor` (0.25, 0.30, 0.40) reaches only **2.46 : 1** against the black the
 * render target clears to (`DawnSceneRenderer::render` clears 0,0,0,1 outside
 * every pane), under D11's **3 : 1** floor for meaningful non-text. Ticks and
 * the spine are exactly that.
 *
 * `darkTheme()` clears all three, measured with `contrastRatio` below against
 * black (the gutters) and against the showcase panes' 0.05/0.05/0.08 (the plot
 * area) — the numbers are asserted in `theme.test.ts`, not asserted here:
 *
 * | band | colour | ratio | D11 |
 * |---|---|---|---|
 * | text floor ≥ 4.5 | `labelColor` 0.70/0.70/0.75 | **10.07 : 1** vs black | ✓ |
 * | non-text floor ≥ 3 | `tickColor` 0.40/0.40/0.45 | **3.71 : 1** vs black | ✓ |
 * | grid ceiling ≤ 2.0 | `gridColor` 0.20/0.20/0.25 | **1.56 : 1** vs pane | ✓ |
 * | grid visibility ≥ 10/255 | `gridColor` | **38 / 255** vs pane | ✓ |
 *
 * So this is the preset the axis can be drawn in without failing the standard it
 * is being drawn to satisfy. Picking `midnightTheme()` for its reputation and
 * shipping a 2.46 : 1 tick would be §1.0's tier-3 mistake exactly: grading the
 * question you remembered instead of the one written down.
 */
export const defaultAxisTheme = darkAxisTheme;

// ── D11, measurable ─────────────────────────────────────────────────────────

/**
 * WCAG 2.x relative luminance of an sRGB component in [0,1].
 *
 * The `Theme` floats ARE sRGB (they go to the GPU as the colour, and the
 * framebuffer is RGBA8 sRGB-encoded), so no linearisation beyond this one is
 * owed. This is the same transfer function `harness/contrast.py` applies to the
 * delivered raster, so a prediction here and a measurement there are comparable.
 */
function srgbToLinear(c: number): number {
  return c <= 0.03928 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4);
}

/** WCAG 2.x relative luminance of an RGB triple in [0,1]. */
export function relativeLuminance(c: Rgba4 | readonly [number, number, number]): number {
  return (
    0.2126 * srgbToLinear(c[0]) + 0.7152 * srgbToLinear(c[1]) + 0.0722 * srgbToLinear(c[2])
  );
}

/** WCAG 2.x contrast ratio between two colours. Order-independent, 1..21. */
export function contrastRatio(
  a: Rgba4 | readonly [number, number, number],
  b: Rgba4 | readonly [number, number, number],
): number {
  const la = relativeLuminance(a);
  const lb = relativeLuminance(b);
  return (Math.max(la, lb) + 0.05) / (Math.min(la, lb) + 0.05);
}

/** D11's numbers, as constants rather than as prose. */
export const D11_BANDS = {
  textFloor: 4.5,
  largeTextFloor: 3.0,
  nonTextFloor: 3.0,
  gridCeiling: 2.0,
  /** Max per-channel sRGB distance, in 0..255 units. NOT a ratio. */
  gridDeltaFloor: 10,
} as const;

/** What a theme's axis colours do against one background. */
export interface D11Verdict {
  pass: boolean;
  failures: string[];
  ratios: { label: number; tick: number; grid: number; gridDelta255: number };
}

/**
 * Score an `AxisTheme`'s three ink classes against `background`, in D11's bands.
 *
 * The gridline is composited over the background at its own alpha before being
 * measured, because that is what the raster will contain — measuring the
 * unblended `gridColor` of a theme like `midnightTheme()` (α 0.6) overstates
 * its contrast by a wide margin and would let a gridline that is invisible on
 * screen pass the visibility floor.
 */
export function checkD11AxisBands(theme: AxisTheme, background: Rgba4): D11Verdict {
  const bg: Rgba4 = background;
  const g = gridRgba(theme);
  const a = g.a ?? 1;
  const composited: Rgba4 = [
    g.r * a + bg[0] * (1 - a),
    g.g * a + bg[1] * (1 - a),
    g.b * a + bg[2] * (1 - a),
    1,
  ];
  const label = contrastRatio(theme.labelColor, bg);
  const tick = contrastRatio(theme.tickColor, bg);
  const grid = contrastRatio(composited, bg);
  const gridDelta255 = Math.max(
    Math.abs(composited[0] - bg[0]),
    Math.abs(composited[1] - bg[1]),
    Math.abs(composited[2] - bg[2]),
  ) * 255;

  const failures: string[] = [];
  if (!(label >= D11_BANDS.textFloor)) {
    failures.push(`labelColor ${label.toFixed(2)}:1 < ${D11_BANDS.textFloor}:1 (text floor)`);
  }
  if (!(tick >= D11_BANDS.nonTextFloor)) {
    failures.push(`tickColor ${tick.toFixed(2)}:1 < ${D11_BANDS.nonTextFloor}:1 (non-text floor)`);
  }
  if (!(grid <= D11_BANDS.gridCeiling)) {
    failures.push(`gridColor ${grid.toFixed(2)}:1 > ${D11_BANDS.gridCeiling}:1 (grid ceiling)`);
  }
  if (!(gridDelta255 >= D11_BANDS.gridDeltaFloor)) {
    failures.push(
      `gridColor delta ${gridDelta255.toFixed(1)}/255 < ${D11_BANDS.gridDeltaFloor}/255 (grid visibility)`,
    );
  }
  if (!(grid < tick)) {
    failures.push(`ordering: grid ${grid.toFixed(2)}:1 is not below tick ${tick.toFixed(2)}:1`);
  }
  return { pass: failures.length === 0, failures, ratios: { label, tick, grid, gridDelta255 } };
}

/**
 * The colour the canvas shows where no pane paints: `DawnSceneRenderer::render`
 * clears the render target to opaque black before walking the panes, and a pane
 * only paints a clear-quad when it `hasClearColor`. So the axis gutters — which
 * are outside every data pane — are black, and that is the background every
 * tick, spine and label in them is measured against. Verified in the delivered
 * raster, not assumed: the reference chart's captured canvas reports
 * `background: [0, 0, 0]` (`harness/verdicts/live-nexo-candles.verdict.json`).
 */
export const CANVAS_CLEAR_BLACK: Rgba4 = [0, 0, 0, 1];

/** Look a mirrored preset up by name. Case-insensitive; null when unknown. */
export function axisThemeByName(name: string): Readonly<AxisTheme> | null {
  const key = Object.keys(AXIS_THEMES).find((k) => k.toLowerCase() === name.trim().toLowerCase());
  return key ? AXIS_THEMES[key] : null;
}

/** A `Theme` `float[4]` as the `Rgba` the draw commands take. */
export function rgba(c: Rgba4): Rgba {
  return { r: c[0], g: c[1], b: c[2], a: c[3] };
}

/**
 * The gridline colour with `gridOpacity` folded into its alpha — the same
 * composition `PaletteGroup::Kind::GridStyle` performs in
 * `core/src/style/ThemeManager.cpp` ("theme.gridColor/opacity/dash/gap ->
 * setDrawItemStyle"). Do not use `gridColor` raw: on `pastelTheme()` that
 * doubles the gridline's alpha (0.5 * 0.5 = 0.25 is the intent).
 */
export function gridRgba(theme: AxisTheme): Rgba {
  const c = theme.gridColor;
  return { r: c[0], g: c[1], b: c[2], a: c[3] * theme.gridOpacity };
}
