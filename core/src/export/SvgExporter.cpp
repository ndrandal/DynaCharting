// D75: SVG vector export implementation
#include "dc/export/SvgExporter.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/Geometry.hpp"
#include "dc/ingest/IngestProcessor.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <vector>

namespace dc {

// ---- Helpers ----

static std::string fmtDouble(double v, int precision = 2) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.*f", precision, v);
  return buf;
}

static int floatToByte(float f) {
  int v = static_cast<int>(std::round(f * 255.0f));
  if (v < 0) v = 0;
  if (v > 255) v = 255;
  return v;
}

// Read a float from raw bytes at offset
static float readFloat(const std::uint8_t* data, std::uint32_t offset) {
  float v;
  std::memcpy(&v, data + offset, sizeof(float));
  return v;
}

// ---- XML escaping ----

static std::string escapeXml(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    switch (c) {
      case '&':  out += "&amp;"; break;
      case '<':  out += "&lt;"; break;
      case '>':  out += "&gt;"; break;
      case '"':  out += "&quot;"; break;
      case '\'': out += "&apos;"; break;
      default:   out += c;
    }
  }
  return out;
}

// ---- Color helpers ----

std::string SvgExporter::colorToHex(const float color[4]) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "#%02x%02x%02x",
                floatToByte(color[0]),
                floatToByte(color[1]),
                floatToByte(color[2]));
  return buf;
}

std::string SvgExporter::colorToRgba(const float color[4]) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "rgba(%d,%d,%d,%.3f)",
                floatToByte(color[0]),
                floatToByte(color[1]),
                floatToByte(color[2]),
                static_cast<double>(color[3]));
  return buf;
}

double SvgExporter::clampOpacity(float a) {
  double d = static_cast<double>(a);
  if (d < 0.0) d = 0.0;
  if (d > 1.0) d = 1.0;
  return d;
}

// ---- Coordinate mapping ----

double SvgExporter::clipToSvgX(float cx, double svgWidth) {
  // clip space: -1..1 maps to 0..svgWidth
  return (static_cast<double>(cx) + 1.0) * 0.5 * svgWidth;
}

double SvgExporter::clipToSvgY(float cy, double svgHeight) {
  // clip space: -1..1 maps to svgHeight..0 (SVG Y is top-down, GL is bottom-up)
  return (1.0 - static_cast<double>(cy)) * 0.5 * svgHeight;
}

// ---- Transform ----

std::string SvgExporter::transformToSvg(const float mat3[9],
                                          double svgWidth, double svgHeight) {
  // The scene transform is a 2D affine in clip space (column-major mat3):
  //   [sx,  0, 0]
  //   [ 0, sy, 0]
  //   [tx, ty, 1]
  //
  // In SVG pixel space we need to:
  //   1. Map from SVG coords to clip: cx = (px / W)*2 - 1, cy = 1 - (py / H)*2
  //   2. Apply clip-space transform
  //   3. Map back to SVG coords
  //
  // This yields an SVG matrix(a, b, c, d, e, f) that operates in pixel space.
  float sx = mat3[0];
  float sy = mat3[4];
  float tx = mat3[6];
  float ty = mat3[7];

  // In SVG coords: pxOut = sx * pxIn + tx * (W/2)
  //                pyOut = sy * pyIn + (-ty) * (H/2)
  // plus offset adjustments for the center shift
  double a = static_cast<double>(sx);
  double d = static_cast<double>(sy);
  double e = static_cast<double>(tx) * svgWidth * 0.5;
  // ty in clip space is upward-positive; SVG Y is downward, so negate
  double f = static_cast<double>(-ty) * svgHeight * 0.5;

  // Offset correction: the scaling center in SVG coords is (W/2, H/2).
  // After scaling, the center shifts: e += (1-sx)*W/2, f += (1-sy)*H/2
  e += (1.0 - a) * svgWidth * 0.5;
  f += (1.0 - d) * svgHeight * 0.5;

  char buf[256];
  std::snprintf(buf, sizeof(buf), "matrix(%.6f,0,0,%.6f,%.3f,%.3f)",
                a, d, e, f);
  return buf;
}

// ---- Header / Footer ----

