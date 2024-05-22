#include "traffic_mode.hpp"

#include "openlr/openlr_model_xml.hpp"

#include "indexer/data_source.hpp"

#include "base/assert.hpp"
#include "base/scope_guard.hpp"

#include <QItemSelection>
#include <QMessageBox>

namespace openlr
{
namespace
{
void RemovePointFromPull(m2::PointD const & toBeRemoved, std::vector<m2::PointD> & pool)
{
  pool.erase(
      remove_if(begin(pool), end(pool),
                [&toBeRemoved](m2::PointD const & p) { return p.EqualDxDy(toBeRemoved, 1e-6); }),
      end(pool));
}

std::vector<m2::PointD> GetReachablePoints(m2::PointD const & srcPoint,
                                           std::vector<m2::PointD> const path,
                                           PointsControllerDelegateBase const & pointsDelegate,
                                           size_t const lookbackIndex)
{
  auto reachablePoints = pointsDelegate.GetReachablePoints(srcPoint);
  if (lookbackIndex < path.size())
  {
    auto const & toBeRemoved = path[path.size() - lookbackIndex - 1];
    RemovePointFromPull(toBeRemoved, reachablePoints);
  }
  return reachablePoints;
}
}  // namespace

namespace impl
{
// static
size_t const RoadPointCandidate::kInvalidId = std::numeric_limits<size_t>::max();

/// This class denotes a "non-deterministic" feature point.
/// I.e. it is a set of all pairs <FeatureID, point index>
/// located at a specified coordinate.
/// Only one point at a time is considered active.
RoadPointCandidate::RoadPointCandidate(std::vector<FeaturePoint> const & points,
                                       m2::PointD const & coord)
  : m_coord(coord)
  , m_points(points)
{
  LOG(LDEBUG, ("Candidate points:", points));
}

void RoadPointCandidate::ActivateCommonPoint(RoadPointCandidate const & rpc)
{
  for (auto const & fp1 : m_points)
  {
    for (auto const & fp2 : rpc.m_points)
    {
      if (fp1.first == fp2.first)
      {
        SetActivePoint(fp1.first);
        return;
      }
    }
  }
  CHECK(false, ("One common feature id should exist."));
}

FeaturePoint const & RoadPointCandidate::GetPoint() const
{
  CHECK_NOT_EQUAL(m_activePointIndex, kInvalidId, ("No point is active."));
  return m_points[m_activePointIndex];
}

m2::PointD const & RoadPointCandidate::GetCoordinate() const
{
  return m_coord;
}

void RoadPointCandidate::SetActivePoint(FeatureID const & fid)
{
  for (size_t i = 0; i < m_points.size(); ++i)
  {
    if (m_points[i].first == fid)
    {
      m_activePointIndex = i;
      return;
    }
  }
  CHECK(false, ("One point should match."));
}
}  // namespace impl

// TrafficMode -------------------------------------------------------------------------------------
TrafficMode::TrafficMode(std::string const & dataFileName, DataSource const & dataSource,
                         std::unique_ptr<TrafficDrawerDelegateBase> drawerDelegate,
                         std::unique_ptr<PointsControllerDelegateBase> pointsDelegate,
                         QObject * parent)
  : QAbstractTableModel(parent)
  , m_dataSource(dataSource)
  , m_drawerDelegate(std::move(drawerDelegate))
  , m_pointsDelegate(std::move(pointsDelegate))
{
  // TODO(mgsergio): Collect stat how many segments of each kind were parsed.
  pugi::xml_document doc;
  if (!doc.load_file(dataFileName.data()))
    MYTHROW(TrafficModeError, ("Can't load file:", strerror(errno)));

  // Save root node without children.
  {
    auto const root = doc.document_element();
    auto node = m_template.append_child(root.name());
    for (auto const & attr : root.attributes())
      node.append_copy(attr);
  }

  // Select all Segment elements that are direct children of the root.
  auto const segments = doc.document_element().select_nodes("./Segment");

  try
  {
    for (auto const & xpathNode : segments)
    {
      auto const xmlSegment = xpathNode.node();

      openlr::Path matchedPath;
      openlr::Path fakePath;
      openlr::Path goldenPath;

      openlr::LinearSegment segment;

      // TODO(mgsergio): Unify error handling interface of openlr_xml_mode and decoded_path parsers.
      auto const partnerSegmentXML = xmlSegment.child("reportSegments");
      if (!openlr::SegmentFromXML(partnerSegmentXML, segment))
        MYTHROW(TrafficModeError, ("An error occurred while parsing: can't parse segment"));

      if (auto const route = xmlSegment.child("Route"))
        openlr::PathFromXML(route, m_dataSource, matchedPath);
      if (auto const route = xmlSegment.child("FakeRoute"))
        openlr::PathFromXML(route, m_dataSource, fakePath);
      if (auto const route = xmlSegment.child("GoldenRoute"))
        openlr::PathFromXML(route, m_dataSource, goldenPath);

      uint32_t positiveOffsetM = 0;
      uint32_t negativeOffsetM = 0;
      if (auto const reportSegmentLRC = partnerSegmentXML.child("ReportSegmentLRC"))
      {
        if (auto const method = reportSegmentLRC.child("method"))
        {
          if (auto const locationReference = method.child("olr:locationReference"))
          {
            if (auto const optionLinearLocationReference = locationReference
                .child("olr:optionLinearLocationReference"))
            {
              if (auto const positiveOffset = optionLinearLocationReference.child("olr:positiveOffset"))
                positiveOffsetM = UintValueFromXML(positiveOffset);

              if (auto const negativeOffset = optionLinearLocationReference.child("olr:negativeOffset"))
                negativeOffsetM = UintValueFromXML(negativeOffset);
            }
          }
        }
      }

      m_segments.emplace_back(segment, positiveOffsetM, negativeOffsetM, matchedPath, fakePath,
                              goldenPath, partnerSegmentXML);
      if (auto const status = xmlSegment.child("Ignored"))
      {
        if (status.text().as_bool())
          m_segments.back().Ignore();
      }
    }
  }
  catch (openlr::DecodedPathLoadError const & e)
  {
    MYTHROW(TrafficModeError, ("An exception occurred while parsing", dataFileName, e.Msg()));
  }

  LOG(LINFO, (segments.size(), "segments are loaded."));
}

// TODO(mgsergio): Check if a path was committed, or commit it.
bool TrafficMode::SaveSampleAs(std::string const & fileName) const
{
  CHECK(!fileName.empty(), ("Can't save to an empty file."));

  pugi::xml_document result;
  result.reset(m_template);
  auto root = result.document_element();

  for (auto const & sc : m_segments)
  {
    auto segment = root.append_child("Segment");
    segment.append_copy(sc.GetPartnerXMLSegment());

    if (sc.GetStatus() == SegmentCorrespondence::Status::Ignored)
    {
      segment.append_child("Ignored").text() = true;
    }
    if (sc.HasMatchedPath())
    {
      auto node = segment.append_child("Route");
      openlr::PathToXML(sc.GetMatchedPath(), node);
    }
    if (sc.HasFakePath())
    {
      auto node = segment.append_child("FakeRoute");
      openlr::PathToXML(sc.GetFakePath(), node);
    }
    if (sc.HasGoldenPath())
    {
      auto node = segment.append_child("GoldenRoute");
      openlr::PathToXML(sc.GetGoldenPath(), node);
    }
  }

  result.save_file(fileName.data(), "  " /* indent */);
  return true;
}

int TrafficMode::rowCount(const QModelIndex & /* parent */) const
{
  return static_cast<int>(m_segments.size());
}

int TrafficMode::columnCount(const QModelIndex & /* parent */) const { return 4; }

QVariant TrafficMode::data(const QModelIndex & index, int role) const
{
  if (!index.isValid())
    return QVariant();

  if (index.row() >= rowCount())
    return QVariant();

  if (role != Qt::DisplayRole && role != Qt::EditRole)
    return QVariant();

  if (index.column() == 0)
    return m_segments[index.row()].GetPartnerSegmentId();

  if (index.column() == 1)
    return static_cast<int>(m_segments[index.row()].GetStatus());

  if (index.column() == 2)
    return m_segments[index.row()].GetPositiveOffset();

  if (index.column() == 3)
    return m_segments[index.row()].GetNegativeOffset();

  return QVariant();
}

QVariant TrafficMode::headerData(int section, Qt::Orientation orientation,
                                 int role /* = Qt::DisplayRole */) const
{
  if (orientation != Qt::Horizontal && role != Qt::DisplayRole)
    return QVariant();

  switch (section)
  {
  case 0: return "Segment id"; break;
  case 1: return "Status code"; break;
  case 2: return "Positive offset (Meters)"; break;
  case 3: return "Negative offset (Meters)"; break;
  }
  UNREACHABLE();
}

void TrafficMode::OnItemSelected(QItemSelection const & selected, QItemSelection const &)
{
  ASSERT(!selected.empty(), ());
  ASSERT(!m_segments.empty(), ());

  auto const row = selected.front().top();

  CHECK_LESS(static_cast<size_t>(row), m_segments.size(), ());
  m_currentSegment = &m_segments[row];

  auto const & partnerSegment = m_currentSegment->GetPartnerSegment();
  auto const & partnerSegmentPoints = partnerSegment.GetMercatorPoints();
  auto const & viewportCenter = partnerSegmentPoints.front();

  m_drawerDelegate->ClearAllPaths();
  // TODO(mgsergio): Use a better way to set viewport and scale.
  m_drawerDelegate->SetViewportCenter(viewportCenter);
  m_drawerDelegate->DrawEncodedSegment(partnerSegmentPoints);
  if (m_currentSegment->HasMatchedPath())
    m_drawerDelegate->DrawDecodedSegments(GetPoints(m_currentSegment->GetMatchedPath()));
  if (m_currentSegment->HasGoldenPath())
    m_drawerDelegate->DrawGoldenPath(GetPoints(m_currentSegment->GetGoldenPath()));

  emit SegmentSelected(static_cast<int>(partnerSegment.m_segmentId));
}

Qt::ItemFlags TrafficMode::flags(QModelIndex const & index) const
{
  if (!index.isValid())
    return Qt::ItemIsEnabled;

  return QAbstractItemModel::flags(index);
}

void TrafficMode::GoldifyMatchedPath()
{
  if (!m_currentSegment->HasMatchedPath())
  {
    QMessageBox::information(nullptr /* parent */, "Error",
                             "The selected segment does not have a matched path");
    return;
  }

  if (!StartBuildingPathChecks())
    return;

  m_currentSegment->SetGoldenPath(m_currentSegment->GetMatchedPath());
  m_goldenPath.clear();
  m_drawerDelegate->DrawGoldenPath(GetPoints(m_currentSegment->GetGoldenPath()));
}

void TrafficMode::StartBuildingPath()
{
  if (!StartBuildingPathChecks())
    return;

  m_currentSegment->SetGoldenPath({});

  m_buildingPath = true;
  m_drawerDelegate->ClearGoldenPath();
  m_drawerDelegate->VisualizePoints(m_pointsDelegate->GetAllJunctionPointsInViewport());
}

void TrafficMode::PushPoint(m2::PointD const & coord, std::vector<FeaturePoint> const & points)
{
  impl::RoadPointCandidate point(points, coord);
  if (!m_goldenPath.empty())
    m_goldenPath.back().ActivateCommonPoint(point);
  m_goldenPath.push_back(point);
}

void TrafficMode::PopPoint()
{
  CHECK(!m_goldenPath.empty(), ("Attempt to pop point from an empty path."));
  m_goldenPath.pop_back();
}

void TrafficMode::CommitPath()
{
  CHECK(m_currentSegment, ("No segments selected"));

  if (!m_buildingPath)
    MYTHROW(TrafficModeError, ("Path building is not started"));

  SCOPE_GUARD(guard, [this] { emit EditingStopped(); });

  m_buildingPath = false;
  m_drawerDelegate->ClearAllVisualizedPoints();

  if (m_goldenPath.size() == 1)
  {
    LOG(LDEBUG, ("Golden path is empty"));
    return;
  }

  CHECK_GREATER(m_goldenPath.size(), 1, ("Path cannot consist of only one point"));

  // Activate last point. Since no more points will be availabe we link it to the same
  // feature as the previous one was linked to.
  m_goldenPath.back().ActivateCommonPoint(m_goldenPath[GetPointsCount() - 2]);

  openlr::Path path;
  for (size_t i = 1; i < GetPointsCount(); ++i)
  {
    auto const prevPoint = m_goldenPath[i - 1];
    auto point = m_goldenPath[i];

    // The start and the end of the edge should lie on the same feature.
    point.ActivateCommonPoint(prevPoint);

    auto const & prevFt = prevPoint.GetPoint();
    auto const & ft = point.GetPoint();

    path.push_back(Edge::MakeReal(
        ft.first, prevFt.second < ft.second /* forward */, base::checked_cast<uint32_t>(prevFt.second),
        geometry::PointWithAltitude(prevPoint.GetCoordinate(), 0 /* altitude */),
        geometry::PointWithAltitude(point.GetCoordinate(), 0 /* altitude */)));
  }

  m_currentSegment->SetGoldenPath(path);
  m_goldenPath.clear();
}

void TrafficMode::RollBackPath()
{
  CHECK(m_currentSegment, ("No segments selected"));
  CHECK(m_buildingPath, ("No path building is in progress."));

  m_buildingPath = false;

  // TODO(mgsergio): Add a method for common visual manipulations.
  m_drawerDelegate->ClearAllVisualizedPoints();
  m_drawerDelegate->ClearGoldenPath();
  if (m_currentSegment->HasGoldenPath())
    m_drawerDelegate->DrawGoldenPath(GetPoints(m_currentSegment->GetGoldenPath()));

  m_goldenPath.clear();
  emit EditingStopped();
}

void TrafficMode::IgnorePath()
{
  CHECK(m_currentSegment, ("No segments selected"));

  if (m_currentSegment->HasGoldenPath())
  {
    auto const btn =
        QMessageBox::question(nullptr /* parent */, "Override warning",
                              "The selected segment has a golden path. Do you want to discard it?");
    if (btn == QMessageBox::No)
      return;
  }

  m_buildingPath = false;

  // TODO(mgsergio): Add a method for common visual manipulations.
  m_drawerDelegate->ClearAllVisualizedPoints();
  m_drawerDelegate->ClearGoldenPath();

  m_currentSegment->Ignore();
  m_goldenPath.clear();
  emit EditingStopped();
}

size_t TrafficMode::GetPointsCount() const
{
  return m_goldenPath.size();
}

m2::PointD const & TrafficMode::GetPoint(size_t const index) const
{
  return m_goldenPath[index].GetCoordinate();
}

m2::PointD const & TrafficMode::GetLastPoint() const
{
  CHECK(!m_goldenPath.empty(), ("Attempt to get point from an empty path."));
  return m_goldenPath.back().GetCoordinate();
}

std::vector<m2::PointD> TrafficMode::GetGoldenPathPoints() const
{
  std::vector<m2::PointD> coordinates;
  for (auto const & roadPoint : m_goldenPath)
    coordinates.push_back(roadPoint.GetCoordinate());
  return coordinates;
}

// TODO(mgsergio): Draw the first point when the path size is 1.
void TrafficMode::HandlePoint(m2::PointD clickPoint, Qt::MouseButton const button)
{
  if (!m_buildingPath)
    return;

  auto const currentPathLength = GetPointsCount();
  auto const lastClickedPoint = currentPathLength != 0
      ? GetLastPoint()
      : m2::PointD::Zero();

  auto const & p = m_pointsDelegate->GetCandidatePoints(clickPoint);
  auto const & candidatePoints = p.first;
  clickPoint = p.second;
  if (candidatePoints.empty())
    return;

  auto reachablePoints = GetReachablePoints(clickPoint, GetGoldenPathPoints(), *m_pointsDelegate,
                                            0 /* lookBackIndex */);
  auto const & clickablePoints = currentPathLength != 0
      ? GetReachablePoints(lastClickedPoint, GetGoldenPathPoints(), *m_pointsDelegate,
                           1 /* lookbackIndex */)
      // TODO(mgsergio): This is not quite correct since view port can change
      // since first call to visualize points. But it's ok in general.
      : m_pointsDelegate->GetAllJunctionPointsInViewport();

  using ClickType = PointsControllerDelegateBase::ClickType;
  switch (m_pointsDelegate->CheckClick(clickPoint, lastClickedPoint, clickablePoints))
  {
  case ClickType::Add:
    // TODO(mgsergio): Think of refactoring this with if (accumulator.empty)
    // instead of pushing point first ad then removing last selection.
    PushPoint(clickPoint, candidatePoints);

    if (currentPathLength > 0)
    {
      // TODO(mgsergio): Should I remove lastClickedPoint from clickablePoints
      // as well?
      RemovePointFromPull(lastClickedPoint, reachablePoints);
      m_drawerDelegate->DrawGoldenPath(GetGoldenPathPoints());
    }

    m_drawerDelegate->ClearAllVisualizedPoints();
    m_drawerDelegate->VisualizePoints(reachablePoints);
    m_drawerDelegate->VisualizePoints({clickPoint});
    break;
  case ClickType::Remove:                       // TODO(mgsergio): Rename this case.
    if (button == Qt::MouseButton::LeftButton)  // RemovePoint
    {
      m_drawerDelegate->ClearAllVisualizedPoints();
      m_drawerDelegate->ClearGoldenPath();

      PopPoint();
      if (m_goldenPath.empty())
      {
        m_drawerDelegate->VisualizePoints(m_pointsDelegate->GetAllJunctionPointsInViewport());
      }
      else
      {
        m_drawerDelegate->VisualizePoints(GetReachablePoints(
            GetLastPoint(), GetGoldenPathPoints(), *m_pointsDelegate, 1 /* lookBackIndex */));
      }

      if (GetPointsCount() > 1)
        m_drawerDelegate->DrawGoldenPath(GetGoldenPathPoints());
    }
    else if (button == Qt::MouseButton::RightButton)
    {
      CommitPath();
    }
    break;
  case ClickType::Miss:
    // TODO(mgsergio): This situation should be handled by checking candidatePoitns.empty() above.
    // Not shure though if all cases are handled by that check.
    return;
  }
}

bool TrafficMode::StartBuildingPathChecks() const
{
  CHECK(m_currentSegment, ("A segment should be selected before path building is started."));

  if (m_buildingPath)
    MYTHROW(TrafficModeError, ("Path building already in progress."));

  if (m_currentSegment->HasGoldenPath())
  {
    auto const btn = QMessageBox::question(
        nullptr /* parent */, "Override warning",
        "The selected segment already has a golden path. Do you want to override?");
    if (btn == QMessageBox::No)
      return false;
  }

  return true;
}
}  // namespace openlr
