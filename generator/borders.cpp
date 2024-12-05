#include "generator/borders.hpp"

#include "generator/borders.hpp"

#include "platform/platform.hpp"

#include "storage/country_decl.hpp"

#include "indexer/scales.hpp"

#include "coding/files_container.hpp"
#include "coding/read_write_utils.hpp"
#include "coding/varint.hpp"

#include "geometry/mercator.hpp"
#include "geometry/parametrized_segment.hpp"
#include "geometry/simplification.hpp"

#include "base/assert.hpp"
#include "base/exception.hpp"
#include "base/file_name_utils.hpp"
#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include <cmath>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <vector>

#include "base/assert.hpp"
#include "base/string_utils.hpp"

#include "defines.hpp"

namespace borders
{
namespace
{
template <class ToDo>
void ForEachCountry(std::string const & baseDir, ToDo && toDo)
{
  std::string const bordersDir = base::JoinPath(baseDir, BORDERS_DIR);
  CHECK(Platform::IsFileExistsByFullPath(bordersDir),
        ("Cannot read borders directory", bordersDir));

  Platform::FilesList files;
  Platform::GetFilesByExt(bordersDir, BORDERS_EXTENSION, files);
  for (std::string file : files)
  {
    std::vector<m2::RegionD> polygons;
    if (LoadBorders(bordersDir + file, polygons))
    {
      base::GetNameWithoutExt(file);
      toDo(file, polygons);
    }
  }
}

class PackedBordersGenerator
{
public:
  explicit PackedBordersGenerator(std::string const & baseDir) : m_writer(baseDir + PACKED_POLYGONS_FILE)
  {
  }

  void operator()(std::string const & name, std::vector<m2::RegionD> const & borders)
  {
    // use index in vector as tag
    auto w = m_writer.GetWriter(strings::to_string(m_polys.size()));
    serial::GeometryCodingParams cp;

    // calc rect
    m2::RectD rect;
    for (m2::RegionD const & border : borders)
      rect.Add(border.GetRect());

    // store polygon info
    m_polys.push_back(storage::CountryDef(name, rect));

    // write polygons as paths
    WriteVarUint(w, borders.size());
    for (m2::RegionD const & border : borders)
    {
      std::vector<m2::PointD> const & in = border.Data();
      std::vector<m2::PointD> out;

      /// @todo Choose scale level for simplification.
      SimplifyDefault(in.begin(), in.end(), math::Pow2(scales::GetEpsilonForSimplify(10)), out);

      serial::SaveOuterPath(out, cp, *w);
    }
  }

  void WritePolygonsInfo()
  {
    auto w = m_writer.GetWriter(PACKED_POLYGONS_INFO_TAG);
    rw::Write(*w, m_polys);
  }

private:
  FilesContainerW m_writer;

