#pragma once
// ENC-984: SceneExport — read a SceneDocument back OUT of a live Scene.
//
// The document module already had both halves of the INBOUND direction:
//   JSON --parseSceneDocument--> SceneDocument --SceneReconciler--> Scene
// and it could serialize a SceneDocument it was handed. What it could not do is
// produce one from a Scene that was built by any other route — and every real
// host (JsonHost, the WASM DcEngineHost, customer-layer) builds its Scene from
// CommandProcessor JSON commands, never from a document. So a live chart had no
// exportable structure: serializeSceneDocument existed but nothing could hand it
// a document describing what was actually on screen.
//
// sceneToDocument closes that loop. It is the inverse of SceneReconciler: where
// the reconciler walks a document and emits the commands that make the Scene
// match it, this walks the Scene and fills the document that describes it.
//
//   Scene --sceneToDocument--> SceneDocument --serializeSceneDocument--> JSON
//   JSON  --parseSceneDocument--> SceneDocument --SceneReconciler--> Scene
//
// WHAT IT DOES NOT CARRY, and why (this is a real boundary, not an omission):
//
//  * Buffer BYTES. `DocBuffer::data` is left empty and only `byteLength` is
//    written. The vertex bytes are not in the Scene at all — the Scene's Buffer
//    is {id, byteLength}; the bytes live in IngestProcessor / CpuBufferStore.
//    A host that wants them reads them alongside the document (the WASM host's
//    getBufferBytes(id) does exactly that). Inlining megabytes of floats into a
//    structure document would also be the wrong shape for the save path.
//
//  * viewports / textOverlay / bindings. These are DOCUMENT-level declarations
//    that the Scene has no representation for: the reconciler never applies them
//    to the Scene, hosts consume them directly off the parsed document. Nothing
//    in a Scene can reconstruct them, so a Scene-derived document leaves them
//    empty rather than inventing them. A host that parsed a document and wants a
//    faithful re-export should merge those three sections back in from the
//    document it applied.
//
//  * Geometry bounds (boundsMin/boundsMax/boundsValid). The SceneDocument model
//    has no field for them (pre-existing gap in DocGeometry, not introduced here).

#include "dc/document/SceneDocument.hpp"
#include "dc/scene/Scene.hpp"

namespace dc {

// Fill `out` with the document form of `scene`. `out` is fully overwritten.
// viewportWidth/viewportHeight are NOT derived from the Scene (it does not know
// the surface size); the caller sets them if it knows them.
void sceneToDocument(const Scene& scene, SceneDocument& out);

// Convenience: extract and serialize in one step.
//
// NAMED `...AsDocument`, not `serializeScene`, on purpose: `dc::serializeScene`
// ALREADY EXISTS (`dc/session/SceneSerializer.hpp`, D45) and emits a DIFFERENT,
// incompatible JSON dialect from the same input. As plain overloads in one
// namespace the two compile fine until a translation unit includes both headers,
// at which point `serializeScene(scene)` is ambiguous — and until then the real
// hazard is a reader picking the wrong one by name. The two serializers are also
// not interchangeable in fidelity: D45's carries Geometry bounds and the
// `hasAnchor` flag, which the SceneDocument schema has no field for (see above).
std::string serializeSceneAsDocument(const Scene& scene, bool compact = false);

} // namespace dc
