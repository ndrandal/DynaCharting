# DESIGN — Showcase UI direction (T5.0 / ENC-540)

**Status:** Design direction. No code. This is the brief the UI agents (T5.1–T5.7) build against.
**Scope:** the public-facing case-study / POC for the *DynaCharting Capabilities Showcase — "Frontier"*.
**Reads against:** `PLAN.md` (the ~17-view native→composed→walled program + frontier map) and `CONTRACT-buffer-id.md` (per-view metadata: tier, referenceTool, explainer, buffer/pipeline facts). The scaffold already establishes a near-black bg + green accent (`apps/showcase/src/App.css`); this doc formalizes and extends it.

**The bar (from PLAN):** *"looks designed and is self-explanatory — each view tells a viewer what they're looking at and why it's interesting."* Every decision below serves that sentence. Two failure modes to design against: (a) a wall of pretty charts with no thesis, (b) a thesis nobody can see because the charts aren't legible. We solve (a) with the **narrative arc** and the **frontier map**; we solve (b) with the **explainer panel**.

---

## 0. The one-line thesis (say it everywhere)

> **A data-aware GPU charting engine, driven entirely by JSON manifests, over a faithful market-data path — pushed until exactly one wall remains.**

The whole site is an argument for that sentence. The hero asserts it, the gallery proves the breadth, and the **frontier map is the QED** — it shows where JSON-manifest expressiveness ends and *why the remaining wall (real-time GPU compute / custom shaders) is the right next investment.* The frontier map is the climax, not an appendix.

---

## 1. Narrative arc

Four acts, in order, scroll-and-route. The visitor should be able to stop after any act and still have gotten the point — but the payoff lands only if they reach Act III.

### Act I — Hero / frame (`/`)
A single screen that does three jobs in ~5 seconds:
1. **Assert the thesis** (the sentence above, as the H1).
2. **Prove it's live, not a screenshot** — a real engine view renders *behind/beside* the headline, already streaming the 20s replay. Motion is the proof of life. Pick a flagship view (candlestick + volume + EMA) so even a finance-literate skim reads as "real market data, real chart."
3. **Show the spine** — a slim inline diagram of the data path: `Mock GMA → embassy → dataplane WS → dc-wasm / WebGPU`. This is the "faithful market data path" claim made visible and honest. Two CTAs: **Explore the gallery** (primary) and **See the frontier** (secondary — plant the climax early).

Tone: confident, specific, no marketing fog. The hero says *what it is and how it works*, not *how amazing it is*.

### Act II — The gallery (`/gallery`, `/view/:id`)
The breadth argument. ~17 live views, **grouped by tier** (native → composed → walled), each a card. The ordering is itself the narrative: *"first the things any chart engine does (native), then the things that look impossible-from-JSON but aren't (composed), then the honest edge (walled)."* Tier grouping is load-bearing — it's the gallery telling the same story the frontier map will tell, just in list form.

Clicking a card enters **single-view mode**: the live canvas at full size with the **explainer panel** docked beside it. This is where "self-explanatory" is won or lost — see §3 (explainer) and §5 (copy).

### Act III — The frontier map (`/frontier`) — THE PAYOFF
The intellectual climax. Not a table; a **map**. It plots all ~17 views across the three tiers and makes one argument visible: *the JSON manifest can express far more than you'd guess — the wall has been pushed back to a single sharp edge.*

The emotional beat: the eye travels left-to-right across native and composed (a wide, full field — "look how much rides on plain JSON + upstream precompute") and then hits the **walled band**, which is deliberately narrow and singular. The copy at that edge names the one remaining wall precisely:

> Everything to the left is a JSON manifest over a pure data path. The wall on the right is the same shape every time: **computation that must happen on the GPU, per-pixel, in real time** — live FFT, live KDE density, live marching-squares, glow. We render the *precomputed* version (texture-fed, purity preserved) and mark the *live-GPU* gap. **That gap is the custom-WGSL-pipeline-from-JSON decision.** That's why this wall matters: it's the one thing the manifest model can't currently buy, and the map shows exactly how much it would unlock.

