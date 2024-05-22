#include "addresses_collector.hpp"

#include "generator/feature_builder.hpp"
#include "generator/osm_element.hpp"

#include "search/house_numbers_matcher.hpp"

#include "indexer/ftypes_matcher.hpp"

#include "coding/read_write_utils.hpp"

namespace generator
{
using namespace feature;
using namespace strings;

namespace
{
uint64_t GetOsmID(FeatureBuilder const & fb)
{
  return fb.GetMostGenericOsmId().GetSerialId();
}

AddressesHolder::AddressInfo FromFB(FeatureBuilder const & fb)
{
  auto const & params = fb.GetParams();
  return { params.house.Get(), std::string(params.GetStreet()), std::string(params.GetPostcode()), {} };
}

void LogWarning(std::string const & msg, uint64_t id)
{
  LOG(LWARNING, ("addr:interpolation: " + msg, id));
}
} // namespace


std::string AddressesHolder::AddressInfo::FormatRange() const
{
  if (m_house.empty() || m_house2.empty())
    return {};

  using namespace search::house_numbers;

  std::vector<TokensT> left, right;
  ParseHouseNumber(MakeUniString(m_house), left);
  ParseHouseNumber(MakeUniString(m_house2), right);

  /// @todo Or try to take min/max range. See example here:
  /// https://www.openstreetmap.org/way/1118117172
  if (left.size() != 1 || right.size() != 1)
    return {};

  CHECK(!left[0].empty() && !right[0].empty(), (m_house, m_house2));
  auto & l = left[0][0];
  auto & r = right[0][0];
  if (l.m_type != Token::TYPE_NUMBER || r.m_type != Token::TYPE_NUMBER)
    return {};

  uint64_t const li = ToUInt(l.m_value);
  uint64_t const ri = ToUInt(r.m_value);
  if (li == 0 || ri == 0 || li == ri)
    return {};

  UniString sep(1, ':');
  if (li < ri)
  {
    l.m_value += sep;
    l.m_value += r.m_value;
    return ToUtf8(l.m_value);
  }
  else
  {
    r.m_value += sep;
    r.m_value += l.m_value;
    return ToUtf8(r.m_value);
  }
}


void AddressesHolder::Add(FeatureBuilder const & fb)
{
  CHECK(fb.IsPoint(), ());
  CHECK(m_addresses.emplace(GetOsmID(fb), FromFB(fb)).second, ());
}

bool AddressesHolder::Update(feature::FeatureBuilder & fb) const
{
  uint64_t const id = GetOsmID(fb);
  auto const i = m_addresses.find(id);
  if (i == m_addresses.end())
    return false;

  // Get existing params (street, postcode).
  auto const info = FromFB(fb);
  auto & params = fb.GetParams();

  // Force update ref as house's range.
  if (!params.ref.empty())
    LogWarning("Ref is not empty", id);

  params.ref = i->second.FormatRange();
  if (params.ref.empty())
  {
    LogWarning("Invalid range", id);
    return false;
  }

  // Update from saved params if needed (prefer existing).
  if (!i->second.m_street.empty() && i->second.m_street != info.m_street)
  {
    if (!info.m_street.empty())
      LogWarning("Different streets", id);
    else
      params.SetStreet(i->second.m_street);
  }

  if (!i->second.m_postcode.empty() && i->second.m_postcode != info.m_postcode)
  {
    if (!info.m_postcode.empty())
      LogWarning("Different postcodes", id);
    else
      params.SetPostcode(i->second.m_postcode);
  }

  return true;
}

void AddressesHolder::MergeInto(AddressesHolder & holder) const
{
  for (auto const & e : m_addresses)
    holder.m_addresses.emplace(e.first, e.second);
}

void AddressesHolder::Deserialize(std::string const & filePath)
{
  FileReader reader(filePath);
  ReaderSource<FileReader> src(reader);

  while (src.Size())
  {
    uint64_t const id = ReadVarUint<uint64_t>(src);

    AddressInfo info;
    rw::Read(src, info.m_house);
    rw::Read(src, info.m_house2);
    rw::Read(src, info.m_street);
    rw::Read(src, info.m_postcode);

    CHECK(m_addresses.emplace(id, std::move(info)).second, (id));
  }
}


AddressesCollector::AddressesCollector(std::string const & filename)
  : CollectorInterface(filename)
{
}

std::shared_ptr<CollectorInterface> AddressesCollector::Clone(IDRInterfacePtr const &) const
{
  return std::make_shared<AddressesCollector>(GetFilename());
}

void AddressesCollector::CollectFeature(FeatureBuilder const & fb, OsmElement const & element)
{
  if (element.m_type == OsmElement::EntityType::Node)
  {
    CHECK(fb.IsPoint(), ());
    if (element.HasTag("addr:housenumber"))
      m_nodeAddresses.Add(fb);
  }
  else if (element.m_type == OsmElement::EntityType::Way)
  {
    /// @todo Should merge interpolation ways somehow.
    /// Looks easy at first glance, but need to refactor Collector<->FeatureBuilder routine ..
    /// Example: https://www.openstreetmap.org/way/237469988

    if (fb.IsLine() && ftypes::IsAddressInterpolChecker::Instance()(fb.GetTypes()))
    {
      CHECK_GREATER(element.m_nodes.size(), 1, (fb));
      WayInfo info{element.m_nodes.front(), element.m_nodes.back()};
      CHECK(m_interpolWays.emplace(element.m_id, std::move(info)).second, (fb));
    }
  }
}

void AddressesCollector::Save()
{
  FileWriter writer(GetFilename());

  for (auto const & e : m_interpolWays)
  {
    auto beg = m_nodeAddresses.Get(e.second.m_beg);
    auto end = m_nodeAddresses.Get(e.second.m_end);
    if (beg == nullptr || end == nullptr)
    {
      LogWarning("No beg/end address point", e.first);
      continue;
    }

    if (beg->m_street != end->m_street && !beg->m_street.empty() && !end->m_street.empty())
      LogWarning("Different streets for beg/end points", e.first);

    if (beg->m_postcode != end->m_postcode && !beg->m_postcode.empty() && !end->m_postcode.empty())
      LogWarning("Different postcodes for beg/end points", e.first);

    WriteVarUint(writer, e.first);

    rw::Write(writer, beg->m_house);
    rw::Write(writer, end->m_house);
    rw::Write(writer, !beg->m_street.empty() ? beg->m_street : end->m_street);
    rw::Write(writer, !beg->m_postcode.empty() ? beg->m_postcode : end->m_postcode);
  }
}

void AddressesCollector::MergeInto(AddressesCollector & collector) const
{
  m_nodeAddresses.MergeInto(collector.m_nodeAddresses);

  for (auto const & e : m_interpolWays)
    collector.m_interpolWays.emplace(e.first, e.second);
}

} // namespace generator
