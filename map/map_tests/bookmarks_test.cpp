#include "testing/testing.hpp"

#include "map/bookmark_helpers.hpp"
#include "map/framework.hpp"

#include "drape_frontend/visual_params.hpp"

#include "indexer/feature_utils.hpp"
#include "indexer/mwm_set.hpp"

#include "platform/platform.hpp"
#include "platform/preferred_languages.hpp"

#include "coding/internal/file_data.hpp"
#include "coding/string_utf8_multilang.hpp"
#include "coding/zip_reader.hpp"

#include "base/file_name_utils.hpp"
#include "base/scope_guard.hpp"

#include <array>
#include <cstring>  // strlen
#include <map>
#include <numeric>  // std::reduce
#include <set>
#include <string>
#include <vector>

namespace bookmarks_test
{
using namespace std;

using Runner = Platform::ThreadRunner;

static FrameworkParams const kFrameworkParams(false /* m_enableDiffs */);

char const * kmlString =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<kml xmlns=\"http://earth.google.com/kml/2.2\">"
    "<Document>"
      "<name>MapName</name>"
      "<description><![CDATA[MapDescription]]></description>"
      "<visibility>0</visibility>"
      "<Style id=\"placemark-blue\">"
        "<IconStyle>"
          "<Icon>"
            "<href>https://omaps.app/placemarks/placemark-blue.png</href>"
          "</Icon>"
        "</IconStyle>"
      "</Style>"
      "<Style id=\"placemark-brown\">"
        "<IconStyle>"
          "<Icon>"
            "<href>https://omaps.app/placemarks/placemark-brown.png</href>"
          "</Icon>"
        "</IconStyle>"
      "</Style>"
      "<Style id=\"placemark-green\">"
        "<IconStyle>"
          "<Icon>"
            "<href>https://omaps.app/placemarks/placemark-green.png</href>"
          "</Icon>"
        "</IconStyle>"
      "</Style>"
      "<Style id=\"placemark-orange\">"
        "<IconStyle>"
          "<Icon>"
            "<href>https://omaps.app/placemarks/placemark-orange.png</href>"
          "</Icon>"
        "</IconStyle>"
      "</Style>"
      "<Style id=\"placemark-pink\">"
        "<IconStyle>"
          "<Icon>"
            "<href>https://omaps.app/placemarks/placemark-pink.png</href>"
          "</Icon>"
        "</IconStyle>"
      "</Style>"
      "<Style id=\"placemark-purple\">"
        "<IconStyle>"
          "<Icon>"
            "<href>https://omaps.app/placemarks/placemark-purple.png</href>"
          "</Icon>"
        "</IconStyle>"
      "</Style>"
      "<Style id=\"placemark-red\">"
        "<IconStyle>"
          "<Icon>"
            "<href>https://omaps.app/placemarks/placemark-red.png</href>"
          "</Icon>"
        "</IconStyle>"
      "</Style>"
      "<Placemark>"
        "<name>Nebraska</name>"
        "<description><![CDATA[]]></description>"
        "<styleUrl>#placemark-red</styleUrl>"
        "<Point>"
          "<coordinates>-99.901810,41.492538,0.000000</coordinates>"
        "</Point>"
      "</Placemark>"
      "<Placemark>"
        "<name>Monongahela National Forest</name>"
        "<description><![CDATA[Huttonsville, WV 26273<br>]]></description>"
        "<styleUrl>#placemark-pink</styleUrl>"
        "<TimeStamp>"
          "<when>1986-08-12T07:10:43Z</when>"
        "</TimeStamp>"
        "<Point>"
          "<coordinates>-79.829674,38.627785,0.000000</coordinates>"
        "</Point>"
      "</Placemark>"
      "<Placemark>"
        "<name>From: Минск, Минская область, Беларусь</name>"
        "<description><![CDATA[]]></description>"
        "<styleUrl>#placemark-blue</styleUrl>"
        "<TimeStamp>"
          "<when>1998-03-03T03:04:48+01:30</when>"
        "</TimeStamp>"
        "<Point>"
          "<coordinates>27.566765,53.900047,0</coordinates>"
        "</Point>"
      "</Placemark>"
      "<Placemark>"
        "<name><![CDATA[<MWM & Sons>]]></name>"
        "<description><![CDATA[Amps & <brackets>]]></description>"
        "<styleUrl>#placemark-green</styleUrl>"
        "<TimeStamp>"
          "<when>2048 bytes in two kilobytes - some invalid timestamp</when>"
        "</TimeStamp>"
        "<Point>"
          "<coordinates>27.551532,53.89306</coordinates>"
        "</Point>"
      "</Placemark>"
    "</Document>"
    "</kml>";

#define BM_CALLBACKS {                                                            \
    []() -> StringsBundle const &                                                \
    {                                                                            \
      static StringsBundle const dummyBundle;                                    \
      return dummyBundle;                                                        \
    },                                                                           \
    static_cast<BookmarkManager::Callbacks::GetSeacrhAPIFn>(nullptr),            \
    static_cast<BookmarkManager::Callbacks::CreatedBookmarksCallback>(nullptr),  \
    static_cast<BookmarkManager::Callbacks::UpdatedBookmarksCallback>(nullptr),  \
    static_cast<BookmarkManager::Callbacks::DeletedBookmarksCallback>(nullptr),  \
    static_cast<BookmarkManager::Callbacks::AttachedBookmarksCallback>(nullptr), \
    static_cast<BookmarkManager::Callbacks::DetachedBookmarksCallback>(nullptr)  \
  }

void CheckBookmarks(BookmarkManager const & bmManager, kml::MarkGroupId groupId)
{
  auto const & markIds = bmManager.GetUserMarkIds(groupId);
  TEST_EQUAL(markIds.size(), 4, ());

  auto it = markIds.rbegin();
  Bookmark const * bm = bmManager.GetBookmark(*it++);
  TEST_EQUAL(kml::GetDefaultStr(bm->GetName()), "Nebraska", ());
  TEST_EQUAL(bm->GetColor(), kml::PredefinedColor::Red, ());
  TEST(bm->GetDescription().empty(), ());
  TEST_EQUAL(kml::ToSecondsSinceEpoch(bm->GetTimeStamp()), 0, ());

  bm = bmManager.GetBookmark(*it++);
  TEST_EQUAL(kml::GetDefaultStr(bm->GetName()), "Monongahela National Forest", ());
  TEST_EQUAL(bm->GetColor(), kml::PredefinedColor::Pink, ());
  TEST_EQUAL(bm->GetDescription(), "Huttonsville, WV 26273<br>", ());
  TEST_EQUAL(kml::ToSecondsSinceEpoch(bm->GetTimeStamp()), 524214643, ());

  bm = bmManager.GetBookmark(*it++);
  m2::PointD org = bm->GetPivot();

  double constexpr kEps = 1e-6;
  TEST(base::AlmostEqualAbs(mercator::XToLon(org.x), 27.566765, kEps), ());
  TEST(base::AlmostEqualAbs(mercator::YToLat(org.y), 53.900047, kEps), ());
  TEST_EQUAL(kml::GetDefaultStr(bm->GetName()), "From: Минск, Минская область, Беларусь", ());
  TEST_EQUAL(bm->GetColor(), kml::PredefinedColor::Blue, ());
  TEST(bm->GetDescription().empty(), ());
  TEST_EQUAL(kml::ToSecondsSinceEpoch(bm->GetTimeStamp()), 888888888, ());

  bm = bmManager.GetBookmark(*it++);
  org = bm->GetPivot();
  TEST(base::AlmostEqualAbs(mercator::XToLon(org.x), 27.551532, kEps), ());
  TEST(base::AlmostEqualAbs(mercator::YToLat(org.y), 53.89306, kEps), ());
  TEST_EQUAL(kml::GetDefaultStr(bm->GetName()), "<MWM & Sons>", ());
  TEST_EQUAL(bm->GetDescription(), "Amps & <brackets>", ());
  TEST_EQUAL(kml::ToSecondsSinceEpoch(bm->GetTimeStamp()), 0, ());
}

KmlFileType GetActiveKmlFileType()
{
  return KmlFileType::Text;
}

UNIT_CLASS_TEST(Runner, Bookmarks_ImportKML)
{
  BookmarkManager bmManager(BM_CALLBACKS);
  bmManager.EnableTestMode(true);

  BookmarkManager::KMLDataCollection kmlDataCollection;

  kmlDataCollection.emplace_back(""/* filePath */,
                                 LoadKmlData(MemReader(kmlString, strlen(kmlString)), KmlFileType::Text));
  TEST(kmlDataCollection.back().second, ());
  bmManager.CreateCategories(std::move(kmlDataCollection));
  TEST_EQUAL(bmManager.GetBmGroupsCount(), 1, ());

  auto const groupId = bmManager.GetUnsortedBmGroupsIdList().front();
  CheckBookmarks(bmManager, groupId);

  // Name should be overridden from the KML
  TEST_EQUAL(bmManager.GetCategoryName(groupId), "MapName", ());
  TEST_EQUAL(bmManager.IsVisible(groupId), false, ());
}

