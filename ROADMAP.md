# DynaCharting Feature Roadmap

Engine-level primitives that expand what applications can build without touching core.

---

## Data Manipulation

### Filtering
Render a subset of a buffer by index list without copying data. An index buffer that says "draw records 3, 7, 12, 45" from an existing vertex buffer. Enables table row filtering, category toggling, and search-driven visibility.

### Sorting
Render a buffer in a different order via indirection. An order buffer or sort key that controls draw sequence without rearranging the underlying data. Enables column sorting in tables, ranked views, and z-order control.

### Cross-Pane Data Binding
Selection in pane A drives what's visible in pane B. Engine-level primitive that connects a SelectionState to a filter on another geometry. The common use case: selecting rows in a table filters a chart to show only those rows. Supports multi-select.

---

## Interaction

### Range / Box Selection
Select all points within a rectangular region, not just single-click nearest-point picking. Drag to define a selection box, engine returns all indices within bounds. Enables bulk selection, zoom-to-region, and lasso-style workflows.

### Drag & Drop
Reorder elements, move items between panes, drag data from one visualization to another. Engine provides hit testing + drag state tracking, application decides what the drop means.

### Cursor Hints
Engine tells the host what cursor to show based on hover state — pointer over clickable elements, crosshair over data areas, resize handles on dividers, grab hand during pan. Currently the host has no signal from the engine about cursor intent.

---

## Visual

### Animation / Transitions
Interpolate transforms, colors, or vertex data between states over time. Engine-level tweening so that viewport changes, data updates, and visibility toggles animate smoothly instead of snapping. Configurable easing and duration.

### Gradient Fills
Per-vertex color or color ramp uniforms — not just flat color per draw item. Enables heatmaps, area charts with gradient fills, and color-mapped scatter plots. Could be a new vertex format with per-vertex RGBA or a uniform-based color stop system.

### Texture / Image Rendering
Render images on quads — logos, heatmap tiles, custom icons, rasterized content. A textured-quad pipeline that takes a texture ID and UV coordinates.

### Dashed / Dotted Lines
Line style beyond solid. Dash pattern as a per-draw-item property (dash length, gap length, offset). Common for reference lines, projections, grid subdivisions, and drawing tool styles.

### Rounded Rectangles
Corner radius on instancedRect. Needed for buttons, cards, tooltip backgrounds, and any UI-like overlay. Could be an additional field on the rect format or a separate pipeline.

### Clipping Masks
Clip geometry to an arbitrary shape, not just rectangular scissor. Stencil-buffer-based masking where one geometry defines the visible region for others. Enables scroll containers, circular viewports, and shaped overlays.

### Blend Modes
Per-draw-item blend mode — multiply, screen, additive, not just standard alpha-over. Enables overlay effects, glow, and non-standard compositing.

---

## Data Flow

### Event / Callback System
Engine notifies the host when selection changes, viewport changes, geometry is hovered, or data updates — rather than the host polling every frame. Observer pattern or callback registration on engine state.

### Computed / Derived Buffers
Engine-level "this buffer is a filtered view of that buffer" without the application manually rebuilding vertex data. Declarative buffer derivation: source buffer + filter indices = derived buffer, automatically kept in sync when source changes.

---

## Layout

### Anchoring
Position elements relative to pane edges — legend always top-right, tooltip clamped to bounds, axis labels pinned to margins. Currently everything is positioned in raw clip-space math by the application.

### Virtualization
For large datasets (tables with 100k rows, long scrollable lists), only emit geometry for the visible window. Engine tracks the visible range and requests data for just that slice, rather than loading everything into GPU buffers.

---

## Accessibility

### Keyboard Navigation
Tab between interactive elements, arrow keys within groups, Enter to activate. Engine tracks focus state and provides a navigation order. Enables keyboard-only operation of charts and UI elements.

### Semantic Annotations
Attach metadata to draw items — role, label, value — so the host can surface it to screen readers and assistive technology. The engine doesn't render this metadata, but exposes it for the host to bridge to platform accessibility APIs.

---

## Performance

### GPU Picking
Color-ID render pass for pixel-perfect hit testing. Each draw item renders with a unique color to an offscreen framebuffer, then readPixels at the cursor position identifies exactly what was hit. Replaces CPU-side buffer scanning for complex scenes.