std::string SvgExporter::buildHeader(const SvgExportOptions& options) {
  std::ostringstream ss;
  ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  ss << "<svg xmlns=\"http://www.w3.org/2000/svg\"";
  ss << " width=\"" << fmtDouble(options.width, 0) << "\"";
  ss << " height=\"" << fmtDouble(options.height, 0) << "\"";
  ss << " viewBox=\"0 0 " << fmtDouble(options.width, 0) << " "
     << fmtDouble(options.height, 0) << "\"";
  ss << ">\n";

  if (!options.title.empty()) {
    ss << "<title>" << escapeXml(options.title) << "</title>\n";
  }

  return ss.str();
}

std::string SvgExporter::buildFooter() {
  return "</svg>\n";
}

std::string SvgExporter::buildBackground(const SvgExportOptions& options) {
  if (!options.includeBackground) return "";

  std::ostringstream ss;
  ss << "<rect width=\"100%\" height=\"100%\" fill=\""
     << colorToHex(options.backgroundColor) << "\"";
  if (options.backgroundColor[3] < 1.0f) {
    ss << " fill-opacity=\"" << fmtDouble(clampOpacity(options.backgroundColor[3]), 3) << "\"";
  }
  ss << "/>\n";
  return ss.str();
}

// ---- Gradient defs ----

std::string SvgExporter::buildGradientDef(const DrawItem& di, std::uint32_t uniqueId) {
  if (di.gradientType == 0) return "";

  std::ostringstream ss;
  char idBuf[64];
  std::snprintf(idBuf, sizeof(idBuf), "grad_%u", uniqueId);

  if (di.gradientType == 1) {
    // Linear gradient
    // gradientAngle is in radians. SVG linear gradient uses x1,y1,x2,y2.
    double angle = static_cast<double>(di.gradientAngle);
    double dx = std::cos(angle);
    double dy = std::sin(angle);

    // Map from unit direction to percentage
    double x1 = 50.0 - dx * 50.0;
    double y1 = 50.0 + dy * 50.0;  // SVG Y is down
    double x2 = 50.0 + dx * 50.0;
    double y2 = 50.0 - dy * 50.0;

    ss << "<linearGradient id=\"" << idBuf << "\""
       << " x1=\"" << fmtDouble(x1, 1) << "%\""
       << " y1=\"" << fmtDouble(y1, 1) << "%\""
       << " x2=\"" << fmtDouble(x2, 1) << "%\""
       << " y2=\"" << fmtDouble(y2, 1) << "%\">\n";
    ss << "  <stop offset=\"0%\" stop-color=\"" << colorToHex(di.gradientColor0) << "\""
       << " stop-opacity=\"" << fmtDouble(clampOpacity(di.gradientColor0[3]), 3) << "\"/>\n";
    ss << "  <stop offset=\"100%\" stop-color=\"" << colorToHex(di.gradientColor1) << "\""
       << " stop-opacity=\"" << fmtDouble(clampOpacity(di.gradientColor1[3]), 3) << "\"/>\n";
    ss << "</linearGradient>\n";
  } else if (di.gradientType == 2) {
    // Radial gradient
    ss << "<radialGradient id=\"" << idBuf << "\""
       << " cx=\"" << fmtDouble(static_cast<double>(di.gradientCenter[0]) * 100.0, 1) << "%\""
       << " cy=\"" << fmtDouble(static_cast<double>(di.gradientCenter[1]) * 100.0, 1) << "%\""
       << " r=\"" << fmtDouble(static_cast<double>(di.gradientRadius) * 100.0, 1) << "%\">\n";
    ss << "  <stop offset=\"0%\" stop-color=\"" << colorToHex(di.gradientColor0) << "\""
       << " stop-opacity=\"" << fmtDouble(clampOpacity(di.gradientColor0[3]), 3) << "\"/>\n";
    ss << "  <stop offset=\"100%\" stop-color=\"" << colorToHex(di.gradientColor1) << "\""
       << " stop-opacity=\"" << fmtDouble(clampOpacity(di.gradientColor1[3]), 3) << "\"/>\n";
    ss << "</radialGradient>\n";
  }

  return ss.str();
}

// ---- Pipeline renderers ----