UNIT_CLASS_TEST(Runner, Bookmarks_ExportKML)
{
  string const dir = GetBookmarksDirectory();
  bool const delDirOnExit = Platform::MkDir(dir) == Platform::ERR_OK;
  SCOPE_GUARD(dirDeleter, [&](){ if (delDirOnExit) (void)Platform::RmDir(dir); });
  string const ext = ".kmb";
  string const fileName = base::JoinPath(dir, "UnitTestBookmarks" + ext);
  SCOPE_GUARD(fileDeleter, [&](){ (void)base::DeleteFileX(fileName); });

  BookmarkManager bmManager(BM_CALLBACKS);
  bmManager.EnableTestMode(true);

  BookmarkManager::KMLDataCollection kmlDataCollection1;
  kmlDataCollection1.emplace_back("",
                                  LoadKmlData(MemReader(kmlString, strlen(kmlString)), KmlFileType::Text));
  bmManager.CreateCategories(std::move(kmlDataCollection1));
  TEST_EQUAL(bmManager.GetBmGroupsCount(), 1, ());

  auto const groupId1 = bmManager.GetUnsortedBmGroupsIdList().front();
  CheckBookmarks(bmManager, groupId1);

  TEST_EQUAL(bmManager.IsVisible(groupId1), false, ());

  // Change visibility
  bmManager.GetEditSession().SetIsVisible(groupId1, true);
  TEST_EQUAL(bmManager.IsVisible(groupId1), true, ());

  {
    FileWriter writer(fileName);
    bmManager.SaveBookmarkCategory(groupId1, writer, GetActiveKmlFileType());
  }

  bmManager.GetEditSession().ClearGroup(groupId1);
  TEST_EQUAL(bmManager.GetUserMarkIds(groupId1).size(), 0, ());

  bmManager.GetEditSession().DeleteBmCategory(groupId1);
  TEST_EQUAL(bmManager.HasBmCategory(groupId1), false, ());
  TEST_EQUAL(bmManager.GetBmGroupsCount(), 0, ());

  BookmarkManager::KMLDataCollection kmlDataCollection2;
  kmlDataCollection2.emplace_back("", LoadKmlData(FileReader(fileName), GetActiveKmlFileType()));
  TEST(kmlDataCollection2.back().second, ());

  bmManager.CreateCategories(std::move(kmlDataCollection2));
  TEST_EQUAL(bmManager.GetBmGroupsCount(), 1, ());

  auto const groupId2 = bmManager.GetUnsortedBmGroupsIdList().front();
  CheckBookmarks(bmManager, groupId2);
  TEST_EQUAL(bmManager.IsVisible(groupId2), true, ());

  bmManager.GetEditSession().DeleteBmCategory(groupId2);
  TEST_EQUAL(bmManager.HasBmCategory(groupId2), false, ());

  BookmarkManager::KMLDataCollection kmlDataCollection3;
  kmlDataCollection3.emplace_back(fileName, LoadKmlFile(fileName, GetActiveKmlFileType()));
  TEST(kmlDataCollection3.back().second, ());

  bmManager.CreateCategories(std::move(kmlDataCollection3), true /* autoSave */);
  TEST_EQUAL(bmManager.GetBmGroupsCount(), 1, ());

  auto const groupId3 = bmManager.GetUnsortedBmGroupsIdList().front();
  CheckBookmarks(bmManager, groupId3);

  TEST(bmManager.SaveBookmarkCategory(groupId3), ());
  // old file shouldn't be deleted if we save bookmarks with new category name
  uint64_t dummy;
  TEST(base::GetFileSize(fileName, dummy), ());
}

namespace
{
  void DeleteCategoryFiles(vector<string> const & arrFiles)
  {
    string const path = GetBookmarksDirectory();
    string const extension = ".kmb";
    for (auto const & fileName : arrFiles)
      FileWriter::DeleteFileX(base::JoinPath(path, fileName + extension));
  }

  UserMark const * GetMark(Framework & fm, m2::PointD const & pt)
  {
    m2::AnyRectD rect;
    fm.GetTouchRect(fm.GtoP(pt), 20, rect);

    return fm.GetBookmarkManager().FindNearestUserMark(rect);
  }

  Bookmark const * GetBookmark(Framework & fm, m2::PointD const & pt)
  {
    auto const * mark = GetMark(fm, pt);
    ASSERT(mark, ());
    ASSERT(mark->GetMarkType() == UserMark::BOOKMARK, ());
    return dynamic_cast<Bookmark const *>(mark);
  }

  Bookmark const * GetBookmarkPxPoint(Framework & fm, m2::PointD const & pt)
  {
    return GetBookmark(fm, fm.PtoG(pt));
  }

  bool IsValidBookmark(Framework & fm, m2::PointD const & pt)
  {
    auto const * mark = GetMark(fm, pt);
    return (mark != nullptr) && (mark->GetMarkType() == UserMark::BOOKMARK);
  }
}  // namespace

UNIT_TEST(Bookmarks_Timestamp)
{
  Framework fm(kFrameworkParams);
  df::VisualParams::Init(1.0, 1024);

  BookmarkManager & bmManager = fm.GetBookmarkManager();
  bmManager.EnableTestMode(true);

  m2::PointD constexpr orgPoint(10, 10);
  vector<string> const arrCat = {"cat", "cat1"};

  kml::BookmarkData b1;
  kml::SetDefaultStr(b1.m_name, "name");
  b1.m_point = orgPoint;
  auto cat1 = bmManager.CreateBookmarkCategory(arrCat[0], false /* autoSave */);

  Bookmark const * pBm1 = bmManager.GetEditSession().CreateBookmark(std::move(b1), cat1);
  TEST_NOT_EQUAL(kml::ToSecondsSinceEpoch(pBm1->GetTimeStamp()), 0, ());

  kml::BookmarkData b2;
  kml::SetDefaultStr(b2.m_name, "newName");
  b2.m_point = orgPoint;
  kml::BookmarkData b22 = b2;
  Bookmark const * pBm2 = bmManager.GetEditSession().CreateBookmark(std::move(b2), cat1);

  auto cat2 = bmManager.CreateBookmarkCategory(arrCat[0], false /* autoSave */);
  Bookmark const * pBm3 = bmManager.GetEditSession().CreateBookmark(std::move(b22), cat2);

  // Check bookmarks order here. First added should be in the bottom of the list.
  auto const firstId = * bmManager.GetUserMarkIds(cat1).rbegin();
  TEST_EQUAL(firstId, pBm1->GetId(), ());

  Bookmark const * pBm01 = bmManager.GetBookmark(pBm1->GetId());

  TEST_EQUAL(kml::GetDefaultStr(pBm01->GetName()), "name", ());

  Bookmark const * pBm02 = bmManager.GetBookmark(pBm2->GetId());

  TEST_EQUAL(kml::GetDefaultStr(pBm02->GetName()), "newName", ());

  Bookmark const * pBm03 = bmManager.GetBookmark(pBm3->GetId());

  TEST_EQUAL(kml::GetDefaultStr(pBm03->GetName()), "newName", ());

  TEST_EQUAL(bmManager.GetUserMarkIds(cat1).size(), 2, ());
  TEST_EQUAL(bmManager.GetUserMarkIds(cat2).size(), 1, ());

  DeleteCategoryFiles(arrCat);
}

UNIT_TEST(Bookmarks_Getting)
{
  Framework fm(kFrameworkParams);
  df::VisualParams::Init(1.0, 1024);
  fm.OnSize(800, 400);
  fm.ShowRect(m2::RectD(0, 0, 80, 40));

  // This is not correct because Framework::OnSize doesn't work until SetRenderPolicy is called.
  //TEST(m2::AlmostEqualULPs(m2::PointD(400, 200), pixC), (pixC));

  BookmarkManager & bmManager = fm.GetBookmarkManager();
  bmManager.EnableTestMode(true);

  vector<string> const arrCat = {"cat1", "cat2", "cat3"};

  auto const cat1 = bmManager.CreateBookmarkCategory(arrCat[0], false /* autoSave */);
  auto const cat2 = bmManager.CreateBookmarkCategory(arrCat[1], false /* autoSave */);
  auto const cat3 = bmManager.CreateBookmarkCategory(arrCat[2], false /* autoSave */);

  kml::BookmarkData bm1;
  kml::SetDefaultStr(bm1.m_name, "1");
  bm1.m_point = m2::PointD(38, 20);
  auto const * pBm1 = bmManager.GetEditSession().CreateBookmark(std::move(bm1), cat1);

  kml::BookmarkData bm2;
  kml::SetDefaultStr(bm2.m_name, "2");
  bm2.m_point = m2::PointD(41, 20);
  auto const * pBm2 = bmManager.GetEditSession().CreateBookmark(std::move(bm2), cat2);

  kml::BookmarkData bm3;
  kml::SetDefaultStr(bm3.m_name, "3");
  bm3.m_point = m2::PointD(41, 40);
  auto const * pBm3 = bmManager.GetEditSession().CreateBookmark(std::move(bm3), cat3);

  TEST_NOT_EQUAL(pBm1->GetGroupId(), pBm2->GetGroupId(), ());
  TEST_NOT_EQUAL(pBm1->GetGroupId(), pBm3->GetGroupId(), ());
  TEST_NOT_EQUAL(pBm1->GetGroupId(), pBm3->GetGroupId(), ());

  TEST_EQUAL(bmManager.GetBmGroupsCount(), 3, ());

  TEST(IsValidBookmark(fm, m2::PointD(40, 20)), ());
  Bookmark const * mark = GetBookmark(fm, m2::PointD(40, 20));
  TEST_EQUAL(bmManager.GetCategoryName(mark->GetGroupId()), "cat2", ());

  TEST(!IsValidBookmark(fm, m2::PointD(0, 0)), ());
  TEST(!IsValidBookmark(fm, m2::PointD(800, 400)), ());

  TEST(IsValidBookmark(fm, m2::PointD(41, 40)), ());
  mark = GetBookmark(fm, m2::PointD(41, 40));
  TEST_EQUAL(bmManager.GetCategoryName(mark->GetGroupId()), "cat3", ());

  kml::BookmarkData bm4;
  kml::SetDefaultStr(bm4.m_name, "4");
  bm4.m_point = m2::PointD(41, 40);
  bm4.m_color.m_predefinedColor = kml::PredefinedColor::Blue;
  auto const * pBm4 = bmManager.GetEditSession().CreateBookmark(std::move(bm4), cat3);

  TEST_EQUAL(pBm3->GetGroupId(), pBm4->GetGroupId(), ());

  mark = GetBookmark(fm, m2::PointD(41, 40));

  // Should find last added valid result, there two results with the
  // same coordinates 3 and 4, but 4 was added later.
  TEST_EQUAL(kml::GetDefaultStr(mark->GetName()), "4", ());
  TEST_EQUAL(mark->GetColor(), kml::PredefinedColor::Blue, ());

  TEST_EQUAL(bmManager.GetUserMarkIds(mark->GetGroupId()).size(), 2, ());
  bmManager.GetEditSession().DeleteBookmark(mark->GetId());
  TEST_EQUAL(bmManager.GetUserMarkIds(cat3).size(), 1, ());

  DeleteCategoryFiles(arrCat);
}

