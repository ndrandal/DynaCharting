/* ENC-1253 — the TS axis-theme mirror cannot drift from the C++ `dc::Theme`.
 *
 * `theme.ts` exists because `dc::Theme` is not bound into the WASM module, so
 * the browser-side axis cannot call it (SPEC D4 / ENC-1259 is the ticket that
 * would change that). A hand-copied palette is worth exactly as much as the
 * check that keeps it honest, so this test PARSES the two C++ sources —
 * `core/include/dc/style/Theme.hpp` for the struct defaults and
 * `core/src/style/Theme.cpp` for each preset's overrides — and asserts the TS
 * constants still agree, field by field.
 *
 * If this test fails after a C++ theme edit, the C++ is right and `theme.ts` is
 * stale. Fix the TS, never the assertion.
 *
 * The second half asserts D11's contrast bands on the theme the axis actually
 * defaults to, against the background the canvas actually has — so "the axis
 * colours meet the standard" is a measurement in the suite rather than a claim
 * in a comment.
 */

import { describe, it, expect } from "vitest";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";
import {
  AXIS_THEMES,
  CANVAS_CLEAR_BLACK,
  D11_BANDS,
  axisThemeByName,
  checkD11AxisBands,
  contrastRatio,
  darkAxisTheme,
  defaultAxisTheme,
  gridRgba,
  midnightAxisTheme,
  rgba,
  type AxisTheme,
  type Rgba4,
} from "./theme";

const HERE = dirname(fileURLToPath(import.meta.url));
// src/chart → repo root is four levels up.
const REPO_ROOT = resolve(HERE, "../../../..");
const THEME_HPP = resolve(REPO_ROOT, "core/include/dc/style/Theme.hpp");
const THEME_CPP = resolve(REPO_ROOT, "core/src/style/Theme.cpp");

/** The C++ preset function name for each mirrored theme. */
const PRESET_FN: Record<string, string> = {
  Dark: "darkTheme",
  Light: "lightTheme",
  Midnight: "midnightTheme",
  Neon: "neonTheme",
  Pastel: "pastelTheme",
  Bloomberg: "bloombergTheme",
};

/** The axis fields this module mirrors, and their C++ member names. */
const COLOR_FIELDS = ["backgroundColor", "textColor", "gridColor", "tickColor", "labelColor"] as const;
const SCALAR_FIELDS = [
  "gridLineWidth",
  "tickLineWidth",
  "gridDashLength",
  "gridGapLength",
  "gridOpacity",
] as const;

/** Parse `Theme.hpp`'s in-struct defaults for the fields we mirror. */
function parseHppDefaults(): { colors: Record<string, Rgba4>; scalars: Record<string, number> } {
  const src = readFileSync(THEME_HPP, "utf8");
  const colors: Record<string, Rgba4> = {};
  for (const f of COLOR_FIELDS) {
    const m = new RegExp(`float\\s+${f}\\[4\\]\\s*=\\s*\\{([^}]*)\\}`).exec(src);
    if (!m) throw new Error(`Theme.hpp: no default for ${f}`);
    const v = m[1].split(",").map((s) => Number(s.trim().replace(/f$/, "")));
    colors[f] = [v[0], v[1], v[2], v[3]];
  }
  const scalars: Record<string, number> = {};
  for (const f of SCALAR_FIELDS) {
    const m = new RegExp(`float\\s+${f}\\{([^}]*)\\}`).exec(src);
    if (!m) throw new Error(`Theme.hpp: no default for ${f}`);
    scalars[f] = Number(m[1].trim().replace(/f$/, ""));
  }
  return { colors, scalars };
}

/** The body of one `Theme xTheme() { … }` function in Theme.cpp. */
function presetBody(fn: string): string {
  const src = readFileSync(THEME_CPP, "utf8");
  const start = src.indexOf(`Theme ${fn}() {`);
  if (start < 0) throw new Error(`Theme.cpp: no ${fn}()`);
  const end = src.indexOf("\n}", start);
  return src.slice(start, end);
}

/** Resolve one preset's mirrored fields out of the C++ sources. */
function parsePreset(name: string): AxisTheme {
  const { colors, scalars } = parseHppDefaults();
  const body = presetBody(PRESET_FN[name]);
  const out: Record<string, unknown> = { name };
  for (const f of COLOR_FIELDS) {
    // The presets set colours through `setColor4(t.<field>, r, g, b, a);`.
    const m = new RegExp(`setColor4\\(t\\.${f},([^)]*)\\)`).exec(body);
    if (m) {
      const v = m[1].split(",").map((s) => Number(s.trim().replace(/f$/, "")));
      out[f] = [v[0], v[1], v[2], v[3]] as Rgba4;
    } else {
      out[f] = colors[f];
    }
  }
  for (const f of SCALAR_FIELDS) {
    const m = new RegExp(`t\\.${f}\\s*=\\s*([0-9.]+)f?\\s*;`).exec(body);
    out[f] = m ? Number(m[1]) : scalars[f];
  }
  return out as unknown as AxisTheme;
}