std::string SvgExporter::renderTriangles(const Scene& scene,
                                           const IngestProcessor* ingest,
                                           const DrawItem& di,
                                           const SvgExportOptions& options) {
  if (!ingest) return "";
  const Geometry* geom = scene.getGeometry(di.geometryId);
  if (!geom || geom->vertexCount < 3) return "";

  const std::uint8_t* data = ingest->getBufferData(geom->vertexBufferId);
  if (!data) return "";

  std::uint32_t stride = strideOf(geom->format);
  if (stride == 0) return "";

  std::uint32_t bufSize = ingest->getBufferSize(geom->vertexBufferId);
  std::string fill = colorToHex(di.color);
  double opacity = clampOpacity(di.color[3]);

  // Check for gradient fill
  bool hasGradient = (di.gradientType != 0);
  char gradRef[64] = {};
  if (hasGradient) {
    std::snprintf(gradRef, sizeof(gradRef), "url(#grad_%u)",
                  static_cast<std::uint32_t>(di.id));
  }

  std::ostringstream ss;

  // Emit triangles as <polygon> elements (3 vertices each)
  std::uint32_t triCount = geom->vertexCount / 3;
  for (std::uint32_t t = 0; t < triCount; ++t) {
    ss << "<polygon points=\"";
    for (std::uint32_t v = 0; v < 3; ++v) {
      std::uint32_t idx = t * 3 + v;
      std::uint32_t byteOff = idx * stride;
      if (byteOff + 8 > bufSize) break;  // need at least 2 floats (x, y)

      float cx = readFloat(data, byteOff);
      float cy = readFloat(data, byteOff + 4);
      double sx = clipToSvgX(cx, options.width);
      double sy = clipToSvgY(cy, options.height);

      if (v > 0) ss << " ";
      ss << fmtDouble(sx, 2) << "," << fmtDouble(sy, 2);
    }
    ss << "\" fill=\"" << (hasGradient ? gradRef : fill.c_str()) << "\"";
    if (!hasGradient && opacity < 1.0) {
      ss << " fill-opacity=\"" << fmtDouble(opacity, 3) << "\"";
    }
    ss << "/>\n";
  }

  return ss.str();
}

std::string SvgExporter::renderLines(const Scene& scene,
                                       const IngestProcessor* ingest,
                                       const DrawItem& di,
                                       const SvgExportOptions& options) {
  if (!ingest) return "";
  const Geometry* geom = scene.getGeometry(di.geometryId);
  if (!geom || geom->vertexCount < 2) return "";

  const std::uint8_t* data = ingest->getBufferData(geom->vertexBufferId);
  if (!data) return "";

  std::uint32_t stride = strideOf(geom->format);
  if (stride == 0) return "";

  std::uint32_t bufSize = ingest->getBufferSize(geom->vertexBufferId);
  std::string stroke = colorToHex(di.color);
  double strokeW = static_cast<double>(di.lineWidth) * options.strokeWidthScale;
  double opacity = clampOpacity(di.color[3]);

  std::ostringstream ss;

  // lineAA@1 uses Rect4 format (x0, y0, x1, y1 per segment);
  // line2d@1 uses Pos2_Clip paired vertices (2 vertices per segment).
  bool isRect4 = (geom->format == VertexFormat::Rect4);

  if (isRect4) {
    // Rect4: each record is a complete segment (x0, y0, x1, y1)
    for (std::uint32_t i = 0; i < geom->vertexCount; ++i) {
      std::uint32_t off = i * stride;
      if (off + 16 > bufSize) break;

      float cx0 = readFloat(data, off);
      float cy0 = readFloat(data, off + 4);
      float cx1 = readFloat(data, off + 8);
      float cy1 = readFloat(data, off + 12);

      double sx0 = clipToSvgX(cx0, options.width);
      double sy0 = clipToSvgY(cy0, options.height);
      double sx1 = clipToSvgX(cx1, options.width);
      double sy1 = clipToSvgY(cy1, options.height);

      ss << "<line x1=\"" << fmtDouble(sx0, 2)
         << "\" y1=\"" << fmtDouble(sy0, 2)
         << "\" x2=\"" << fmtDouble(sx1, 2)
         << "\" y2=\"" << fmtDouble(sy1, 2)
         << "\" stroke=\"" << stroke << "\""
         << " stroke-width=\"" << fmtDouble(strokeW, 2) << "\"";

      if (opacity < 1.0) {
        ss << " stroke-opacity=\"" << fmtDouble(opacity, 3) << "\"";
      }

      if (di.dashLength > 0.0f && di.gapLength > 0.0f) {
        ss << " stroke-dasharray=\""
           << fmtDouble(static_cast<double>(di.dashLength), 1) << " "
           << fmtDouble(static_cast<double>(di.gapLength), 1) << "\"";
      }

      ss << "/>\n";
    }
  } else {
    // Pos2_Clip: paired vertices (2 vertices per segment)
    std::uint32_t segCount = geom->vertexCount / 2;
    for (std::uint32_t s = 0; s < segCount; ++s) {
      std::uint32_t i0 = s * 2;
      std::uint32_t i1 = s * 2 + 1;
      std::uint32_t off0 = i0 * stride;
      std::uint32_t off1 = i1 * stride;
      if (off1 + 8 > bufSize) break;

      float cx0 = readFloat(data, off0);
      float cy0 = readFloat(data, off0 + 4);
      float cx1 = readFloat(data, off1);
      float cy1 = readFloat(data, off1 + 4);

      double sx0 = clipToSvgX(cx0, options.width);
      double sy0 = clipToSvgY(cy0, options.height);
      double sx1 = clipToSvgX(cx1, options.width);
      double sy1 = clipToSvgY(cy1, options.height);

      ss << "<line x1=\"" << fmtDouble(sx0, 2)
         << "\" y1=\"" << fmtDouble(sy0, 2)
         << "\" x2=\"" << fmtDouble(sx1, 2)
         << "\" y2=\"" << fmtDouble(sy1, 2)
         << "\" stroke=\"" << stroke << "\""
         << " stroke-width=\"" << fmtDouble(strokeW, 2) << "\"";

      if (opacity < 1.0) {
        ss << " stroke-opacity=\"" << fmtDouble(opacity, 3) << "\"";
      }

      if (di.dashLength > 0.0f && di.gapLength > 0.0f) {
        ss << " stroke-dasharray=\""
           << fmtDouble(static_cast<double>(di.dashLength), 1) << " "
           << fmtDouble(static_cast<double>(di.gapLength), 1) << "\"";
      }

      ss << "/>\n";
    }
  }

  return ss.str();
}

