#include "map/chart_generator.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"
#include "base/math.hpp"

#include <algorithm>

#include "3party/agg/agg_conv_curve.h"
#include "3party/agg/agg_conv_stroke.h"
#include "3party/agg/agg_path_storage.h"
#include "3party/agg/agg_pixfmt_rgba.h"
#include "3party/agg/agg_rasterizer_scanline_aa.h"
#include "3party/agg/agg_renderer_scanline.h"
#include "3party/agg/agg_scanline_p.h"

namespace maps
{
using namespace std;

namespace
{
template <class Color, class Order>
struct BlendAdaptor
{
  using order_type = Order;
  using color_type = Color;
  using TValueType = typename color_type::value_type;
  using TCalcType = typename color_type::calc_type;

  enum
  {
    ScaleShift = color_type::base_shift,
    ScaleMask = color_type::base_mask,
  };

  static AGG_INLINE void blend_pix(unsigned op, TValueType * p, unsigned cr, unsigned cg,
                                   unsigned cb, unsigned ca, unsigned cover)
  {
    using TBlendTable = agg::comp_op_table_rgba<Color, Order>;
    if (p[Order::A])
    {
      TBlendTable::g_comp_op_func[op](p, (cr * ca + ScaleMask) >> ScaleShift,
                                      (cg * ca + ScaleMask) >> ScaleShift,
                                      (cb * ca + ScaleMask) >> ScaleShift, ca, cover);
    }
    else
    {
      TBlendTable::g_comp_op_func[op](p, cr, cg, cb, ca, cover);
    }
  }
};

agg::rgba8 GetLineColor(MapStyle mapStyle)
{
  switch (mapStyle)
  {
  case MapStyleCount:
    LOG(LERROR, ("Wrong map style param."));  // fallthrough
  case MapStyleDefaultDark:
  case MapStyleVehicleDark:
  case MapStyleOutdoorsDark:
    return agg::rgba8(255, 230, 140, 255);
  case MapStyleDefaultLight:
  case MapStyleVehicleLight:
  case MapStyleOutdoorsLight:
  case MapStyleMerged:
    return agg::rgba8(30, 150, 240, 255);
  }
  UNREACHABLE();
}

agg::rgba8 GetCurveColor(MapStyle mapStyle)
{
  switch (mapStyle)
  {
  case MapStyleCount:
    LOG(LERROR, ("Wrong map style param."));
    [[fallthrough]];
    // No need break or return here.
  case MapStyleDefaultDark:
  case MapStyleVehicleDark:
  case MapStyleOutdoorsDark:
    return agg::rgba8(255, 230, 140, 20);
  case MapStyleDefaultLight:
  case MapStyleVehicleLight:
  case MapStyleOutdoorsLight:
  case MapStyleMerged:
    return agg::rgba8(30, 150, 240, 20);
  }
  UNREACHABLE();
}
}  // namespace

void ScaleChartData(vector<double> & chartData, double scale)
{
  for (size_t i = 0; i < chartData.size(); ++i)
    chartData[i] *= scale;
}

void ShiftChartData(vector<double> & chartData, double shift)
{
  for (size_t i = 0; i < chartData.size(); ++i)
    chartData[i] += shift;
}

void ReflectChartData(vector<double> & chartData)
{
  for (size_t i = 0; i < chartData.size(); ++i)
    chartData[i] = -chartData[i];
}

bool NormalizeChartData(vector<double> const & distanceDataM,
                        geometry::Altitudes const & altitudeDataM, size_t resultPointCount,
                        vector<double> & uniformAltitudeDataM)
{
  double constexpr kEpsilon = 1e-6;

  if (distanceDataM.size() != altitudeDataM.size())
  {
    LOG(LERROR, ("Altitude and distance data have different size."));
    return false;
  }

  if (!is_sorted(distanceDataM.cbegin(), distanceDataM.cend()))
  {
    LOG(LERROR, ("Route segment distances are not sorted."));
    return false;
  }

  if (distanceDataM.empty() || resultPointCount == 0)
  {
    uniformAltitudeDataM.clear();
    return true;
  }

  auto const calculateAltitude = [&](double distFormStartM) {
    if (distFormStartM <= distanceDataM.front())
      return static_cast<double>(altitudeDataM.front());
    if (distFormStartM >= distanceDataM.back())
      return static_cast<double>(altitudeDataM.back());

    auto const lowerIt = lower_bound(distanceDataM.cbegin(), distanceDataM.cend(), distFormStartM);
    size_t const nextPointIdx = distance(distanceDataM.cbegin(), lowerIt);
    ASSERT_LESS(0, nextPointIdx, ("distFormStartM is greater than 0 but nextPointIdx == 0."));
    size_t const prevPointIdx = nextPointIdx - 1;

    if (AlmostEqualAbs(distanceDataM[prevPointIdx], distanceDataM[nextPointIdx], kEpsilon))
      return static_cast<double>(altitudeDataM[prevPointIdx]);

    double const k = (altitudeDataM[nextPointIdx] - altitudeDataM[prevPointIdx]) /
                     (distanceDataM[nextPointIdx] - distanceDataM[prevPointIdx]);
    return static_cast<double>(altitudeDataM[prevPointIdx]) +
           k * (distFormStartM - distanceDataM[prevPointIdx]);
  };

  double const routeLenM = distanceDataM.back();
  uniformAltitudeDataM.resize(resultPointCount);
  double const stepLenM = resultPointCount <= 1 ? 0.0 : routeLenM / (resultPointCount - 1);

  for (size_t i = 0; i < resultPointCount; ++i)
    uniformAltitudeDataM[i] = calculateAltitude(static_cast<double>(i) * stepLenM);

  return true;
}

bool GenerateYAxisChartData(uint32_t height, double minMetersPerPxl,
                            vector<double> const & altitudeDataM, vector<double> & yAxisDataPxl)
{
  if (altitudeDataM.empty())
  {
    yAxisDataPxl.clear();
    return true;
  }

  uint32_t constexpr kHeightIndentPxl = 2;
  uint32_t heightIndentPxl = kHeightIndentPxl;
  if (height <= 2 * kHeightIndentPxl)
  {
    LOG(LERROR, ("Chart height is less or equal than 2 * kHeightIndentPxl (", 2 * kHeightIndentPxl, ")"));
    heightIndentPxl = 0;
  }

  auto const minMaxAltitudeIt = minmax_element(altitudeDataM.begin(), altitudeDataM.end());
  double const minAltM = *minMaxAltitudeIt.first;
  double const maxAltM = *minMaxAltitudeIt.second;
  double const deltaAltM = maxAltM - minAltM;
  uint32_t const drawHeightPxl = height - 2 * heightIndentPxl;
  double const metersPerPxl = max(minMetersPerPxl, deltaAltM / static_cast<double>(drawHeightPxl));
  if (metersPerPxl == 0.0)
  {
    LOG(LERROR, ("metersPerPxl == 0.0"));
    return false;
  }
  // int avoids double errors which make freeHeightSpacePxl slightly less than zero.
  int const deltaAltPxl = deltaAltM / metersPerPxl;
  double const freeHeightSpacePxl = drawHeightPxl - deltaAltPxl;
  if (freeHeightSpacePxl < 0 || freeHeightSpacePxl > drawHeightPxl)
  {
    LOG(LERROR, ("Number of pixels free of chart points (", freeHeightSpacePxl,
                 ") is below zero or greater than the number of pixels for the chart (", drawHeightPxl, ")."));
    return false;
  }

  double const maxAltPxl = maxAltM / metersPerPxl;
  yAxisDataPxl.assign(altitudeDataM.cbegin(), altitudeDataM.cend());
  ScaleChartData(yAxisDataPxl, 1.0 / metersPerPxl);
  ReflectChartData(yAxisDataPxl);
  ShiftChartData(yAxisDataPxl, maxAltPxl + heightIndentPxl + freeHeightSpacePxl / 2.0);

  return true;
}

bool GenerateChartByPoints(uint32_t width, uint32_t height, vector<m2::PointD> const & geometry,
                           MapStyle mapStyle, vector<uint8_t> & frameBuffer)
{
  frameBuffer.clear();
  if (width == 0 || height == 0)
    return false;

  agg::rgba8 const kBackgroundColor = agg::rgba8(255, 255, 255, 0);
  agg::rgba8 const kLineColor = GetLineColor(mapStyle);
  agg::rgba8 const kCurveColor = GetCurveColor(mapStyle);
  double constexpr kLineWidthPxl = 2.0;

  using TBlender = BlendAdaptor<agg::rgba8, agg::order_rgba>;
  using TPixelFormat = agg::pixfmt_custom_blend_rgba<TBlender, agg::rendering_buffer>;
  using TBaseRenderer = agg::renderer_base<TPixelFormat>;
  using TPath = agg::poly_container_adaptor<vector<m2::PointD>>;
  using TStroke = agg::conv_stroke<TPath>;

  agg::rendering_buffer renderBuffer;
  TPixelFormat pixelFormat(renderBuffer, agg::comp_op_src_over);
  TBaseRenderer baseRenderer(pixelFormat);

  frameBuffer.assign(width * kAltitudeChartBPP * height, 0);
  renderBuffer.attach(&frameBuffer[0], static_cast<unsigned>(width),
                      static_cast<unsigned>(height), static_cast<int>(width * kAltitudeChartBPP));

  // Background.
  baseRenderer.reset_clipping(true);
  unsigned const op = pixelFormat.comp_op();
  pixelFormat.comp_op(agg::comp_op_src);
  baseRenderer.clear(kBackgroundColor);
  pixelFormat.comp_op(op);

  agg::rasterizer_scanline_aa<> rasterizer;
  rasterizer.clip_box(0, 0, width, height);

  if (geometry.empty())
    return true; /* No chart line to draw. */

  // Polygon under chart line.
  agg::path_storage underChartGeometryPath;
  underChartGeometryPath.move_to(geometry.front().x, static_cast<double>(height));
  for (auto const & p : geometry)
    underChartGeometryPath.line_to(p.x, p.y);
  underChartGeometryPath.line_to(geometry.back().x, static_cast<double>(height));
  underChartGeometryPath.close_polygon();

  agg::conv_curve<agg::path_storage> curve(underChartGeometryPath);
  rasterizer.add_path(curve);
  agg::scanline32_p8 scanline;
  agg::render_scanlines_aa_solid(rasterizer, scanline, baseRenderer, kCurveColor);

  // Chart line.
  TPath path_adaptor(geometry, false);
  TStroke stroke(path_adaptor);
  stroke.width(kLineWidthPxl);
  stroke.line_cap(agg::round_cap);
  stroke.line_join(agg::round_join);

  rasterizer.add_path(stroke);
  agg::render_scanlines_aa_solid(rasterizer, scanline, baseRenderer, kLineColor);
  return true;
}

bool GenerateChart(uint32_t width, uint32_t height, vector<double> const & distanceDataM,
                   geometry::Altitudes const & altitudeDataM, MapStyle mapStyle,
                   vector<uint8_t> & frameBuffer)
{
  if (distanceDataM.size() != altitudeDataM.size())
  {
    LOG(LERROR, ("The route is in inconsistent state. Size of altitudes is", altitudeDataM.size(),
                 ". Number of segment is", distanceDataM.size()));
    return false;
  }

  vector<double> uniformAltitudeDataM;
  if (!NormalizeChartData(distanceDataM, altitudeDataM, width, uniformAltitudeDataM))
    return false;

  vector<double> yAxisDataPxl;
  if (!GenerateYAxisChartData(height, 1.0 /* minMetersPerPxl */, uniformAltitudeDataM, yAxisDataPxl))
    return false;

  size_t const uniformAltitudeDataSize = yAxisDataPxl.size();
  vector<m2::PointD> geometry(uniformAltitudeDataSize);

  if (uniformAltitudeDataSize != 0)
  {
    double const oneSegLenPix =
        static_cast<double>(width) / (uniformAltitudeDataSize == 1 ? 1 : (uniformAltitudeDataSize - 1));
    for (size_t i = 0; i < uniformAltitudeDataSize; ++i)
      geometry[i] = m2::PointD(i * oneSegLenPix, yAxisDataPxl[i]);
  }

  return GenerateChartByPoints(width, height, geometry, mapStyle, frameBuffer);
}
}  // namespace maps
