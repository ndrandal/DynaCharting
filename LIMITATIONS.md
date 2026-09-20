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

**Verified in full at `6684a00` on 2026-09-14**, except six entries ENC-984 had to re-verify
because it changed files their `Re-check` commands name: DC-L01, DC-L05, DC-L06, DC-L07 and
DC-L08 at `5ac198a`, and DC-L10 at `5ac198a`. Every command below was executed at the commit
**its own entry stamps**, and produced the output shown there. A stamp naming a commit where the
command does not yet hold is a bug in the entry, not a shortcut — ENC-984 shipped five of those
and corrected them.

---

## DC-L01 — A green default `ctest` says nothing about the renderer 🔴

**Claim.** `cmake -B build && ctest --test-dir build` runs **196** tests and builds **no
renderer at all**. `dc_gpu`, `dc_json_host`, all four headless demo servers and **47 render
tests** are excluded at *configure* time by `DC_FETCH_DAWN` (default `OFF`,
`core/CMakeLists.txt:165`). They are not "skipped" — they never enter `CTestTestfile.cmake`,
so nothing reports them as missing.

**Why it bites.** "196/196 passed" is the most reassuring possible output and it is compatible
with the renderer being completely broken. Every pixel-level guarantee in this engine lives in
the 47 tests that did not run — **including the tier-0 check that the chart depicts its data at
all** (ENC-1249, `scripts/tier0.sh`).

**Re-check.**
```bash
grep -cE '^\s*add_test\(' core/CMakeLists.txt            # 243  — all tests that exist
grep -c '^add_test('  build/core/CTestTestfile.cmake     # 196  — all tests you just ran
grep -n 'DC_FETCH_DAWN:BOOL' build/CMakeCache.txt        # OFF
```
The 47-test gap is the single `if (DC_HAS_DAWN)` block at `core/CMakeLists.txt:1916-2489`.
Target-level gap: **52** targets (`dc_gpu`, `dc_glfw_system`, `dc_json_host`,
`dc_dawn_window_demo`, 4 servers, 44 test executables) behind the four `if (DC_HAS_DAWN)`
guards at lines 274, 374, 391 and 1916. Measured directly:
```bash
find build-dawn/core -maxdepth 1 -type f -executable | wc -l   # 242
find build/core      -maxdepth 1 -type f -executable | wc -l   # 192  -> 50 executables missing
```
(50 executables, not 52 targets: `dc_gpu` is a library, and `dc_glfw_system` /
`dc_dawn_window_demo` need the *second* gate `-DDC_DAWN_WINDOWED=ON`.)

**ENC-1249 corrected two stale details here**, both dating from before ENC-995's stamp: the
block was already at 1916, not 1899, and the "five guarded ranges" list named boundaries
(`361-366`) that no `if (DC_HAS_DAWN)` guard starts at. Re-derive them with
`grep -n 'if (DC_HAS_DAWN)' core/CMakeLists.txt` rather than trusting a transcribed range.

**ENC-1277 re-ran the first two Re-check lines on 2026-09-20 at `f907f93`** and restamped the
pair `192/239` → `193/240`; **ENC-1265 restamped it again the same day** to
**`196/243`** (three default-build tests: `dc_enc1265_render_timing` and its two
`WILL_FAIL` negative controls). The **47-test gap is unchanged** through both, which is the
point this entry makes and the reason the pair is not worth chasing on its own. Only those two lines were
re-measured — the executable counts below (`242`/`192`) need a `-DDC_FETCH_DAWN=ON` build and
still carry ENC-1249's stamp.

**Working around it.** Build Dawn once (~55-60 min, then incremental) and keep the build dir:
```bash
cmake -B build-dawn -G Ninja -DDC_BUILD_TESTS=ON -DDC_FETCH_DAWN=ON
cmake --build build-dawn -j$(nproc)
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json ctest --test-dir build-dawn -j$(nproc)   # 236/236
```
Three sessions did exactly this on 2026-09-14 (ENC-992, ENC-993, ENC-995), so it is feasible,
not theoretical. ENC-992/993 got 231/231 on lavapipe; ENC-995 added one always-built test and
got 233/233 on lavapipe and 231/233 on Vulkan/NVK hardware — the two hardware failures
being the `dc_enc619_dawn_fft` / `dc_enc619_dawn_marching_squares` pair the correction below
already pins to the *unmodified* tree. `-DDC_DAWN_WINDOWED=ON` is a *second*,
independent gate for `dc_dawn_window_demo` — `-DDC_FETCH_DAWN=ON` alone gets 50 of the 51.

