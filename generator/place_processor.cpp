#include "generator/place_processor.hpp"

#include "generator/cluster_finder.hpp"
#include "generator/feature_maker_base.hpp"
#include "generator/routing_city_boundaries_processor.hpp"

#include "indexer/classificator.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "geometry/area_on_earth.hpp"

#include "base/stl_helpers.hpp"

#include <algorithm>
#include <limits>


namespace generator
{
using namespace feature;

m2::RectD GetLimitRect(FeaturePlace const & fp)
{
  return fp.m_rect;
}

uint32_t FeaturePlace::GetPlaceType() const
{
  static uint32_t const placeType = classif().GetTypeByPath({"place"});
  return m_fb.GetParams().FindType(placeType, 1 /* level */);
}

bool FeaturePlace::IsRealCapital() const
{
  static uint32_t const capitalType = classif().GetTypeByPath({"place", "city", "capital", "2"});
  return m_fb.GetParams().IsTypeExist(capitalType);
}

double GetRadiusM(ftypes::LocalityType const & type)
{
  switch (type)
  {
  case ftypes::LocalityType::Country: return 1000000.0;
  case ftypes::LocalityType::State: return 100000.0;
  case ftypes::LocalityType::City: return 30000.0;
  case ftypes::LocalityType::Town: return 20000.0;
  case ftypes::LocalityType::Village: return 5000.0;
  default: return 10000.0;
  }
}

// Returns true if left place is worse than right place.
bool IsWorsePlace(FeaturePlace const & left, FeaturePlace const & right)
{
  // Capital type should be always better.
  bool const lCapital = left.IsRealCapital();
  bool const rCapital = right.IsRealCapital();
  if (lCapital != rCapital)
    return rCapital;

  double constexpr kRankCoeff = 1.0;
  double constexpr kLangsCountCoeff = 1.0;
  double constexpr kAreaCoeff = 0.05;
  double constexpr kIsCapitalCoeff = 0.1;
  double constexpr kIsNodeCoeff = 0.15;
  double constexpr kIsAreaTooBigCoeff = -0.5;

  auto const normalizeRank = [](uint8_t rank)
  {
    return static_cast<double>(rank) / static_cast<double>(std::numeric_limits<uint8_t>::max());
  };

  auto const normalizeLangsCount = [](uint8_t langsCount)
  {
    return static_cast<double>(langsCount) /
           static_cast<double>(StringUtf8Multilang::kMaxSupportedLanguages);
  };

  auto const normalizeArea = [](double area)
  {
    // We need to compare areas to choose bigger feature from multipolygonal features.
    // |kMaxAreaM2| should be greater than cities exclaves (like airports or Zelenograd for Moscow).
    double const kMaxAreaM2 = 4e8;
    area = math::Clamp(area, 0.0, kMaxAreaM2);
    return area / kMaxAreaM2;
  };

  auto const isAreaTooBig = [](ftypes::LocalityType type, double area)
  {
    // 100*100 km. There are few such big cities in the world (Beijing, Tokyo). These cities are
    // well-maped and have node which we want to prefer because relation boundaries may include big
    // exclaves and/or have bad center: https://www.openstreetmap.org/relation/1543125.
    if (type == ftypes::LocalityType::City)
      return area > 1e10;

    /// @todo By VNG: It doesn't work with updated heuristic when collecting boundaries.
    /// Should connect (by name) Node and Way places to select best boundary (like with Relations).
    /// @see Relation_Wiki test.
    /*
    // ~14*14 km
    if (type == ftypes::LocalityType::Town)
      return area > 2e8;

    // 10*10 km
    if (type == ftypes::LocalityType::Village)
      return area > 1e8;
    */

    return false;
  };

  static_assert(kRankCoeff >= 0, "");
  static_assert(kLangsCountCoeff >= 0, "");
  static_assert(kAreaCoeff >= 0, "");
  static_assert(kIsCapitalCoeff >= 0, "");
  static_assert(kIsAreaTooBigCoeff <= 0, "");

  auto const getScore = [&](FeaturePlace const & place)
  {
    auto const rank = place.GetRank();
    auto const langsCount = place.m_fb.GetMultilangName().CountLangs();
    auto const area = mercator::AreaOnEarth(place.m_rect);

    auto const placeType = place.GetPlaceType();
    auto const isCapital = ftypes::IsCapitalChecker::Instance()(placeType);

    auto const type = place.m_fb.GetMostGenericOsmId().GetType();
    auto const isNode = (type == base::GeoObjectId::Type::OsmNode) ||
                        (type == base::GeoObjectId::Type::ObsoleteOsmNode);

    auto const tooBig =
        isAreaTooBig(ftypes::IsLocalityChecker::Instance().GetType(placeType), area);

    return kRankCoeff * normalizeRank(rank) +
           kLangsCountCoeff * normalizeLangsCount(langsCount) +
           kAreaCoeff * normalizeArea(area) +
           kIsCapitalCoeff * (isCapital ? 1.0 : 0.0) +
           kIsNodeCoeff * (isNode ? 1.0 : 0.0) +
           kIsAreaTooBigCoeff * (tooBig ? 1.0 : 0.0);
  };

  return getScore(left) < getScore(right);
}

std::vector<std::vector<FeaturePlace const *>> FindClusters(std::vector<FeaturePlace> const & places)
{
  auto const & localityChecker = ftypes::IsLocalityChecker::Instance();

  auto getRaduis = [&localityChecker](FeaturePlace const & fp)
  {
    return GetRadiusM(localityChecker.GetType(fp.GetPlaceType()));
  };

  auto isSamePlace = [&localityChecker](FeaturePlace const & lFP, FeaturePlace const & rFP)
  {
    if (lFP.m_rect.IsIntersect(rFP.m_rect))
      return true;

    // If we have "true" boundaries and they are not intersecting - return false.
    // IsEmptyInterior() == true for Point FB.
    if (!lFP.m_rect.IsEmptyInterior() && !rFP.m_rect.IsEmptyInterior())
      return false;

    /// @todo Small hack here. Need to check enclosing/parent region.
    /// @see https://github.com/organicmaps/organicmaps/issues/2035 for more details.
    // Do not merge places with different ranks (population). See https://www.openstreetmap.org/#map=15/33.0145/-92.7249
    // Junction City in Arkansas and Louisiana.
    if (lFP.GetRank() > 0 && rFP.GetRank() > 0 && lFP.GetRank() != rFP.GetRank())
      return false;

    // Assume that equal-type localities within some fixed radius (GetRadiusM) are the same.
    return localityChecker.GetType(lFP.GetPlaceType()) == localityChecker.GetType(lFP.GetPlaceType());
  };

  return GetClusters(places, std::move(getRaduis), std::move(isSamePlace));
}

PlaceProcessor::PlaceProcessor(std::string const & filename)
  : m_logTag("PlaceProcessor")
{
  if (!filename.empty())
    m_boundariesHolder.Deserialize(filename);
}

std::vector<FeatureBuilder> PlaceProcessor::ProcessPlaces(std::vector<IDsContainerT> * ids/* = nullptr*/)
{
  std::vector<FeatureBuilder> finalPlaces;
  for (auto const & name2PlacesEntry : m_nameToPlaces)
  {
    for (auto const & cluster : FindClusters(name2PlacesEntry.second))
    {
      auto best = std::max_element(cluster.begin(), cluster.end(),
          [](FeaturePlace const * l, FeaturePlace const * r)
          {
            return IsWorsePlace(*l, *r);
          });

      auto bestFb = (*best)->m_fb;

      /// @todo Assign FB point from possible boundary node from |bestBnd| below?
      if (bestFb.IsArea())
        TransformToPoint(bestFb);

      IDsContainerT fbIDs;
      fbIDs.reserve(cluster.size());
      base::Transform(cluster, std::back_inserter(fbIDs), [](FeaturePlace const * fp)
      {
        return fp->m_fb.GetMostGenericOsmId();
      });

      auto const bestBnd = m_boundariesHolder.GetBestBoundary(fbIDs, bestFb.GetKeyPoint());
      if (bestBnd)
      {
        auto const id = bestFb.GetMostGenericOsmId();

        // Sanity check
        using namespace ftypes;
        auto const bndLocality = bestBnd->GetPlace();
        auto const fbLocality = IsLocalityChecker::Instance().GetType((*best)->GetPlaceType());
        if (bndLocality != fbLocality)
        {
          LOG(LWARNING, (m_logTag, "Different locality types:", bndLocality, fbLocality, "for", id));
          // Happens in USA. For example: https://www.openstreetmap.org/relation/6603680
          if (bndLocality == LocalityType::Village && fbLocality == LocalityType::City)
            continue;
        }

        double exactArea = 0.0;
        bool const checkArea = !bestBnd->IsHonestCity();

        // Assign bestBoundary to the bestFb.
        for (auto const & poly : bestBnd->m_boundary)
        {
          if (checkArea)
            exactArea += generator::AreaOnEarth(poly);
          m_boundariesTable.Append(id, indexer::CityBoundary(poly));
        }

        if (checkArea)
        {
          // Heuristic: filter *very* big Relation borders. Some examples:
          // https://www.openstreetmap.org/relation/2374222
          // https://www.openstreetmap.org/relation/3388660

          double const circleArea = ms::CircleAreaOnEarth(
                GetRadiusByPopulationForRouting(bestBnd->GetPopulation(), bndLocality));
          if (exactArea > circleArea * 20.0)
          {
            LOG(LWARNING, (m_logTag, "Delete big boundary for", id, exactArea / circleArea));
            m_boundariesTable.Delete(id);
          }
        }
      }

      if (ids)
        ids->push_back(std::move(fbIDs));

      finalPlaces.emplace_back(std::move(bestFb));
    }
  }

  return finalPlaces;
}

FeaturePlace PlaceProcessor::CreatePlace(feature::FeatureBuilder && fb) const
{
  m2::RectD rect = fb.GetLimitRect();

  if (fb.GetGeomType() == GeomType::Point)
  {
    // Update point rect with boundary polygons. Rect is used to filter places.
    auto const id = fb.GetMostGenericOsmId();

    m2::RectD const r = m_boundariesHolder.GetBoundaryRect(id);
    if (r.IsValid())
    {
      if (!r.IsIntersect(rect))
      {
        LOG(LERROR, (m_logTag, "FB center not in polygon's bound for:", id,
                    mercator::ToLatLon(rect), mercator::ToLatLon(r)));
      }
      rect.Add(r);
    }
  }

  return {std::move(fb), rect};
}

void PlaceProcessor::Add(FeatureBuilder && fb)
{
  // Get name as key to find place clusters.
  auto name = fb.GetName(StringUtf8Multilang::kDefaultCode);
  auto const id = fb.GetMostGenericOsmId();

  if (name.empty())
  {
    name = fb.GetName(StringUtf8Multilang::kEnglishCode);
    if (name.empty())
    {
      LOG(LWARNING, (m_logTag, "Place with empty name", id));
      return;
    }
  }

  // Naive name->key implementation. Probably should also make simplification, lower-case, etc ...
  std::string key(name);
  while (true)
  {
    size_t i = key.find("St. ");
    if (i == std::string::npos)
      break;
    key.replace(i, 4, "Saint ");
  }

  // Places are "equal candidates" if they have equal boundary index or equal name.
  m_nameToPlaces[{m_boundariesHolder.GetIndex(id), std::move(key)}].push_back(CreatePlace(std::move(fb)));
}

}  // namespace generator
