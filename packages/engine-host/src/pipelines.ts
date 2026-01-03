export type PipelineId = "triSolid@1";

export type PipelineSpec = {
  id: PipelineId;
  // required vertex attributes
  attributes: {
    a_pos: { size: 2; type: "f32" }; // vec2 position
  };
  // uniforms: allow identity transform (optional)
  uniforms: {
    u_transform?: "mat3"; // optional
    u_color?: "vec4";     // optional
  };
};

export const PIPELINES: Record<PipelineId, PipelineSpec> = {
  "triSolid@1": {
    id: "triSolid@1",
    attributes: { a_pos: { size: 2, type: "f32" } },
    uniforms: { u_transform: "mat3", u_color: "vec4" },
  },
};
