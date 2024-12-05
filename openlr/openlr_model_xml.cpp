#include "openlr/openlr_model_xml.hpp"
#include "openlr/openlr_model.hpp"

#include "geometry/mercator.hpp"

#include "base/logging.hpp"

#include <cstring>
#include <optional>
#include <type_traits>

#include <pugixml.hpp>

using namespace std;

namespace  // Primitive utilities to handle simple OpenLR-like XML data.
{
template <typename Value>
bool IntegerFromXML(pugi::xml_node const & node, Value & value)
{
  if (!node)
    return false;
  value = static_cast<Value>(is_signed<Value>::value
                             ? node.text().as_int()
                             : node.text().as_uint());
  return true;
}

std::optional<double> DoubleFromXML(pugi::xml_node const & node)
{
  return node ? node.text().as_double() : std::optional<double>();
}

bool GetLatLon(pugi::xml_node const & node, int32_t & lat, int32_t & lon)
{
  if (!IntegerFromXML(node.child("olr:latitude"), lat) ||
      !IntegerFromXML(node.child("olr:longitude"), lon))
  {
    return false;
  }

  return true;
}

// This helper is used to parse records like this:
// <olr:lfrcnp olr:table="olr001_FunctionalRoadClass" olr:code="4"/>
template <typename Value>
bool TableValueFromXML(pugi::xml_node const & node, Value & value)
{
  if (!node)
    return false;
  value = static_cast<Value>(is_signed<Value>::value
                             ? node.attribute("olr:code").as_int()
                             : node.attribute("olr:code").as_uint());
  return true;
}

pugi::xml_node GetLinearLocationReference(pugi::xml_node const & node)
{
  // There should be only one linear location reference child of a location reference.
  return node.select_node(".//olr:locationReference/olr:optionLinearLocationReference").node();
}

pugi::xml_node GetCoordinates(pugi::xml_node const & node)
{
  return node.select_node(".//coordinates").node();
}


bool IsLocationReferenceTag(pugi::xml_node const & node)
{
  return node.select_node(".//olr:locationReference").node();
}

bool IsCoordinatesTag(pugi::xml_node const & node)
{
  return node.select_node("coordinates").node();
}

// This helper is used do deal with xml nodes of the form
// <node>
//   <value>integer<value>
// </node>
template <typename Value>
bool ValueFromXML(pugi::xml_node const & node, Value & value)
{
  auto const valueNode = node.child("olr:value");
  return IntegerFromXML(valueNode, value);
}

template <typename Value>
bool ParseValueIfExists(pugi::xml_node const & node, Value & value)
{
  if (!node)
    return true;
  return ValueFromXML(node, value);
}
}  // namespace

