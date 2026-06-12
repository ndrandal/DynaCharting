// ENC-620c (Epic ENC-620) — AI GRAMMAR-CARD / CONTEXT-KIT generator.
//
// WHAT THIS IS
// ------------
// The CONTEXT an AI model needs to author a `dc-manifest/1` reliably — the final
// piece of the AI-authoring harness (RESEARCH §6.5). Authoring a manifest is
// program-synthesis against a strongly-typed, total DSL: a finite mark vocabulary
// (~8), finite scales (~11), a finite transform vocabulary, and a finite
// pipeline->format table. Give the model that closed vocabulary + a few worked
// anchors + the validator-as-oracle and the synthesis is "fill a Vega-Lite spec",
// not open-ended codegen. This header assembles, into one prompt-ready kit:
//
//   1. GRAMMAR CARD — the available marks (+ their required channels), scales (+
//      accepted dtypes), transforms (+ output-schema rule + streaming class), and
//      the pipeline->required-vertex-format table. Every section is ENUMERATED FROM
//      THE LIVE CATALOGS/REGISTRIES (PipelineCatalog::keys() + markSpecOf, the
//      Scale dtype matrix, the real transform nodes) so the card NEVER drifts from
//      the code — adding a pipeline or a mark changes the card automatically.
//   2. FEED SCHEMA DESCRIPTOR — an available feed's streamKeys/fields/dtypes + the
//      f64-time-stays-CPU rule, so the model knows which columns it may bind.
//   3. WORKED MANIFESTS — 2-3 canonical correct manifests (bar, candle+SMA,
//      treemap) as few-shot anchors. Every one PASSES the ManifestValidator (a unit
//      test asserts it), so the few-shot set can never rot.
//   4. REPAIR LOOP (scaffold) — the execution-guided loop: author -> validate ->
//      if invalid, feed the LOCALIZED ValidationReport back as the repair signal ->
//      re-author. No LLM call here — just the loop structure wired to the real
//      validator + a `repairSignal` that turns a report into the prompt a model
//      consumes (RESEARCH §6.5: "localized errors for an execution-guided repair
//      loop").
//
// SCOPE (ENC-620c only)
// ---------------------
// ONLY the grammar card / context kit / few-shot anchors / repair-loop scaffold.
// NO replay harness (ENC-620b, parallel), NO GPU. Pure `dc` (C++17). It builds the
// card by READING the merged catalogs — it adds no new vocabulary of its own.
#pragma once

#include "dc/manifest/ManifestValidator.hpp"

#include <functional>
#include <string>
#include <vector>

namespace dc {

// ---------------------------------------------------------------------------
// FeedField — one column an available feed exposes: its name, dtype string
// ("f32"/"i32"/"cat"/"timestamp"), and a short human note (e.g. "OHLC close").
// ---------------------------------------------------------------------------
struct FeedField {
  std::string name;
  std::string dtype;
  std::string note;
};

// ---------------------------------------------------------------------------
// FeedDescriptor — an available feed's schema: its source id, its rowKey column,
// the stream keys it multiplexes (e.g. symbols), and its typed fields. This is the
// "what columns can I bind?" half of the context kit (RESEARCH §6.5). The descriptor
// carries the f64-time-stays-CPU rule implicitly: a `timestamp` field is bound only
// through a `time` scale (the grammar card states the rule explicitly).
// ---------------------------------------------------------------------------
struct FeedDescriptor {
  std::string id;                       // the data-source id (manifest `data.sources[].id`)
  std::string rowKey;                   // the rowKey column name (e.g. "t")
  std::vector<std::string> streamKeys;  // multiplexed keys (e.g. ["AAPL","MSFT"])
  std::vector<FeedField> fields;        // the typed columns

  // Render this feed as the Markdown "FEED SCHEMA" block of the context kit.
  std::string toMarkdown() const;

