# Trial Rules

A trial is a single-pass visualization test of the DynaCharting engine. Each trial produces one JSON chart and one rendered image. The purpose is to stress-test the engine's expressive range, accumulate spatial reasoning lessons, and build a library of reference visualizations.

---

## Roles

**Claude (orchestrator)** — writes the spec, delegates to the agent, audits the result.
**Agent (builder)** — reads prior critiques, generates the chart JSON, captures the image. One shot. No revisions.

---

## Phase 1: Specification

Claude writes a visualization specification that satisfies:

1. **Unique** — must not duplicate the concept of any previous trial. Check `docs/trials/` for existing trial descriptions before choosing.
2. **Concrete** — specifies the visual layout, data content, chart types, color scheme, and any interactive behavior. Not vague ("make something cool") but precise ("3-pane layout with X in pane 1, Y in pane 2").
3. **Challenging** — should push at least one dimension: spatial complexity, coordinate math, multi-pane layout, color/style sophistication, unusual chart type, or data density.
4. **Achievable** — must be expressible with the engine's existing primitives (10 pipelines, transforms, text overlay, viewports). No features that don't exist.

The spec is a plain-text description, not JSON. It describes *what* the output should look like, not *how* to build it. The agent figures out the how.

---

## Phase 2: Generation

Claude delegates the spec to an agent with these instructions:

### Agent responsibilities

1. **Read all prior critiques.** Before writing any JSON, read every trial document in `docs/trials/` to absorb lessons from previous failures. This is non-negotiable — the same spatial reasoning mistakes must not repeat.

2. **Read CHART_AUTHORING.md.** The authoring guide is the definitive reference for the engine's capabilities, coordinate spaces, vertex formats, and command semantics.

3. **Generate the chart JSON.** Produce a single `.json` file conforming to the extended SceneDocument format (inline buffer data, viewports, text overlay). The JSON must be self-contained — no external data files.

4. **Validate the JSON.** Run the chart through `dc_json_host` to confirm it produces a frame without errors. Pipe `{"cmd":"render"}` to stdin and verify non-zero output.

5. **Capture an image.** Render the chart to a PNG file for visual review. Use the engine's export path or OSMesa pixel readback.

6. **Deliver artifacts.** Place both files in `docs/trials/`:
   - `NNN-slug.json` — the chart file (also copy to `charts/` for interactive use)
   - `NNN-slug.png` — the rendered image

### Agent constraints

- **One shot.** The agent does not iterate. No "let me fix that and try again." The output is whatever comes out of the first generation pass. This forces the agent to think carefully before writing.
- **No shortcuts.** The agent must compute vertex data, coordinate transforms, and layout math correctly. No placeholder data, no "TODO" comments, no hardcoded clip-space positions that happen to look right at one resolution.
- **ID discipline.** All resource IDs must be globally unique across the unified namespace. Use non-overlapping ranges per type.

---

## Phase 3: Audit

Claude examines the rendered image and JSON with the highest level of critique. The audit is adversarial — the goal is to find every flaw, not to validate the agent's work.

### Audit checklist

**Structural correctness**
- Does the JSON parse without errors?
- Does `dc_json_host` produce a valid frame?
- Are all resource IDs unique (no namespace collisions)?
- Are vertex counts correct for each geometry's format?
- Do viewports reference valid transforms and panes?

**Spatial accuracy**
- Are pane regions non-overlapping and correctly positioned?
- Is data visible within its intended pane (not clipped off-screen, not overflowing)?
- Do text labels land at their intended positions without overlap?
- Are gaps between panes intentionally sized (not accidental)?
- Does the pixel-space result match what the clip-space math predicts?

**Visual quality**
- Is the color scheme coherent and intentional?
- Are elements visually distinguishable (sufficient contrast)?
- Is there appropriate padding/margin around content?
- Are pane separations visible (borders, background contrast, or whitespace)?
- Does text have readable size and placement?

**Specification compliance**
- Does the output match what the spec asked for?
- Are all requested chart types, data series, and layout elements present?
- Does interactive behavior (viewports, link groups) work as specified?

### Audit output

The audit produces a trial document at `docs/trials/NNN-slug.md` containing:

1. **Header** — trial number, date, one-line goal, one-line outcome
2. **What was built** — factual description of the output
3. **Defects found** — every issue, categorized by severity:
   - **Critical** — prevents correct rendering or causes data misrepresentation
   - **Major** — significant visual/layout problem that undermines the visualization
   - **Minor** — cosmetic issue or missed polish opportunity
4. **Spatial reasoning analysis** — what coordinate math was done right, what was done wrong, and why
5. **Lessons for future trials** — specific, actionable takeaways (not generic advice)

---

## Naming Convention

Trials are numbered sequentially: `001`, `002`, `003`, etc.

The slug is a short kebab-case name describing the visualization concept:
- `001-market-command-center`
- `002-heatmap-grid`
- `003-multi-asset-sparklines`

---

## Directory Structure

```
docs/trials/
  001-market-command-center.md    # audit document
  002-slug.md
  002-slug.json                   # chart JSON
  002-slug.png                    # rendered image
  ...

charts/
  002-slug.json                   # copy for interactive use via dc_json_host
  ...
```

---

## Agent Specification