namespace  // OpenLR tools and abstractions
{
bool FirstCoordinateFromXML(pugi::xml_node const & node, ms::LatLon & latLon)
{
  int32_t lat, lon;
  if (!GetLatLon(node.child("olr:coordinate"), lat, lon))
    return false;

  latLon.m_lat = ((lat - math::Sign(lat) * 0.5) * 360) / (1 << 24);
  latLon.m_lon = ((lon - math::Sign(lon) * 0.5) * 360) / (1 << 24);

  return true;
}

std::optional<ms::LatLon> LatLonFormXML(pugi::xml_node const & node)
{
  auto const lat = DoubleFromXML(node.child("latitude"));
  auto const lon = DoubleFromXML(node.child("longitude"));

  return lat && lon ? ms::LatLon(*lat, *lon) : ms::LatLon::Zero();
}

bool CoordinateFromXML(pugi::xml_node const & node, ms::LatLon const & prevCoord,
                       ms::LatLon & latLon)
{
  int32_t lat, lon;
  if (!GetLatLon(node.child("olr:coordinate"), lat, lon))
    return false;

  // This constant is provided by the given OpenLR variant
  // with no special meaning and is used as a factor to store doubles as ints.
  auto const kOpenlrDeltaFactor = 100000;

  latLon.m_lat = prevCoord.m_lat + static_cast<double>(lat) / kOpenlrDeltaFactor;
  latLon.m_lon = prevCoord.m_lon + static_cast<double>(lon) / kOpenlrDeltaFactor;

  return true;
}

bool LinePropertiesFromXML(pugi::xml_node const & linePropNode,
                           openlr::LocationReferencePoint & locPoint)
{
  if (!linePropNode)
  {
    LOG(LERROR, ("linePropNode is NULL"));
    return false;
  }

  if (!TableValueFromXML(linePropNode.child("olr:frc"), locPoint.m_functionalRoadClass))
  {
    LOG(LERROR, ("Can't parse functional road class"));
    return false;
  }

  if (!TableValueFromXML(linePropNode.child("olr:fow"), locPoint.m_formOfWay))
  {
    LOG(LERROR, ("Can't parse form of a way"));
    return false;
  }

  if (!ValueFromXML(linePropNode.child("olr:bearing"), locPoint.m_bearing))
  {
    LOG(LERROR, ("Can't parse bearing"));
    return false;
  }

  return true;
}

bool PathPropertiesFromXML(pugi::xml_node const & locPointNode,
                           openlr::LocationReferencePoint & locPoint)
{
  // Last point does not contain path properties.
  if (strcmp(locPointNode.name(), "olr:last") == 0)
    return true;

  auto const propNode = locPointNode.child("olr:pathProperties");
  if (!propNode)
  {
    LOG(LERROR, ("Can't parse path properties"));
    return false;
  }

  if (!ValueFromXML(propNode.child("olr:dnp"), locPoint.m_distanceToNextPoint))
  {
    LOG(LERROR, ("Can't parse dnp"));
    return false;
  }

  if (!TableValueFromXML(propNode.child("olr:lfrcnp"), locPoint.m_lfrcnp))
  {
    LOG(LERROR, ("Can't parse lfrcnp"));
    return false;
  }

  auto const directionNode = propNode.child("olr:againstDrivingDirection");
  if (!directionNode)
  {
    LOG(LERROR, ("Can't parse driving direction"));
    return false;
  }
  locPoint.m_againstDrivingDirection = directionNode.text().as_bool();

  return true;
}

bool LocationReferencePointFromXML(pugi::xml_node const & locPointNode,
                                   openlr::LocationReferencePoint & locPoint)
{
  if (!FirstCoordinateFromXML(locPointNode, locPoint.m_latLon))
  {
    LOG(LERROR, ("Can't get first coordinate"));
    return false;
  }

  return LinePropertiesFromXML(locPointNode.child("olr:lineProperties"), locPoint) &&
         PathPropertiesFromXML(locPointNode, locPoint);
}

bool LocationReferencePointFromXML(pugi::xml_node const & locPointNode,
                                   ms::LatLon const & firstPoint,
                                   openlr::LocationReferencePoint & locPoint)
{
  if (!CoordinateFromXML(locPointNode, firstPoint, locPoint.m_latLon))
  {
    LOG(LERROR, ("Can't get last coordinate"));
    return false;
  }

  return LinePropertiesFromXML(locPointNode.child("olr:lineProperties"), locPoint) &&
         PathPropertiesFromXML(locPointNode, locPoint);
}

bool LinearLocationReferenceFromXML(pugi::xml_node const & locRefNode,
                                    openlr::LinearLocationReference & locRef)
{
  if (!locRefNode)
  {
    LOG(LERROR, ("Can't get location reference."));
    return false;
  }

  {
    openlr::LocationReferencePoint point;
    if (!LocationReferencePointFromXML(locRefNode.child("olr:first"), point))
      return false;
    locRef.m_points.push_back(point);
  }

  for (auto const pointNode : locRefNode.select_nodes("olr:intermediates"))
  {
    openlr::LocationReferencePoint point;
    if (!LocationReferencePointFromXML(pointNode.node(), locRef.m_points.back().m_latLon, point))
      return false;
    locRef.m_points.push_back(point);
  }

  {
    openlr::LocationReferencePoint point;
    if (!LocationReferencePointFromXML(locRefNode.child("olr:last"),
                                       locRef.m_points.back().m_latLon, point))
      return false;
    locRef.m_points.push_back(point);
  }

  if (!ParseValueIfExists(locRefNode.child("olr:positiveOffset"), locRef.m_positiveOffsetMeters))
  {
    LOG(LERROR, ("Can't parse positive offset"));
    return false;
  }

  if(!ParseValueIfExists(locRefNode.child("olr:negativeOffset"), locRef.m_negativeOffsetMeters))
  {
    LOG(LERROR, ("Can't parse negative offset"));
    return false;
  }

  return true;
}

bool CoordinatesFromXML(pugi::xml_node const & coordsNode, openlr::LinearLocationReference & locRef)
{
  if (!coordsNode)
  {
    LOG(LERROR, ("Can't get <coordinates>."));
    return false;
  }

  auto const latLonStart = LatLonFormXML(coordsNode.child("start"));
  auto const latLonEnd = LatLonFormXML(coordsNode.child("end"));
  if (!latLonStart || !latLonEnd)
  {
    LOG(LERROR, ("Can't get <start> or <end> of <coordinates>."));
    return false;
  }

  LOG(LINFO, ("from:", *latLonStart, "to:", *latLonEnd));
  locRef.m_points.resize(2);
  locRef.m_points[0].m_latLon = *latLonStart;
  locRef.m_points[1].m_latLon = *latLonEnd;
  return true;
}
}  // namespace

namespace openlr
{
bool ParseOpenlr(pugi::xml_document const & document, vector<LinearSegment> & segments)
{
  for (auto const segmentXpathNode : document.select_nodes("//reportSegments"))
  {
    LinearSegment segment;
    auto const & node = segmentXpathNode.node();
    if (!IsLocationReferenceTag(node) && !IsCoordinatesTag(node))
    {
      LOG(LWARNING, ("A segment with a strange tag. It is not <coordinates>"
                     " or <optionLinearLocationReference>, skipping..."));
      continue;
    }

    if (!SegmentFromXML(node, segment))
      return false;

    segments.push_back(segment);
  }
  return true;
}

bool SegmentFromXML(pugi::xml_node const & segmentNode, LinearSegment & segment)
{
  CHECK(segmentNode, ());
  if (!IntegerFromXML(segmentNode.child("ReportSegmentID"), segment.m_segmentId))
  {
    LOG(LERROR, ("Can't parse segment id"));
    return false;
  }

  if (!IntegerFromXML(segmentNode.child("segmentLength"), segment.m_segmentLengthMeters))
  {
    LOG(LERROR, ("Can't parse segment length"));
    return false;
  }

  if (IsLocationReferenceTag(segmentNode))
  {
    auto const locRefNode = GetLinearLocationReference(segmentNode);
    auto const result = LinearLocationReferenceFromXML(locRefNode, segment.m_locationReference);
    if (result)
      segment.m_source = LinearSegmentSource::FromLocationReferenceTag;

    return result;
  }

  CHECK(IsCoordinatesTag(segmentNode), ());
  auto const coordsNode = GetCoordinates(segmentNode);
  if (!CoordinatesFromXML(coordsNode, segment.m_locationReference))
    return false;

  CHECK_EQUAL(segment.m_locationReference.m_points.size(), 2, ());
  segment.m_locationReference.m_points[0].m_distanceToNextPoint = segment.m_segmentLengthMeters;
  segment.m_source = LinearSegmentSource::FromCoordinatesTag;
  return true;
}
}  // namespace openlr
