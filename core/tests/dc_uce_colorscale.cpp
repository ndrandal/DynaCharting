// ENC-610 / ENC-611 (P2) — COLOR SCALES tests: numeric -> ramp -> packed RGBA8.
//
// Covers, per the two tickets:
//   ENC-610 (sequential):
//     * RGBA8 packing round-trip + the WebGPU RGBA8Unorm byte order,
//     * ramp endpoints + midpoint sample exactly the stop colors,
//     * a magnitude value maps to the EXPECTED RGBA8 (acceptance),
//     * streaming auto-domain via RunningDomain extends as data grows (the feed),
//     * named ramps + a manifest-style explicit stop list.
//   ENC-611 (diverging):
//     * values map SYMMETRICALLY around the declared mid (mid -> neutral),
//     * the mid color lands at mid even for a LOPSIDED domain,
//     * a class-4 scale WITHOUT a baseline policy is REJECTED at construction.
#include "dc/scale/ColorScale.hpp"
#include "dc/data/TableStore.hpp"
#include "dc/ingest/IngestProcessor.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
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

static bool approx(double a, double b, double eps = 1e-9) {
  return std::fabs(a - b) <= eps * (1.0 + std::fabs(a) + std::fabs(b));
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

int main() {
  std::printf("=== ENC-610/611 Color scales (sequential + diverging -> RGBA8) ===\n");

  // ----- Rgba8 packing: WebGPU RGBA8Unorm byte order + round-trip -------------
  {
    dc::Rgba8 c{0x11, 0x22, 0x33, 0x44};
    // Little-endian u32 of bytes R,G,B,A == (a<<24)|(b<<16)|(g<<8)|r.
    check(c.toU32() == 0x44332211u, "RGBA8 packs to little-endian R,G,B,A bytes");
    check(dc::Rgba8::fromU32(0x44332211u) == c, "RGBA8 unpacks round-trip");

    // The raw bytes are R,G,B,A in memory (what the encode pass writes).
    std::uint32_t v = c.toU32();
    auto* b = reinterpret_cast<std::uint8_t*>(&v);
    bool littleEndian = (b[0] == 0x11 && b[1] == 0x22 && b[2] == 0x33 &&
                         b[3] == 0x44);
    // (On a big-endian host the byte order flips; the logical u32 is what matters.)
    check(littleEndian || c.toU32() == 0x44332211u,
          "RGBA8 memory layout is R,G,B,A (or u32 is canonical)");

    check(dc::Rgba8::fromFloats(1.0f, 0.0f, 0.0f) == (dc::Rgba8{255, 0, 0, 255}),
          "fromFloats(1,0,0) -> opaque red");
    check(dc::Rgba8::fromFloats(0.5f, 0.5f, 0.5f).r == 128,
          "fromFloats rounds 0.5 -> 128");
    check(dc::Rgba8::fromFloats(2.0f, -1.0f, 0.0f) == (dc::Rgba8{255, 0, 0, 255}),
          "fromFloats clamps out-of-range");
  }

  // ----- ColorRamp: endpoints, midpoint, and lerp -----------------------------
  {
    dc::ColorRamp ramp({
        {0.0, dc::Rgba8{0, 0, 0, 255}},
        {0.5, dc::Rgba8{100, 100, 100, 255}},
        {1.0, dc::Rgba8{200, 200, 200, 255}},
    });
    check(ramp.sample(0.0) == (dc::Rgba8{0, 0, 0, 255}), "ramp t=0 is first stop");
    check(ramp.sample(1.0) == (dc::Rgba8{200, 200, 200, 255}),
          "ramp t=1 is last stop");
    check(ramp.sample(0.5) == (dc::Rgba8{100, 100, 100, 255}),
          "ramp t=0.5 hits the middle stop exactly");
    // Quarter point: halfway between stop0 (0) and stop1 (100) -> 50.
    check(ramp.sample(0.25) == (dc::Rgba8{50, 50, 50, 255}),
          "ramp lerps linearly between stops");
    // Out-of-range clamps to the endpoints.
    check(ramp.sample(-1.0) == (dc::Rgba8{0, 0, 0, 255}), "ramp clamps below 0");
    check(ramp.sample(2.0) == (dc::Rgba8{200, 200, 200, 255}),
          "ramp clamps above 1");

    // Named ramps resolve and have the right endpoints.
    dc::ColorRamp v;
    check(dc::ColorRamp::byName("viridis", v), "named ramp 'viridis' resolves");
    check(v.sample(0.0) == (dc::Rgba8{68, 1, 84, 255}),
          "viridis t=0 is the dark-purple anchor");
    check(v.sample(1.0) == (dc::Rgba8{253, 231, 37, 255}),
          "viridis t=1 is the yellow anchor");
    dc::ColorRamp unused;
    check(!dc::ColorRamp::byName("not-a-ramp", unused),
          "unknown ramp name is rejected");
  }

  // ----- ENC-610: SequentialColorScale value -> expected RGBA8 ----------------
  {
    // domain [0,100] over a blue->red ramp.
    dc::SequentialColorScale s(dc::Domain{0.0, 100.0, false},
                               dc::ColorRamp::blueRed());
    // map() is the ramp parameter t in [0,1].
    check(approx(s.map(0.0), 0.0), "seq map domain-min -> t=0");
    check(approx(s.map(100.0), 1.0), "seq map domain-max -> t=1");
    check(approx(s.map(50.0), 0.5), "seq map midpoint -> t=0.5");
    // Values outside the domain clamp (a color is always a valid color).
    check(approx(s.map(200.0), 1.0), "seq map above domain clamps to t=1");
    check(approx(s.map(-50.0), 0.0), "seq map below domain clamps to t=0");

    // A magnitude value maps to the EXPECTED RGBA8 (the acceptance criterion).
    check(s.mapColor(0.0) == (dc::Rgba8{0, 0, 255, 255}),
          "seq: magnitude 0 -> blue (ramp low)");
    check(s.mapColor(100.0) == (dc::Rgba8{255, 0, 0, 255}),
          "seq: magnitude 100 -> red (ramp high)");
    // Midpoint magnitude -> the exact halfway lerp of blue->red.
    check(s.mapColor(50.0) == (dc::Rgba8{128, 0, 128, 255}),
          "seq: magnitude 50 -> halfway blue/red purple");
    check(s.mapU32(0.0) == (dc::Rgba8{0, 0, 255, 255}).toU32(),
          "seq mapU32 packs the expected u32 (the encode-pass output)");

    // invert round-trips through map.
    check(approx(s.invert(s.map(42.0)), 42.0), "seq map/invert round-trip");

    // Degenerate domain parks at the ramp midpoint.
    dc::SequentialColorScale flat(dc::Domain{5.0, 5.0, false},
                                  dc::ColorRamp::blueRed());
    check(approx(flat.map(5.0), 0.5), "seq degenerate domain -> t=0.5");
    check(flat.mapColor(999.0) == flat.mapColor(-999.0),
          "seq degenerate domain maps ALL values to the same (mid) color");
  }

  // ----- ENC-610: streaming auto-domain EXTENDS as data grows (the feed) ------
  {
    dc::IngestProcessor ingest;
    dc::TableStore tables;
    auto src = dc::makeBufferByteSource(ingest);

    const dc::Id kBuf = 510;  // f32 magnitude column
    const dc::Id kTable = 11;
    check(tables.defineTable(kTable, "heat"), "defineTable for color scale");
    check(tables.addColumn(kTable, "mag", dc::DType::F32, kBuf),
          "addColumn mag/f32");

    dc::SequentialColorScale s(dc::ColorRamp::blueRed());
    s.bindColumn(kTable, "mag");
    check(s.hasBoundColumn(), "seq scale has bound column");
    check(!s.updateDomain(tables, src), "seq updateDomain false on empty column");

    // Tick 1: append [10, 50] -> domain [10,50]; 10 is the low (blue) end.
    {
      float v[2] = {10.0f, 50.0f};
      std::vector<std::uint8_t> batch;
      appendRecord(batch, kBuf, v, sizeof(v));
      ingest.processBatch(batch.data(),
                          static_cast<std::uint32_t>(batch.size()));
      check(s.updateDomain(tables, src), "seq updateDomain true after append");
      check(approx(s.domain().min, 10.0) && approx(s.domain().max, 50.0),
            "seq auto-domain is [10,50] after tick 1");
      check(s.mapColor(10.0) == (dc::Rgba8{0, 0, 255, 255}),
            "seq: value at running-min is the ramp-low color (blue)");
      check(s.mapColor(50.0) == (dc::Rgba8{255, 0, 0, 255}),
            "seq: value at running-max is the ramp-high color (red)");
    }
    // Tick 2: append a NEW global max (90). Domain EXTENDS; the SAME value 50 now
    // maps to a DIFFERENT (no-longer-max) color — proof the domain grew.
    {
      dc::Rgba8 before50 = s.mapColor(50.0);  // was red (the max)
      float v[1] = {90.0f};
      std::vector<std::uint8_t> batch;
      appendRecord(batch, kBuf, v, sizeof(v));
      ingest.processBatch(batch.data(),
                          static_cast<std::uint32_t>(batch.size()));
      s.updateDomain(tables, src);
      check(approx(s.domain().max, 90.0),
            "seq auto-domain EXTENDS to [10,90] as data grows");
      check(s.mapColor(90.0) == (dc::Rgba8{255, 0, 0, 255}),
            "seq: the new running-max takes the ramp-high color");
      check(!(s.mapColor(50.0) == before50),
            "seq: value 50 re-colors as the domain extends (no longer the max)");
      check(s.runningDomain().consumedCount() == 3,
            "seq reducer consumed exactly 3 appended rows (O(Δ) high-water)");
    }
  }

  // ----- ENC-611: DivergingColorScale — symmetry around the declared mid ------
  {
    // correlation domain [-1, +1], mid 0, red…white…green.
    dc::BaselinePolicy pol = dc::BaselinePolicy::fixedEpoch();
    auto sp = dc::makeDivergingColorScale(0.0, dc::ColorRamp::redNeutralGreen(),
                                          pol);
    check(sp != nullptr, "diverging scale with a baseline policy is accepted");
    dc::DivergingColorScale& s = *sp;
    s.setDomain(-1.0, 1.0);

    check(approx(s.map(0.0), 0.5), "diverging mid -> t=0.5 (neutral)");
    check(approx(s.map(-1.0), 0.0), "diverging min -> t=0 (low end)");
    check(approx(s.map(1.0), 1.0), "diverging max -> t=1 (high end)");
    // SYMMETRY: equal-magnitude deviations land symmetric distances from 0.5.
    check(approx(s.map(-0.5) - 0.5, -(s.map(0.5) - 0.5)),
          "diverging: ±0.5 map symmetrically around the mid");
    check(approx(s.map(-0.25) - 0.5, -(s.map(0.25) - 0.5)),
          "diverging: ±0.25 map symmetrically around the mid");

    // The mid color is the exact neutral ramp stop; the ends are the ramp ends.
    check(s.mapColor(0.0) == (dc::Rgba8{255, 255, 255, 255}),
          "diverging: value at mid is the neutral (white) color");
    check(s.mapColor(-1.0) == (dc::Rgba8{215, 48, 39, 255}),
          "diverging: value at min is the red (low) color");
    check(s.mapColor(1.0) == (dc::Rgba8{26, 152, 80, 255}),
          "diverging: value at max is the green (high) color");
    check(s.mapU32(0.0) == (dc::Rgba8{255, 255, 255, 255}).toU32(),
          "diverging mapU32 packs the neutral color (the encode-pass output)");

    // LOPSIDED domain: mid STILL lands exactly on the neutral color. Domain
    // [-1, +5], mid 0 — the high side is 5x the low side, yet mid -> t=0.5.
    s.setDomain(-1.0, 5.0);
    check(approx(s.map(0.0), 0.5),
          "diverging: mid stays at t=0.5 for a LOPSIDED domain");
    check(s.mapColor(0.0) == (dc::Rgba8{255, 255, 255, 255}),
          "diverging: mid is neutral even when the ends are lopsided");
    // The two halves are each interpolated independently to their full extent.
    check(approx(s.map(-1.0), 0.0) && approx(s.map(5.0), 1.0),
          "diverging: both lopsided ends still span the full ramp");
    // A non-zero mid works too.
    s.setMid(2.0);
    s.setDomain(0.0, 10.0);
    check(approx(s.map(2.0), 0.5), "diverging: non-zero mid still maps to t=0.5");

    // invert round-trips piecewise around the mid.
    check(approx(s.invert(s.map(7.0)), 7.0), "diverging map/invert round-trip hi");
    check(approx(s.invert(s.map(1.0)), 1.0), "diverging map/invert round-trip lo");
  }

  // ----- ENC-611: class-4 rejection — no baseline policy -> rejected ----------
  {
    // No policy (BaselinePolicyKind::None) -> the factory REJECTS (nullptr).
    dc::BaselinePolicy none;  // default kind == None
    check(!none.valid(), "default baseline policy is invalid (None)");
    auto rejected = dc::makeDivergingColorScale(
        0.0, dc::ColorRamp::redNeutralGreen(), none);
    check(rejected == nullptr,
          "class-4 diverging scale WITHOUT a baseline policy is REJECTED");

    // Each declared policy is accepted and recorded on the scale.
    for (const char* name : {"fixedEpoch", "decaying", "referenceWindow"}) {
      dc::BaselinePolicy p;
      check(dc::BaselinePolicy::byName(name, p), "baseline policy name resolves");
      auto ok = dc::makeDivergingColorScale(0.0,
                                            dc::ColorRamp::redNeutralGreen(), p);
      check(ok != nullptr && ok->baselinePolicy().valid(),
            "diverging scale accepted + carries the declared policy");
    }
    dc::BaselinePolicy bad;
    check(!dc::BaselinePolicy::byName("not-a-policy", bad),
          "unknown baseline policy name is rejected");
  }

  // ----- ENC-611: diverging streaming auto-domain (ends drift, mid fixed) -----
  {
    dc::IngestProcessor ingest;
    dc::TableStore tables;
    auto src = dc::makeBufferByteSource(ingest);

    const dc::Id kBuf = 520;
    const dc::Id kTable = 12;
    check(tables.defineTable(kTable, "corr"), "defineTable for diverging scale");
    check(tables.addColumn(kTable, "v", dc::DType::F32, kBuf),
          "addColumn v/f32");

    auto sp = dc::makeDivergingColorScale(0.0, dc::ColorRamp::redNeutralGreen(),
                                          dc::BaselinePolicy::fixedEpoch());
    check(sp != nullptr, "diverging auto-domain scale constructed");
    dc::DivergingColorScale& s = *sp;
    s.bindColumn(kTable, "v");

    float v[3] = {-0.4f, 0.2f, 0.8f};
    std::vector<std::uint8_t> batch;
    appendRecord(batch, kBuf, v, sizeof(v));
    ingest.processBatch(batch.data(),
                        static_cast<std::uint32_t>(batch.size()));
    check(s.updateDomain(tables, src), "diverging updateDomain true after append");
    // -0.4f / 0.8f are not exactly representable in f32; compare at f32 epsilon.
    check(approx(s.domain().min, -0.4, 1e-6) && approx(s.domain().max, 0.8, 1e-6),
          "diverging auto-domain ends track running [min,max]");
    check(approx(s.mid(), 0.0), "diverging mid stays FIXED as the ends drift");
    check(approx(s.map(0.0), 0.5),
          "diverging: mid still maps to neutral against the streamed domain");
  }

  std::printf("=== ENC-610/611 Results: %d passed, %d failed ===\n", passed,
              failed);
  return failed > 0 ? 1 : 0;
}
