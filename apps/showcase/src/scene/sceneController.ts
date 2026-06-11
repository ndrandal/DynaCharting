/* apps/showcase/src/scene/sceneController.ts
 *
 * The thin orchestration layer between a SceneManifest and an EngineHost:
 *
 *   - applyManifest(host, manifest) wraps the manifest's command list in
 *     beginFrame/commitFrame (frame atomicity, per the CommandProcessor
 *     contract) and applies each command via host.applyControl, then pushes the
 *     binary buffer uploads through the data plane (host.enqueueData).
 *
 *   - resetScene(host, prev) tears down a previously-applied manifest so a new
 *     one starts from a clean scene. Per the buffer-ID contract, deleting a pane
 *     cascades to its layers -> drawItems, but buffers and transforms are
 *     top-level resources that must be deleted explicitly. Geometries are
 *     deleted explicitly too (they are not owned by the pane subtree).
 *
 * The WASM EngineHost wrapper does not expose listResources(), so resetScene
 * works off the manifest we last applied (the showcase owns its manifests, so
 * this is sufficient — and on first mount there is nothing to reset).
 */

import type { EngineHost } from '@repo/dc-wasm';
import { encodeUpload, type SceneManifest } from './commands';

/** IDs referenced by a manifest, grouped by resource kind (for teardown). */
interface ManifestIds {
  panes: number[];
  geometries: number[];
  buffers: number[];
  transforms: number[];
}

function collectIds(manifest: SceneManifest): ManifestIds {
  const ids: ManifestIds = { panes: [], geometries: [], buffers: [], transforms: [] };
  for (const c of manifest.commands) {
    const id = typeof c.id === 'number' ? c.id : undefined;
    switch (c.cmd) {
      case 'createPane':
        if (id !== undefined) ids.panes.push(id);
        break;
      case 'createGeometry':
        if (id !== undefined) ids.geometries.push(id);
        break;
      case 'createBuffer':
        if (id !== undefined) ids.buffers.push(id);
        break;
      case 'createTransform':
        if (id !== undefined) ids.transforms.push(id);
        break;
      default:
        break;
    }
  }
  return ids;
}

/**
 * Delete every resource a previous manifest created. Pane deletion cascades to
 * layers + drawItems; geometries, buffers, and transforms are deleted
 * explicitly. Wrapped in a frame so the teardown is atomic. Safe to call with
 * `null` (no-op on first mount).
 */
export function resetScene(host: EngineHost, prev: SceneManifest | null): void {
  if (!prev) return;
  const ids = collectIds(prev);

  host.applyControl({ cmd: 'beginFrame' });
  // Panes first: deletion cascades to their layers -> drawItems.
  for (const id of ids.panes) host.applyControl({ cmd: 'delete', kind: 'pane', id });
  // Then the top-level resources the cascade does NOT cover.
  for (const id of ids.geometries) host.deleteGeometry(id);
  for (const id of ids.buffers) host.deleteBuffer(id);
  for (const id of ids.transforms) host.deleteTransform(id);
  host.applyControl({ cmd: 'commitFrame' });
  host.markDirty();
}

/**
 * Apply a manifest: structural/style commands inside one frame, then the binary
 * buffer uploads. Returns the manifest (so the caller can hold it for the next
 * resetScene).
 */
export function applyManifest(host: EngineHost, manifest: SceneManifest): SceneManifest {
  host.applyControl({ cmd: 'beginFrame' });
  for (const c of manifest.commands) {
    const r = host.applyControl(c);
    if (!r.ok) {
      // Surface but don't abort — the core skips the bad command and the rest of
      // the frame still renders (and the dev console shows the reason).
      console.warn('[showcase] command rejected:', r.error, c);
    }
  }
  host.applyControl({ cmd: 'commitFrame' });

  for (const u of manifest.uploads) {
    host.enqueueData(encodeUpload(u));
  }
  host.markDirty();
  return manifest;
}
