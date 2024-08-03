#include "generator/utils.hpp"

#include "search/search_quality/helpers.hpp"

#include "search/categories_cache.hpp"
#include "search/engine.hpp"
#include "search/locality_finder.hpp"
#include "search/reverse_geocoder.hpp"

#include "storage/country_info_getter.hpp"
#include "storage/storage.hpp"
#include "storage/storage_defines.hpp"

#include "indexer/classificator.hpp"
#include "indexer/classificator_loader.hpp"
#include "indexer/data_source.hpp"
#include "indexer/feature.hpp"
#include "indexer/ftypes_matcher.hpp"
#include "indexer/map_object.hpp"
#include "indexer/map_style_reader.hpp"

#include "platform/platform_tests_support/helpers.hpp"

#include "platform/local_country_file_utils.hpp"
#include "platform/platform.hpp"

#include "geometry/mercator.hpp"
#include "geometry/point2d.hpp"

#include "base/file_name_utils.hpp"
#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <vector>

using namespace std;

class ClosestPoint
{
  m2::PointD const & m_center;
  m2::PointD m_best;
  double m_distance = numeric_limits<double>::max();

public:
  explicit ClosestPoint(m2::PointD const & center) : m_center(center), m_best(0, 0) {}

  m2::PointD GetBest() const { return m_best; }

  void operator()(m2::PointD const & point)
  {
    double distance = m_center.SquaredLength(point);
    if (distance < m_distance)
    {
      m_distance = distance;
      m_best = point;
    }
  }
};

m2::PointD FindCenter(FeatureType & f)
{
  ClosestPoint closest(f.GetLimitRect(FeatureType::BEST_GEOMETRY).Center());
  if (f.GetGeomType() == feature::GeomType::Area)
  {
    f.ForEachTriangle([&closest](m2::PointD const & p1, m2::PointD const & p2,
                                 m2::PointD const & p3) { closest((p1 + p2 + p3) / 3); },
                      FeatureType::BEST_GEOMETRY);
  }
  else
  {
    f.ForEachPoint(closest, FeatureType::BEST_GEOMETRY);
  }
  return closest.GetBest();
}

size_t const kLangCount = StringUtf8Multilang::GetSupportedLanguages().size();

string GetReadableType(FeatureType & f)
{
  string result;
  f.ForEachType([&](uint32_t type)
  {
    if (ftypes::IsPoiChecker::Instance()(type) || ftypes::IsPlaceChecker::Instance()(type))
      result = classif().GetReadableObjectName(type);
  });
  return result;
}

string GetWheelchairType(FeatureType & f)
{
  static const uint32_t wheelchair = classif().GetTypeByPath({"wheelchair"});
  string result;
  f.ForEachType([&result](uint32_t type) {
    uint32_t truncated = type;
    ftype::TruncValue(truncated, 1);
    if (truncated == wheelchair)
    {
      string fullName = classif().GetReadableObjectName(type);
      auto pos = fullName.find("-");
      if (pos != string::npos)
        result = fullName.substr(pos + 1);
    }
  });
  return result;
}

bool HasAtm(FeatureType & f)
{
  static const uint32_t atm = classif().GetTypeByPath({"amenity", "atm"});
  bool result = false;
  f.ForEachType([&result](uint32_t type) {
    if (type == atm)
      result = true;
  });
  return result;
}

string BuildUniqueId(ms::LatLon const & coords, string const & name)
{
  ostringstream ss;
  ss << strings::to_string_dac(coords.m_lat, 6) << ','
     << strings::to_string_dac(coords.m_lon, 6) << ',' << name;
  uint32_t hash = 0;
  for (char const c : ss.str())
    hash = hash * 101 + c;
  return strings::to_string(hash);
}

void AppendNames(FeatureType & f, vector<string> & columns)
{
  vector<string> names(kLangCount);
  f.GetNames().ForEach([&names](int8_t code, string_view name) { names[code] = name; });
  columns.insert(columns.end(), next(names.begin()), names.end());
}

void PrintAsCSV(vector<string> const & columns, char const delimiter, ostream & out)
{
  bool first = true;
  for (string value : columns)
  {
    // Newlines are hard to process, replace them with spaces. And trim the
    // string.
    replace(value.begin(), value.end(), '\r', ' ');
    replace(value.begin(), value.end(), '\n', ' ');
    strings::Trim(value);

    if (first)
      first = false;
    else
      out << delimiter;
    bool needsQuotes = value.find('"') != string::npos || value.find(delimiter) != string::npos;
    if (!needsQuotes)
    {
      out << value;
    }
    else
    {
      size_t pos = 0;
      while ((pos = value.find('"', pos)) != string::npos)
      {
        value.insert(pos, 1, '"');
        pos += 2;
      }
      out << '"' << value << '"';
    }
  }
  out << endl;
}

