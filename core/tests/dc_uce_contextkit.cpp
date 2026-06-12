// ENC-620c (Epic ENC-620) — AI GRAMMAR-CARD / CONTEXT-KIT test.
//
// Proves the four deliverables:
//   1. The grammar card is CATALOG-DERIVED — known marks/scales/transforms/pipelines
//      appear in the generated card (assert specific live-catalog entries are
//      present, so a hardcoded stale list could not pass).
//   2. The feed schema descriptor carries the f64-time-stays-CPU rule.
//   3. The 2-3 worked manifests each PASS the ManifestValidator (few-shot anchors
//      cannot rot).
//   4. The repair-loop scaffold: a broken manifest yields a non-empty repair signal
//      (the validator report IS the signal), and the loop drives author->validate->
//      repair to a valid result.
#include "dc/manifest/ContextKit.hpp"
#include "dc/manifest/ManifestValidator.hpp"

#include <cstdio>
#include <string>

static int passed = 0;
static int failed = 0;

static void check(bool cond, const char* name) {
  if (cond) {
    std::printf("  PASS: %s\n", name);
    ++passed;
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", name);
    ++failed;
  }
}

static bool contains(const std::string& hay, const std::string& needle) {
  return hay.find(needle) != std::string::npos;
}

int main() {
  // ===== 1. GRAMMAR CARD is catalog-derived =================================
  {
    const std::string marks = dc::ContextKit::marksSection();
    // Marks come from markSpecOf over the Mark enum — assert known mark->pipeline
    // rows are present (these are read from the LIVE markSpecOf, not literals).
    check(contains(marks, "`candle`") && contains(marks, "instancedCandle@1"),
          "marks section has candle -> instancedCandle@1 (from markSpecOf)");
    check(contains(marks, "`point`") && contains(marks, "points@1"),
          "marks section has point -> points@1");
    check(contains(marks, "`rect`") && contains(marks, "instancedRect@1"),
          "marks section has rect -> instancedRect@1");
    check(contains(marks, "lineAA@1"),
          "marks section surfaces the lineAA variant");
    // required channels are the markSpecOf channel set
    check(contains(marks, "`open`") && contains(marks, "`high`") &&
              contains(marks, "`low`") && contains(marks, "`close`"),
          "candle's required OHLC channels are listed");
  }
  {
    const std::string scales = dc::ContextKit::scalesSection();
    check(contains(scales, "`linear`") && contains(scales, "f32, i32"),
          "scales section: linear accepts f32, i32");
    check(contains(scales, "`time`") && contains(scales, "timestamp"),
          "scales section: time accepts timestamp");
    check(contains(scales, "`band`") && contains(scales, "cat"),
          "scales section: band accepts cat");
    check(contains(scales, "`log`"), "scales section lists log");
  }
  {
    const std::string tx = dc::ContextKit::transformsSection();
    // Each op string is read from a REAL transform node's op() — assert the live
    // op names are present (a stale literal list could miss one).
    check(contains(tx, "`filter`"), "transforms section has filter");
    check(contains(tx, "`window`"), "transforms section has window");
    check(contains(tx, "`aggregate`"), "transforms section has aggregate");
    check(contains(tx, "`bin`"), "transforms section has bin");
    check(contains(tx, "`sort`"), "transforms section has sort");
    check(contains(tx, "`stack`"), "transforms section has stack");
    check(contains(tx, "`join`"), "transforms section has join");
    check(contains(tx, "class-1") && contains(tx, "class-3"),
          "transforms section carries streaming classes");
  }
  {
    const std::string pipes = dc::ContextKit::pipelinesSection();
    // The pipeline rows come from PipelineCatalog::keys() + each spec's format.
    check(contains(pipes, "instancedCandle@1") && contains(pipes, "candle6"),
          "pipelines section: instancedCandle@1 -> candle6 (from catalog)");
    check(contains(pipes, "points@1") && contains(pipes, "pos2_clip"),
          "pipelines section: points@1 -> pos2_clip");
    check(contains(pipes, "instancedRectColor@1") && contains(pipes, "rect4_color"),
          "pipelines section: instancedRectColor@1 -> rect4_color");
    check(contains(pipes, "lineAA@1"), "pipelines section lists lineAA@1");
  }
  {
    const std::string card = dc::ContextKit::grammarCardMarkdown();
    check(contains(card, "MARKS") && contains(card, "SCALES") &&
              contains(card, "TRANSFORMS") && contains(card, "PIPELINES"),
          "full grammar card concatenates all four sections");
  }

  // ===== 2. FEED SCHEMA DESCRIPTOR (f64-time-stays-CPU rule) ================
  {
    dc::FeedDescriptor f = dc::FeedDescriptor::demoOhlc();
    const std::string md = f.toMarkdown();
    check(contains(md, "`close`") && contains(md, "f32"),
          "feed descriptor lists OHLC fields + dtypes");
    check(contains(md, "timestamp") && contains(md, "time` scale"),
          "feed descriptor: timestamp bound only through a time scale");
    check(contains(md, "epoch-ms") && contains(md, "CPU"),
          "feed descriptor states the f64-time-stays-CPU rule");
  }

  // ===== 3. WORKED MANIFESTS each PASS the validator ========================
  {
    dc::ManifestValidator V;
    auto anchors = dc::ContextKit::workedManifests();
    check(anchors.size() >= 3, "at least 3 worked-manifest anchors");
    for (const auto& wm : anchors) {
      auto r = V.validate(wm.json);
      if (!r.valid())
        std::fprintf(stderr, "anchor '%s' invalid:\n%s", wm.title.c_str(),
                     r.toString().c_str());
      check(r.valid(), ("worked manifest PASSES validator: " + wm.title).c_str());
    }
  }

  // ===== 4. REPAIR-LOOP scaffold ===========================================
  {
    dc::ManifestValidator V;

    // A broken manifest (dangling scale ref) yields a NON-EMPTY repair signal that
    // IS the localized validator report.
    const char* broken = R"JSON({
      "version":"dc-manifest/1","id":"broke",
      "data":{"sources":[{"id":"src","columns":{"v":{"dtype":"f32"}}}]},
      "scales":[{"id":"sx","type":"linear","domain":[0,1],"range":"width"}],
      "marks":[{"id":"mk","type":"point","from":"src","pipeline":"points@1",
        "encoding":{"x":{"scale":"NOPE","field":"v"},"y":{"scale":"sx","field":"v"}}}]
    })JSON";
    auto rb = V.validate(broken);
    check(!rb.valid(), "broken manifest is invalid");
    std::string sig = dc::repairSignalFor(rb);
    check(!sig.empty(), "broken manifest yields a non-empty repair signal");
    check(contains(sig, "NOPE") || contains(sig, "does not resolve"),
          "repair signal is the localized validator report");

    // A valid manifest yields an EMPTY repair signal (nothing to repair).
    const char* good = R"JSON({
      "version":"dc-manifest/1","id":"ok",
      "data":{"sources":[{"id":"src","columns":{"v":{"dtype":"f32"}}}]},
      "scales":[{"id":"sx","type":"linear","domain":[0,1],"range":"width"}],
      "marks":[{"id":"mk","type":"point","from":"src","pipeline":"points@1",
        "encoding":{"x":{"scale":"sx","field":"v"},"y":{"scale":"sx","field":"v"}}}]
    })JSON";
    check(dc::repairSignalFor(V.validate(good)).empty(),
          "valid manifest yields an empty repair signal");

    // The execution-guided loop: a stub author emits broken FIRST, then (once it
    // receives a non-empty repair signal) the fixed manifest. The loop must reach a
    // valid candidate and return the full trace.
    int turns = 0;
    auto author = [&](const std::string& repairSignal) -> std::string {
      ++turns;
      // First turn: empty signal -> emit the broken manifest.
      if (repairSignal.empty() && turns == 1) return broken;
      // Subsequent turns: the report drove us here -> emit the fix.
      return good;
    };
    auto trace = dc::runRepairLoop(V, author, 4);
    check(trace.size() == 2, "repair loop took two turns (broken -> repaired)");
    check(!trace.front().done && !trace.front().repairSignal.empty(),
          "first attempt failed with a repair signal");
    check(trace.back().done, "final attempt validated (loop converged)");

    // A loop whose author NEVER fixes the manifest stops at maxAttempts (no hang).
    auto stubborn = [&](const std::string&) -> std::string { return broken; };
    auto t2 = dc::runRepairLoop(V, stubborn, 3);
    check(t2.size() == 3 && !t2.back().done,
          "non-converging loop stops at maxAttempts");
  }

  std::printf("\n%d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