The map must make the visitor *feel* that the wall is small and specific — that's the whole point. A frontier that looks empty is a failure; a frontier that looks *nearly conquered, with one clearly-named redoubt* is the win.

### Act IV — CTA / close
After the map: a short, plain close. What this proves (manifest-driven breadth + a documented edge), the link to the **frontier report** (the empirical artifact, T6.3), and a "how to add a view" pointer (T6.5) for the technical reader. No newsletter, no fake urgency. One or two real links. The CTA is *"read the evidence"*, not *"sign up."*

---

## 2. Visual language

**Feel:** precise, technical, premium. Bloomberg's data density and tabular discipline × Linear/Vercel-docs restraint and whitespace. Dark, near-black, one confident green accent, monospaced numerics everywhere a number appears. Charts are the brightest thing on screen — chrome recedes, data advances.

### Palette (build on the scaffold — these extend `App.css`)

Dark surfaces, layered by elevation. The scaffold's `#0b0d12` / `#4caf50` are kept; the green is nudged slightly brighter for accent contrast on dark, with the scaffold value retained as the "calm" accent.

| Token | Hex | Use |
|---|---|---|
| `--bg-0` | `#0b0d12` | app background (scaffold) — the void behind everything |
| `--bg-1` | `#0f1218` | raised surface (cards at rest, gallery grid) |
| `--bg-2` | `#11141b` | panels, titlebar, explainer (scaffold gradient top) |
| `--bg-3` | `#161a23` | hover / active surface, chips |
| `--line` | `#1d212b` | hairline borders (scaffold) |
| `--line-strong` | `#2a3040` | emphasized dividers, card borders on hover |
| `--text-hi` | `#e6e8ec` | headings, primary copy (slightly cooler than scaffold `#e0e0e0`) |
| `--text` | `#b3b8c2` | body copy |
| `--text-dim` | `#8a8f99` | captions, status, metadata (scaffold) |
| `--text-faint` | `#5b616e` | disabled, ghost numerics |
| `--accent` | `#3ddc84` | **the green** — primary accent, CTAs, "live" pulse, native tier |
| `--accent-calm` | `#4caf50` | scaffold green — quieter accent (links, focus rings) |
| `--accent-ink` | `#06140c` | text on a filled accent button |

**Semantic / tier + state colors** (each tier owns a hue — this is doing real semantic work, not decoration):

| Token | Hex | Meaning |
|---|---|---|
| `--tier-native` | `#3ddc84` (green) | native — the engine does this directly |
| `--tier-composed` | `#4ea3ff` (blue) | composed — JSON + upstream precompute / scalar-fan / texture-feed |
| `--tier-walled` | `#f0a830` (amber) | walled — renders precomputed; the live-GPU version is the wall |
| `--state-error` | `#ef5350` | error (scaffold) |
| `--state-warn` | `#f0a830` | warning (== walled amber, intentional) |
| `--state-ok` | `#3ddc84` | ready / settled (== accent) |

Rationale: **green = native = "yes, directly"**, **blue = composed = "yes, by routing/precompute"**, **amber = walled = "not live yet — here's the honest edge."** Amber, not red: walled is *interesting*, not *broken*. The visitor should read amber as "frontier," never "failure." Red is reserved strictly for runtime errors.

**Accent discipline:** green is precious. Use it for exactly one thing per screen (the primary action, or the live pulse, or the native tier — never all three competing). Blue and amber appear only as tier semantics. Everything else is greyscale. A screen with three saturated colors fighting reads as a dashboard, not a case study.

### Typography

