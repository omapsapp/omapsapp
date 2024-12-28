#include "testing/testing.hpp"

#include "generator/affiliation.hpp"

#include "transit/world_feed/world_feed.hpp"

#include "platform/platform.hpp"

#include "base/assert.hpp"
#include "base/file_name_utils.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace
{
std::string const kFeedsSubdir = "test_data/world_feed_integration_tests_data";
}  // namespace

namespace transit
{
class WorldFeedIntegrationTests
{
public:
  WorldFeedIntegrationTests()
    : m_mwmMatcher(GetPlatform().ResourcesDir(), false /* haveBordersForWholeWorld */)
    , m_globalFeed(m_generator, m_generatorEdges, m_colorPicker, m_mwmMatcher)
  {
    m_testPath = base::JoinPath(GetPlatform().WritableDir(), kFeedsSubdir);
    CHECK(GetPlatform().IsFileExistsByFullPath(m_testPath), ());

    m_generator = IdGenerator(base::JoinPath(m_testPath, "mapping.txt"));
    m_generatorEdges = IdGenerator(base::JoinPath(m_testPath, "mapping_edges.txt"));
  }

  void ReadMinimalisticFeed()
  {
    gtfs::Feed feed(base::JoinPath(m_testPath, "minimalistic_feed"));
    TEST_EQUAL(feed.read_feed().code, gtfs::ResultCode::OK, ());
    TEST(m_globalFeed.SetFeed(std::move(feed)), ());

    TEST_EQUAL(m_globalFeed.m_networks.m_data.size(), 1, ());
    TEST_EQUAL(m_globalFeed.m_routes.m_data.size(), 2, ());
    // We check that lines with no entries in stop_times.txt are not added.
    TEST_EQUAL(m_globalFeed.m_lines.m_data.size(), 2, ());
    TEST_EQUAL(m_globalFeed.m_stops.m_data.size(), 7, ());
    TEST_EQUAL(m_globalFeed.m_shapes.m_data.size(), 2, ());
    // There are 2 lines with 3 and 4 stops correspondingly. So we have 5 edges.
    TEST_EQUAL(m_globalFeed.m_edges.m_data.size(), 5, ());
  }

  void ReadRealLifeFeed()
  {
    gtfs::Feed feed(base::JoinPath(m_testPath, "real_life_feed"));
    TEST_EQUAL(feed.read_feed().code, gtfs::ResultCode::OK, ());
    TEST(m_globalFeed.SetFeed(std::move(feed)), ());

    TEST_EQUAL(m_globalFeed.m_networks.m_data.size(), 21, ());
    TEST_EQUAL(m_globalFeed.m_routes.m_data.size(), 87, ());
    // All trips have unique service_id so each line corresponds to some trip.
    TEST_EQUAL(m_globalFeed.m_lines.m_data.size(), 392, ());
    TEST_EQUAL(m_globalFeed.m_stops.m_data.size(), 1008, ());
    // 64 shapes contained in other shapes should be skipped.
    TEST_EQUAL(m_globalFeed.m_shapes.m_data.size(), 329, ());
    TEST_EQUAL(m_globalFeed.m_gates.m_data.size(), 0, ());
    TEST_EQUAL(m_globalFeed.m_transfers.m_data.size(), 0, ());
    TEST_EQUAL(m_globalFeed.m_edges.m_data.size(), 3999, ());
    TEST_EQUAL(m_globalFeed.m_edgesTransfers.m_data.size(), 0, ());
  }

  void ReadFeedWithMultipleShapeProjections()
  {
    gtfs::Feed feed(base::JoinPath(m_testPath, "feed_with_multiple_shape_projections"));
    TEST_EQUAL(feed.read_feed().code, gtfs::ResultCode::OK, ());
    TEST(m_globalFeed.SetFeed(std::move(feed)), ());

    TEST_EQUAL(m_globalFeed.m_networks.m_data.size(), 1, ());
    TEST_EQUAL(m_globalFeed.m_routes.m_data.size(), 1, ());
    TEST_EQUAL(m_globalFeed.m_lines.m_data.size(), 2, ());
    TEST_EQUAL(m_globalFeed.m_stops.m_data.size(), 16, ());
    TEST_EQUAL(m_globalFeed.m_shapes.m_data.size(), 1, ());
    TEST_EQUAL(m_globalFeed.m_gates.m_data.size(), 0, ());
    TEST_EQUAL(m_globalFeed.m_transfers.m_data.size(), 2, ());
    TEST_EQUAL(m_globalFeed.m_edges.m_data.size(), 27, ());
    TEST_EQUAL(m_globalFeed.m_edgesTransfers.m_data.size(), 2, ());
  }

  void ReadFeedWithWrongStopsOrder()
  {
    gtfs::Feed feed(base::JoinPath(m_testPath, "feed_with_wrong_stops_order"));
    TEST_EQUAL(feed.read_feed().code, gtfs::ResultCode::OK, ());
    // Feed has wrong stops order (impossible for trip shape) and should be rejected.
    TEST(!m_globalFeed.SetFeed(std::move(feed)), ());
  }