namespace
{
void CheckPlace(Framework const & fm, std::shared_ptr<MwmInfo> const & mwmInfo, double lat, double lon,
                StringUtf8Multilang const & streetNames, std::string const & houseNumber)
{
  auto const info = fm.GetAddressAtPoint(mercator::FromLatLon(lat, lon));

  feature::NameParamsOut out;
  feature::GetReadableName({ streetNames, mwmInfo->GetRegionData(), languages::GetCurrentNorm(),
                             false /* allowTranslit */ }, out);

  TEST_EQUAL(info.GetStreetName(), out.primary, ());
  TEST_EQUAL(info.GetHouseNumber(), houseNumber, ());
}
}  // namespace

UNIT_TEST(Bookmarks_AddressInfo)
{
  // Maps added in constructor (we need minsk-pass.mwm only)
  Framework fm(kFrameworkParams);
  fm.DeregisterAllMaps();
  auto const regResult = fm.RegisterMap(platform::LocalCountryFile::MakeForTesting("minsk-pass"));
  fm.OnSize(800, 600);

  TEST_EQUAL(regResult.second, MwmSet::RegResult::Success, ());

  auto mwmInfo = regResult.first.GetInfo();

  TEST(mwmInfo != nullptr, ());

  StringUtf8Multilang streetNames;
  streetNames.AddString("default", "улица Карла Маркса");
  streetNames.AddString("int_name", "vulica Karla Marksa");
  streetNames.AddString("be", "вуліца Карла Маркса");
  streetNames.AddString("ru", "улица Карла Маркса");

  CheckPlace(fm, mwmInfo, 53.8964918, 27.555559, streetNames, "10" /* houseNumber */);
  CheckPlace(fm, mwmInfo, 53.8964365, 27.5554007, streetNames, "10" /* houseNumber */);
}

UNIT_TEST(Bookmarks_IllegalFileName)
{
  vector<string> const arrIllegal = {"?", "?|", "ч\"x", "|x:", "x<>y", "xy*地圖"};
  vector<string> const arrLegal =   {"",  "",   "чx",   "x",   "xy",   "xy地圖"};

  for (size_t i = 0; i < arrIllegal.size(); ++i)
    TEST_EQUAL(arrLegal[i], RemoveInvalidSymbols(arrIllegal[i]), ());
}

UNIT_TEST(Bookmarks_UniqueFileName)
{
  string const BASE = "SomeUniqueFileName";
  string const FILEBASE = "./" + BASE;
  string const FILENAME = FILEBASE + string{kKmlExtension};

  {
    FileWriter file(FILENAME);
    file.Write(FILENAME.data(), FILENAME.size());
  }

  string gen = GenerateUniqueFileName(".", BASE);
  TEST_NOT_EQUAL(gen, FILENAME, ());
  TEST_EQUAL(gen, FILEBASE + "1.kml", ());

  string const FILENAME1 = gen;
  {
    FileWriter file(FILENAME1);
    file.Write(FILENAME1.data(), FILENAME1.size());
  }
  gen = GenerateUniqueFileName(".", BASE);
  TEST_NOT_EQUAL(gen, FILENAME, ());
  TEST_NOT_EQUAL(gen, FILENAME1, ());
  TEST_EQUAL(gen, FILEBASE + "2.kml", ());

  FileWriter::DeleteFileX(FILENAME);
  FileWriter::DeleteFileX(FILENAME1);

  gen = GenerateUniqueFileName(".", BASE);
  TEST_EQUAL(gen, FILENAME, ());
}

UNIT_TEST(Bookmarks_AddingMoving)
{
  Framework fm(kFrameworkParams);
  fm.OnSize(800, 400);
  fm.ShowRect(m2::RectD(0, 0, 80, 40));

  m2::PointD constexpr globalPoint = m2::PointD(40, 20);
  m2::PointD const pixelPoint = fm.GtoP(globalPoint);

  BookmarkManager & bmManager = fm.GetBookmarkManager();
  bmManager.EnableTestMode(true);

  vector<string> const arrCat = {"cat1", "cat2"};
  auto const cat1 = bmManager.CreateBookmarkCategory(arrCat[0], false /* autoSave */);
  auto const cat2 = bmManager.CreateBookmarkCategory(arrCat[1], false /* autoSave */);

  kml::BookmarkData bm1;
  kml::SetDefaultStr(bm1.m_name, "name");
  bm1.m_point = globalPoint;
  auto const * pBm1 = bmManager.GetEditSession().CreateBookmark(std::move(bm1), cat1);
  Bookmark const * mark = GetBookmarkPxPoint(fm, pixelPoint);
  TEST_EQUAL(bmManager.GetCategoryName(mark->GetGroupId()), "cat1", ());

  kml::BookmarkData bm2;
  kml::SetDefaultStr(bm2.m_name, "name2");
  bm2.m_point = globalPoint;
  bm2.m_color.m_predefinedColor = kml::PredefinedColor::Blue;
  auto const * pBm11 = bmManager.GetEditSession().CreateBookmark(std::move(bm2), cat1);
  TEST_EQUAL(pBm1->GetGroupId(), pBm11->GetGroupId(), ());
  mark = GetBookmarkPxPoint(fm, pixelPoint);
  TEST_EQUAL(bmManager.GetCategoryName(mark->GetGroupId()), "cat1", ());
  TEST_EQUAL(kml::GetDefaultStr(mark->GetName()), "name2", ());
  TEST_EQUAL(mark->GetColor(), kml::PredefinedColor::Blue, ());

  // Edit name, type and category of bookmark
  kml::BookmarkData bm3;
  kml::SetDefaultStr(bm3.m_name, "name3");
  bm3.m_point = globalPoint;
  bm3.m_color.m_predefinedColor = kml::PredefinedColor::Green;
  auto const * pBm2 = bmManager.GetEditSession().CreateBookmark(std::move(bm3), cat2);
  TEST_NOT_EQUAL(pBm1->GetGroupId(), pBm2->GetGroupId(), ());
  TEST_EQUAL(bmManager.GetBmGroupsCount(), 2, ());
  mark = GetBookmarkPxPoint(fm, pixelPoint);
  TEST_EQUAL(bmManager.GetCategoryName(mark->GetGroupId()), "cat1", ());
  TEST_EQUAL(bmManager.GetUserMarkIds(cat1).size(), 2,
             ("Bookmark wasn't moved from one category to another"));
  TEST_EQUAL(kml::GetDefaultStr(mark->GetName()), "name2", ());
  TEST_EQUAL(mark->GetColor(), kml::PredefinedColor::Blue, ());

  DeleteCategoryFiles(arrCat);
}