class Processor
{
  search::ReverseGeocoder m_geocoder;
  base::Cancellable m_cancellable;
  search::CitiesBoundariesTable m_boundariesTable;
  search::VillagesCache m_villagesCache;
  search::LocalityFinder m_finder;

public:
  explicit Processor(DataSource const & dataSource)
    : m_geocoder(dataSource)
    , m_boundariesTable(dataSource)
    , m_villagesCache(m_cancellable)
    , m_finder(dataSource, m_boundariesTable, m_villagesCache)
  {
    m_boundariesTable.Load();
  }

  void ClearCache() { m_villagesCache.Clear(); }

  void operator()(FeatureType & f, map<uint32_t, base::GeoObjectId> const & ft2osm)
  {
    Process(f, ft2osm);
  }

  void Process(FeatureType & f, map<uint32_t, base::GeoObjectId> const & ft2osm)
  {
    f.ParseHeader2();
    string const & category = GetReadableType(f);
    auto const & meta = f.GetMetadata();

    auto const metaOperator = meta.Get(feature::Metadata::FMD_OPERATOR);
    auto const & osmIt = ft2osm.find(f.GetID().m_index);
    if ((!f.HasName() && metaOperator.empty()) ||
        (f.GetGeomType() == feature::GeomType::Line && category != "highway-pedestrian") ||
        category.empty())
    {
      return;
    }
    m2::PointD const & center = FindCenter(f);
    ms::LatLon const & ll = mercator::ToLatLon(center);
    osm::MapObject obj;
    obj.SetFromFeatureType(f);

    string_view city;
    m_finder.GetLocality(center, [&city](search::LocalityItem const & item)
    {
      item.GetSpecifiedOrDefaultName(StringUtf8Multilang::kDefaultCode, city);
    });

    string const & mwmName = f.GetID().GetMwmName();

    string name(f.GetName(StringUtf8Multilang::kDefaultCode));
    if (name.empty())
    {
      name = f.GetReadableName();
      if (name.empty())
        name = metaOperator;
    }

    string osmId = osmIt != ft2osm.cend() ? to_string(osmIt->second.GetEncodedId()) : "";
    string const & uid = BuildUniqueId(ll, name);
    string const & lat = strings::to_string_dac(ll.m_lat, 6);
    string const & lon = strings::to_string_dac(ll.m_lon, 6);
    search::ReverseGeocoder::Address addr;
    string addrStreet = "";
    string addrHouse = "";
    double constexpr kDistanceThresholdMeters = 0.5;
    if (m_geocoder.GetExactAddress(f, addr))
    {
      addrStreet = addr.GetStreetName();
      addrHouse = addr.GetHouseNumber();
    }
    else
    {
      m_geocoder.GetNearbyAddress(center, addr);
      if (addr.GetDistance() < kDistanceThresholdMeters)
      {
        addrStreet = addr.GetStreetName();
        addrHouse = addr.GetHouseNumber();
      }
    }
    string const phone(meta.Get(feature::Metadata::FMD_PHONE_NUMBER));
    string const website(meta.Get(feature::Metadata::FMD_WEBSITE));
    string const contact_facebook(meta.Get(feature::Metadata::FMD_CONTACT_FACEBOOK));
    string const contact_instagram(meta.Get(feature::Metadata::FMD_CONTACT_INSTAGRAM));
    string const contact_twitter(meta.Get(feature::Metadata::FMD_CONTACT_TWITTER));
    string const contact_vk(meta.Get(feature::Metadata::FMD_CONTACT_VK));
    string const contact_line(meta.Get(feature::Metadata::FMD_CONTACT_LINE));
    string const stars(meta.Get(feature::Metadata::FMD_STARS));
    string const internet(meta.Get(feature::Metadata::FMD_INTERNET));
    string const denomination(meta.Get(feature::Metadata::FMD_DENOMINATION));
    string const wheelchair(GetWheelchairType(f));
    string const opening_hours(meta.Get(feature::Metadata::FMD_OPEN_HOURS));
    string const collection_times(meta.Get(feature::Metadata::FMD_COLLECTION_TIMES));
    string const wikipedia(meta.Get(feature::Metadata::FMD_WIKIPEDIA));
    string const wikimedia_commons(meta.Get(feature::Metadata::FMD_WIKIMEDIA_COMMONS));
    string const floor(meta.Get(feature::Metadata::FMD_LEVEL));
    string const fee = category.ends_with("-fee") ? "yes" : "";
    string const atm = HasAtm(f) ? "yes" : "";

    vector<string> columns = {
        osmId,             uid,             lat,           lon,       mwmName, category,     name,    std::string(city),
        addrStreet,        addrHouse,       phone,         website,   stars,   std::string(metaOperator), internet,
        denomination,      wheelchair,      opening_hours, wikipedia, floor,   fee,          atm,     contact_facebook,
        contact_instagram, contact_twitter, contact_vk,    contact_line, wikimedia_commons, collection_times};

    AppendNames(f, columns);
    PrintAsCSV(columns, ';', cout);
  }
};

