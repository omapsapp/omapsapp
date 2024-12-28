#include "indexer/feature.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature_algo.hpp"
#include "indexer/feature_impl.hpp"
#include "indexer/feature_utils.hpp"
#include "indexer/map_object.hpp"
#include "indexer/shared_load_info.hpp"

#include "geometry/mercator.hpp"

#include "platform/preferred_languages.hpp"

#include "coding/byte_stream.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"
#include "base/stl_helpers.hpp"

#include <algorithm>
#include <limits>

using namespace feature;
using namespace std;

namespace
{
uint32_t constexpr kInvalidOffset = numeric_limits<uint32_t>::max();

bool IsRealGeomOffset(uint32_t offset)
{
  return (offset != kInvalidOffset && offset != kGeomOffsetFallback);
}

// Get an index of inner geometry scale range.
// @param[in]  scale:
// -1 : index for the best geometry
// -2 : index for the worst geometry
// default : index for the request scale
int GetScaleIndex(SharedLoadInfo const & loadInfo, int scale)
{
  int const count = loadInfo.GetScalesCount();

  switch (scale)
  {
  case FeatureType::WORST_GEOMETRY: return 0;
  case FeatureType::BEST_GEOMETRY: return count - 1;
  default:
    // In case of WorldCoasts we should get correct last geometry.
    int const lastScale = loadInfo.GetLastScale();
    if (scale > lastScale)
      scale = lastScale;

    for (int i = 0; i < count; ++i)
    {
      if (scale <= loadInfo.GetScale(i))
        return i;
    }
    ASSERT(false, ("No suitable geometry scale range in the map file."));
    return -1;
  }
}

// Get an index of outer geometry scale range.
int GetScaleIndex(SharedLoadInfo const & loadInfo, int scale,
                  FeatureType::GeometryOffsets const & offsets)
{
  int const count = loadInfo.GetScalesCount();
  ASSERT_EQUAL(count, static_cast<int>(offsets.size()), ());

  int ind = 0;
  switch (scale)
  {
  case FeatureType::BEST_GEOMETRY:
    // Choose the best existing geometry for the last visible scale.
    ind = count - 1;
    while (ind >= 0 && !IsRealGeomOffset(offsets[ind]))
      --ind;
    if (ind >= 0)
      return ind;
    break;

  case FeatureType::WORST_GEOMETRY:
    // Choose the worst existing geometry for the first visible scale.
    while (ind < count && !IsRealGeomOffset(offsets[ind]))
      ++ind;
    if (ind < count)
      return ind;
    break;

  default:
  {
    // In case of WorldCoasts we should get correct last geometry.
    int const lastScale = loadInfo.GetLastScale();
    if (scale > lastScale)
      scale = lastScale;

    // If there is no geometry for the requested scale (kHasGeoOffsetFlag) fallback to the next more detailed one.
    while (ind < count && (scale > loadInfo.GetScale(ind) || offsets[ind] == kGeomOffsetFallback))
      ++ind;

    // Some WorldCoasts features have idx == 0 geometry only and its possible
    // other features to be visible on e.g. idx == 1 only,
    // but then they shouldn't be attempted to be drawn using other geom scales.
    ASSERT_LESS(ind, count, ("No suitable geometry scale range in the map file."));
    return (offsets[ind] != kInvalidOffset ? ind : -1);
  }
  }

  ASSERT(false, ("A feature doesn't have any geometry."));
  return -1;
}

uint32_t CalcOffset(ArrayByteSource const & source, uint8_t const * start)
{
  ASSERT_GREATER_OR_EQUAL(source.PtrUint8(), start, ());
  return static_cast<uint32_t>(distance(start, source.PtrUint8()));
}

uint8_t Header(vector<uint8_t> const & data)
{
 CHECK(!data.empty(), ());
 return data[0];
}

void ReadOffsets(SharedLoadInfo const & loadInfo, ArrayByteSource & src, uint8_t mask,
                 FeatureType::GeometryOffsets & offsets)
{
  ASSERT(offsets.empty(), ());
  ASSERT_GREATER(mask, 0, ());

  offsets.resize(loadInfo.GetScalesCount(), kInvalidOffset);
  size_t ind = 0;

  while (mask > 0)
  {
    if (mask & 0x01)
      offsets[ind] = ReadVarUint<uint32_t>(src);

    ++ind;
    mask = mask >> 1;
  }
}

class BitSource
{
  uint8_t const * m_ptr;
  uint8_t m_pos;

public:
  explicit BitSource(uint8_t const * p) : m_ptr(p), m_pos(0) {}

