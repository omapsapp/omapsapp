#include "testing/testing.hpp"

#include "generator/osm_o5m_source.hpp"

#include <iterator>
#include <set>
#include <utility>
#include <vector>

#include "source_data.hpp"

namespace osm_o5m_source_test
{
using std::begin, std::end, std::pair, std::string, std::vector;

UNIT_TEST(OSM_O5M_Source_Node_read_test)
{
  string data(begin(node2_o5m_data), end(node2_o5m_data));
  std::stringstream ss(data);

  osm::O5MSource dataset([&ss](uint8_t * buffer, size_t size)
  {
    return ss.read(reinterpret_cast<char *>(buffer), size).gcount();
  }, 10 /* buffer size */);

  osm::O5MSource::Iterator it = dataset.begin();
  osm::O5MSource::Entity const & em = *it;

  TEST_EQUAL(em.id, 513709898, ());
  TEST_EQUAL(em.user, string("Xmypblu"), ());
  TEST_EQUAL(em.uid, 395071, ());
  TEST_EQUAL(em.version, 8, ());
  TEST_EQUAL(em.changeset, 12059128, ());
  TEST(AlmostEqualAbs(em.lon, 38.7666704, 1e-7), ());
  TEST(AlmostEqualAbs(em.lat, 55.0927062, 1e-7), ());

  auto const tags = em.Tags();
  auto tagIterator = tags.begin();
  auto const & tag = *tagIterator;
  TEST_EQUAL(tag.key, string("amenity"), ());
  TEST_EQUAL(tag.value, string("cinema"), ());
  ++tagIterator;
  TEST_EQUAL(tag.key, string("name"), ());
  TEST_EQUAL(tag.value, string("КТ Горизонт"), ());
  ++tagIterator;
  TEST(!(tagIterator != tags.end()), ());
}

UNIT_TEST(OSM_O5M_Source_Way_read_test)
{
  string data(begin(way_o5m_data), end(way_o5m_data));
  std::stringstream ss(data);

  osm::O5MSource dataset([&ss](uint8_t * buffer, size_t size)
  {
    return ss.read(reinterpret_cast<char *>(buffer), size).gcount();
  }, 10 /* buffer size */);

  std::set<int64_t> nodes;

  vector<pair<string, string>> const validTags = {
      {"name", "Yukon River"}, {"name:ru", "Юкон"}, {"waterway", "river"}};

  for (auto const & em : dataset)
  {
    switch (em.type)
    {
      case osm::O5MSource::EntityType::Node:
      {
        nodes.insert(em.id);
        for (auto const & tag : em.Tags())
        {
          TEST(false, ("Unexpected tag:", tag.key, tag.value));
        }
        break;
      }
      case osm::O5MSource::EntityType::Way:
      {
        size_t ndCounter = 0;
        size_t tagCounter = 0;
        for (auto const & nd : em.Nodes())
        {
          ndCounter++;
          TEST(nodes.count(nd), ());
        }
        TEST_EQUAL(nodes.size(), ndCounter, ());
        for (auto const & tag : em.Tags())
        {
          TEST_EQUAL(tag.key, validTags[tagCounter].first, ());
          TEST_EQUAL(tag.value, validTags[tagCounter].second, ());
          tagCounter++;
        }
        TEST_EQUAL(validTags.size(), tagCounter, ());
        break;
      }
      default:
        break;
    }
  }
}

UNIT_TEST(OSM_O5M_Source_Relation_read_test)
{
  string data(begin(relation_o5m_data), end(relation_o5m_data));
  std::stringstream ss(data);

  osm::O5MSource dataset([&ss](uint8_t * buffer, size_t size)
  {
    return ss.read(reinterpret_cast<char *>(buffer), size).gcount();
  }, 10 /* buffer size */);

  std::set<int64_t> nodes;
  std::set<int64_t> entities;

  vector<pair<string, string>> const validNodeTags = {
    {"name", "Whitehorse"}
    , {"place", "town"}};

  vector<pair<string, string>> const validRelationTags = {
      {"name", "Whitehorse"}
    , {"place", "town"}
    , {"type", "multipolygon"}};

  using TType = osm::O5MSource::EntityType;
  vector<pair<TType, string>> const relationMembers = {
      {TType::Way, "outer"}
    , {TType::Node, ""}};

  for (auto const & em : dataset)
  {
    entities.insert(em.id);

    switch (em.type)
    {
      case TType::Node:
      {
        nodes.insert(em.id);
        size_t tagCounter = 0;
        for (auto const & tag : em.Tags())
        {
          TEST_EQUAL(tag.key, validNodeTags[tagCounter].first, ());
          TEST_EQUAL(tag.value, validNodeTags[tagCounter].second, ());
          tagCounter++;
        }
        break;
      }
      case TType::Way:
      {
        size_t ndCounter = 0;
        for (auto const & nd : em.Nodes())
        {
          ndCounter++;
          TEST(nodes.count(nd), ());
        }
        TEST_EQUAL(nodes.size(), ndCounter, ());
        for (auto const & tag : em.Tags())
        {
          TEST(false, ("Unexpected tag:", tag.key, tag.value));
        }
        break;
      }
      case TType::Relation:
      {
        size_t memberCounter = 0;
        size_t tagCounter = 0;
        for (auto const & member : em.Members())
        {
          TEST(entities.count(member.ref), ());
          TEST_EQUAL(relationMembers[memberCounter].first, member.type,
                      ("Current member:", memberCounter));
          TEST_EQUAL(relationMembers[memberCounter].second, member.role,
                      ("Current member:", memberCounter));
          memberCounter++;
        }
        TEST_EQUAL(memberCounter, 2, ());
        for (auto const & tag : em.Tags())
        {
          TEST_EQUAL(tag.key, validRelationTags[tagCounter].first, ());
          TEST_EQUAL(tag.value, validRelationTags[tagCounter].second, ());
          tagCounter++;
        }
        TEST_EQUAL(validRelationTags.size(), tagCounter, ());
        break;
      }
      default:
        break;
    }
  }
}
}  // namespace osm_o5m_source_test
