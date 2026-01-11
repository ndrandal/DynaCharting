export function validatePlan(plan: {
  commands: any[];
  disposeCommands: any[];
}) {
  const buffers = new Set<number>();
  const geometries = new Set<number>();
  const drawItems = new Set<number>();
  const transforms = new Set<number>();

  for (const c of plan.commands) {
    switch (c.cmd) {
      case "createBuffer":
        buffers.add(c.id);
        break;
      case "createGeometry":
      case "createInstancedGeometry":
        if (!buffers.has(c.vertexBufferId ?? c.instanceBufferId)) {
          throw new Error(`Geometry ${c.id} references missing buffer`);
        }
        geometries.add(c.id);
        break;
      case "createTransform":
        transforms.add(c.id);
        break;
      case "createDrawItem":
        if (!geometries.has(c.geometryId)) {
          throw new Error(`DrawItem ${c.id} references missing geometry`);
        }
        drawItems.add(c.id);
        break;
      case "attachTransform":
        if (!transforms.has(c.transformId)) {
          throw new Error(`attachTransform references missing transform ${c.transformId}`);
        }
        break;
    }
  }
}
