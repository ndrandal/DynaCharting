#pragma once
// D75: SVG vector export
#include "dc/ids/Id.hpp"
#include "dc/scene/Types.hpp"

#include <cstdint>
#include <string>

namespace dc {

// Forward declarations
class Scene;
class IngestProcessor;

struct SvgExportOptions {
  double width{800};
  double height{600};
  bool includeBackground{true};
  float backgroundColor[4] = {0.1f, 0.1f, 0.12f, 1.0f};
  std::string title;
  bool embedFonts{false};
  double strokeWidthScale{1.0};  // multiply all line widths
};

class SvgExporter {
public:
  // Export entire scene to SVG string.
  // ingest provides CPU-side vertex buffer data (may be nullptr if no data needed).
  static std::string exportScene(const Scene& scene,
                                  const IngestProcessor* ingest,
                                  const SvgExportOptions& options = {});

  // Export a single pane.
  static std::string exportPane(const Scene& scene,
                                 const IngestProcessor* ingest,
                                 Id paneId,
                                 const SvgExportOptions& options = {});

private:
  static std::string buildHeader(const SvgExportOptions& options);
  static std::string buildFooter();
  static std::string buildBackground(const SvgExportOptions& options);

  // Color helpers
  static std::string colorToHex(const float color[4]);
  static std::string colorToRgba(const float color[4]);
  static double clampOpacity(float a);

  // Transform to SVG matrix attribute string
  static std::string transformToSvg(const float mat3[9], double svgWidth, double svgHeight);

  // Map clip-space coordinates (-1..1) to SVG pixel coordinates
  static double clipToSvgX(float cx, double svgWidth);
  static double clipToSvgY(float cy, double svgHeight);

  // Pipeline-specific SVG generators.
  // These read vertex data from IngestProcessor buffers and produce SVG elements.
  static std::string renderTriangles(const Scene& scene,
                                      const IngestProcessor* ingest,
                                      const DrawItem& di,
                                      const SvgExportOptions& options);
  static std::string renderLines(const Scene& scene,
                                  const IngestProcessor* ingest,
                                  const DrawItem& di,
                                  const SvgExportOptions& options);
  static std::string renderPoints(const Scene& scene,
                                   const IngestProcessor* ingest,
                                   const DrawItem& di,
                                   const SvgExportOptions& options);
  static std::string renderRects(const Scene& scene,
                                  const IngestProcessor* ingest,
                                  const DrawItem& di,
                                  const SvgExportOptions& options);
  static std::string renderCandles(const Scene& scene,
                                    const IngestProcessor* ingest,
                                    const DrawItem& di,
                                    const SvgExportOptions& options);
  static std::string renderText(const Scene& scene,
                                 const IngestProcessor* ingest,
                                 const DrawItem& di,
                                 const SvgExportOptions& options);

  // Gradient def generation
  static std::string buildGradientDef(const DrawItem& di, std::uint32_t uniqueId);
};

} // namespace dc