UNIT_TEST(Bookmarks_Sorting)
{
  Framework fm(kFrameworkParams);
  fm.DeregisterAllMaps();
  fm.RegisterMap(platform::LocalCountryFile::MakeForTesting("World"));

  BookmarkManager & bmManager = fm.GetBookmarkManager();
  bmManager.EnableTestMode(true);

  auto constexpr kDay = std::chrono::hours{24};
  auto constexpr kWeek = 7 * kDay;
  auto constexpr kMonth = 31 * kDay;
  auto constexpr kYear = 365 * kDay;
  auto constexpr kUnknownTime = std::chrono::hours{0};
  auto const currentTime = kml::TimestampClock::now();

  auto const & c = classif();
  auto const setFeatureTypes = [&c](std::vector<std::string> const & readableTypes, kml::BookmarkData & bmData)
  {
    for (auto const & readableType : readableTypes)
    {
      auto const type = c.GetTypeByReadableObjectName(readableType);
      if (c.IsTypeValid(type))
      {
        auto const typeInd = c.GetIndexForType(type);
        bmData.m_featureTypes.push_back(typeInd);
      }
    }
  };

  struct TestMarkData
  {
    kml::MarkId m_markId;
    m2::PointD m_position;
    std::chrono::hours m_hoursSinceCreation;
    // Name is autogenerated from types.
    std::vector<std::string> m_types;
  };

  struct TestTrackData
  {
    kml::TrackId m_trackId;
    std::string m_name;
    std::chrono::hours m_hoursSinceCreation;
  };

  auto const kMoscowCenter = mercator::FromLatLon(55.750441, 37.6175138);

  auto const addrMoscow = fm.GetBookmarkManager().GetLocalizedRegionAddress(kMoscowCenter);

  double constexpr kNearR = 20 * 1000;
  m2::PointD const myPos = mercator::GetSmPoint(kMoscowCenter, -kNearR, 0.0);

  std::vector<TestMarkData> const testMarksData =
  {
    {0, mercator::GetSmPoint(myPos, kNearR * 0.07, 0.0), kDay + std::chrono::hours(1), {"historic-ruins"}},
    {1, mercator::GetSmPoint(myPos, kNearR * 0.06, 0.0), kUnknownTime, {"amenity-restaurant", "cuisine-sushi"}},
    {2, mercator::GetSmPoint(myPos, kNearR * 0.05, 0.0), kUnknownTime, {"shop-music", "shop-gift"}},
    {3, mercator::GetSmPoint(myPos, kNearR * 1.01, 0.0), kWeek + std::chrono::hours(2), {"historic-castle"}},
    {4, mercator::GetSmPoint(myPos, kNearR * 0.04, 0.0), kWeek + std::chrono::hours(3), {"amenity-fast_food"}},
    {5, mercator::GetSmPoint(myPos, kNearR * 1.02, 0.0), kMonth + std::chrono::hours(1), {"historic-memorial"}},
    {6, mercator::GetSmPoint(myPos, kNearR * 0.03, 0.0), kMonth + std::chrono::hours(2), {"shop-music"}},
    {7, mercator::GetSmPoint(myPos, kNearR * 1.05, 0.0), kUnknownTime, {"amenity-cinema"}},
    {8, mercator::GetSmPoint(myPos, kNearR * 0.02, 0.0), std::chrono::hours(1), {"leisure-stadium"}},
    {9, mercator::GetSmPoint(myPos, kNearR * 1.06, 0.0), kDay + std::chrono::hours(3), {"amenity-bar"}},
    {10, mercator::GetSmPoint(myPos, kNearR * 1.03, 0.0), kYear + std::chrono::hours(3), {"historic-castle"}},
    {11, m2::PointD(0.0, 0.0), kWeek + std::chrono::hours(1), {}},
    {12, mercator::GetSmPoint(myPos, kNearR * 1.04, 0.0), kDay + std::chrono::hours(2), {"shop-music"}},
  };

  std::vector<TestTrackData> const testTracksData =
  {
    {0, "Z Last Track", kDay + std::chrono::hours(1)},
    {1, "Middle Track", kUnknownTime},
    {2, "First Track", kMonth + std::chrono::hours(1)}
  };

  BookmarkManager::SortedBlocksCollection const expectedSortedByDistance =
  {
    {BookmarkManager::GetTracksSortedBlockName(), {}, {0, 1, 2}},
    {BookmarkManager::GetNearMeSortedBlockName(), {8, 6, 4, 2, 1, 0}, {}},
    {addrMoscow, {3, 5, 10, 12, 7, 9}, {}},
    {BookmarkManager::GetOthersSortedBlockName(), {11}, {}}
  };

  BookmarkManager::SortedBlocksCollection const expectedSortedByTime =
  {
    {BookmarkManager::GetTracksSortedBlockName(), {}, {0, 2, 1}},
    {BookmarkManager::GetSortedByTimeBlockName(BookmarkManager::SortedByTimeBlockType::WeekAgo), {8, 0, 12, 9}, {}},
    {BookmarkManager::GetSortedByTimeBlockName(BookmarkManager::SortedByTimeBlockType::MonthAgo), {11, 3, 4}, {}},
    {BookmarkManager::GetSortedByTimeBlockName(BookmarkManager::SortedByTimeBlockType::MoreThanMonthAgo), {5, 6}, {}},
    {BookmarkManager::GetSortedByTimeBlockName(BookmarkManager::SortedByTimeBlockType::MoreThanYearAgo), {10}, {}},
    {BookmarkManager::GetSortedByTimeBlockName(BookmarkManager::SortedByTimeBlockType::Others), {7, 2, 1}, {}}
  };

  BookmarkManager::SortedBlocksCollection const expectedSortedByType =
  {
    {BookmarkManager::GetTracksSortedBlockName(), {}, {0, 1, 2}},
    {GetLocalizedBookmarkBaseType(BookmarkBaseType::Sights), {0, 3, 5, 10}, {}},
    {GetLocalizedBookmarkBaseType(BookmarkBaseType::Food), {9, 4, 1}, {}},
    {GetLocalizedBookmarkBaseType(BookmarkBaseType::Shop), {12, 6, 2}, {}},
    {BookmarkManager::GetOthersSortedBlockName(), {8, 11, 7}, {}}
  };

  BookmarkManager::SortedBlocksCollection const expectedSortedByName =
  {
    {BookmarkManager::GetTracksSortedBlockName(), {}, {2, 1, 0}},
    {BookmarkManager::GetBookmarksSortedBlockName(), {11, 9, 7, 4, 1, 10, 3, 5, 0, 8, 12, 6, 2}, {}},
  };

  auto const kBerlin1 = mercator::FromLatLon(52.5038994, 13.3982282);
  auto const kBerlin2 = mercator::FromLatLon(52.5007139, 13.4005403);
  auto const kBerlin3 = mercator::FromLatLon(52.437256, 13.3026692);
  auto const kMinsk1 = mercator::FromLatLon(53.9040184, 27.5567595);
  auto const kMinsk2 = mercator::FromLatLon(53.9042397, 27.5593612);
  auto const kMinsk3 = mercator::FromLatLon(53.9005419, 27.5416291);
  auto const kMoscow1 = mercator::FromLatLon(55.7640256, 37.5922593);
  auto const kMoscow2 = mercator::FromLatLon(55.7496148, 37.6137586);
  auto const kGreenland = mercator::FromLatLon(62.730205, -46.939619);
  auto const kWashington = mercator::FromLatLon(38.9005971, -77.0385621);
  auto const kKathmandu = mercator::FromLatLon(27.6739262, 85.3255313);
  auto const kVladimir = mercator::FromLatLon(56.2102137, 40.5195297);
  auto const kBermuda = mercator::FromLatLon(32.2946391, -64.7820014);

  std::vector<TestMarkData> const testMarksData2 =
  {
    {100,  kBerlin1, kUnknownTime, {"amenity", "building", "wheelchair-yes", "tourism-museum"}},
    {101,  kGreenland, kUnknownTime, {}},
    {102,  kVladimir, kUnknownTime, {"tourism-artwork"}},
    {103,  kKathmandu, kUnknownTime, {"internet_access-wlan", "wheelchair-no", "amenity-cafe"}},
    {104,  kMinsk1, kUnknownTime, {"amenity-place_of_worship"}},
    {105,  kBerlin2, kUnknownTime, {"building", "amenity-place_of_worship-christian"}},
    {106,  kMoscow2, kUnknownTime, {"tourism-museum"}},
    {107,  kMinsk2, kUnknownTime, {"amenity-restaurant"}},
    {108,  kMinsk3, kUnknownTime, {"amenity-place_of_worship-jewish"}},
    {109,  kWashington, kUnknownTime, {"amenity-restaurant"}},
    {110, kBerlin3, kUnknownTime, {"tourism-museum"}},
    {111, kBermuda, kUnknownTime, {"amenity-cafe"}},
    {112, kMoscow1, kUnknownTime, {"leisure-park"}},
  };

  m2::PointD const myPos2 = mercator::GetSmPoint(kVladimir, 2.0 * kNearR, 2.0 * kNearR);

  auto const addrBerlin = fm.GetBookmarkManager().GetLocalizedRegionAddress(kBerlin1);
  auto const addrMinsk = fm.GetBookmarkManager().GetLocalizedRegionAddress(kMinsk1);
  auto const addrGreenland = fm.GetBookmarkManager().GetLocalizedRegionAddress(kGreenland);
  auto const addrWashington = fm.GetBookmarkManager().GetLocalizedRegionAddress(kWashington);
  auto const addrKathmandu = fm.GetBookmarkManager().GetLocalizedRegionAddress(kKathmandu);
  auto const addrVladimir = fm.GetBookmarkManager().GetLocalizedRegionAddress(kVladimir);
  auto const addrBermuda = fm.GetBookmarkManager().GetLocalizedRegionAddress(kBermuda);

  BookmarkManager::SortedBlocksCollection const expectedSortedByDistance2 =
  {
    {addrVladimir, {102}, {}},
    {addrMoscow, {106, 112}, {}},
    {addrMinsk, {107, 104, 108}, {}},
    {addrBerlin, {100, 105, 110}, {}},
    {addrGreenland, {101}, {}},
    {addrKathmandu, {103}, {}},
    {addrWashington, {109}, {}},
    {addrBermuda, {111}, {}},
  };

  BookmarkManager::SortedBlocksCollection const expectedSortedByType2 =
  {
    {GetLocalizedBookmarkBaseType(BookmarkBaseType::Food), {111, 109, 107, 103}, {}},
    {GetLocalizedBookmarkBaseType(BookmarkBaseType::Museum), {110, 106, 100}, {}},
    {GetLocalizedBookmarkBaseType(BookmarkBaseType::ReligiousPlace), {108, 105, 104}, {}},
    {BookmarkManager::GetOthersSortedBlockName(), {112, 102, 101}, {}}
  };

  std::vector<TestMarkData> const testMarksData3 =
  {
    {200,  {0.0, 0.0}, kUnknownTime, {"tourism-museum"}},
    {201,  {0.0, 0.0}, kUnknownTime, {"leisure-park"}},
    {202,  {0.0, 0.0}, kUnknownTime, {"tourism-artwork"}},
    {203,  {0.0, 0.0}, kUnknownTime, {"amenity-cafe"}},
    {204,  {0.0, 0.0}, kUnknownTime, {"amenity-place_of_worship"}},
    {205,  {0.0, 0.0}, kUnknownTime, {"amenity-place_of_worship-christian"}},
  };

  std::vector<TestMarkData> const testMarksData4 =
  {
    {300,  {0.0, 0.0}, kUnknownTime, {"tourism-museum"}},
    {301,  {0.0, 0.0}, kUnknownTime, {"leisure-park"}},
    {302,  {0.0, 0.0}, kUnknownTime, {"tourism-artwork"}},
    {303,  {0.0, 0.0}, kUnknownTime, {"amenity-cafe"}},
    {304,  {0.0, 0.0}, kUnknownTime, {"amenity-place_of_worship"}},
    {305,  {0.0, 0.0}, kUnknownTime, {"tourism-hotel"}},
  };

  BookmarkManager::SortedBlocksCollection expectedSortedByType4 =
  {
    {GetLocalizedBookmarkBaseType(BookmarkBaseType::Hotel), {305}, {}},
    {BookmarkManager::GetOthersSortedBlockName(), {304, 303, 302, 301, 300}, {}}
  };

  std::vector<TestTrackData> const testTracksData5 =
  {
    {40, "t", kUnknownTime},
    {41, "a", kUnknownTime},
    {42, "u", std::chrono::hours(1)},
    {43, "a", kUnknownTime}
  };

  BookmarkManager::SortedBlocksCollection const expectedSortedByTime5 =
  {
    {BookmarkManager::GetTracksSortedBlockName(), {}, {42, 40, 41, 43}}
  };

  std::vector<TestTrackData> const testTracksData6 =
  {
    {50, "-11", kUnknownTime},
    {51, "41", kUnknownTime},
    {52, "", kUnknownTime}
  };

  auto const fillCategory = [&](kml::MarkGroupId cat, std::vector<TestMarkData> const & marksData, std::vector<TestTrackData> const & tracksData)
  {
    auto es = bmManager.GetEditSession();
    for (auto const & [id, position, hours, types] : marksData)
    {
      kml::BookmarkData bmData;
      bmData.m_id = id;
      bmData.m_name = kml::LocalizableString{{kml::kDefaultLangCode,
          std::reduce(types.begin(), types.end(), std::string{}, [](auto const & sum, auto const & type) { return sum + type + " "; })}};
      bmData.m_point = position;
      if (hours != kUnknownTime)
        bmData.m_timestamp = currentTime - hours;
      setFeatureTypes(types, bmData);
      auto const * bm = es.CreateBookmark(std::move(bmData));
      es.AttachBookmark(bm->GetId(), cat);
    }
    for (auto const & [id, name, hours] : tracksData)
    {
      kml::TrackData trackData;
      trackData.m_id = id;
      trackData.m_name = kml::LocalizableString{{kml::kDefaultLangCode, name}};
      trackData.m_geometry.Assign({{{0.0, 0.0}, 1}, {{1.0, 0.0}, 2}});
      if (hours != kUnknownTime)
        trackData.m_timestamp = currentTime - hours;
      auto const * track = es.CreateTrack(std::move(trackData));
      es.AttachTrack(track->GetId(), cat);
    }
  };

  auto const printBlocks = [](std::string const & /* name */, BookmarkManager::SortedBlocksCollection const & /* blocks */)
  {
    // Uncomment for debug output.
    /*
    LOG(LINFO, ("\nvvvvvvvvvv   ", name, "   vvvvvvvvvv"));
    for (auto const & [blockName, markIds, trackIds] : blocks)
    {
      LOG(LINFO, ("========== ", blockName));
      for (auto const trackId : trackIds)
        LOG(LINFO, ("   track", trackId));
      for (auto const markId : markIds)
        LOG(LINFO, ("   bookmark", markId));
    }
    */
  };

  auto const getSortedBokmarks = [&bmManager](kml::MarkGroupId groupId, BookmarkManager::SortingType sortingType,
                                              bool hasMyPosition, m2::PointD const & myPosition)
  {
    BookmarkManager::SortedBlocksCollection sortedBlocks;
    BookmarkManager::SortParams params;
    params.m_groupId = groupId;
    params.m_sortingType = sortingType;
    params.m_hasMyPosition = hasMyPosition;
    params.m_myPosition = myPosition;
    params.m_onResults = [&sortedBlocks](BookmarkManager::SortedBlocksCollection && results,
                                         BookmarkManager::SortParams::Status /* status */)
    {
      sortedBlocks = std::move(results);
    };
    bmManager.GetSortedCategory(params);
    return sortedBlocks;
  };

  {
    kml::MarkGroupId catId = bmManager.CreateBookmarkCategory("test", false);
    fillCategory(catId, testMarksData, testTracksData);

    std::vector<BookmarkManager::SortingType> const expectedSortingTypes =
    {
      BookmarkManager::SortingType::ByType,
      BookmarkManager::SortingType::ByDistance,
      BookmarkManager::SortingType::ByTime,
      BookmarkManager::SortingType::ByName
    };

    auto const sortingTypes = bmManager.GetAvailableSortingTypes(catId, true);
    TEST(sortingTypes == expectedSortingTypes, ());

    auto const sortedByTime = getSortedBokmarks(catId, BookmarkManager::SortingType::ByTime, true, myPos);
    printBlocks("Sorted by time", sortedByTime);
    TEST(sortedByTime == expectedSortedByTime, ());

    auto const sortedByType = getSortedBokmarks(catId, BookmarkManager::SortingType::ByType, true, myPos);
    printBlocks("Sorted by type", sortedByType);
    TEST(sortedByType == expectedSortedByType, ());

    auto const sortedByDistance = getSortedBokmarks(catId, BookmarkManager::SortingType::ByDistance, true, myPos);
    printBlocks("Sorted by distance", sortedByDistance);
    TEST(sortedByDistance == expectedSortedByDistance, ());

    auto const sortedByName = getSortedBokmarks(catId, BookmarkManager::SortingType::ByName, false, myPos);
    printBlocks("Sorted by name", sortedByName);
    TEST(sortedByName == expectedSortedByName, ());
  }

  {
    kml::MarkGroupId const catId2 = bmManager.CreateBookmarkCategory("test2", false);
    fillCategory(catId2, testMarksData2, {} /* tracksData */);

    std::vector<BookmarkManager::SortingType> const expectedSortingTypes2 =
    {
      BookmarkManager::SortingType::ByType,
      BookmarkManager::SortingType::ByDistance,
      BookmarkManager::SortingType::ByName,
    };

    auto const sortingTypes2 = bmManager.GetAvailableSortingTypes(catId2, true);
    TEST(sortingTypes2 == expectedSortingTypes2, ());

    std::vector<BookmarkManager::SortingType> expectedSortingTypes2_2 =
    {
      BookmarkManager::SortingType::ByType,
      BookmarkManager::SortingType::ByName,
    };

    auto const sortingTypes2_2 = bmManager.GetAvailableSortingTypes(catId2, false);
    TEST(sortingTypes2_2 == expectedSortingTypes2_2, ());

    auto const sortedByType2 = getSortedBokmarks(catId2, BookmarkManager::SortingType::ByType, false, {});
    printBlocks("Sorted by type 2", sortedByType2);
    TEST(sortedByType2 == expectedSortedByType2, ());

    auto const sortedByDistance2 = getSortedBokmarks(catId2, BookmarkManager::SortingType::ByDistance,
                                                                  true, myPos2);
    printBlocks("Sorted by distance 2", sortedByDistance2);
    TEST(sortedByDistance2 == expectedSortedByDistance2, ());
  }

  {
    kml::MarkGroupId const catId3 = bmManager.CreateBookmarkCategory("test3", false);
    fillCategory(catId3, testMarksData3, {} /* tracksData */);

    std::vector<BookmarkManager::SortingType> const expectedSortingTypes3 = {BookmarkManager::SortingType::ByName};
    auto const sortingTypes3 = bmManager.GetAvailableSortingTypes(catId3, false);
    TEST(sortingTypes3 == expectedSortingTypes3, ());
  }

  {
    kml::MarkGroupId const catId4 = bmManager.CreateBookmarkCategory("test4", false);
    fillCategory(catId4, testMarksData4, {} /* tracksData */);

    std::vector<BookmarkManager::SortingType> const expectedSortingTypes4 = { BookmarkManager::SortingType::ByType, BookmarkManager::SortingType::ByName };
    auto const sortingTypes4 = bmManager.GetAvailableSortingTypes(catId4, false);
    TEST(sortingTypes4 == expectedSortingTypes4, ());

    auto const sortedByType4 = getSortedBokmarks(catId4, BookmarkManager::SortingType::ByType, false, {});
    printBlocks("Sorted by type 4", sortedByType4);
    TEST(sortedByType4 == expectedSortedByType4, ());
  }

  {
    kml::MarkGroupId const catId5 = bmManager.CreateBookmarkCategory("test5", false);
    fillCategory(catId5, {} /* marksData */, testTracksData5);
    std::vector<BookmarkManager::SortingType> const expectedSortingTypes5 = { BookmarkManager::SortingType::ByTime, BookmarkManager::SortingType::ByName };

    auto const sortingTypes5 = bmManager.GetAvailableSortingTypes(catId5, false);
    TEST(sortingTypes5 == expectedSortingTypes5, ());

    auto const sortedByTime5 = getSortedBokmarks(catId5, BookmarkManager::SortingType::ByTime, false, {});
    printBlocks("Sorted by time 5", sortedByTime5);
    TEST(sortedByTime5 == expectedSortedByTime5, ());
  }

  {
    kml::MarkGroupId const catId6 = bmManager.CreateBookmarkCategory("test6", false);
    fillCategory(catId6, {} /* marksData */, testTracksData6);
    std::vector<BookmarkManager::SortingType> const expectedSortingTypes6 = {BookmarkManager::SortingType::ByName};

    auto const sortingTypes6 = bmManager.GetAvailableSortingTypes(catId6, false);
    TEST(sortingTypes6 == expectedSortingTypes6, ());
  }
}