std::string SvgExporter::renderPoints(const Scene& scene,
                                        const IngestProcessor* ingest,
                                        const DrawItem& di,
                                        const SvgExportOptions& options) {
  if (!ingest) return "";
  const Geometry* geom = scene.getGeometry(di.geometryId);
  if (!geom || geom->vertexCount < 1) return "";

  const std::uint8_t* data = ingest->getBufferData(geom->vertexBufferId);
  if (!data) return "";

  std::uint32_t stride = strideOf(geom->format);
  if (stride == 0) return "";

  std::uint32_t bufSize = ingest->getBufferSize(geom->vertexBufferId);
  std::string fill = colorToHex(di.color);
  double opacity = clampOpacity(di.color[3]);
  double radius = static_cast<double>(di.pointSize) * 0.5;

  std::ostringstream ss;

  for (std::uint32_t i = 0; i < geom->vertexCount; ++i) {
    std::uint32_t off = i * stride;
    if (off + 8 > bufSize) break;

    float cx = readFloat(data, off);
    float cy = readFloat(data, off + 4);
    double sx = clipToSvgX(cx, options.width);
    double sy = clipToSvgY(cy, options.height);

    ss << "<circle cx=\"" << fmtDouble(sx, 2)
       << "\" cy=\"" << fmtDouble(sy, 2)
       << "\" r=\"" << fmtDouble(radius, 2)
       << "\" fill=\"" << fill << "\"";
    if (opacity < 1.0) {
      ss << " fill-opacity=\"" << fmtDouble(opacity, 3) << "\"";
    }
    ss << "/>\n";
  }

  return ss.str();
}

