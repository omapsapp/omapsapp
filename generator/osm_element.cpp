#include "generator/osm_element.hpp"

#include "geometry/mercator.hpp"  // kPointEqualityEps

#include "base/logging.hpp"
#include "base/math.hpp"
#include "base/string_utils.hpp"
#include "base/stl_helpers.hpp"

#include <sstream>

std::string DebugPrint(OsmElement::EntityType type)
{
  switch (type)
  {
  case OsmElement::EntityType::Unknown:
    return "unknown";
  case OsmElement::EntityType::Bounds:
    return "bounds";
  case OsmElement::EntityType::Way:
    return "way";
  case OsmElement::EntityType::Tag:
    return "tag";
  case OsmElement::EntityType::Relation:
    return "relation";
  case OsmElement::EntityType::Osm:
    return "osm";
  case OsmElement::EntityType::Node:
    return "node";
  case OsmElement::EntityType::Nd:
    return "nd";
  case OsmElement::EntityType::Member:
    return "member";
  }
  UNREACHABLE();
}

namespace
{
std::string_view constexpr kUselessKeys[] = {
  "created_by", "source", "odbl", "note", "fixme", "iemv",

  // Skip tags for speedup, now we don't use it
  "not:", "artist_name", "whitewater",// https://wiki.openstreetmap.org/wiki/Whitewater_sports

  // In future we can use this tags for improve our search
  "nat_name", "reg_name", "loc_name", "lock_name", "local_name", "short_name", "official_name"
};
} // namespace

void OsmElement::AddTag(std::string_view key, std::string_view value)
{
  strings::Trim(key);
  strings::Trim(value);

  // Seems like source osm data has empty values. They are useless for us.
  if (key.empty() || value.empty())
    return;

  for (auto const & useless : kUselessKeys)
  {
    if (key == useless)
      return;
  }

  m_tags.emplace_back(key, value);
}

bool OsmElement::HasTag(std::string_view const & key) const
{
  return base::AnyOf(m_tags, [&](auto const & t) { return t.m_key == key; });
}

bool OsmElement::HasTag(std::string_view const & key, std::string_view const & value) const
{
  return base::AnyOf(m_tags, [&](auto const & t) { return t.m_key == key && t.m_value == value; });
}

void OsmElement::Validate()
{
  if (GetTag("type") != "multipolygon")
    return;

  struct MembersCompare
  {
    bool operator()(Member const * l, Member const * r) const
    {
      return *l < *r;
    }
  };

  // Don't to change the initial order of m_members, so make intermediate set.
  std::set<Member const *, MembersCompare> theSet;
  for (Member & m : m_members)
  {
    ASSERT(m.m_ref > 0, (m_id));
    ASSERT(m.m_type != EntityType::Unknown, (m_id));

    if (!theSet.insert(&m).second)
    {
      LOG(LWARNING, ("Duplicating member:", m.m_ref, "in multipolygon Relation:", m_id));
      m.m_ref = 0;
    }
  }

  if (theSet.size() != m_members.size())
  {
    m_members.erase(std::remove_if(m_members.begin(), m_members.end(), [](Member const & m)
    {
      return m.m_ref == 0;
    }), m_members.end());
  }
}

void OsmElement::Clear()
{
  m_type = EntityType::Unknown;
  m_id = 0;
  m_lon = 0.0;
  m_lat = 0.0;
  m_ref = 0;
  m_k.clear();
  m_v.clear();
  m_memberType = EntityType::Unknown;
  m_role.clear();

  m_nodes.clear();
  m_members.clear();
  m_tags.clear();
}

std::string OsmElement::ToString(std::string const & shift) const
{
  std::stringstream ss;
  ss << (shift.empty() ? "\n" : shift);
  switch (m_type)
  {
  case EntityType::Node:
    ss << "Node: " << m_id << " (" << std::fixed << std::setw(7) << m_lat << ", " << m_lon << ")"
       << " tags: " << m_tags.size();
    break;
  case EntityType::Nd:
    ss << "Nd ref: " << m_ref;
    break;
  case EntityType::Way:
    ss << "Way: " << m_id << " nds: " << m_nodes.size() << " tags: " << m_tags.size();
    if (!m_nodes.empty())
    {
      std::string shift2 = shift;
      shift2 += shift2.empty() ? "\n  " : "  ";
      for (auto const & e : m_nodes)
        ss << shift2 << e;
    }
    break;
  case EntityType::Relation:
    ss << "Relation: " << m_id << " members: " << m_members.size() << " tags: " << m_tags.size();
    if (!m_members.empty())
    {
      std::string shift2 = shift;
      shift2 += shift2.empty() ? "\n  " : "  ";
      for (auto const & e : m_members)
        ss << shift2 << e.m_ref << " " << DebugPrint(e.m_type) << " " << e.m_role;
    }
    break;
  case EntityType::Tag:
    ss << "Tag: " << m_k << " = " << m_v;
    break;
  case EntityType::Member:
    ss << "Member: " << m_ref << " type: " << DebugPrint(m_memberType) << " role: " << m_role;
    break;
  default:
    UNREACHABLE();
    break;
  }

  if (!m_tags.empty())
  {
    std::string shift2 = shift;
    shift2 += shift2.empty() ? "\n  " : "  ";
    for (auto const & e : m_tags)
      ss << shift2 << e.m_key << " = " << e.m_value;
  }
  return ss.str();
}

bool OsmElement::operator==(OsmElement const & other) const
{
  return m_type == other.m_type
         && m_id == other.m_id
         && AlmostEqualAbs(m_lon, other.m_lon, mercator::kPointEqualityEps)
         && AlmostEqualAbs(m_lat, other.m_lat, mercator::kPointEqualityEps)
         && m_ref == other.m_ref
         && m_k == other.m_k
         && m_v == other.m_v
         && m_memberType == other.m_memberType
         && m_role == other.m_role
         && m_nodes == other.m_nodes
         && m_members == other.m_members
         && m_tags == other.m_tags;
}

std::string OsmElement::GetTag(std::string const & key) const
{
  auto const it = base::FindIf(m_tags, [&key](Tag const & tag) { return tag.m_key == key; });
  return it == m_tags.cend() ? std::string() : it->m_value;
}

std::string DebugPrint(OsmElement const & element)
{
  return element.ToString();
}

std::string DebugPrint(OsmElement::Tag const & tag)
{
  std::stringstream ss;
  ss << tag.m_key << '=' << tag.m_value;
  return ss.str();
}

base::GeoObjectId GetGeoObjectId(OsmElement const & element)
{
  switch (element.m_type)
  {
  case OsmElement::EntityType::Node:
    return base::MakeOsmNode(element.m_id);
  case OsmElement::EntityType::Way:
    return base::MakeOsmWay(element.m_id);
  case OsmElement::EntityType::Relation:
    return base::MakeOsmRelation(element.m_id);
  default:
    UNREACHABLE();
    return base::GeoObjectId();
  }
}

std::string DebugPrintID(OsmElement const & e)
{
  return (e.m_type != OsmElement::EntityType::Unknown) ? DebugPrint(GetGeoObjectId(e)) : std::string("Unknown");
}
