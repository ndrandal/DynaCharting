// ENC-696 regression: EngineHost.blitFramebuffer() must flip framebuffer rows
// so the rendered scene is right-side-up on the <canvas>. The Dawn backends
// render with a clip-space Y negation while the readback is top-down, so a raw
// (un-flipped) putImageData renders the scene upside-down (confirmed in ENC-695:
// an apex-up triangle pointed down; candle highs sat at the bottom). This test
// pins the row-flip without needing a GPU/WebGPU context.

import { describe, it, expect, vi, beforeAll } from "vitest";
import { EngineHost } from "./EngineHost";

// Minimal ImageData polyfill so blitFramebuffer can run under the node test env
// (no jsdom needed — blitFramebuffer only constructs ImageData, never touches a
// real DOM canvas).
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

/** Build a fbW x fbH RGBA buffer where each row's bytes encode its row index,
 *  so a flip is unambiguous: row y has every byte set to (y * 10 + channel). */
function makeFrame(fbW: number, fbH: number): Uint8Array {
  const buf = new Uint8Array(fbW * fbH * 4);
  for (let y = 0; y < fbH; y++) {
    for (let x = 0; x < fbW; x++) {
      const i = (y * fbW + x) * 4;
      buf[i] = y; // tag the row in the red channel
      buf[i + 1] = 0;
      buf[i + 2] = 0;
      buf[i + 3] = 255;
    }
  }
  return buf;
}

function harness(fbW: number, fbH: number) {
  const frame = makeFrame(fbW, fbH);
  const fakeCore = {
    framebufferWidth: () => fbW,
    framebufferHeight: () => fbH,
    framebuffer: () => frame,
  };
  const putImageData = vi.fn();
  const fakeCtx = { putImageData } as unknown as CanvasRenderingContext2D;
  const fakeCanvas = { width: 0, height: 0 } as HTMLCanvasElement;

  const host = new EngineHost();
  // Inject the render surface directly (private fields) — blitFramebuffer only
  // touches core/ctx2d/canvas, so no async init is needed.
  (host as unknown as Record<string, unknown>).core = fakeCore;
  (host as unknown as Record<string, unknown>).ctx2d = fakeCtx;
  (host as unknown as Record<string, unknown>).canvas = fakeCanvas;

  // blitFramebuffer is private; invoke it via the instance.
  (host as unknown as { blitFramebuffer: () => void }).blitFramebuffer();

  return { putImageData, fakeCanvas, fbW, fbH };
}

describe("EngineHost.blitFramebuffer Y-orientation (ENC-696)", () => {
  it("flips framebuffer rows so clip-up renders at image-top", () => {
    const { putImageData, fakeCanvas, fbW, fbH } = harness(3, 5);
    expect(putImageData).toHaveBeenCalledTimes(1);
    const img = putImageData.mock.calls[0][0] as ImageData;
    expect(img.width).toBe(fbW);
    expect(img.height).toBe(fbH);
    expect(fakeCanvas.width).toBe(fbW);
    expect(fakeCanvas.height).toBe(fbH);

    // After the flip, output row y must come from source row (fbH-1-y).
    const rowBytes = fbW * 4;
    for (let y = 0; y < fbH; y++) {
      const redOfOutputRow = img.data[y * rowBytes]; // red channel of first px
      expect(redOfOutputRow).toBe(fbH - 1 - y);
    }
  });

  it("is an involution: flipping the blitted result restores the source order", () => {
    const fbW = 2;
    const fbH = 4;
    const { putImageData } = harness(fbW, fbH);
    const img = putImageData.mock.calls[0][0] as ImageData;
    const rowBytes = fbW * 4;
    // Re-flip the output and confirm it matches the original ascending row tags.
    for (let y = 0; y < fbH; y++) {
      const reflipped = img.data[(fbH - 1 - y) * rowBytes];
      expect(reflipped).toBe(y);
    }
  });

  it("no-ops cleanly when there is no render surface", () => {
    const host = new EngineHost();
    expect(() =>
      (host as unknown as { blitFramebuffer: () => void }).blitFramebuffer(),
    ).not.toThrow();
  });
});
