#include "testing/testing.hpp"

#include "generator/node_mixer.hpp"
#include "generator/osm_element.hpp"

#include <sstream>

UNIT_TEST(NodeMixerTests)
{
  std::istringstream stream1("");
  generator::MixFakeNodes(stream1, [](OsmElement &) {
    TEST(false, ("Returned an object for an empty input stream."));
  });

  std::istringstream stream2("shop=gift\nname=Shop\n");
  generator::MixFakeNodes(stream2, [](OsmElement &) {
    TEST(false, ("Returned an object for a source without coordinates."));
  });

  std::istringstream stream3("lat=4.0\nlon=-4.1\n");
  generator::MixFakeNodes(stream3, [](OsmElement &) {
    TEST(false, ("Returned an object for a source without tags."));
  });

  std::istringstream stream4("lat=10.0\nlon=-4.8\nshop=gift\nname=Shop");
  int count4 = 0;
  generator::MixFakeNodes(stream4, [&](OsmElement & p) {
    count4++;
    TEST_EQUAL(p.m_type, OsmElement::EntityType::Node, ());
    TEST_EQUAL(p.m_lat, 10.0, ());
    TEST_EQUAL(p.m_lon, -4.8, ());
    TEST_EQUAL(p.Tags().size(), 2, ());
    TEST_EQUAL(p.GetTag("name"), "Shop", ());
  });
  TEST_EQUAL(count4, 1, ());

  std::istringstream stream5("lat=10.0\nlon=-4.8\nid=1\nname=First\n\nid=2\nlat=60\nlon=1\nname=Second\n\n\n");
  int count5 = 0;
  generator::MixFakeNodes(stream5, [&](OsmElement & p) {
    count5++;
    TEST_EQUAL(p.m_type, OsmElement::EntityType::Node, ());
    TEST_EQUAL(p.Tags().size(), 2, ());
    std::string id = p.GetTag("id");
    TEST(!id.empty(), ("No id tag when every object has it."));
    TEST_EQUAL(p.GetTag("name"), id == "1" ? "First" : "Second", ());
  });
  TEST_EQUAL(count5, 2, ());

  std::istringstream stream6("lat=0\nlon=-4.8\nshop=mall");
  int count6 = 0;
  generator::MixFakeNodes(stream6, [&](OsmElement & p) {
    count6++;
    TEST_EQUAL(p.m_lat, 0.0, ());
    TEST_EQUAL(p.m_lon, -4.8, ());
  });
  TEST_EQUAL(count6, 1, ());
}
