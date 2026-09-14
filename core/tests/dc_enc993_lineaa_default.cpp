// ENC-993 — lineAA@1 is the DEFAULT for line marks.
//
// WHY THIS TEST EXISTS
// --------------------
// The AA pipelines have shipped since ENC-490, but almost nothing reached them:
// `markSpecOf` fell back to line2d@1 unless a caller explicitly asked for
// LineStyle::LineAA, and every chart-line recipe (SMA, MACD, Bollinger, level
// lines, the generic LineRecipe, the axis ticks) HARDCODED "line2d@1". Since
// sampleCount == 1 everywhere, shader-side coverage is the only antialiasing
// that exists, so those lines rendered as raw 1px LineList aliased hairlines.
// ENC-587 claimed this landed; it did not. This test pins the inversion so it
// cannot silently regress again.
//
// THE CONTRACT ASSERTED HERE
//   1. The encode pass defaults a Line mark to lineAA@1 / Rect4.
//   2. LineStyle::Line2d is still an honoured, explicit ESCAPE HATCH.
//   3. A manifest with no "pipeline" key on a line mark gets lineAA@1;
//      a manifest that PINS "line2d@1" still gets line2d@1.
//   4. Every DATA-carrying line recipe binds lineAA@1 with rect4 geometry.
//   5. Non-data 1px chrome (crosshair) deliberately KEEPS line2d@1.
#include "dc/commands/CommandProcessor.hpp"
#include "dc/data/TableStore.hpp"
#include "dc/encode/EncodePass.hpp"
#include "dc/encode/Encoding.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/manifest/Manifest.hpp"
#include "dc/recipe/AxisRecipe.hpp"
#include "dc/recipe/BollingerRecipe.hpp"
#include "dc/recipe/CrosshairRecipe.hpp"
#include "dc/recipe/LevelLineRecipe.hpp"
#include "dc/recipe/LineRecipe.hpp"
#include "dc/recipe/MacdRecipe.hpp"
#include "dc/recipe/SmaRecipe.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/scene/Scene.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

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

// One 13-byte ingest APPEND record (op=1) — the existing wire format.
static void appendF32(dc::IngestProcessor& ingest, dc::Id buf,
                      const std::vector<float>& vals) {
  std::vector<std::uint8_t> wire;
  auto u32 = [&wire](std::uint32_t v) {
    wire.push_back(static_cast<std::uint8_t>(v & 0xFF));
    wire.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    wire.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    wire.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
  };
  const auto len = static_cast<std::uint32_t>(vals.size() * sizeof(float));
  wire.push_back(1);
  u32(static_cast<std::uint32_t>(buf));
  u32(0);
  u32(len);
  const auto* p = reinterpret_cast<const std::uint8_t*>(vals.data());
  wire.insert(wire.end(), p, p + len);
  ingest.processBatch(wire.data(), static_cast<std::uint32_t>(wire.size()));
}

// True if ANY command in `cmds` contains every one of `needles`.
static bool anyCmdHas(const std::vector<std::string>& cmds,
                      std::initializer_list<const char*> needles) {
  for (const auto& c : cmds) {
    bool all = true;
    for (const char* n : needles) {
      if (c.find(n) == std::string::npos) { all = false; break; }
    }
    if (all) return true;
  }
  return false;
}

// A recipe's `drawItemId` must be bound to `pipeline` and its geometry declared
// with `format`. Asserted on the emitted command strings — the recipes' only
// output — so this catches a hardcode anywhere in the build().
static void checkLineGroup(const std::vector<std::string>& cmds, dc::Id geomId,
                           dc::Id diId, const char* what) {
  const std::string g = std::to_string(geomId);
  const std::string d = std::to_string(diId);
  check(anyCmdHas(cmds, {"\"bindDrawItem\"", ("\"drawItemId\":" + d).c_str(),
                         "\"pipeline\":\"lineAA@1\""}),
        what);
  check(anyCmdHas(cmds, {"\"createGeometry\"", ("\"id\":" + g).c_str(),
                         "\"format\":\"rect4\""}),
        (std::string(what) + " — geometry is rect4").c_str());
}