The builder agent is a `general-purpose` subagent spawned via the Agent tool. Claude constructs the prompt at invocation time by filling in the template below.

### Invocation

```
Agent(
  subagent_type = "general-purpose",
  description   = "Trial NNN: <slug>",
  prompt        = <filled template below>
)
```

### Prompt Template

Claude fills `{{TRIAL_NUMBER}}`, `{{SLUG}}`, and `{{SPEC}}` then passes the entire block as the agent's prompt.

```
You are the builder agent for DynaCharting Trial {{TRIAL_NUMBER}}: {{SLUG}}.

YOUR TASK: Generate a single self-contained JSON chart file and a rendered PNG screenshot.
You get ONE attempt. No iterations, no retries, no "let me fix that." Think carefully before writing.

SPECIFICATION:
{{SPEC}}

INSTRUCTIONS — follow this exact sequence:

1. READ PRIOR CRITIQUES
   Read every .md file in docs/trials/ to learn from past mistakes.
   This is mandatory. Do not skip it.

2. READ THE AUTHORING GUIDE
   Read CHART_AUTHORING.md (especially sections on coordinate spaces, vertex formats,
   pipeline types, and the SceneDocument schema). This is your reference for what the
   engine can do and how data must be structured.

3. PLAN YOUR ID ALLOCATION
   All resource types share ONE namespace. Write out your ID plan before generating JSON:
   - Panes: 1-9
   - Layers: 10-49
   - Transforms: 50-69
   - Buffers: 100, 103, 106, ... (groups of 3: buf, geom, drawItem)
   - Geometries: 101, 104, 107, ...
   - DrawItems: 102, 105, 108, ...
   Verify every ID is globally unique.

4. PLAN YOUR SPATIAL LAYOUT
   Before writing vertex data, compute the clip-space regions for each pane and verify:
   - Pane regions don't overlap
   - There's adequate gap between panes (or explicit separators)
   - Converted to pixels: does the layout make visual sense at the viewport resolution?

5. GENERATE THE JSON
   Write the chart file to: docs/trials/{{TRIAL_NUMBER}}-{{SLUG}}.json
   Also copy it to: charts/{{TRIAL_NUMBER}}-{{SLUG}}.json
   The file must be a valid extended SceneDocument with:
   - "viewport" (width/height)
   - "buffers" with inline "data" arrays
   - "transforms", "panes", "layers", "geometries", "drawItems"
   - "viewports" (if interactive pan/zoom is specified)
   - "textOverlay" (for any labels/titles)

6. VALIDATE
   Run: echo '{"cmd":"render"}' | build/core/dc_json_host docs/trials/{{TRIAL_NUMBER}}-{{SLUG}}.json | wc -c
   Output must be > 0. If it fails, you still deliver what you have — no retries.

7. CAPTURE PNG
   Run: build/core/dc_json_host --png docs/trials/{{TRIAL_NUMBER}}-{{SLUG}}.png docs/trials/{{TRIAL_NUMBER}}-{{SLUG}}.json
   Verify the PNG file was created.

8. REPORT
   When done, report back:
   - Whether validation passed
   - The PNG file path (so the orchestrator can view it)
   - Any concerns you have about the output (spatial math you're unsure of, etc.)

CONSTRAINTS:
- ONE SHOT. Do not modify the JSON after writing it. Do not re-run or fix anything.
- All vertex data must be numerically correct for the format (e.g., candle6 = 6 floats per instance).
- Vertex counts must match the actual data array size divided by the format's floats-per-vertex.
- Do not use hardcoded clip-space positions for data that should go through a viewport transform.
- The JSON must be self-contained — no external data files, no code generation.
```

### What the orchestrator does after the agent returns

1. Read the PNG at `docs/trials/NNN-slug.png` using the Read tool (it renders images)
2. Read the JSON at `docs/trials/NNN-slug.json`
3. Perform the full audit per the Phase 3 checklist
4. Write the trial document at `docs/trials/NNN-slug.md`

---

## Image Capture

The `dc_json_host` binary supports `--png` for one-shot rendering:

```bash
# Render to PNG and exit (no stdin protocol, no input loop):
build/core/dc_json_host --png output.png charts/my_chart.json

# Interactive mode (TEXT/FRME protocol on stdin/stdout):
build/core/dc_json_host charts/my_chart.json
```

When `--png` is given, the host renders one frame, writes the PNG, and exits with code 0 on success.

---

## Session Flow Summary

```
Claude                          Agent
  │                               │
  ├─ 1. Check prior trials        │
  ├─ 2. Write spec                │
  ├─ 3. Fill prompt template      │
  ├─ 4. Spawn agent ─────────────►│
  │                               ├─ 5. Read docs/trials/*.md
  │                               ├─ 6. Read CHART_AUTHORING.md
  │                               ├─ 7. Plan IDs + layout
  │                               ├─ 8. Write JSON (one shot)
  │                               ├─ 9. Validate via dc_json_host
  │                               ├─ 10. Capture PNG via --png
  │                               ├─ 11. Report results
  │◄──────────────────────────────┤
  ├─ 12. Read PNG + JSON          │
  ├─ 13. Audit (adversarial)      │
  ├─ 14. Write trial document     │
  └─ done                         │
```