std::string SvgExporter::renderRects(const Scene& scene,
                                       const IngestProcessor* ingest,
                                       const DrawItem& di,
                                       const SvgExportOptions& options) {
  if (!ingest) return "";
  const Geometry* geom = scene.getGeometry(di.geometryId);
  if (!geom || geom->vertexCount < 1) return "";
  if (geom->format != VertexFormat::Rect4) return "";

  const std::uint8_t* data = ingest->getBufferData(geom->vertexBufferId);
  if (!data) return "";

  std::uint32_t stride = strideOf(geom->format);  // 16 bytes = 4 floats
  std::uint32_t bufSize = ingest->getBufferSize(geom->vertexBufferId);
  std::string fill = colorToHex(di.color);
  double opacity = clampOpacity(di.color[3]);
  double cr = static_cast<double>(di.cornerRadius);

  // Check for gradient fill
  bool hasGradient = (di.gradientType != 0);
  char gradRef[64] = {};
  if (hasGradient) {
    std::snprintf(gradRef, sizeof(gradRef), "url(#grad_%u)",
                  static_cast<std::uint32_t>(di.id));
  }

  std::ostringstream ss;

  for (std::uint32_t i = 0; i < geom->vertexCount; ++i) {
    std::uint32_t off = i * stride;
    if (off + 16 > bufSize) break;

    // Rect4 format: x0, y0, x1, y1 in clip space
    float x0c = readFloat(data, off);
    float y0c = readFloat(data, off + 4);
    float x1c = readFloat(data, off + 8);
    float y1c = readFloat(data, off + 12);

    double x0 = clipToSvgX(x0c, options.width);
    double y0 = clipToSvgY(y0c, options.height);
    double x1 = clipToSvgX(x1c, options.width);
    double y1 = clipToSvgY(y1c, options.height);

    // Ensure positive width/height (y0 > y1 possible due to coordinate flip)
    double rx = std::min(x0, x1);
    double ry = std::min(y0, y1);
    double rw = std::fabs(x1 - x0);
    double rh = std::fabs(y1 - y0);

    ss << "<rect x=\"" << fmtDouble(rx, 2)
       << "\" y=\"" << fmtDouble(ry, 2)
       << "\" width=\"" << fmtDouble(rw, 2)
       << "\" height=\"" << fmtDouble(rh, 2) << "\"";

    if (cr > 0.0) {
      ss << " rx=\"" << fmtDouble(cr, 2)
         << "\" ry=\"" << fmtDouble(cr, 2) << "\"";
    }

    ss << " fill=\"" << (hasGradient ? gradRef : fill.c_str()) << "\"";
    if (!hasGradient && opacity < 1.0) {
      ss << " fill-opacity=\"" << fmtDouble(opacity, 3) << "\"";
    }
    ss << "/>\n";
  }

  return ss.str();
}

std::string SvgExporter::renderCandles(const Scene& scene,
                                         const IngestProcessor* ingest,
                                         const DrawItem& di,
                                         const SvgExportOptions& options) {
  if (!ingest) return "";
  const Geometry* geom = scene.getGeometry(di.geometryId);
  if (!geom || geom->vertexCount < 1) return "";
  if (geom->format != VertexFormat::Candle6) return "";

  const std::uint8_t* data = ingest->getBufferData(geom->vertexBufferId);
  if (!data) return "";

  std::uint32_t stride = strideOf(geom->format);  // 24 bytes = 6 floats
  std::uint32_t bufSize = ingest->getBufferSize(geom->vertexBufferId);

  std::string colorUp = colorToHex(di.colorUp);
  std::string colorDown = colorToHex(di.colorDown);

  std::ostringstream ss;

  for (std::uint32_t i = 0; i < geom->vertexCount; ++i) {
    std::uint32_t off = i * stride;
    if (off + 24 > bufSize) break;

    // Candle6: x, open, high, low, close, halfWidth (all clip-space floats)
    float x = readFloat(data, off);
    float open = readFloat(data, off + 4);
    float high = readFloat(data, off + 8);
    float low = readFloat(data, off + 12);
    float close = readFloat(data, off + 16);
    float halfW = readFloat(data, off + 20);

    bool isUp = (close >= open);
    const std::string& fillColor = isUp ? colorUp : colorDown;

    double sx = clipToSvgX(x, options.width);
    double sOpen = clipToSvgY(open, options.height);
    double sHigh = clipToSvgY(high, options.height);
    double sLow = clipToSvgY(low, options.height);
    double sClose = clipToSvgY(close, options.height);

    // halfWidth is in clip-space X units; convert to SVG pixels
    double hw = static_cast<double>(halfW) * options.width * 0.5;

    // Wick (high to low vertical line)
    ss << "<line x1=\"" << fmtDouble(sx, 2)
       << "\" y1=\"" << fmtDouble(sHigh, 2)
       << "\" x2=\"" << fmtDouble(sx, 2)
       << "\" y2=\"" << fmtDouble(sLow, 2)
       << "\" stroke=\"" << fillColor
       << "\" stroke-width=\"1\"/>\n";

    // Body (rect from open to close)
    double bodyTop = std::min(sOpen, sClose);
    double bodyH = std::fabs(sClose - sOpen);
    if (bodyH < 1.0) bodyH = 1.0;  // minimum visible height

    ss << "<rect x=\"" << fmtDouble(sx - hw, 2)
       << "\" y=\"" << fmtDouble(bodyTop, 2)
       << "\" width=\"" << fmtDouble(hw * 2.0, 2)
       << "\" height=\"" << fmtDouble(bodyH, 2)
       << "\" fill=\"" << fillColor << "\"/>\n";
  }

  return ss.str();
}