namespace
{
char const * kmlString2 =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<kml xmlns=\"http://earth.google.com/kml/2.1\">"
    "<Document>"
     "<name>busparkplatz</name>"
     "<Folder>"
      "<name>Waypoint</name>"
      "<Style id=\"poiIcon37\">"
       "<IconStyle>"
       "<scale>1</scale>"
       "<Icon><href>http://maps.google.com/mapfiles/kml/shapes/placemark_circle.png</href></Icon>"
       "<hotSpot x=\"0.5\" y=\"0\" xunits=\"fraction\" yunits=\"fraction\"/>"
       "</IconStyle>"
      "</Style>"
      "<Placemark>"
       "<name>[P] Silvrettastrae[Bieler Hhe]</name>"
       "<description></description>"
       "<styleUrl>#poiIcon37</styleUrl>"
       "<Point>"
        "<coordinates>10.09237,46.91741,0</coordinates>"
       "</Point>"
      "</Placemark>"
    "</Folder>"
   "</Document>"
   "</kml>";
}

UNIT_CLASS_TEST(Runner, Bookmarks_InnerFolder)
{
  BookmarkManager bmManager(BM_CALLBACKS);
  bmManager.EnableTestMode(true);

  BookmarkManager::KMLDataCollection kmlDataCollection;

  kmlDataCollection.emplace_back("" /* filePath */,
                                 LoadKmlData(MemReader(kmlString2, strlen(kmlString2)), KmlFileType::Text));
  bmManager.CreateCategories(std::move(kmlDataCollection));
  auto const & groupIds = bmManager.GetUnsortedBmGroupsIdList();
  TEST_EQUAL(groupIds.size(), 1, ());
  TEST_EQUAL(bmManager.GetUserMarkIds(groupIds.front()).size(), 1, ());
}