  std::vector<storage::CountryDef> m_polys;
};

bool ReadPolygon(std::istream & stream, m2::RegionD & region, std::string const & filename)
{
  std::string line, name;
  double lon, lat;

  // read ring id, fail if it's empty
  std::getline(stream, name);
  if (name.empty() || name == "END")
    return false;

  while (stream.good())
  {
    std::getline(stream, line);
    strings::Trim(line);

    if (line.empty())
      continue;

    if (line == "END")
      break;

    std::istringstream iss(line);
    iss >> lon >> lat;
    CHECK(!iss.fail(), ("Incorrect data in", filename));

    region.AddPoint(mercator::FromLatLon(lat, lon));
  }

  // drop inner rings
  return name[0] != '!';
}
}  // namespace

bool CountryPolygons::Contains(m2::PointD const & point) const
{
  return m_polygons.ForAnyInRect(m2::RectD(point, point), [&](auto const & rgn)
  {
    return rgn.Contains(point, ContainsCompareFn(GetContainsEpsilon()));
  });
}

bool LoadBorders(std::string const & borderFile, std::vector<m2::RegionD> & outBorders)
{
  std::ifstream stream(borderFile);
  std::string line;
  if (!std::getline(stream, line).good())  // skip title
  {
    LOG(LERROR, ("Polygon file is empty:", borderFile));
    return false;
  }

  m2::RegionD currentPolygon;
  while (ReadPolygon(stream, currentPolygon, borderFile))
  {
    CHECK(currentPolygon.IsValid(), ("Invalid region in", borderFile));
    outBorders.emplace_back(std::move(currentPolygon));
    currentPolygon = m2::RegionD();
  }

  CHECK(!outBorders.empty(), ("No borders were loaded from", borderFile));
  return true;
}

bool GetBordersRect(std::string const & baseDir, std::string const & country,
                    m2::RectD & bordersRect)
{
  auto const bordersFile = base::JoinPath(baseDir, BORDERS_DIR, country + BORDERS_EXTENSION);
  if (!Platform::IsFileExistsByFullPath(bordersFile))
  {
    LOG(LWARNING, ("File with borders does not exist:", bordersFile));
    return false;
  }

  std::vector<m2::RegionD> borders;
  CHECK(LoadBorders(bordersFile, borders), ());
  bordersRect.MakeEmpty();
  for (auto const & border : borders)
    bordersRect.Add(border.GetRect());

  return true;
}

CountryPolygonsCollection LoadCountriesList(std::string const & baseDir)
{
  LOG(LINFO, ("Loading countries in", BORDERS_DIR, "folder in", baseDir));

  CountryPolygonsCollection countryPolygonsCollection;
  ForEachCountry(baseDir, [&](auto const & name, auto const & borders)
  {
    PolygonsTree polygons;
    for (m2::RegionD const & border : borders)
      polygons.Add(border, border.GetRect());

    countryPolygonsCollection.Add(CountryPolygons(name, polygons));
  });

  LOG(LINFO, ("Countries loaded:", countryPolygonsCollection.GetSize()));
  CHECK_NOT_EQUAL(countryPolygonsCollection.GetSize(), 0, (baseDir));
  return countryPolygonsCollection;
}

void GeneratePackedBorders(std::string const & baseDir)
{
  PackedBordersGenerator generator(baseDir);
  ForEachCountry(baseDir, generator);
  generator.WritePolygonsInfo();
}

void DumpBorderToPolyFile(std::string const & targetDir, storage::CountryId const & mwmName,
                          std::vector<m2::RegionD> const & polygons)
{
  CHECK(!polygons.empty(), ());

  std::string const filePath = base::JoinPath(targetDir, mwmName + ".poly");
  std::ofstream poly(filePath);
  CHECK(poly.good(), ());

  poly << std::setprecision(20) << std::fixed;

  poly << mwmName << std::endl;
  size_t polygonId = 1;
  for (auto const & points : polygons)
  {
    poly << polygonId << std::endl;
    ++polygonId;
    for (auto const & point : points.Data())
    {
      ms::LatLon const ll = mercator::ToLatLon(point);
      poly << "    " << std::scientific << ll.m_lon << "    " << ll.m_lat << std::endl;
    }
    poly << "END" << std::endl;
  }

  poly << "END" << std::endl;
  poly.close();
}

void UnpackBorders(std::string const & baseDir, std::string const & targetDir)
{
  if (!Platform::IsFileExistsByFullPath(targetDir) && !Platform::MkDirChecked(targetDir))
    MYTHROW(FileSystemException, ("Unable to find or create directory", targetDir));

  std::vector<storage::CountryDef> countries;
  FilesContainerR reader(base::JoinPath(baseDir, PACKED_POLYGONS_FILE));
  ReaderSource<ModelReaderPtr> src(reader.GetReader(PACKED_POLYGONS_INFO_TAG));
  rw::Read(src, countries);

  for (size_t id = 0; id < countries.size(); id++)
  {
    storage::CountryId const mwmName = countries[id].m_countryId;

    src = reader.GetReader(strings::to_string(id));

    auto const polygons = ReadPolygonsOfOneBorder(src);
    DumpBorderToPolyFile(targetDir, mwmName, polygons);
  }
}

CountryPolygonsCollection const & GetOrCreateCountryPolygonsTree(std::string const & baseDir)
{
  /// @todo Are there many different paths with polygons, that we have to store map?
  static std::mutex mutex;
  static std::unordered_map<std::string, CountryPolygonsCollection> countriesMap;

  std::lock_guard<std::mutex> lock(mutex);
  auto const it = countriesMap.find(baseDir);
  if (it != countriesMap.cend())
    return it->second;

  auto const eIt = countriesMap.emplace(baseDir, LoadCountriesList(baseDir));
  return eIt.first->second;
}
}  // namespace borders
