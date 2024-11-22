#include "drape_frontend/traffic_renderer.hpp"
#include "drape_frontend/color_constants.hpp"
#include "drape_frontend/visual_params.hpp"

#include "shaders/programs.hpp"

#include "drape/glsl_func.hpp"
#include "drape/support_manager.hpp"
#include "drape/vertex_array_buffer.hpp"

#include "indexer/map_style_reader.hpp"
#include "indexer/scales.hpp"

#include "base/logging.hpp"

#include <array>
#include <algorithm>
#include <utility>

namespace df
{
namespace
{
df::ColorConstant const kTrafficArrowLightColor = "TrafficArrowLight";
df::ColorConstant const kTrafficArrowDarkColor = "TrafficArrowDark";
df::ColorConstant const kTrafficOutlineColor = "TrafficOutline";

int constexpr kMinVisibleArrowZoomLevel = 16;
int constexpr kRoadClass2MinVisibleArrowZoomLevel = 17;
int constexpr kOutlineMinZoomLevel = 14;

float const kTrafficArrowAspect = 128.0f / 8.0f;

std::array<float, 20> const kLeftWidthInPixel =
{
  // 1   2     3     4     5     6     7     8     9    10
  0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
  //11   12    13   14    15    16    17   18     19    20
  0.5f, 0.5f, 0.5f, 0.6f, 1.6f, 2.7f, 3.5f, 4.0f, 4.0f, 4.0f
};

std::array<float, 20> const kRightWidthInPixel =
{
  // 1   2     3     4     5     6     7     8     9    10
  2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.5f, 3.0f, 3.0f,
  //11  12    13    14    15    16    17    18    19     20
  3.0f, 3.5f, 4.0f, 3.9f, 3.2f, 2.7f, 3.5f, 4.0f, 4.0f, 4.0f
};

std::array<float, 20> const kRoadClass1WidthScalar =
{
  // 1   2     3     4     5     6     7     8     9    10
  0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.3,
  //11  12    13    14    15    16    17    18    19     20
  0.3, 0.35f, 0.45f, 0.55f, 0.6f, 0.8f, 1.0f, 1.0f, 1.0f, 1.0f
};

std::array<float, 20> const kRoadClass2WidthScalar =
{
  // 1   2     3     4     5     6     7     8     9     10
  0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.3f,
  //11  12    13    14    15     16   17    18    19    20
  0.3f, 0.3f, 0.3f, 0.4f, 0.5f, 0.5f, 0.65f, 0.85f, 0.95f, 1.0f
};

std::array<float, 20> const kTwoWayOffsetInPixel =
{
  // 1   2     3     4     5     6     7     8     9     10
  0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
  //11  12    13    14    15     16   17    18    19    20
  0.0f, 0.5f, 0.5f, 0.75f, 1.7f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f
};

std::array<int, 3> const kLineDrawerRoadClass1 = {12, 13, 14};

std::array<int, 2> const kLineDrawerRoadClass2 = {15, 16};

float CalculateHalfWidth(ScreenBase const & screen, RoadClass const & roadClass, bool left)
{
  double zoom = 0.0;
  int index = 0;
  float lerpCoef = 0.0f;
  ExtractZoomFactors(screen, zoom, index, lerpCoef);

  std::array<float, 20> const * halfWidth = left ? &kLeftWidthInPixel : &kRightWidthInPixel;
  float radius = InterpolateByZoomLevels(index, lerpCoef, *halfWidth);
  if (roadClass == RoadClass::Class1)
    radius *= InterpolateByZoomLevels(index, lerpCoef, kRoadClass1WidthScalar);
  else if (roadClass == RoadClass::Class2)
    radius *= InterpolateByZoomLevels(index, lerpCoef, kRoadClass2WidthScalar);

  return radius * static_cast<float>(VisualParams::Instance().GetVisualScale());
}
}  // namespace

void TrafficRenderer::AddRenderData(ref_ptr<dp::GraphicsContext> context,
                                    ref_ptr<gpu::ProgramManager> mng,
                                    TrafficRenderData && renderData)
{
  // Remove obsolete render data.
  TileKey const tileKey(renderData.m_tileKey);
  m_renderData.erase(std::remove_if(m_renderData.begin(), m_renderData.end(),
                                    [&tileKey](TrafficRenderData const & rd)
  {
    return tileKey == rd.m_tileKey && rd.m_tileKey.m_generation < tileKey.m_generation;
  }), m_renderData.end());

  // Add new render data.
  m_renderData.emplace_back(std::move(renderData));
  TrafficRenderData & rd = m_renderData.back();

  auto program = mng->GetProgram(rd.m_state.GetProgram<gpu::Program>());
  program->Bind();
  rd.m_bucket->GetBuffer()->Build(context, program);

  std::sort(m_renderData.begin(), m_renderData.end());
}

void TrafficRenderer::OnUpdateViewport(CoverageResult const & coverage, int currentZoomLevel,
                                       buffer_vector<TileKey, 8> const & tilesToDelete)
{
  m_renderData.erase(std::remove_if(m_renderData.begin(), m_renderData.end(),
                                    [&coverage, &currentZoomLevel, &tilesToDelete](TrafficRenderData const & rd)
  {
    return rd.m_tileKey.m_zoomLevel == currentZoomLevel &&
           (rd.m_tileKey.m_x < coverage.m_minTileX || rd.m_tileKey.m_x >= coverage.m_maxTileX ||
           rd.m_tileKey.m_y < coverage.m_minTileY || rd.m_tileKey.m_y >= coverage.m_maxTileY ||
           base::IsExist(tilesToDelete, rd.m_tileKey));
  }), m_renderData.end());
}

void TrafficRenderer::OnGeometryReady(int currentZoomLevel)
{
  m_renderData.erase(std::remove_if(m_renderData.begin(), m_renderData.end(),
                                    [&currentZoomLevel](TrafficRenderData const & rd)
  {
    return rd.m_tileKey.m_zoomLevel != currentZoomLevel;
  }), m_renderData.end());
}

void TrafficRenderer::RenderTraffic(ref_ptr<dp::GraphicsContext> context,
                                    ref_ptr<gpu::ProgramManager> mng, ScreenBase const & screen,
                                    int zoomLevel, float opacity, FrameValues const & frameValues)
{
  if (m_renderData.empty() || zoomLevel < kRoadClass0ZoomLevel)
    return;

  dp::Color const lightArrowColor = df::GetColorConstant(df::kTrafficArrowLightColor);
  dp::Color const darkArrowColor = df::GetColorConstant(df::kTrafficArrowDarkColor);
  dp::Color const outlineColor = df::GetColorConstant(df::kTrafficOutlineColor);

  for (TrafficRenderData & renderData : m_renderData)
  {
    if (renderData.m_state.GetDrawAsLine())
    {
      auto program = mng->GetProgram(renderData.m_state.GetProgram<gpu::Program>());
      program->Bind();
      dp::ApplyState(context, program, renderData.m_state);

      gpu::TrafficProgramParams params;
      frameValues.SetTo(params);
      math::Matrix<float, 4, 4> const mv = renderData.m_tileKey.GetTileBasedModelView(screen);
      params.m_modelView = glsl::make_mat4(mv.m_data);
      params.m_opacity = opacity;
      mng->GetParamsSetter()->Apply(context, program, params);
      renderData.m_bucket->Render(context, true /* draw as line */);
    }
    else
    {
      auto const program = renderData.m_state.GetProgram<gpu::Program>();
      if (program == gpu::Program::TrafficCircle)
      {
        ref_ptr<dp::GpuProgram> programPtr = mng->GetProgram(program);
        programPtr->Bind();
        dp::ApplyState(context, programPtr, renderData.m_state);

        gpu::TrafficProgramParams params;
        frameValues.SetTo(params);
        math::Matrix<float, 4, 4> const mv = renderData.m_tileKey.GetTileBasedModelView(screen);
        params.m_modelView = glsl::make_mat4(mv.m_data);
        params.m_opacity = opacity;
        // Here we reinterpret light/dark colors as left/right sizes by road classes.
        params.m_lightArrowColor = glsl::vec3(CalculateHalfWidth(screen, RoadClass::Class0, true /* left */),
                                              CalculateHalfWidth(screen, RoadClass::Class1, true /* left */),
                                              CalculateHalfWidth(screen, RoadClass::Class2, true /* left */));
        params.m_darkArrowColor = glsl::vec3(CalculateHalfWidth(screen, RoadClass::Class0, false /* left */),
                                             CalculateHalfWidth(screen, RoadClass::Class1, false /* left */),
                                             CalculateHalfWidth(screen, RoadClass::Class2, false /* left */));
        mng->GetParamsSetter()->Apply(context, programPtr, params);

        renderData.m_bucket->Render(context, false /* draw as line */);
        continue;
      }

      // Filter by road class.
      int minVisibleArrowZoomLevel = kMinVisibleArrowZoomLevel;
      float outline = 0.0f;

      int visibleZoomLevel = kRoadClass0ZoomLevel;
      if (renderData.m_roadClass == RoadClass::Class0)
      {
        outline = (zoomLevel <= kOutlineMinZoomLevel ? 1.0f : 0.0f);
      }
      else if (renderData.m_roadClass == RoadClass::Class1)
      {
        outline = (zoomLevel <= kOutlineMinZoomLevel ? 1.0f : 0.0f);
        visibleZoomLevel = kRoadClass1ZoomLevel;
      }
      else if (renderData.m_roadClass == RoadClass::Class2)
      {
        visibleZoomLevel = kRoadClass2ZoomLevel;
        minVisibleArrowZoomLevel = kRoadClass2MinVisibleArrowZoomLevel;
      }
      if (zoomLevel < visibleZoomLevel)
        continue;

      float const leftPixelHalfWidth =
          CalculateHalfWidth(screen, renderData.m_roadClass, true /* left */);
      float const invLeftPixelLength = 1.0f / (2.0f * leftPixelHalfWidth * kTrafficArrowAspect);
      float const rightPixelHalfWidth =
          CalculateHalfWidth(screen, renderData.m_roadClass, false /* left */);
      float const kEps = 1e-5;
      if (fabs(leftPixelHalfWidth) < kEps && fabs(rightPixelHalfWidth) < kEps)
        continue;

      ref_ptr<dp::GpuProgram> programPtr = mng->GetProgram(program);
      programPtr->Bind();
      dp::ApplyState(context, programPtr, renderData.m_state);

      gpu::TrafficProgramParams params;
      frameValues.SetTo(params);
      math::Matrix<float, 4, 4> const mv = renderData.m_tileKey.GetTileBasedModelView(screen);
      params.m_modelView = glsl::make_mat4(mv.m_data);
      params.m_opacity = opacity;
      params.m_outline = outline;
      params.m_lightArrowColor = glsl::ToVec3(lightArrowColor);
      params.m_darkArrowColor = glsl::ToVec3(darkArrowColor);
      params.m_outlineColor = glsl::ToVec3(outlineColor);
      params.m_trafficParams = glsl::vec4(leftPixelHalfWidth, rightPixelHalfWidth, invLeftPixelLength,
                                          zoomLevel >= minVisibleArrowZoomLevel ? 1.0f : 0.0f);
      mng->GetParamsSetter()->Apply(context, programPtr, params);

      renderData.m_bucket->Render(context, false /* draw as line */);
    }
  }
}

void TrafficRenderer::ClearContextDependentResources()
{
  m_renderData.clear();
}

void TrafficRenderer::Clear(MwmSet::MwmId const & mwmId)
{
  auto removePredicate = [&mwmId](TrafficRenderData const & data) { return data.m_mwmId == mwmId; };

  m_renderData.erase(std::remove_if(m_renderData.begin(), m_renderData.end(), removePredicate),
                                    m_renderData.end());
}

// static
float TrafficRenderer::GetTwoWayOffset(RoadClass const & roadClass, int zoomLevel)
{
  // There is no offset for class-0 roads, the offset for them is created by
  // kLeftWidthInPixel and kRightWidthInPixel.
  int const kRoadClass0MinZoomLevel = 14;
  if (roadClass == RoadClass::Class0 && zoomLevel <= kRoadClass0MinZoomLevel)
    return 0.0f;

  ASSERT_GREATER(zoomLevel, 1, ());
  ASSERT_LESS_OR_EQUAL(zoomLevel, scales::GetUpperStyleScale(), ());
  int const index = zoomLevel - 1;
  float const halfWidth = 0.5f * df::TrafficRenderer::GetPixelWidth(roadClass, zoomLevel);
  return kTwoWayOffsetInPixel[index] *
             static_cast<float>(VisualParams::Instance().GetVisualScale()) +
         halfWidth;
}

// static
float TrafficRenderer::GetPixelWidth(RoadClass const & roadClass, int zoomLevel)
{
  int width = 0;
  if (CanBeRenderedAsLine(roadClass, zoomLevel, width))
    return static_cast<float>(width);

  return GetPixelWidthInternal(roadClass, zoomLevel);
}

// static
float TrafficRenderer::GetPixelWidthInternal(RoadClass const & roadClass, int zoomLevel)
{
  ASSERT_GREATER(zoomLevel, 1, ());
  ASSERT_LESS_OR_EQUAL(zoomLevel, scales::GetUpperStyleScale(), ());
  std::array<float, 20> const * widthScalar = nullptr;
  if (roadClass == RoadClass::Class1)
    widthScalar = &kRoadClass1WidthScalar;
  else if (roadClass == RoadClass::Class2)
    widthScalar = &kRoadClass2WidthScalar;

  int const index = zoomLevel - 1;
  float const baseWidth = (kLeftWidthInPixel[index] + kRightWidthInPixel[index]) *
                          static_cast<float>(df::VisualParams::Instance().GetVisualScale());
  return (widthScalar != nullptr) ? (baseWidth * (*widthScalar)[index]) : baseWidth;
}

// static
bool TrafficRenderer::CanBeRenderedAsLine(RoadClass const & roadClass, int zoomLevel, int & width)
{
  if (roadClass == RoadClass::Class0)
    return false;

  int const *lineDrawer = nullptr;
  int const *lineDrawerEnd = nullptr;
  if (roadClass == RoadClass::Class1)
    std::tie(lineDrawer, lineDrawerEnd) = std::make_pair(
        kLineDrawerRoadClass1.data(), kLineDrawerRoadClass1.data() + kLineDrawerRoadClass1.size());
  else if (roadClass == RoadClass::Class2)
    std::tie(lineDrawer, lineDrawerEnd) = std::make_pair(
        kLineDrawerRoadClass2.data(), kLineDrawerRoadClass2.data() + kLineDrawerRoadClass2.size());

  ASSERT(lineDrawer != nullptr, ());
  auto it = std::find(lineDrawer, lineDrawerEnd, zoomLevel);
  if (it == lineDrawerEnd)
    return false;

  width = std::max(1, math::SignedRound(TrafficRenderer::GetPixelWidthInternal(roadClass, zoomLevel)));
  return width <= dp::SupportManager::Instance().GetMaxLineWidth();
}
}  // namespace df
