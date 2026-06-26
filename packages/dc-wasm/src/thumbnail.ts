/* packages/dc-wasm/src/thumbnail.ts — ENC-778 (Canvas & Tabs Redesign)
 *
 * Pure pixel-handling for `EngineHost.captureThumbnail`: take the engine's
 * already-rendered native framebuffer (RGBA8, top-down readback) and turn it
 * into a small downscaled PNG **data URL** suitable for storing on a Session and
 * showing as a gallery card (SPEC §7, ADR-0003).
 *
 * The capture reads whatever is CURRENTLY in the framebuffer — it needs no live
 * pipeline or data at call time; a view that has rendered once has a full frame
 * sitting in memory.
 *
 * The flip + downscale steps are deliberately split into small pure functions so
 * the pixel path is unit-testable against a synthetic framebuffer without a real
 * GPU/WebGPU context or a real <canvas> (see thumbnail.test.ts). Only the final
 * PNG encode needs a canvas, and that step is injectable via a canvas factory.
 */

export const DEFAULT_THUMB_W = 320;
export const DEFAULT_THUMB_H = 200;

export type RgbaBytes = Uint8Array | Uint8ClampedArray;

/**
 * Flip RGBA rows top<->bottom.
 *
 * Mirrors the Y-orientation fix `EngineHost.blitFramebuffer` applies on every
 * browser frame (ENC-696, confirmed by ENC-695): the Dawn scene pipelines render
 * with a clip-space Y negation while the framebuffer readback is faithfully
 * top-down, so an authored clip-y-up vertex lands in the BOTTOM rows of
 * `framebuffer()`. Both `putImageData` and the thumbnail encode are top-down, so
 * we flip rows here so clip-up == image-up. Reusing the SAME single flip as the
 * blit path is what guarantees the thumbnail matches the on-canvas picture and is
 * never double-flipped.
 */
export function flipRowsRGBA(
  src: RgbaBytes,
  w: number,
  h: number,
): Uint8ClampedArray<ArrayBuffer> {
  const rowBytes = w * 4;
  const out = new Uint8ClampedArray(w * h * 4);
  for (let y = 0; y < h; y++) {
    const srcStart = (h - 1 - y) * rowBytes;
    out.set(src.subarray(srcStart, srcStart + rowBytes), y * rowBytes);
  }
  return out;
}

/**
 * Box-average (area) downscale of an RGBA image from sw×sh to dw×dh.
 *
 * Each destination pixel averages the source pixels it covers, which gives a
 * cleaner shrink than nearest-neighbour for thumbnails. Pure and deterministic
 * (no canvas), so it is directly unit-testable. Intended for downscaling only
 * (dw ≤ sw, dh ≤ sh); it still produces output for upscales but with no
 * interpolation. The destination is filled to exactly dw×dh — aspect ratio is
 * NOT preserved (the engine framebuffer and the requested thumbnail box are both
 * effectively fixed-aspect chart canvases, and the gallery cards are uniform, so
 * a stretch is simpler and avoids letterbox bars). Callers wanting aspect-fit
 * should pass dimensions that match the source aspect.
 */
export function downscaleBoxRGBA(
  src: RgbaBytes,
  sw: number,
  sh: number,
  dw: number,
  dh: number,
): Uint8ClampedArray<ArrayBuffer> {
  const dst = new Uint8ClampedArray(dw * dh * 4);
  for (let dy = 0; dy < dh; dy++) {
    const sy0 = Math.floor((dy * sh) / dh);
    const sy1 = Math.max(sy0 + 1, Math.floor(((dy + 1) * sh) / dh));
    for (let dx = 0; dx < dw; dx++) {
      const sx0 = Math.floor((dx * sw) / dw);
      const sx1 = Math.max(sx0 + 1, Math.floor(((dx + 1) * sw) / dw));
      let r = 0;
      let g = 0;
      let b = 0;
      let a = 0;
      let n = 0;
      for (let sy = sy0; sy < sy1; sy++) {
        for (let sx = sx0; sx < sx1; sx++) {
          const i = (sy * sw + sx) * 4;
          r += src[i];
          g += src[i + 1];
          b += src[i + 2];
          a += src[i + 3];
          n++;
        }
      }
      const di = (dy * dw + dx) * 4;
      dst[di] = r / n;
      dst[di + 1] = g / n;
      dst[di + 2] = b / n;
      dst[di + 3] = a / n;
    }
  }
  return dst;
}

/** The tiny slice of a 2D canvas the PNG encode needs. */
export interface ThumbnailCanvas {
  width: number;
  height: number;
  getContext(
    contextId: "2d",
  ): { putImageData(image: ImageData, dx: number, dy: number): void } | null;
  toDataURL(type?: string): string;
}

export type ThumbnailCanvasFactory = (w: number, h: number) => ThumbnailCanvas;

/**
 * Default encode surface: a detached <canvas>. `captureThumbnail` runs on the
 * main thread at save time (where the EngineHost lives), so `document` and a
 * synchronous `toDataURL` are available. `OffscreenCanvas` is intentionally not
 * used here because it has no synchronous `toDataURL` (only async
 * `convertToBlob`), and the public API returns a `string`.
 */
function defaultCanvasFactory(w: number, h: number): ThumbnailCanvas {
  if (typeof document !== "undefined" && document.createElement) {
    const canvas = document.createElement("canvas");
    canvas.width = w;
    canvas.height = h;
    return canvas as unknown as ThumbnailCanvas;
  }
  throw new Error(
    "captureThumbnail: no document/<canvas> available to encode a PNG data URL",
  );
}

/**
 * Flip + downscale a native framebuffer and PNG-encode it to a data URL.
 *
 * `fb` is the raw RGBA8 readback (top-down) at fbW×fbH; output is an `image/png`
 * data URL at outW×outH. The canvas factory is injectable so the flip→downscale→
 * encode wiring can be exercised in a non-DOM test environment.
 */
export function framebufferToThumbnailDataURL(
  fb: RgbaBytes,
  fbW: number,
  fbH: number,
  outW: number = DEFAULT_THUMB_W,
  outH: number = DEFAULT_THUMB_H,
  canvasFactory: ThumbnailCanvasFactory = defaultCanvasFactory,
): string {
  const flipped = flipRowsRGBA(fb, fbW, fbH);
  const scaled = downscaleBoxRGBA(flipped, fbW, fbH, outW, outH);
  const canvas = canvasFactory(outW, outH);
  const ctx = canvas.getContext("2d");
  if (!ctx) {
    throw new Error("captureThumbnail: 2d context unavailable for PNG encode");
  }
  ctx.putImageData(new ImageData(scaled, outW, outH), 0, 0);
  return canvas.toDataURL("image/png");
}
