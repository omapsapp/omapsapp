#include "map_object.hpp"

#include "indexer/feature.hpp"
#include "indexer/feature_algo.hpp"
#include "indexer/ftypes_matcher.hpp"
#include "indexer/road_shields_parser.hpp"

#include "geometry/mercator.hpp"

#include "platform/localization.hpp"
#include "platform/measurement_utils.hpp"
#include "platform/distance.hpp"

#include "base/logging.hpp"
#include "base/string_utils.hpp"

namespace osm
{
using namespace std;

void MapObject::SetFromFeatureType(FeatureType & ft)
{
  m_mercator = feature::GetCenter(ft);
  m_name = ft.GetNames();

  Classificator const & cl = classif();
  m_types = feature::TypesHolder(ft);
  m_types.RemoveIf([&cl](uint32_t t)
  {
    return !cl.IsTypeValid(t);
  });
  // Actually, we can't select object on map with invalid (non-drawable or deprecated) type.
  // TODO: in android prod a user will see an "empty" PP if a spot is selected in old mwm
  // where a deprecated feature was; and could crash if play with routing to it, bookmarking it..
  // A desktop/qt prod segfaults when trying to select such spots.
  ASSERT(!m_types.Empty(), ());

  m_metadata = ft.GetMetadata();
  m_houseNumber = ft.GetHouseNumber();
  m_roadShields = ftypes::GetRoadShieldsNames(ft);
  m_featureID = ft.GetID();
  m_geomType = ft.GetGeomType();
  m_layer = ft.GetLayer();

  // TODO: BEST_GEOMETRY is likely needed for some special cases only,
  // i.e. matching an edited OSM feature, in other cases like opening
  // a place page WORST_GEOMETRY is going to be enough?
  if (m_geomType == feature::GeomType::Area)
    assign_range(m_triangles, ft.GetTrianglesAsPoints(FeatureType::BEST_GEOMETRY));
  else if (m_geomType == feature::GeomType::Line)
    assign_range(m_points, ft.GetPoints(FeatureType::BEST_GEOMETRY));
    
  // Fill runtime metadata
  m_metadata.Set(feature::Metadata::EType::FMD_WHEELCHAIR, feature::GetReadableWheelchairType(m_types));

#ifdef DEBUG
  if (ftypes::IsWifiChecker::Instance()(ft))
    ASSERT(m_metadata.Has(MetadataID::FMD_INTERNET), ());
#endif
}

FeatureID const & MapObject::GetID() const { return m_featureID; }
ms::LatLon MapObject::GetLatLon() const { return mercator::ToLatLon(m_mercator); }
m2::PointD const & MapObject::GetMercator() const { return m_mercator; }
vector<m2::PointD> const & MapObject::GetTriangesAsPoints() const { return m_triangles; }
vector<m2::PointD> const & MapObject::GetPoints() const { return m_points; }
feature::TypesHolder const & MapObject::GetTypes() const { return m_types; }

string_view MapObject::GetDefaultName() const
{
  string_view name;
  UNUSED_VALUE(m_name.GetString(StringUtf8Multilang::kDefaultCode, name));
  return name;
}

StringUtf8Multilang const & MapObject::GetNameMultilang() const
{
  return m_name;
}

string const & MapObject::GetHouseNumber() const { return m_houseNumber; }

std::string_view MapObject::GetPostcode() const
{
  return m_metadata.Get(MetadataID::FMD_POSTCODE);
}

std::string MapObject::GetLocalizedType() const
{
  ASSERT(!m_types.Empty(), ());
  feature::TypesHolder copy(m_types);
  copy.SortBySpec();

  return platform::GetLocalizedTypeName(classif().GetReadableObjectName(copy.GetBestType()));
}

std::string MapObject::GetLocalizedAllTypes(bool withMainType) const
{
  ASSERT(!m_types.Empty(), ());
  feature::TypesHolder copy(m_types);
  copy.SortBySpec();

  auto const & isPoi = ftypes::IsPoiChecker::Instance();
  auto const & amenityChecker = ftypes::IsAmenityChecker::Instance();

  std::ostringstream oss;
  bool isMainType = true;
  bool isFirst = true;
  for (auto const type : copy)
  {
    if (isMainType && !withMainType)
    {
      isMainType = false;
      continue;
    }

    // Ignore types that are not POI
    if (!isMainType && !isPoi(type))
      continue;
      
    // Ignore general amenity
    if (!isMainType && amenityChecker.GetType() == type)
      continue;
      
    isMainType = false;
    
    // Add fields separator between types
    if (isFirst)
      isFirst = false;
    else
      oss << feature::kFieldsSeparator;
    
    oss << platform::GetLocalizedTypeName(classif().GetReadableObjectName(type));
  }
  
  return oss.str();
}

std::string_view MapObject::GetMetadata(MetadataID type) const
{
  return m_metadata.Get(type);
}

std::string_view MapObject::GetOpeningHours() const
{
  return m_metadata.Get(MetadataID::FMD_OPEN_HOURS);
}

std::string_view MapObject::GetCollectionTimes() const
{
  return m_metadata.Get(MetadataID::FMD_COLLECTION_TIMES);
}

feature::Internet MapObject::GetInternet() const
{
  return feature::InternetFromString(m_metadata.Get(MetadataID::FMD_INTERNET));
}

vector<string> MapObject::GetCuisines() const { return feature::GetCuisines(m_types); }

vector<string> MapObject::GetLocalizedCuisines() const
{
  return feature::GetLocalizedCuisines(m_types);
}

vector<string> MapObject::GetRecyclingTypes() const { return feature::GetRecyclingTypes(m_types); }

vector<string> MapObject::GetLocalizedRecyclingTypes() const
{
  return feature::GetLocalizedRecyclingTypes(m_types);
}

string MapObject::GetLocalizedFeeType() const
{
  return feature::GetLocalizedFeeType(m_types);
}

bool MapObject::HasAtm() const
{
  return feature::HasAtm(m_types);
}

bool MapObject::HasToilets() const
{
  return feature::HasToilets(m_types);
}

string MapObject::FormatCuisines() const
{
  return strings::JoinStrings(GetLocalizedCuisines(), feature::kFieldsSeparator);
}

string MapObject::FormatRoadShields() const
{
  return strings::JoinStrings(m_roadShields, feature::kFieldsSeparator);
}

int MapObject::GetStars() const
{
  uint8_t count = 0;

  auto const sv = m_metadata.Get(MetadataID::FMD_STARS);
  if (!sv.empty())
  {
    if (!strings::to_uint(sv, count))
      count = 0;
  }

  return count;
}

bool MapObject::IsPointType() const { return m_geomType == feature::GeomType::Point; }
bool MapObject::IsBuilding() const { return ftypes::IsBuildingChecker::Instance()(m_types); }
bool MapObject::IsPublicTransportStop() const { return ftypes::IsPublicTransportStopChecker::Instance()(m_types); }

}  // namespace osm
