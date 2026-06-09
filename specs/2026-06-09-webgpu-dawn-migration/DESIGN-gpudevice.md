# DESIGN — `GpuDevice` abstraction (ENC-481, P1.1)

Status: interface + design only. No behaviour change. Renderer.cpp still uses
raw GL and is untouched. ENC-482 introduces `GlDevice` and migrates the
dispatcher; ENC-49x introduces `DawnDevice`.

## Why

The renderer dispatch in `core/src/gl/Renderer.cpp` (~1.6k LOC) is written
directly against OpenGL 3.3: `glViewport`/`glClear`/`glScissor`, `glBlendFunc*`,
`glStencil*`/`glColorMask`, FBO setup for picking, a shared VAO, per-draw
`glVertexAttribPointer`/`glUniform*`/`glDraw*`, plus buffer binding via
`GpuBufferManager` and texture binding via `TextureManager`.

To migrate to WebGPU/Dawn we need a single device seam that:

- hides the concrete graphics API behind opaque handles + enums + POD
  descriptors (no `GLuint`/`WGPU*`/glad/dawn types leak across the boundary),
- lives in the **pure `dc` library** (no GL link), so logic tests still build,
- is shaped for WebGPU's **immutable render-pipeline** model rather than GL's
  mutable global state.

Files:
- `core/include/dc/render/GpuDevice.hpp` — the abstraction (handles, enums,
  descriptors, the `GpuDevice` interface).
- `core/include/dc/render/IRendererBackend.hpp` — finalized device-agnostic
  per-pipeline backend seam (`init(GpuDevice&)` replaces `initGL()`).
- `core/src/render/GpuDeviceCompileCheck.cpp` — tiny TU so the headers are
  actually compiled into `dc` (with `static_assert`s enforcing the POD contract).

## Operation set

Derived 1:1 from what the Renderer dispatcher actually does:

| Category            | GpuDevice operations                                              | Covers in Renderer.cpp |
|---------------------|------------------------------------------------------------------|------------------------|
| Lifecycle           | `init()`                                                         | `Renderer::init` (VAO, point-size, shader build) |
| Buffers             | `createBuffer`, `updateBuffer`, `writeBufferRange`, `destroyBuffer` | `GpuBufferManager` (setCpuData / reserve / writeRange / uploadDirty) |
| Textures            | `createTexture`, `updateTexture`, `destroyTexture`              | `uploadAtlasIfDirty`, `TextureManager`, pick RBO |
| Pipelines           | `createPipeline`, `destroyPipeline`, `bindPipeline`            | the 12 `ShaderProgram`s + `*.use()` |
| Bind groups         | `createBindGroup`, `destroyBindGroup`                          | per-draw attrib pointers + uniforms + texture units |
| Render pass         | `beginRenderPass`, `endRenderPass`, `setViewport`, `setScissorRect` | viewport/clear at top of `render`/`renderPick`; per-pane `glScissor`; pick FBO bind |
| Per-draw state      | `setBlendMode`, `setClipState`                                | `applyBlendMode`; the D29.2 two-pass stencil block |
| Draw                | `draw`, `drawInstanced`                                        | `glDrawArrays`/`glDrawElements` + `glDrawArraysInstanced` |
| Readback            | `readPixel`                                                   | `glReadPixels` in `renderPick` |

## Handle / descriptor model

**Opaque handles** are trivially-copyable structs wrapping one `uint32_t id`
(`id == 0` == null): `BufferHandle`, `TextureHandle`, `PipelineHandle`,
`BindGroupHandle`, `RenderTargetHandle`. The caller never inspects the id; the
concrete device maps it to a `GLuint` / `WGPUBuffer` / slot index. Enforced by
`static_assert(is_trivially_copyable && sizeof == sizeof(u32))` in the
compile-check TU.

**Enums** express fixed-function state API-neutrally: `DeviceBlendMode`
(Normal/Additive/Multiply/Screen), `ClipMode` (None/WriteMask/UseMask),
`PrimitiveTopology`, `IndexFormat`, `VertexComponentType`, `TextureFormat`,
`TextureFilter`.

