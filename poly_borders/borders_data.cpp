#include "poly_borders/borders_data.hpp"

#include "poly_borders/help_structures.hpp"

#include "generator/borders.hpp"
#include "generator/routing_city_boundaries_processor.hpp"

#include "platform/platform.hpp"

#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"
#include "geometry/region2d.hpp"

#include "base/assert.hpp"
#include "base/file_name_utils.hpp"
#include "base/logging.hpp"
#include "base/string_utils.hpp"
#include "base/thread_pool_computational.hpp"

#include <algorithm>
#include <future>
#include <tuple>

namespace
{
using namespace poly_borders;

void PrintWithSpaces(std::string const & str, size_t maxN)
{
  std::cout << str;
  if (maxN <= str.size())
    return;

  maxN -= str.size();
  for (size_t i = 0; i < maxN; ++i)
    std::cout << " ";
}

std::string RemoveIndexFromMwmName(std::string const & mwmName)
{
  auto const pos = mwmName.find(BordersData::kBorderExtension);
  CHECK_NOT_EQUAL(pos, std::string::npos, ());
  auto const end = mwmName.begin() + pos + BordersData::kBorderExtension.size();
  std::string result(mwmName.cbegin(), end);
  return result;
}

bool IsReversedIntervals(size_t fromDst, size_t toDst, size_t fromSrc, size_t toSrc)
{
  return (fromDst > toDst) != (fromSrc > toSrc);
}

std::vector<m2::PointD> AppendPointsWithAnyDirection(std::vector<MarkedPoint> const & copyFrom,
                                                     size_t from, size_t to)
{
  std::vector<m2::PointD> result;
  if (from > to)
    std::swap(from, to);

  for (size_t k = from; k <= to; ++k)
    result.emplace_back(copyFrom[k].m_point);

  return result;
}

double AbsAreaDiff(std::vector<m2::PointD> const & firstPolygon,
                   std::vector<m2::PointD> const & secondPolygon)
{
  auto const firstArea = generator::AreaOnEarth(firstPolygon);
  auto const secondArea = generator::AreaOnEarth(secondPolygon);
  return std::abs(secondArea - firstArea);
}

bool NeedReplace(std::vector<m2::PointD> const & curSubpolygon,
                 std::vector<m2::PointD> const & anotherSubpolygon)
{
  auto const areaDiff = AbsAreaDiff(curSubpolygon, anotherSubpolygon);
  double constexpr kMaxAreaDiffMetersSquared = 20000.0;
  if (areaDiff > kMaxAreaDiffMetersSquared)
    return false;

  if (AlmostEqualAbs(areaDiff, 0.0, BordersData::kEqualityEpsilon))
    return false;

  // We know that |curSize| is always greater than 1, because we construct it such way, but we know
  // nothing about |anotherSize|, and we do not want to replace current subpolygon of several points
  // with a subpolygon consisting of one point or consisting of too many points.
  CHECK_GREATER(curSubpolygon.size(), 1, ());
  return 1 < anotherSubpolygon.size() && anotherSubpolygon.size() < 10;
}

bool ShouldLog(size_t i, size_t n)
{
  return (i % 100 == 0) || (i + 1 == n);
}

void Append(size_t from, size_t to, bool reversed, std::vector<MarkedPoint> const & copyFrom,
            std::vector<MarkedPoint> & copyTo)
{
  if (!reversed)
  {
    for (size_t k = from; k <= to; ++k)
      copyTo.emplace_back(copyFrom[k].m_point);
  }
  else
  {
    size_t k = to;
    while (k >= from)
    {
      copyTo.emplace_back(copyFrom[k].m_point);
      if (k == 0)
        break;
      --k;
    }
  }
}

m2::RectD BoundingBox(std::vector<MarkedPoint> const & points)
{
  m2::RectD rect;
  for (auto const & point : points)
    rect.Add(point.m_point);

  return rect;
}

void SwapIfNeeded(size_t & a, size_t & b)
{
  if (a > b)
    std::swap(a, b);
}
}  // namespace