  uint8_t Read(uint8_t count)
  {
    ASSERT_LESS(count, 9, ());

    uint8_t v = *m_ptr;
    v >>= m_pos;
    v &= ((1 << count) - 1);

    m_pos += count;
    if (m_pos >= 8)
    {
      ASSERT_EQUAL(m_pos, 8, ());
      ++m_ptr;
      m_pos = 0;
    }

    return v;
  }

  uint8_t const * RoundPtr()
  {
    if (m_pos > 0)
    {
      ++m_ptr;
      m_pos = 0;
    }
    return m_ptr;
  }
};

template <class TSource>
uint8_t ReadByte(TSource & src)
{
  return ReadPrimitiveFromSource<uint8_t>(src);
}
}  // namespace

FeatureType::FeatureType(SharedLoadInfo const * loadInfo, vector<uint8_t> && buffer,
                         indexer::MetadataDeserializer * metadataDeserializer)
  : m_loadInfo(loadInfo)
  , m_data(std::move(buffer))
  , m_metadataDeserializer(metadataDeserializer)
{
  CHECK(m_loadInfo, ());

  m_header = Header(m_data); // Parse the header and optional name/layer/addinfo.
}

std::unique_ptr<FeatureType> FeatureType::CreateFromMapObject(osm::MapObject const & emo)
{
  auto ft = std::unique_ptr<FeatureType>(new FeatureType());

  HeaderGeomType headerGeomType = HeaderGeomType::Point;
  ft->m_limitRect.MakeEmpty();

  switch (emo.GetGeomType())
  {
  case feature::GeomType::Undefined:
    // It is not possible because of FeatureType::GetGeomType() never returns GeomType::Undefined.
    UNREACHABLE();
  case feature::GeomType::Point:
    headerGeomType = HeaderGeomType::Point;
    ft->m_center = emo.GetMercator();
    ft->m_limitRect.Add(ft->m_center);
    break;
  case feature::GeomType::Line:
    headerGeomType = HeaderGeomType::Line;
    assign_range(ft->m_points, emo.GetPoints());
    for (auto const & p : ft->m_points)
      ft->m_limitRect.Add(p);
    break;
  case feature::GeomType::Area:
    headerGeomType = HeaderGeomType::Area;
    assign_range(ft->m_triangles, emo.GetTriangesAsPoints());
    for (auto const & p : ft->m_triangles)
      ft->m_limitRect.Add(p);
    break;
  }

  ft->m_parsed.m_points = ft->m_parsed.m_triangles = true;

  ft->m_params.name = emo.GetNameMultilang();
  string const & house = emo.GetHouseNumber();
  if (house.empty())
    ft->m_params.house.Clear();
  else
    ft->m_params.house.Set(house);
  ft->m_parsed.m_common = true;

  emo.AssignMetadata(ft->m_metadata);
  ft->m_parsed.m_metadata = true;
  ft->m_parsed.m_metaIds = true;

  CHECK_LESS_OR_EQUAL(emo.GetTypes().Size(), feature::kMaxTypesCount, ());
  copy(emo.GetTypes().begin(), emo.GetTypes().end(), ft->m_types.begin());

  ft->m_parsed.m_types = true;
  ft->m_header = CalculateHeader(emo.GetTypes().Size(), headerGeomType, ft->m_params);
  ft->m_parsed.m_header2 = true;

  ft->m_id = emo.GetID();
  return ft;
}

