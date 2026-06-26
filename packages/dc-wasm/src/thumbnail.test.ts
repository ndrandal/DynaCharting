// ENC-778 (Canvas & Tabs Redesign): unit tests for the captureThumbnail pixel
// path — flip + downscale + PNG-encode — against a SYNTHETIC framebuffer.
//
// COVERAGE / SCOPE: these tests exercise the deterministic, GPU-free pixel
// handling: (1) the ENC-696 row flip, (2) the box-average downscale, (3) the
// flip→downscale→encode wiring through an injected canvas factory, and (4)
// EngineHost.captureThumbnail's framebuffer read + guards against a fake core.
// They do NOT exercise the real WASM/Dawn render (no GPU/WebGPU in the node test
// env) nor a real <canvas> PNG encode — `toDataURL` is mocked. The encode path is
// the SAME ImageData→putImageData→toDataURL("image/png") call the real browser
// canvas implements, so what is untested here is only the browser's PNG codec.

import { describe, it, expect, vi, beforeAll } from "vitest";
import {
  flipRowsRGBA,
  downscaleBoxRGBA,
  framebufferToThumbnailDataURL,
  DEFAULT_THUMB_W,
  DEFAULT_THUMB_H,
  type ThumbnailCanvas,
} from "./thumbnail";
import { EngineHost } from "./EngineHost";

// Minimal ImageData polyfill (node test env has no DOM canvas) — matches the
// pattern in EngineHost.blit.test.ts. framebufferToThumbnailDataURL only
// constructs ImageData; it never touches a real DOM canvas.
beforeAll(() => {
  if (typeof (globalThis as { ImageData?: unknown }).ImageData === "undefined") {
    (globalThis as { ImageData: unknown }).ImageData = class {
      data: Uint8ClampedArray;
      width: number;
      height: number;
      constructor(data: Uint8ClampedArray, width: number, height: number) {
        this.data = data;
        this.width = width;
        this.height = height;
      }
    };
  }
});

/** RGBA buffer where each row's red channel encodes its row index, so a flip is
 *  unambiguous (row y has red === y). */
function rowTaggedFrame(w: number, h: number): Uint8Array {
  const buf = new Uint8Array(w * h * 4);
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const i = (y * w + x) * 4;
      buf[i] = y;
      buf[i + 3] = 255;
    }
  }
  return buf;
}

/** A fake encode surface that records the ImageData it is handed. */
function fakeCanvasFactory() {
  const calls: { w: number; h: number; img?: ImageData }[] = [];
  const factory = (w: number, h: number): ThumbnailCanvas => {
    const rec = { w, h } as { w: number; h: number; img?: ImageData };
    calls.push(rec);
    return {
      width: w,
      height: h,
      getContext: () => ({
        putImageData: (img: ImageData) => {
          rec.img = img;
        },
      }),
      toDataURL: (type?: string) => `data:${type ?? "image/png"};base64,STUB`,
    };
  };
  return { factory, calls };
}

describe("flipRowsRGBA (ENC-696 orientation)", () => {
  it("flips rows so output row y comes from source row (h-1-y)", () => {
    const w = 3;
    const h = 5;
    const out = flipRowsRGBA(rowTaggedFrame(w, h), w, h);
    const rowBytes = w * 4;
    for (let y = 0; y < h; y++) {
      expect(out[y * rowBytes]).toBe(h - 1 - y);
    }
  });

  it("is an involution: flipping twice restores the source order", () => {
    const w = 2;
    const h = 4;
    const once = flipRowsRGBA(rowTaggedFrame(w, h), w, h);
    const twice = flipRowsRGBA(once, w, h);
    const rowBytes = w * 4;
    for (let y = 0; y < h; y++) {
      expect(twice[y * rowBytes]).toBe(y);
    }
  });
});

