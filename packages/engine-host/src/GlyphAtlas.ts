// packages/engine-host/src/GlyphAtlas.ts

export type GlyphInfo = {
  codepoint: number;
  // UV in atlas
  u0: number; v0: number; u1: number; v1: number;
  // glyph layout metrics in pixels (for recipe helpers)
  advance: number;
  bearingX: number;
  bearingY: number;
  w: number;
  h: number;
  // atlas placement in pixels
  ax: number; ay: number;
};

type Shelf = { x: number; y: number; h: number };

export class GlyphAtlas {
  readonly atlasSize: number;
  readonly pad: number;
  readonly glyphPx: number;
  readonly sdfRange: number; // px distance range encoded into [0..255]

  private font: string;
  private canvas: OffscreenCanvas;
  private ctx: OffscreenCanvasRenderingContext2D;

  private glyphMap = new Map<number, GlyphInfo>();

  // Simple shelf packer
  private shelves: Shelf[] = [{ x: 1, y: 1, h: 0 }];

  // staging buffer for texSubImage2D
  private tmpAlpha: Uint8ClampedArray | null = null;

  constructor(opts?: {
    atlasSize?: number;
    glyphPx?: number;
    pad?: number;
    sdfRange?: number;
    font?: string;
  }) {
    this.atlasSize = opts?.atlasSize ?? 1024;
    this.glyphPx = opts?.glyphPx ?? 48;       // source render size
    this.pad = opts?.pad ?? 2;                // padding around glyph in atlas
    this.sdfRange = opts?.sdfRange ?? 12;     // SDF distance range (px)
    this.font = opts?.font ?? `600 ${this.glyphPx}px system-ui`;

    this.canvas = new OffscreenCanvas(this.atlasSize, this.atlasSize);
    const ctx = this.canvas.getContext("2d", { willReadFrequently: true });
    if (!ctx) throw new Error("GlyphAtlas: OffscreenCanvas 2D ctx not available");
    this.ctx = ctx;

    // Clear atlas (transparent)
    this.ctx.clearRect(0, 0, this.atlasSize, this.atlasSize);
  }

  setFont(font: string) {
    if (font && font !== this.font) this.font = font;
  }

  hasGlyph(cp: number): boolean {
    return this.glyphMap.has(cp);
  }

  getGlyph(cp: number): GlyphInfo | null {
    return this.glyphMap.get(cp) ?? null;
  }

  /**
   * Ensure glyphs exist in atlas.
   * Returns list of glyphs newly added (with pixel rect + SDF pixels for upload).
   */
  ensureGlyphs(chars: string): Array<{ g: GlyphInfo; sdfR8: Uint8Array; w: number; h: number }> {
    const added: Array<{ g: GlyphInfo; sdfR8: Uint8Array; w: number; h: number }> = [];

    for (const ch of chars) {
      const cp = ch.codePointAt(0)!;
      if (this.glyphMap.has(cp)) continue;

      const built = this.buildGlyph(ch, cp);
      if (!built) continue;

      const { g, sdfR8, w, h } = built;
      this.glyphMap.set(cp, g);
      added.push({ g, sdfR8, w, h });
    }

    return added;
  }

  // -------------------- glyph building --------------------

  private buildGlyph(ch: string, cp: number): { g: GlyphInfo; sdfR8: Uint8Array; w: number; h: number } | null {
    // Render glyph into a small cell and build SDF from alpha.
    const cell = this.glyphPx + this.pad * 2 + this.sdfRange * 2; // include SDF border
    const w = cell;
    const h = cell;

    // lazy allocate temp alpha buffer
    if (!this.tmpAlpha || this.tmpAlpha.length < w * h * 4) {
      this.tmpAlpha = new Uint8ClampedArray(w * h * 4);
    }

    const ctx = this.ctx;

    // draw into a scratch ImageData via ctx (use atlas ctx but clip region)
    // We'll draw into atlas temporarily at (0,0), read back, then overwrite later on upload.
    ctx.save();
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.clearRect(0, 0, w, h);

    ctx.fillStyle = "white";
    ctx.textBaseline = "alphabetic";
    ctx.textAlign = "left";
    ctx.font = this.font;

    const metrics = ctx.measureText(ch);
    // bearings approximations:
    const advance = metrics.width;

    // place baseline around center with extra border
    const bx = this.pad + this.sdfRange;
    const by = this.pad + this.sdfRange + this.glyphPx; // baseline

    ctx.fillText(ch, bx, by);

    const img = ctx.getImageData(0, 0, w, h);
    ctx.restore();

    const alpha = new Uint8Array(w * h);
    for (let i = 0, j = 0; i < img.data.length; i += 4, j++) {
      alpha[j] = img.data[i + 3]; // A
    }

    // Build SDF in [0..255], 128 ~= edge.
    const sdf = buildSdfR8(alpha, w, h, this.sdfRange);

    // Pack into atlas
    const place = this.pack(w, h);
    if (!place) return null;

    const { ax, ay } = place;

    const u0 = ax / this.atlasSize;
    const v0 = ay / this.atlasSize;
    const u1 = (ax + w) / this.atlasSize;
    const v1 = (ay + h) / this.atlasSize;

    const g: GlyphInfo = {
      codepoint: cp,
      u0, v0, u1, v1,
      advance,
      bearingX: 0,
      bearingY: 0,
      w, h,
      ax, ay
    };

    return { g, sdfR8: sdf, w, h };
  }

