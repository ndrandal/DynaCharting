#pragma once

namespace dc {

// Map a value from [dataMin, dataMax] to [clipMin, clipMax].
inline float normalizeToClip(float value, float dataMin, float dataMax,
                              float clipMin, float clipMax) {
  if (dataMax <= dataMin) return clipMin;
  float t = (value - dataMin) / (dataMax - dataMin);
  return clipMin + t * (clipMax - clipMin);
}

} // namespace dc