describe("downscaleBoxRGBA", () => {
  it("halving a checkerboard averages to the mid grey of each 2x2 block", () => {
    // 2x2 source: white / black / black / white -> one dest pixel = avg = 127.5
    const src = new Uint8ClampedArray([
      255, 255, 255, 255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255, 255,
    ]);
    const out = downscaleBoxRGBA(src, 2, 2, 1, 1);
    expect(out.length).toBe(4);
    // (255 + 0 + 0 + 255) / 4 = 127.5 -> clamped/rounded to 128 by Uint8ClampedArray
    expect(out[0]).toBe(128);
    expect(out[1]).toBe(128);
    expect(out[2]).toBe(128);
    expect(out[3]).toBe(255);
  });

  it("downscales a solid color to a uniform output of the requested size", () => {
    const sw = 8;
    const sh = 6;
    const src = new Uint8Array(sw * sh * 4);
    for (let i = 0; i < src.length; i += 4) {
      src[i] = 10;
      src[i + 1] = 20;
      src[i + 2] = 30;
      src[i + 3] = 255;
    }
    const dw = 4;
    const dh = 3;
    const out = downscaleBoxRGBA(src, sw, sh, dw, dh);
    expect(out.length).toBe(dw * dh * 4);
    for (let i = 0; i < out.length; i += 4) {
      expect(out[i]).toBe(10);
      expect(out[i + 1]).toBe(20);
      expect(out[i + 2]).toBe(30);
      expect(out[i + 3]).toBe(255);
    }
  });

  it("preserves a top/bottom split through the flip+downscale composition", () => {
    // Source (top-down readback): top half red, bottom half blue. After the
    // ENC-696 flip the image-top must be the flipped result; downscale keeps the
    // two bands. We assert the encoded ImageData has a red top and blue bottom.
    const w = 4;
    const h = 4;
    const src = new Uint8Array(w * h * 4);
    for (let y = 0; y < h; y++) {
      for (let x = 0; x < w; x++) {
        const i = (y * w + x) * 4;
        if (y < h / 2) {
          src[i] = 200; // red-ish top rows (source space)
        } else {
          src[i + 2] = 200; // blue-ish bottom rows (source space)
        }
        src[i + 3] = 255;
      }
    }
    const { factory, calls } = fakeCanvasFactory();
    framebufferToThumbnailDataURL(src, w, h, 2, 2, factory);
    const img = calls[0].img as ImageData;
    // Output is 2x2. Source top is red; the flip puts source-bottom (blue) at
    // image-top and source-top (red) at image-bottom. Row 0 = blue, row 1 = red.
    const topRowBlue = img.data[2]; // px(0,0) blue channel
    const botRowRed = img.data[(1 * 2 + 0) * 4]; // px(0,1) red channel
    expect(topRowBlue).toBeGreaterThan(100);
    expect(botRowRed).toBeGreaterThan(100);
  });
});

describe("framebufferToThumbnailDataURL", () => {
  it("flips, downscales to the requested size, and PNG-encodes via the canvas", () => {
    const w = 6;
    const h = 6;
    const { factory, calls } = fakeCanvasFactory();
    const url = framebufferToThumbnailDataURL(rowTaggedFrame(w, h), w, h, 3, 2, factory);
    expect(url).toBe("data:image/png;base64,STUB");
    expect(calls).toHaveLength(1);
    expect(calls[0].w).toBe(3);
    expect(calls[0].h).toBe(2);
    const img = calls[0].img as ImageData;
    expect(img.width).toBe(3);
    expect(img.height).toBe(2);
    expect(img.data.length).toBe(3 * 2 * 4);
  });

  it("defaults to 320x200 when no size is given", () => {
    const { factory, calls } = fakeCanvasFactory();
    framebufferToThumbnailDataURL(rowTaggedFrame(10, 10), 10, 10, undefined, undefined, factory);
    expect(calls[0].w).toBe(DEFAULT_THUMB_W);
    expect(calls[0].h).toBe(DEFAULT_THUMB_H);
    expect(DEFAULT_THUMB_W).toBe(320);
    expect(DEFAULT_THUMB_H).toBe(200);
  });
});

describe("EngineHost.captureThumbnail (ENC-778)", () => {
  function hostWithCore(core: unknown): EngineHost {
    const host = new EngineHost();
    (host as unknown as Record<string, unknown>).core = core;
    return host;
  }

  it("reads the current framebuffer and returns a PNG data URL (no live data needed)", () => {
    const fbW = 8;
    const fbH = 6;
    const frame = rowTaggedFrame(fbW, fbH);
    const core = {
      framebufferWidth: () => fbW,
      framebufferHeight: () => fbH,
      framebuffer: vi.fn(() => frame),
    };
    const { factory, calls } = fakeCanvasFactory();
    const url = hostWithCore(core).captureThumbnail(4, 3, factory);
    expect(url).toBe("data:image/png;base64,STUB");
    expect(core.framebuffer).toHaveBeenCalledTimes(1);
    expect(calls[0].w).toBe(4);
    expect(calls[0].h).toBe(3);
  });

  it("returns '' when there is no core", () => {
    const { factory } = fakeCanvasFactory();
    expect(new EngineHost().captureThumbnail(4, 3, factory)).toBe("");
  });

  it("returns '' for a zero-sized framebuffer", () => {
    const { factory } = fakeCanvasFactory();
    const core = {
      framebufferWidth: () => 0,
      framebufferHeight: () => 0,
      framebuffer: () => new Uint8Array(0),
    };
    expect(hostWithCore(core).captureThumbnail(4, 3, factory)).toBe("");
  });

  it("returns '' while a render is in flight (ASYNCIFY guard)", () => {
    const { factory } = fakeCanvasFactory();
    const core = {
      framebufferWidth: () => 8,
      framebufferHeight: () => 6,
      framebuffer: () => rowTaggedFrame(8, 6),
    };
    const host = hostWithCore(core);
    (host as unknown as Record<string, unknown>).rendering = true;
    expect(host.captureThumbnail(4, 3, factory)).toBe("");
  });
});