**Descriptor structs** are plain data with borrowed pointers (valid for the
call): `VertexAttribute`, `VertexBufferLayout`, `PipelineDesc`, `UniformBinding`,
`BindGroupDesc`, `DrawParams`, `DrawInstancedParams`, `RenderPassDesc`,
`ScissorRect`, `TextureDesc`. Buffers feed a pipeline via `VertexBufferLayout`
(stride + `stepInstance` + attributes) — this captures the split-attribute
instanced layouts (candle `a_c0`/`a_c1`, text `a_g0`/`a_g1`).

### Streaming buffer semantics

`writeBufferRange(buf, offset, data, bytes)` mirrors
`GpuBufferManager::writeRange`'s `[offset, offset+bytes)` dirty-range model — the
live-tick tail-append path that coalesces to a single `glBufferSubData` (GL) /
`queueWriteBuffer` (WebGPU). `updateBuffer` is the full-replace path
(`glBufferData` / recreate-if-grown), and `createBuffer` takes an explicit
capacity so growth is callable-controlled, exactly like `reserve`.

## Why immutable-PSO shaped

In GL, blend mode, stencil/clip state, color-write mask, shader program and
primitive topology are independent **global** switches flipped per draw
(`applyBlendMode`, the `glStencilFunc/Op/glColorMask` block, `program.use()`,
the `mode` arg to `glDraw*`). In WebGPU **all of that is baked into an immutable
`RenderPipeline`** chosen *before* the draw, and the per-draw reference value
(`SetStencilReference`) + bind groups are the only mutable inputs.

So `PipelineDesc` carries `blend`, `clip`, `topology` and the vertex layout as
**pipeline-creation** fields, not runtime setters. The intended steady state is:
*select* the pipeline permutation, set bind groups, draw — never mutate pipeline
state mid-pass.

### Transition affordance + ENC-493 direction

`v1` still exposes `setBlendMode` / `setClipState` / `setViewport` /
`setScissorRect` so the **GlDevice** (ENC-482) can be a thin, behaviour-
preserving wrapper over today's mutable-global code without first building a
pipeline cache. On **DawnDevice** these are implemented by *selecting the
matching immutable pipeline variant* (and `SetStencilReference(1)` for clip), not
by mutating global state.

**ENC-493 (permutation cache):** materialise one `PipelineHandle` per
`(pipelineId, DeviceBlendMode, ClipMode, PrimitiveTopology)` tuple. The
dispatcher then keys into the cache and `bindPipeline`s the right variant, and
the mutable `setBlendMode`/`setClipState` setters can be folded into that key and
removed. `WriteMask`/`UseMask` become two pipeline variants of the same shader.

## GL ↔ WebGPU mapping table

