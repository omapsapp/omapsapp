#pragma once

#include "kml/types.hpp"
#include "kml/serdes_common.hpp"

#include "coding/parse_xml.hpp"
#include "coding/reader.hpp"
#include "coding/writer.hpp"

#include "geometry/point2d.hpp"
#include "geometry/point_with_altitude.hpp"

#include "base/exception.hpp"
#include "base/stl_helpers.hpp"

#include <chrono>
#include <string>

namespace kml
{
namespace gpx
{
class GpxParser
{
public:
  explicit GpxParser(FileData & data);
  bool Push(std::string_view name);
  void AddAttr(std::string const & attr, std::string const & value);
  std::string_view GetTagFromEnd(size_t n) const;
  void Pop(std::string_view tag);
  void CharData(std::string value);

private:
  enum GeometryType
  {
    GEOMETRY_TYPE_UNKNOWN,
    GEOMETRY_TYPE_POINT,
    GEOMETRY_TYPE_LINE
  };

  void ResetPoint();
  bool MakeValid();
  void ParseColor(std::string const & value);

  FileData & m_data;
  CategoryData m_compilationData;
  CategoryData * m_categoryData;  // never null

  std::vector<std::string> m_tags;
  GeometryType m_geometryType;
  MultiGeometry m_geometry;
  uint32_t m_color;

  LocalizableString m_name;
  LocalizableString m_description;
  PredefinedColor m_predefinedColor;
  m2::PointD m_org;

  double m_lat;
  double m_lon;

  MultiGeometry::LineT m_line;
  LocalizableString m_customName;
  std::vector<TrackLayer> m_trackLayers;
};
}  // namespace gpx

class DeserializerGpx
{
public:
  DECLARE_EXCEPTION(DeserializeException, RootException);
  
  explicit DeserializerGpx(FileData & fileData);
  
  template <typename ReaderType>
  void Deserialize(ReaderType const & reader)
  {
    NonOwningReaderSource src(reader);
    
    gpx::GpxParser parser(m_fileData);
    if (!ParseXML(src, parser, true))
    {
      // Print corrupted GPX file for debug and restore purposes.
      std::string gpxText;
      reader.ReadAsString(gpxText);
      if (!gpxText.empty() && gpxText[0] == '<')
        LOG(LWARNING, (gpxText));
      MYTHROW(DeserializeException, ("Could not parse GPX."));
    }
  }
  
private:
  FileData & m_fileData;
};
}  // namespace kml