feature::GeomType FeatureType::GetGeomType() const
{
  // FeatureType::FeatureType(osm::MapObject const & emo) expects
  // that GeomType::Undefined is never returned.
  auto const headerGeomType = static_cast<HeaderGeomType>(m_header & HEADER_MASK_GEOMTYPE);
  switch (headerGeomType)
  {
  case HeaderGeomType::Line: return GeomType::Line;
  case HeaderGeomType::Area: return GeomType::Area;
  default: return GeomType::Point; // HeaderGeomType::Point/PointEx
  }
}

void FeatureType::ParseTypes()
{
  if (m_parsed.m_types)
    return;

  auto const typesOffset = sizeof(m_header);
  Classificator & c = classif();
  ArrayByteSource source(m_data.data() + typesOffset);

  size_t const count = GetTypesCount();
  for (size_t i = 0; i < count; ++i)
  {
    uint32_t index = ReadVarUint<uint32_t>(source);
    uint32_t const type = c.GetTypeForIndex(index);
    if (type > 0)
      m_types[i] = type;
    else
    {
      // Possible for newer MWMs with added types.
      LOG(LWARNING, ("Incorrect type index for feature. FeatureID:", m_id, ". Incorrect index:", index,
                   ". Loaded feature types:", m_types, ". Total count of types:", count));

      m_types[i] = c.GetStubType();
    }
  }

  m_offsets.m_common = CalcOffset(source, m_data.data());
  m_parsed.m_types = true;
}

void FeatureType::ParseCommon()
{
  if (m_parsed.m_common)
    return;

  CHECK(m_loadInfo, ());
  ParseTypes();

  ArrayByteSource source(m_data.data() + m_offsets.m_common);
  uint8_t const h = Header(m_data);
  m_params.Read(source, h);

  if (GetGeomType() == GeomType::Point)
  {
    m_center = serial::LoadPoint(source, m_loadInfo->GetDefGeometryCodingParams());
    m_limitRect.Add(m_center);
  }

  m_offsets.m_header2 = CalcOffset(source, m_data.data());
  m_parsed.m_common = true;
}

m2::PointD FeatureType::GetCenter()
{
  ASSERT_EQUAL(GetGeomType(), feature::GeomType::Point, ());
  ParseCommon();
  return m_center;
}

int8_t FeatureType::GetLayer()
{
  if ((m_header & feature::HEADER_MASK_HAS_LAYER) == 0)
    return feature::LAYER_EMPTY;

  ParseCommon();
  return m_params.layer;
}