**Ticket.** [ENC-994](https://linear.app/encultured/issue/ENC-994) (Backlog) covers two of the
43 (`d28_1_dawn_lineaa`, `d29_1_dawn_blend`). The other 41 have no owner.

**The absolute numbers move; the shape of the gap does not.** ENC-984 added one always-built
logic test, taking the pair from 188/231 to 189/232; ENC-995 added another, taking it to
190/233; **ENC-1249 added three Dawn-only tests, taking it to 190/236 and the gap from 43 to 46;
ENC-1257 added one default-build and two Dawn-only tests, taking it to 191/238 and the gap
to 47; ENC-1251 added one default-build test, taking it to 192/239 with the gap unchanged at
47** (measured, not predicted). The gap is still exactly the `DC_HAS_DAWN` block, every time. Read the *difference*, not
to 47; ENC-1253 added one default-build test, taking it to 192/239 with the gap UNCHANGED at
47** (measured post-merge, not predicted). The gap is still exactly the `DC_HAS_DAWN` block, every time. Read the *difference*, not
the left-hand number: a change that grows the registered count tells you nothing about the
renderer either — which is the whole point, and is why three consecutive tickets moving this
number changed nothing about what the default build proves.

**And ENC-1249 is the case that shows why the gap matters rather than merely being untidy.** The
tier-0 check (`scripts/tier0.sh` -> `dc_enc1249_tier0_truthful`) is the one that asserts a chart
depicts its data — that a rising series rises. It is a claim about pixels, so it needs the
renderer, so it is inside the 47. A green default `ctest` therefore proves nothing about tier 0
either. The check exits **3** (never 0) when no adapter comes up, and `scripts/tier0.sh` turns
that into exit 2 "CANNOT RUN", precisely so it cannot join the class of things this entry is
about.

**Verified at** `ENC-1251 HEAD`, 2026-09-19 — counted statically from `core/CMakeLists.txt`
(239) and empirically from a real default configure in the ENC-1251 worktree (192), giving a gap
of **47**; executable counts measured from `build-dawn` (242) against `build-default` (192).
The `236/236` in the *Working around it* block above is ENC-1249's lavapipe figure and is left
as that session recorded it; this session measured **237 of 239 on hardware**, the two failures
being the `dc_enc619_dawn_*` pair the correction below pins to the unmodified tree.
**Verified at** `ENC-1253 HEAD`, 2026-09-20 — counted statically from `core/CMakeLists.txt`
(239) and empirically from a real default configure in the ENC-1253 worktree (192, all passing),
giving a gap of **47**. Executable counts are carried forward from the ENC-1249 measurement
(`build-dawn` 239 against `build` 190); no Dawn build was made in this worktree, which is itself
this entry's point — ENC-1253's renderer change (`DawnTextSdfBackend`, §C0) is inside the 47 and
was verified by a browser capture rather than by `ctest`.

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

**Verified at** `5ac198a`, 2026-09-14 — re-run in the ENC-984 worktree (which edits
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

**Verified at** `5ac198a`, 2026-09-14 — all four residuals read in source and unmoved by
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

**Verified at** `5ac198a`, 2026-09-14 — re-run in the ENC-984 worktree, which edits
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

**Verified at** `5ac198a`, 2026-09-14 — dispatch tables read; `strings` re-run on the wasm
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

## DC-L10 — Exporting a Scene as a SceneDocument is lossy in four specific places 🟠

**Claim.** `getSceneDocument()` / `dc::sceneToDocument` (ENC-984) export a `SceneDocument` from
the live `Scene`. The extraction itself is faithful for every field the document schema has a
slot for — pinned field-by-field, and for **every** `blendMode` / `anchorPoint` / `gradientType`
/ vertex-format spelling, by `dc_enc984_scene_export`. But **save → restore is not the identity
function**, in four places, and one of them is invisible to the obvious test.

**1. `viewports`, `textOverlay`, `bindings` always come back empty.** They are *document-only*
declarations. `SceneReconciler::reconcile` never applies them to the `Scene` (grep: 0 hits) —
hosts read them straight off the parsed document instead (`JsonHost` emits `textOverlay` as a
`TEXT` protocol message; `BindingEvaluator` takes `DocBinding` values directly). Nothing in a
`Scene` can reconstruct them. **`DocViewport` is the one that bites**: it holds the
`xMin/xMax/yMin/yMax` data-space window and the pan/zoom/link flags, so a naive "export the
scene to save the view" **does not save the view**. The affine `DocTransform` the pan/zoom
currently drives *is* exported, so the visible framing survives; the declared data window and
the interaction policy do not.

**2. Geometry bounds have no field in the schema.** `boundsMin`/`boundsMax`/`boundsValid` are
live Scene state — `DawnSceneRenderer.cpp:359` frustum-culls on them — set by the real
`setGeometryBounds` command. `DocGeometry` has no slot, so they cannot be exported at all. Note
the *older* D45 `dc::serializeScene` (`core/src/session/SceneSerializer.cpp:156-166`) **does**
carry them: this dialect is less faithful here, which is the reason the ENC-984 helper is named
`serializeSceneAsDocument` rather than overloading `serializeScene`.

**3. A stale value behind a cleared flag is exported but not restored.** Two reachable cases,
both with `reconcile.ok == 1`:

  * `setDrawItemGradient type:"none"` clears the type and leaves `angle`/`center`/`radius` in
    the Scene. The export carries them; the reconciler only emits `setDrawItemGradient` when the
    type is non-empty → `gradientAngle 1.25 → 0`, `gradientCenter [0.25,0.75] → [0.5,0.5]`,
    `gradientRadius 0.875 → 0.5`.
  * `setPaneClearColor enabled:false` clears only `hasClearColor` and leaves the colour. The
    export carries it; `reconcilePanes` only emits `setPaneClearColor` when the document says
    `hasClearColor` → `clearColor [0.5,0.25,0.125,1] → [0,0,0,1]`.

  In both, **`sceneToDocument` is not the culprit** — it exports what the Scene holds. The loss
  is in `SceneReconciler`, so it predates ENC-984 and applies to any document restore.

**4. …and in `compact` mode that loss is SILENT.** Compact omits fields at their default on
*both* sides, so the same round trip reports byte-identical output while the values are gone. An
equality check cannot see a field that neither side serialized. This is why
`dc_enc984_scene_export`'s round-trip assertion is scoped to the states it builds and explicitly
does **not** claim general losslessness; `test_known_lossy_round_trips` pins all four cases,
including the compact-mode false positive.

**Re-check.**
```bash
# 1 — the three document-only sections, and the reconciler that never applies them
node -e 'const m=await import("./packages/dc-wasm/wasm/dc_engine_host.js");
const M=await m.default(), h=new M.DcEngineHost();
h.applyControl(JSON.stringify({cmd:"createPane",id:1,name:"p"}));
const d=JSON.parse(h.getSceneDocument(false));
console.log("viewports",JSON.stringify(d.viewports),"textOverlay",JSON.stringify(d.textOverlay),
            "bindings",JSON.stringify(d.bindings));' --input-type=module
# -> viewports {} textOverlay {"fontSize":12,"color":"#b2b5bc","labels":[]} bindings {}
#    present in the schema, empty of content; the textOverlay is the struct default.
grep -c 'viewports\|textOverlay\|bindings' core/src/document/SceneReconciler.cpp   # 0

# 2, 3 and 4 — all pinned as executable assertions
ctest --test-dir build -R dc_enc984_scene_export --output-on-failure
# -> [enc984] known-lossy cases are pinned, not papered over ... all tests passed
```

**Workaround.** Keep the document you applied. A host that got its scene from
`parseSceneDocument` already holds the `viewports` / `textOverlay` / `bindings` it parsed; merge
those sections back into the exported document before saving. A host that built its scene from
commands never had them and must track them itself. For case 3, re-issue the clearing command
after restore. For case 2, use the D45 `dc::serializeScene` if bounds are what you need.

**Ticket.** None for the deep fix (it needs the Scene to own the three declarations, or the host
to own a document alongside the Scene, plus reconciler changes for case 3). Relevant to
[ENC-949](https://linear.app/encultured/issue/ENC-949) (view-state persistence — this entry is
why "serialize the scene" is not sufficient for it) and
[ENC-986](https://linear.app/encultured/issue/ENC-986) (snapshot/restore).

**Not a limitation, by design:** buffer **bytes**. The document carries each buffer's
`byteLength` and you read the contents with `getBufferBytes(id)`. Structure and bytes are
separate calls on purpose.

**Verified at** `5ac198a`, 2026-09-14 — the node snippet and the grep run in the ENC-984
worktree against the rebuilt wasm; cases 2/3/4 reproduced standalone against `libdc.a` and then
pinned as test assertions. Stamped at `5ac198a` and not at its parent on purpose: the `ctest`
line above only produces the shown output once `test_known_lossy_round_trips` exists, and a
stamp naming a commit where the command does not yet hold is exactly the defect this entry's
own PR had to fix in five other entries.

---

## DC-L11 — A manifest `arc` mark is centred at the clip origin, and is an ellipse 🟠

**Claim.** `ArcOptions` — the polar centre and the chord count for `Mark::Arc` — is a
**defaulted parameter that no production caller ever passes**. Both call sites,
`Manifest::build` and `InteractionRuntime::compileAll`, stop at `lineStyle`. So every `arc`
mark authored through a manifest (`"type":"arc"`, `ManifestValidator.cpp:260`) is pinned to
`polar.centerX/centerY = 0,0`, and there is no manifest key that moves it.

Worse, `polarToClip` maps `(theta, r)` to `cx + r*cos(theta), cy + r*sin(theta)` in **clip**
units on **both** axes, and clip units are not square. On any viewport that is not 1:1 a polar
"circle" comes out stretched to exactly the viewport's aspect ratio — measured below at 2.000
on a 1200x600 target. The two headless demo servers sidestep this by computing `pieRadiusX` and
`pieRadiusY` separately from `W` and `H` (`showcase_server.cpp:617`, `dashboard_server.cpp:718`)
and tessellating pie geometry by hand, never touching `Mark::Arc` — which is the tell.

**Why it bites.** The arc mark looks fully wired: the validator knows `"arc"`, the encode pass
compiles it, `dc_enc613_dawn_polar_arc` renders it green. What none of that exercises is the
manifest path — nor the browser, which does not carry the encode pass at all (check 5 below); because the corpus contains no `arc` chart at all (`grep -l '"arc"' charts/*.json`
-> nothing; `charts/072-polar-rose.json` is precomputed vertex data, not an arc mark). The first
person to author a pie in a manifest gets one centred on the middle of the viewport whether they
want that or not, and egg-shaped unless their pane happens to be square.

**Re-check.**
```bash
# 1 — neither production caller passes ArcOptions (both stop at lineStyle)
grep -n -A3 'encodePass_.compile' core/src/manifest/Manifest.cpp
grep -n -A3 'encode_.compile'     core/src/interaction/InteractionRuntime.cpp
# -> ..., nullptr, md.lineStyle);   /   ..., /*rowIds=*/nullptr, s.lineStyle);

# 2 — the polar map carries no aspect term
grep -n -A4 'inline void polarToClip' core/src/encode/EncodePass.cpp
# -> outX = p.centerX + r * cos(theta);  outY = p.centerY + r * sin(theta);

# 3 — no manifest key reaches either field
grep -rn 'segmentsPerArc\|segmentsPerTurn\|PolarParams' core/src/manifest/
# -> (no output)

# 4 — and no chart in the corpus uses the mark
grep -l '"arc"' charts/*.json | wc -l        # -> 0

# 5 — the browser module does not contain the encode pass AT ALL. dc_engine_host
#     compiles 18 objects (the Dawn backends + the embind host) and links libdc.a;
#     nothing in it references EncodePass, so the archive member is never pulled.
strings packages/dc-wasm/wasm/dc_engine_host.wasm | grep -c 'lockstep broken'   # -> 0
strings packages/dc-wasm/wasm/dc_engine_host.wasm | grep -c 'triGradient@1'     # -> 1
# i.e. the PIPELINE names are in there (catalog + backends) and the compiler that
# feeds them is not. Rebuilding the wasm after changing EncodePass.cpp is the
# other half of the proof: ENC-995 did, and got a byte-identical artifact
# (sha256 3bb50045...8ec9 before and after).
```
Rendered proof of the aspect half, on the real Dawn path: a full-turn wedge with equal inner and
outer clip radii, drawn to a 1200x600 offscreen target, occupies **1140 px wide x 570 px tall,
aspect 2.000** — the viewport's aspect, exactly. The same geometry on 900x900 is 855 x 855.

**Working around it.** Pre-scale the radius per axis yourself, as both demo servers do, and
accept the clip-origin centre — or drive `EncodePass::compile` directly with an explicit
`ArcOptions` instead of going through `Manifest::build`. ENC-995 fixed the *chord count* half of
the default (it is now derived from the wedge span rather than a fixed 24), which is why this
entry is about the centre and the aspect and not about smoothness.

**Ticket.** None yet — `ArcOptions` needs a manifest surface, and `PolarParams` needs either an
aspect term or a documented "clip units, pre-scale yourself" contract.

**Verified at** `ENC-995 HEAD`, 2026-09-14 — greps 1-4 run in the ENC-995 worktree; the render
measurement from a `dc_gpu` harness against `build-dawn` on Vulkan/NVK.

## DC-L12 — A candle6 `halfWidth` is a request, not the rendered width 🟡

**Claim.** Since ENC-1257 `instancedCandle@1` treats the per-instance `halfWidth` (candle6 byte
offset 20) as the author's *nominal proportion* and resolves the width it actually draws from
the bar pitch in PIXELS: `dc::resolveBarWidth` (`core/include/dc/render/BarSizing.hpp`) enforces
a **1px minimum inter-bar gap** and a **24px maximum body**, and caps the fixed-pixel wick by the
same budget. So `halfWidth` no longer maps one-to-one onto pixels, and two draws of the same
records at different viewport widths or zoom levels can render different body widths.

**Why it bites.** Authoring code that computes a `halfWidth` to hit an exact pixel width will be
overruled at the extremes — and only at the extremes. Inside the band (pitch between roughly 5px
and 30px) the authored value is returned unchanged, which is why every pre-existing scene in this
repo renders identically; you will meet this only on a very dense or a very sparse chart, i.e.
exactly where a naive value was wrong anyway. The resolution is host-side and per-draw, so it is
invisible in the buffer bytes: `getBufferBytes()` and `getSceneDocument()` still report the
authored `halfWidth`, and so does `SvgExporter` (`core/src/export/SvgExporter.cpp:517`), which
does **not** apply the rule — an SVG export of a dense candle chart still fuses.

**Re-check.**
```bash
cmake -B build && cmake --build build --target dc_enc1257_bar_sizing -j$(nproc)
./build/core/dc_enc1257_bar_sizing | grep -E 'candles-aapl (pre|post)-fix|live 55px'
#   candles-aapl pre-fix: pitch 4.5333 body 3.6267 gap 0.9067
#   candles-aapl post-fix: body 3.5333 gap 1.0000 halfClip 0.004417
#   live 55px pitch: body 24.000 gap 31.000
```
The first line is the authored width; the second is what is drawn.

**Working around it.** Nothing to work around if you want legible bars — that is the point. If
you need an exact pixel width, widen the config rather than fighting it: `BarSizingConfig` is a
defaulted parameter on every entry point, and passing `maxBodyPx` / `minGapPx` of your choosing
restores whatever behaviour you need. The rule never applies at all when the draw has no pitch
(a single bar), no viewport, or a degenerate transform.

**Ticket.** None for the engine. `SvgExporter` not applying the rule is a real gap and is
unowned.

**Verified at** `ENC-1257 HEAD`, 2026-09-19 — the command above was run in the ENC-1257
worktree and produced exactly the three lines shown; the pixel-level counterpart
(`dc_enc1257_dawn_candle_gap`, Dawn-gated) renders 10, 500 and the 156-bar candles-aapl
configuration and asserts one lit run per bar.

---

## DC-L13 — The axis domain is now measured; nothing in the engine draws it, and nothing frames the plot to it 🟠

**Claim.** As of ENC-1252 a chart can *state* a real domain — `DomainTracker`
(`packages/dc-wasm/src/chart/domain.ts`) folds the live dataplane records and the showcase
publishes the result on the overlay root (`data-dc-axis-domain`) and `window.__dcAxisDomain`.
Two halves of "the chart has an axis" are still missing, and they are missing in ways that are
easy to mistake for the domain being wrong:

1. **The axis MARKS are still a DOM/SVG overlay.** `apps/showcase/src/chrome/AxisOverlay.tsx`
   emits `<svg><line>/<text>`; no showcase manifest creates axis geometry and no `AxisRecipe`
   is reachable from the browser path. So any capture taken below the DOM — every
   `dc_json_host --png`, every engine framebuffer readback — has **no axis at all**, not merely
   no text. This compounds **DC-L02** rather than being covered by it: DC-L02 is about labels,
   this is about the spine, ticks and gridlines too.

2. **The measured domain does not drive the framing.** The data→clip `transform` is still a
   baked literal in each `view.json`, so the domain the axis states and the window the plot
   shows are independent numbers. Measured on `candles-aapl`: the stated x domain is
   **3.6 … 270.4** (267 real records) while the x-anchored window spans **150** index units, so
   ~44% of the stated domain is off-frame. `AxisOverlay` filters ticks outside the box, so the
   *visible* tick count silently drops — 5 of the requested 7 x ticks at 16 s of replay,
   observed in the running app (nvidia/ampere adapter, 2026-09-19). Every label shown is true;
   there are just fewer of them than the view asked for, and the axis does not say so.

3. **Most views still state a literal.** 3 of the 14 showcase views with `chrome.axes` declare
   an `axisDomain` (`candles-aapl`, `ohlc-bars`, `candle-overlays`); the other 11 keep their
   hand-typed `min`/`max` and are captions in exactly the sense
   `specs/2026-09-19-chart-quality-bar/SPEC.md` §1.3 describes. The report labels which is
   which (`source: "derived" | "literal"`), so this is visible rather than assumed — but it is
   not yet fixed.

**Why it bites.** A reader who sees the derived domain land and concludes "the axis works now"
will be wrong twice: the engine still draws nothing, and the numbers do not describe the frame.
The failure mode of (2) in particular *looks* like a domain bug — sparser ticks after a change
that was supposed to improve the axis — when it is the absent framing rule.

**Re-check.**
```bash
# 1 — RETIRED by ENC-1253: the engine draws the marks. The SVG overlay still
#     exists and is still on by default, but it is now a duplicate — these two
#     commands say the axis is engine-drawn:
grep -c 'EngineAxis' apps/showcase/src/chrome/useEngineAxis.ts    # -> >= 1
grep -rl 'AxisRecipe' apps/showcase/ --include=*.ts --include=*.tsx | wc -l   # -> 0 (still: the
#     browser path authors textSDF@1/lineAA@1 directly; the C++ AxisRecipe is unbound, ENC-990)

# 2 — the framing is still a literal, per view
grep -h '"transform"' apps/showcase/views/candles-aapl/view.json
# -> "transform": { "sx": 0.011333333, "sy": 0.10625, "tx": -0.895333333, "ty": -43.88125 },

# 3 — measured vs. literal, across the catalogue
grep -l 'axisDomain' apps/showcase/views/*/manifest.ts | wc -l              # -> 3
grep -l '"min": .*"format"' apps/showcase/views/*/view.json | wc -l         # -> 11
grep -l '"axes"' apps/showcase/views/*/view.json | wc -l                    # -> 14

# 4 — the domain itself is real and tracks the data (folds the committed captures)
npx vitest run apps/showcase/src/chrome/deriveAxes.test.ts                  # -> 14 passed
```

**Working around it.** To read a chart's domain without a screenshot, read
`window.__dcAxisDomain[viewId]` or the overlay's `data-dc-axis-domain` attribute — both are
live and both name their provenance. Do not infer the visible window from the stated domain;
they are not the same number until (2) lands.

**Update, 2026-09-19 (ENC-1256) — part (2) now has a primitive, and still has no caller.**
`packages/dc-wasm/src/chart/plotbox.ts` supplies the missing output side: a clip-space plot box
derived from pixel gutters, `fitToPlotBox` / `frameSeries` to map a measured `ObservedDomain`
onto it, and `checkTier2Framing` to score the result against SPEC D1's tier-2 measures. So "the
measured domain does not drive the framing" is no longer a *missing capability*. It is now an
*unadopted* one: every `view.json` still bakes its `transform` literal, the numbers in (2) above
are unchanged on the app path, and the new module has zero callers on any render path. That
half is **DC-L14**, which is where the re-check for it now lives.

**Update, 2026-09-20 (ENC-1253) — PART (1) IS NO LONGER TRUE. The engine draws the marks.**
`packages/dc-wasm/src/chart/axis.ts` emits the gridlines, tick marks and spine as `lineAA@1`
clip-space geometry and the tick labels and axis titles as `textSDF@1` glyph runs, into the same
scene and the same canvas as the data. The showcase drives it from the SAME resolved axes and
the SAME ticks the SVG overlay uses (`useEngineAxis` / `engineAxis.ts`), so the overlay is now a
duplicate rather than the source — `?svgAxis=0` removes it and the axis stays.

Measured on a canvas-only capture (SPEC D10, `harness/shoot-live.mjs --mode canvas`, hardware
adapter `vendor: nvidia, architecture: ampere`, `info.isFallbackAdapter: false`,
`subgroupMinSize: 32`), scored with `harness/score.py --diagnose`, same view and same replay
with the engine axis off and on:

| tier-1 check | `?engineAxis=0` | engine axis on |
|---|---|---|
| T1.1 the engine draws an axis | **FAIL** — "declares no engine-drawn tick label, gridline or spine" | **PASS** — 3 x labels, 5 y labels, 8 gridlines, 2 spines |
| T1.3 text in the same raster | *did not run* (no text to check) | **PASS** — all 10 runs carry ink |
| T1.4 labels disjoint + in frame | *did not run* | **PASS** |
| T1.5 x renders time as time | *did not run* | **PASS** |
| T1.7 text contrast ≥ 4.5:1 | *did not run* | **PASS** |
| T1.8 gridline ceiling + visibility | *did not run* | **PASS** |

Parts **(2)** and **(3)** are unchanged and still true: every view still bakes a literal
`transform` (**DC-L14**), and 11 of 14 views still state a literal domain. The consequence you
can see in the raster is that the data is not fitted to the plot box the furniture is laid out
against, so the leftmost bars run under the price labels.

**Ticket.** Part (1): **ENC-1253**, done. Part (2)'s primitive is **ENC-1256** (done); adopting
it on the showcase and customer-layer render paths is **DC-L14** / **ENC-1273**. Converting the
remaining 11 views is unticketed.

**Verified at** `ENC-1253 HEAD`, 2026-09-20 — (3)'s greps re-run in the ENC-1253 worktree and
unchanged. (1) is retired by the measurement above. (2)'s tick-count and domain observations are
carried forward from the ENC-1252 measurement over CDP and remain true because nothing on the
framing path changed.

---

## DC-L14 — The plot box exists, and no render path frames anything with it 🟡

**Claim.** As of ENC-1256 this engine finally has a plot-box concept —
`packages/dc-wasm/src/chart/plotbox.ts`: a clip-space rectangle inset from the canvas by
per-side gutters given in CSS pixels, `gutters()` naming the four furniture bands,
`paneRegionFor()` deriving the matching `setPaneRegion`, `fitToPlotBox()` / `frameSeries()`
mapping an ENC-1252 `ObservedDomain` into it, and `checkTier2Framing()` scoring the result
against SPEC D1's tier-2 measures. It is exported from `@repo/dc-wasm` and
`@repo/dc-wasm/chart`, and covered by 32 unit tests including a before/after reconstruction of
the live NEXO chart's measured framing.

**Nothing that renders calls any of it.** The one consumer is `SceneBuilder`'s
`transform({domain, canvas})` / `pane({plotBox})` overloads, and `SceneBuilder` itself has zero
non-test callers — so the whole chain is reachable only from tests. Concretely:

- Every `apps/showcase/views/*/view.json` still bakes a literal `transform`, and
  `useViewSwitch.bakeTransform` still writes that literal into the engine. **The showcase's
  framing is exactly what it was**; DC-L13 part (2)'s measured numbers are unchanged.
- `customer-layer` (the live product, and the surface SPEC §1.0 measured) is a separate repo
  and has not adopted it either.

**Why it bites.** This is the **DC-L08 shape**, and DC-L08 is in this file precisely because
nobody noticed it happening: a well-built layer with no callers reads, at a glance, like a
capability the product has. Someone who greps `plotBox` and finds a tested module will
reasonably conclude the charts are framed. They are not. The tier-2 numbers on the *product*
move when a render path calls `frameSeries`, not when this module lands.

It is logged at 🟡 rather than 🟠 because, unlike DC-L08, the gap is one call site rather than
an unreachable subsystem — the module is pure, exported, and has a worked example in
`SceneBuilder.test.ts`.

**Re-check.**
```bash
# 1 — the primitive exists, is exported, and is covered
grep -c 'export function frameSeries' packages/dc-wasm/src/chart/plotbox.ts        # -> 1
grep -c 'from "./chart/plotbox"' packages/dc-wasm/src/index.ts                     # -> 2
npx vitest run packages/dc-wasm/src/chart/plotbox.test.ts                          # -> 32 passed

# 2 — one app file references it now, and it is the AXIS, not the framing
grep -rl 'frameSeries\|fitToPlotBox\|plotBox' apps/ --include=*.ts --include=*.tsx
# -> apps/showcase/src/chrome/engineAxis.ts   (ENC-1253: the furniture is laid
#    out against plotBox(canvas); the DATA is still on the view's baked literal,
#    which is what this entry is about and is unchanged)

# 3 — its only package-side consumer is SceneBuilder ...
grep -rl 'frameSeries\|fitToPlotBox\|paneRegionFor' packages/ --include=*.ts \
  | grep -v 'chart/plotbox' | grep -v 'index.ts'          # -> packages/dc-wasm/src/chart/SceneBuilder.ts

# 4 — ... which is itself never constructed outside a test (the DC-L08 shape)
grep -rl 'new SceneBuilder' apps/ packages/ --include=*.ts --include=*.tsx \
  | grep -v '\.test\.' | wc -l                            # -> 0

# 5 — the showcase still bakes a literal, so its framing is unchanged
grep -h '"transform"' apps/showcase/views/candles-aapl/view.json
# -> "transform": { "sx": 0.011333333, "sy": 0.10625, "tx": -0.895333333, "ty": -43.88125 },
```

**Working around it.** To frame a chart today you call it yourself:

```ts
const framed = frameSeries(tracker.domain(), { width: canvas.clientWidth, height: canvas.clientHeight });
host.applyControl({ cmd: 'setPaneRegion', id: PANE, ...framed.paneRegion });
if (framed.transform) host.applyControl({ cmd: 'setTransform', id: TRANSFORM, ...framed.transform });
```

Re-run it when the domain moves or the canvas resizes — nothing recomputes it for you. Do not
read `checkTier2Framing` passing in a unit test as the product being framed; score the delivered
raster (SPEC D10) for that.

**Ticket.** **ENC-1273** — adopt the plot box on the showcase render path (`useViewSwitch` +
the chrome overlay's tick mapping, which must map through the SAME transform or the ticks and
the geometry will disagree). customer-layer adoption is separate and unticketed. **ENC-1253**
(engine-drawn axis marks) is the first intended consumer of `gutters()`.

**Update, 2026-09-20 (ENC-1253).** `plotBox()` now has its first app caller —
`apps/showcase/src/chrome/engineAxis.ts` lays the axis furniture out against it. That does NOT
retire this entry, and the distinction is the whole point: the furniture knows where the frame
is, the DATA still does not go there. Every `view.json` still bakes its `transform`, no render
path calls `frameSeries`, and `grep -rl 'new SceneBuilder' apps/ packages/ --include=*.ts
--include=*.tsx | grep -v '\.test\.'` is still empty. The visible consequence, in the
ENC-1253 capture: the leftmost bars are drawn to the left of the plot box's left edge, under the
price labels. Adoption for the data is still **ENC-1273**.

**Verified at** `ENC-1253 HEAD`, 2026-09-20 — commands 1, 3, 4 and 5 re-run unchanged in the
ENC-1253 worktree; command 2's expected output is restamped above.

---

## DC-L15 — Every committed showcase still is vertically MIRRORED: all 23 predate the ENC-696 blit fix 🔴

**Claim.** The 23 PNGs in `apps/showcase/stills/` were all captured in one commit — `537c995`,
**2026-06-11** — and the Y-orientation fix they needed landed in `d6b5acd`, **2026-06-21**, ten
days later (§R L2, DC-L05). Every one of them is therefore an **upside-down** picture of a
correct render. They are the *only* rendered evidence several documents reason from, and nothing
in the repo says they are stale.

This is not a claim about the renderer. `EngineHost.blitFramebuffer` flips rows today
(`packages/dc-wasm/src/EngineHost.ts:918-934`, regression test `EngineHost.blit.test.ts`) and
the engine's own rect geometry is right — `instructions.json` puts the baseline in `y0` and the
value in `y1`, and `instancedRect@1` fills between them. The stills simply predate the fix.

**Why it bites — it already did.** `specs/2026-09-19-chart-quality-bar/SPEC.md` §1.1 reads
`price-line-area.png` as *"the area is filled on the wrong side of the line… the dark silhouette
is the price series"* and scores the view **tier 0 (Truthful) FAIL**. §5 Q6 then made that a
locked open question. Both are artifacts of the mirror: the green mass **is** the fill, the dark
region **is** the complement, and the whole frame is flipped. §5 Q1 checked the right things —
the blit flip is intact, `sy` is positive — and drew the wrong conclusion, because it compared
the still against *today's* code instead of against the code that produced it. See **§C6**.

The general trap: a mirrored random walk still looks like a random walk, and the axis numbers
beside it are a hand-typed DOM overlay (SPEC §1.3), so **nothing in the frame contradicts the
mirror**. Orientation cannot be eyeballed off these images at all; it has to be measured against
the data the capture replayed.

**Re-check.**
```bash
# 1 — every still is from one pre-fix commit, and the fix came later
git log -1 --format='%h %ad' --date=short -- apps/showcase/stills/   # -> 537c995 2026-06-11
git log -1 --format='%h %ad' --date=short -S 'fbH - 1 - y' \
  -- packages/dc-wasm/src/EngineHost.ts                              # -> d6b5acd 2026-06-21
for f in apps/showcase/stills/*.png; do \
  git log -1 --format='%ad' --date=short -- "$f"; done | sort -u     # -> 2026-06-11 (only)

# 2 — the mirror, measured on price-line-area.png against its own records.json.
#     Fit the green fill's lower boundary against the price each column replayed:
#     view.json's sy=0.121428571 over H=600 predicts -36.43 px/$ upright and
#     +36.43 px/$ mirrored.
python3 apps/showcase/tools/still-orientation.py price-line-area   # exit 1 == mirrored
# -> fit       : slope +36.40 px/unit   r = +0.9895   median|resid| = 0.3 px
# -> predicted : upright -36.43   mirrored +36.43   (sy=0.121428571, H=600)
# -> VERDICT   : MIRRORED

# 3 — the engine itself is correct (this is the control for 2)
bash scripts/tier0.sh          # case C: C2/C3/C4 hold on the presented raster
```

**Working around it.** Do not score, measure or cite a committed still. Treat
`apps/showcase/stills/` as a 2026-06 contact sheet of *what was drawn*, not of *how it looked*;
anything vertical read off one is inverted. For a claim about orientation or fill direction use
`scripts/tier0.sh` (which asserts on the presented raster and ships the mirror as a negative
control). Recapturing the gallery is **ENC-1250's follow-up, not ENC-1250** — it rewrites 23
binary artifacts and needs the capture harness, whose `EMBASSY_REPO` path does not exist on this
machine (`apps/showcase/tools/capture.mjs`); `apps/showcase/stills/README.md` marks the
directory stale until then.

**Ticket.** Recapture + the SPEC correction: **ENC-1276**. **Verified at `2423de6`, 2026-09-19**
— fit measured on both adapters the tier-0 control run used (llvmpipe, and NVIDIA GeForce
RTX 3070 Ti / NVK GA104).

---

## DC-L16 — Nothing on the market-data path carries a timestamp, so a time axis is the client's *observation* time 🟠

**Claim.** As of ENC-1254 the x axis of the market views renders time rather than `INDEX`
(SPEC D1 tier 1). The time it renders is **when this client saw the record**, not when the bar
closed upstream — because no timestamp exists anywhere on the path to render. Four independent
places drop it:

1. **GMA_V3 does not emit one.** A value update is exactly four keys — `type`, `id`,
   `streamKey`, `value` (`GMA_V3/src/ws/WSResponder.cpp`). `StreamValue` itself has no time
   slot (`GMA_V3/include/gma/StreamValue.hpp`).
2. **The one node that computes a bar boundary throws it away.** `BucketTime` floor-aligns to
   a wall-clock period expressly so "a 1m bar means the same wall-clock window across the data
   plane" — and then emits `StreamValue{"", 0.0}` (`GMA_V3/src/nodes/BucketTime.cpp:70,72`).
   The aligned instant it just computed never enters the stream.
3. **embassy declares the field and never reads it.** `inboundProbe.Timestamp *int64` exists
   (`embassy/internal/gma/types.go:53`) and has **zero** readers; the handler signature
   forecloses it anyway — `type ValueHandler func(value float32)`.
4. **treaty's dataplane record has no time field**, and the binary record is
   `[1B op][4B bufferId][4B offsetBytes][4B payloadBytes]` + little-endian f32s. `x` is
   embassy's `recordIndex`, a uint32 counter.

And the lane could not carry one if it were wired: it is `float32` end to end, whose 24-bit
mantissa quantises an ms epoch (~1.7e12) to roughly **2-minute** steps.

**What ENC-1254 therefore does.** `IndexTimeTracker` (`packages/dc-wasm/src/chart/time.ts`)
fits `t = origin + recordIndex·msPerIndex` by least squares from the times at which the client
*observed* each record, and the fitted `TimeBasis` carries an **`epochKnown`** flag:

- `true` — a live socket stamping `Date.now()`. The labels are real wall-clock instants, and
  they are the time of *receipt*, not of the bar.
- `false` — the showcase, replaying a captured tape. The origin is the tape's own zero, the
  labels are rendered in UTC, and they describe the **capture's cadence** (≈75 ms/record), not
  a market interval. `candles-aapl`'s 267 bars therefore span 20 seconds of axis, not 267
  seconds of market.

`epochKnown` and `msPerIndex` are published on `data-dc-axis-domain` /
`window.__dcAxisDomain[viewId].x.time`, so the distinction is readable without trusting a label.

**Why it bites.** A reader who sees a clock on the x axis will assume it is market time. On the
showcase it is tape time; on the live product it will be arrival time. Both are true statements
about the stream and neither is the statement a trading chart normally makes — and because the
frame contains no second opinion, there is nothing to contradict the assumption (SPEC §1.3, the
same mechanism that let `INDEX 4→160` stand for six months).

**Re-check.**
```bash
# 1, 2, 3 and 4 — run from the WORKSPACE root (the parent of DynaCharting/)
grep -c 'w.Key(' GMA_V3/src/ws/WSResponder.cpp                    # -> 4  (type,id,streamKey,value)
grep -n 'onValue(StreamValue' GMA_V3/src/nodes/BucketTime.cpp     # -> StreamValue{"", 0.0} ×2
grep -rn 'Timestamp' embassy/internal/gma/ | wc -l                # -> 1  (the declaration; no readers)
grep -n 'type ValueHandler' embassy/internal/gma/client.go        # -> func(value float32)
grep -cE 'timestamp|epoch|time' treaty/proto/dataplane/v1/dynacharting.proto   # -> 0

# 5 — from the DynaCharting repo root: the capture's ONLY time is the frame's own offset
python3 -c "import json;d=json.load(open('apps/showcase/views/candles-aapl/records.json'));print(sorted(d['meta']),sorted(d['frames'][0]))"
# -> ['cadenceMs', 'durationMs', 'frameCount', 'viewId'] ['b64', 't']

# 6 — and the axis says so, rather than implying market time
npx vitest run apps/showcase/src/chrome/axisTicks.test.ts         # -> 16 passed
```

**Working around it.** Read `window.__dcAxisDomain[viewId].x.time.epochKnown` before quoting a
time off a chart. To get *market* time onto the axis, the producer has to state it: the pieces
already exist — `BucketTime` guarantees `recordIndex` is an exact, wall-clock-aligned bar
ordinal, and forum already authors the interval (`forum/db/seed/seed.go`, `candleWindowMs =
3000`) and ships it to GMA as opaque `pipeline_json` that embassy never parses. So
`t = base + recordIndex × periodMs` would be **exact** if `base` and `periodMs` were transmitted
once per buffer on the dataplane buffer spec. `TimeBasis.source: 'declared'` is the seam that
consumes them; nothing produces them yet. This is a `treaty` + `embassy` change, not a client
one — do not try to infer a bar interval in the browser.

**Ticket.** None yet for the wire change (it belongs to `treaty`/`embassy`, not this repo).
ENC-1254 landed the client half. DynaCharting's own `dc::TimeScale`
(`core/include/dc/scale/Scale.hpp`) already maps epoch-ms and needs a timestamp column that the
live path never produces — it is unreachable for the same reason.

**Verified at** `2423de6`, 2026-09-19 — every command above run by hand in the ENC-1254
worktree and against the sibling repos at their checked-out state. The `epochKnown: false` /
`msPerIndex: 74.9998` figures were read off the running showcase over CDP (headless Chrome,
`vendor: nvidia, architecture: ampere`, `info.isFallbackAdapter: false`, `subgroupMinSize: 32`,
`maxBufferSize: 2 GiB` — SPEC D8), not inferred from the tests. The float32-mantissa figure is
arithmetic, not a measurement.

## DC-L17 — A candle body has a 2 px floor, so a doji is not zero-height on screen 🟡

**Claim.** Since ENC-1251 `instancedCandle@1` floors the height it draws a candle body at:
`dc::kMinBodyHeightPx` = **2 device pixels** (`core/include/dc/render/CandleBodyFloor.hpp`),
applied in CLIP space per draw. So the drawn body is no longer a pure function of the record's
`open`/`close` — a bar whose body is thinner than 2 px is drawn at 2 px, centred on the
open/close level and slid back inside `low..high` when the wick has the room. This is the
vertical counterpart of **DC-L12** (ENC-1257's horizontal rule) and the same caveat applies:
the resolution is host/shader-side and per-draw, so it is invisible in the bytes —
`getBufferBytes()` and `getSceneDocument()` still report the authored `open`/`close`.

**Why it exists.** Before it, `open == close` made both body triangles degenerate — zero area,
zero fragments — so an ordinary **doji drew no body at all** and the bar read as a bare wick
with its open/close level missing. That is not rare: ENC-1251 wiretapped the live dataplane for
100 s (customer-layer → embassy `candles-v1` → GMA_V3 → feed-simulator; NEXO/`lastPrice`, 3 s
tumbling windows) and decoded the candle6 records that drew the chart — **17 of 146 (11.6%)**
carried `open == close` exactly. It is the answer to
`specs/2026-09-19-chart-quality-bar/SPEC.md` §1.0's open question, and it was a tier-0 failure:
the record carried an open and a close and the mark depicted neither.

**Why it bites.** Two ways.

1. **You cannot read a body's exact height off a dense or a flat chart.** Under ~2 px the
   drawn height is the floor, not the data. Measure body height from the records, never from
   the raster — the same rule DC-L12 states for width.
2. **`SvgExporter` has its OWN floor and it is a different one.** `core/src/export/SvgExporter.cpp:548`
   (`if (bodyH < 1.0) bodyH = 1.0;`) clamps to **1** unit in the exporter's own scaled space,
   not to 2 device pixels, and it predates ENC-1251 — it is why the exporter always drew dojis
   the renderer did not. The two paths now agree that a doji is visible and still disagree on
   how tall it is. (`SvgExporter` also still does not apply DC-L12's bar-sizing rule.)

**Re-check.**
```bash
cmake -B build-dawn -G Ninja -DDC_BUILD_TESTS=ON -DDC_FETCH_DAWN=ON
cmake --build build-dawn -j$(nproc) --target dc_enc1249_tier0_truthful
./build-dawn/core/dc_enc1249_tier0_truthful | grep 'doji-on-high'
#   doji-on-high    cx=102 wick[153..225]  bodyX=108 body[153..154]  rows high=152.9 open=152.9 close=152.9 low=226.3
#   PASS  D1 [doji-on-high] full extent at cx is low..high      got [153..225] want [152.9..226.3]
#   PASS  D2 [doji-on-high] the body is drawn at all            x=108 lit=2 px  (open == close: a DOJI)
#   PASS  D3 [doji-on-high] the body sits at the open/close level   mid=153.5 want=152.9 (+-3 px)
#   PASS  D4b [doji-on-high] the floored doji body stays thin   span=2 px (need 1..4)

# one floor, two shaders — the pick footprint cannot drift from the drawn one
grep -n 'constexpr float kMinBodyHeightPx' core/include/dc/render/CandleBodyFloor.hpp
#   -> 54:constexpr float kMinBodyHeightPx = 2.0f;
grep -c 'dcCandleBodyFloor' core/src/gpu/DawnInstancedCandleBackend.cpp core/src/gpu/DawnPickBackend.cpp
#   -> 1 and 1
```
`body[153..154]` is the whole entry: two rows where the record's open and close are the same
number. Before ENC-1251 that column read `body[-1..-1]` — nothing lit.

**Working around it.** Nothing to work around if you want a readable doji — that is the point.
For an exact pixel height, read the records. `kMinBodyHeightPx` is a single constant in
`CandleBodyFloor.hpp` if a caller genuinely needs a different convention; unlike ENC-1257's
`BarSizingConfig` it is **not** yet a per-draw parameter, because nothing has asked for one.
The rule is off entirely when the draw has no viewport (`viewH <= 0`).

**Ticket.** ENC-1251. `SvgExporter`'s divergent floor is real and unowned.

**Verified at** `ENC-1251 HEAD`, 2026-09-19 — every command above was run in the ENC-1251
worktree and produced exactly the output shown, on a hardware adapter
(`backend=Vulkan name="NVIDIA GeForce RTX 3070 Ti (NVK GA104)"` — SPEC D8). The wire figures
come from the decoded dataplane capture the tier-0 D case quotes verbatim.

---

---

## DC-L18 — A pane's clear colour paints over every pane created before it 🟠

**Claim.** WebGPU has no scissored mid-pass clear, so `DawnSceneRenderer` implements a pane's
clear colour as a **full-pane quad drawn inside the render pass**, bounded by the pane scissor
(`core/src/gpu/DawnSceneRenderer.cpp` → `clearPane`, ENC-511). Panes are walked in **scene
order**. Therefore a pane created LATER does not merely render on top of earlier panes' draw
items — its clear quad **erases them**, everywhere its region overlaps theirs.

There is no warning, no rejection and nothing in the error list. Every command succeeds; the
pixels are simply gone.

**How it bit (ENC-1253).** The engine axis lives in its own pane at `FULL_CLIP_REGION`, because
the data pane's region is the plot box and furniture drawn there is scissored away (plotbox.ts
contract note 7). The showcase creates the axis once and **re-applies the view's manifest on
every replay loop** (`useViewSwitch` → `resetScene` + `applyManifest`, roughly every 20s), which
makes the view's pane newer than the axis pane. Its clear — `±0.95`, i.e. most of the canvas —
then covered the lot. Measured: 8 gridlines, 8 tick marks, 2 spines and 10 labels all issued,
`applyControl` rejections **zero**, the published plan reporting `tier1.pass`, and the canvas
containing nothing but candles.

Note the shape: this is the *same* failure mode as the pane-scissor trap one level down — the
correct commands, accepted, producing no pixels — and the two have opposite fixes (be inside the
region / be after the pane).

**Re-check.**
```bash
# 1 — the clear is a drawn quad inside the pass, walked in scene order
grep -n 'clearPane(\*pane' core/src/gpu/DawnSceneRenderer.cpp
# -> 342:      clearPane(*pane, pane->clearColor);
grep -n 'in scene order' core/src/gpu/DawnSceneRenderer.cpp
# -> 325:  // Walk all draw items: pane (scissor) -> layer -> drawItem, in scene order.

# 2 — and the showcase really does re-create its pane on every loop
sed -n '/^function applyView/,/^}/p' apps/showcase/src/views/useViewSwitch.ts
# -> resetScene(host, prev); applyManifest(host, view.manifest); bakeTransform(...)
grep -n 'resetAndReplay()' apps/showcase/src/views/useViewSwitch.ts   # -> the loop calls it
```

**Working around it.** Create your overlay pane AFTER every pane it must sit on top of, and
**re-create it whenever those panes are re-created**. `useEngineAxis` watches
`useViewSwitch`'s `sceneEpoch` and does exactly that: `dispose()`, `IdAllocator.reset()` (so the
ids are reused rather than walked), then re-sync. Do not reach for z-order — there is none; the
only ordering the renderer has is creation order.

**Ticket.** None. The workaround is cheap and correct, and the alternative — an explicit pane
z-order, or a clear that does not overwrite — is a renderer design change nobody has asked for.
Raise one if a second consumer hits it.

**Verified at** `ENC-1253 HEAD`, 2026-09-20 — both greps run in the ENC-1253 worktree; the
symptom was observed and then removed on a canvas-only capture of the showcase, hardware adapter
(`vendor: nvidia, architecture: ampere`, `info.isFallbackAdapter: false` — SPEC D8).

---

## DC-L-1265 — The HUD's `ms` is CPU encode time; this engine measures no GPU time at all 🟠

**Claim.** ENC-1265 turned `dc::Stats::frameMs` (declared 2026-05, **assigned by nothing**,
displayed as `1000 / fps`) into `dc::Stats::renderCpuMs`, a real wall-clock measurement. Be
precise about what survived: it is **CPU wall-clock from the top of
`DawnSceneRenderer::render` to `device_->endRenderPass()`** — the scene walk, the per-pipeline
draw encoding, and the queue submit. `DawnDevice::endRenderPass` is `pass_.End(); encoder_.Finish();
queue_.Submit(...)` and **nothing else**: no `onSubmittedWorkDone`, no fence, no wait. There is
no timestamp query anywhere in `core/`.

So a badge reading `1 fps · 3.2 ms cpu` says *the CPU-side encode took 3.2 ms*. It does **not**
say the GPU was idle, and it does not say where the other ~997 ms went. On the browser path
`readbackMs` (the full-target RGBA8 copy, `DcEngineStats::readbackMs`) accounts for one more
slice, and the rest — GPU execution, the `putImageData` blit, and every other main-thread job
between rAF ticks — is still **unmeasured**. `fps` remains the main-thread rAF callback rate,
not a render rate, exactly as PERF-CLAIMS **C3** describes.

Two smaller edges of the same instrument:

- **Browser-side resolution is the browser's.** Under Emscripten `std::chrono::steady_clock`
  resolves to WASI `clock_time_get` → `emscripten_get_now()` → **`performance.now()`**, which
  Chromium coarsens as a Spectre mitigation. A scene walk finishing below that clamp reads
  `0.0`, and the HUD then shows `–` rather than a number. That is deliberate — ENC-1265 removed
  the `1000 / fps` fallback, so "no measurement" now prints as no measurement — but `–` means
  *below the clock's resolution*, not *instant*.
- **The renderer-side assignment is inside DC-L01's 47.** `stats.renderCpuMs = …` lives in
  `dc_gpu`, and the only test that asserts it produced a plausible number
  (`d50_dawn_scene_renderer`, ENC-1265 block) needs `-DDC_FETCH_DAWN=ON`. What runs in the
  default build is `dc_enc1265_render_timing` **[5]**, a source gate: it fails if any `*Ms`
  field of `dc::Stats` / `DcEngineStats` is assigned nowhere under `core/src` or `core/wasm`.
  That gate catches *the original bug's shape* — a declared-but-never-written timing field — and
  it cannot see whether the value is sane.

**Re-check.**
```bash
# 1 — no GPU timing exists anywhere in the engine
grep -rniE 'writeTimestamp|timestampWrites|QuerySet|timestamp-query' core/src core/include \
     --include=*.cpp --include=*.hpp | wc -l
# -> 0

# 2 — the measured span ends at the submit, and the submit does not wait
grep -n 'renderCpuMs = renderClock' core/src/gpu/DawnSceneRenderer.cpp   # -> 450:
awk '/void DawnDevice::endRenderPass/,/^}/' core/src/gpu/DawnDevice.cpp
# -> pass_.End(); encoder_.Finish(); queue_.Submit(1, &cmd);   (no wait, no callback)

# 3 — the browser clock is performance.now()
node -e 'const fs=require("fs");const m=new WebAssembly.Module(fs.readFileSync(
  "packages/dc-wasm/wasm/dc_engine_host.wasm"));console.log(
  WebAssembly.Module.imports(m).map(i=>i.module+"."+i.name)
    .filter(n=>/now|time|clock/i.test(n)).join("\n"))'
# -> wasi_snapshot_preview1.clock_time_get
grep -n '_emscripten_get_now = () => performance.now()' packages/dc-wasm/wasm/dc_engine_host.js
# -> 3434:  var _emscripten_get_now = () => performance.now();

# 4 — the sane-value assertion is NOT in the build you ran (DC-L01)
cmake -B build -DDC_BUILD_TESTS=ON >/dev/null && cmake --build build -j$(nproc) >/dev/null
ctest --test-dir build -N | grep -c d50_dawn_scene_renderer   # -> 0
ctest --test-dir build -N | grep -c dc_enc1265                # -> 3  (test + 2 negative controls)
```

**Working around it.** If you need to know whether the GPU is the bottleneck, this engine
cannot tell you — use the browser's own profiler, or add a `timestamp-query` path (no ticket).
For the CPU/readback split that *is* available, read `EngineStats.renderCpuMs` and
`EngineStats.readbackMs` off `EngineHost.getStats()`; the HUD badge deliberately shows only
`renderCpuMs`, because three numbers in a 14 px badge is how the referent got lost the first
time.

**Ticket.** [ENC-1265](https://linear.app/encultured/issue/ENC-1265) is this change. GPU
timestamp queries have no ticket — raise one if a real bottleneck question needs them.

**Verified at** `ENC-1265 HEAD`, 2026-09-20 — commands 1-3 run in the ENC-1265 worktree;
command 4's counts are `196` registered of `243` declared (the **47-test gap is unchanged** —
ENC-1265 adds three tests to the default build and one assertion block to an existing Dawn test).

---

# §C — Corrections

Beliefs that were held confidently and were wrong. They are here because each one cost real
time, and because a reader who half-remembers the wrong version needs to find the correction.

### C0 — ENC-558 fixed the stale-GPU-buffer cache in every Dawn backend except `textSDF@1`

ENC-558 (and ENC-569 for `line2d@1`) taught the Dawn backends to remember the `CpuBufferStore`
**version** each cached GPU instance buffer was built from, and to re-gather when it moves —
which is what lets a streaming series keep animating past the first frame. Six backends carry
the comment; `DawnTextSdfBackend` did not. Its `ensureGeoBuffers` was keyed on `geometryId`
alone and returned the first upload forever, glyph **count** included.

Nothing noticed for four months because **nothing had ever re-laid out text**. The browser's
only text caller was a one-shot demo; the C++ recipes that build axis labels are unbound in the
WASM module (`strings dc_engine_host.wasm | grep -ci recipe` → `0`). A cache that is only ever
written once cannot be observed to be stale.

ENC-1253 was the first caller to re-lay out a label — an axis whose numbers track a measured
domain — and the symptom was a chart whose gridlines moved while its numbers did not: the plan
said `$410 … $418` with three time labels, and the canvas showed `$414 … $418` with two, from
several seconds earlier. The fix is the ENC-558 pattern, ported verbatim
(`core/src/gpu/DawnTextSdfBackend.cpp` `buildGeoBuffers` + the version check in
`ensureGeoBuffers`), and it is in the committed wasm.

Re-check: `grep -c 'vtxVersion' core/include/dc/gpu/DawnTextSdfBackend.hpp` → `1` (it was `0`),
and `grep -l 'vtxVersion' core/include/dc/gpu/Dawn*.hpp | wc -l` → `9`, i.e. every geometry
backend now carries it.

The general lesson is the one this file keeps relearning: **a capability with one caller is a
capability with no coverage.** DC-L08 and §1.2 of the chart-quality SPEC are the same shape, and
in-engine text was in it — "unused, not unbuilt" (ENC-1260) turned out to also mean "unused, and
broken in a way only use could reveal".

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

### C5 — "the high lands near the framebuffer top" — a test file asserted the opposite of DC-L05

`core/tests/d14_6_dawn_candle.cpp`'s **file header** said that with an identity transform "the
HIGH (large y) lands near framebuffer TOP and the LOW near framebuffer BOTTOM". Its own inline
comment forty lines later said the true thing — low near the top, high near the bottom — and
**DC-L05 is the general statement of it**: every Dawn backend negates clip y while the readback
is top-down, so the raw framebuffer is vertically MIRRORED relative to what a user sees.

Nothing caught the contradiction because the test's assertions are up/down **symmetric**: it
probes for a lit wick pixel above *and* below the body, in the same colour, so an upside-down
frame satisfies every one of them. That is DC-L05's own warning ("silent on any vertically
symmetric scene") reproduced inside a test whose subject is exactly that orientation.

Measured under ENC-1249 on lavapipe: an authored clip y of `+0.80` reads back at row **230** of
256 (the bottom) and lands at row **26** (the top) only after the flip
`EngineHost.blitFramebuffer` applies. The header was corrected in the same commit. Re-check:
```bash
bash scripts/tier0.sh          # B1/B2 spans hold on the presented raster and fail on --invert-render
```

**The general lesson, and it is the reason ENC-1249 exists:** a pixel assertion has to name
*which raster* it is about. Asserting on the raw readback would have enshrined the mirror in the
quality standard and "proved" that a rising series falls.

### C4 — `CHART_AUTHORING.md` §8/§9 used to promise GPU gradient fills that do not render

Until ENC-991, §8 ("Apply a gradient to any DrawItem… this creates gradient area charts") and
§9 ("Dashboard panels — `instancedRect@1` with `cornerRadius` and gradient fills") described
behaviour that does not exist on the GPU path — see DC-L04. Anyone following the guide would
have authored a chart that silently rendered flat. Both passages were corrected in the same PR
as this file, and now point at DC-L04 and at the `triGradient@1` workaround.

Worth noting as a pattern: the authoring guide and the limitations log had drifted into
contradicting each other, and the guide was the one people read.

### C6 — "`price-line-area` fills on the wrong side of the line" — it does not; the still is upside down

`specs/2026-09-19-chart-quality-bar/SPEC.md` §1.1 scored `price-line-area` a **tier-0 (Truthful)
failure** — *"the area is filled on the wrong side of the line. The dark silhouette is the price
series; the green mass is everything above it"* — and §5 Q6 promoted that to a locked open
question. It was wrong. The green mass **is** the fill and the dark region **is** the complement;
the frame is vertically **mirrored**, because every committed still was captured at `537c995`
(2026-06-11) and the `EngineHost` blit flip landed at `d6b5acd` (2026-06-21), ten days later.

The engine was never at fault. `instruction.json` puts the baseline in `y0` and the streamed
value in `y1`, `instancedRect@1` mixes `y0..y1`, and the baked `sy` is positive — all three of
which §5 Q1 checked and found correct. **Q1's error was comparing a 2026-06 artifact against
2026-09 source** and concluding the mirror had been ruled out. The fix it verified was real; it
just postdated the picture.

Why nobody caught it for three months: **a mirrored random walk is still a random walk.** The
only numbers in the frame are a hand-typed DOM overlay (SPEC §1.3), so they flip with the image
and agree with it either way. There is no feature of the picture that contradicts the mirror —
which is exactly DC-L05's "silent on any vertically symmetric scene" one level up, at the level
of a whole chart instead of a single mark. Orientation had to be measured against the data the
capture replayed, and the measurement is decisive because the two hypotheses differ in **sign**:
`view.json`'s `sy = 0.121428571` over `H = 600` predicts **-36.43 px/$** upright and
**+36.43 px/$** mirrored, and the fill boundary fits **+36.40 px/$** at `r = +0.9895`.

Re-check:
```bash
python3 apps/showcase/tools/still-orientation.py price-line-area   # -> VERDICT : MIRRORED
bash scripts/tier0.sh                                             # -> case C: the ENGINE fills correctly
```

**The lesson, and it is DC-L09's lesson arriving somewhere new:** a rendered artifact carries the
date of the code that drew it, not the date you look at it. Before reading a bug out of a
committed image, check whether the image predates the fix — `git log -1 -- <image>` against
`git log -1 -S<the fix> -- <source>` is the whole test, and it cost three months here.
See **DC-L15**.

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

**Seven things this file does differently.**

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

7. **The id is allocated by Linear, not by the author (ENC-1277).** The sequential
   `DC-Lnn` space **`DC-L01`…`DC-L18` is CLOSED**. Every entry added after ENC-1277 is
   **`DC-L-<its ENC ticket number>`** — `DC-L-1277`, `DC-L-1301`. You do not pick it, you do
   not check whether it is free, and there is nothing to race for: Linear already allocated it
   and it is unique for the same reason the ticket id is.

   *Why the old scheme could not be repaired.* Sequential allocation is racy **by
   construction**, and the window is the whole life of a branch — not the moment you look. It
   collided three times in two days, and every one of those authors checked first (one grepped
   every `enc-12*` remote branch, not just `main`):

   | Colliding tickets | Id |
   |---|---|
   | ENC-1252 / ENC-1257 | `DC-L12` |
   | ENC-1250 / ENC-1254 / ENC-1256 | `DC-L14` (three ways) |
   | ENC-1251 / ENC-1253 | `DC-L17` |

   **Twice there was no conflict at all.** Git auto-merged the two entries into different parts
   of the file and left two identical headings coexisting with no marker and no error. And the
   resolution is its own hazard: taking `--ours` wholesale on this file once silently dropped
   ENC-1257's entire `DC-L12`, including the only record that `SvgExporter` does not apply the
   bar-sizing rule. A human reading the diff caught it; nothing else would have.

   *Why the existing eighteen were NOT renumbered.* Device 6 above is the reason — an id is a
   citation target, and 234 of them exist across 35 files in **two repos** (124 in this one, 110
   under the workspace's `specs/`), which by the workspace guardrail is two tickets and two PRs
   with a window where half the citations dangle. Renumbering also cannot reach the citations in
   merged commit messages, PR bodies and Linear comments at all. And it is not even *defined*:
   **ten of the eighteen entries say `Ticket. None`**, and `DC-L01`…`DC-L09` all arrived in a
   single commit (`e95a79d`, ENC-991), so numbering them by their ticket would produce a
   **nine-way collision** — the exact defect being fixed. Entries that are already merged cannot
   collide with anything; only future ones can, and those are the ones the new scheme covers.

   *The separator is load-bearing.* `DC-L-1277`, not `DC-L1277`. Without the hyphen, `grep
   DC-L12` matches `DC-L1277`, and ten of the eighteen legacy ids (`DC-L10`…`DC-L18`) are
   prefixes of some four-digit ticket. A citation lookup that silently returns an extra entry —
   or a gate that counts one — is the same class of quiet wrong answer as everything else in
   this file.

   *And it is checked, not merely written down.* `scripts/check-limitation-ids.sh` fails when
   two entries share an id, when an id that existed in `origin/main` has vanished (the `--ours`
   drop), or when a heading's id is malformed. `scripts/limitation-ids.test.ts` runs it inside
   **`pnpm test`** — the only gate in this repo that needs no Dawn, no GPU and no build, so it
   is the only one everybody actually runs. `ctest` was the alternative and DC-L01 disqualifies
   it. Paste it any time:

   ```bash
   bash scripts/check-limitation-ids.sh   # 0 clean, 1 violation, 2 could-not-run
   ```

**Adding an entry** — a checklist, not a ceremony:

- [ ] Verify it against current `main` yourself. Someone else's earlier assessment is a lead,
      not evidence.
- [ ] Write the `Re-check` command and **run it**. Paste the output you actually got.
- [ ] Stamp `Verified at <sha>, <date>`.
- [ ] Give it the id `DC-L-<your ENC ticket number>` (device 7 — do **not** pick the next free
      `DC-Lnn`; that space is closed), a severity, and a ticket — or say "None", explicitly.
- [ ] Run `bash scripts/check-limitation-ids.sh`. `pnpm test` runs it too, so a bad id fails
      the gate whether or not you remember.
- [ ] Say what the **workaround** is. An entry with no workaround and no ticket is a complaint.
- [ ] Add a pointer from wherever someone would hit it.

**Retiring an entry:** move the row to §R with the fixing commit and ticket, and if a residual
survives, open the new entry and link them in both directions (DC-L05 and DC-L06 are the worked
examples). If the entry was not merely fixed but *wrong*, it belongs in §C as well — that is the
case the old L4 taught us.

**Severity.** 🔴 will silently produce a wrong result or a false green. 🟠 will cost you an
afternoon. 🟡 is a sharp edge you should know about before you hit it.
