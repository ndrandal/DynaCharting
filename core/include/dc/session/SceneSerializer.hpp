#pragma once
// D45: Scene serialization / deserialization
#include "dc/scene/Scene.hpp"
#include "dc/scene/Types.hpp"
#include "dc/scene/Geometry.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <string>

namespace dc {

// Serialize the entire scene graph to a deterministic JSON string.
// IDs are sorted ascending for determinism.
std::string serializeScene(const Scene& scene);

// Deserialize a JSON string back into a scene via CommandProcessor commands.
// Replays in dependency order: buffers -> transforms -> panes -> layers -> geometries -> drawItems.
// Returns false on parse error.
bool deserializeScene(const std::string& json, Scene& scene, CommandProcessor& cp);

} // namespace dc
