#include "poly_borders/poly_borders_tests/tools.hpp"

#include "testing/testing.hpp"

#include "poly_borders/borders_data.hpp"

#include "platform/platform_tests_support/scoped_dir.hpp"
#include "platform/platform_tests_support/scoped_file.hpp"

#include "geometry/point2d.hpp"

#include <set>
#include <string>
#include <vector>

namespace remove_empty_spaces_tests
{
using namespace platform::tests_support;
using namespace platform;
using namespace poly_borders;
using namespace std;

string const kTestDir = "borders_poly_dir";
auto constexpr kSmallShift = 1e-9;
auto constexpr kSmallPointShift = m2::PointD(kSmallShift, kSmallShift);

void Process(BordersData & bordersData, string const & bordersDir)
{
  bordersData.Init(bordersDir);
  bordersData.MarkPoints();
  bordersData.RemoveEmptySpaceBetweenBorders();
}

bool ConsistsOf(Polygon const & polygon, vector<m2::PointD> const & points)
{
  CHECK_EQUAL(polygon.m_points.size(), points.size(), ());

  set<size_t> used;
  for (auto const & point : points)
  {
    for (size_t i = 0; i < polygon.m_points.size(); ++i)
    {
      static double constexpr kEps = 1e-5;
      if (AlmostEqualAbs(point, polygon.m_points[i].m_point, kEps) &&
          used.count(i) == 0)
      {
        used.emplace(i);
        break;
      }
    }
  }

  return used.size() == points.size();
}

UNIT_TEST(PolyBordersPostprocessor_RemoveEmptySpaces_1)
{
  ScopedDir const scopedDir(kTestDir);
  string const & bordersDir = scopedDir.GetFullPath();

  m2::PointD a(0.0, 0.0);
  m2::PointD b(1.0, 0.0);
  m2::PointD c(2.0, 0.0);
  m2::PointD d(3.0, 0.0);
  m2::PointD e(4.0, 0.0);

  vector<vector<m2::PointD>> polygons1 = {
    {a, b, c, d, e}
  };

  vector<vector<m2::PointD>> polygons2 = {
    {a, b, c, d, e}
  };

  vector<shared_ptr<ScopedFile>> files;
  files.emplace_back(CreatePolyBorderFileByPolygon(kTestDir, "First", polygons1));
  files.emplace_back(CreatePolyBorderFileByPolygon(kTestDir, "Second", polygons2));

  BordersData bordersData;
  Process(bordersData, bordersDir);

  auto const & bordersPolygon1 = bordersData.GetBordersPolygonByName("First" + BordersData::kBorderExtension + "1");
  TEST(ConsistsOf(bordersPolygon1, {a, b, c, d, e}), ());

  auto const & bordersPolygon2 = bordersData.GetBordersPolygonByName("Second" + BordersData::kBorderExtension + "1");
  TEST(ConsistsOf(bordersPolygon2, {a, b, c, d, e}), ());
}

UNIT_TEST(PolyBordersPostprocessor_RemoveEmptySpaces_2)
{
  ScopedDir const scopedDir(kTestDir);
  string const & bordersDir = scopedDir.GetFullPath();

  m2::PointD a(0.0, 0.0);
  m2::PointD b(1.0, 0.0);
  // We should make c.y small because in other case changed area
  // will be so great, that point |c| will not be removed.
  m2::PointD c(2.0, kSmallShift);
  m2::PointD d(3.0, 0.0);
  m2::PointD e(4.0, 0.0);

  // Point |c| is absent from polygons2, algorithm should remove |c| from polygon1.
  vector<vector<m2::PointD>> polygons1 = {
      {a, b, c, d, e}
  };

  vector<vector<m2::PointD>> polygons2 = {
      {a, b, d, e}
  };

  vector<shared_ptr<ScopedFile>> files;
  files.emplace_back(CreatePolyBorderFileByPolygon(kTestDir, "First", polygons1));
  files.emplace_back(CreatePolyBorderFileByPolygon(kTestDir, "Second", polygons2));

  BordersData bordersData;
  Process(bordersData, bordersDir);

  auto const & bordersPolygon1 = bordersData.GetBordersPolygonByName("First" + BordersData::kBorderExtension + "1");
  TEST(ConsistsOf(bordersPolygon1, {a, b, d, e}), ());

  auto const & bordersPolygon2 = bordersData.GetBordersPolygonByName("Second" + BordersData::kBorderExtension + "1");
  TEST(ConsistsOf(bordersPolygon2, {a, b, d, e}), ());
}

// Like |PolyBordersPostprocessor_RemoveEmptySpaces_2| but two points will be
// added instead of one.
UNIT_TEST(PolyBordersPostprocessor_RemoveEmptySpaces_3)
{
  ScopedDir const scopedDir(kTestDir);
  string const & bordersDir = scopedDir.GetFullPath();

  m2::PointD a(0.0, 0.0);
  m2::PointD b(1.0, 0.0);
  // We should make c.y (and d.y) small because in other case changed area
  // will be so great, that point |c| (|d|) will not be removed.
  m2::PointD c(2.0, kSmallShift);
  m2::PointD d(2.5, kSmallShift);
  m2::PointD e(4.0, 0.0);
  m2::PointD f(5.0, 0.0);

  // Point |c| and |d| is absent from polygons2, algorithm should remove |c| from polygon1.
  vector<vector<m2::PointD>> polygons1 = {
      {a, b, c, d, e, f}
  };

  vector<vector<m2::PointD>> polygons2 = {
      {a, b, e, f}
  };

  vector<shared_ptr<ScopedFile>> files;
  files.emplace_back(CreatePolyBorderFileByPolygon(kTestDir, "First", polygons1));
  files.emplace_back(CreatePolyBorderFileByPolygon(kTestDir, "Second", polygons2));

  BordersData bordersData;
  Process(bordersData, bordersDir);

  auto const & bordersPolygon1 = bordersData.GetBordersPolygonByName("First" + BordersData::kBorderExtension + "1");
  TEST(ConsistsOf(bordersPolygon1, {a, b, e, f}), ());

  auto const & bordersPolygon2 = bordersData.GetBordersPolygonByName("Second" + BordersData::kBorderExtension + "1");
  TEST(ConsistsOf(bordersPolygon2, {a, b, e, f}), ());
}

// Do not remove point |c| because changed area is too big.
UNIT_TEST(PolyBordersPostprocessor_RemoveEmptySpaces_4)
{
  ScopedDir const scopedDir(kTestDir);
  string const & bordersDir = scopedDir.GetFullPath();

  m2::PointD a(0.0, 0.0);
  m2::PointD b(1.0, 0.0);
  m2::PointD c(2.0, 1.0);
  m2::PointD d(4.0, 0.0);
  m2::PointD e(5.0, 0.0);

  vector<vector<m2::PointD>> polygons1 = {
      {a, b, c, d, e}
  };

  vector<vector<m2::PointD>> polygons2 = {
      {a, b, d, e}
  };

  vector<shared_ptr<ScopedFile>> files;
  files.emplace_back(CreatePolyBorderFileByPolygon(kTestDir, "First", polygons1));
  files.emplace_back(CreatePolyBorderFileByPolygon(kTestDir, "Second", polygons2));

  BordersData bordersData;
  Process(bordersData, bordersDir);

  auto const & bordersPolygon1 = bordersData.GetBordersPolygonByName("First" + BordersData::kBorderExtension + "1");
  TEST(ConsistsOf(bordersPolygon1, {a, b, c, d, e}), ());

  auto const & bordersPolygon2 = bordersData.GetBordersPolygonByName("Second" + BordersData::kBorderExtension + "1");
  TEST(ConsistsOf(bordersPolygon2, {a, b, d, e}), ());
}

// Replace {c1, d1, e1} -> {c2, d2}.
UNIT_TEST(PolyBordersPostprocessor_RemoveEmptySpaces_5)
{
  ScopedDir const scopedDir(kTestDir);
  string const & bordersDir = scopedDir.GetFullPath();

  m2::PointD a(0.0, 0.0);
  m2::PointD b(9.0, 0.0);

  m2::PointD c1(2.0, 3.0);
  m2::PointD d1(4.0, 4.0);
  m2::PointD e1(d1 + kSmallPointShift + kSmallPointShift);

  m2::PointD c2(c1 + kSmallPointShift);
  m2::PointD d2(d1 + kSmallPointShift);

  vector<vector<m2::PointD>> polygons1 = {
      {a, c1, d1, e1, b}
  };

  vector<vector<m2::PointD>> polygons2 = {
      {a, c2, d2, b}
  };

  vector<shared_ptr<ScopedFile>> files;
  files.emplace_back(CreatePolyBorderFileByPolygon(kTestDir, "First", polygons1));
  files.emplace_back(CreatePolyBorderFileByPolygon(kTestDir, "Second", polygons2));

  BordersData bordersData;
  Process(bordersData, bordersDir);

  auto const & bordersPolygon1 = bordersData.GetBordersPolygonByName("First" + BordersData::kBorderExtension + "1");
  TEST(ConsistsOf(bordersPolygon1, {a, c2, d2, b}), ());

  auto const & bordersPolygon2 = bordersData.GetBordersPolygonByName("Second" + BordersData::kBorderExtension + "1");
  TEST(ConsistsOf(bordersPolygon2, {a, c2, d2, b}), ());
}

// Removes duplicates.
UNIT_TEST(PolyBordersPostprocessor_RemoveEmptySpaces_6)
{
  ScopedDir const scopedDir(kTestDir);
  string const & bordersDir = scopedDir.GetFullPath();

  m2::PointD a(0.0, 0.0);
  m2::PointD b(1.0, 0.0);
  m2::PointD c(2.0, 1.0);
  m2::PointD d(4.0, 0.0);
  m2::PointD e(5.0, 0.0);

  vector<vector<m2::PointD>> polygons1 = {
      {a, b, c, d, d, d, e, e, e}
  };

  vector<vector<m2::PointD>> polygons2 = {
      {a, d, d, d, e}
  };

  vector<shared_ptr<ScopedFile>> files;
  files.emplace_back(CreatePolyBorderFileByPolygon(kTestDir, "First", polygons1));
  files.emplace_back(CreatePolyBorderFileByPolygon(kTestDir, "Second", polygons2));

  BordersData bordersData;
  Process(bordersData, bordersDir);

  auto const & bordersPolygon1 = bordersData.GetBordersPolygonByName("First" + BordersData::kBorderExtension + "1");
  TEST(ConsistsOf(bordersPolygon1, {a, b, c, d, e}), ());

  auto const & bordersPolygon2 = bordersData.GetBordersPolygonByName("Second" + BordersData::kBorderExtension + "1");
  TEST(ConsistsOf(bordersPolygon2, {a, d, e}), ());
}
}  // namespace remove_empty_spaces_tests