UNIT_CLASS_TEST(Runner, BookmarkCategory_EmptyName)
{
  BookmarkManager bmManager(BM_CALLBACKS);
  bmManager.EnableTestMode(true);

  auto const catId = bmManager.CreateBookmarkCategory("", false /* autoSave */);
  kml::BookmarkData bm;
  bm.m_point = m2::PointD(0, 0);
  bmManager.GetEditSession().CreateBookmark(std::move(bm), catId);

  TEST(bmManager.SaveBookmarkCategory(catId), ());

  bmManager.GetEditSession().SetCategoryName(catId, "xxx");

  TEST(bmManager.SaveBookmarkCategory(catId), ());

  vector<string> const arrFiles = {"Bookmarks", "xxx"};
  DeleteCategoryFiles(arrFiles);
}

namespace
{
// Do check for "bad" names without CDATA markers.
char const * kmlString3 =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<kml xmlns=\"http://earth.google.com/kml/2.1\">"
    "<Document>"
     "<name>3663 and M <![CDATA[&]]> J Seafood Branches</name>"
     "<visibility>1</visibility>"
      "<Placemark>"
       "<name>![X1]{X2}(X3)</name>"
       "<Point>"
        "<coordinates>50, 50</coordinates>"
       "</Point>"
      "</Placemark>"
   "</Document>"
   "</kml>";

  bool EqualBookmarks(Bookmark const & b1, Bookmark const & b2)
  {
    if (b1.GetName() != b2.GetName())
      return false;
    if (b1.GetDescription() != b2.GetDescription())
      return false;
    if (b1.GetColor() != b2.GetColor())
      return false;
    if (b1.GetScale() != b2.GetScale())
      return false;
    if (!base::AlmostEqualAbs(b1.GetPivot(), b2.GetPivot(), 1e-6 /* eps*/))
      return false;

    // do not check timestamp
    return true;
  }
}

UNIT_CLASS_TEST(Runner, Bookmarks_SpecialXMLNames)
{
  BookmarkManager bmManager(BM_CALLBACKS);
  bmManager.EnableTestMode(true);

  auto const file1Name = "file1";
  BookmarkManager::KMLDataCollection kmlDataCollection1;
  kmlDataCollection1.emplace_back(file1Name /* filePath */,
                                 LoadKmlData(MemReader(kmlString3, strlen(kmlString3)), KmlFileType::Text));
  bmManager.CreateCategories(std::move(kmlDataCollection1));

  auto const & groupIds = bmManager.GetSortedBmGroupIdList();
  TEST_EQUAL(groupIds.size(), 1, ());

  auto const catId = groupIds.front();
  TEST_EQUAL(bmManager.GetUserMarkIds(catId).size(), 1, ());

  string expectedName = "3663 and M & J Seafood Branches";
  TEST_EQUAL(bmManager.GetCategoryName(catId), expectedName, ());

  // change category name to avoid merging it with the second one
  expectedName = "test";
  bmManager.GetEditSession().SetCategoryName(catId, expectedName);

  TEST(bmManager.SaveBookmarkCategory(catId), ());

  auto const fileName = bmManager.GetCategoryFileName(catId);
  auto const fileNameTmp = fileName + ".backup";
  TEST(base::CopyFileX(fileName, fileNameTmp), ());

  bmManager.GetEditSession().DeleteBmCategory(catId);

  auto const file2Name = "file2";
  BookmarkManager::KMLDataCollection kmlDataCollection2;
  kmlDataCollection2.emplace_back(file1Name /* filePath */, LoadKmlFile(fileNameTmp, GetActiveKmlFileType()));
  bmManager.CreateCategories(std::move(kmlDataCollection2));

  BookmarkManager::KMLDataCollection kmlDataCollection3;
  kmlDataCollection3.emplace_back(file2Name /* filePath */,
                                  LoadKmlData(MemReader(kmlString3, strlen(kmlString3)), KmlFileType::Text));

  bmManager.CreateCategories(std::move(kmlDataCollection3));

  TEST_EQUAL(bmManager.GetBmGroupsCount(), 2, ());
  auto const catId2 = bmManager.GetSortedBmGroupIdList().front();
  auto const catId3 = bmManager.GetSortedBmGroupIdList().back();

  TEST_EQUAL(bmManager.GetUserMarkIds(catId2).size(), 1, ());
  TEST_EQUAL(bmManager.GetCategoryName(catId2), expectedName, ());
  TEST_EQUAL(bmManager.GetCategoryFileName(catId2), file1Name, ());
  TEST_EQUAL(bmManager.GetCategoryFileName(catId3), file2Name, ());

  auto const bmId1 = *bmManager.GetUserMarkIds(catId2).begin();
  auto const * bm1 = bmManager.GetBookmark(bmId1);
  auto const bmId2 = *bmManager.GetUserMarkIds(catId3).begin();
  auto const * bm2 = bmManager.GetBookmark(bmId2);
  TEST(EqualBookmarks(*bm1, *bm2), ());
  TEST_EQUAL(kml::GetDefaultStr(bm1->GetName()), "![X1]{X2}(X3)", ());

  TEST(base::DeleteFileX(fileNameTmp), ());
}