  void ReadFeedWithBackwardOrder()
  {
    gtfs::Feed feed(base::JoinPath(m_testPath, "feed_with_backward_order"));
    TEST_EQUAL(feed.read_feed().code, gtfs::ResultCode::OK, ());
    TEST(m_globalFeed.SetFeed(std::move(feed)), ());

    TEST_EQUAL(m_globalFeed.m_networks.m_data.size(), 1, ());
    TEST_EQUAL(m_globalFeed.m_routes.m_data.size(), 1, ());
    TEST_EQUAL(m_globalFeed.m_lines.m_data.size(), 1, ());
    TEST_EQUAL(m_globalFeed.m_stops.m_data.size(), 16, ());
    TEST_EQUAL(m_globalFeed.m_shapes.m_data.size(), 1, ());
    TEST_EQUAL(m_globalFeed.m_gates.m_data.size(), 0, ());
    TEST_EQUAL(m_globalFeed.m_transfers.m_data.size(), 2, ());
    TEST_EQUAL(m_globalFeed.m_edges.m_data.size(), 16, ());
    TEST_EQUAL(m_globalFeed.m_edgesTransfers.m_data.size(), 2, ());
  }

  // Test for train itinerary that passes through 4 regions in Europe and consists of 4 stops
  // (each in separate mwm) and 1 route with 1 line. This line passes through 4 stops:
  // [1] Switzerland_Ticino -> [2] Switzerland_Eastern ->
  // [3] Italy_Lombardy_Como -> [4] Italy_Lombardy_Monza and Brianza
  void SplitFeedIntoMultipleRegions()
  {
    gtfs::Feed feed(base::JoinPath(m_testPath, "feed_long_itinerary"));
    TEST_EQUAL(feed.read_feed().code, gtfs::ResultCode::OK, ());
    TEST(m_globalFeed.SetFeed(std::move(feed)), ());

    TEST_EQUAL(m_globalFeed.m_lines.m_data.size(), 1, ());
    TEST_EQUAL(m_globalFeed.m_stops.m_data.size(), 4, ());
    TEST_EQUAL(m_globalFeed.m_edges.m_data.size(), 3, ());

    m_globalFeed.SplitFeedIntoRegions();

    size_t const mwmCount = 4;
    // We check that count of keys in each regions-to-ids mapping corresponds to the mwms count.
    TEST_EQUAL(m_globalFeed.m_splitting.m_networks.size(), mwmCount, ());
    TEST_EQUAL(m_globalFeed.m_splitting.m_routes.size(), mwmCount, ());
    TEST_EQUAL(m_globalFeed.m_splitting.m_lines.size(), mwmCount, ());
    TEST_EQUAL(m_globalFeed.m_splitting.m_stops.size(), mwmCount, ());

    auto & stopsInRegions = m_globalFeed.m_splitting.m_stops;
    auto & edgesInRegions = m_globalFeed.m_splitting.m_edges;

    // First and last stops are connected through 1 edge with 1 nearest stop.
    // Stops in the middle are connected through 2 edges with 2 nearest stops.
    TEST_EQUAL(stopsInRegions["Switzerland_Ticino"].size(), 3, ());
    TEST_EQUAL(edgesInRegions["Switzerland_Ticino"].size(), 1, ());

    TEST_EQUAL(stopsInRegions["Switzerland_Eastern"].size(), 4, ());
    TEST_EQUAL(edgesInRegions["Switzerland_Eastern"].size(), 2, ());

    TEST_EQUAL(stopsInRegions["Italy_Lombardy_Como"].size(), 4, ());
    TEST_EQUAL(edgesInRegions["Italy_Lombardy_Como"].size(), 2, ());

    TEST_EQUAL(stopsInRegions["Italy_Lombardy_Monza and Brianza"].size(), 3, ());
    TEST_EQUAL(edgesInRegions["Italy_Lombardy_Monza and Brianza"].size(), 1, ());
  }

private:
  std::string m_testPath;
  IdGenerator m_generator;
  IdGenerator m_generatorEdges;
  transit::ColorPicker m_colorPicker;
  feature::CountriesFilesAffiliation m_mwmMatcher;
  WorldFeed m_globalFeed;
};

UNIT_CLASS_TEST(WorldFeedIntegrationTests, MinimalisticFeed)
{
  ReadMinimalisticFeed();
}

UNIT_CLASS_TEST(WorldFeedIntegrationTests, RealLifeFeed)
{
  ReadRealLifeFeed();
}

UNIT_CLASS_TEST(WorldFeedIntegrationTests, FeedWithLongItinerary)
{
  SplitFeedIntoMultipleRegions();
}

UNIT_CLASS_TEST(WorldFeedIntegrationTests, FeedWithMultipleShapeProjections)
{
  ReadFeedWithMultipleShapeProjections();
}

UNIT_CLASS_TEST(WorldFeedIntegrationTests, FeedWithWrongStopsOrder)
{
  ReadFeedWithWrongStopsOrder();
}

UNIT_CLASS_TEST(WorldFeedIntegrationTests, FeedWithBackwardOrder)
{
  ReadFeedWithBackwardOrder();
}
}  // namespace transit