- **UI / body font stack:** `'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif`. (Inter if bundled; the scaffold's system stack is the fallback and is acceptable for v1.)
- **Mono / numerics:** `'JetBrains Mono', 'SF Mono', 'Roboto Mono', ui-monospace, Menlo, Consolas, monospace`. **Every number, buffer ID, hex, pipeline name, and metric uses mono with `font-variant-numeric: tabular-nums`.** This single rule is most of what makes it read "Bloomberg-precise."
- **Scale** (1.25 ratio, rem):

| Token | Size | Weight | Use |
|---|---|---|---|
| `display` | 2.75rem / 44px | 650 | hero H1 |
| `h1` | 1.75rem / 28px | 620 | page titles (frontier, gallery) |
| `h2` | 1.25rem / 20px | 600 | section / tier headers, explainer title |
| `h3` | 1.0rem / 16px | 600 | card title |
| `body` | 0.9rem / 14.5px | 400 | explainer copy, prose |
| `small` | 0.8rem / 13px | 450 | metadata, chips |
| `micro` | 0.72rem / 11.5px | 500 | tier badges, labels — uppercase, `letter-spacing: 0.06em` |

- Headings: tight tracking (`letter-spacing: -0.01em`), the scaffold's `0.02em` reserved for the wordmark only.
- Line-height: 1.5 for prose, 1.2 for headings, 1 for numeric readouts.

### Spacing & form

- **4px base grid.** Spacing tokens: `4 / 8 / 12 / 16 / 24 / 32 / 48 / 64`.
- **Radii:** `6px` chips/badges, `10px` cards/panels, `8px` buttons. Nothing pill-round; this is technical, not playful.
- **Borders:** 1px hairlines (`--line`) are the default separator — borders, not shadows, define structure (Linear/Vercel idiom). Shadows used sparingly and softly (`0 1px 0 rgba(0,0,0,.4)` for the titlebar, a faint `0 8px 24px rgba(0,0,0,.35)` for the active card lift only).
- **Density:** generous around the hero and frontier map (room to breathe = premium); tight and tabular inside the explainer's fact block (density = credibility).
- **Motion:** restrained. 120–180ms ease for hover/active; the only ambient motion is the **live charts themselves** + a single 2s "live" pulse dot. No decorative parallax, no scroll-jacking. The charts move; the chrome holds still.

---

## 3. Component system

ASCII wireframes below are normative for layout intent, not pixel-exact.

### 3.1 App shell + nav

A persistent slim top bar (extends the scaffold `.showcase-titlebar`). Left: wordmark `DynaCharting · Frontier` (the `·` and "Frontier" in `--accent-calm`). Center/right: route nav `Gallery / Frontier / Report`, a global **replay control** (visible in single-view + frontier), and a connection status pill (the scaffold's dot, now labeled).

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ ◆ DynaCharting · Frontier        Gallery   Frontier   Report      ● Live  ⏯ 0:12│
└──────────────────────────────────────────────────────────────────────────────┘
```

- Status pill states: `● Live` (accent), `◌ Connecting` (dim, animated), `● Settled` (accent, after isSettled), `● Error` (red).
- Bar is `--bg-2`, 48px tall, bottom hairline `--line`, sticky.

### 3.2 View card (gallery grid)

The unit of the gallery. A live (or settled-thumbnail) preview, title, tier badge, reference-tool chip. Hover lifts (`--bg-3`, `--line-strong`, faint shadow) and reveals a "View →" affordance.

```
┌───────────────────────────────┐
│ ░░░░░░ live preview ░░░░░░░░░ │  ← canvas thumbnail or settled still
│ ░░░░░░ (the chart, moving) ░░ │     16:10, the chart is the hero of the card
│                               │
├───────────────────────────────┤
│ Candlestick + Volume + EMA    │  ← h3 title
│ ▢ NATIVE        ≈ TradingView │  ← tier badge (left) · reference-tool chip (right)
└───────────────────────────────┘
```

- Thumbnail: the live canvas where perf allows, else the settled still from T6.2 capture; never a flat icon. **A card with a frozen/blank chart fails the bar.**
- Reference-tool chip: `≈ <tool>` (the `≈` signals "in the spirit of," from `referenceTool`), `--text-dim`, mono, in `--bg-3`. It orients a viewer instantly ("oh, like a depth ladder in X").
- Card is `--bg-1`, 10px radius, 1px `--line`. Grid is tier-grouped (see §4).

### 3.3 Tier badge

Small, uppercase, mono-micro, a 2px left color bar + dot. Distinct color per tier (§2), and on hover/focus a tooltip with the one-line meaning. The badge is the same component everywhere (card, explainer, frontier map) so the color→meaning mapping is learned once.

```
 ▍● NATIVE     green   — the engine renders this directly from the manifest.
 ▍● COMPOSED   blue    — JSON manifest + upstream precompute (scalar-fan / texture-feed).
 ▍● WALLED     amber   — rendered from precomputed data; the live-GPU version is the frontier.
```

### 3.4 Explainer panel — the "self-explanatory" workhorse

This is the most important component on the site. When a visitor looks at any view, this panel answers — *without them having to ask* — **what am I looking at, and why is it interesting?** Docked beside the canvas in single-view mode. Top-to-bottom it reads as a tightening funnel: human framing → one-sentence thesis → the technical receipts.

```
┌─────────────────────────────────────────┐
│ Candlestick + Volume + EMA(20)           │  h2 title
│ ▍● NATIVE              ≈ TradingView      │  tier badge + reference-tool chip
├─────────────────────────────────────────┤
│ WHAT'S GOING ON                          │  micro label
│ Per-second OHLC candles for AAPL with a  │  body, 1–2 sentences.
│ volume sub-pane and a 20-period EMA      │  = DATA (what) + TECHNIQUE (how).
│ overlay — streamed tick-by-tick over the │
│ live data path and drawn as instanced    │
│ GPU geometry.                            │
├─────────────────────────────────────────┤
│ DATA            AAPL · 1s OHLCV · ~20s    │  ── the receipts: a tight, mono,
│ PIPELINE        instancedCandle@1 +       │     tabular fact block. Density here
│                 line2d@1                  │     = credibility. Every value mono,
│ WRITE MODE      compound (OHLC) + append  │     tabular-nums, dim labels / hi values.
│ BUFFERS         700 candle6 · 500 pos2    │
│ SOURCE          Mock GMA → embassy → WS   │
└─────────────────────────────────────────┘
```

Fields (all sourced from `view.md` / `view.json` / the buffer-id contract):
1. **Title** (h2) + **tier badge** + **reference-tool chip** — orientation in one glance.
2. **"WHAT'S GOING ON"** — the 1–2 sentence heart. **Formula: one clause of DATA (what market/field, what symbol, what cadence) + one clause of TECHNIQUE (how it's routed/composed/drawn).** This is the sentence that makes a view self-explanatory; §5 sets its voice.
3. **Fact block** — `DATA / PIPELINE / WRITE MODE / BUFFERS / SOURCE`, label–value rows, mono, tabular. For **composed** views add a `COMPOSED VIA` row (e.g. *"20-level scalar-fan → one fixed buffer"*) — this is where the "looks impossible-from-JSON but isn't" reveal lives. For **walled** views add a `THE WALL` row in amber (e.g. *"live KDE density needs per-pixel GPU compute — shown here precomputed"*). The fact block is what separates this from a dashboard tooltip: it shows the visitor the *mechanism*.

Panel is `--bg-2`, 10px radius, internal sections split by `--line` hairlines, ~360–400px wide docked.

### 3.5 Frontier-map visualization

The headline artifact (T5.5). A **map**, not a table. Three horizontal bands (tiers), left→right = increasing distance from "native." Each view is a node (a small live/settled thumbnail or a labeled dot) placed in its band; clicking a node deep-links to that view. The composed band is wide and full; the walled band is narrow and singular — the *shape* carries the argument.

```
┌──────────────────────────────────────────────────────────────────────────────┐
│  THE FRONTIER                                                                  │
│  How far a JSON manifest reaches — and the one wall left.                       │
│                                                                                │
│  NATIVE ──────────────┊ COMPOSED ─────────────────────────┊ WALLED ───────────│
│  green                ┊ blue                               ┊ amber             │
│  ● OHLC bars          ┊ ● depth ladder    ● treemap        ┊ ● GPU density     │
│  ● candle+vol+EMA     ┊ ● market profile  ● ridgeline      ┊ ● contour/isolines│
│  ● line/area/baseline ┊ ● footprint       ● streamgraph    ┊ ● spectrogram     │
│  ● price+RSI+MACD     ┊ ● renko / P&F     ● Sankey ribbons ┊                   │
│  ● scatter            ┊ ● radial season.  ● corr heatmap   ┊  ◀ the wall:       │
│                       ┊ ● weather radar   ● ECG / audio    ┊  real-time GPU     │
│                       ┊ ● sports shots                     ┊  per-pixel compute │
│  "directly"           ┊ "JSON + upstream precompute"       ┊  "shown precomputed"│
│                                                                                │
│  Reading the map:  ◀──────── what the manifest expresses ────────▶│ the gap ──▶│
└──────────────────────────────────────────────────────────────────────────────┘
```

- **Why-it-lands-there** is explicit: hovering/selecting a node shows the *reason* it's in that band (the `COMPOSED VIA` / `THE WALL` fact). The map isn't just placement — it's placement *with justification*, which is the whole frontier-report thesis rendered interactively.
- The walled band gets a **callout** (amber, pinned right): the precise wall statement from §1 Act III. This is the emotional climax — design it as the visual terminus the eye lands on.
- Optional toggle: **"render mode"** dot per node — filled = rendered live here, ◐ = rendered precomputed (walled). Makes "we render everything; one path is precomputed" legible at a glance.
- Mobile: bands stack vertically, same left→right reading reframed as top→bottom progression.

### 3.6 Replay controls

The 20s replay is the proof-of-life; give the visitor the transport. Compact transport in the top bar (global) + an inline scrubber in single-view mode.

```
  ⏮  ⏯  ━━━━━━━━●────────────  0:12 / 0:20   ↻ loop
```

- Play/pause, restart, a scrubber bound to the replay timeline, elapsed/total (mono, tabular), loop toggle (default on — ambient motion). Accent fill on the played portion. Keyboard: `space` = play/pause, `r` = restart (T5.7).

### 3.7 Loading / empty / error / unsupported states

Each state is designed, on-brand, and *honest* — never a raw spinner on void.

- **Loading:** skeleton card/canvas in `--bg-1` with a slow accent shimmer + label `Compiling manifest…` / `Subscribing to data path…` (the labels reinforce *how it works* even while waiting). Tie the "settled" transition to `window.__dcEngine.isSettled()` (from T6.2), not a fixed timeout.
- **Empty** (e.g. filtered gallery): centered `--text-dim` line `No views match.` + a reset link. Never a blank panel.
- **Error:** red dot + a plain statement (`Couldn't reach the data path.` / `Manifest failed to apply.`) + a `Retry` button + a tiny mono detail line for the curious. Errors are matter-of-fact, never alarming — the tone stays confident.
- **WebGPU unsupported** (T5.7): a designed fallback panel, not a broken canvas — `This showcase renders live via WebGPU. Your browser doesn't support it yet.` + the captured **still images** (T6.2 contact sheet) so the case study is *still legible without a GPU*. The explainers and frontier map remain fully functional on stills. This matters: the case study must survive being opened in any browser.

---

## 4. Layout & responsive

Three primary layouts. The canvas (live engine) is always the brightest, largest element; chrome frames it.

### 4.1 Gallery (`/gallery`)

Tier-grouped responsive grid. Section header per tier (badge + one-line meaning + count). 3-up desktop → 2-up tablet → 1-up mobile.

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ ◆ DynaCharting · Frontier      Gallery   Frontier   Report        ● Live  ⏯    │
├──────────────────────────────────────────────────────────────────────────────┤
│  THE GALLERY      17 live views · JSON manifests over a faithful data path      │
│                                                                                │
│  ▍● NATIVE · the engine renders these directly                          (5)    │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐                               │
│  │ ░ chart ░ │  │ ░ chart ░ │  │ ░ chart ░ │   …                            │
│  │ OHLC bars  │  │ candle+vol │  │ line/area  │                               │
│  │ ▍●NATIVE ≈X│  │ ▍●NATIVE ≈X│  │ ▍●NATIVE ≈X│                               │
│  └────────────┘  └────────────┘  └────────────┘                               │
│                                                                                │
│  ▍● COMPOSED · JSON + upstream precompute                               (9)    │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐                               │
│  │ depth ladder│ │ market prof│  │ footprint  │   …                          │
│  └────────────┘  └────────────┘  └────────────┘                               │
│                                                                                │
│  ▍● WALLED · rendered precomputed — the live-GPU edge                    (3)    │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐                               │
│  │ GPU density│  │ contour    │  │ spectrogram│                               │
│  └────────────┘  └────────────┘  └────────────┘                               │
└──────────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Single view + explainer (`/view/:id`)

The canvas dominates (≥65% width); explainer docked right; a thin filmstrip of sibling views along the bottom for lateral nav (keeps "breadth" present while you're deep). The scaffold's full-bleed `.showcase-canvas-wrap` is the canvas region.

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ ◆ DynaCharting · Frontier   ← Gallery        Frontier   Report   ● Live ⏯ 0:12 │
├───────────────────────────────────────────────────┬────────────────────────────┤
│                                                   │ Candlestick + Volume + EMA  │
│                                                   │ ▍● NATIVE      ≈ TradingView │
│              ░░░░  LIVE CANVAS  ░░░░               ├────────────────────────────┤
│          (the chart, streaming, full size)        │ WHAT'S GOING ON             │
│                                                   │ Per-second OHLC candles …   │
│                                                   ├────────────────────────────┤
│                                                   │ DATA      AAPL · 1s OHLCV   │
│                                                   │ PIPELINE  instancedCandle@1 │
│                                                   │ BUFFERS   700 · 500         │
│                                                   │ SOURCE    GMA→embassy→WS    │
│                                                   ├────────────────────────────┤
│   ⏮ ⏯ ━━━━●──────── 0:12/0:20  ↻                   │  ← Prev      Next →         │
├───────────────────────────────────────────────────┴────────────────────────────┤
│  ▸ filmstrip: ▢ OHLC  ▢ candle  ▢ line  ▢ RSI  ▢ depth  ▢ profile  ▢ … →        │
└──────────────────────────────────────────────────────────────────────────────┘
```

Responsive: tablet → explainer collapses to a bottom sheet under the canvas; mobile → canvas on top (16:10), explainer stacked below, filmstrip becomes a horizontal swipe row, replay transport pinned above the explainer.

### 4.3 Frontier map (`/frontier`)

Full-bleed, generous margins, the §3.5 band map centered with the hero statement above and the CTA/report link below. This page is allowed the most whitespace on the site — it's the climax, give it air. On mobile the bands stack (native→composed→walled, top→bottom) preserving the progression.

---

## 5. Microcopy & tone

**Voice:** an expert engineer explaining their work to a sharp peer. Precise, plain, quietly proud, zero hype. Prefer the concrete noun over the adjective ("instanced GPU geometry" > "blazing-fast rendering"). Name real things — `instancedCandle@1`, `scalar-fan`, `dataplane WS`. Never "revolutionary," "seamless," "powerful," "cutting-edge." The confidence comes from specificity, not superlatives.

**Rules:**
- **Numbers are mono and exact.** "20-period EMA," "1s OHLCV," "20-level depth," not "many levels."
- **Every "WHAT'S GOING ON" follows the DATA + TECHNIQUE formula** (§3.4): say *what data* and *what was done to render it*, in 1–2 sentences, ≤ ~40 words.
- **Tier vocabulary is fixed:** native = "directly," composed = "JSON manifest + upstream precompute," walled = "rendered precomputed; the live version is the frontier." Reuse these phrases verbatim so the three tiers read as one consistent argument across card, explainer, and map.
- **The wall is named, never hedged.** Always "real-time GPU per-pixel compute / custom shaders," not "some limitations." Honesty *is* the case study's credibility.
- **Reference tools get "≈ / in the spirit of"**, never "better than." We orient, we don't compete.

**Worked example — the Candlestick view** (the voice-setting reference; T5.3 copy should match this register):

> **Candlestick + Volume + EMA(20)**
> `▍● NATIVE`  ·  `≈ TradingView`
>
> **What's going on**
> Per-second OHLC candles for AAPL with a volume sub-pane and a 20-period EMA overlay — streamed tick-by-tick over the live data path (Mock GMA → embassy → dataplane WS) and drawn as instanced GPU geometry straight from the JSON manifest. No bespoke chart code: the manifest declares the panes, buffers, and pipelines; the engine does the rest.
>
> | | |
> |---|---|
> | **DATA** | AAPL · 1s OHLCV · ~20s replay |
> | **PIPELINE** | `instancedCandle@1` + `line2d@1` |
> | **WRITE MODE** | compound (OHLC packed) + append (EMA) |
> | **BUFFERS** | `700` candle6 · `500` pos2_clip |
> | **SOURCE** | Mock GMA → embassy → dataplane WS → dc-wasm |

A *composed* explainer adds a `COMPOSED VIA` row (e.g. depth ladder: *"20-level book → a fan of `depth.bid.k` scalars → one fixed buffer, overwritten in place each tick"*) — this is the line that makes the "impossible-from-JSON, but isn't" reveal land. A *walled* explainer adds an amber `THE WALL` row (e.g. *"live KDE density wants per-pixel GPU compute; shown here as a precomputed colormap texture"*).

---

## 6. Build notes for the UI agents (T5.1–5.7)

- **Tokens first (T5.1):** ship the palette/type/spacing above as CSS custom properties extending `App.css`; every later ticket consumes tokens, never raw hex. One source of truth.
- **Data-driven, no hardcoding (T5.2/5.3):** cards, explainers, badges, and the frontier map all render from `view.json` (`tier`, `referenceTool`, `datasetId`) + `view.md` (explainer copy) + the manifest's buffer facts. Adding a view = adding files, never editing components (this also satisfies T6.5 "how to add a view").
- **One engine instance (T5.4):** single canvas, `applyManifest()` / `resetScene()` between views (per PLAN A5). Card thumbnails use settled stills (T6.2) where running 17 live engines is infeasible; single-view is always live.
- **Settled, not timed (T5.6):** drive loading→ready off `window.__dcEngine.isSettled()`.
- **Frontier map is a first-class page (T5.5), not a footer.** Budget for it accordingly — it's the deliverable the case study is remembered for.
- **Accessibility (T5.7):** tier color is never the *only* signal — always paired with the text label (color-blind safe). Keyboard view-nav across cards/filmstrip; ARIA on the status pill and replay transport; the WebGPU-unsupported still-image fallback keeps the whole case study legible without a GPU.

---

## 7. Acceptance — "is it designed and self-explanatory?"

A view passes when, with sound off and no prior context, a viewer can answer in ~5 seconds: **(1) what am I looking at** (title + reference tool), **(2) what's the data and how was it rendered** (the WHAT'S-GOING-ON sentence + fact block), **(3) why does its tier placement make sense** (badge + the composed-via / the-wall fact). The site as a whole passes when a viewer leaves able to state the thesis (§0) and name the one remaining wall in their own words. If the frontier map didn't make them *feel* how small and specific that wall is, the climax failed — iterate on Act III first.