// TODO: there is a room to store more information in Header2 (geometry header),
// but it needs a mwm version change.
// 1st bit - inner / outer flag
// 4 more bits - inner points/triangles count or outer geom offsets mask
//   (but actually its enough to store number of the first existing geom level only - 2 bits)
// 3-5 more bits are spare
// One of them could be used for a closed line flag to avoid storing identical first + last points.
void FeatureType::ParseHeader2()
{
  if (m_parsed.m_header2)
    return;

  CHECK(m_loadInfo, ());
  ParseCommon();

  uint8_t elemsCount = 0, geomScalesMask = 0;
  BitSource bitSource(m_data.data() + m_offsets.m_header2);
  auto const headerGeomType = static_cast<HeaderGeomType>(Header(m_data) & HEADER_MASK_GEOMTYPE);

  if (headerGeomType == HeaderGeomType::Line || headerGeomType == HeaderGeomType::Area)
  {
    elemsCount = bitSource.Read(4);
    // For outer geometry read the geom scales (offsets) mask.
    // For inner geometry remaining 4 bits are not used.
    if (elemsCount == 0)
      geomScalesMask = bitSource.Read(4);
    else
    {
      ASSERT(headerGeomType == HeaderGeomType::Area || elemsCount > 1, ());
    }
  }

  ArrayByteSource src(bitSource.RoundPtr());
  serial::GeometryCodingParams const & cp = m_loadInfo->GetDefGeometryCodingParams();

  if (headerGeomType == HeaderGeomType::Line)
  {
    if (elemsCount > 0)
    {
      // Inner geometry.
      // Number of bytes in simplification mask:
      // first and last points are never simplified/discarded,
      // 2 bits are used per each other point, i.e.
      // 3-6 pts - 1 byte, 7-10 pts - 2b, 11-14 pts - 3b.
      int const count = ((elemsCount - 2) + 4 - 1) / 4;
      ASSERT_LESS(count, 4, ());

      for (int i = 0; i < count; ++i)
      {
        uint32_t mask = ReadByte(src);
        m_ptsSimpMask += (mask << (i << 3));
      }

      auto const * start = src.PtrUint8();
      src = ArrayByteSource(serial::LoadInnerPath(start, elemsCount, cp, m_points));
      // TODO: here and further m_innerStats is needed for stats calculation in generator_tool only
      m_innerStats.m_points = CalcOffset(src, start);
    }
    else
    {
      // Outer geometry: first (base) point is stored in the header still.
      auto const * start = src.PtrUint8();
      m_points.emplace_back(serial::LoadPoint(src, cp));
      m_innerStats.m_firstPoints = CalcOffset(src, start);
      ReadOffsets(*m_loadInfo, src, geomScalesMask, m_offsets.m_pts);
    }
  }
  else if (headerGeomType == HeaderGeomType::Area)
  {
    if (elemsCount > 0)
    {
      // Inner geometry (strips).
      elemsCount += 2;

      auto const * start = src.PtrUint8();
      src = ArrayByteSource(serial::LoadInnerTriangles(start, elemsCount, cp, m_triangles));
      m_innerStats.m_strips = CalcOffset(src, start);
    }
    else
    {
      // Outer geometry.
      ReadOffsets(*m_loadInfo, src, geomScalesMask, m_offsets.m_trg);
    }
  }
  // Size of the whole header incl. inner geometry / triangles.
  m_innerStats.m_size = CalcOffset(src, m_data.data());
  m_parsed.m_header2 = true;
}

void FeatureType::ResetGeometry()
{
  // Do not reset geometry for features created from MapObjects.
  if (!m_loadInfo)
    return;

  m_points.clear();
  m_triangles.clear();

  if (GetGeomType() != GeomType::Point)
    m_limitRect = m2::RectD();

  m_parsed.m_header2 = m_parsed.m_points = m_parsed.m_triangles = false;
  m_offsets.m_pts.clear();
  m_offsets.m_trg.clear();
  m_ptsSimpMask = 0;
}

void FeatureType::ParseGeometry(int scale)
{
  if (!m_parsed.m_points)
  {
    CHECK(m_loadInfo, ());
    ParseHeader2();

    auto const headerGeomType = static_cast<HeaderGeomType>(Header(m_data) & HEADER_MASK_GEOMTYPE);
    if (headerGeomType == HeaderGeomType::Line)
    {
      size_t const pointsCount = m_points.size();
      if (pointsCount < 2)
      {
        ASSERT_EQUAL(pointsCount, 1, ());

        // Outer geometry.
        int const ind = GetScaleIndex(*m_loadInfo, scale, m_offsets.m_pts);
        if (ind != -1)
        {
          ReaderSource<FilesContainerR::TReader> src(m_loadInfo->GetGeometryReader(ind));
          src.Skip(m_offsets.m_pts[ind]);

          serial::GeometryCodingParams cp = m_loadInfo->GetGeometryCodingParams(ind);
          cp.SetBasePoint(m_points[0]);
          serial::LoadOuterPath(src, cp, m_points);
        }
      }
      else
      {
        // Filter inner geometry according to points simplification mask.
        PointsBufferT points;
        points.reserve(pointsCount);

        int const ind = GetScaleIndex(*m_loadInfo, scale);
        points.emplace_back(m_points.front());
        for (size_t i = 1; i + 1 < pointsCount; ++i)
        {
          // Check if the point is visible in the requested scale index.
          if (static_cast<int>((m_ptsSimpMask >> (2 * (i - 1))) & 0x3) <= ind)
            points.emplace_back(m_points[i]);
        }
        points.emplace_back(m_points.back());

        m_points.swap(points);
      }

      CalcRect(m_points, m_limitRect);
    }
    m_parsed.m_points = true;
  }
}

