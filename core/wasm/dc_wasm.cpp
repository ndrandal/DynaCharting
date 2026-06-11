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
#include <emscripten/val.h>

#include <cstdint>
#include <string>
#include <vector>

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"

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

// ENC-505 (P6.4): result of ingesting one binary DcCommand[] batch, marshalled
// to a plain JS object via Embind. Mirrors the fields of the TS CoreIngestStub
// IngestResult so JS can assert on them (touched buffer count, payload/dropped
// byte totals). The per-buffer bytes are read back separately via
// getBufferBytes / bufferSize / bufferDigest.
struct DcIngestResult {
  int touchedBufferCount{0};
  double payloadBytes{0};
  double droppedBytes{0};
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

  // ---- ENC-505 (P6.4): binary data-plane ingest -------------------------
  //
  // The browser data plane ships record batches as raw little-endian bytes:
  //   [1B op][4B bufferId u32 LE][4B offsetBytes u32 LE][4B payloadBytes u32 LE][payload]
  // (op 1 = append, op 2 = updateRange; a batch is a concatenation of records).
  //
  // This wires that path into the WASM core, replacing the TypeScript
  // CoreIngestStub. The actual record parsing + append/updateRange write model
  // lives in pure dc (dc::IngestProcessor::processBatch) so it is shared with
  // the native build; this binding is a thin marshalling wrapper.
  //
  // The batch crosses the JS boundary as a Uint8Array (an emscripten::val).
  // We take raw bytes via val — NOT std::string — because Embind's std::string
  // marshalling does UTF-8 text conversion and would corrupt high/embedded
  // bytes. emscripten::convertJSArrayToNumberVector copies the typed array's
  // bytes verbatim into a std::vector<uint8_t>.
  DcIngestResult ingestBinary(emscripten::val batch) {
    std::vector<std::uint8_t> bytes =
        emscripten::convertJSArrayToNumberVector<std::uint8_t>(batch);
    dc::IngestResult r = ingest_.processBatch(
        bytes.data(), static_cast<std::uint32_t>(bytes.size()));
    DcIngestResult out;
    out.touchedBufferCount = static_cast<int>(r.touchedBufferIds.size());
    out.payloadBytes = static_cast<double>(r.payloadBytes);
    out.droppedBytes = static_cast<double>(r.droppedBytes);
    return out;
  }

  // Read back the full CPU bytes of a buffer as a JS Uint8Array so JS can verify
  // the resulting buffer state byte-for-byte against what the CoreIngestStub
  // would produce. Returns an empty Uint8Array if the buffer is unknown/empty.
  emscripten::val getBufferBytes(double bufferId) const {
    auto id = static_cast<dc::Id>(bufferId);
    const std::uint8_t* data = ingest_.getBufferData(id);
    std::uint32_t size = ingest_.getBufferSize(id);
    // typed_memory_view wraps the WASM heap region; the JS caller copies it
    // (we slice() on the JS side) before the next ingest can realloc it.
    return emscripten::val(emscripten::typed_memory_view(size, data));
  }

  // Current byte length of a buffer (0 if unknown/empty).
  double bufferSize(double bufferId) const {
    return static_cast<double>(
        ingest_.getBufferSize(static_cast<dc::Id>(bufferId)));
  }

  // Order-sensitive byte digest (FNV-1a 32-bit) of a buffer's CPU bytes — a
  // cheap independent cross-check JS can recompute over its expected bytes.
  double bufferDigest(double bufferId) const {
    auto id = static_cast<dc::Id>(bufferId);
    const std::uint8_t* data = ingest_.getBufferData(id);
    std::uint32_t size = ingest_.getBufferSize(id);
    std::uint32_t h = 2166136261u;  // FNV offset basis
    for (std::uint32_t i = 0; i < size; ++i) {
      h ^= data[i];
      h *= 16777619u;  // FNV prime
    }
    return static_cast<double>(h);
  }

private:
  dc::Scene scene_{};
  dc::ResourceRegistry reg_{};
  dc::CommandProcessor cp_;
  dc::IngestProcessor ingest_{};  // ENC-505 binary data-plane ingest
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

  em::value_object<DcIngestResult>("DcIngestResult")
      .field("touchedBufferCount", &DcIngestResult::touchedBufferCount)
      .field("payloadBytes", &DcIngestResult::payloadBytes)
      .field("droppedBytes", &DcIngestResult::droppedBytes);

  em::class_<DcCore>("DcCore")
      .constructor<>()
      .function("applyCommand", &DcCore::applyCommand)
      .function("paneCount", &DcCore::paneCount)
      .function("layerCount", &DcCore::layerCount)
      .function("drawItemCount", &DcCore::drawItemCount)
      .function("bufferCount", &DcCore::bufferCount)
      .function("geometryCount", &DcCore::geometryCount)
      .function("listResources", &DcCore::listResources)
      // ENC-505 (P6.4): binary data-plane ingest surface.
      .function("ingestBinary", &DcCore::ingestBinary)
      .function("getBufferBytes", &DcCore::getBufferBytes)
      .function("bufferSize", &DcCore::bufferSize)
      .function("bufferDigest", &DcCore::bufferDigest);
}
