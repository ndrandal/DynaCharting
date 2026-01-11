import type { PipelineId } from "@repo/engine-host";

export type ApplyOk = { ok: true };
export type ApplyErr = { ok: false; error: string };
export type ApplyResult = ApplyOk | ApplyErr;

export type ControlCommand =
  | { cmd: "createBuffer"; id: number }
  | { cmd: "createGeometry"; id: number; vertexBufferId: number; format?: "pos2_clip"; strideBytes?: number }
  | { cmd: "createInstancedGeometry"; id: number; instanceBufferId: number; instanceFormat: "rect4" | "candle6" | "glyph8"; instanceStrideBytes: number }
  | { cmd: "createDrawItem"; id: number; geometryId: number; pipeline: PipelineId }
  | { cmd: "setDrawItemPipeline"; id: number; pipeline: PipelineId }
  | { cmd: "delete"; kind: "drawItem" | "geometry" | "transform" | "buffer"; id: number }
  | { cmd: "createTransform"; id: number }
  | { cmd: "setTransform"; id: number; tx?: number; ty?: number; sx?: number; sy?: number }
  | { cmd: "attachTransform"; targetId: number; transformId: number }
  | { cmd: "bufferSetMaxBytes"; id: number; maxBytes: number }
  | { cmd: "bufferEvictFront"; id: number; bytes: number }
  | { cmd: "bufferKeepLast"; id: number; bytes: number }
  | { cmd: "ensureGlyphs"; chars: string; font?: string }
  | { cmd: "setDebug"; showBounds?: boolean; wireframe?: boolean };
