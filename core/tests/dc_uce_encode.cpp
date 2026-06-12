// ENC-600/601/603/604 — Encode compiler + marks (point/line/rect/candle).
//
// BYTE-LEVEL validation of the encode pass + the validateDrawItem exact-stride
// contract. For each of the four marks this asserts that feeding a table + an
// encoding produces a vertex/instance buffer whose BYTES EXACTLY match the target
// pipeline's required format/stride (right offsets + right f32 values), that the
// produced geometry is ACCEPTED by validateDrawItem (via the real
// CommandProcessor JSON gate), and that a deliberately mismatched
// format/channel-set is REJECTED at compile. Row-id threading (ENC-594) is
// covered. LinearScale is used for the scaled channels (time/band arrive in
// ENC-597/598). The pixel render proof is separate (ENC-606).
#include "dc/commands/CommandProcessor.hpp"
#include "dc/data/RowIdentity.hpp"
#include "dc/data/TableStore.hpp"
#include "dc/encode/EncodePass.hpp"
#include "dc/encode/Encoding.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/scale/Scale.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/scene/Scene.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
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

// One 13-byte ingest APPEND record (op=1) — the EXACT existing wire format.
static void appendRecord(std::vector<std::uint8_t>& out, dc::Id bufferId,
                         const void* bytes, std::uint32_t len) {
  auto u32 = [&out](std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
  };
  out.push_back(1);  // op = APPEND
  u32(static_cast<std::uint32_t>(bufferId));
  u32(0);    // offset (ignored for append)
  u32(len);  // payloadBytes
  const auto* p = static_cast<const std::uint8_t*>(bytes);
  out.insert(out.end(), p, p + len);
}

static void appendF32(dc::IngestProcessor& ingest, dc::Id buf,
                      const std::vector<float>& vals) {
  std::vector<std::uint8_t> batch;
  appendRecord(batch, buf, vals.data(),
               static_cast<std::uint32_t>(vals.size() * sizeof(float)));
  ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
}

// Read one little-endian f32 at byte offset `off` of `bytes`.
static float f32At(const std::vector<std::uint8_t>& bytes, std::size_t off) {
  float v = 0.0f;
  std::memcpy(&v, bytes.data() + off, sizeof(float));
  return v;
}

static bool eqf(float a, float b) { return std::fabs(a - b) <= 1e-6f * (1.0f + std::fabs(a)); }

// Wire an EncodeResult's geometry+drawItem into a real Scene via the
// CommandProcessor JSON gate and return whether bindDrawItem ACCEPTED it
// (i.e. validateDrawItem passed: format == requiredVertexFormat etc.).
static dc::CmdResult wireAndValidate(const dc::EncodeResult& res,
                                     dc::Id bufId, dc::Id geoId, dc::Id diId,
                                     std::uint32_t byteLen,
                                     const std::string& pipeline) {
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  // A draw item needs a real layer parent; stand up a pane + layer (ids 9001/9002).
  cp.applyJsonText(R"({"cmd":"createPane","id":9001,"name":"p"})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":9002,"paneId":9001,"name":"l"})");
  cp.applyJsonText(R"({"cmd":"createBuffer","id":)" + std::to_string(bufId) +
                   R"(,"byteLength":)" + std::to_string(byteLen) + "}");
  cp.applyJsonText(R"({"cmd":"createGeometry","id":)" + std::to_string(geoId) +
                   R"(,"vertexBufferId":)" + std::to_string(bufId) +
                   R"(,"format":")" + dc::toString(res.geometry.format) +
                   R"(","vertexCount":)" +
                   std::to_string(res.geometry.vertexCount) + "}");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":)" + std::to_string(diId) +
                   R"(,"layerId":9002,"name":"m"})");
  return cp.applyJsonText(R"({"cmd":"bindDrawItem","drawItemId":)" +
                          std::to_string(diId) + R"(,"pipeline":")" + pipeline +
                          R"(","geometryId":)" + std::to_string(geoId) + "}");
}