std::string SvgExporter::renderText(const Scene& /*scene*/,
                                      const IngestProcessor* /*ingest*/,
                                      const DrawItem& /*di*/,
                                      const SvgExportOptions& /*options*/) {
  // textSDF@1 is glyph-based and cannot be trivially converted to SVG text
  // without the glyph atlas mapping. Skip for now (same as pick shaders skip text).
  return "<!-- textSDF not exported to SVG -->\n";
}

// ---- Main export ----

std::string SvgExporter::exportScene(const Scene& scene,
                                       const IngestProcessor* ingest,
                                       const SvgExportOptions& options) {
  std::ostringstream svg;
  svg << buildHeader(options);
  svg << buildBackground(options);

  // Collect gradient defs
  std::string defs;
  auto diIds = scene.drawItemIds();
  for (Id diId : diIds) {
    const DrawItem* di = scene.getDrawItem(diId);
    if (!di || !di->visible) continue;
    std::string gDef = buildGradientDef(*di, static_cast<std::uint32_t>(diId));
    if (!gDef.empty()) defs += gDef;
  }
  if (!defs.empty()) {
    svg << "<defs>\n" << defs << "</defs>\n";
  }

  // Walk panes -> layers -> draw items (sorted by ID for deterministic order)
  auto paneIds = scene.paneIds();
  for (Id paneId : paneIds) {
    const Pane* pane = scene.getPane(paneId);
    if (!pane) continue;

    svg << "<g id=\"pane_" << paneId << "\"";
    if (!pane->name.empty()) {
      svg << " data-name=\"" << escapeXml(pane->name) << "\"";
    }
    svg << ">\n";

    // Pane clear color as background rect (if set)
    if (pane->hasClearColor) {
      double px0 = clipToSvgX(pane->region.clipXMin, options.width);
      double py0 = clipToSvgY(pane->region.clipYMax, options.height);
      double px1 = clipToSvgX(pane->region.clipXMax, options.width);
      double py1 = clipToSvgY(pane->region.clipYMin, options.height);
      svg << "<rect x=\"" << fmtDouble(px0, 2)
          << "\" y=\"" << fmtDouble(py0, 2)
          << "\" width=\"" << fmtDouble(px1 - px0, 2)
          << "\" height=\"" << fmtDouble(py1 - py0, 2)
          << "\" fill=\"" << colorToHex(pane->clearColor)
          << "\"/>\n";
    }

    auto layerIds = scene.layerIds();
    for (Id layerId : layerIds) {
      const Layer* layer = scene.getLayer(layerId);
      if (!layer || layer->paneId != paneId) continue;

      svg << "<g id=\"layer_" << layerId << "\"";
      if (!layer->name.empty()) {
        svg << " data-name=\"" << escapeXml(layer->name) << "\"";
      }
      svg << ">\n";

      for (Id dId : diIds) {
        const DrawItem* di = scene.getDrawItem(dId);
        if (!di || di->layerId != layerId) continue;
        if (!di->visible) continue;
        if (di->isClipSource) continue;  // clip sources produce no visible output

        // Wrap in transform group if transform is set
        bool hasXform = (di->transformId != 0);
        if (hasXform) {
          const Transform* xform = scene.getTransform(di->transformId);
          if (xform) {
            std::string xStr = transformToSvg(xform->mat3, options.width, options.height);
            svg << "<g transform=\"" << xStr << "\">\n";
          } else {
            hasXform = false;
          }
        }

        // Dispatch based on pipeline type
        std::string content;
        const std::string& pipe = di->pipeline;
        if (pipe == "triSolid@1" || pipe == "triAA@1" || pipe == "triGradient@1") {
          content = renderTriangles(scene, ingest, *di, options);
        } else if (pipe == "line2d@1" || pipe == "lineAA@1") {
          content = renderLines(scene, ingest, *di, options);
        } else if (pipe == "points@1") {
          content = renderPoints(scene, ingest, *di, options);
        } else if (pipe == "instancedRect@1") {
          content = renderRects(scene, ingest, *di, options);
        } else if (pipe == "instancedCandle@1") {
          content = renderCandles(scene, ingest, *di, options);
        } else if (pipe == "textSDF@1") {
          content = renderText(scene, ingest, *di, options);
        } else if (pipe == "texturedQuad@1") {
          content = "<!-- texturedQuad not exported to SVG -->\n";
        }

        svg << content;

        if (hasXform) {
          svg << "</g>\n";
        }
      }

      svg << "</g>\n";
    }

    svg << "</g>\n";
  }

  svg << buildFooter();
  return svg.str();
}