  // The canonical demo OHLC feed ({t, open, high, low, close, volume, symbol}) —
  // the feed the worked manifests bind. A ready-made descriptor for the kit.
  static FeedDescriptor demoOhlc();
};

// ---------------------------------------------------------------------------
// WorkedManifest — one few-shot anchor: a short title, a one-line description of
// what it demonstrates, and the manifest JSON. The JSON is guaranteed to PASS the
// ManifestValidator (the unit test asserts every anchor is valid).
// ---------------------------------------------------------------------------
struct WorkedManifest {
  std::string title;        // "Bar chart (volume by symbol)"
  std::string description;  // one-line: what grammar feature it anchors
  std::string json;         // the manifest source (passes ManifestValidator)
};

// ---------------------------------------------------------------------------
// ContextKit — the assembled AI-authoring context. Built ENTIRELY from the live
// catalogs (the grammar card) + a curated feed + the few-shot anchors. `toPrompt()`
// concatenates everything into one Markdown document an AI model is given before it
// authors a manifest.
// ---------------------------------------------------------------------------
class ContextKit {
 public:
  ContextKit() = default;

  // ----- 1. GRAMMAR CARD (catalog-derived) ----------------------------------

  // The full grammar card (Markdown): marks + channels, scales + dtypes,
  // transforms + output rule + streaming class, and the pipeline->format table.
  // ENUMERATED from PipelineCatalog::keys(), markSpecOf, the scale dtype matrix,
  // and the real transform vocabulary — never a hardcoded list.
  static std::string grammarCardMarkdown();

  // The individual card sections (each catalog-derived), exposed for tests that
  // assert specific entries are present (proving the card is derived, not stale).
  static std::string marksSection();       // marks + required channels
  static std::string scalesSection();      // scales + accepted dtypes
  static std::string transformsSection();  // transforms + output rule + class
  static std::string pipelinesSection();   // pipeline -> required vertex format

  // ----- 2. FEED SCHEMA DESCRIPTOR ------------------------------------------

  void setFeed(FeedDescriptor feed) { feed_ = std::move(feed); }
  const FeedDescriptor& feed() const { return feed_; }

  // ----- 3. WORKED MANIFESTS (few-shot anchors) -----------------------------

  // The canonical correct manifests (bar / candle+SMA / treemap-ish). Each PASSES
  // the validator. Built fresh each call; reusable as few-shot anchors.
  static std::vector<WorkedManifest> workedManifests();

  // ----- assemble -----------------------------------------------------------

  // The full prompt: grammar card + feed descriptor + worked anchors + the repair
  // protocol note. A model is given this, then asked to author a manifest.
  std::string toPrompt() const;

 private:
  FeedDescriptor feed_{FeedDescriptor::demoOhlc()};
};

// ---------------------------------------------------------------------------
// RepairAttempt — one turn of the execution-guided repair loop: the candidate
// manifest a model produced, the validator's verdict, and (if invalid) the repair
// signal to feed back. `done` is true once the candidate validates.
// ---------------------------------------------------------------------------
struct RepairAttempt {
  std::string candidate;        // the manifest JSON evaluated this turn
  ValidationReport report;      // the validator's verdict
  bool done{false};             // candidate is valid (no Error issues)
  std::string repairSignal;     // localized feedback for the next author turn ("" if done)
};

// ---------------------------------------------------------------------------
// repairSignalFor — turn a (failed) ValidationReport into the LOCALIZED repair
// prompt a model consumes for the next author turn (RESEARCH §6.5). It surfaces the
// node-keyed, path-keyed Error issues — the exact "scale `yp` type `log` rejects
// field `close`"-style signal — plus a one-line instruction. Empty if the report is
// valid (nothing to repair).
// ---------------------------------------------------------------------------
std::string repairSignalFor(const ValidationReport& report);

// ---------------------------------------------------------------------------
// runRepairLoop — the execution-guided loop scaffold (RESEARCH §6.5). It does NOT
// call an LLM; the AUTHOR is injected as a callback so the loop structure can be
// tested deterministically and wired to a real model later. Each iteration:
//   1. author(previousSignal) -> a candidate manifest (the FIRST call gets "").
//   2. validator.validate(candidate) -> a report.
//   3. if valid -> done; else compute repairSignalFor(report) and loop.
// Stops when a candidate validates or `maxAttempts` is reached. Returns EVERY
// attempt (the trace), so a caller sees the repair trajectory. The author callback
// takes the prior turn's repair signal ("" on the first turn) and returns the next
// candidate JSON.
// ---------------------------------------------------------------------------
std::vector<RepairAttempt> runRepairLoop(
    const ManifestValidator& validator,
    const std::function<std::string(const std::string& repairSignal)>& author,
    int maxAttempts = 4);

}  // namespace dc