UNIT_CLASS_TEST(Runner, TrackParsingTest_1)
{
  string const kmlFile = GetPlatform().TestsDataPathForFile("kml_test_data/track.kml");
  BookmarkManager bmManager(BM_CALLBACKS);
  bmManager.EnableTestMode(true);

  BookmarkManager::KMLDataCollection kmlDataCollection;
  kmlDataCollection.emplace_back(kmlFile, LoadKmlFile(kmlFile, KmlFileType::Text));
  TEST(kmlDataCollection.back().second, ("KML can't be loaded"));

  bmManager.CreateCategories(std::move(kmlDataCollection));
  TEST_EQUAL(bmManager.GetBmGroupsCount(), 1, ());

  auto catId = bmManager.GetUnsortedBmGroupsIdList().front();
  TEST_EQUAL(bmManager.GetTrackIds(catId).size(), 4, ());

  array<string, 4> const names = {{"Option1", "Pakkred1", "Pakkred2", "Pakkred3"}};
  array<dp::Color, 4> constexpr col = {{dp::Color(230, 0, 0, 255),
                                    dp::Color(171, 230, 0, 255),
                                    dp::Color(0, 230, 117, 255),
                                    dp::Color(0, 59, 230, 255)}};
  array<double, 4> constexpr length = {{3525.46839061, 27172.44338132, 27046.0456586, 23967.35765800}};
  array<geometry::Altitude, 4> constexpr altitudes = {{0, 27, -3, -2}};
  size_t i = 0;
  for (auto const trackId : bmManager.GetTrackIds(catId))
  {
    auto const * track = bmManager.GetTrack(trackId);
    auto const & geom = track->GetSingleGeometry();

    TEST_EQUAL(geom[0].GetAltitude(), altitudes[i], ());
    TEST_EQUAL(names[i], track->GetName(), ());
    TEST_ALMOST_EQUAL_ABS(track->GetLengthMeters(), length[i], 1.0E-6, ());
    TEST_GREATER(track->GetLayerCount(), 0, ());
    TEST_EQUAL(col[i], track->GetColor(0), ());
    ++i;
  }
}

UNIT_CLASS_TEST(Runner, FillEmptyTrackNames)
{
  BookmarkManager bmManager(BM_CALLBACKS);
  bmManager.EnableTestMode(true);

  string const kmlFile1 = GetPlatform().TestsDataPathForFile("gpx_test_data/empty_names1.gpx");
  auto fileData1 = LoadKmlFile(kmlFile1, KmlFileType::Gpx);
  TEST_EQUAL(fileData1->m_categoryData.m_name[kml::kDefaultLangCode], "empty_names1", ());
  TEST_EQUAL(fileData1->m_tracksData[0].m_name[kml::kDefaultLangCode], "empty_names1 1", ());
  TEST_EQUAL(fileData1->m_tracksData[1].m_name[kml::kDefaultLangCode], "empty_names1 2", ());

  string const kmlFile2 = GetPlatform().TestsDataPathForFile("gpx_test_data/empty_names2.gpx");
  auto fileData2 = LoadKmlFile(kmlFile2, KmlFileType::Gpx);
  TEST_EQUAL(fileData2->m_categoryData.m_name[kml::kDefaultLangCode], "empty_names2", ());
  TEST_EQUAL(fileData2->m_tracksData[0].m_name[kml::kDefaultLangCode], "empty_names2", ());
}

UNIT_CLASS_TEST(Runner, TrackParsingTest_2)
{
  string const kmlFile = GetPlatform().TestsDataPathForFile("kml_test_data/track-from-google-earth.kml");
  BookmarkManager bmManager(BM_CALLBACKS);
  bmManager.EnableTestMode(true);

  BookmarkManager::KMLDataCollection kmlDataCollection;
  kmlDataCollection.emplace_back(kmlFile, LoadKmlFile(kmlFile, KmlFileType::Text));

  TEST(kmlDataCollection.back().second, ("KML can't be loaded"));
  bmManager.CreateCategories(std::move(kmlDataCollection));
  TEST_EQUAL(bmManager.GetBmGroupsCount(), 1, ());

  auto catId = bmManager.GetUnsortedBmGroupsIdList().front();
  TEST_EQUAL(bmManager.GetTrackIds(catId).size(), 1, ());
  auto const trackId = *bmManager.GetTrackIds(catId).begin();
  auto const * track = bmManager.GetTrack(trackId);
  TEST_EQUAL(track->GetName(), "XY", ());
  TEST_GREATER(track->GetLayerCount(), 0, ());
  TEST_EQUAL(track->GetColor(0), dp::Color(57, 255, 32, 255), ());
}

UNIT_CLASS_TEST(Runner, Bookmarks_Listeners)
{
  struct Changes
  {
    set<kml::MarkId> m_createdMarks;
    set<kml::MarkId> m_updatedMarks;
    set<kml::MarkId> m_deletedMarks;
    map<kml::MarkGroupId, kml::MarkIdSet> m_attachedMarks;
    map<kml::MarkGroupId, kml::MarkIdSet> m_detachedMarks;
  };

  Changes resultChanges;
  auto const checkNotifications = [&resultChanges](Changes & changes)
  {
    TEST_EQUAL(changes.m_createdMarks, resultChanges.m_createdMarks, ());
    TEST_EQUAL(changes.m_updatedMarks, resultChanges.m_updatedMarks, ());
    TEST_EQUAL(changes.m_deletedMarks, resultChanges.m_deletedMarks, ());
    TEST_EQUAL(changes.m_attachedMarks, resultChanges.m_attachedMarks, ());
    TEST_EQUAL(changes.m_detachedMarks, resultChanges.m_detachedMarks, ());
    changes = Changes();
    resultChanges = Changes();
  };

  bool bookmarksChanged = false;
  auto const checkBookmarksChanges = [&bookmarksChanged](bool expected)
  {
    TEST_EQUAL(bookmarksChanged, expected, ());
    bookmarksChanged = false;
  };

  auto const onCreate = [&resultChanges](vector<BookmarkInfo> const & marks)
  {
    for (auto const & mark : marks)
      resultChanges.m_createdMarks.insert(mark.m_bookmarkId);
  };
  auto const onUpdate = [&resultChanges](vector<BookmarkInfo> const & marks)
  {
    for (auto const & mark : marks)
      resultChanges.m_updatedMarks.insert(mark.m_bookmarkId);
  };
  auto const onDelete = [&resultChanges](vector<kml::MarkId> const & marks)
  {
    resultChanges.m_deletedMarks.insert(marks.begin(), marks.end());
  };
  auto const onAttach = [&resultChanges](vector<BookmarkGroupInfo> const & groupMarksCollection)
  {
    for (auto const & group : groupMarksCollection)
      resultChanges.m_attachedMarks[group.m_groupId].insert(group.m_bookmarkIds.begin(), group.m_bookmarkIds.end());
  };
  auto const onDetach = [&resultChanges](vector<BookmarkGroupInfo> const & groupMarksCollection)
  {
    for (auto const & group : groupMarksCollection)
      resultChanges.m_detachedMarks[group.m_groupId].insert(group.m_bookmarkIds.begin(), group.m_bookmarkIds.end());
  };

  BookmarkManager::Callbacks callbacks(
    []() -> StringsBundle const &
    {
      static StringsBundle const dummyBundle;
      return dummyBundle;
    },
    static_cast<BookmarkManager::Callbacks::GetSeacrhAPIFn>(nullptr),
    onCreate, onUpdate, onDelete, onAttach, onDetach);

  BookmarkManager bmManager(std::move(callbacks));
  bmManager.SetBookmarksChangedCallback([&bookmarksChanged]() { bookmarksChanged = true; });
  bmManager.EnableTestMode(true);
  Changes expectedChanges;

  auto const catId = bmManager.CreateBookmarkCategory("Default", false /* autoSave */);

  {
    auto editSession = bmManager.GetEditSession();
    kml::BookmarkData data0;
    kml::SetDefaultStr(data0.m_name, "name 0");
    data0.m_point = m2::PointD(0.0, 0.0);
    auto * bookmark0 = editSession.CreateBookmark(std::move(data0));
    editSession.AttachBookmark(bookmark0->GetId(), catId);
    expectedChanges.m_createdMarks.insert(bookmark0->GetId());
    expectedChanges.m_attachedMarks[catId].insert(bookmark0->GetId());

    kml::BookmarkData data1;
    kml::SetDefaultStr(data1.m_name, "name 1");
    auto * bookmark1 = editSession.CreateBookmark(std::move(data1));
    editSession.AttachBookmark(bookmark1->GetId(), catId);

    editSession.DeleteBookmark(bookmark1->GetId());
  }
  checkNotifications(expectedChanges);
  checkBookmarksChanges(true);

  auto const markId0 = *bmManager.GetUserMarkIds(catId).begin();
  bmManager.GetEditSession().GetBookmarkForEdit(markId0)->SetName("name 3", kml::kDefaultLangCode);
  expectedChanges.m_updatedMarks.insert(markId0);

  checkNotifications(expectedChanges);
  checkBookmarksChanges(true);

  {
    auto editSession = bmManager.GetEditSession();
    editSession.GetBookmarkForEdit(markId0)->SetName("name 4", kml::kDefaultLangCode);
    editSession.DeleteBookmark(markId0);
    expectedChanges.m_deletedMarks.insert(markId0);
    expectedChanges.m_detachedMarks[catId].insert(markId0);

    kml::BookmarkData data;
    kml::SetDefaultStr(data.m_name, "name 5");
    data.m_point = m2::PointD(0.0, 0.0);
    auto * bookmark1 = editSession.CreateBookmark(std::move(data));
    expectedChanges.m_createdMarks.insert(bookmark1->GetId());
  }
  checkNotifications(expectedChanges);
  checkBookmarksChanges(true);

  // Test postponed notifications.
  bmManager.SetNotificationsEnabled(false);

  Changes postponedChanges;
  {
    auto editSession = bmManager.GetEditSession();
    kml::BookmarkData data;
    kml::SetDefaultStr(data.m_name, "name 6");
    data.m_point = m2::PointD(0.0, 0.0);
    auto * bookmark = editSession.CreateBookmark(std::move(data));
    editSession.AttachBookmark(bookmark->GetId(), catId);
    postponedChanges.m_createdMarks.insert(bookmark->GetId());
    postponedChanges.m_attachedMarks[catId].insert(bookmark->GetId());
  }
  checkNotifications(expectedChanges);
  checkBookmarksChanges(true);

  {
    auto editSession = bmManager.GetEditSession();
    kml::BookmarkData data;
    kml::SetDefaultStr(data.m_name, "name 7");
    data.m_point = m2::PointD(0.0, 0.0);
    auto * bookmark = editSession.CreateBookmark(std::move(data));
    editSession.AttachBookmark(bookmark->GetId(), catId);
    postponedChanges.m_createdMarks.insert(bookmark->GetId());
    postponedChanges.m_attachedMarks[catId].insert(bookmark->GetId());
  }
  checkNotifications(expectedChanges);
  checkBookmarksChanges(true);

  bmManager.SetNotificationsEnabled(true);
  checkNotifications(postponedChanges);
  checkBookmarksChanges(false);
}

