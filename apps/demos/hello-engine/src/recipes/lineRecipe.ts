import type { RecipeBuildResult, RecipeConfig } from "./types";

export function buildLineChartRecipe(cfg: RecipeConfig): RecipeBuildResult {
  // ID allocation: stable + deterministic
  const BUF_LINE = cfg.idBase + 1;
  const GEO_LINE = cfg.idBase + 10;
  const DI_LINE = cfg.idBase + 100;
  const T_VIEW = cfg.idBase + 2000;

  const commands = [
    { cmd: "createBuffer", id: BUF_LINE },
    { cmd: "createGeometry", id: GEO_LINE, vertexBufferId: BUF_LINE, format: "pos2_clip", strideBytes: 8 },
    { cmd: "createDrawItem", id: DI_LINE, geometryId: GEO_LINE, pipeline: "line2d@1" },

    // D1.5 view transform
    { cmd: "createTransform", id: T_VIEW },
    { cmd: "setTransform", id: T_VIEW, tx: 0, ty: 0, sx: 1, sy: 1 },
    { cmd: "attachTransform", targetId: DI_LINE, transformId: T_VIEW }
  ];

  // Dispose in reverse dependency order
  const dispose = [
    { cmd: "delete", kind: "drawItem", id: DI_LINE },
    { cmd: "delete", kind: "geometry", id: GEO_LINE },
    { cmd: "delete", kind: "buffer", id: BUF_LINE },

    { cmd: "delete", kind: "transform", id: T_VIEW }
  ];

  const subscriptions = [
    { kind: "workerStream", stream: "lineSine" as const, bufferId: BUF_LINE }
  ];

  return { commands, dispose, subscriptions };
}
