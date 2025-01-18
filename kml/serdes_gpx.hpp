#pragma once

#include "kml/types.hpp"

#include "coding/parse_xml.hpp"
#include "coding/reader.hpp"
#include "coding/writer.hpp"

#include "geometry/point_with_altitude.hpp"

#include "base/exception.hpp"

#include <string>

namespace kml
{
namespace gpx
{

class GpxWriter
{
public:
  DECLARE_EXCEPTION(WriteGpxException, RootException);

  explicit GpxWriter(Writer & writer)
    : m_writer(writer)
  {}

  void Write(FileData const & fileData);

private:
  Writer & m_writer;
};

class SerializerGpx
{
public:
  DECLARE_EXCEPTION(SerializeException, RootException);

  explicit SerializerGpx(FileData const & fileData)
    : m_fileData(fileData)
  {}

  template <typename Sink>
  void Serialize(Sink & sink)
  {
    GpxWriter gpxWriter(sink);
    gpxWriter.Write(m_fileData);
  }

private:
  FileData const & m_fileData;
};

class GpxParser
{
public:
  explicit GpxParser(FileData & data);
  bool Push(std::string name);
  void AddAttr(std::string_view attr, char const * value);
  std::string const & GetTagFromEnd(size_t n) const;
  void Pop(std::string_view tag);
  void CharData(std::string & value);

private:
  enum GeometryType
  {
    GEOMETRY_TYPE_UNKNOWN,
    GEOMETRY_TYPE_POINT,
    GEOMETRY_TYPE_LINE
  };

  void ResetPoint();
  bool MakeValid();
  void ParseColor(std::string_view value);
  void ParseGarminColor(std::string const & value);
  void ParseOsmandColor(std::string const & value);
  bool IsValidCoordinatesPosition() const;
  void CheckAndCorrectTimestamps();

  FileData & m_data;
  CategoryData m_compilationData;
  CategoryData * m_categoryData;  // never null

  std::vector<std::string> m_tags;
  GeometryType m_geometryType;
  MultiGeometry m_geometry;
  uint32_t m_color;
  uint32_t m_globalColor; // To support OSMAnd extensions with single color per GPX file

  std::string m_name;
  std::string m_description;
  std::string m_comment;
  PredefinedColor m_predefinedColor;
  geometry::PointWithAltitude m_org;

  double m_lat;
  double m_lon;
  geometry::Altitude m_altitude;
  time_t m_timestamp;

  MultiGeometry::LineT m_line;
  MultiGeometry::TimeT m_timestamps;
  std::string m_customName;
  void ParseName(std::string const & value, std::string const & prevTag);
  void ParseDescription(std::string const & value, std::string const & prevTag);
  void ParseAltitude(std::string const & value);
  void ParseTimestamp(std::string const & value);
  std::string BuildDescription() const;
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