UNIT_CLASS_TEST(Runner, Bookmarks_AutoSave)
{
  BookmarkManager bmManager(BM_CALLBACKS);
  bmManager.EnableTestMode(true);

  kml::MarkId bmId0;
  auto const catId = bmManager.CreateBookmarkCategory("test");
  {
    kml::BookmarkData data0;
    data0.m_point = m2::PointD(0.0, 0.0);
    kml::SetDefaultStr(data0.m_name, "name 0");
    auto editSession = bmManager.GetEditSession();
    bmId0 = editSession.CreateBookmark(std::move(data0))->GetId();
    editSession.AttachBookmark(bmId0, catId);
  }
  auto const fileName = bmManager.GetCategoryFileName(catId);

  auto kmlData = LoadKmlFile(fileName, GetActiveKmlFileType());
  TEST(kmlData != nullptr, ());
  TEST_EQUAL(kmlData->m_bookmarksData.size(), 1, ());
  TEST_EQUAL(kml::GetDefaultStr(kmlData->m_bookmarksData.front().m_name), "name 0", ());

  {
    auto editSession = bmManager.GetEditSession();
    editSession.GetBookmarkForEdit(bmId0)->SetName("name 0 renamed", kml::kDefaultLangCode);

    kml::BookmarkData data1;
    kml::SetDefaultStr(data1.m_name, "name 1");
    auto bmId = editSession.CreateBookmark(std::move(data1))->GetId();
    editSession.AttachBookmark(bmId, catId);

    kml::BookmarkData data2;
    kml::SetDefaultStr(data2.m_name, "name 2");
    bmId = editSession.CreateBookmark(std::move(data2))->GetId();
    editSession.AttachBookmark(bmId, catId);

    kml::BookmarkData data3;
    kml::SetDefaultStr(data3.m_name, "name 3");
    bmId = editSession.CreateBookmark(std::move(data3))->GetId();
    editSession.AttachBookmark(bmId, catId);

    kmlData = LoadKmlFile(fileName, GetActiveKmlFileType());
    TEST(kmlData != nullptr, ());
    TEST_EQUAL(kmlData->m_bookmarksData.size(), 1, ());
    TEST_EQUAL(kml::GetDefaultStr(kmlData->m_bookmarksData.front().m_name), "name 0", ());
  }

  kmlData = LoadKmlFile(fileName, GetActiveKmlFileType());
  TEST(kmlData != nullptr, ());
  TEST_EQUAL(kmlData->m_bookmarksData.size(), 4, ());
  TEST_EQUAL(kml::GetDefaultStr(kmlData->m_bookmarksData.front().m_name), "name 0 renamed", ());

  bmManager.GetEditSession().DeleteBookmark(bmId0);

  kmlData = LoadKmlFile(fileName, GetActiveKmlFileType());
  TEST(kmlData != nullptr, ());
  TEST_EQUAL(kmlData->m_bookmarksData.size(), 3, ());
  TEST_EQUAL(kml::GetDefaultStr(kmlData->m_bookmarksData.front().m_name), "name 1", ());

  auto const catId2 = bmManager.CreateBookmarkCategory("test 2");
  auto const movedBmId = *bmManager.GetUserMarkIds(catId).begin();
  bmManager.GetEditSession().MoveBookmark(movedBmId, catId, catId2);

  kmlData = LoadKmlFile(fileName, GetActiveKmlFileType());
  TEST(kmlData != nullptr, ());
  TEST_EQUAL(kmlData->m_bookmarksData.size(), 2, ());
  TEST_EQUAL(kml::GetDefaultStr(kmlData->m_bookmarksData.front().m_name), "name 1", ());

  auto const fileName2 = bmManager.GetCategoryFileName(catId2);
  auto kmlData2 = LoadKmlFile(fileName2, GetActiveKmlFileType());
  TEST(kmlData2 != nullptr, ());
  TEST_EQUAL(kmlData2->m_bookmarksData.size(), 1, ());
  TEST_EQUAL(kml::GetDefaultStr(kmlData2->m_bookmarksData.front().m_name), "name 3", ());

  TEST(base::DeleteFileX(fileName), ());
  TEST(base::DeleteFileX(fileName2), ());
}

UNIT_CLASS_TEST(Runner, ExportAll)
{
  std::string const gpxFiles[] = {
    GetPlatform().TestsDataPathForFile("gpx_test_data/route.gpx"),
    GetPlatform().TestsDataPathForFile("gpx_test_data/points.gpx"),
    GetPlatform().TestsDataPathForFile("gpx_test_data/Üφήが1.gpx"),
    GetPlatform().TestsDataPathForFile("gpx_test_data/Üφήが2.gpx")};
  BookmarkManager bmManager(BM_CALLBACKS);
  bmManager.EnableTestMode(true);

  BookmarkManager::KMLDataCollection kmlDataCollection;
  for (auto const & file : gpxFiles)
    kmlDataCollection.emplace_back(file, LoadKmlFile(file, KmlFileType::Gpx));

  bmManager.CreateCategories(std::move(kmlDataCollection));
  TEST_EQUAL(bmManager.GetBmGroupsCount(), 4, ());

  auto categories = bmManager.GetUnsortedBmGroupsIdList();
  auto checker = [](BookmarkManager::SharingResult const & result)
  {
    auto kmz = result.m_sharingPath;
    ZipFileReader::FileList files;
    ZipFileReader::FilesList(kmz, files);
    TEST_EQUAL(files.size(), 5, ("5 files are expected in kmz"));
    auto index = "doc.kml";
    std::vector<std::string> expectedFiles = {"doc.kml", "files/new.kml", "files/Some random route.kml",
                                               "files/OrganicMaps_1.kml", "files/OrganicMaps_2.kml"};
    for (auto const & file : files)
      TEST(std::find(expectedFiles.begin(), expectedFiles.end(), file.first) != expectedFiles.end(), ());
    auto indexPath = base::JoinPath(GetPlatform().TmpDir(), index);
    ZipFileReader::UnzipFile(kmz, index, indexPath);
    std::string indexContent;
    FileReader(indexPath).ReadAsString(indexContent);
    std::string expectedIndexContent;
    FileReader(GetPlatform().TestsDataPathForFile("kml_test_data/kmz_index.kml"))
        .ReadAsString(expectedIndexContent);
    TEST_EQUAL(expectedIndexContent, indexContent, ("Index content doesnt match expected value"));
    auto tmpPath = base::JoinPath(GetPlatform().TmpDir(), "tmp.xml");
    for (auto const & file : files)
    {
      base::DeleteFileX(tmpPath);
      ZipFileReader::UnzipFile(kmz, file.first, tmpPath);
    }
    TEST(base::DeleteFileX(kmz), ());
    TEST(base::DeleteFileX(indexPath), ());
    TEST(base::DeleteFileX(tmpPath), ());
  };
  bmManager.PrepareFileForSharing(std::move(categories), checker);
}

UNIT_CLASS_TEST(Runner, ExportSingleUnicode)
{
  string file = GetPlatform().TestsDataPathForFile("gpx_test_data/Üφήが1.gpx");
  BookmarkManager bmManager(BM_CALLBACKS);
  bmManager.EnableTestMode(true);
  BookmarkManager::KMLDataCollection kmlDataCollection;
  kmlDataCollection.emplace_back(file, LoadKmlFile(file, KmlFileType::Gpx));
  bmManager.CreateCategories(std::move(kmlDataCollection));
  auto categories = bmManager.GetUnsortedBmGroupsIdList();
  auto checker = [](BookmarkManager::SharingResult const & result)
  {
    auto kmz = result.m_sharingPath;
    ZipFileReader::FileList files;
    ZipFileReader::FilesList(kmz, files);
    TEST_EQUAL(1, files.size(), ());
    TEST_EQUAL("OrganicMaps.kml", files.at(0).first, ());
    auto tmpPath = base::JoinPath(GetPlatform().TmpDir(), "tmp.xml");
    ZipFileReader::UnzipFile(kmz, files.at(0).first, tmpPath);
    TEST(base::DeleteFileX(kmz), ());
    TEST(base::DeleteFileX(tmpPath), ());
  };
  bmManager.PrepareFileForSharing(std::move(categories), checker);
}

UNIT_CLASS_TEST(Runner, Bookmarks_BrokenFile)
{
  string const fileName = GetPlatform().TestsDataPathForFile("broken_bookmarks.kmb.test");
  auto kmlData = LoadKmlFile(fileName, KmlFileType::Binary);
  TEST(kmlData == nullptr, ());
}
} // namespace bookmarks_test