std::string SvgExporter::exportPane(const Scene& scene,
                                      const IngestProcessor* ingest,
                                      Id paneId,
                                      const SvgExportOptions& options) {
  const Pane* pane = scene.getPane(paneId);
  if (!pane) return "";

  std::ostringstream svg;
  svg << buildHeader(options);
  svg << buildBackground(options);

  // Collect gradient defs for draw items in this pane
  std::string defs;
  auto diIds = scene.drawItemIds();
  auto layerIds = scene.layerIds();
  for (Id diId : diIds) {
    const DrawItem* di = scene.getDrawItem(diId);
    if (!di || !di->visible) continue;

    // Check if this draw item belongs to a layer in this pane
    const Layer* layer = scene.getLayer(di->layerId);
    if (!layer || layer->paneId != paneId) continue;

    std::string gDef = buildGradientDef(*di, static_cast<std::uint32_t>(diId));
    if (!gDef.empty()) defs += gDef;
  }
  if (!defs.empty()) {
    svg << "<defs>\n" << defs << "</defs>\n";
  }

  svg << "<g id=\"pane_" << paneId << "\">\n";

  for (Id layerId : layerIds) {
    const Layer* layer = scene.getLayer(layerId);
    if (!layer || layer->paneId != paneId) continue;

    svg << "<g id=\"layer_" << layerId << "\">\n";

    for (Id dId : diIds) {
      const DrawItem* di = scene.getDrawItem(dId);
      if (!di || di->layerId != layerId) continue;
      if (!di->visible) continue;
      if (di->isClipSource) continue;

      bool hasXform = (di->transformId != 0);
      if (hasXform) {
        const Transform* xform = scene.getTransform(di->transformId);
        if (xform) {
          std::string xStr = transformToSvg(xform->mat3, options.width, options.height);
          svg << "<g transform=\"" << xStr << "\">\n";
        } else {
          hasXform = false;
        }
      }

      std::string content;
      const std::string& pipe = di->pipeline;
      if (pipe == "triSolid@1" || pipe == "triAA@1" || pipe == "triGradient@1") {
        content = renderTriangles(scene, ingest, *di, options);
      } else if (pipe == "line2d@1" || pipe == "lineAA@1") {
        content = renderLines(scene, ingest, *di, options);
      } else if (pipe == "points@1") {
        content = renderPoints(scene, ingest, *di, options);
      } else if (pipe == "instancedRect@1") {
        content = renderRects(scene, ingest, *di, options);
      } else if (pipe == "instancedCandle@1") {
        content = renderCandles(scene, ingest, *di, options);
      } else if (pipe == "textSDF@1") {
        content = renderText(scene, ingest, *di, options);
      } else if (pipe == "texturedQuad@1") {
        content = "<!-- texturedQuad not exported to SVG -->\n";
      }

      svg << content;

      if (hasXform) {
        svg << "</g>\n";
      }
    }

    svg << "</g>\n";
  }

  svg << "</g>\n";
  svg << buildFooter();
  return svg.str();
}

} // namespace dc
