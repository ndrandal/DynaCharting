// packages/engine-host/src/pipelines.ts

export type PipelineId =
  | "triSolid@1"
  | "line2d@1"
  | "points@1"
  | "instancedRect@1"
  | "instancedCandle@1";

export type AttrType = "f32";

export type AttrSpec = {
  size: 1 | 2 | 3 | 4;
  type: AttrType;
  strideBytes: number;
  offsetBytes: number;
  divisor?: 0 | 1; // 1 for instanced attributes
};

export type PipelineSpec = {
  id: PipelineId;
  draw: "triangles" | "lines" | "points" | "instancedTriangles";
  attributes: Record<string, AttrSpec>;
  uniforms?: Record<string, "mat3" | "vec4" | "f32">;
};

export const PIPELINES: Record<PipelineId, PipelineSpec> = {
  "triSolid@1": {
    id: "triSolid@1",
    draw: "triangles",
    attributes: {
      a_pos: { size: 2, type: "f32", strideBytes: 8, offsetBytes: 0, divisor: 0 }
    },
    uniforms: { u_transform: "mat3", u_color: "vec4" }
  },

  "line2d@1": {
    id: "line2d@1",
    draw: "lines",
    attributes: {
      a_pos: { size: 2, type: "f32", strideBytes: 8, offsetBytes: 0, divisor: 0 }
    },
    uniforms: { u_transform: "mat3", u_color: "vec4" }
  },

  "points@1": {
    id: "points@1",
    draw: "points",
    attributes: {
      a_pos: { size: 2, type: "f32", strideBytes: 8, offsetBytes: 0, divisor: 0 }
    },
    uniforms: { u_transform: "mat3", u_pointSize: "f32", u_color: "vec4" }
  },

  "instancedRect@1": {
    id: "instancedRect@1",
    draw: "instancedTriangles",
    attributes: {
      a_rect: { size: 4, type: "f32", strideBytes: 16, offsetBytes: 0, divisor: 1 }
    },
    uniforms: { u_transform: "mat3", u_color: "vec4" }
  },

  "instancedCandle@1": {
    id: "instancedCandle@1",
    draw: "instancedTriangles",
    attributes: {
      a_c0: { size: 4, type: "f32", strideBytes: 24, offsetBytes: 0, divisor: 1 },
      a_c1: { size: 2, type: "f32", strideBytes: 24, offsetBytes: 16, divisor: 1 }
    },
    uniforms: { u_transform: "mat3", u_colorUp: "vec4", u_colorDown: "vec4" }
  }
};