describe("the TS axis theme mirrors dc::Theme (ENC-1253, SPEC D4)", () => {
  it("reads the two C++ sources it claims to mirror", () => {
    expect(readFileSync(THEME_HPP, "utf8")).toContain("struct Theme {");
    expect(readFileSync(THEME_CPP, "utf8")).toContain("Theme darkTheme()");
  });

  for (const name of Object.keys(PRESET_FN)) {
    it(`${name}: every mirrored field equals the C++ preset`, () => {
      const cpp = parsePreset(name);
      const ts = AXIS_THEMES[name];
      expect(ts, `AXIS_THEMES has no ${name}`).toBeTruthy();
      for (const f of COLOR_FIELDS) {
        expect(ts[f], `${name}.${f}`).toEqual(cpp[f]);
      }
      for (const f of SCALAR_FIELDS) {
        expect(ts[f], `${name}.${f}`).toBeCloseTo(cpp[f] as number, 9);
      }
    });
  }

  it("mirrors every preset the C++ declares — a new one is a test failure", () => {
    const hpp = readFileSync(THEME_HPP, "utf8");
    const declared = [...hpp.matchAll(/^Theme (\w+Theme)\(\);$/gm)].map((m) => m[1]);
    expect(declared.length).toBeGreaterThan(0);
    expect(new Set(declared)).toEqual(new Set(Object.values(PRESET_FN)));
  });

  it("darkAxisTheme is the unmodified struct defaults", () => {
    const { colors, scalars } = parseHppDefaults();
    for (const f of COLOR_FIELDS) expect(darkAxisTheme[f]).toEqual(colors[f]);
    for (const f of SCALAR_FIELDS) expect(darkAxisTheme[f]).toBeCloseTo(scalars[f], 9);
  });

  it("axisThemeByName is case-insensitive and null for an unknown name", () => {
    expect(axisThemeByName("midnight")).toBe(midnightAxisTheme);
    expect(axisThemeByName("  DARK ")).toBe(darkAxisTheme);
    expect(axisThemeByName("solarized")).toBeNull();
  });

  it("rgba converts a float[4] to the command shape", () => {
    expect(rgba([0.1, 0.2, 0.3, 0.4])).toEqual({ r: 0.1, g: 0.2, b: 0.3, a: 0.4 });
  });

  it("gridRgba folds gridOpacity into alpha — ThemeManager's GridStyle rule", () => {
    // Pastel is the preset where the two differ: alpha 0.5 AND opacity 0.5.
    expect(gridRgba(AXIS_THEMES.Pastel).a).toBeCloseTo(0.25, 9);
    // Dark has opacity 1, so the alpha passes through untouched.
    expect(gridRgba(darkAxisTheme).a).toBeCloseTo(1, 9);
  });
});

describe("D11's contrast bands, measured on the theme the axis defaults to", () => {
  // The two backgrounds the furniture is actually seen against: the gutters,
  // which are the render target's own clear (black — DawnSceneRenderer clears
  // 0,0,0,1 and a pane only paints where it hasClearColor), and the showcase
  // panes' own clear colour, which is what a gridline crosses.
  const SHOWCASE_PANE: Rgba4 = [0.05, 0.05, 0.08, 1];

  it("contrastRatio matches WCAG's worked values", () => {
    // Black on white is the standard 21:1; a colour against itself is 1:1.
    expect(contrastRatio([0, 0, 0, 1], [1, 1, 1, 1])).toBeCloseTo(21, 6);
    expect(contrastRatio([0.3, 0.4, 0.5, 1], [0.3, 0.4, 0.5, 1])).toBeCloseTo(1, 9);
    // Order-independent.
    expect(contrastRatio([0, 0, 0, 1], [1, 1, 1, 1])).toBeCloseTo(
      contrastRatio([1, 1, 1, 1], [0, 0, 0, 1]),
      9,
    );
  });

  it("the default theme clears all four bands against the black gutters", () => {
    const v = checkD11AxisBands(defaultAxisTheme, CANVAS_CLEAR_BLACK);
    expect(v.failures).toEqual([]);
    expect(v.pass).toBe(true);
    expect(v.ratios.label).toBeGreaterThanOrEqual(D11_BANDS.textFloor);
    expect(v.ratios.tick).toBeGreaterThanOrEqual(D11_BANDS.nonTextFloor);
  });

  it("its gridline recedes over the showcase pane, and is still visible", () => {
    const v = checkD11AxisBands(defaultAxisTheme, SHOWCASE_PANE);
    expect(v.ratios.grid).toBeLessThanOrEqual(D11_BANDS.gridCeiling);
    expect(v.ratios.gridDelta255).toBeGreaterThanOrEqual(D11_BANDS.gridDeltaFloor);
    expect(v.ratios.grid).toBeLessThan(v.ratios.tick); // D11's ordering invariant
    expect(v.pass).toBe(true);
  });

  it("midnightTheme's ticks FAIL the non-text floor — why it is not the default", () => {
    // This is the measurement behind `defaultAxisTheme`'s doc comment, and it is
    // asserted rather than asserted-in-prose: SPEC §1.2 recommends midnight for
    // its gridline, which is true, and its tick colour is under D11's 3:1 floor
    // on the surface it is drawn against.
    const v = checkD11AxisBands(midnightAxisTheme, CANVAS_CLEAR_BLACK);
    expect(v.pass).toBe(false);
    expect(v.ratios.tick).toBeLessThan(D11_BANDS.nonTextFloor);
    expect(v.failures.join(" ")).toContain("tickColor");
    // Its gridline is fine — the failure is specific, not a blanket verdict.
    expect(v.ratios.grid).toBeLessThanOrEqual(D11_BANDS.gridCeiling);
  });
});
