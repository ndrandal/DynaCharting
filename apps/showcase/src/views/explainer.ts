/* apps/showcase/src/views/explainer.ts
 *
 * Parse a view's `explainer.md` into the structured pieces the explainer panel
 * (T5.3) and frontier map (T5.5) render. The file is:
 *
 *   ---
 *   title: ...
 *   referenceTool: ...
 *   tier: native|composed|walled
 *   ---
 *   <one-or-two sentence DATA + TECHNIQUE "what's going on" prose>
 *
 *   | | |
 *   |---|---|
 *   | **DATA** | AAPL · 1s OHLC · ~20s replay |
 *   | **PIPELINE** | `instancedCandle@1` |
 *   | ...
 *
 * We parse front-matter (key: value), the body prose (everything before the
 * fact table), and the fact rows (label → value) from the markdown table. This
 * keeps the panel data-driven: the copy lives with the view, never in the
 * component (DESIGN-showcase-ui.md §6).
 */

export interface FactRow {
  label: string;
  value: string;
  /** THE WALL rows render in amber (walled tier emphasis). */
  emphasis?: 'wall' | 'composed';
}

export interface ParsedExplainer {
  /** Front-matter key/value pairs (title, referenceTool, tier, …). */
  frontMatter: Record<string, string>;
  /** The DATA+TECHNIQUE prose (the "WHAT'S GOING ON" sentence/s), markdown-stripped. */
  body: string;
  /** The fact block rows (DATA / PIPELINE / WRITE MODE / BUFFERS / SOURCE / …). */
  facts: FactRow[];
}

/** Strip markdown inline `code`/**bold** to plain text for the prose body. */
function stripInline(s: string): string {
  return s
    .replace(/`([^`]*)`/g, '$1')
    .replace(/\*\*([^*]*)\*\*/g, '$1')
    .replace(/\*([^*]*)\*/g, '$1')
    .trim();
}

/** Whether a value/label names "the wall" (→ amber emphasis in the fact block). */
function emphasisFor(label: string): FactRow['emphasis'] {
  const L = label.toUpperCase();
  if (L.includes('WALL')) return 'wall';
  if (L.includes('COMPOSED')) return 'composed';
  return undefined;
}

export function parseExplainer(md: string): ParsedExplainer {
  const frontMatter: Record<string, string> = {};
  let rest = md;

  // Front-matter block delimited by leading `---` … `---`.
  const fmMatch = md.match(/^---\s*\n([\s\S]*?)\n---\s*\n?/);
  if (fmMatch) {
    for (const line of fmMatch[1].split('\n')) {
      const i = line.indexOf(':');
      if (i > 0) {
        const key = line.slice(0, i).trim();
        const val = line.slice(i + 1).trim();
        if (key) frontMatter[key] = val;
      }
    }
    rest = md.slice(fmMatch[0].length);
  }

  const lines = rest.split('\n');
  const bodyLines: string[] = [];
  const facts: FactRow[] = [];

  for (const raw of lines) {
    const line = raw.trim();
    if (!line) continue;
    if (line.startsWith('|')) {
      // Markdown table row. Skip the header/separator rows; parse 2-col facts.
      const cells = line
        .split('|')
        .slice(1, -1)
        .map((c) => c.trim());
      // Separator rows are like |---|---| ; header rows are empty `| |`.
      const isSeparator = cells.every((c) => /^:?-+:?$/.test(c) || c === '');
      if (isSeparator) continue;
      if (cells.length >= 2 && cells[0]) {
        const label = stripInline(cells[0]);
        const value = stripInline(cells[1]);
        if (label) facts.push({ label, value, emphasis: emphasisFor(label) });
      }
      continue;
    }
    bodyLines.push(stripInline(line));
  }

  return { frontMatter, body: bodyLines.join(' ').trim(), facts };
}
