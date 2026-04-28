#pragma once
#include "dc/ids/Id.hpp"
#include "dc/style/Theme.hpp"

#include <cstdint>
#include <vector>

namespace dc {

// ChartTheme: recipe-layer conventions on top of the engine's agnostic Theme.
// This header is where chart-domain concepts (candles, volume, overlays,
// crosshair) are mapped onto Theme's generic palette slots. Keeping these out
// of the core Theme preserves the engine's agnosticism.
namespace chart_theme {

// Default palette slot assignments. Callers can pick their own instead.
constexpr std::uint8_t kSlotCandle    = 0;   // palette[0]/paletteAlt[0] = candle up/down
constexpr std::uint8_t kSlotVolume    = 1;   // palette[1]/paletteAlt[1] = volume up/down
constexpr std::uint8_t kSlotOverlay0  = 2;   // palette[2..9] = 8 overlay colors
constexpr std::uint8_t kSlotCrosshair = 10;  // palette[10] = crosshair tint

// Accessors that interpret the generic palette with chart semantics.
inline const float* candleUp(const Theme& t)    { return t.palette[kSlotCandle]; }
inline const float* candleDown(const Theme& t)  { return t.paletteAlt[kSlotCandle]; }
inline const float* volumeUp(const Theme& t)    { return t.palette[kSlotVolume]; }
inline const float* volumeDown(const Theme& t)  { return t.paletteAlt[kSlotVolume]; }
inline const float* crosshair(const Theme& t)   { return t.palette[kSlotCrosshair]; }
inline const float* overlay(const Theme& t, std::size_t i) {
  return t.palette[kSlotOverlay0 + (i % 8u)];
}

// Mutable variants for authoring custom themes by name.
inline float* candleUp(Theme& t)    { return t.palette[kSlotCandle]; }
inline float* candleDown(Theme& t)  { return t.paletteAlt[kSlotCandle]; }
inline float* volumeUp(Theme& t)    { return t.palette[kSlotVolume]; }
inline float* volumeDown(Theme& t)  { return t.paletteAlt[kSlotVolume]; }
inline float* crosshair(Theme& t)   { return t.palette[kSlotCrosshair]; }
inline float* overlay(Theme& t, std::size_t i) {
  return t.palette[kSlotOverlay0 + (i % 8u)];
}

// PaletteGroup factories. These keep chart-aware setup out of the engine core.
inline PaletteGroup candleGroup(std::vector<Id> drawItemIds) {
  return {PaletteGroup::Kind::UpDownPair, kSlotCandle, std::move(drawItemIds), false};
}
inline PaletteGroup volumeGroup(std::vector<Id> drawItemIds) {
  return {PaletteGroup::Kind::UpDownPair, kSlotVolume, std::move(drawItemIds), false};
}
inline PaletteGroup crosshairGroup(std::vector<Id> drawItemIds) {
  return {PaletteGroup::Kind::Solid, kSlotCrosshair, std::move(drawItemIds), false};
}
// Overlay targets use rotateSlots: targets[i] uses palette[kSlotOverlay0 + i%8].
inline PaletteGroup overlayGroup(std::vector<Id> drawItemIds) {
  return {PaletteGroup::Kind::Solid, kSlotOverlay0, std::move(drawItemIds), true};
}
inline PaletteGroup paneBackgroundGroup(std::vector<Id> paneIds) {
  return {PaletteGroup::Kind::PaneBackground, 0, std::move(paneIds), false};
}
inline PaletteGroup gridGroup(std::vector<Id> drawItemIds) {
  return {PaletteGroup::Kind::GridStyle, 0, std::move(drawItemIds), false};
}
inline PaletteGroup tickGroup(std::vector<Id> drawItemIds) {
  return {PaletteGroup::Kind::TickStyle, 0, std::move(drawItemIds), false};
}
inline PaletteGroup labelGroup(std::vector<Id> drawItemIds) {
  return {PaletteGroup::Kind::LabelColor, 0, std::move(drawItemIds), false};
}
inline PaletteGroup textGroup(std::vector<Id> drawItemIds) {
  return {PaletteGroup::Kind::TextColor, 0, std::move(drawItemIds), false};
}
inline PaletteGroup highlightGroup(std::vector<Id> drawItemIds) {
  return {PaletteGroup::Kind::HighlightColor, 0, std::move(drawItemIds), false};
}
inline PaletteGroup drawingGroup(std::vector<Id> drawItemIds) {
  return {PaletteGroup::Kind::DrawingColor, 0, std::move(drawItemIds), false};
}

} // namespace chart_theme
} // namespace dc