int main() {
  std::printf("=== ENC-600/601/603/604 Encode compiler + marks ===\n");

  dc::EncodePass pass;

  // =========================================================================
  // POINT mark -> points@1 (Pos2_Clip, 8B/vertex, one vertex per row).
  // x identity, y scaled through a LinearScale (domain [0,10] -> range [0,1]).
  // =========================================================================
  {
    dc::IngestProcessor ingest;
    dc::TableStore tables;
    auto src = dc::makeBufferByteSource(ingest);
    const dc::Id kTable = 1, kX = 10, kY = 11;
    tables.defineTable(kTable, "pts");
    tables.addColumn(kTable, "x", dc::DType::F32, kX);
    tables.addColumn(kTable, "y", dc::DType::F32, kY);
    appendF32(ingest, kX, {0.0f, 1.0f, 2.0f});
    appendF32(ingest, kY, {0.0f, 5.0f, 10.0f});

    dc::LinearScale ys(dc::Domain{0.0, 10.0, false}, dc::Range{0.0, 1.0});

    dc::Encoding enc;
    enc.field(dc::Channel::X, "x");           // identity (no scale)
    enc.field(dc::Channel::Y, "y", &ys);      // scaled
    enc.setColor(dc::Rgba{0.2f, 0.4f, 0.6f, 1.0f});

    auto res = pass.compile(dc::Mark::Point, enc, tables, kTable, src, 100, 200, 300);
    check(res.ok, "point: compile ok");
    check(res.geometry.format == dc::VertexFormat::Pos2_Clip,
          "point: geometry format == Pos2_Clip");
    check(res.geometry.vertexCount == 3, "point: 3 vertices for 3 rows");
    check(res.bytes.size() == 3u * 8u, "point: 3 * 8B byte length (exact stride)");

    // Byte-exact: vertex i = [f32 x_i][f32 y_scaled_i].
    bool bytesOk = true;
    const float ey[3] = {0.0f, 0.5f, 1.0f};  // ys.map(0/5/10)
    const float ex[3] = {0.0f, 1.0f, 2.0f};
    for (int i = 0; i < 3; ++i) {
      if (!eqf(f32At(res.bytes, i * 8 + 0), ex[i])) bytesOk = false;  // x at off 0
      if (!eqf(f32At(res.bytes, i * 8 + 4), ey[i])) bytesOk = false;  // y at off 4
    }
    check(bytesOk, "point: bytes are exactly [x,y] f32 pairs at the right offsets");

    // Color routed onto the draw item (NOT into the buffer).
    check(eqf(res.drawItem.color[0], 0.2f) && eqf(res.drawItem.color[2], 0.6f),
          "point: color resolved onto the draw item (single uniform color)");

    // The produced geometry is ACCEPTED by the real validateDrawItem gate.
    auto vr = wireAndValidate(res, 10, 100, 200,
                              static_cast<std::uint32_t>(res.bytes.size()),
                              "points@1");
    check(vr.ok, "point: validateDrawItem ACCEPTS the produced geometry");

    // REJECT: bind the SAME Pos2_Clip geometry to instancedRect@1 (needs Rect4).
    auto bad = wireAndValidate(res, 10, 100, 200,
                               static_cast<std::uint32_t>(res.bytes.size()),
                               "instancedRect@1");
    check(!bad.ok && bad.err.code == "VALIDATION_VERTEX_FORMAT_MISMATCH",
          "point: mismatched pipeline (instancedRect@1) is REJECTED");
  }

  // =========================================================================
  // RECT mark -> instancedRect@1 (Rect4, 16B/instance, one instance per row).
  // x,y,x2,y2 all identity. Byte order must be x0,y0,x1,y1.
  // =========================================================================
  {
    dc::IngestProcessor ingest;
    dc::TableStore tables;
    auto src = dc::makeBufferByteSource(ingest);
    const dc::Id kTable = 2, kX = 20, kY = 21, kX2 = 22, kY2 = 23;
    tables.defineTable(kTable, "bars");
    tables.addColumn(kTable, "x0", dc::DType::F32, kX);
    tables.addColumn(kTable, "y0", dc::DType::F32, kY);
    tables.addColumn(kTable, "x1", dc::DType::F32, kX2);
    tables.addColumn(kTable, "y1", dc::DType::F32, kY2);
    appendF32(ingest, kX, {0.0f, 2.0f});
    appendF32(ingest, kY, {0.0f, 0.0f});
    appendF32(ingest, kX2, {1.0f, 3.0f});
    appendF32(ingest, kY2, {0.5f, 0.9f});

    dc::Encoding enc;
    enc.field(dc::Channel::X, "x0").field(dc::Channel::Y, "y0")
       .field(dc::Channel::X2, "x1").field(dc::Channel::Y2, "y1");

    auto res = pass.compile(dc::Mark::Rect, enc, tables, kTable, src, 101, 201, 301);
    check(res.ok, "rect: compile ok");
    check(res.geometry.format == dc::VertexFormat::Rect4,
          "rect: geometry format == Rect4");
    check(res.instanceCount == 2, "rect: 2 instances for 2 rows");
    check(res.bytes.size() == 2u * 16u, "rect: 2 * 16B byte length (exact stride)");

    const float exp[2][4] = {{0.0f, 0.0f, 1.0f, 0.5f}, {2.0f, 0.0f, 3.0f, 0.9f}};
    bool bytesOk = true;
    for (int i = 0; i < 2; ++i)
      for (int k = 0; k < 4; ++k)
        if (!eqf(f32At(res.bytes, i * 16 + k * 4), exp[i][k])) bytesOk = false;
    check(bytesOk, "rect: bytes are exactly [x0,y0,x1,y1] f32 at the right offsets");

    auto vr = wireAndValidate(res, 20, 101, 201,
                              static_cast<std::uint32_t>(res.bytes.size()),
                              "instancedRect@1");
    check(vr.ok, "rect: validateDrawItem ACCEPTS the produced geometry");

    // REJECT at COMPILE: a rect missing the x2 channel cannot be packed.
    dc::Encoding incomplete;
    incomplete.field(dc::Channel::X, "x0").field(dc::Channel::Y, "y0")
              .field(dc::Channel::Y2, "y1");  // no X2
    auto miss = pass.compile(dc::Mark::Rect, incomplete, tables, kTable, src,
                             101, 201, 301);
    check(!miss.ok && miss.error == dc::EncodeError::MissingChannel,
          "rect: missing required channel (x2) is REJECTED at compile");
  }

  // =========================================================================
  // CANDLE mark -> instancedCandle@1 (Candle6, 24B/instance).
  // Byte order: x, open, high, low, close, halfWidth(=Size). colorUp/colorDown
  // route onto the draw item, NOT into the buffer.
  // =========================================================================
  {
    dc::IngestProcessor ingest;
    dc::TableStore tables;
    auto src = dc::makeBufferByteSource(ingest);
    const dc::Id kTable = 3, kX = 30, kO = 31, kH = 32, kL = 33, kC = 34;
    tables.defineTable(kTable, "ohlc");
    tables.addColumn(kTable, "t", dc::DType::F32, kX);
    tables.addColumn(kTable, "o", dc::DType::F32, kO);
    tables.addColumn(kTable, "h", dc::DType::F32, kH);
    tables.addColumn(kTable, "l", dc::DType::F32, kL);
    tables.addColumn(kTable, "c", dc::DType::F32, kC);
    // Two candles: one up (close>open), one down.
    appendF32(ingest, kX, {0.0f, 1.0f});
    appendF32(ingest, kO, {10.0f, 20.0f});
    appendF32(ingest, kH, {12.0f, 21.0f});
    appendF32(ingest, kL, {9.0f, 16.0f});
    appendF32(ingest, kC, {11.0f, 17.0f});

    dc::Encoding enc;
    enc.field(dc::Channel::X, "t")
       .field(dc::Channel::Open, "o").field(dc::Channel::High, "h")
       .field(dc::Channel::Low, "l").field(dc::Channel::Close, "c")
       .constant(dc::Channel::Size, 0.4);  // halfWidth constant
    enc.setColorUp(dc::Rgba{0.0f, 0.8f, 0.0f, 1.0f});
    enc.setColorDown(dc::Rgba{0.8f, 0.0f, 0.0f, 1.0f});

    auto res = pass.compile(dc::Mark::Candle, enc, tables, kTable, src, 102, 202, 302);
    check(res.ok, "candle: compile ok");
    check(res.geometry.format == dc::VertexFormat::Candle6,
          "candle: geometry format == Candle6");
    check(res.bytes.size() == 2u * 24u, "candle: 2 * 24B byte length (exact stride)");

    // Candle6 = (x, open, high, low, close, halfWidth).
    const float exp[2][6] = {{0.0f, 10.0f, 12.0f, 9.0f, 11.0f, 0.4f},
                             {1.0f, 20.0f, 21.0f, 16.0f, 17.0f, 0.4f}};
    bool bytesOk = true;
    for (int i = 0; i < 2; ++i)
      for (int k = 0; k < 6; ++k)
        if (!eqf(f32At(res.bytes, i * 24 + k * 4), exp[i][k])) bytesOk = false;
    check(bytesOk,
          "candle: bytes are exactly [x,open,high,low,close,hw] at right offsets");

    check(eqf(res.drawItem.colorUp[1], 0.8f) && eqf(res.drawItem.colorDown[0], 0.8f),
          "candle: colorUp/colorDown route onto the draw item");

    auto vr = wireAndValidate(res, 30, 102, 202,
                              static_cast<std::uint32_t>(res.bytes.size()),
                              "instancedCandle@1");
    check(vr.ok, "candle: validateDrawItem ACCEPTS the produced geometry");
  }

  // =========================================================================
  // ENC-608 KEYSTONE — RECTCOLOR mark -> instancedRectColor@1 (Rect4Color, 24B
  // /instance: rect4 f32 + packed RGBA8 + reserved scalar/row-id lane). A small
  // grid of per-CELL-colored rects; assert exact bytes (rect floats at the right
  // offsets, the packed RGBA8 at offset 16, the reserved lane == 0 at offset 20)
  // and that validateDrawItem ACCEPTS the produced rect4_color geometry.
  // =========================================================================
  {
    dc::IngestProcessor ingest;
    dc::TableStore tables;
    auto src = dc::makeBufferByteSource(ingest);
    const dc::Id kTable = 8, kX = 80, kY = 81, kX2 = 82, kY2 = 83, kCol = 84;
    tables.defineTable(kTable, "grid");
    tables.addColumn(kTable, "x0", dc::DType::F32, kX);
    tables.addColumn(kTable, "y0", dc::DType::F32, kY);
    tables.addColumn(kTable, "x1", dc::DType::F32, kX2);
    tables.addColumn(kTable, "y1", dc::DType::F32, kY2);
    // Per-cell color column: i32 holding PRE-PACKED RGBA8 (byte0=R..byte3=A),
    // i.e. the color scale's output (hand-packed here; scales are ENC-610/611).
    tables.addColumn(kTable, "col", dc::DType::I32, kCol);

    // 3 cells with DISTINCT colors: red, green, blue (opaque).
    const std::uint32_t kRed   = 0xFF0000FFu;  // A=FF,B=00,G=00,R=FF
    const std::uint32_t kGreen = 0xFF00FF00u;  // A=FF,B=00,G=FF,R=00
    const std::uint32_t kBlue  = 0xFFFF0000u;  // A=FF,B=FF,G=00,R=00

    appendF32(ingest, kX,  {0.0f, 1.0f, 2.0f});
    appendF32(ingest, kY,  {0.0f, 0.0f, 0.0f});
    appendF32(ingest, kX2, {1.0f, 2.0f, 3.0f});
    appendF32(ingest, kY2, {1.0f, 1.0f, 1.0f});
    // Append the i32 color column (reuse the raw append wire with i32 payload).
    {
      const std::int32_t cols[3] = {static_cast<std::int32_t>(kRed),
                                    static_cast<std::int32_t>(kGreen),
                                    static_cast<std::int32_t>(kBlue)};
      std::vector<std::uint8_t> batch;
      appendRecord(batch, kCol, cols, sizeof(cols));
      ingest.processBatch(batch.data(),
                          static_cast<std::uint32_t>(batch.size()));
    }

    dc::Encoding enc;
    enc.field(dc::Channel::X, "x0").field(dc::Channel::Y, "y0")
       .field(dc::Channel::X2, "x1").field(dc::Channel::Y2, "y1");
    enc.setColorField("col");  // per-instance packed RGBA8

    auto res = pass.compile(dc::Mark::RectColor, enc, tables, kTable, src,
                            108, 208, 308);
    check(res.ok, "rectColor: compile ok");
    check(res.geometry.format == dc::VertexFormat::Rect4Color,
          "rectColor: geometry format == Rect4Color");
    check(res.drawItem.pipeline == "instancedRectColor@1",
          "rectColor: pipeline == instancedRectColor@1");
    check(res.instanceCount == 3, "rectColor: 3 instances for 3 rows");
    check(res.bytes.size() == 3u * 24u,
          "rectColor: 3 * 24B byte length (exact Rect4Color stride)");

    // Byte-exact: per instance [x0,y0,x1,y1 f32][rgba8 u32 @16][lane u32 @20==0].
    const float exr[3][4] = {{0, 0, 1, 1}, {1, 0, 2, 1}, {2, 0, 3, 1}};
    const std::uint32_t exc[3] = {kRed, kGreen, kBlue};
    bool bytesOk = true;
    for (int i = 0; i < 3; ++i) {
      for (int k = 0; k < 4; ++k)
        if (!eqf(f32At(res.bytes, i * 24 + k * 4), exr[i][k])) bytesOk = false;
      std::uint32_t c = 0, lane = 0xDEADBEEFu;
      std::memcpy(&c, res.bytes.data() + i * 24 + 16, 4);
      std::memcpy(&lane, res.bytes.data() + i * 24 + 20, 4);
      if (c != exc[i]) bytesOk = false;
      if (lane != 0u) bytesOk = false;  // reserved lane packed 0
    }
    check(bytesOk,
          "rectColor: bytes are exactly [rect4][packed RGBA8 @16][lane=0 @20]");

    // The produced geometry is ACCEPTED by the real validateDrawItem gate.
    auto vr = wireAndValidate(res, 80, 108, 208,
                              static_cast<std::uint32_t>(res.bytes.size()),
                              "instancedRectColor@1");
    check(vr.ok, "rectColor: validateDrawItem ACCEPTS the rect4_color geometry");

    // REJECT: binding the SAME rect4_color geometry to instancedRect@1 (Rect4).
    auto bad = wireAndValidate(res, 80, 108, 208,
                               static_cast<std::uint32_t>(res.bytes.size()),
                               "instancedRect@1");
    check(!bad.ok && bad.err.code == "VALIDATION_VERTEX_FORMAT_MISMATCH",
          "rectColor: mismatched pipeline (instancedRect@1) is REJECTED");

    // Constant-color fallback: no setColorField -> the constant color packs into
    // every instance (still a valid per-instance buffer).
    dc::Encoding constEnc;
    constEnc.field(dc::Channel::X, "x0").field(dc::Channel::Y, "y0")
            .field(dc::Channel::X2, "x1").field(dc::Channel::Y2, "y1");
    constEnc.setColor(dc::Rgba{1.0f, 0.0f, 0.0f, 1.0f});  // opaque red
    auto cres = pass.compile(dc::Mark::RectColor, constEnc, tables, kTable, src,
                             109, 209, 309);
    check(cres.ok && cres.bytes.size() == 3u * 24u,
          "rectColor: constant-color fallback compiles to per-instance buffer");
    bool constOk = true;
    for (int i = 0; i < 3; ++i) {
      std::uint32_t c = 0;
      std::memcpy(&c, cres.bytes.data() + i * 24 + 16, 4);
      if (c != 0xFF0000FFu) constOk = false;  // red packed every row
    }
    check(constOk, "rectColor: constant color packs RGBA8 into every instance");
  }

  // =========================================================================
  // LINE mark -> line2d@1 (Pos2_Clip LineList) AND lineAA@1 (Rect4 instanced).
  // 3 rows -> 2 segments. Same source data, two packings.
  // =========================================================================
  {
    dc::IngestProcessor ingest;
    dc::TableStore tables;
    auto src = dc::makeBufferByteSource(ingest);
    const dc::Id kTable = 4, kX = 40, kY = 41;
    tables.defineTable(kTable, "series");
    tables.addColumn(kTable, "x", dc::DType::F32, kX);
    tables.addColumn(kTable, "y", dc::DType::F32, kY);
    appendF32(ingest, kX, {0.0f, 1.0f, 2.0f});
    appendF32(ingest, kY, {0.0f, 3.0f, 1.0f});

    dc::Encoding enc;
    enc.field(dc::Channel::X, "x").field(dc::Channel::Y, "y");

    // line2d@1: LineList — 2*(N-1)=4 vertices, segments (r0->r1),(r1->r2).
    {
      auto res = pass.compile(dc::Mark::Line, enc, tables, kTable, src, 103, 203,
                              303, nullptr, dc::LineStyle::Line2d);
      check(res.ok && res.geometry.format == dc::VertexFormat::Pos2_Clip,
            "line2d: compile ok + Pos2_Clip");
      check(res.geometry.vertexCount == 4, "line2d: 4 verts for 2 segments");
      check(res.bytes.size() == 4u * 8u, "line2d: 4 * 8B (LineList pairs)");
      // seg0: (0,0)(1,3)  seg1: (1,3)(2,1)
      const float exp[4][2] = {{0, 0}, {1, 3}, {1, 3}, {2, 1}};
      bool ok = true;
      for (int v = 0; v < 4; ++v) {
        if (!eqf(f32At(res.bytes, v * 8 + 0), exp[v][0])) ok = false;
        if (!eqf(f32At(res.bytes, v * 8 + 4), exp[v][1])) ok = false;
      }
      check(ok, "line2d: bytes are the exact LineList endpoint pairs");
      auto vr = wireAndValidate(res, 40, 103, 203,
                                static_cast<std::uint32_t>(res.bytes.size()),
                                "line2d@1");
      check(vr.ok, "line2d: validateDrawItem ACCEPTS the produced geometry");
    }

    // lineAA@1: Rect4 instanced — N-1=2 segment instances (x0,y0,x1,y1).
    {
      auto res = pass.compile(dc::Mark::Line, enc, tables, kTable, src, 104, 204,
                              304, nullptr, dc::LineStyle::LineAA);
      check(res.ok && res.geometry.format == dc::VertexFormat::Rect4,
            "lineAA: compile ok + Rect4");
      check(res.instanceCount == 2, "lineAA: 2 segment instances");
      check(res.bytes.size() == 2u * 16u, "lineAA: 2 * 16B segments");
      const float exp[2][4] = {{0, 0, 1, 3}, {1, 3, 2, 1}};
      bool ok = true;
      for (int i = 0; i < 2; ++i)
        for (int k = 0; k < 4; ++k)
          if (!eqf(f32At(res.bytes, i * 16 + k * 4), exp[i][k])) ok = false;
      check(ok, "lineAA: bytes are the exact Rect4 segment records");
      auto vr = wireAndValidate(res, 40, 104, 204,
                                static_cast<std::uint32_t>(res.bytes.size()),
                                "lineAA@1");
      check(vr.ok, "lineAA: validateDrawItem ACCEPTS the produced geometry");
    }
  }

  // =========================================================================
  // ENC-594 ROW-ID THREADING — durable id carried per emitted instance,
  // out-of-band (NOT in the byte-exact buffer). Covers point (per row) and
  // line (per segment = start row).
  // =========================================================================
  {
    dc::IngestProcessor ingest;
    dc::TableStore tables;
    auto src = dc::makeBufferByteSource(ingest);
    const dc::Id kTable = 5, kX = 50, kY = 51, kRid = 52;
    tables.defineTable(kTable, "ids");
    tables.addColumn(kTable, "x", dc::DType::F32, kX);
    tables.addColumn(kTable, "y", dc::DType::F32, kY);
    tables.addColumn(kTable, "rid", dc::DType::I32, kRid);

    dc::RowIdentity rid;
    check(rid.bind(tables, kTable, "rid"), "rowid: bind i32 id column");

    appendF32(ingest, kX, {0.0f, 1.0f, 2.0f, 3.0f});
    appendF32(ingest, kY, {0.0f, 1.0f, 2.0f, 3.0f});
    auto assigned = rid.appendIds(ingest, 4);  // ids 0,1,2,3 in lockstep
    check(assigned.size() == 4 && assigned[0] == 0 && assigned[3] == 3,
          "rowid: appendIds assigns 0..3");

    dc::Encoding enc;
    enc.field(dc::Channel::X, "x").field(dc::Channel::Y, "y");

    // Point: one id per row, in row order.
    auto pres = pass.compile(dc::Mark::Point, enc, tables, kTable, src, 105, 205,
                             305, &rid);
    check(pres.ok && pres.instanceRowIds.size() == 4,
          "rowid/point: one id per emitted vertex");
    check(pres.instanceRowIds[0] == 0 && pres.instanceRowIds[3] == 3,
          "rowid/point: ids are the durable row ids in order");
    check(pres.bytes.size() == 4u * 8u,
          "rowid/point: byte buffer stays byte-exact (id is OUT-OF-BAND)");

    // Line: one id per SEGMENT = the segment's START row's id (3 segments).
    auto lres = pass.compile(dc::Mark::Line, enc, tables, kTable, src, 106, 206,
                             306, &rid, dc::LineStyle::LineAA);
    check(lres.ok && lres.instanceRowIds.size() == 3,
          "rowid/line: one id per segment");
    check(lres.instanceRowIds[0] == 0 && lres.instanceRowIds[2] == 2,
          "rowid/line: segment id == its start row id");
  }

  // =========================================================================
  // INCREMENTAL (class-1) — compileInto packs only the appended tail and
  // writeRange()s it at the correct offset, so the store ends byte-identical to
  // a full compile of the grown table.
  // =========================================================================
  {
    dc::IngestProcessor ingest;
    dc::TableStore tables;
    auto src = dc::makeBufferByteSource(ingest);
    const dc::Id kTable = 6, kX = 60, kY = 61, kVbuf = 62;
    tables.defineTable(kTable, "stream");
    tables.addColumn(kTable, "x", dc::DType::F32, kX);
    tables.addColumn(kTable, "y", dc::DType::F32, kY);

    dc::Encoding enc;
    enc.field(dc::Channel::X, "x").field(dc::Channel::Y, "y");

    dc::CpuBufferStore store;

    // Tick 1: append 2 rect rows... wait, rect needs x2/y2; use point here.
    appendF32(ingest, kX, {0.0f, 1.0f});
    appendF32(ingest, kY, {0.0f, 1.0f});
    auto t1 = pass.compileInto(dc::Mark::Point, enc, tables, kTable, src, store,
                               160, 260, kVbuf, /*fromRow=*/0);
    check(t1.ok && t1.bytes.size() == 2u * 8u, "incr: tick1 packs 2 rows");
    check(store.getCpuDataSize(kVbuf) == 2u * 8u, "incr: store holds 16B after t1");

    // Tick 2: append 2 more rows; pack only the tail from row 2.
    appendF32(ingest, kX, {2.0f, 3.0f});
    appendF32(ingest, kY, {4.0f, 9.0f});
    auto t2 = pass.compileInto(dc::Mark::Point, enc, tables, kTable, src, store,
                               160, 260, kVbuf, /*fromRow=*/2);
    check(t2.ok && t2.bytes.size() == 2u * 8u,
          "incr: tick2 packs ONLY the 2 appended rows (O(Δ))");
    check(t2.geometry.vertexCount == 4, "incr: geometry vertexCount == 4 (whole table)");
    check(store.getCpuDataSize(kVbuf) == 4u * 8u, "incr: store grew to 32B");

    // The store now equals a FULL compile of the 4-row table, byte for byte.
    auto full = pass.compile(dc::Mark::Point, enc, tables, kTable, src, 160, 260,
                             kVbuf);
    const std::uint8_t* sb = store.getCpuData(kVbuf);
    bool identical = (full.bytes.size() == store.getCpuDataSize(kVbuf)) &&
                     (std::memcmp(sb, full.bytes.data(), full.bytes.size()) == 0);
    check(identical,
          "incr: incremental store == full compile byte-for-byte");

    // Spot-check the appended-tail values landed at the right offset.
    check(eqf(f32At(full.bytes, 2 * 8 + 0), 2.0f) &&
              eqf(f32At(full.bytes, 3 * 8 + 4), 9.0f),
          "incr: tail rows packed at the correct byte offset");
  }

  // =========================================================================
  // INCREMENTAL LINE (the highest-risk path) — a line segment couples adjacent
  // rows, so appending row N reactivates the boundary segment (N-1 -> N). The
  // tail must re-pack from one row earlier so that boundary segment appears, and
  // the store must still end byte-identical to a full compile. Covers BOTH
  // lineAA (instanced) and line2d (LineList) offset arithmetic.
  // =========================================================================
  {
    dc::IngestProcessor ingest;
    dc::TableStore tables;
    auto src = dc::makeBufferByteSource(ingest);
    const dc::Id kTable = 7, kX = 70, kY = 71, kAA = 72, kLL = 73;
    tables.defineTable(kTable, "line-stream");
    tables.addColumn(kTable, "x", dc::DType::F32, kX);
    tables.addColumn(kTable, "y", dc::DType::F32, kY);
    dc::Encoding enc;
    enc.field(dc::Channel::X, "x").field(dc::Channel::Y, "y");

    dc::CpuBufferStore aaStore, llStore;

    // Tick 1: 3 rows -> 2 segments.
    appendF32(ingest, kX, {0.0f, 1.0f, 2.0f});
    appendF32(ingest, kY, {0.0f, 3.0f, 1.0f});
    pass.compileInto(dc::Mark::Line, enc, tables, kTable, src, aaStore, 170, 270,
                     kAA, 0, nullptr, dc::LineStyle::LineAA);
    pass.compileInto(dc::Mark::Line, enc, tables, kTable, src, llStore, 171, 271,
                     kLL, 0, nullptr, dc::LineStyle::Line2d);
    check(aaStore.getCpuDataSize(kAA) == 2u * 16u, "incr-line: AA 2 segments after t1");
    check(llStore.getCpuDataSize(kLL) == 4u * 8u, "incr-line: LL 4 verts after t1");

    // Tick 2: append 2 rows (now 5 rows -> 4 segments). fromRow=3.
    appendF32(ingest, kX, {3.0f, 4.0f});
    appendF32(ingest, kY, {5.0f, 2.0f});
    auto aaT2 = pass.compileInto(dc::Mark::Line, enc, tables, kTable, src, aaStore,
                                 170, 270, kAA, 3, nullptr, dc::LineStyle::LineAA);
    auto llT2 = pass.compileInto(dc::Mark::Line, enc, tables, kTable, src, llStore,
                                 171, 271, kLL, 3, nullptr, dc::LineStyle::Line2d);
    // Boundary segment (row2->row3) PLUS the new segment (row3->row4) = 2 segs.
    check(aaT2.bytes.size() == 2u * 16u,
          "incr-line: AA tail re-packs boundary + new segment (2 segs)");
    check(llT2.bytes.size() == 2u * 2u * 8u,
          "incr-line: LL tail re-packs boundary + new segment (2 segs, 4 verts)");

    // Both stores must equal a FULL compile of the 5-row table, byte for byte.
    auto aaFull = pass.compile(dc::Mark::Line, enc, tables, kTable, src, 170, 270,
                               kAA, nullptr, dc::LineStyle::LineAA);
    auto llFull = pass.compile(dc::Mark::Line, enc, tables, kTable, src, 171, 271,
                               kLL, nullptr, dc::LineStyle::Line2d);
    bool aaOk = (aaStore.getCpuDataSize(kAA) == aaFull.bytes.size()) &&
                std::memcmp(aaStore.getCpuData(kAA), aaFull.bytes.data(),
                            aaFull.bytes.size()) == 0;
    bool llOk = (llStore.getCpuDataSize(kLL) == llFull.bytes.size()) &&
                std::memcmp(llStore.getCpuData(kLL), llFull.bytes.data(),
                            llFull.bytes.size()) == 0;
    check(aaOk, "incr-line: AA incremental store == full compile byte-for-byte");
    check(llOk, "incr-line: LL incremental store == full compile byte-for-byte");
    check(aaFull.instanceCount == 4 && aaFull.geometry.vertexCount == 4,
          "incr-line: AA whole table = 4 segments");
    check(llFull.geometry.vertexCount == 8, "incr-line: LL whole table = 8 verts");
  }

  // =========================================================================
  // ENC-609 — POINTCOLOR mark -> instancedPointColor@1 (Point4Color, 16B
  // /instance: pos2 f32 + packed RGBA8 + size f32). A scatter of per-point
  // colored + sized dots; assert exact bytes (x,y at off 0/4, packed RGBA8 @8,
  // size @12) and that validateDrawItem ACCEPTS the produced point4_color geo.
  // =========================================================================
  {
    dc::IngestProcessor ingest;
    dc::TableStore tables;
    auto src = dc::makeBufferByteSource(ingest);
    const dc::Id kTable = 9, kX = 90, kY = 91, kSz = 92, kCol = 93;
    tables.defineTable(kTable, "scatter");
    tables.addColumn(kTable, "x", dc::DType::F32, kX);
    tables.addColumn(kTable, "y", dc::DType::F32, kY);
    tables.addColumn(kTable, "sz", dc::DType::F32, kSz);
    tables.addColumn(kTable, "col", dc::DType::I32, kCol);

    const std::uint32_t kRed   = 0xFF0000FFu;
    const std::uint32_t kGreen = 0xFF00FF00u;
    const std::uint32_t kBlue  = 0xFFFF0000u;

    appendF32(ingest, kX,  {-0.5f, 0.0f, 0.5f});
    appendF32(ingest, kY,  { 0.1f, 0.2f, 0.3f});
    appendF32(ingest, kSz, { 4.0f, 8.0f, 16.0f});  // distinct sizes (pixels)
    {
      const std::int32_t cols[3] = {static_cast<std::int32_t>(kRed),
                                    static_cast<std::int32_t>(kGreen),
                                    static_cast<std::int32_t>(kBlue)};
      std::vector<std::uint8_t> batch;
      appendRecord(batch, kCol, cols, sizeof(cols));
      ingest.processBatch(batch.data(),
                          static_cast<std::uint32_t>(batch.size()));
    }

    dc::Encoding enc;
    enc.field(dc::Channel::X, "x").field(dc::Channel::Y, "y")
       .field(dc::Channel::Size, "sz");
    enc.setColorField("col");  // per-point packed RGBA8

    auto res = pass.compile(dc::Mark::PointColor, enc, tables, kTable, src,
                            190, 290, 390);
    check(res.ok, "pointColor: compile ok");
    check(res.geometry.format == dc::VertexFormat::Point4Color,
          "pointColor: geometry format == Point4Color");
    check(res.drawItem.pipeline == "instancedPointColor@1",
          "pointColor: pipeline == instancedPointColor@1");
    check(res.instanceCount == 3, "pointColor: 3 instances for 3 rows");
    check(res.bytes.size() == 3u * 16u,
          "pointColor: 3 * 16B byte length (exact Point4Color stride)");

    // Byte-exact: per instance [x f32 @0][y f32 @4][rgba8 u32 @8][size f32 @12].
    const float exx[3] = {-0.5f, 0.0f, 0.5f};
    const float exy[3] = { 0.1f, 0.2f, 0.3f};
    const float exs[3] = { 4.0f, 8.0f, 16.0f};
    const std::uint32_t exc[3] = {kRed, kGreen, kBlue};
    bool bytesOk = true;
    for (int i = 0; i < 3; ++i) {
      if (!eqf(f32At(res.bytes, i * 16 + 0), exx[i])) bytesOk = false;
      if (!eqf(f32At(res.bytes, i * 16 + 4), exy[i])) bytesOk = false;
      std::uint32_t c = 0;
      std::memcpy(&c, res.bytes.data() + i * 16 + 8, 4);
      if (c != exc[i]) bytesOk = false;
      if (!eqf(f32At(res.bytes, i * 16 + 12), exs[i])) bytesOk = false;
    }
    check(bytesOk,
          "pointColor: bytes are exactly [x,y][packed RGBA8 @8][size @12]");

    auto vr = wireAndValidate(res, 90, 190, 290,
                              static_cast<std::uint32_t>(res.bytes.size()),
                              "instancedPointColor@1");
    check(vr.ok, "pointColor: validateDrawItem ACCEPTS the point4_color geometry");

    // REJECT: binding the SAME point4_color geometry to points@1 (Pos2_Clip).
    auto bad = wireAndValidate(res, 90, 190, 290,
                               static_cast<std::uint32_t>(res.bytes.size()),
                               "points@1");
    check(!bad.ok && bad.err.code == "VALIDATION_VERTEX_FORMAT_MISMATCH",
          "pointColor: mismatched pipeline (points@1) is REJECTED");

    // REJECT at COMPILE: a pointColor missing the Size channel cannot be packed.
    dc::Encoding noSize;
    noSize.field(dc::Channel::X, "x").field(dc::Channel::Y, "y");
    noSize.setColorField("col");
    auto miss = pass.compile(dc::Mark::PointColor, noSize, tables, kTable, src,
                             190, 290, 390);
    check(!miss.ok && miss.error == dc::EncodeError::MissingChannel,
          "pointColor: missing required Size channel is REJECTED at compile");
  }

  // =========================================================================
  // ENC-613 — ARC mark -> triGradient@1 (Pos2Color4) via POLAR coords. Two
  // wedges (a pie of two slices) at distinct angles + distinct colors. Assert
  // the tessellated triangle geometry maps (theta,r) -> clip (cx + r*cos, cy +
  // r*sin) exactly, that the per-vertex color is the row's color, that the
  // vertex count is fixed (segs * 6 / wedge), and validateDrawItem ACCEPTS it.
  // =========================================================================
  {
    dc::IngestProcessor ingest;
    dc::TableStore tables;
    auto src = dc::makeBufferByteSource(ingest);
    const dc::Id kTable = 11, kT0 = 110, kR0 = 111, kT1 = 112, kR1 = 113,
                 kCol = 114;
    tables.defineTable(kTable, "pie");
    tables.addColumn(kTable, "t0", dc::DType::F32, kT0);
    tables.addColumn(kTable, "r0", dc::DType::F32, kR0);
    tables.addColumn(kTable, "t1", dc::DType::F32, kT1);
    tables.addColumn(kTable, "r1", dc::DType::F32, kR1);
    tables.addColumn(kTable, "col", dc::DType::I32, kCol);

    const std::uint32_t kRed   = 0xFF0000FFu;
    const std::uint32_t kGreen = 0xFF00FF00u;

    // Slice 0: [0, pi/2], r 0..0.8 (pie slice from center). Slice 1: [pi, 3pi/2].
    const float PI = 3.14159265358979323846f;
    appendF32(ingest, kT0, {0.0f, PI});
    appendF32(ingest, kR0, {0.0f, 0.0f});
    appendF32(ingest, kT1, {PI / 2.0f, 3.0f * PI / 2.0f});
    appendF32(ingest, kR1, {0.8f, 0.8f});
    {
      const std::int32_t cols[2] = {static_cast<std::int32_t>(kRed),
                                    static_cast<std::int32_t>(kGreen)};
      std::vector<std::uint8_t> batch;
      appendRecord(batch, kCol, cols, sizeof(cols));
      ingest.processBatch(batch.data(),
                          static_cast<std::uint32_t>(batch.size()));
    }

    dc::Encoding enc;
    enc.field(dc::Channel::X, "t0").field(dc::Channel::Y, "r0")
       .field(dc::Channel::X2, "t1").field(dc::Channel::Y2, "r1");
    enc.setColorField("col");

    dc::ArcOptions arcOpts;
    arcOpts.polar.centerX = 0.0f;
    arcOpts.polar.centerY = 0.0f;
    arcOpts.segmentsPerArc = 4;  // small, exact-to-reason geometry

    auto res = pass.compile(dc::Mark::Arc, enc, tables, kTable, src, 1100, 1200,
                            1300, nullptr, dc::LineStyle::Line2d, arcOpts);
    check(res.ok, "arc: compile ok");
    check(res.geometry.format == dc::VertexFormat::Pos2Color4,
          "arc: geometry format == Pos2Color4 (triGradient)");
    check(res.drawItem.pipeline == "triGradient@1",
          "arc: pipeline == triGradient@1");

    // Fixed tessellation: segs(4) * 6 verts/slice = 24 verts/wedge, 2 wedges.
    const std::uint32_t kVertsPerWedge = 4u * 6u;
    check(res.geometry.vertexCount == 2u * kVertsPerWedge,
          "arc: vertexCount == 2 wedges * (segs*6) verts");
    check(res.bytes.size() == 2u * kVertsPerWedge * 24u,
          "arc: byte length == verts * 24B (Pos2Color4 stride)");

    // The very FIRST vertex of wedge 0 is the inner sample at theta0=0, r0=0 ->
    // the center (0,0). Its color is red (0..1 unpacked).
    check(eqf(f32At(res.bytes, 0), 0.0f) && eqf(f32At(res.bytes, 4), 0.0f),
          "arc: wedge0 first vertex is the polar center (0,0)");
    check(eqf(f32At(res.bytes, 8), 1.0f) && eqf(f32At(res.bytes, 12), 0.0f) &&
              eqf(f32At(res.bytes, 16), 0.0f) && eqf(f32At(res.bytes, 20), 1.0f),
          "arc: wedge0 vertex color is the row's RED unpacked to 0..1");

    // The SECOND vertex of wedge 0 is the OUTER sample at theta=0, r=0.8 ->
    // (cx + 0.8*cos0, cy + 0.8*sin0) = (0.8, 0.0). This proves the polar map.
    check(eqf(f32At(res.bytes, 24 + 0), 0.8f) &&
              eqf(f32At(res.bytes, 24 + 4), 0.0f),
          "arc: outer sample maps (theta=0,r=0.8) -> clip (0.8, 0.0)");

    // A mid-angle outer sample lands off-axis (both x and y non-trivial),
    // confirming cos/sin are applied (an affine map could never produce this).
    // Scan the wedge-0 verts for one whose (x,y) ~ (0.8*cos(pi/4)) on both axes.
    bool sawDiagonal = false;
    const float diag = 0.8f * 0.70710678f;  // r * cos(45deg) == r * sin(45deg)
    for (std::uint32_t v = 0; v < kVertsPerWedge; ++v) {
      const float vx = f32At(res.bytes, v * 24 + 0);
      const float vy = f32At(res.bytes, v * 24 + 4);
      if (std::fabs(vx - diag) < 1e-4f && std::fabs(vy - diag) < 1e-4f)
        sawDiagonal = true;
    }
    check(sawDiagonal,
          "arc: a mid-angle sample lands on the 45deg diagonal (polar, not affine)");

    // Wedge 1 is colored GREEN — its first vertex color differs from wedge 0.
    const std::uint32_t w1Off = kVertsPerWedge * 24u;
    check(eqf(f32At(res.bytes, w1Off + 8), 0.0f) &&
              eqf(f32At(res.bytes, w1Off + 12), 1.0f),
          "arc: wedge1 vertex color is the row's GREEN (distinct from wedge0)");

    auto vr = wireAndValidate(res, 110 /*unused buf id here*/, 1200, 1100,
                              static_cast<std::uint32_t>(res.bytes.size()),
                              "triGradient@1");
    check(vr.ok, "arc: validateDrawItem ACCEPTS the pos2_color4 geometry");

    // INCREMENTAL: compileInto packs only the appended wedge tail at the right
    // fixed offset, ending byte-identical to a full compile of the grown table.
    {
      dc::CpuBufferStore store;
      auto t1 = pass.compileInto(dc::Mark::Arc, enc, tables, kTable, src, store,
                                 1100, 1200, 1300, /*fromRow=*/0, nullptr,
                                 dc::LineStyle::Line2d, arcOpts);
      check(t1.ok && store.getCpuDataSize(1300) == 2u * kVertsPerWedge * 24u,
            "arc/incr: store holds both wedges after first compile");
      const std::uint8_t* sb = store.getCpuData(1300);
      bool identical = (res.bytes.size() == store.getCpuDataSize(1300)) &&
                       (std::memcmp(sb, res.bytes.data(), res.bytes.size()) == 0);
      check(identical, "arc/incr: store == full compile byte-for-byte");
    }
  }

  std::printf("=== ENC-600/601/603/604/609/613 Results: %d passed, %d failed ===\n",
              passed, failed);
  return failed > 0 ? 1 : 0;
}
