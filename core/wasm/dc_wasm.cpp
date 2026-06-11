// dc_wasm.cpp — Emscripten/WASM binding skeleton for the pure `dc` core.
//
// ENC-502 (P6.1): compile the backend-agnostic `dc` logic core (scene graph,
// command processor, ingest, recipes, document — NO graphics) to WebAssembly and
// expose a minimal JS/TS surface so the core can be driven from JavaScript.
//
// This is a SKELETON: just enough to prove dc logic runs in WASM and round-trips
// a JSON command across the JS boundary. NO WebGPU/render path here — the Dawn
// renderer (dc_gpu) lands in ENC-503 on emdawnwebgpu and will plug a render
// surface on top of the same DcCore.
//
// Binding approach: Embind (preferred over extern "C"/cwrap) — it lets us expose
// a real C++ class (DcCore) with value-returning string methods and map the
// CmdResult into a small JS object, no manual pointer marshalling.

#include <emscripten/bind.h>

#include <string>

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

namespace {

// Result of applying one command, marshalled to a plain JS object via Embind.
struct DcCmdResult {
  bool ok{true};
  // Numeric id of any resource created by the command (0 if none).
  double createdId{0};
  std::string errorCode;
  std::string errorMessage;
  std::string errorDetails;
};

// DcCore — owns a Scene + ResourceRegistry + CommandProcessor and exposes the
// minimal surface needed to apply JSON commands and read back state from JS.
//
// The members are declared in dependency order: CommandProcessor holds
// references to scene_ and reg_, so those must be constructed first.
class DcCore {
public:
  DcCore() : cp_(scene_, reg_) {}

  // Apply a single JSON command (e.g. {"cmd":"createPane","name":"Main"}).
  // Returns a structured result the JS side can inspect (ok / createdId / error).
  DcCmdResult applyCommand(const std::string& jsonText) {
    dc::CmdResult r = cp_.applyJsonText(jsonText);
    DcCmdResult out;
    out.ok = r.ok;
    out.createdId = static_cast<double>(r.createdId);
    out.errorCode = r.err.code;
    out.errorMessage = r.err.message;
    out.errorDetails = r.err.details;
    return out;
  }

  // Read-back accessors over the active scene (prove state round-trips).
  int paneCount() const { return static_cast<int>(scene_.paneIds().size()); }
  int layerCount() const { return static_cast<int>(scene_.layerIds().size()); }
  int drawItemCount() const { return static_cast<int>(scene_.drawItemIds().size()); }
  int bufferCount() const { return static_cast<int>(scene_.bufferIds().size()); }
  int geometryCount() const { return static_cast<int>(scene_.geometryIds().size()); }

  // Full resource listing as JSON (handy for debugging from JS).
  std::string listResources() const { return cp_.listResourcesJson(); }

private:
  dc::Scene scene_{};
  dc::ResourceRegistry reg_{};
  dc::CommandProcessor cp_;
};

}  // namespace

EMSCRIPTEN_BINDINGS(dc_wasm) {
  namespace em = emscripten;

  em::value_object<DcCmdResult>("DcCmdResult")
      .field("ok", &DcCmdResult::ok)
      .field("createdId", &DcCmdResult::createdId)
      .field("errorCode", &DcCmdResult::errorCode)
      .field("errorMessage", &DcCmdResult::errorMessage)
      .field("errorDetails", &DcCmdResult::errorDetails);

  em::class_<DcCore>("DcCore")
      .constructor<>()
      .function("applyCommand", &DcCore::applyCommand)
      .function("paneCount", &DcCore::paneCount)
      .function("layerCount", &DcCore::layerCount)
      .function("drawItemCount", &DcCore::drawItemCount)
      .function("bufferCount", &DcCore::bufferCount)
      .function("geometryCount", &DcCore::geometryCount)
      .function("listResources", &DcCore::listResources);
}