  private pack(w: number, h: number): { ax: number; ay: number } | null {
    // shelf packer
    const max = this.atlasSize - 1;

    for (let i = 0; i < this.shelves.length; i++) {
      const s = this.shelves[i];
      const canFitX = s.x + w <= max;
      const canFitY = s.y + h <= max;
      if (!canFitX || !canFitY) continue;

      // if shelf height not set yet, set it
      if (s.h === 0) s.h = h;
      if (h > s.h) continue;

      const ax = s.x;
      const ay = s.y;

      s.x += w;

      return { ax, ay };
    }

    // new shelf
    const last = this.shelves[this.shelves.length - 1];
    const ny = last.y + (last.h || 0);
    if (ny + h > max) return null;

    const ns: Shelf = { x: 1 + w, y: ny, h };
    this.shelves.push(ns);

    return { ax: 1, ay: ny };
  }
}

// -------------------- SDF builder --------------------
// alpha: 0..255; inside=alpha>127
// Returns Uint8Array size w*h where 128 is edge, >128 inside, <128 outside.
function buildSdfR8(alpha: Uint8Array, w: number, h: number, rangePx: number): Uint8Array {
  const inside = new Uint8Array(w * h);
  for (let i = 0; i < inside.length; i++) inside[i] = (alpha[i] > 127) ? 1 : 0;

  // distance to nearest zero for inside mask and outside mask
  const distToOutside = distanceTransform(inside, w, h);          // inside pixels -> distance to outside
  const inv = new Uint8Array(w * h);
  for (let i = 0; i < inv.length; i++) inv[i] = inside[i] ? 0 : 1;
  const distToInside = distanceTransform(inv, w, h);              // outside pixels -> distance to inside

  const out = new Uint8Array(w * h);
  const r = Math.max(1, rangePx);

  for (let i = 0; i < out.length; i++) {
    const dIn = distToOutside[i];
    const dOut = distToInside[i];
    const signed = inside[i] ? dIn : -dOut;

    // map [-r..r] to [0..255] with 128 at 0
    const clamped = Math.max(-r, Math.min(r, signed));
    const v = 128 + (clamped / r) * 127;
    out[i] = (v < 0) ? 0 : (v > 255) ? 255 : (v | 0);
  }

  return out;
}

// Approximate distance transform: 2-pass chamfer (fast, good enough for UI text SDF).
function distanceTransform(mask: Uint8Array, w: number, h: number): Float32Array {
  const INF = 1e9;
  const d = new Float32Array(w * h);

  // init
  for (let i = 0; i < d.length; i++) d[i] = mask[i] ? INF : 0;

  // forward pass
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const i = y * w + x;
      const v = d[i];
      if (v === 0) continue;

      let best = v;
      if (x > 0) best = Math.min(best, d[i - 1] + 1);
      if (y > 0) best = Math.min(best, d[i - w] + 1);
      if (x > 0 && y > 0) best = Math.min(best, d[i - w - 1] + 1.4142);
      if (x + 1 < w && y > 0) best = Math.min(best, d[i - w + 1] + 1.4142);
      d[i] = best;
    }
  }

  // backward pass
  for (let y = h - 1; y >= 0; y--) {
    for (let x = w - 1; x >= 0; x--) {
      const i = y * w + x;
      const v = d[i];
      if (v === 0) continue;

      let best = v;
      if (x + 1 < w) best = Math.min(best, d[i + 1] + 1);
      if (y + 1 < h) best = Math.min(best, d[i + w] + 1);
      if (x + 1 < w && y + 1 < h) best = Math.min(best, d[i + w + 1] + 1.4142);
      if (x > 0 && y + 1 < h) best = Math.min(best, d[i + w - 1] + 1.4142);
      d[i] = best;
    }
  }

  return d;
}
