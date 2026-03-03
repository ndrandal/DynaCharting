# D78: Modern Chart Styling

Expand the styling system to produce modern, visually appealing charts. Four areas: curated theme palettes, post-process effects, pane styling, and grid/axis polish.

---

## 1. Curated Theme Palettes (Theme.hpp / Theme.cpp)

### 1a. Extend Theme struct with new fields

Add to `Theme` in `core/include/dc/style/Theme.hpp`:

```cpp
// Overlay colors: expand from 4 to 8 slots
float overlayColors[8][4];  // (change from [4][4])

// Grid styling
float gridDashLength{0.0f};    // 0 = solid
float gridGapLength{0.0f};
float gridOpacity{1.0f};       // multiplied into gridColor alpha

// Pane borders
float paneBorderColor[4] = {0.3f, 0.3f, 0.35f, 1.0f};
float paneBorderWidth{0.0f};   // 0 = no border (pixels)

// Pane separator lines
float separatorColor[4] = {0.25f, 0.25f, 0.3f, 1.0f};
float separatorWidth{0.0f};    // 0 = no separator (pixels)
```

### 1b. Add 4 new theme presets in Theme.cpp

- **`midnightTheme()`** — Deep navy background (#0a0e1a), teal/blue-green accents, subtle grey grid
- **`neonTheme()`** — Near-black background, electric green/magenta candles, vibrant cyan/orange overlays, visible grid glow
- **`pastelTheme()`** — Soft warm cream background, muted sage green / soft rose candles, gentle overlay palette
- **`bloombergTheme()`** — Classic terminal black, amber/white candles, orange/blue overlays, medium grid with dash pattern

Each preset sets all fields (including the new grid/border/separator fields).

### 1c. Update ThemeManager constructor

Register all 6 presets (Dark, Light, Midnight, Neon, Pastel, Bloomberg) in the constructor.

### 1d. Update interpolate() and generateThemeCommands()

- `interpolate()`: lerp the 4 new overlay color slots + new float fields
- `generateThemeCommands()`: emit grid dash/opacity and separator style when targets are provided

### 1e. Extend ThemeTarget

Add to `ThemeTarget`:
```cpp
std::vector<Id> separatorDrawItemIds;  // for separator style commands
```

### 1f. Update SceneDocument support

Add grid style fields to `DocDrawItem` so they can be expressed in JSON:
- `gridDashLength`, `gridGapLength` — already covered by existing `dashLength`/`gapLength` on DrawItem
- No new DocDrawItem fields needed (dash/gap already in the document model)

---

## 2. Built-in Post-Process Effects (PostProcessPass)

### 2a. Add `BuiltinEffects.hpp` / `BuiltinEffects.cpp`

New files: `core/include/dc/gl/BuiltinEffects.hpp`, `core/src/gl/BuiltinEffects.cpp`

Provide factory functions that create pre-configured PostProcessPass instances:

```cpp
namespace dc {
  // Returns fragment shader source for each effect
  const char* vignetteFragmentSrc();
  const char* bloomBrightPassFragSrc();
  const char* bloomBlurFragSrc();
  const char* bloomCompositeFragSrc();

  // Convenience: add a vignette pass to a stack
  void addVignettePass(PostProcessStack& stack,
                       float strength = 0.3f, float radius = 0.8f);

  // Convenience: add a bloom pass chain (bright extract → blur → composite)
  void addBloomPasses(PostProcessStack& stack,
                      float threshold = 0.8f, float intensity = 0.5f);
}
```

### 2b. Vignette shader

Darkens edges smoothly toward center. Uniforms: `u_strength`, `u_radius`.

### 2c. Bloom shader chain

3-pass: bright extract (threshold), Gaussian blur, additive composite. Uniforms: `u_threshold`, `u_intensity`, `u_direction` (for separable blur).

---

## 3. Pane Styling (Renderer)

### 3a. Add Renderer pane border/separator support

New method in Renderer:
```cpp
void drawPaneBorder(const Pane& pane, int viewW, int viewH,
                    const float color[4], float widthPx);
void drawPaneSeparators(const Scene& scene, int viewW, int viewH,
                        const float color[4], float widthPx);
```

These use a simple GL line/quad draw (reuse pos2 shader) to render:
- **Border**: 4-edge rectangle at pane clip boundary
- **Separator**: horizontal line between consecutive panes

### 3b. Add style config to Renderer

```cpp
struct RenderStyle {
  float paneBorderColor[4] = {0,0,0,0};
  float paneBorderWidth{0.0f};
  float separatorColor[4] = {0,0,0,0};
  float separatorWidth{0.0f};
};
void setRenderStyle(const RenderStyle& style);
```

The render() method checks these and draws borders/separators after each pane's content (but before the next pane's scissor).

