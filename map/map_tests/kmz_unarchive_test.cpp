#include "testing/testing.hpp"

#include "map/bookmark_helpers.hpp"

#include "platform/platform.hpp"

#include "base/scope_guard.hpp"


UNIT_TEST(KMZ_UnzipTest)
{
  std::string const kmzFile = GetPlatform().TestsDataPathForFile("test_data/kml/test.kmz");
  auto const filePaths = GetKMLOrGPXFilesPathsToLoad(kmzFile);
  TEST_EQUAL(1, filePaths.size(), ());
  std::string const filePath = filePaths[0];
  TEST(!filePath.empty(), ());
  SCOPE_GUARD(fileGuard, std::bind(&base::DeleteFileX, filePath));

  TEST(filePath.ends_with("doc.kml"), (filePath));

  auto const kmlData = LoadKmlFile(filePath, KmlFileType::Text);
  TEST(kmlData != nullptr, ());

  TEST_EQUAL(kmlData->m_bookmarksData.size(), 6, ("Category wrong number of bookmarks"));

  {
    Bookmark const bm(std::move(kmlData->m_bookmarksData[0]));
    TEST_EQUAL(kml::GetDefaultStr(bm.GetName()), ("Lahaina Breakwall"), ("KML wrong name!"));
    TEST_EQUAL(bm.GetColor(), kml::PredefinedColor::Red, ("KML wrong type!"));
    TEST_ALMOST_EQUAL_ULPS(bm.GetPivot().x, -156.6777046791284, ("KML wrong org x!"));
    TEST_ALMOST_EQUAL_ULPS(bm.GetPivot().y, 21.34256685860084, ("KML wrong org y!"));
    TEST_EQUAL(bm.GetScale(), 0, ("KML wrong scale!"));
  }
  {
    Bookmark const bm(std::move(kmlData->m_bookmarksData[1]));
    TEST_EQUAL(kml::GetDefaultStr(bm.GetName()), ("Seven Sacred Pools, Kipahulu"), ("KML wrong name!"));
    TEST_EQUAL(bm.GetColor(), kml::PredefinedColor::Red, ("KML wrong type!"));
    TEST_ALMOST_EQUAL_ULPS(bm.GetPivot().x, -156.0405130750025, ("KML wrong org x!"));
    TEST_ALMOST_EQUAL_ULPS(bm.GetPivot().y, 21.12480639056074, ("KML wrong org y!"));
    TEST_EQUAL(bm.GetScale(), 0, ("KML wrong scale!"));
  }
}

UNIT_TEST(Multi_KML_KMZ_UnzipTest)
{
  std::string const kmzFile = GetPlatform().TestsDataPathForFile("test_data/kml/BACRNKMZ.kmz");
  auto const filePaths = GetKMLOrGPXFilesPathsToLoad(kmzFile);
  std::vector<std::string> expectedFileNames =
  {
      "BACRNKMZfilesCampgrounds 26may2022 green and tree icon",
      "BACRNKMZfilesIndoor Accommodations 26may2022 placemark purple and bed icon",
      "BACRNKMZfilesRoute 1 Canada - West-East Daily Segments",
      "BACRNKMZfilesRoute 2 Canada - West-East Daily Segments",
      "BACRNKMZfilesRoute Connector Canada - West-East Daily Segments",
      "BACRNKMZdoc"

  };
  TEST_EQUAL(expectedFileNames.size(), filePaths.size(), ());
  for (auto const & filePath : filePaths)
  {
    auto matched = false;
    for (auto const & expectedFileName : expectedFileNames)
    {
      matched = filePath.find(expectedFileName) != std::string::npos;
      if (matched)
        break;
    }
    TEST(matched, ("Unexpected file path: " + filePath));
  }
}
