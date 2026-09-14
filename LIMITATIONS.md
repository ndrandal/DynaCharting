# DynaCharting — Limitations

What this engine **cannot do**, what it does **differently than you expect**, and what is
**built but unreachable**. Read this before designing around a constraint, and before filing a
bug for behaviour that is listed here as deliberate.

- **Active** limitations are below, newest concern first.
- **Corrections** (§C) are beliefs that were confidently held and *wrong* — they cost someone
  time once, so they are recorded rather than quietly dropped.
- **Retired** (§R) are entries that stopped being true, kept with the commit that killed them.

Every active entry carries a `Verified at` commit and a **`Re-check`** command you can paste.
If a re-check disagrees with the entry, the entry is wrong — fix it in the same PR as whatever
you were doing. See [§H — How this file stays true](#h--how-this-file-stays-true); it exists
because the previous attempt at this document did not.

**Verified in full at `6684a00` on 2026-09-14.** Every command below was executed at that
commit and produced the output shown.

---

## DC-L01 — A green default `ctest` says nothing about the renderer 🔴

**Claim.** `cmake -B build && ctest --test-dir build` runs **189** tests and builds **no
renderer at all**. `dc_gpu`, `dc_json_host`, all four headless demo servers and **43 render
tests** are excluded at *configure* time by `DC_FETCH_DAWN` (default `OFF`,
`core/CMakeLists.txt:165`). They are not "skipped" — they never enter `CTestTestfile.cmake`,
so nothing reports them as missing.

**Why it bites.** "189/189 passed" is the most reassuring possible output and it is compatible
with the renderer being completely broken. Every pixel-level guarantee in this engine lives in
the 43 tests that did not run.

**Re-check.**
```bash
grep -cE '^\s*add_test\(' core/CMakeLists.txt            # 232  — all tests that exist
grep -c '^add_test('  build/core/CTestTestfile.cmake     # 189  — all tests you just ran
grep -n 'DC_FETCH_DAWN:BOOL' build/CMakeCache.txt        # OFF
```
The 43-test gap is the single `if (DC_HAS_DAWN)` block at `core/CMakeLists.txt:1899-2447`.
Target-level gap: **51** targets (`dc_gpu`, `dc_glfw_system`, `dc_json_host`,
`dc_dawn_window_demo`, 4 servers, 43 test executables) across five guarded ranges — 274-351,
361-366, 374-383, 391-409, 1899-2447.

**Working around it.** Build Dawn once (~55-60 min, then incremental) and keep the build dir:
```bash
cmake -B build-dawn -G Ninja -DDC_BUILD_TESTS=ON -DDC_FETCH_DAWN=ON
cmake --build build-dawn -j$(nproc)
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json ctest --test-dir build-dawn -j$(nproc)   # 231/231
```
Two sessions did exactly this on 2026-09-14 (ENC-992, ENC-993) and both got 231/231 on
lavapipe, so it is feasible, not theoretical. `-DDC_DAWN_WINDOWED=ON` is a *second*,
independent gate for `dc_dawn_window_demo` — `-DDC_FETCH_DAWN=ON` alone gets 50 of the 51.

**Ticket.** [ENC-994](https://linear.app/encultured/issue/ENC-994) (Backlog) covers two of the
43 (`d28_1_dawn_lineaa`, `d29_1_dawn_blend`). The other 41 have no owner.

**The absolute numbers move; the 43-test gap does not.** ENC-984 added one always-built logic
test, taking the pair from 188/231 to **189/232** — the gap is still exactly the `DC_HAS_DAWN`
block. Read the *difference*, not the left-hand number: a change that grows the registered count
tells you nothing about the renderer either.

**Verified at** `937538a`, 2026-09-14 — counted statically from `core/CMakeLists.txt` and
empirically from a real default configure in the ENC-984 worktree; both give 189 of 232, gap 43.

---

## DC-L02 — `dc_json_host --png` captures can never contain text 🔴

**Claim.** A one-shot PNG is the chart **minus every title, axis label and legend**. This is by
design and is not an ordering bug: moving the `writeTextOverlay()` call above the `--png` early
return does not fix it.

**Two independent mechanisms**, either of which alone is sufficient:

1. **`writeTextOverlay` rasterizes nothing.** It serializes a `TEXT` protocol record to stdout
   for the *browser client* to composite as DOM (`core/src/host/JsonHost.cpp:64`, writing at
   `:92-95`). Zero pixels are produced, so there is nothing for a PNG to contain.
2. **`textSDF@1` is not registered in this host.** `JsonHost` builds its backend with no glyph
   atlas — `core/src/host/JsonHost.cpp:340` calls `std::make_unique<DawnHostBackend>()` against
   the `atlas = nullptr` default at `:146-147`. The text backend is only created when an atlas
   exists (`core/src/gpu/DawnSceneRenderer.cpp:96-97`) and only registered when it was created
   (`:154-155`, the one conditional among ten pipelines). A draw item whose pipeline has no
   registered backend is **silently skipped** — no warning, no error, no `else`
   (`core/src/gpu/DawnSceneRenderer.cpp:391-398`).

So even a chart that authored real `textSDF@1` geometry renders textless here.

**Re-check.**
```bash
sed -n '402,430p' core/src/host/JsonHost.cpp                  # --png returns before writeTextOverlay
sed -n '146,147p'  core/src/host/JsonHost.cpp                 # DawnHostBackend(atlas = nullptr, ...)
sed -n '154,155p'  core/src/gpu/DawnSceneRenderer.cpp         # textSDF@1 registered only if textSdf_
sed -n '391,398p'  core/src/gpu/DawnSceneRenderer.cpp         # unregistered pipeline => silently skipped
dc_json_host --help                                           # states it outright
```
The same design intent, stated where it originated:
`core/demos/live_server.cpp:579-580` — *"Text labels are suppressed below (drawn by the HTML
overlay), so no GlyphAtlas is wired into the renderer."*

**Do not read a textless capture as a rendering bug.** 76 of the 277 authoring-trial writeups
record it as one (`grep -rl 'Text labels invisible in PNG capture' docs/trials/*.md | wc -l`
→ `76`). That is the cost of this not having been written down.

**Status.** Deliberate. The ENC-992 session demonstrated it with pixels — charts with and
without their `textOverlay` came out byte-identical — but **that evidence is not reproducible
from the repo**: no test or script was committed, and the measurement was taken at a reduced
320×240 because of DC-L03. Treat the source mechanism above as the proof; treat the pixel
counts as a session observation recorded in `6684a00`'s commit message.

**Ticket.** [ENC-992](https://linear.app/encultured/issue/ENC-992) — documented, closed as
intended behaviour. See also `CHART_AUTHORING.md` §"Text and static captures (ENC-992)".

**Verified at** `6684a00`, 2026-09-14 — both mechanisms traced in source; `6684a00` itself is
documentation-only (`git show --stat 6684a00` → 4 files, all docs/comments), so nothing about
the behaviour changed when it was written down.

---

## DC-L03 — `--png` does one GPU round trip per pixel ✅ *(RETIRED — fixed by ENC-1093)*

**Retired 2026-09-14**, the day it was written, by ENC-1093 (`7169457`). Kept in place rather
than deleted, and kept here rather than moved to §R, because §R holds the *previous* document's
`L*` ids — this is the first `DC-L*` entry to fall, and the point of §H is that a falsification
is visible where the claim was.

**What the claim said.** `DawnHostBackend::render` read the framebuffer one pixel at a time, so
a 900×600 capture cost 540,000 blocking round trips and exceeded 300 seconds. That was true and
the mechanism was correctly traced: each `readPixel` copied the *entire* texture to extract four
bytes, as `DawnDevice.cpp:1653` admits in its own comment.

**What fixed it.** Exactly the remedy this entry named — `JsonHost.cpp` now calls
`DawnDevice::readFramebufferRGBA()` (ENC-503), one copy and one map, which every other readback
caller already used. `core/demos/dawn_server_util.hpp` carried the same loop and got the same
change. 56 insertions, 9 deletions.

**Measured, which this entry could not be.** 900×600, `charts/072-polar-rose.json`:

| adapter | before | after | speedup |
|---|---:|---:|---:|
| NVIDIA RTX 3070 Ti (Mesa NVK) | 302.0 s | 1.399 s | **216×** |
| llvmpipe / lavapipe | 516.8 s | 1.397 s | **370×** |

Output proven unchanged: five 900×600 charts sha256-identical as PNGs **and** as raw FRME
frames (2.16 MB of raw RGBA each, no encoder in between, so row order, alpha and 256-byte row
padding are proven directly). `dc_gallery`'s five PPM cards identical too, 207.9 s → 1.42 s.

**Two corrections this retirement carries** — both bear on entries that remain:

- **This box is not lavapipe.** Dawn's default adapter here is a real NVIDIA RTX 3070 Ti via
  Mesa NVK. The ">300 s" figure above was a lavapipe measurement presented as the general case;
  hardware was 302 s, so the conclusion held, but *this entry's* environment label was wrong.
  DC-L01 is not affected: it pins the adapter explicitly with
  `VK_ICD_FILENAMES=.../lvp_icd.x86_64.json` and says "on lavapipe" in words — it was right, and
  this correction is only about the figure in the paragraph above.
- **The Dawn suite is 229/231 on hardware, not 231/231.** `dc_enc619_dawn_fft` and
  `dc_enc619_dawn_marching_squares` fail identically on the *unmodified* tree
  (`maxRelErr=4.485e-03` against a `1e-3` tolerance) and pass under lavapipe. Pre-existing, in
  GPU *compute*, unrelated to readback. DC-L01 already scopes its 231/231 to lavapipe correctly;
  what is new here is that the *hardware* number was never measured until now, and it is 229.

**Re-check.**
```bash
grep -n 'readFramebufferRGBA(' core/src/host/JsonHost.cpp   # a real CALL (~:182) => fixed
grep -c 'for (int y' core/src/host/JsonHost.cpp             # the per-pixel loop survives only as fallback
```

**Ticket.** [ENC-1093](https://linear.app/encultured/issue/ENC-1093) — merged `7169457`.

**Verified at** `7169457`, 2026-09-14 — retirement confirmed by the managing session against
merged main, independently of the implementing agent.

---

## DC-L04 — `setDrawItemGradient` renders nothing 🟠

**Claim.** `setDrawItemGradient` (linear or radial) is **accepted with `ok:true`** and has **no
effect on any rendered pixel**. The item draws in its flat base colour. This is true of the
native Dawn renderer, not merely of the WASM build.

**Where it dies.** The command parses (`core/src/commands/CommandProcessor.cpp:171` →
`:1275`) and the state is stored on the DrawItem (`core/include/dc/scene/Types.hpp:130-136`:
`gradientType`, `gradientAngle`, `gradientColor0/1`, `gradientCenter`, `gradientRadius`). It is
then faithfully round-tripped by the serializer, the reconciler and the scene document — and
read by **no renderer backend at all**. All 13 Dawn backends live under `core/src/gpu/`
(registered `core/src/gpu/DawnSceneRenderer.cpp:143-156`) and not one of them touches a
gradient field. `core/src/gl/` no longer exists; Dawn is the only renderer.

**Re-check.** This is the whole argument in one command:
```bash
grep -rl 'gradientType\|gradientAngle\|gradientColor\|gradientCenter\|gradientRadius' \
     core/src/gpu/ | wc -l        # 0   — no renderer backend reads gradient state
strings packages/dc-wasm/wasm/dc_engine_host.wasm | grep -ci gradientType   # 0
```
If the first command ever returns non-zero, a backend started consuming it — re-verify and
retire this entry.

**Workaround — use `triGradient@1`.** Per-vertex colour interpolation gives true linear and
radial gradients today. Beware the near-collision in names: `DawnTriGradientBackend` implements
the `triGradient@1` *pipeline* and is unrelated to `setDrawItemGradient`; its own WGSL comment
(`core/src/gpu/DawnTriGradientBackend.cpp:46`) notes the uniform colour is unused because
colour is per-vertex.

**One real consumer exists**, which is why the command was ever added:
`core/src/export/SvgExporter.cpp` (`buildGradientDef`, `:175,181,204,241,443`). Gradients are
genuine in SVG output and inert on the GPU.

**Ticket.** None. Either wire it in the Dawn backends or delete the command — but it should not
keep returning `ok:true` for a no-op.

**Verified at** `6684a00`, 2026-09-14 — command traced parse → storage → consumers; zero reads
under `core/src/gpu/`; zero `gradientType` strings in the committed wasm.

---

## DC-L05 — Raw `framebuffer()` bytes are bottom-up; every consumer must flip 🟠

**Claim.** The Dawn scene shaders negate clip-space Y in every backend, while
`DawnDevice::readFramebufferRGBA` is faithfully top-down. Net effect: an authored clip-Y-**up**
vertex lands in the **bottom** rows of `core.framebuffer()`. `putImageData` is also top-down, so
**a raw blit renders the scene upside down.**

**This is the residue of ENC-696, not a regression.** That ticket fixed the *symptom* at the one
place browser frames are painted (`packages/dc-wasm/src/EngineHost.ts:910-915`) and its comment
says plainly that the real fix — normalizing the shader Y-negation and the C++ readback — was
deferred because it needs the Dawn golden PNGs re-baselined and the wasm rebuilt. So the
convention is unchanged; only the two known consumers compensate.

**Consequence for new code.** There are exactly two consumers today and both flip:
`blitFramebuffer` (`EngineHost.ts:894`) and `captureThumbnail` (`EngineHost.ts:626` →
`thumbnail.ts:147`), which deliberately share the single `flipRowsRGBA` helper so results are
never double-flipped. **A third consumer that reads `core.framebuffer()` raw will render
inverted**, and the failure is silent on any vertically symmetric scene — which is how this
survived undetected until the only live demo stopped being an orientation-ambiguous line.

**Re-check.**
```bash
grep -rn 'core.framebuffer()' packages/dc-wasm/src --include='*.ts' | grep -v test
# every hit must be followed by a flip (flipRowsRGBA, or the fbH-1-y loop)
sed -n '921,940p' packages/dc-wasm/src/EngineHost.ts    # the deferral is stated in the comment
```

**Ticket.** None for the deep fix. [ENC-696](https://linear.app/encultured/issue/ENC-696)
(`d6b5acd`) fixed the blit only.

**Verified at** `937538a`, 2026-09-14 — re-run in the ENC-984 worktree (which edits
`EngineHost.ts`, shifting the `sed` range by +25): both real `core.framebuffer()` consumers
(`captureThumbnail`, `blitFramebuffer`) still copy-then-flip.

---

## DC-L06 — `applyControl` rejections are visible but still ignorable 🟠

**Claim.** A rejected control no longer vanishes — but `applyControl` still **does not throw**,
so a caller that ignores the return value gets a chart that draws nothing plus a console
warning. Author bugs still present as "nothing rendered".

**What ENC-701 (`181c305`) fixed.** Rejections now route through
`recordControlRejection` (`packages/dc-wasm/src/EngineHost.ts:227-244`) at all three call sites
(`:296`, `:539-541`, `:869-870`): they fire `onControlRejected` if supplied, else `console.warn`,
and are always recorded on `getLastErrors()`. Regression test:
`packages/dc-wasm/src/EngineHost.rejections.test.ts` (4 cases).

**Four residuals that keep this an active limitation:**

1. **Nothing throws.** `applyControl` returns `{ ok: true } | { ok: false; error: string }`
   (`EngineHost.ts:513-515`). Ignoring it is still the path of least resistance — and the
   first-party authoring helper does exactly that: `packages/dc-wasm/src/chart/text.ts:252`
   calls `target.applyControl(step.command)` and discards the result.
2. **`console.warn` is the default**, which is invisible in a headless runner, in CI, and in the
   corpus runner unless console is explicitly captured.
3. **The raw Embind surface is unchanged.** `core/wasm/dc_engine_host.cpp:565` returns a plain
   `DcControlResult`. Driving the module directly — as `validate-node.mjs` does — gets no
   warning at all.
4. **Before the module is ready, rejections are reported as success.** Buffered commands
   `return { ok: true }` unconditionally (`EngineHost.ts:533-536`) and only surface on drain. So
   `{ok:false}` is not a reliable signal *at call time*.

**Do this in product code.** Pass `onControlRejected` and fail loudly; count rejects and treat
`rejects > 0` as a failed render rather than a warning.

**Re-check.**
```bash
sed -n '227,244p' packages/dc-wasm/src/EngineHost.ts    # warn-by-default, no throw
sed -n '533,536p' packages/dc-wasm/src/EngineHost.ts    # pre-ready commands return {ok:true}
npx vitest run packages/dc-wasm/src/EngineHost.rejections.test.ts
```

**Ticket.** None for the residuals.

**Verified at** `937538a`, 2026-09-14 — all four residuals read in source and unmoved by
ENC-984 (its insertion is below both `sed` ranges); regression test run green as part of
`pnpm test` (18 files, 185 tests — ENC-984 adds `EngineHost.sceneDocument.test.ts`).

---

## DC-L07 — The raw Embind `pick` is not the pick API you want 🟡

**Claim.** On the raw `DcEngineHost` WASM object, `pick` takes **four** arguments in the order
**`(w, h, x, y)`** — width and height *first* — and returns a **`Promise`**, not an int, because
the module is built with `-sASYNCIFY=1`. Calling any other host method before that promise
settles hard-aborts the runtime with *"cannot have multiple async operations in flight at
once"*. `renderPick` is **not** exported.

**Use the TS wrapper instead.** `@repo/dc-wasm`'s `EngineHost` has had the ergonomic form since
ENC-506: `pick(x, y): PickResult` (`packages/dc-wasm/src/EngineHost.ts:641`) and
`async pickAsync(x, y)` (`:726`). Higher still, `attachInteraction()` (ENC-634/639/640) wires
pointer events straight to hover/selection/brush signals and is what you almost certainly want.

**The one gotcha that survives in the wrapper:** `pick(x, y)` returns the **last known** result
synchronously and kicks off an async refresh, so it returns `null` until the first pick
completes and is **one frame stale** thereafter. Fine for pointer-move polling; wrong for a
single authoritative hit test — use `pickAsync` there.

**Re-check.**
```bash
sed -n '429,435p' core/wasm/dc_engine_host.cpp                            # double pick(int w, int h, int x, int y)
grep -n 'function("pick"' core/wasm/dc_engine_host.cpp                    # the only pick export
strings packages/dc-wasm/wasm/dc_engine_host.wasm | grep -c renderPick    # 0
```

**Not reproducible under node**, and this matters: `pick` returns 0 there because there is no
WebGPU device (`ensureRenderer()` fails, `core/wasm/dc_engine_host.cpp:431`), not because the
pick path is unwired. `packages/dc-wasm/scripts/validate-node.mjs:6-7` says so; the browser
harness is `examples/engine_host_demo.html`. **A zero from node is not evidence about picking.**
See §C3 — this exact confusion produced a wrong diagnosis once already.

**Ticket.** None. Export `renderPick` / document the raw contract if the raw surface is ever
meant to be used directly.

**Verified at** `937538a`, 2026-09-14 — re-run in the ENC-984 worktree, which edits
`dc_engine_host.cpp` (+1 line above `pick`, bindings block now `584-624`) **and rebuilds the
committed wasm**. `renderPick` is still absent from the rebuilt artifact: adding an export does
not drag in neighbouring symbols.

---

## DC-L08 — Layout and hierarchy algorithms exist, and you cannot reach them 🟡

**Claim.** The C++ core contains a real **squarified treemap** (Bruls et al.) plus `stratify`,
`partition`, `pack`, `dendrogram` and `sankey`
(`core/{include/dc,src}/transform/transforms/`, ENC-618a `fe0e4c7`), and a label
**`CollisionSolver`**, `LayoutGrid`, `Anchor` and `LayoutInteraction`
(`core/{include/dc,src}/layout/`). **None of them is reachable from the JSON command surface or
the browser**, so in practice layout is still entirely on the consumer — but for a different
reason than "the engine has no layout".

**Three independent proofs of unreachability:**

1. **No op-string dispatch.** The literal `"treemap"` occurs exactly once in the whole repo —
   its own accessor. `ManifestValidator::buildNode` (`core/src/manifest/ManifestValidator.cpp:776`)
   dispatches `filter, formula, window, bin, aggregate, sort, stack, sample, join/lookup,
   customCompute` and no hierarchy op.
2. **`createTransform` is a different concept entirely** — affine 2D
   (`core/src/commands/CommandProcessor.cpp:619`, `:644`), with no `op` field. Passing
   `{"cmd":"createTransform","op":"treemap"}` returns `ok:true` and silently creates an identity
   transform.
3. **Dead-stripped from the shipped wasm**, because nothing reaches it.

Likewise four of the six layout headers have **zero** non-test callers — they are tested and
otherwise unused.

**Re-check.**
```bash
grep -rn '"treemap"' core packages --include='*.cpp' --include='*.hpp' --include='*.ts' \
  | grep -v node_modules                                                   # 1 hit: its own op()
strings packages/dc-wasm/wasm/dc_engine_host.wasm | grep -ci treemap       # 0
# non-test, non-self includers of each layout header:
for h in CollisionSolver LayoutGrid Anchor LayoutInteraction PaneLayout LayoutManager; do
  n=$(grep -rl "dc/layout/$h.hpp" --include='*.cpp' --include='*.hpp' core/ \
      | grep -v '^core/tests/' | grep -v "/layout/$h\." | wc -l)
  printf '%-20s %s\n' "$h" "$n"
done
# CollisionSolver 0 · LayoutGrid 0 · Anchor 0 · LayoutInteraction 0 · PaneLayout 8 · LayoutManager 2
```
(`PaneLayout.hpp` and `LayoutManager.hpp` *are* used — 8 and 2 non-test includers. It is the
other four that are stranded.)

**Genuinely absent — still on you.** Text wrapping: `layoutText`
(`core/include/dc/text/TextLayout.hpp:14`) is a single-line advance loop with no newline
handling, no `maxWidth` and no wrap, and the TS `TextDrawSpec`
(`packages/dc-wasm/src/chart/text.ts:66-78`) carries no width budget. ENC-715's `drawText`
helper did not change this. Force-directed layout does not exist in any form — the only mention
is aspirational, in a comment at `core/include/dc/transform/StreamingScheduler.hpp:15`.

Also: `packages/authoring-kit/src/packing.ts` is **not** a layout module despite the name — it
is vertex-buffer byte packing (ENC-714).

**Ticket.** None. Either expose the hierarchy transforms through the manifest op dispatch or
mark them explicitly as internal/unshipped.

**Verified at** `937538a`, 2026-09-14 — dispatch tables read; `strings` re-run on the wasm
**as rebuilt by ENC-984** (`treemap` still 0, and so is `recipe`), per-header includer counts
re-run. A rebuild that adds one export does not resurrect dead-stripped code — only a binding
does.

---

## DC-L09 — The authoring-trials corpus is frozen and predates the current renderer 🟡

**Claim.** `docs/trials/` — **277** writeups, 645 files — was last touched on **2026-04-28**,
which is **139 days** and **98 commits to `core/`** before this entry was written. It is the
de-facto record of what the engine can draw, and it describes an engine that no longer exists.

**Concretely mis-calibrating.** 76 of the 277 writeups record "Text labels invisible in PNG
capture" as a defect; that is DC-L02, deliberate and now documented. Trials also predate
`lineAA@1` becoming the default line pipeline (ENC-993, `6a6f3d4`, 2026-09-14) and the
per-instance-colour pipelines reaching the browser (ENC-713, `9f1d2c9`).

**Re-check.**
```bash
ls docs/trials/*.md | wc -l                                              # 277
git log -1 --date=short --pretty='%ad' -- docs/trials                    # 2026-04-28
git rev-list --count "$(git log -1 --format=%H -- docs/trials)"..HEAD -- core/   # 98
```

**Treat a trial writeup as a dated observation, not as current behaviour.** Where a trial
contradicts this file, this file is newer by construction.

**Ticket.** None for a regeneration.

**Verified at** `6684a00`, 2026-09-14 — counts produced by the commands above.

---

## DC-L10 — An exported scene document cannot carry viewports, text overlay or bindings 🟠

**Claim.** `getSceneDocument()` / `dc::sceneToDocument` (ENC-984) export a `SceneDocument` from
the live `Scene`, and that document is **structurally complete for everything the Scene holds**
— panes, layers, transforms, buffers (byteLength), geometries, draw items and every style field,
proven byte-identical across a full save/restore loop by `dc_enc984_scene_export`. But three
sections of the `SceneDocument` schema come back **empty, always**: `viewports`, `textOverlay`
and `bindings`.

**Why.** Those three are *document-only* declarations. `SceneReconciler::reconcile` never
applies them to the `Scene` — it reconciles buffers, transforms, panes, layers, geometries and
draw items, and nothing else — and hosts read them straight off the parsed document instead
(`JsonHost` consumes `textOverlay` as a `TEXT` protocol message; `BindingEvaluator` takes
`DocBinding` values directly). There is therefore nothing in a `Scene` from which they could be
reconstructed, and inventing plausible values would be worse than omitting them.

**What this costs you.** Round-tripping a document *through* a Scene is lossy in exactly these
three places:

    parse(json) -> reconcile -> Scene -> sceneToDocument -> serialize

loses the viewports / textOverlay / bindings that `json` carried. `DocViewport` is the one that
bites: it holds the `xMin/xMax/yMin/yMax` data-space window and the pan/zoom/link flags, so a
naive "export the scene to save the view" **does not save the view**. The affine `DocTransform`
that the pan/zoom currently drives *is* exported, so the visible framing survives; the declared
data window and the interaction policy do not.

**Re-check.**
```bash
node -e 'const m=await import("./packages/dc-wasm/wasm/dc_engine_host.js");
const M=await m.default(), h=new M.DcEngineHost();
h.applyControl(JSON.stringify({cmd:"createPane",id:1,name:"p"}));
const d=JSON.parse(h.getSceneDocument(false));
console.log("viewports",JSON.stringify(d.viewports),"textOverlay",JSON.stringify(d.textOverlay),
            "bindings",JSON.stringify(d.bindings));' --input-type=module
# -> viewports {} textOverlay {"fontSize":12,"color":"#b2b5bc","labels":[]} bindings {}
#    i.e. present in the schema and empty of content — the textOverlay is the struct
#    default (12px, #b2b5bc, zero labels), not anything the scene actually declared.
grep -c 'viewports\|textOverlay\|bindings' core/src/document/SceneReconciler.cpp   # 0
```

**Workaround.** Keep the document you applied. A host that got its scene from
`parseSceneDocument` already holds the `viewports` / `textOverlay` / `bindings` it parsed;
merge those three sections back into the exported document before saving. A host that built its
scene from commands never had them and must track them itself.

**Ticket.** None for the deep fix (it needs the Scene to own the three declarations, or the host
to own a document alongside the Scene). Relevant to
[ENC-949](https://linear.app/encultured/issue/ENC-949) (view-state persistence — this is the
entry that says why "serialize the scene" is not sufficient for it) and
[ENC-986](https://linear.app/encultured/issue/ENC-986) (snapshot/restore).

**See also** the same boundary for *buffer bytes*, which is deliberate rather than a
limitation: the document carries each buffer's `byteLength` and you read the contents with
`getBufferBytes(id)`. Structure and bytes are separate calls on purpose.

**Verified at** `937538a`, 2026-09-14 — both commands above run in the ENC-984 worktree against
the rebuilt wasm; reconciler grep gives 0 hits.

---

# §C — Corrections

Beliefs that were held confidently and were wrong. They are here because each one cost real
time, and because a reader who half-remembers the wrong version needs to find the correction.

### C1 — `AxisRecipe::enableAALines` does not antialias anything

It **adds a second, shorter set of tick marks** hugging the axis (slots 16-21) alongside the
base ticks, which are created unconditionally. Turning it on **doubles the ticks**; it does not
smooth them. Since ENC-993 both sets bind the *same* `lineAA@1` pipeline
(`core/src/recipe/AxisRecipe.cpp:72` and `:114`/`:129`), which settles the question — a flag
cannot be "the AA switch" when what it gates and what it sits beside render identically.

This misreading was the stated premise of ENC-993's own ticket. The correction is now recorded
in both the header and the source: `core/include/dc/recipe/AxisRecipe.hpp:35-38`,
`core/src/recipe/AxisRecipe.cpp:97-101`. Re-check: `sed -n '95,101p' core/src/recipe/AxisRecipe.cpp`.

### C2 — `lineAA@1` is the default line pipeline; `line2d@1` is the opt-out

Since ENC-993 (`6a6f3d4`) a line mark gets `lineAA@1` unless it explicitly asks otherwise
(`core/src/encode/EncodePass.cpp:44-60`; the default parameter is `LineStyle::LineAA` at
`core/include/dc/encode/EncodePass.hpp:140`). There is **no MSAA anywhere in the renderer** —
every target is `sampleCount = 1` — so lineAA's shader coverage is the only antialiasing the
engine has, and it is the only line pipeline that honours `lineWidth` and the dash pattern.

Opt back out by pinning `"pipeline": "line2d@1"` on the mark (inference reads both directions,
`core/src/manifest/Manifest.cpp:454-463`) or passing `LineStyle::Line2d`. `CrosshairRecipe` and
`DebugOverlay` are deliberate holdouts. Reach for it only when you want a raw 1px hairline.

### C3 — A `pick()` of 0 under node means "no WebGPU", not "picking is unwired"

The retired workspace entry diagnosed a zero pick result as `renderPick`/`DawnPickBackend` not
being triggered. That was wrong on both counts: `pick` *does* call `renderPick`
(`core/wasm/dc_engine_host.cpp:433`) and `DawnPickBackend` *is* registered
(`core/src/gpu/DawnSceneRenderer.cpp:160`). The zero came from `ensureRenderer()` failing under
node, which has no `navigator.gpu`. See DC-L07.

### C4 — `CHART_AUTHORING.md` §8/§9 used to promise GPU gradient fills that do not render

Until ENC-991, §8 ("Apply a gradient to any DrawItem… this creates gradient area charts") and
§9 ("Dashboard panels — `instancedRect@1` with `cornerRadius` and gradient fills") described
behaviour that does not exist on the GPU path — see DC-L04. Anyone following the guide would
have authored a chart that silently rendered flat. Both passages were corrected in the same PR
as this file, and now point at DC-L04 and at the `triGradient@1` workaround.

Worth noting as a pattern: the authoring guide and the limitations log had drifted into
contradicting each other, and the guide was the one people read.

---

# §R — Retired

Entries that stopped being true. Nothing is deleted: a log that shows its own falsifications is
the only kind worth believing on the entries it still asserts.

The `L*` ids below belong to the superseded workspace-level document
(`specs/2026-06-21-dynacharting-authoring-corpus/docs/LIMITATIONS.md`, single commit `cf21af4`,
2026-06-21, never amended). This file's ids are `DC-L*` and do not continue that numbering.

| Old | Claim | Disposition |
|---|---|---|
| **L1** 🔴 | Per-instance colour pipelines absent from the WASM build (`rect4_color` → `UNSUPPORTED_VERTEX_FORMAT`, `instancedRectColor@1` → `UNKNOWN_PIPELINE`) | **RESOLVED** by ENC-713 (`9f1d2c9`, 2026-06-23) — the artifact was stale, not the source. Both pipelines now create and bind successfully against the committed wasm. |
| **L2** 🟠 | WASM framebuffer readback is bottom-up; a raw blit renders inverted | **RESOLVED at the blit** by ENC-696 (`d6b5acd`, 2026-06-21), with regression test `EngineHost.blit.test.ts`. The underlying convention is unchanged → **DC-L05**. |
| **L3** | Pipeline/format matrix: 10 available, `instancedRectColor@1` and `instancedPointColor@1` unavailable | **SUPERSEDED.** All **12** registered pipelines are now present and accepted by the committed wasm; there are no ✗ rows. Re-check: `grep -c '^  reg(' core/src/pipelines/PipelineCatalog.cpp` → `12`. |
| **L4** 🟡 | Picking API present but not trivially exercisable | **CARRIED with its cause corrected** → **DC-L07** and **§C3**. The symptom was real; the diagnosis was wrong. |
| **L5** 🟠 | `applyControl` failures are silent | **MITIGATED** by ENC-701 (`181c305`, 2026-06-21). Four residuals survive → **DC-L06**. |
| **L6** | The engine has no layout — treemap, collision-avoided labels etc. are on you | **SUPERSEDED.** The engine *has* a squarified treemap and a collision solver; they are unreachable → **DC-L08**. Text wrapping and force-directed remain genuinely absent. |
| **L7** 🟠 | `setDrawItemGradient` is a no-op *in this WASM build* | **STILL TRUE and broader** — it is a no-op in the native Dawn renderer too → **DC-L04**. |

**Three of these seven were false, and two were false before the document was committed.**
ENC-696 landed at 19:52:31 and ENC-701 at 19:52:34 on 2026-06-21; the document was committed at
20:06:20 the same evening. Fourteen minutes. Its author could not have known, because the file
lived in a different repository from the code it described.

---

# §H — How this file stays true

The previous limitations log failed in a specific, diagnosable way, and this section is the
response to that diagnosis rather than a promise to try harder.

**What went wrong.** It lived in `specs/`, in a different repo from the code. Its author could
not see two fixes that landed fourteen minutes earlier. Nothing in any subsequent PR touched it
or pointed at it. Its entries recorded conclusions but not *procedures*, so falsifying one meant
redoing the original investigation — which nobody did, for 85 days, while its 🔴 top-severity
entry was wrong.

**Six things this file does differently.**

1. **It is repo-local.** This is the variable that actually predicts survival here, and the
   evidence is in the same git history: `CHART_AUTHORING.md`, at this repo's root, was amended
   by **both** of the last two feature PRs (`6a6f3d4`, `6684a00`) *on the day each landed*. The
   workspace-level limitations file got **one** commit in 85 days. Same authors, same period,
   same discipline — different directory. A doc a contributor has already checked out is a doc
   they can fix in the PR that falsified it.

2. **Every entry carries a `Re-check` command.** If falsifying an entry costs an investigation,
   nobody falsifies it. If it costs one paste, the next person through does it for free. This is
   also an admission gate: **if you cannot write the one-liner, the entry is not ready** — you
   have an impression, not a limitation.

3. **Every entry carries a `Verified at <sha> (<date>)`.** A stale SHA is a visible expiry date,
   which the old file had no equivalent of. The rule: **if your change touches a file an entry's
   `Re-check` names, run it and either restamp the entry or move it to §R.** That rule is
   mechanical, scoped to files you already have open, and is the whole maintenance burden.

4. **Nothing is deleted.** Fixed entries move to §R with the commit that killed them; wrong
   beliefs move to §C. Silently dropping an entry is indistinguishable from forgetting it, and a
   reader who cannot see the log correcting itself has no reason to believe the entries that
   remain.

5. **Pointers live where the limitation is hit**, not only here. This PR installed them rather
   than promising them: `CLAUDE.md` (top, the `ctest` block → DC-L01, the `--png` block →
   DC-L02/DC-L03) and `CHART_AUTHORING.md` (header, §8 and §9 → DC-L04). Adding the pointer is
   part of adding an entry. The DC-L02 evidence is the argument for doing it: 76 of 277 trial
   writeups filed the same by-design behaviour as a bug, because the only record of it was
   somewhere they were not.

6. **Entry ids are stable and greppable.** `DC-L04` can be cited from a code comment, a Linear
   ticket or a PR description without ambiguity, and reusing an id for different content is
   never allowed — retired ids stay retired.

**Adding an entry** — a checklist, not a ceremony:

- [ ] Verify it against current `main` yourself. Someone else's earlier assessment is a lead,
      not evidence.
- [ ] Write the `Re-check` command and **run it**. Paste the output you actually got.
- [ ] Stamp `Verified at <sha>, <date>`.
- [ ] Give it the next free `DC-L*` id, a severity, and a ticket — or say "None", explicitly.
- [ ] Say what the **workaround** is. An entry with no workaround and no ticket is a complaint.
- [ ] Add a pointer from wherever someone would hit it.

**Retiring an entry:** move the row to §R with the fixing commit and ticket, and if a residual
survives, open the new entry and link them in both directions (DC-L05 and DC-L06 are the worked
examples). If the entry was not merely fixed but *wrong*, it belongs in §C as well — that is the
case the old L4 taught us.

**Severity.** 🔴 will silently produce a wrong result or a false green. 🟠 will cost you an
afternoon. 🟡 is a sharp edge you should know about before you hit it.