void PrintHeader()
{
  vector<string> columns = {"id",              "old_id",       "lat",        "lon",              "mwm",
                            "category",        "name",         "city",       "street",           "house",
                            "phone",           "website",      "cuisines",   "stars",            "operator",
                            "internet",        "denomination", "wheelchair", "opening_hours",    "wikipedia",
                            "floor",           "fee",          "atm",        "contact_facebook", "contact_instagram",
                            "contact_twitter", "contact_vk",   "contact_line", "wikimedia_commons", "collection_times"};
  // Append all supported name languages in order.
  for (uint8_t idx = 1; idx < kLangCount; idx++)
    columns.push_back("name_" + string(StringUtf8Multilang::GetLangByCode(idx)));
  PrintAsCSV(columns, ';', cout);
}

bool ParseFeatureIdToOsmIdMapping(string const & path, map<uint32_t, base::GeoObjectId> & mapping)
{
  return generator::ForEachOsmId2FeatureId(
      path, [&](auto const & compositeId, uint32_t const featureId) {
        mapping[featureId] = compositeId.m_mainId;
      });
}

void DidDownload(storage::CountryId const & /* countryId */,
                 shared_ptr<platform::LocalCountryFile> const & /* localFile */)
{
}

bool WillDelete(storage::CountryId const & /* countryId */,
                shared_ptr<platform::LocalCountryFile> const & /* localFile */)
{
  return false;
}

int main(int argc, char ** argv)
{
  platform::tests_support::ChangeMaxNumberOfOpenFiles(search::search_quality::kMaxOpenFiles);
  if (argc <= 1)
  {
    LOG(LERROR, ("Usage:", argc == 1 ? argv[0] : "feature_list",
                 "<mwm_path> [<data_path>] [<mwm_prefix>]"));
    return 1;
  }

  Platform & pl = GetPlatform();
  pl.SetWritableDirForTests(argv[1]);

  string countriesFile = COUNTRIES_FILE;
  if (argc > 2)
  {
    pl.SetResourceDir(argv[2]);
    countriesFile = base::JoinPath(argv[2], COUNTRIES_FILE);
  }

  storage::Storage storage(countriesFile, argv[1]);
  storage.Init(&DidDownload, &WillDelete);
  auto infoGetter = storage::CountryInfoReader::CreateCountryInfoGetter(pl);
  infoGetter->SetAffiliations(storage.GetAffiliations());

  GetStyleReader().SetCurrentStyle(MapStyleMerged);
  classificator::Load();

  FrozenDataSource dataSource;
  vector<platform::LocalCountryFile> mwms;
  platform::FindAllLocalMapsAndCleanup(numeric_limits<int64_t>::max() /* the latest version */,
                                       mwms);
  for (auto & mwm : mwms)
  {
    mwm.SyncWithDisk();
    auto const & p = dataSource.RegisterMap(mwm);
    CHECK_EQUAL(MwmSet::RegResult::Success, p.second, ("Could not register map", mwm));
    MwmSet::MwmId const & id = p.first;
    CHECK(id.IsAlive(), ("Mwm is not alive?", mwm));
  }

  Processor doProcess(dataSource);
  PrintHeader();
  vector<shared_ptr<MwmInfo>> mwmInfos;
  dataSource.GetMwmsInfo(mwmInfos);
  for (auto const & mwmInfo : mwmInfos)
  {
    if (mwmInfo->GetType() != MwmInfo::COUNTRY)
      continue;
    if (argc > 3 && !(mwmInfo->GetCountryName() + DATA_FILE_EXTENSION).starts_with(argv[3]))
      continue;
    LOG(LINFO, ("Processing", mwmInfo->GetCountryName()));
    string osmToFeatureFile = base::JoinPath(
        argv[1], mwmInfo->GetCountryName() + DATA_FILE_EXTENSION + OSM2FEATURE_FILE_EXTENSION);
    map<uint32_t, base::GeoObjectId> featureIdToOsmId;
    ParseFeatureIdToOsmIdMapping(osmToFeatureFile, featureIdToOsmId);
    MwmSet::MwmId mwmId(mwmInfo);
    FeaturesLoaderGuard loader(dataSource, mwmId);
    for (uint32_t ftIndex = 0; ftIndex < loader.GetNumFeatures(); ftIndex++)
    {
      if (auto ft = loader.GetFeatureByIndex(static_cast<uint32_t>(ftIndex)))
        doProcess.Process(*ft, featureIdToOsmId);
    }
    doProcess.ClearCache();
  }

  return 0;
}