int main() {
  std::printf("=== ENC-993 lineAA@1 is the default for line marks ===\n");

  // =========================================================================
  // 1. markSpecOf — the DEFAULT, and the explicit escape hatch.
  // =========================================================================
  {
    const dc::MarkSpec def = dc::markSpecOf(dc::Mark::Line);
    check(def.pipeline == "lineAA@1", "markSpecOf(Line) defaults to lineAA@1");
    check(def.format == dc::VertexFormat::Rect4,
          "markSpecOf(Line) defaults to Rect4");

    const dc::MarkSpec aa = dc::markSpecOf(dc::Mark::Line, dc::LineStyle::LineAA);
    check(aa.pipeline == "lineAA@1", "explicit LineAA still lineAA@1");

    const dc::MarkSpec flat =
        dc::markSpecOf(dc::Mark::Line, dc::LineStyle::Line2d);
    check(flat.pipeline == "line2d@1",
          "ESCAPE HATCH: explicit Line2d still resolves to line2d@1");
    check(flat.format == dc::VertexFormat::Pos2_Clip,
          "ESCAPE HATCH: explicit Line2d still packs Pos2_Clip");

    // Non-line marks must be untouched by the flip.
    check(dc::markSpecOf(dc::Mark::Point).pipeline == "points@1",
          "point mark unaffected");
    check(dc::markSpecOf(dc::Mark::Rect).pipeline == "instancedRect@1",
          "rect mark unaffected");
    check(dc::markSpecOf(dc::Mark::Candle).pipeline == "instancedCandle@1",
          "candle mark unaffected");
  }

  // =========================================================================
  // 2. EncodePass::compile with NO lineStyle argument -> lineAA@1 / Rect4.
  //    3 rows -> 2 segment instances, 16B each.
  // =========================================================================
  {
    dc::IngestProcessor ingest;
    dc::TableStore tables;
    auto src = dc::makeBufferByteSource(ingest);
    const dc::Id kTable = 1, kX = 10, kY = 11;
    tables.defineTable(kTable, "series");
    tables.addColumn(kTable, "x", dc::DType::F32, kX);
    tables.addColumn(kTable, "y", dc::DType::F32, kY);
    appendF32(ingest, kX, {0.0f, 1.0f, 2.0f});
    appendF32(ingest, kY, {0.0f, 3.0f, 1.0f});

    dc::Encoding enc;
    enc.field(dc::Channel::X, "x").field(dc::Channel::Y, "y");

    dc::EncodePass pass;
    auto res = pass.compile(dc::Mark::Line, enc, tables, kTable, src, 100, 200, 300);
    check(res.ok, "default compile of a line mark succeeds");
    check(res.drawItem.pipeline == "lineAA@1",
          "default compile binds lineAA@1 (was line2d@1)");
    check(res.geometry.format == dc::VertexFormat::Rect4,
          "default compile produces Rect4 geometry");
    check(res.instanceCount == 2, "default compile: 2 segment instances");
    check(res.bytes.size() == 2u * 16u, "default compile: 2 * 16B rect4 records");

    // The escape hatch, through the same entry point.
    auto flat = pass.compile(dc::Mark::Line, enc, tables, kTable, src, 101, 201,
                             301, nullptr, dc::LineStyle::Line2d);
    check(flat.ok && flat.drawItem.pipeline == "line2d@1" &&
              flat.geometry.format == dc::VertexFormat::Pos2_Clip,
          "ESCAPE HATCH: explicit Line2d compile still line2d@1 / Pos2_Clip");
  }

  // =========================================================================
  // 3. Manifest — an unpinned line mark defaults to lineAA@1; a manifest that
  //    PINS line2d@1 is still honoured (and still validates).
  // =========================================================================
  {
    static const char* kUnpinned = R"JSON(
{
  "version": "dc-manifest/1", "id": "m",
  "data": { "sources": [{
    "id": "s", "kind": "stream",
    "stream": { "rowKey": "x", "columns": {
      "x": {"from":"rowKey","dtype":"f32"},
      "y": {"from":"field:y","dtype":"f32"} } } }] },
  "scales":[{"id":"sx","type":"linear","domainFrom":{"data":"s","field":"x"},"range":"width"},
            {"id":"sy","type":"linear","domainFrom":{"data":"s","field":"y"},"range":"height"}],
  "coords":{"type":"cartesian"},
  "marks":[{"id":"l","type":"line","from":"s",
      "encoding":{"x":{"scale":"sx","field":"x"},"y":{"scale":"sy","field":"y"}}}]
}
)JSON";

    dc::Manifest m;
    auto lr = m.load(kUnpinned);
    if (!lr.ok()) std::fprintf(stderr, "    load error: %s\n", lr.message.c_str());
    check(lr.ok(), "manifest without a pipeline key loads");
    if (!lr.ok()) { std::printf("\nENC-993: %d passed, %d failed\n", passed, failed); return 1; }

    dc::IngestProcessor ingest;
    auto src = dc::makeBufferByteSource(ingest);
    appendF32(ingest, *m.columnBufferId("s", "x"), {0.0f, 1.0f, 2.0f});
    appendF32(ingest, *m.columnBufferId("s", "y"), {0.0f, 1.0f, 0.0f});
    auto br = m.build(src);
    check(br.ok(), "manifest without a pipeline key builds");
    check(m.compiledMarks().size() == 1, "one compiled mark");
    check(m.compiledMarks()[0].pipeline == "lineAA@1",
          "unpinned manifest line mark resolves to lineAA@1 (was line2d@1)");
    check(m.compiledMarks()[0].result.geometry.format == dc::VertexFormat::Rect4,
          "unpinned manifest line mark packs Rect4");
  }
  {
    static const char* kPinnedFlat = R"JSON(
{
  "version": "dc-manifest/1", "id": "m",
  "data": { "sources": [{
    "id": "s", "kind": "stream",
    "stream": { "rowKey": "x", "columns": {
      "x": {"from":"rowKey","dtype":"f32"},
      "y": {"from":"field:y","dtype":"f32"} } } }] },
  "scales":[{"id":"sx","type":"linear","domainFrom":{"data":"s","field":"x"},"range":"width"},
            {"id":"sy","type":"linear","domainFrom":{"data":"s","field":"y"},"range":"height"}],
  "coords":{"type":"cartesian"},
  "marks":[{"id":"l","type":"line","from":"s","pipeline":"line2d@1",
      "encoding":{"x":{"scale":"sx","field":"x"},"y":{"scale":"sy","field":"y"}}}]
}
)JSON";

    dc::Manifest m;
    auto lr = m.load(kPinnedFlat);
    if (!lr.ok()) std::fprintf(stderr, "    load error: %s\n", lr.message.c_str());
    check(lr.ok(),
          "ESCAPE HATCH: a manifest pinning line2d@1 still loads (not a mismatch)");
    if (!lr.ok()) { std::printf("\nENC-993: %d passed, %d failed\n", passed, failed); return 1; }
    dc::IngestProcessor ingest;
    auto src = dc::makeBufferByteSource(ingest);
    appendF32(ingest, *m.columnBufferId("s", "x"), {0.0f, 1.0f, 2.0f});
    appendF32(ingest, *m.columnBufferId("s", "y"), {0.0f, 1.0f, 0.0f});
    auto br = m.build(src);
    check(br.ok() && m.compiledMarks().size() == 1 &&
              m.compiledMarks()[0].pipeline == "line2d@1",
          "ESCAPE HATCH: pinned line2d@1 manifest mark stays line2d@1");
  }

  // =========================================================================
  // 4. The recipes. Every DATA-carrying line binds lineAA@1 with rect4.
  // =========================================================================
  {
    dc::SmaRecipeConfig cfg;
    cfg.layerId = 2;
    cfg.name = "sma";
    dc::SmaRecipe r(100, cfg);
    auto br = r.build();
    checkLineGroup(br.createCommands, r.geometryId(), r.drawItemId(),
                   "SmaRecipe line binds lineAA@1");
    check(br.subscriptions.size() == 1 &&
              br.subscriptions[0].format == dc::VertexFormat::Rect4,
          "SmaRecipe subscription format is Rect4");
  }
  {
    dc::MacdRecipeConfig cfg;
    cfg.lineLayerId = 2;
    cfg.histLayerId = 3;
    cfg.name = "macd";
    dc::MacdRecipe r(200, cfg);
    auto br = r.build();
    checkLineGroup(br.createCommands, r.macdLineGeomId(), r.macdLineDrawItemId(),
                   "MacdRecipe MACD line binds lineAA@1");
    checkLineGroup(br.createCommands, r.signalLineGeomId(),
                   r.signalLineDrawItemId(),
                   "MacdRecipe signal line binds lineAA@1");
    check(br.subscriptions.size() == 4 &&
              br.subscriptions[0].format == dc::VertexFormat::Rect4 &&
              br.subscriptions[1].format == dc::VertexFormat::Rect4,
          "MacdRecipe line subscriptions are Rect4");
  }
  {
    dc::BollingerRecipeConfig cfg;
    cfg.lineLayerId = 2;
    cfg.fillLayerId = 3;
    cfg.name = "bb";
    dc::BollingerRecipe r(300, cfg);
    auto br = r.build();
    checkLineGroup(br.createCommands, r.middleGeomId(), r.middleDrawItemId(),
                   "BollingerRecipe middle band binds lineAA@1");
    checkLineGroup(br.createCommands, r.upperGeomId(), r.upperDrawItemId(),
                   "BollingerRecipe upper band binds lineAA@1");
    checkLineGroup(br.createCommands, r.lowerGeomId(), r.lowerDrawItemId(),
                   "BollingerRecipe lower band binds lineAA@1");
    // The FILL stays triSolid@1 / pos2_clip — it is not a line.
    check(anyCmdHas(br.createCommands,
                    {"\"bindDrawItem\"",
                     ("\"drawItemId\":" + std::to_string(r.fillDrawItemId())).c_str(),
                     "\"pipeline\":\"triSolid@1\""}),
          "BollingerRecipe fill stays triSolid@1");
  }
  {
    dc::LevelLineRecipeConfig cfg;
    cfg.lineLayerId = 2;
    cfg.labelLayerId = 3;
    cfg.name = "lvl";
    dc::LevelLineRecipe r(400, cfg);
    auto br = r.build();
    checkLineGroup(br.createCommands, r.lineGeomId(), r.lineDrawItemId(),
                   "LevelLineRecipe binds lineAA@1");
  }
  {
    dc::LineRecipeConfig cfg;
    cfg.layerId = 2;
    cfg.name = "line";
    dc::LineRecipe r(500, cfg);
    auto br = r.build();
    checkLineGroup(br.createCommands, r.geometryId(), r.drawItemId(),
                   "LineRecipe binds lineAA@1");
    check(br.subscriptions.size() == 1 &&
              br.subscriptions[0].format == dc::VertexFormat::Rect4,
          "LineRecipe subscription format is Rect4");
  }
  {
    dc::AxisRecipeConfig cfg;
    cfg.tickLayerId = 2;
    cfg.labelLayerId = 3;
    cfg.name = "axis";
    dc::AxisRecipe r(600, cfg);
    auto br = r.build();
    checkLineGroup(br.createCommands, r.yTickGeomId(), r.yTickDrawItemId(),
                   "AxisRecipe Y ticks bind lineAA@1");
    checkLineGroup(br.createCommands, r.xTickGeomId(), r.xTickDrawItemId(),
                   "AxisRecipe X ticks bind lineAA@1");
    check(br.subscriptions.size() >= 2 &&
              br.subscriptions[0].format == dc::VertexFormat::Rect4 &&
              br.subscriptions[1].format == dc::VertexFormat::Rect4,
          "AxisRecipe tick subscriptions are Rect4");
  }

  // =========================================================================
  // 5. The ESCAPE HATCH in the recipe layer: 1px non-data chrome KEEPS
  //    line2d@1 on purpose. A crosshair is a cursor-follow hairline redrawn on
  //    every mouse move — quad expansion buys nothing and a 1px hairline is the
  //    intended look.
  // =========================================================================
  {
    dc::CrosshairRecipeConfig cfg;
    cfg.lineLayerId = 2;
    cfg.labelLayerId = 3;
    cfg.name = "xh";
    dc::CrosshairRecipe r(700, cfg);
    auto br = r.build();
    check(anyCmdHas(br.createCommands,
                    {"\"bindDrawItem\"",
                     ("\"drawItemId\":" + std::to_string(r.hLineDrawItemId())).c_str(),
                     "\"pipeline\":\"line2d@1\""}),
          "ESCAPE HATCH: crosshair H-line deliberately stays line2d@1");
    check(anyCmdHas(br.createCommands,
                    {"\"bindDrawItem\"",
                     ("\"drawItemId\":" + std::to_string(r.vLineDrawItemId())).c_str(),
                     "\"pipeline\":\"line2d@1\""}),
          "ESCAPE HATCH: crosshair V-line deliberately stays line2d@1");
  }

  // =========================================================================
  // 6. The converted recipes still produce a scene the CommandProcessor's
  //    validateDrawItem gate ACCEPTS (rect4 geometry + lineAA@1 pipeline).
  // =========================================================================
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    bool ok = cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})").ok &&
              cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1,"name":"L"})").ok;
    check(ok, "scene scaffold applies");

    dc::SmaRecipeConfig cfg;
    cfg.paneId = 1;
    cfg.layerId = 2;
    cfg.name = "sma";
    cfg.createTransform = true;
    dc::SmaRecipe r(800, cfg);
    bool allOk = true;
    for (const auto& c : r.build().createCommands) {
      if (!cp.applyJsonText(c).ok) allOk = false;
    }
    check(allOk, "SmaRecipe lineAA@1 commands are ACCEPTED by validateDrawItem");
    const auto* di = scene.getDrawItem(r.drawItemId());
    check(di != nullptr && di->pipeline == "lineAA@1",
          "live scene draw item is bound to lineAA@1");
    const auto* geo = scene.getGeometry(r.geometryId());
    check(geo != nullptr && geo->format == dc::VertexFormat::Rect4,
          "live scene geometry is Rect4");
  }

  std::printf("\nENC-993: %d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
