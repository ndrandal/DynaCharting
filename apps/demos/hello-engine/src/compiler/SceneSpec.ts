// apps/demos/hello-engine/src/compiler/SceneSpec.ts
import type { StreamType, WorkerSubscription } from "../protocol";

export type SceneSpecV0 = {
  version: 0;
  name?: string;

  spaces: SpaceSpecV0[];
  views: ViewSpecV0[];
  layers: LayerSpecV0[];
};

export type SpaceSpecV0 =
  | {
      id: string;
      type: "clip";
      // identity mapping; layer coordinates are already clip-space
    }
  | {
      id: string;
      type: "screen01";
      // layer coords are 0..1 within the view rect (origin top-left)
      // this maps them into clip space.
    }
  | {
      id: string;
      type: "domain2d";
      // explicit domain; bounded or “free” is represented by wide ranges or runtime updates later
      domain: { xMin: number; xMax: number; yMin: number; yMax: number };
    };

export type ViewSpecV0 = {
  id: string;
  // relative canvas rect; origin top-left
  rect: { x: number; y: number; w: number; h: number; units: "relative" };
};

export type LayerSpecV0 = {
  id: string;
  viewId: string;
  spaceId: string;

  data:
    | { kind: "workerStream"; stream: StreamType }
    | {
        kind: "staticAppend";
        // raw bytes that will be appended once to the layer buffer
        // (typically Float32Array bytes)
        bytes: Uint8Array;
      };

  mark:
    | { kind: "lineStrip"; pipeline?: "line2d@1" }
    | { kind: "points"; pipeline?: "points@1"; pointSize?: number }
    | { kind: "instancedRect"; pipeline?: "instancedRect@1" }
    // textSDF requires glyph8 instances with UVs. We support the mark shape here,
    // but for now your static builder must provide glyph8 instances (x0,y0,x1,y1,u0,v0,u1,v1).
    | { kind: "textSDF"; pipeline?: "textSDF@1"; color?: [number, number, number, number]; pxRange?: number };

  // optional colors (engine supports uniforms; we’ll wire these next sprint if you want)
  style?: Record<string, unknown>;
};

export type ViewTransform = { tx: number; ty: number; sx: number; sy: number };

export type CompiledPlan = {
  commands: any[];
  subscriptions: WorkerSubscription[];
  initialBatches: ArrayBuffer[];
  dispose: any[];

  runtime: {
    // call this when pan/zoom changes; recomputes all layer transforms deterministically
    setView: (
      hostApply: (cmd: any) => { ok: true } | { ok: false; error: string },
      t: ViewTransform
    ) => void;
  };
};
