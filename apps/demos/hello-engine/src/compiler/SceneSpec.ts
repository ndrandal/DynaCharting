// apps/demos/hello-engine/src/compiler/SceneSpec.ts
import type { StreamType, WorkerSubscription } from "@repo/protocol";

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

export type AxisSpecV0 = {
  // simple visual axes (no labels yet; labels come once text uv querying is formalized)
  side: "bottom" | "top" | "left" | "right";
  ticks?: number;        // default 6
  tickLen?: number;      // in screen01 units (default 0.03)
  inset?: number;        // padding from edge in screen01 units (default 0.02)
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
        bytes: Uint8Array;
      }
    | {
        kind: "none";
        // no data feed (compiler will generate static bytes)
      };

  mark:
    | { kind: "lineStrip"; pipeline?: "line2d@1" }
    | { kind: "points"; pipeline?: "points@1"; pointSize?: number }
    | { kind: "instancedRect"; pipeline?: "instancedRect@1" }
    | { kind: "axis2d"; axis: AxisSpecV0; pipeline?: "line2d@1" };

  style?: Record<string, unknown>;
};

export type ViewTransform = { tx: number; ty: number; sx: number; sy: number };

export type CompiledPlan = {
  commands: any[];
  subscriptions: WorkerSubscription[];
  initialBatches: ArrayBuffer[];
  dispose: any[];

  runtime: {
    // recomputes all layer transforms deterministically
    setView: (
      hostApply: (cmd: any) => { ok: true } | { ok: false; error: string },
      t: ViewTransform
    ) => void;

    // updates a domain2d space by id, then recomputes transforms for layers that use it
    setDomain: (
      hostApply: (cmd: any) => { ok: true } | { ok: false; error: string },
      spaceId: string,
      domain: { xMin: number; xMax: number; yMin: number; yMax: number }
    ) => void;
  };
};
