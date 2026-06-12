// ENC-620a (Epic ENC-620) — Manifest LOAD-TIME VALIDATOR.
//
// WHAT THIS IS
// ------------
// A pure, data-free validator that checks a `dc-manifest/1` is WELL-FORMED before
// any byte streams. It is the core of the AI-authoring harness (RESEARCH §6.1 +
// §6.5): a manifest is a program in a strongly-typed total DSL, and the validator
// is its type checker. Every check runs WITHOUT data, so a synthesizing model gets
// LOCALIZED errors ("scale `yp` type `log` rejects field `close` which can be ≤0")
// keyed to the offending node id / path — exactly the signal an execution-guided
// repair loop needs.
//
// THE FOUR §6.1 CHECKS (run with NO data)
// ---------------------------------------
//   1. REFERENCE RESOLUTION — every `from`/`in`/`scale`/`domainFrom.data`/
//      `lookup.from` resolves into the ONE namespace data.sources ∪ transforms ∪
//      scales. A dangling ref is a hard error carrying the offending path.
//   2. COLUMN-SET + DTYPE INFERENCE — each dataset's column set + dtypes is
//      statically inferred (source columns; a transform's output via its own
//      inferSchema: inputs − dropped + `as` outputs). A channel / scale domain /
//      field referencing a MISSING column, or a column with a dtype the bound
//      scale rejects, fails.
//   3. CHANNEL↔SCALE↔DTYPE↔PIPELINE MATRIX — a channel binds `scale(field)` only
//      if: the field dtype is accepted by the scale type, the channel is legal for
//      the mark, the mark is legal for the pipeline, AND the resolved channel set
//      covers the pipeline's REQUIRED vertex/instance format (the validateDrawItem
//      clause mirrored at load).
//   4. DAG ACYCLICITY + STREAMING-CLASS COHERENCE — the data→transform DAG must
//      topo-sort (a cycle is a hard error). A `globalRecompute` (class-3) node
//      feeding a `perFrame` mark is a coherence WARNING: the mark is downgraded to
//      the global's cadence (not a hard error — the chart still renders, just not
//      every frame off that node).
//
// HOW IT REUSES THE MERGED STACK
// ------------------------------
// Column-set inference instantiates the REAL transform nodes (ENC-616*) and calls
// their `inferSchema` / `inferSchemaBinary` — the same data-free typing the DAG
// runs at addTransform — so the validator and the runtime agree by construction.
// The scale↔dtype matrix mirrors the Manifest parser's table; the pipeline format
// clause mirrors EncodePass::markSpecOf + the PipelineCatalog. The validator owns
// NO byte buffers and NEVER runs a transform — it only reasons over names + dtypes.
//
// SCOPE (ENC-620a only)
// ---------------------
// ONLY the four checks + structured error reporting. NO feed→frames replay /
// property harness (ENC-620b), NO AI grammar-card / context-kit (ENC-620c). Pure
// `dc` (C++17).
#pragma once

#include "dc/data/TableStore.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace dc {

// ---------------------------------------------------------------------------
// ValidationCheck — which of the four §6.1 checks an issue came from. Lets a
// repair loop (and a human) bucket failures by category.
// ---------------------------------------------------------------------------
enum class ValidationCheck : std::uint8_t {
  Structure,        // pre-check: malformed JSON / missing required section
  RefResolution,    // §6.1 #1 — a dangling from/in/scale/domainFrom/lookup ref
  ColumnSet,        // §6.1 #2 — a missing column or a dtype a scale rejects
  ChannelMatrix,    // §6.1 #3 — channel↔scale↔dtype↔pipeline / format coverage
  DagCoherence,     // §6.1 #4 — a cycle, or a streaming-class coherence warning
};

const char* toString(ValidationCheck c);

// ---------------------------------------------------------------------------
// Severity — an Error makes the manifest invalid (it must not be loaded); a
// Warning is a coherence note the engine resolves itself (e.g. a class-3 → perFrame
// downgrade) and does NOT invalidate the manifest.
// ---------------------------------------------------------------------------
enum class Severity : std::uint8_t { Error, Warning };

const char* toString(Severity s);

// ---------------------------------------------------------------------------
// ValidationIssue — ONE localized finding. `nodeId` is the manifest id of the
// offending node (a source / transform / scale / mark, "" for a top-level issue);
// `path` is the JSON path to the offending member (e.g.
// "marks['candles'].encoding.y.scale"); `message` is the human-readable, localized
// reason — the exact form a model consumes for a repair loop.
// ---------------------------------------------------------------------------
struct ValidationIssue {
  Severity severity{Severity::Error};
  ValidationCheck check{ValidationCheck::Structure};
  std::string nodeId;   // the offending node's manifest id ("" if N/A)
  std::string path;     // JSON path to the offending member
  std::string message;  // localized, human-readable reason
};

// ---------------------------------------------------------------------------
// ValidationReport — the outcome of validate(). `valid()` is true iff there is no
// Error issue (Warnings alone keep the manifest valid). Issues are ordered as
// discovered (check 1 → 2 → 3 → 4), so the FIRST error is the earliest cause.
// ---------------------------------------------------------------------------
struct ValidationReport {
  std::vector<ValidationIssue> issues;

  bool valid() const {
    for (const auto& i : issues)
      if (i.severity == Severity::Error) return false;
    return true;
  }
  bool hasWarnings() const {
    for (const auto& i : issues)
      if (i.severity == Severity::Warning) return true;
    return false;
  }
  std::size_t errorCount() const {
    std::size_t n = 0;
    for (const auto& i : issues)
      if (i.severity == Severity::Error) ++n;
    return n;
  }
  std::size_t warningCount() const { return issues.size() - errorCount(); }

  // The first Error issue, or nullptr if the manifest is valid. (Convenience for a
  // repair loop that fixes one error at a time.)
  const ValidationIssue* firstError() const {
    for (const auto& i : issues)
      if (i.severity == Severity::Error) return &i;
    return nullptr;
  }

  // A single multi-line human-readable rendering of every issue (one per line,
  // "[SEVERITY check] node 'id' path: message"). For logs / repair-loop prompts.
  std::string toString() const;
};

// ---------------------------------------------------------------------------
// ManifestValidator — the stateless, data-free type checker. Construct once, call
// validate(jsonText) per candidate manifest; it owns no buffers and mutates no
// global state, so it is safe to reuse across many candidates in a repair loop.
// ---------------------------------------------------------------------------
class ManifestValidator {
 public:
  ManifestValidator() = default;

  // Run the four §6.1 checks over `jsonText`. Returns a structured report. NEVER
  // touches data — it reasons solely over the declared schema (column names +
  // dtypes, transform inferSchema, scale↔dtype matrix, pipeline format table).
  ValidationReport validate(const std::string& jsonText) const;
};

}  // namespace dc