FeatureType::GeomStat FeatureType::GetOuterGeometryStats()
{
  CHECK(m_loadInfo && m_parsed.m_header2 && !m_parsed.m_points, ("Call geometry stats first and once!"));
  size_t const scalesCount = m_loadInfo->GetScalesCount();
  ASSERT_LESS_OR_EQUAL(scalesCount, DataHeader::kMaxScalesCount, ("MWM has too many geometry scales!"));
  FeatureType::GeomStat res;

  auto const headerGeomType = static_cast<HeaderGeomType>(Header(m_data) & HEADER_MASK_GEOMTYPE);
  if (headerGeomType == HeaderGeomType::Line)
  {
    size_t const pointsCount = m_points.size();
    if (pointsCount < 2)
    {
      // Outer geometry present.
      ASSERT_EQUAL(pointsCount, 1, ());

      PointsBufferT points;

      for (size_t ind = 0; ind < scalesCount; ++ind)
      {
        uint32_t const scaleOffset = m_offsets.m_pts[ind];
        if (IsRealGeomOffset(scaleOffset))
        {
          points.clear();
          points.emplace_back(m_points.front());

          ReaderSource<FilesContainerR::TReader> src(m_loadInfo->GetGeometryReader(ind));
          src.Skip(scaleOffset);

          serial::GeometryCodingParams cp = m_loadInfo->GetGeometryCodingParams(static_cast<int>(ind));
          cp.SetBasePoint(points[0]);
          serial::LoadOuterPath(src, cp, points);

          res.m_sizes[ind] = static_cast<uint32_t>(src.Pos() - scaleOffset);
          res.m_elements[ind] = static_cast<uint32_t>(points.size());
        }
      }
      // Retain best geometry.
      m_points.swap(points);
    }
    CalcRect(m_points, m_limitRect);
  }
  m_parsed.m_points = true;

  // Points count can come from the inner geometry.
  res.m_elements[scalesCount - 1] = static_cast<uint32_t>(m_points.size());
  return res;
}

void FeatureType::ParseTriangles(int scale)
{
  if (!m_parsed.m_triangles)
  {
    CHECK(m_loadInfo, ());
    ParseHeader2();

    auto const headerGeomType = static_cast<HeaderGeomType>(Header(m_data) & HEADER_MASK_GEOMTYPE);
    if (headerGeomType == HeaderGeomType::Area)
    {
      if (m_triangles.empty())
      {
        int const ind = GetScaleIndex(*m_loadInfo, scale, m_offsets.m_trg);
        if (ind != -1)
        {
          ReaderSource<FilesContainerR::TReader> src(m_loadInfo->GetTrianglesReader(ind));
          src.Skip(m_offsets.m_trg[ind]);
          serial::LoadOuterTriangles(src, m_loadInfo->GetGeometryCodingParams(ind), m_triangles);
        }
      }

      CalcRect(m_triangles, m_limitRect);
    }
    m_parsed.m_triangles = true;
  }
}