### 3c. Wire theme → render style

`generateThemeCommands()` doesn't directly set render style (it's not a command target). Instead, the caller reads theme fields and calls `renderer.setRenderStyle(...)`. Document this pattern.

---

## 4. Grid & Axis Polish (AxisRecipe)

### 4a. Add AxisRecipeConfig fields

```cpp
float gridDashLength{0.0f};  // pixels, 0 = solid
float gridGapLength{0.0f};   // pixels
float gridOpacity{1.0f};     // 0-1, multiplied into gridColor alpha
```

### 4b. Update AxisRecipe::build()

When creating grid DrawItems, pass through dash/gap/opacity:
- Set `dashLength` and `gapLength` on grid DrawItems via `setDrawItemStyle`
- Multiply `gridOpacity` into gridColor alpha channel

### 4c. Wire from Theme

When building an AxisRecipe from a Theme, copy grid style fields:
```cpp
axisCfg.gridDashLength = theme.gridDashLength;
axisCfg.gridGapLength  = theme.gridGapLength;
axisCfg.gridOpacity    = theme.gridOpacity;
```

---

## 5. Tests

### 5a. `d78_1_theme_presets.cpp` (unit test, no GL)
- T1: Verify all 6 presets have distinct names and non-zero colors
- T2: Verify overlay colors array is 8-deep
- T3: Verify new grid/border/separator fields have sensible defaults
- T4: ThemeManager registers all 6, `registeredThemes()` returns 6 names
- T5: Interpolate between themes with new fields

### 5b. `d78_2_builtin_effects.cpp` (GL test, OSMesa)
- T1: Create vignette pass, apply to solid-color texture, verify center is brighter than edges
- T2: Create bloom passes, apply to scene with a bright spot, verify glow
- T3: Enable/disable passes in stack

### 5c. `d78_3_pane_borders.cpp` (GL test, OSMesa)
- T1: Render a 2-pane scene with borders, verify border pixels are non-background color
- T2: Render with separators, verify separator line exists at pane boundary
- T3: Zero-width border/separator produces no visual artifacts

### 5d. `d78_4_grid_style.cpp` (unit test, no GL)
- T1: AxisRecipe build with dash/gap produces setDrawItemStyle commands with dashLength/gapLength
- T2: Grid opacity modifies alpha channel of grid color
- T3: Default (0/0/1.0) produces solid grid lines

---

## 6. Demo: `d78_showcase.cpp`

Gallery-style demo rendering 4 images:
1. **Midnight** theme — multi-pane candle chart with grid, borders, separators
2. **Neon** theme — same data with neon colors + bloom post-process
3. **Pastel** theme — same data with pastel colors + vignette
4. **Bloomberg** theme — classic terminal look with dashed grid

Each renders to a separate PPM file.

---

## 7. File Summary

| Action | File |
|--------|------|
| Edit | `core/include/dc/style/Theme.hpp` — new fields + 4 preset declarations |
| Edit | `core/src/style/Theme.cpp` — 4 new presets, updated interpolate/generateThemeCommands |
| Edit | `core/src/style/ThemeManager.cpp` — register 6 presets |
| New | `core/include/dc/gl/BuiltinEffects.hpp` — effect factory declarations |
| New | `core/src/gl/BuiltinEffects.cpp` — shader sources + factory functions |
| Edit | `core/include/dc/gl/Renderer.hpp` — RenderStyle, border/separator methods |
| Edit | `core/src/gl/Renderer.cpp` — border/separator drawing in render loop |
| Edit | `core/include/dc/recipe/AxisRecipe.hpp` — new config fields |
| Edit | `core/src/recipe/AxisRecipe.cpp` — pass dash/gap/opacity to grid DrawItems |
| Edit | `core/CMakeLists.txt` — add new source files |
| New | `core/tests/d78_1_theme_presets.cpp` |
| New | `core/tests/d78_2_builtin_effects.cpp` |
| New | `core/tests/d78_3_pane_borders.cpp` |
| New | `core/tests/d78_4_grid_style.cpp` |
| New | `core/demos/d78_showcase.cpp` |

## 8. Implementation Order

1. Theme struct expansion + presets (foundation, no deps)
2. ThemeManager + interpolate updates (depends on 1)
3. AxisRecipe grid polish (depends on 1, small delta)
4. Renderer pane borders/separators (independent)
5. BuiltinEffects (independent, uses existing PostProcessPass)
6. Tests (after their respective implementations)
7. Demo (after everything)
8. Build + verify all tests pass
