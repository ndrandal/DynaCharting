// packages/engine-host/src/pipelines.ts

export type PipelineId =
  | "triSolid@1"
  | "line2d@1"
  | "points@1"
  | "instancedRect@1"
  | "instancedCandle@1"
  | "textSDF@1";

export type AttrType = "f32";

export type AttrSpec = {
  size: 1 | 2 | 3 | 4;
  type: AttrType;
  strideBytes: number;
  offsetBytes: number;
  divisor?: 0 | 1; // 1 for instanced attributes
};

export type UniformType = "mat3" | "vec4" | "f32" | "sampler2D";

export type PipelineSpec = {
  id: PipelineId;
  draw: "triangles" | "lines" | "points" | "instancedTriangles";
  attributes: Record<string, AttrSpec>;
  uniforms?: Record<string, UniformType>;
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
  },

  // textSDF@1:
  // instance buffer = 8 floats (32 bytes):
  //  a_g0 = vec4(x0,y0,x1,y1) in clip space (or your chart space pre-transform)
  //  a_g1 = vec4(u0,v0,u1,v1) atlas UVs
  "textSDF@1": {
    id: "textSDF@1",
    draw: "instancedTriangles",
    attributes: {
      a_g0: { size: 4, type: "f32", strideBytes: 32, offsetBytes: 0, divisor: 1 },
      a_g1: { size: 4, type: "f32", strideBytes: 32, offsetBytes: 16, divisor: 1 }
    },
    uniforms: {
      u_transform: "mat3",
      u_color: "vec4",
      u_atlas: "sampler2D",
      u_pxRange: "f32"
    }
  }
};
