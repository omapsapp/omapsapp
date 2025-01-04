#pragma once

#include "indexer/map_style.hpp"

#include "geometry/point2d.hpp"
#include "geometry/point_with_altitude.hpp"

#include <cstdint>
#include <vector>

namespace maps
{
uint32_t constexpr kAltitudeChartBPP = 4;

void ScaleChartData(std::vector<double> & chartData, double scale);
void ShiftChartData(std::vector<double> & chartData, double shift);
void ReflectChartData(std::vector<double> & chartData);

/// \brief fills uniformAltitudeDataM with altitude data which evenly distributed by
/// |resultPointCount| points. |distanceDataM| and |altitudeDataM| form a curve of route altitude.
/// This method is used to generalize and evenly distribute points of the chart.
bool NormalizeChartData(std::vector<double> const & distanceDataM,
                        geometry::Altitudes const & altitudeDataM, size_t resultPointCount,
                        std::vector<double> & uniformAltitudeDataM, 
                        std::vector<double> & uniformSlopeDataM);

/// \brief fills |yAxisDataPxl|. |yAxisDataPxl| is formed to pevent displaying
/// big waves on the chart in case of small deviation in absolute values in |yAxisData|.
/// \param height image chart height in pixels.
/// \param minMetersInPixel minimum meter number per height pixel.
/// \param altitudeDataM altitude data vector in meters.
/// \param yAxisDataPxl Y-axis data of altitude chart in pixels.
bool GenerateYAxisChartData(uint32_t height, double minMetersPerPxl,
                            std::vector<double> const & altitudeDataM,
                            std::vector<double> & yAxisDataPxl);

/// \brief generates chart image on a canvas with size |width|, |height| with |geometry|.
/// (0, 0) is a left-top corner. X-axis goes down and Y-axis goes right.
/// \param width is result image width in pixels.
/// \param height is result image height in pixels.
/// \param geometry is points which is used to draw a curve of the chart.
/// \param mapStyle is a current map style.
/// \param frameBuffer is a vector for a result image. It's resized in this method.
/// It's filled with RGBA(8888) image date.
bool GenerateChartByPoints(uint32_t width, uint32_t height,
                           std::vector<m2::PointD> const & geometry, 
                           std::vector<double> const & slopes,
                           MapStyle mapStyle,
                           std::vector<uint8_t> & frameBuffer);

bool GenerateChart(uint32_t width, uint32_t height, std::vector<double> const & distanceDataM,
                   geometry::Altitudes const & altitudeDataM, MapStyle mapStyle,
                   std::vector<uint8_t> & frameBuffer);
}  // namespace maps
