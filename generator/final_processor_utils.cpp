#include "generator/final_processor_utils.hpp"

#include "indexer/feature_data.hpp"

#include "base/stl_helpers.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <mutex>
#include <tuple>
#include <utility>

using namespace base::thread_pool::computational;
using namespace feature;

namespace generator
{
ProcessorCities::ProcessorCities(std::string const & temporaryMwmPath,
                                 AffiliationInterface const & affiliation,
                                 PlaceHelper & citiesHelper, size_t threadsCount)
  : m_temporaryMwmPath(temporaryMwmPath)
  , m_affiliation(affiliation)
  , m_citiesHelper(citiesHelper)
  , m_threadsCount(threadsCount)
{
}

void ProcessorCities::Process()
{
  std::mutex mutex;
  std::vector<FeatureBuilder> allCities;
  ForEachMwmTmp(m_temporaryMwmPath, [&](auto const & country, auto const & path)
  {
    if (!m_affiliation.HasCountryByName(country))
      return;

    std::vector<FeatureBuilder> cities;
    FeatureBuilderWriter<serialization_policy::MaxAccuracy> writer(path, true /* mangleName */);
    ForEachFeatureRawFormat<serialization_policy::MaxAccuracy>(path, [&](auto  && fb, auto /* pos */)
    {
      if (PlaceHelper::IsPlace(fb))
        cities.emplace_back(std::move(fb));
      else
        writer.Write(std::move(fb));
    });

    std::lock_guard<std::mutex> lock(mutex);
    std::move(std::begin(cities), std::end(cities), std::back_inserter(allCities));
  }, m_threadsCount);

  Order(allCities);
  for (auto const & city : allCities)
    m_citiesHelper.Process(city);

  AppendToMwmTmp(m_citiesHelper.GetFeatures(), m_affiliation, m_temporaryMwmPath, m_threadsCount);
}

PlaceHelper::PlaceHelper()
  : m_table(std::make_shared<OsmIdToBoundariesTable>())
  , m_processor(m_table)
{
}

PlaceHelper::PlaceHelper(std::string const & filename)
  : PlaceHelper()
{
  feature::ForEachFeatureRawFormat<feature::serialization_policy::MaxAccuracy>(
      filename, [&](auto const & fb, auto const &) { m_processor.Add(fb); });
}

// static
bool PlaceHelper::IsPlace(feature::FeatureBuilder const & fb)
{
  auto const type = GetPlaceType(fb);
  return type != ftype::GetEmptyValue() && !fb.GetName().empty() && NeedProcessPlace(fb);
}

bool PlaceHelper::Process(feature::FeatureBuilder const & fb)
{
  m_processor.Add(fb);
  return true;
}

std::vector<feature::FeatureBuilder> PlaceHelper::GetFeatures()
{
  return m_processor.ProcessPlaces();
}

std::shared_ptr<OsmIdToBoundariesTable> PlaceHelper::GetTable() const { return m_table; }

bool Less(FeatureBuilder const & lhs, FeatureBuilder const & rhs)
{
  auto const lGeomType = static_cast<int8_t>(lhs.GetGeomType());
  auto const rGeomType = static_cast<int8_t>(rhs.GetGeomType());

  auto const lId = lhs.HasOsmIds() ? lhs.GetMostGenericOsmId() : base::GeoObjectId();
  auto const rId = rhs.HasOsmIds() ? rhs.GetMostGenericOsmId() : base::GeoObjectId();

  auto const lPointsCount = lhs.GetPointsCount();
  auto const rPointsCount = rhs.GetPointsCount();

  auto const lKeyPoint = lhs.GetKeyPoint();
  auto const rKeyPoint = rhs.GetKeyPoint();

  return std::tie(lGeomType, lId, lPointsCount, lKeyPoint) <
         std::tie(rGeomType, rId, rPointsCount, rKeyPoint);
}

void Order(std::vector<FeatureBuilder> & fbs) { std::sort(std::begin(fbs), std::end(fbs), Less); }

void OrderTextFileByLine(std::string const & filename)
{
  std::ifstream istream;
  istream.exceptions(std::fstream::badbit);
  istream.open(filename);
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(istream, line))
    lines.emplace_back(std::move(line));

  std::sort(std::begin(lines), std::end(lines));
  istream.close();

  std::ofstream ostream;
  ostream.exceptions(std::fstream::failbit | std::fstream::badbit);
  ostream.open(filename);
  std::copy(std::begin(lines), std::end(lines), std::ostream_iterator<std::string>(ostream, "\n"));
}

std::vector<std::vector<std::string>> GetAffiliations(std::vector<FeatureBuilder> const & fbs,
                                                      AffiliationInterface const & affiliation,
                                                      size_t threadsCount)
{
  ThreadPool pool(threadsCount);
  std::vector<std::future<std::vector<std::string>>> futuresAffiliations;
  for (auto const & fb : fbs)
  {
    auto result = pool.Submit([&]() { return affiliation.GetAffiliations(fb); });
    futuresAffiliations.emplace_back(std::move(result));
  }

  std::vector<std::vector<std::string>> resultAffiliations;
  resultAffiliations.reserve(futuresAffiliations.size());
  for (auto & f : futuresAffiliations)
    resultAffiliations.emplace_back(f.get());

  return resultAffiliations;
}
}  // namespace generator
