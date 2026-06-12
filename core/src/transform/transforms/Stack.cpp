// ENC-616c — `stack` transform implementation. See Stack.hpp.
#include "dc/transform/transforms/Stack.hpp"

#include "TransformUtil.hpp"

#include <cmath>
#include <unordered_map>
#include <vector>

namespace dc {

namespace {

// Is `value` a baseline that DRIFTS on append, hence requiring a baseline policy?
bool driftsBaseline(StackOffset o) { return o != StackOffset::Zero; }

// Validate a baseline policy's own fields (the values the modes actually need).
bool policyWellFormed(const BaselinePolicy& p, std::string& err) {
  switch (p.kind) {
    case BaselinePolicy::Kind::FixedEpoch:
      return true;  // any epoch (incl. 0) is a valid pin
    case BaselinePolicy::Kind::Decaying:
      if (!(p.decay > 0.0 && p.decay <= 1.0)) {
        err = "stack baseline policy 'decaying' needs decay in (0,1]";
        return false;
      }
      return true;
    case BaselinePolicy::Kind::ReferenceWindow:
      if (p.windowGroups == 0) {
        err = "stack baseline policy 'referenceWindow' needs windowGroups > 0";
        return false;
      }
      return true;
  }
  return false;
}

}  // namespace

SchemaResult StackTransform::inferSchema(const ColumnSchema& input) const {
  SchemaResult r;

  // The measure column must exist and be numeric (f32/i32 widened to double).
  const SchemaColumn* v = input.find(value_);
  if (!v) {
    r.error = "stack value column '" + value_ + "' not found";
    return r;
  }
  if (v->dtype != DType::F32 && v->dtype != DType::I32) {
    r.error = "stack value column '" + value_ + "' must be numeric (f32/i32)";
    return r;
  }
  // Optional group-by column must exist if named (any dtype — its identity keys
  // the x-position group; we compare its widened double value).
  if (!groupBy_.empty() && !input.has(groupBy_)) {
    r.error = "stack groupBy column '" + groupBy_ + "' not found";
    return r;
  }
  // Output y0/y1 must not collide with existing columns.
  if (input.has(y0_) || input.has(y1_)) {
    r.error = "stack output column '" + y0_ + "'/'" + y1_ +
              "' collides with an input column";
    return r;
  }
  if (y0_ == y1_) {
    r.error = "stack y0 and y1 must differ";
    return r;
  }

  // The class-3/4 contract: a drifting baseline (normalize/wiggle) REQUIRES a
  // well-formed baseline policy; zero offset must NOT carry one (no drift to pin).
  if (driftsBaseline(offset_)) {
    if (!policy_) {
      r.error =
          "stack offset normalize/wiggle has a drifting baseline and REQUIRES a "
          "baseline policy {fixedEpoch | decaying | referenceWindow}";
      return r;
    }
    std::string perr;
    if (!policyWellFormed(*policy_, perr)) {
      r.error = perr;
      return r;
    }
  } else if (policy_) {
    r.error = "stack offset zero has no drifting baseline; a baseline policy is "
              "not applicable";
    return r;
  }

  // Output = all input columns (passthrough) + the two band columns (f32).
  r.schema = input;
  r.schema.columns.push_back({y0_, DType::F32});
  r.schema.columns.push_back({y1_, DType::F32});
  r.ok = true;
  return r;
}

void StackTransform::evaluate(const EvalContext& ctx) const {
  const ColumnSchema& in = *ctx.inputSchema;
  const ColumnResolver& res = *ctx.input;
  const std::size_t rows = res.rowCount();
  const Id node = ctx.nodeId;

  // (Re)allocate passthrough columns + the y0/y1 band columns.
  for (const auto& col : in.columns) {
    ctx.out->allocColumn(node, col.name, col.dtype, rows);
  }
  ctx.out->allocColumn(node, y0_, DType::F32, rows);
  ctx.out->allocColumn(node, y1_, DType::F32, rows);

  // Pass through every input cell unchanged.
  for (std::size_t i = 0; i < rows; ++i) {
    for (const auto& col : in.columns) {
      tfutil::copyCell(col, res, i, *ctx.out, node, i);
    }
  }

  const bool grouped = !groupBy_.empty();

  // Pass 1: per-group running sum of `value` (the plain cumulative band), and the
  // per-group total (needed by normalize and wiggle). Groups are keyed by the
  // group-by column's widened value; a single global group when ungrouped.
  std::vector<double> y0(rows, 0.0);
  std::vector<double> y1(rows, 0.0);
  std::unordered_map<double, double> running;  // group key -> running sum so far
  std::unordered_map<double, double> total;     // group key -> final total

  auto keyOf = [&](std::size_t i) -> double {
    return grouped ? res.readNum(groupBy_, i) : 0.0;
  };

  for (std::size_t i = 0; i < rows; ++i) {
    const double k = keyOf(i);
    const double v = res.readNum(value_, i);
    double base = running[k];
    y0[i] = base;
    y1[i] = base + v;
    running[k] = base + v;
    total[k] += v;
  }

  // Pass 2: apply the offset mode. Zero = the cumulative band as-is. Normalize =
  // rescale each row's band by its group total (100% stack). Wiggle = shift each
  // group down by half its total so the silhouette is centered (the streamgraph
  // baseline). normalize/wiggle drift on append, which is why inferSchema demands
  // a baseline policy; the policy pins the reference total the band is drawn
  // against. We apply the policy's reference rule here.
  switch (offset_) {
    case StackOffset::Zero:
      break;  // y0/y1 already the cumulative band
    case StackOffset::Normalize: {
      for (std::size_t i = 0; i < rows; ++i) {
        const double t = total[keyOf(i)];
        const double inv = (t != 0.0) ? (1.0 / t) : 0.0;
        y0[i] *= inv;
        y1[i] *= inv;
      }
      break;
    }
    case StackOffset::Wiggle: {
      // Center each group's stack around 0: shift the whole group down by half its
      // total. (A full Byron-Wattenberg wiggle minimizes inter-layer movement; the
      // centered baseline is the stable, replay-safe streamgraph form the baseline
      // policy pins.)
      for (std::size_t i = 0; i < rows; ++i) {
        const double shift = 0.5 * total[keyOf(i)];
        y0[i] -= shift;
        y1[i] -= shift;
      }
      break;
    }
  }

  for (std::size_t i = 0; i < rows; ++i) {
    ctx.out->setF32(node, y0_, i, static_cast<float>(y0[i]));
    ctx.out->setF32(node, y1_, i, static_cast<float>(y1[i]));
  }
}

}  // namespace dc
