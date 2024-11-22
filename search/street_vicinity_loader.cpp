#include "search/street_vicinity_loader.hpp"

#include "indexer/cell_id.hpp"
#include "indexer/feature_covering.hpp"
#include "indexer/feature_decl.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "geometry/mercator.hpp"
#include "geometry/point2d.hpp"

#include "base/math.hpp"


namespace search
{
StreetVicinityLoader::StreetVicinityLoader(int scale, double offsetMeters)
  : m_context(nullptr), m_scale(scale), m_offsetMeters(offsetMeters), m_cache("Streets")
{
}

void StreetVicinityLoader::SetContext(MwmContext * context)
{
  ASSERT(context, ());
  if (m_context == context)
    return;

  m_context = context;
  auto const scaleRange = m_context->m_value.GetHeader().GetScaleRange();
  m_scale = math::Clamp(m_scale, scaleRange.first, scaleRange.second);
}

void StreetVicinityLoader::OnQueryFinished() { m_cache.ClearIfNeeded(); }

StreetVicinityLoader::Street const & StreetVicinityLoader::GetStreet(uint32_t featureId)
{
  auto r = m_cache.Get(featureId);
  if (!r.second)
    return r.first;

  LoadStreet(featureId, r.first);
  return r.first;
}

bool StreetVicinityLoader::IsStreet(FeatureType & ft)
{
  auto const geomType = ft.GetGeomType();
  // Highway should be line or area.
  bool const isLineOrArea = (geomType == feature::GeomType::Line || geomType == feature::GeomType::Area);
  // Square also maybe a point (besides line or area).
  return ((isLineOrArea && ftypes::IsWayChecker::Instance()(ft)) || ftypes::IsSquareChecker::Instance()(ft));
}

void StreetVicinityLoader::LoadStreet(uint32_t featureId, Street & street)
{
  auto feature = m_context->GetFeature(featureId);
  if (!feature || !IsStreet(*feature))
    return;

  /// @todo Can be optimized here. Do not aggregate rect, but aggregate covering intervals for each segment, instead.
  auto const sumRect = [&street, this](m2::PointD const & pt)
  {
    street.m_rect.Add(mercator::RectByCenterXYAndSizeInMeters(pt, m_offsetMeters));
  };

  if (feature->GetGeomType() == feature::GeomType::Area)
  {
    feature->ForEachTriangle([&sumRect](m2::PointD const & p1, m2::PointD const & p2, m2::PointD const & p3)
                             {
                               sumRect(p1);
                               sumRect(p2);
                               sumRect(p3);
                             }, FeatureType::BEST_GEOMETRY);
  }
  else
    feature->ForEachPoint(sumRect, FeatureType::BEST_GEOMETRY);
  ASSERT(street.m_rect.IsValid(), ());

  covering::CoveringGetter coveringGetter(street.m_rect, covering::ViewportWithLowLevels);
  auto const & intervals = coveringGetter.Get<RectId::DEPTH_LEVELS>(m_scale);
  m_context->ForEachIndex(intervals, m_scale, [this, &street, featureId](uint32_t id)
  {
    if (m_context->GetStreet(id) == featureId)
      street.m_features.push_back(id);
  });

  //street.m_calculator = std::make_unique<ProjectionOnStreetCalculator>(points);
}
}  // namespace search
