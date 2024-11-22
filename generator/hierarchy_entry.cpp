#include "generator/hierarchy_entry.hpp"

#include "indexer/feature_utils.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "coding/string_utf8_multilang.hpp"

#include "base/assert.hpp"
#include "base/string_utils.hpp"
#include "base/stl_helpers.hpp"

#include <algorithm>
#include <sstream>
#include <tuple>
#include <unordered_map>

#include "cppjansson/cppjansson.hpp"

namespace
{
// GetRussianName returns a Russian feature name if it's possible.
// Otherwise, GetRussianName function returns a name that GetReadableName returns.
std::string GetRussianName(StringUtf8Multilang const & str)
{
  feature::NameParamsOut out;
  feature::GetReadableName({ str, {} /* regionData */, "ru", false /* allowTranslit */ }, out);
  std::string result(out.primary);

  for (auto const & ch : {';', '\n', '\t'})
    std::replace(std::begin(result), std::end(result), ch, ',');
  return result;
}
}  // namespace

namespace generator
{
bool operator==(HierarchyEntry const & lhs, HierarchyEntry const & rhs)
{
  return AlmostEqualAbs(lhs.m_center, rhs.m_center, 1e-7) &&
      (std::tie(lhs.m_id, lhs.m_parentId, lhs.m_depth, lhs.m_name, lhs.m_country, lhs.m_type) ==
       std::tie(rhs.m_id, rhs.m_parentId, rhs.m_depth, rhs.m_name, rhs.m_country, rhs.m_type));
}

std::string DebugPrint(HierarchyEntry const & entry)
{
  auto obj = base::NewJSONObject();
  ToJSONObject(*obj, "id", DebugPrint(entry.m_id));
  if (entry.m_parentId)
    ToJSONObject(*obj, "parentId", DebugPrint(*entry.m_parentId));
  ToJSONObject(*obj, "depth", entry.m_depth);
  ToJSONObject(*obj, "type", classif().GetReadableObjectName(entry.m_type));
  ToJSONObject(*obj, "name", entry.m_name);
  ToJSONObject(*obj, "country", entry.m_country);

  auto center = base::NewJSONObject();
  ToJSONObject(*center, "x", entry.m_center.x);
  ToJSONObject(*center, "y", entry.m_center.y);
  ToJSONObject(*obj, "center", center);
  return DumpToString(obj);
}

namespace hierarchy
{
uint32_t GetMainType(FeatureParams::Types const & types)
{
  auto const & airportChecker = ftypes::IsAirportChecker::Instance();
  auto it = base::FindIf(types, airportChecker);
  if (it != std::cend(types))
    return *it;

  auto const & attractChecker = ftypes::AttractionsChecker::Instance();
  auto const type = attractChecker.GetBestType(types);
  if (type != ftype::GetEmptyValue())
    return type;

  auto const & eatChecker = ftypes::IsEatChecker::Instance();
  it = base::FindIf(types, eatChecker);
  if (it != std::cend(types))
    return *it;

  auto const & buildingPartChecker = ftypes::IsBuildingPartChecker::Instance();
  it = base::FindIf(types, buildingPartChecker);
  return it != std::cend(types) ? *it : ftype::GetEmptyValue();
}

std::string GetName(StringUtf8Multilang const & str) { return GetRussianName(str); }

std::string HierarchyEntryToCsvString(HierarchyEntry const & entry, char delim)
{
  return strings::JoinStrings(HierarchyEntryToCsvRow(entry), delim);
}

coding::CSVReader::Row HierarchyEntryToCsvRow(HierarchyEntry const & entry)
{
  coding::CSVReader::Row row;
  row.emplace_back(entry.m_id.ToString());
  std::string parentId;
  if (entry.m_parentId)
    parentId = (*entry.m_parentId).ToString();

  row.emplace_back(parentId);
  row.emplace_back(strings::to_string(entry.m_depth));
  row.emplace_back(strings::to_string_dac(entry.m_center.x, 7));
  row.emplace_back(strings::to_string_dac(entry.m_center.y, 7));
  row.emplace_back(strings::to_string(classif().GetReadableObjectName(entry.m_type)));
  row.emplace_back(strings::to_string(entry.m_name));
  row.emplace_back(strings::to_string(entry.m_country));
  return row;
}

HierarchyEntry HierarchyEntryFromCsvRow(coding::CSVReader::Row const & row)
{
  CHECK_EQUAL(row.size(), 8, (row));

  auto const & id = row[0];
  auto const & parentId = row[1];
  auto const & depth = row[2];
  auto const & x = row[3];
  auto const & y = row[4];
  auto const & type = row[5];
  auto const & name = row[6];
  auto const & country = row[7];

  HierarchyEntry entry;
  entry.m_id = CompositeId(id);
  if (!parentId.empty())
    entry.m_parentId = CompositeId(parentId);

  VERIFY(strings::to_size_t(depth, entry.m_depth), (row));
  VERIFY(strings::to_double(x, entry.m_center.x), (row));
  VERIFY(strings::to_double(y, entry.m_center.y), (row));
  entry.m_type = classif().GetTypeByReadableObjectName(type);
  entry.m_name = name;
  entry.m_country = country;
  return entry;
}

tree_node::types::Ptrs<HierarchyEntry> LoadHierachy(std::string const & filename)
{
  std::unordered_map<CompositeId, tree_node::types::Ptr<HierarchyEntry>> nodes;
  for (auto const & row : coding::CSVRunner(
         coding::CSVReader(filename, false /* hasHeader */, kCsvDelimiter)))
  {
    auto entry = HierarchyEntryFromCsvRow(row);
    auto const id = entry.m_id;
    nodes.emplace(id, tree_node::MakeTreeNode(std::move(entry)));
  }
  for (auto const & pair : nodes)
  {
    auto const & node = pair.second;
    auto const parentIdOpt = node->GetData().m_parentId;
    if (parentIdOpt)
    {
      auto const it = nodes.find(*parentIdOpt);
      CHECK(it != std::cend(nodes), (*it));
      tree_node::Link(node, it->second);
    }
  }
  std::vector<tree_node::types::Ptr<HierarchyEntry>> trees;
  base::Transform(nodes, std::back_inserter(trees), base::RetrieveSecond());
  base::EraseIf(trees, [](auto const & node) { return node->HasParent(); });
  return trees;
}
}  // namespace hierarchy
}  // namespace generator