| GpuDevice                          | OpenGL 3.3 (today)                                                          | WebGPU / Dawn |
|------------------------------------|-----------------------------------------------------------------------------|---------------|
| `init`                             | current ctx, `glGenVertexArrays`, `glEnable(PROGRAM_POINT_SIZE)`           | acquire `Device`/`Queue`, configure surface |
| `createBuffer`                     | `glGenBuffers` + `glBufferData(cap, STREAM_DRAW)`                          | `CreateBuffer({size, Vertex\|Index\|CopyDst})` |
| `updateBuffer`                     | `glBufferData` (realloc) / full `glBufferSubData`                          | recreate-if-grown + `queueWriteBuffer(0,…)` |
| `writeBufferRange`                 | `glBufferSubData(offset, bytes, data)`                                     | `queueWriteBuffer(offset, data, bytes)` |
| `createTexture`                    | `glGenTextures` + `glTexImage2D` (R8/RGBA8) + filter/wrap params           | `CreateTexture` + `queueWriteTexture` + sampler |
| `updateTexture`                    | `glBindTexture` + `glTexImage2D` (atlas re-upload)                         | `queueWriteTexture` |
| `createPipeline`                   | build `ShaderProgram`; cache blend/stencil/topology                       | `CreateRenderPipeline` (immutable) |
| `createBindGroup`                  | record VBO/IBO + uniforms + texture units                                 | `CreateBindGroup` (uniform buf + views + samplers) |
| `beginRenderPass`                  | `glBindFramebuffer` + `glViewport` + (`glClearColor`+`glClear`)           | `BeginRenderPass` with loadOp=Clear/clearValue |
| `endRenderPass`                    | `glFlush`, unbind VAO/FBO                                                  | `pass.End()` + `queue.Submit(Finish())` |
| `setViewport`                      | `glViewport(0,0,w,h)`                                                      | `SetViewport` |
| `setScissorRect`                   | `glScissor` (with `GL_SCISSOR_TEST` enabled)                              | `SetScissorRect` |
| `setBlendMode`                     | `applyBlendMode` → `glBlendFunc`/`glBlendFuncSeparate` (4 modes)          | select pipeline variant (baked `BlendState`) |
| `setClipState` (None/Write/Use)    | `glEnable/Disable(STENCIL_TEST)` + `glStencilFunc/Op` + `glColorMask`     | select pipeline variant + `SetStencilReference(1)` |
| `bindPipeline`                     | `program.use()` + re-apply cached fixed-function state                    | `SetPipeline` |
| `draw`                             | `glDrawArrays` / `glDrawElements`                                          | `SetBindGroup`/`SetVertexBuffer`/`Draw`/`DrawIndexed` |
| `drawInstanced`                    | `glVertexAttribDivisor(…,1)` + `glDrawArraysInstanced`                     | `Draw(vertsPerInstance, instanceCount)` |
| `readPixel`                        | `glReadPixels(x,y,1,1,RGBA,UBYTE)`                                         | `copyTextureToBuffer` + `mapAsync` (block on native) |

### Blend mode detail

| `DeviceBlendMode` | GL (`applyBlendMode`)                                                          |
|-------------------|-------------------------------------------------------------------------------|
| `Normal`          | `glBlendFunc(SRC_ALPHA, ONE_MINUS_SRC_ALPHA)`                                  |
| `Additive`        | `glBlendFunc(SRC_ALPHA, ONE)`                                                  |
| `Multiply`        | `glBlendFuncSeparate(DST_COLOR, ZERO, ONE, ONE_MINUS_SRC_ALPHA)`               |
| `Screen`          | `glBlendFuncSeparate(ONE, ONE_MINUS_SRC_COLOR, ONE, ONE_MINUS_SRC_ALPHA)`      |

### Clip / stencil detail (D29.2 two-pass)

| `ClipMode`  | GL                                                                                        | WebGPU |
|-------------|-------------------------------------------------------------------------------------------|--------|
| `None`      | `glDisable(STENCIL_TEST)`; `glColorMask(1,1,1,1)`                                          | default pipeline, no stencil |
| `WriteMask` | `glEnable(STENCIL_TEST)`; `func(ALWAYS,1,0xFF)`; `op(KEEP,KEEP,REPLACE)`; `colorMask(0,0,0,0)` | stencil-write pipeline variant, `writeMask=none` |
| `UseMask`   | `glEnable(STENCIL_TEST)`; `func(EQUAL,1,0xFF)`; `op(KEEP,KEEP,KEEP)`; `colorMask(1,1,1,1)`     | stencil-test pipeline variant, `SetStencilReference(1)` |

After a `WriteMask` (clip-source) draw the dispatcher restores the color mask to
`(1,1,1,1)`; on Dawn that is implicit (the next pipeline's `writeMask`).

## Compatibility / scope notes

- `IRendererBackend` had no implementers (it was a drafted seam), so renaming
  `initGL()` → `init(GpuDevice&)` and routing `renderDrawItem` through a
  `GpuDevice` breaks no compilation. `Renderer.cpp` does not reference it.
- A `TODO(ENC-482)` marks where the concrete backends will port each `draw*`
  helper from `Renderer.cpp` onto the device.
- `dc` and its logic tests (incl. `dc_d1_1_smoke`) build unchanged; `dc_gl` is
  skipped in CI here (no OSMesa/GLFW), as expected.