FeatureType::GeomStat FeatureType::GetOuterTrianglesStats()
{
  CHECK(m_loadInfo && m_parsed.m_header2 && !m_parsed.m_triangles, ("Call geometry stats first and once!"));
  int const scalesCount = m_loadInfo->GetScalesCount();
  ASSERT_LESS_OR_EQUAL(scalesCount, static_cast<int>(DataHeader::kMaxScalesCount), ("MWM has too many geometry scales!"));
  FeatureType::GeomStat res;

  auto const headerGeomType = static_cast<HeaderGeomType>(Header(m_data) & HEADER_MASK_GEOMTYPE);
  if (headerGeomType == HeaderGeomType::Area)
  {
    if (m_triangles.empty())
    {
      for (int ind = 0; ind < scalesCount; ++ind)
      {
        uint32_t const scaleOffset = m_offsets.m_trg[ind];
        if (IsRealGeomOffset(scaleOffset))
        {
          m_triangles.clear();

          ReaderSource<FilesContainerR::TReader> src(m_loadInfo->GetTrianglesReader(ind));
          src.Skip(scaleOffset);
          serial::LoadOuterTriangles(src, m_loadInfo->GetGeometryCodingParams(ind), m_triangles);

          res.m_sizes[ind] = static_cast<uint32_t>(src.Pos() - scaleOffset);
          res.m_elements[ind] = static_cast<uint32_t>(m_triangles.size() / 3);
        }
      }
      // The best geometry is retained in m_triangles.
    }
    CalcRect(m_triangles, m_limitRect);
  }
  m_parsed.m_triangles = true;

  // Triangles count can come from the inner geometry.
  res.m_elements[scalesCount - 1] = static_cast<uint32_t>(m_triangles.size() / 3);
  return res;
}

void FeatureType::ParseMetadata()
{
  if (m_parsed.m_metadata)
    return;

  CHECK(m_metadataDeserializer, ());
  try
  {
    UNUSED_VALUE(m_metadataDeserializer->Get(m_id.m_index, m_metadata));
  }
  catch (Reader::OpenException const &)
  {
    LOG(LERROR, ("Error reading metadata", m_id));
  }

  m_parsed.m_metadata = true;
}

void FeatureType::ParseMetaIds()
{
  if (m_parsed.m_metaIds)
    return;

  CHECK(m_metadataDeserializer, ());
  try
  {
    UNUSED_VALUE(m_metadataDeserializer->GetIds(m_id.m_index, m_metaIds));
  }
  catch (Reader::OpenException const &)
  {
    LOG(LERROR, ("Error reading metadata", m_id));
  }

  m_parsed.m_metaIds = true;
}

StringUtf8Multilang const & FeatureType::GetNames()
{
  ParseCommon();
  return m_params.name;
}

std::string FeatureType::DebugString()
{
  ParseGeometryAndTriangles(FeatureType::BEST_GEOMETRY);

  std::string res = DebugPrint(m_id) + " " + DebugPrint(GetGeomType());

  m2::PointD keyPoint;
  switch (GetGeomType())
  {
  case GeomType::Point:
    keyPoint = m_center;
    break;

  case GeomType::Line:
    if (m_points.empty())
      break;
    keyPoint = m_points.front();
    break;

  case GeomType::Area:
    if (m_triangles.empty())
      break;
    ASSERT_GREATER(m_triangles.size(), 2, ());
    keyPoint = (m_triangles[0] + m_triangles[1] + m_triangles[2]) / 3.0;
    break;

  case GeomType::Undefined:
    ASSERT(false, ());
    break;
  }
  // Print coordinates in (lat,lon) for better investigation capabilities.
  res += ": " + DebugPrint(keyPoint) + "; " + DebugPrint(mercator::ToLatLon(keyPoint)) + "\n";

  Classificator const & c = classif();

  res += "Types";
  uint32_t const count = GetTypesCount();
  for (size_t i = 0; i < count; ++i)
    res += (" : " + c.GetReadableObjectName(m_types[i]));
  res += "\n";

  auto const paramsStr = m_params.DebugString();
  if (!paramsStr.empty())
    res += paramsStr + "\n";

  return res;
}

m2::RectD FeatureType::GetLimitRect(int scale)
{
  ParseGeometryAndTriangles(scale);

  if (m_triangles.empty() && m_points.empty() && (GetGeomType() != GeomType::Point))
  {
    ASSERT(false, (m_id));

    // This function is called during indexing, when we need
    // to check visibility according to feature sizes.
    // So, if no geometry for this scale, assume that rect has zero dimensions.
    m_limitRect = m2::RectD(0, 0, 0, 0);
  }

  return m_limitRect;
}

