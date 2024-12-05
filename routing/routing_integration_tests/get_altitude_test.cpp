#include "testing/testing.hpp"

#include "routing/routing_integration_tests/routing_test_tools.hpp"

#include "indexer/altitude_loader.hpp"
#include "indexer/classificator.hpp"
#include "indexer/classificator_loader.hpp"
#include "indexer/data_source.hpp"
#include "indexer/feature_altitude.hpp"
#include "indexer/feature_data.hpp"
#include "indexer/feature_processor.hpp"

#include "routing/routing_helpers.hpp"

#include "geometry/mercator.hpp"
#include "geometry/point_with_altitude.hpp"

#include "platform/local_country_file.hpp"

#include "base/math.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace get_altitude_tests
{
using namespace feature;
using namespace platform;
using namespace std;

void TestAltitudeOfAllMwmFeatures(string const & countryId,
                                  geometry::Altitude const altitudeLowerBoundMeters,
                                  geometry::Altitude const altitudeUpperBoundMeters)
{
  FrozenDataSource dataSource;

  LocalCountryFile const country = integration::GetLocalCountryFileByCountryId(CountryFile(countryId));
  TEST_NOT_EQUAL(country, LocalCountryFile(), ());
  TEST(country.HasFiles(), (country));

  pair<MwmSet::MwmId, MwmSet::RegResult> const res = dataSource.RegisterMap(country);
  TEST_EQUAL(res.second, MwmSet::RegResult::Success, ());
  auto const handle = dataSource.GetMwmHandleById(res.first);
  TEST(handle.IsAlive(), ());

  auto altitudeLoader = make_unique<AltitudeLoaderCached>(*handle.GetValue());

  ForEachFeature(country.GetPath(MapFileType::Map), [&](FeatureType & f, uint32_t const & id)
  {
    if (!routing::IsRoad(TypesHolder(f)))
      return;

    f.ParseGeometry(FeatureType::BEST_GEOMETRY);
    size_t const pointsCount = f.GetPointsCount();
    if (pointsCount == 0)
      return;

    geometry::Altitudes const & altitudes = altitudeLoader->GetAltitudes(id, pointsCount);
    TEST(!altitudes.empty(),
         ("Empty altitude vector. MWM:", countryId, ", feature id:", id, ", altitudes:", altitudes));

    for (auto const altitude : altitudes)
    {
      TEST_EQUAL(math::Clamp(altitude, altitudeLowerBoundMeters, altitudeUpperBoundMeters), altitude,
                 ("Unexpected altitude. MWM:", countryId, ", feature id:", id, ", altitudes:", altitudes));
    }
  });
}

UNIT_TEST(AllMwmFeaturesGetAltitudeTest)
{
  classificator::Load();

  TestAltitudeOfAllMwmFeatures("Russia_Moscow", 50 /* altitudeLowerBoundMeters */,
                               300 /* altitudeUpperBoundMeters */);
  TestAltitudeOfAllMwmFeatures("Nepal_Kathmandu", 250 /* altitudeLowerBoundMeters */,
                               6000 /* altitudeUpperBoundMeters */);
  TestAltitudeOfAllMwmFeatures("Netherlands_North Holland_Amsterdam", -25 /* altitudeLowerBoundMeters */,
                               50 /* altitudeUpperBoundMeters */);
}
}  // namespace get_altitude_tests