namespace poly_borders
{
// BordersData::Processor --------------------------------------------------------------------------
void BordersData::Processor::operator()(size_t borderId)
{
  if (ShouldLog(borderId, m_data.m_bordersPolygons.size()))
    LOG(LINFO, ("Marking:", borderId + 1, "/", m_data.m_bordersPolygons.size()));

  auto const & polygon = m_data.m_bordersPolygons[borderId];
  for (size_t pointId = 0; pointId < polygon.m_points.size(); ++pointId)
    m_data.MarkPoint(borderId, pointId);
}

// BordersData -------------------------------------------------------------------------------------
void BordersData::Init(std::string const & bordersDir)
{
  LOG(LINFO, ("Borders path:", bordersDir));

  std::vector<std::string> files;
  Platform::GetFilesByExt(bordersDir, kBorderExtension, files);

  LOG(LINFO, ("Start reading data from .poly files."));
  size_t prevIndex = 0;
  for (auto const & file : files)
  {
    auto const fullPath = base::JoinPath(bordersDir, file);
    size_t polygonId = 1;

    std::vector<m2::RegionD> borders;
    borders::LoadBorders(fullPath, borders);
    for (auto const & region : borders)
    {
      Polygon polygon;
      // Some mwms have several polygons. For example, for Japan_Kanto_Tokyo that has 2 polygons we
      // will write 2 files:
      // Japan_Kanto_Tokyo.poly1
      // Japan_Kanto_Tokyo.poly2
      auto const fileCopy = file + std::to_string(polygonId);

      m_indexToPolyFileName[prevIndex] = fileCopy;
      m_polyFileNameToIndex[fileCopy] = prevIndex++;
      for (auto const & point : region.Data())
        polygon.m_points.emplace_back(point);

      polygon.m_rect = region.GetRect();
      ++polygonId;
      m_bordersPolygons.emplace_back(std::move(polygon));
    }
  }

  m_duplicatedPointsCount += RemoveDuplicatePoints();
  LOG(LINFO, ("Removed:", m_duplicatedPointsCount, "from input data."));
}

void BordersData::MarkPoints()
{
  size_t const threadsNumber = std::thread::hardware_concurrency();
  LOG(LINFO, ("Start marking points, threads number:", threadsNumber));

  base::ComputationalThreadPool threadPool(threadsNumber);

  std::vector<std::future<void>> tasks;
  for (size_t i = 0; i < m_bordersPolygons.size(); ++i)
  {
    Processor processor(*this);
    tasks.emplace_back(threadPool.Submit(processor, i));
  }

  for (auto & task : tasks)
    task.wait();
}

void BordersData::DumpPolyFiles(std::string const & targetDir)
{
  size_t n = m_bordersPolygons.size();
  for (size_t i = 0; i < n; )
  {
    // Russia_Moscow.poly1 -> Russia_Moscow.poly
    auto name = RemoveIndexFromMwmName(m_indexToPolyFileName.at(i));

    size_t j = i + 1;
    while (j < n && name == RemoveIndexFromMwmName(m_indexToPolyFileName.at(j)))
      ++j;

    std::vector<m2::RegionD> regions;
    for (; i < j; i++)
    {
      if (ShouldLog(i, n))
        LOG(LINFO, ("Dumping poly files:", i + 1, "/", n));

      m2::RegionD region;
      for (auto const & markedPoint : m_bordersPolygons[i].m_points)
        region.AddPoint(markedPoint.m_point);

      regions.emplace_back(std::move(region));
    }

    CHECK(strings::ReplaceFirst(name, kBorderExtension, ""), (name));
    borders::DumpBorderToPolyFile(targetDir, name, regions);
  }
}

size_t BordersData::RemoveDuplicatePoints()
{
  size_t count = 0;

  auto const pointsAreEqual = [](auto const & p1, auto const & p2) {
    return AlmostEqualAbs(p1.m_point, p2.m_point, kEqualityEpsilon);
  };

  for (auto & polygon : m_bordersPolygons)
  {
    auto & points = polygon.m_points;
    auto const last = std::unique(points.begin(), points.end(), pointsAreEqual);

    count += std::distance(last, points.end());
    points.erase(last, points.end());

    if (polygon.m_points.begin() == polygon.m_points.end())
      continue;

    while (points.size() > 1 && pointsAreEqual(points.front(), points.back()))
    {
      ++count;
      points.pop_back();
    }
  }

  return count;
}

void BordersData::MarkPoint(size_t curBorderId, size_t curPointId)
{
  MarkedPoint & curMarkedPoint = m_bordersPolygons[curBorderId].m_points[curPointId];

  for (size_t anotherBorderId = 0; anotherBorderId < m_bordersPolygons.size(); ++anotherBorderId)
  {
    if (curBorderId == anotherBorderId)
      continue;

    if (curMarkedPoint.m_marked)
      return;

    Polygon & anotherPolygon = m_bordersPolygons[anotherBorderId];

    if (!anotherPolygon.m_rect.IsPointInside(curMarkedPoint.m_point))
      continue;

    for (size_t anotherPointId = 0; anotherPointId < anotherPolygon.m_points.size(); ++anotherPointId)
    {
      auto & anotherMarkedPoint = anotherPolygon.m_points[anotherPointId];

      if (AlmostEqualAbs(anotherMarkedPoint.m_point, curMarkedPoint.m_point, kEqualityEpsilon))
      {
        anotherMarkedPoint.m_marked = true;
        curMarkedPoint.m_marked = true;

        // Save info that border with id: |anotherBorderId| has the same point with id:
        // |anotherPointId|.
        curMarkedPoint.AddLink(anotherBorderId, anotherPointId);
        // And vice versa.
        anotherMarkedPoint.AddLink(curBorderId, curPointId);

        return;
      }
    }
  }
}

void BordersData::PrintDiff()
{
  using Info = std::tuple<double, std::string, size_t, size_t>;

  std::set<Info> info;

  size_t allNumberBeforeCount = 0;
  size_t maxMwmNameLength = 0;
  for (size_t i = 0; i < m_bordersPolygons.size(); ++i)
  {
    auto const & mwmName = m_indexToPolyFileName[i];

    auto const all = static_cast<int32_t>(m_bordersPolygons[i].m_points.size());
    auto const allBefore = static_cast<int32_t>(m_prevCopy[i].m_points.size());

    CHECK_GREATER_OR_EQUAL(allBefore, all, ());
    m_removedPointsCount += allBefore - all;
    allNumberBeforeCount += allBefore;

    double area = 0.0;
    double constexpr kAreaEpsMetersSqr = 1e-4;
    if (m_additionalAreaMetersSqr[i] >= kAreaEpsMetersSqr)
      area = m_additionalAreaMetersSqr[i];

    maxMwmNameLength = std::max(maxMwmNameLength, mwmName.size());
    info.emplace(area, mwmName, allBefore, all);
  }

  for (auto const & [area, name, allBefore, all] : info)
  {
    size_t diff = allBefore - all;
    PrintWithSpaces(name, maxMwmNameLength + 1);
    PrintWithSpaces("-" + std::to_string(diff) + " points", 17);

    std::cout << " total changed area: " << area << " m^2" << std::endl;
  }

  CHECK_NOT_EQUAL(allNumberBeforeCount, 0, ("Empty input?"));
  std::cout << "Number of removed points: " << m_removedPointsCount << std::endl
            << "Removed duplicate point: " << m_duplicatedPointsCount << std::endl
            << "Total removed points: " << m_removedPointsCount + m_duplicatedPointsCount
            << std::endl;
  std::cout << "Points number before processing: " << allNumberBeforeCount << ", remove( "
            << static_cast<double>(m_removedPointsCount + m_duplicatedPointsCount) /
                   allNumberBeforeCount * 100.0
            << "% )" << std::endl;
}

void BordersData::RemoveEmptySpaceBetweenBorders()
{
  LOG(LINFO, ("Start removing empty space between borders."));

  for (size_t curBorderId = 0; curBorderId < m_bordersPolygons.size(); ++curBorderId)
  {
    LOG(LDEBUG, ("Get:", m_indexToPolyFileName[curBorderId]));

    if (ShouldLog(curBorderId, m_bordersPolygons.size()))
      LOG(LINFO, ("Removing empty spaces:", curBorderId + 1, "/", m_bordersPolygons.size()));

    auto & curPolygon = m_bordersPolygons[curBorderId];
    for (size_t curPointId = 0; curPointId < curPolygon.m_points.size(); ++curPointId)
    {
      if (curPolygon.IsFrozen(curPointId, curPointId) || !HasLinkAt(curBorderId, curPointId))
        continue;

      size_t constexpr kMaxLookAhead = 5;
      for (size_t shift = 1; shift <= kMaxLookAhead; ++shift)
      {
        if (TryToReplace(curBorderId, curPointId /* curLeftPointId */,
                         curPointId + shift /* curRightPointId */) == base::ControlFlow::Break)
        {
          break;
        }
      }
    }
  }

  DoReplace();
}

base::ControlFlow BordersData::TryToReplace(size_t curBorderId, size_t & curLeftPointId,
                                            size_t curRightPointId)
{
  auto & curPolygon = m_bordersPolygons[curBorderId];
  if (curRightPointId >= curPolygon.m_points.size())
    return base::ControlFlow::Break;

  if (curPolygon.IsFrozen(curRightPointId, curRightPointId))
  {
    curLeftPointId = curRightPointId - 1;
    return base::ControlFlow::Break;
  }

  auto & leftMarkedPoint = curPolygon.m_points[curLeftPointId];
  auto & rightMarkedPoint = curPolygon.m_points[curRightPointId];

  auto op = rightMarkedPoint.GetLink(curBorderId);
  if (!op)
    return base::ControlFlow::Continue;

  Link rightLink = *op;
  Link leftLink = *leftMarkedPoint.GetLink(curBorderId);

  if (leftLink.m_borderId != rightLink.m_borderId)
    return base::ControlFlow::Continue;

  auto const anotherBorderId = leftLink.m_borderId;
  auto const anotherLeftPointId = leftLink.m_pointId;
  auto const anotherRightPointId = rightLink.m_pointId;
  auto & anotherPolygon = m_bordersPolygons[anotherBorderId];

  if (anotherPolygon.IsFrozen(std::min(anotherLeftPointId, anotherRightPointId),
                              std::max(anotherLeftPointId, anotherRightPointId)))
  {
    return base::ControlFlow::Continue;
  }

  auto const anotherSubpolygon =
     AppendPointsWithAnyDirection(anotherPolygon.m_points, anotherLeftPointId, anotherRightPointId);

  auto const curSubpolygon =
     AppendPointsWithAnyDirection(curPolygon.m_points, curLeftPointId, curRightPointId);

  if (!NeedReplace(curSubpolygon, anotherSubpolygon))
    return base::ControlFlow::Break;

  // We want to decrease the amount of points in polygons. So we will replace the greater amounts of
  // points by smaller amounts of points.
  bool const curLenIsLess = curSubpolygon.size() < anotherSubpolygon.size();

  size_t dstFrom = curLenIsLess ? anotherLeftPointId : curLeftPointId;
  size_t dstTo   = curLenIsLess ? anotherRightPointId : curRightPointId;

  size_t srcFrom = curLenIsLess ? curLeftPointId  : anotherLeftPointId;
  size_t srcTo   = curLenIsLess ? curRightPointId : anotherRightPointId;

  size_t const borderIdWhereAreaWillBeChanged = curLenIsLess ? anotherBorderId : curBorderId;
  size_t const srcBorderId = curLenIsLess ? curBorderId : anotherBorderId;

  bool const reversed = IsReversedIntervals(dstFrom, dstTo, srcFrom, srcTo);

  m_additionalAreaMetersSqr[borderIdWhereAreaWillBeChanged] +=
      AbsAreaDiff(curSubpolygon, anotherSubpolygon);

  SwapIfNeeded(dstFrom, dstTo);
  SwapIfNeeded(srcFrom, srcTo);

  // Save info for |borderIdWhereAreaWillBeChanged| - where from it should gets info about
  // replacement.
  m_bordersPolygons[borderIdWhereAreaWillBeChanged].AddReplaceInfo(
      dstFrom, dstTo, srcFrom, srcTo, srcBorderId, reversed);

  // And say for |srcBorderId| that points in segment: [srcFrom, srcTo] are frozen and cannot
  // be used anywhere (because we use them to replace points in segment: [dstFrom, dstTo]).
  m_bordersPolygons[srcBorderId].MakeFrozen(srcFrom, srcTo);

  CHECK_LESS(curLeftPointId, curRightPointId, ());
  curLeftPointId = curRightPointId - 1;
  return base::ControlFlow::Break;
}

void BordersData::DoReplace()
{
  LOG(LINFO, ("Start replacing intervals."));

  std::vector<Polygon> newMwmsPolygons;

  for (size_t borderId = 0; borderId < m_bordersPolygons.size(); ++borderId)
  {
    if (ShouldLog(borderId, m_bordersPolygons.size()))
      LOG(LINFO, ("Replacing:", borderId + 1, "/", m_bordersPolygons.size()));

    auto & polygon = m_bordersPolygons[borderId];

    newMwmsPolygons.emplace_back();
    auto & newPolygon = newMwmsPolygons.back();

    for (size_t i = 0; i < polygon.m_points.size(); ++i)
    {
      auto const replaceDataIter = polygon.FindReplaceData(i);
      bool const noReplaceData = replaceDataIter == polygon.m_replaceData.cend();
      if (noReplaceData)
      {
        newPolygon.m_points.emplace_back(polygon.m_points[i].m_point);
        continue;
      }

      auto const srcBorderId = replaceDataIter->m_srcBorderId;
      size_t const srcFrom = replaceDataIter->m_srcReplaceFrom;
      size_t const srcTo = replaceDataIter->m_srcReplaceTo;
      size_t const nextI = replaceDataIter->m_dstTo;
      bool const reversed = replaceDataIter->m_reversed;

      CHECK_EQUAL(i, replaceDataIter->m_dstFrom, ());

      auto const & srcPolygon = m_bordersPolygons[srcBorderId];

      Append(srcFrom, srcTo, reversed, srcPolygon.m_points, newPolygon.m_points);

      polygon.m_replaceData.erase(replaceDataIter);
      i = nextI - 1;
    }

    newPolygon.m_rect = BoundingBox(newPolygon.m_points);
  }

  m_prevCopy = std::move(m_bordersPolygons);
  m_bordersPolygons = std::move(newMwmsPolygons);
  RemoveDuplicatePoints();
}

Polygon const & BordersData::GetBordersPolygonByName(std::string const & name) const
{
  auto id = m_polyFileNameToIndex.at(name);
  return m_bordersPolygons.at(id);
}

bool BordersData::HasLinkAt(size_t curBorderId, size_t pointId)
{
  auto & leftMarkedPoint = m_bordersPolygons[curBorderId].m_points[pointId];
  return leftMarkedPoint.GetLink(curBorderId).has_value();
}
}  // namespace poly_borders