m2::RectD const & FeatureType::GetLimitRectChecked() const
{
  ASSERT(m_parsed.m_points && m_parsed.m_triangles, (m_id));
  ASSERT(m_limitRect.IsValid(), (m_id));
  return m_limitRect;
}

bool FeatureType::IsEmptyGeometry(int scale)
{
  ParseGeometryAndTriangles(scale);

  switch (GetGeomType())
  {
  case GeomType::Area: return m_triangles.empty();
  case GeomType::Line: return m_points.empty();
  default: return false;
  }
}

size_t FeatureType::GetPointsCount() const
{
  ASSERT(m_parsed.m_points, ());
  return m_points.size();
}

m2::PointD const & FeatureType::GetPoint(size_t i) const
{
  ASSERT_LESS(i, m_points.size(), ());
  ASSERT(m_parsed.m_points, ());
  return m_points[i];
}

FeatureType::PointsBufferT const & FeatureType::GetPoints(int scale)
{
  ParseGeometry(scale);
  return m_points;
}

FeatureType::PointsBufferT const & FeatureType::GetTrianglesAsPoints(int scale)
{
  ParseTriangles(scale);
  return m_triangles;
}

void FeatureType::ParseGeometryAndTriangles(int scale)
{
  ParseGeometry(scale);
  ParseTriangles(scale);
}

void FeatureType::GetPreferredNames(bool allowTranslit, int8_t deviceLang, feature::NameParamsOut & out)
{
  if (!HasName())
    return;

  auto const & mwmInfo = m_id.m_mwmId.GetInfo();
  if (!mwmInfo)
    return;

  ParseCommon();

  feature::GetPreferredNames({ GetNames(), mwmInfo->GetRegionData(), deviceLang, allowTranslit }, out);
}

string_view FeatureType::GetReadableName()
{
  feature::NameParamsOut out;
  GetReadableName(false /* allowTranslit */, StringUtf8Multilang::GetLangIndex(languages::GetCurrentMapLanguage()), out);
  return out.primary;
}

void FeatureType::GetReadableName(bool allowTranslit, int8_t deviceLang, feature::NameParamsOut & out)
{
  if (!HasName())
    return;

  auto const & mwmInfo = m_id.m_mwmId.GetInfo();
  if (!mwmInfo)
    return;

  ParseCommon();

  feature::GetReadableName({ GetNames(), mwmInfo->GetRegionData(), deviceLang, allowTranslit }, out);
}

string const & FeatureType::GetHouseNumber()
{
  ParseCommon();
  return m_params.house.Get();
}

string_view FeatureType::GetName(int8_t lang)
{
  if (!HasName())
    return {};

  ParseCommon();

  // We don't store empty names. UPD: We do for coast features :)
  string_view name;
  if (m_params.name.GetString(lang, name))
  {
    ASSERT(!name.empty() || m_id.m_mwmId.GetInfo()->GetType() == MwmInfo::COASTS, ());
  }

  return name;
}

uint8_t FeatureType::GetRank()
{
  ParseCommon();
  return m_params.rank;
}

uint64_t FeatureType::GetPopulation() { return feature::RankToPopulation(GetRank()); }

string const & FeatureType::GetRef()
{
  ParseCommon();
  return m_params.ref;
}

feature::Metadata const & FeatureType::GetMetadata()
{
  ParseMetadata();
  return m_metadata;
}

std::string_view FeatureType::GetMetadata(feature::Metadata::EType type)
{
  ParseMetaIds();

  auto meta = m_metadata.Get(type);
  if (meta.empty())
  {
    auto const it = base::FindIf(m_metaIds, [&type](auto const & v) { return v.first == type; });
    if (it != m_metaIds.end())
      meta = m_metadata.Set(type, m_metadataDeserializer->GetMetaById(it->second));
  }
  return meta;
}

bool FeatureType::HasMetadata(feature::Metadata::EType type)
{
  ParseMetaIds();
  if (m_metadata.Has(type))
    return true;

  return base::FindIf(m_metaIds, [&type](auto const & v) { return v.first == type; }) !=
         m_metaIds.end();
}
