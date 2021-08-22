#include "testing/testing.hpp"

#include "storage/country_info_getter.hpp"
#include "storage/storage.hpp"
#include "storage/storage_defines.hpp"
#include "storage/storage_helpers.hpp"

#include "storage/storage_tests/fake_map_files_downloader.hpp"
#include "storage/storage_tests/helpers.hpp"
#include "storage/storage_tests/task_runner.hpp"
#include "storage/storage_tests/test_map_files_downloader.hpp"

#include "storage/storage_integration_tests/test_defines.hpp"

#if defined(OMIM_OS_DESKTOP)
#include "generator/generator_tests_support/test_mwm_builder.hpp"
#endif  // defined(OMIM_OS_DESKTOP)

#include "indexer/classificator_loader.hpp"
#include "indexer/indexer_tests/test_mwm_set.hpp"

#include "platform/country_file.hpp"
#include "platform/downloader_defines.hpp"
#include "platform/local_country_file.hpp"
#include "platform/local_country_file_utils.hpp"
#include "platform/mwm_version.hpp"
#include "platform/platform.hpp"
#include "platform/platform_tests_support/scoped_dir.hpp"
#include "platform/platform_tests_support/scoped_file.hpp"
#include "platform/platform_tests_support/writable_dir_changer.hpp"

#include "geometry/mercator.hpp"

#include "coding/file_writer.hpp"
#include "coding/internal/file_data.hpp"

#include "base/assert.hpp"
#include "base/file_name_utils.hpp"
#include "base/scope_guard.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <condition_variable>
#include <exception>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <vector>

#include "defines.hpp"

using namespace platform::tests_support;
using namespace platform;
using namespace std::placeholders;
using namespace std;

namespace storage
{
string const kMapTestDir = "map-tests";

namespace
{
using LocalFilePtr = shared_ptr<LocalCountryFile>;

class DummyDownloadingPolicy : public DownloadingPolicy
{
public:
  bool IsDownloadingAllowed() override { return false; }
};

class SometimesFailingDownloadingPolicy : public DownloadingPolicy
{
public:
  explicit SometimesFailingDownloadingPolicy(vector<uint64_t> const & failedRequests)
    : m_failedRequests(failedRequests)
  {
    sort(m_failedRequests.begin(), m_failedRequests.end());
  }

  bool IsDownloadingAllowed() override
  {
    auto const allowed =
        !binary_search(m_failedRequests.begin(), m_failedRequests.end(), m_request);
    ++m_request;
    return allowed;
  }

private:
  vector<uint64_t> m_failedRequests;
  uint64_t m_request = 0;
};

string const kCountriesTxt = string(R"({
           "id": "Countries",
           "v": )" + strings::to_string(version::FOR_TESTING_MWM1) + R"(,
           "g": [
               {
                "id": "Abkhazia",
                "s": 4689718,
                "old": [
                 "Georgia"
                ],
                "affiliations":
                [
                 "Georgia", "Russia", "Europe"
                ]
               },
               {
               "id": "OutdatedCountry1",
               "s": 50,
               "old": [
                       "NormalCountry"
                       ],
               "affiliations":
               [
                "TestFiles", "Miracle", "Ganimed"
                ]
               },
               {
               "id": "OutdatedCountry2",
               "s": 1000,
               "old": [
                       "NormalCountry"
                       ],
               "affiliations":
               [
                "TestFiles", "Miracle", "Ganimed"
                ]
               },
               {
                "id": "Algeria",
                "g": [
                 {
                  "id": "Algeria_Central",
                  "s": 24177144,
                  "old": [
                   "Algeria"
                  ]
                 },
                 {
                  "id": "Algeria_Coast",
                  "s": 66701534,
                  "old": [
                   "Algeria"
                  ]
                 }
                ]
               },
               {
                "id": "South Korea_South",
                "s": 48394664,
                "old": [
                 "South Korea"
                ]
               },

               {
                "id": "Country1",
                "g": [
                 {
                  "id": "Disputable Territory",
                  "s": 1234,
                  "old": [
                   "Country1"
                  ],
                  "affiliations":
                  [
                   "Stepchild Land1", "Stepchild Land2"
                  ]
                 },
                 {
                  "id": "Indisputable Territory Of Country1",
                  "s": 1111,
                  "old": [
                   "Country1"
                  ],
                  "affiliations":
                  [
                   "Child Land1"
                  ]
                 }
                ],
                "affiliations":
                [
                 "Parent Land1"
                ]
               },
               {
                "id": "Country2",
                "g": [
                 {
                  "id": "Indisputable Territory Of Country2",
                  "s": 2222,
                  "old": [
                   "Country2"
                  ]
                 },
                 {
                  "id": "Disputable Territory",
                  "s": 1234,
                  "old": [
                   "Country2"
                  ],
                  "affiliations":
                  [
                   "Stepchild Land1", "Stepchild Land2"
                  ]
                 }
                ],
                "affiliations":
                [
                 "Parent Land2"
                ]
               }
            ]})");

bool ParentOf(Storage const & storage, string const & parent, string const & country)
{
  Country const c = storage.CountryByCountryId(country);
  return c.GetParent() == parent;
}

// This class checks steps Storage::DownloadMap() performs to download a map.
class CountryDownloaderChecker
{
public:
  CountryDownloaderChecker(Storage & storage, CountryId const & countryId, MapFileType type,
                           vector<Status> const & transitionList)
    : m_storage(storage)
    , m_countryId(countryId)
    , m_countryFile(storage.GetCountryFile(m_countryId))
    , m_type(type)
    , m_bytesDownloaded(0)
    , m_totalBytesToDownload(0)
    , m_slot(0)
    , m_currStatus(0)
    , m_transitionList(transitionList)
  {
    m_slot = m_storage.Subscribe(
        bind(&CountryDownloaderChecker::OnCountryStatusChanged, this, _1),
        bind(&CountryDownloaderChecker::OnCountryDownloadingProgress, this, _1, _2));
    TEST(storage.IsLeaf(countryId), (m_countryFile));
    TEST(!m_transitionList.empty(), (m_countryFile));
  }

  virtual ~CountryDownloaderChecker()
  {
    TEST_EQUAL(m_currStatus + 1, m_transitionList.size(), (m_countryFile));
    m_storage.Unsubscribe(m_slot);
  }

  void StartDownload()
  {
    TEST_EQUAL(0, m_currStatus, (m_countryFile));
    TEST_LESS(m_currStatus, m_transitionList.size(), (m_countryFile));
    TEST_EQUAL(m_transitionList[m_currStatus], m_storage.CountryStatusEx(m_countryId), (m_countryFile));
    m_storage.DownloadCountry(m_countryId, m_type);
  }

protected:
  virtual void OnCountryStatusChanged(CountryId const & countryId)
  {
    if (countryId != m_countryId)
      return;

    Status const nexStatus = m_storage.CountryStatusEx(m_countryId);
    LOG(LINFO, (m_countryFile, "status transition: from", m_transitionList[m_currStatus], "to",
                nexStatus));
    TEST_LESS(m_currStatus + 1, m_transitionList.size(), (m_countryFile));
    TEST_EQUAL(nexStatus, m_transitionList[m_currStatus + 1], (m_countryFile));
    ++m_currStatus;
    if (m_transitionList[m_currStatus] == Status::Downloading ||
        m_transitionList[m_currStatus] == Status::InQueue)
    {
      LocalAndRemoteSize localAndRemoteSize = m_storage.CountrySizeInBytes(m_countryId);
      m_totalBytesToDownload = localAndRemoteSize.second;
    }
  }

  virtual void OnCountryDownloadingProgress(CountryId const & countryId,
                                            downloader::Progress const & progress)
  {
    if (countryId != m_countryId)
      return;

    LOG(LINFO, (m_countryFile, "downloading progress:", progress));

    TEST_GREATER(progress.m_bytesDownloaded, static_cast<
                 decltype(progress.m_bytesDownloaded)>(m_bytesDownloaded), (m_countryFile));
    m_bytesDownloaded = progress.m_bytesDownloaded;
    TEST_LESS_OR_EQUAL(m_bytesDownloaded, m_totalBytesToDownload, (m_countryFile));

    LocalAndRemoteSize localAndRemoteSize = m_storage.CountrySizeInBytes(m_countryId);
    TEST_EQUAL(static_cast<decltype(localAndRemoteSize.second)>(m_totalBytesToDownload),
               localAndRemoteSize.second, (m_countryFile));
  }

  Storage & m_storage;
  CountryId const m_countryId;
  CountryFile const m_countryFile;
  MapFileType const m_type;
  int64_t m_bytesDownloaded;
  int64_t m_totalBytesToDownload;
  int m_slot;

  size_t m_currStatus;
  vector<Status> m_transitionList;
};

class CancelDownloadingWhenAlmostDoneChecker : public CountryDownloaderChecker
{
public:
  CancelDownloadingWhenAlmostDoneChecker(Storage & storage, CountryId const & countryId,
                                         TaskRunner & runner)
    : CountryDownloaderChecker(
          storage, countryId, MapFileType::Map,
          vector<Status>{Status::NotDownloaded, Status::InQueue, Status::Downloading, Status::NotDownloaded})
    , m_runner(runner)
  {
  }

protected:
  // CountryDownloaderChecker overrides:
  void OnCountryDownloadingProgress(CountryId const & countryId,
                                    downloader::Progress const & progress) override
  {
    CountryDownloaderChecker::OnCountryDownloadingProgress(countryId, progress);

    // Cancel downloading when almost done.
    if (progress.m_bytesDownloaded + 2 * FakeMapFilesDownloader::kBlockSize >= progress.m_bytesTotal)
    {
      m_runner.PostTask([&]() { m_storage.CancelDownloadNode(m_countryId); });
    }
  }

  TaskRunner & m_runner;
};

// Checks following state transitions:
// NotDownloaded -> Downloading -> OnDisk.
unique_ptr<CountryDownloaderChecker> AbsentCountryDownloaderChecker(Storage & storage,
                                                                    CountryId const & countryId,
                                                                    MapFileType type)
{
  return make_unique<CountryDownloaderChecker>(
      storage, countryId, type,
      vector<Status>{Status::NotDownloaded, Status::InQueue, Status::Downloading, Status::OnDisk});
}

// Checks following state transitions:
// NotDownloaded -> Downloading -> NotDownloaded.
unique_ptr<CountryDownloaderChecker> CancelledCountryDownloaderChecker(Storage & storage,
                                                                       CountryId const & countryId,
                                                                       MapFileType type)
{
  return make_unique<CountryDownloaderChecker>(
      storage, countryId, type,
      vector<Status>{Status::NotDownloaded, Status::InQueue, Status::Downloading, Status::NotDownloaded});
}

class CountryStatusChecker
{
public:
  CountryStatusChecker(Storage & storage, CountryId const & countryId, Status status)
    : m_storage(storage), m_countryId(countryId), m_status(status), m_triggered(false)
  {
    m_slot = m_storage.Subscribe(
        bind(&CountryStatusChecker::OnCountryStatusChanged, this, _1),
        bind(&CountryStatusChecker::OnCountryDownloadingProgress, this, _1, _2));
  }

  ~CountryStatusChecker()
  {
    TEST(m_triggered, ("Status checker wasn't triggered."));
    m_storage.Unsubscribe(m_slot);
  }

private:
  void OnCountryStatusChanged(CountryId const & countryId)
  {
    if (countryId != m_countryId)
      return;
    TEST(!m_triggered, ("Status checker can be triggered only once."));
    Status status = m_storage.CountryStatusEx(m_countryId);
    TEST_EQUAL(m_status, status, ());
    m_triggered = true;
  }

  void OnCountryDownloadingProgress(CountryId const & /* countryId */,
                                    downloader::Progress const & /* progress */)
  {
    TEST(false, ("Unexpected country downloading progress."));
  }

  Storage & m_storage;
  CountryId const & m_countryId;
  Status m_status;
  bool m_triggered;
  int m_slot;
};

class FailedDownloadingWaiter
{
public:
  FailedDownloadingWaiter(Storage & storage, CountryId const & countryId)
    : m_storage(storage), m_countryId(countryId), m_finished(false)
  {
    m_slot = m_storage.Subscribe(bind(&FailedDownloadingWaiter::OnStatusChanged, this, _1),
                                 bind(&FailedDownloadingWaiter::OnProgress, this, _1, _2));
  }

  ~FailedDownloadingWaiter()
  {
    Wait();
    m_storage.Unsubscribe(m_slot);
  }

  void Wait()
  {
    unique_lock<mutex> lock(m_mu);
    m_cv.wait(lock, [this]()
    {
      return m_finished;
    });
  }

  void OnStatusChanged(CountryId const & countryId)
  {
    if (countryId != m_countryId)
      return;
    Status const status = m_storage.CountryStatusEx(countryId);
    if (status != Status::DownloadFailed)
      return;
    lock_guard<mutex> lock(m_mu);
    m_finished = true;
    m_cv.notify_one();

    testing::StopEventLoop();
  }

  void OnProgress(CountryId const & /* countryId */, downloader::Progress const & /* progress */) {}

private:
  Storage & m_storage;
  CountryId const m_countryId;
  int m_slot;

  mutex m_mu;
  condition_variable m_cv;
  bool m_finished;
};

void OnCountryDownloaded(CountryId const & countryId, LocalFilePtr const localFile)
{
  LOG(LINFO, ("OnCountryDownloaded:", *localFile));
}

LocalFilePtr CreateDummyMapFile(CountryFile const & countryFile, int64_t version, uint64_t size)
{
  LocalFilePtr localFile =
      PreparePlaceForCountryFiles(version, string() /* dataDir */, countryFile);
  TEST(localFile.get(), ("Can't prepare place for", countryFile, "(version", version, ")"));
  {
    string const zeroes(size, '\0');
    FileWriter writer(localFile->GetPath(MapFileType::Map));
    writer.Write(zeroes.data(), zeroes.size());
  }
  localFile->SyncWithDisk();
  TEST(localFile->OnDisk(MapFileType::Map), ());
  TEST_EQUAL(size, localFile->GetSize(MapFileType::Map), ());
  return localFile;
}

void InitStorage(Storage & storage, TaskRunner & runner,
                 Storage::UpdateCallback const & update = &OnCountryDownloaded)
{
  storage.Clear();
  storage.Init(update, [](CountryId const &, LocalFilePtr const) { return false; });
  storage.RegisterAllLocalMaps(false /* enableDiffs */);
  storage.SetDownloaderForTesting(make_unique<FakeMapFilesDownloader>(runner));
  // Disable because of FakeMapFilesDownloader.
  storage.SetEnabledIntegrityValidationForTesting(false);
}

class StorageTest
{
public:
  StorageTest() : writableDirChanger(kMapTestDir) { InitStorage(storage, runner); }

protected:
  Storage storage;
  TaskRunner runner;
  WritableDirChanger writableDirChanger;
};
}  // namespace

UNIT_TEST(StorageTest_ParseIndexFile)
{
  Storage storage;

  TEST_EQUAL(storage.ParseIndexAndGetDataVersion(R"([ { "21010101" : 210317 } ])"), 210317, ());
  TEST_EQUAL(storage.ParseIndexAndGetDataVersion(R"([ { "21010101" : 210317 }, { "21041001" : 210409 } ])"), 210409, ());

  // Invalid jsons
  TEST_EQUAL(storage.ParseIndexAndGetDataVersion(R"([ { "21010101" : 210317 })"), 0, ());
  TEST_EQUAL(storage.ParseIndexAndGetDataVersion(R"([ { "xxx" : 210317 } ])"), 0, ());
  TEST_EQUAL(storage.ParseIndexAndGetDataVersion(R"([ { "21010101" : xxx } ])"), 0, ());
}

UNIT_TEST(StorageTest_Smoke)
{
  Storage storage;

  CountryId const georgiaCountryId = storage.FindCountryIdByFile("Georgia");
  TEST(IsCountryIdValid(georgiaCountryId), ());
  CountryFile usaGeorgiaFile = storage.GetCountryFile(georgiaCountryId);
  TEST_EQUAL(platform::GetFileName(usaGeorgiaFile.GetName(), MapFileType::Map),
             "Georgia" DATA_FILE_EXTENSION, ());
}

UNIT_CLASS_TEST(StorageTest, CountryDownloading)
{
  Platform::ThreadRunner m_runner;
  CountryId const azerbaijanCountryId = storage.FindCountryIdByFile("Azerbaijan");
  TEST(IsCountryIdValid(azerbaijanCountryId), ());

  CountryFile azerbaijanFile = storage.GetCountryFile(azerbaijanCountryId);
  storage.DeleteCountry(azerbaijanCountryId, MapFileType::Map);

  {
    SCOPE_GUARD(cleanupCountryFiles,
                bind(&Storage::DeleteCountry, &storage, azerbaijanCountryId, MapFileType::Map));
    unique_ptr<CountryDownloaderChecker> checker =
        AbsentCountryDownloaderChecker(storage, azerbaijanCountryId, MapFileType::Map);
    checker->StartDownload();
    runner.Run();
  }

  {
    SCOPE_GUARD(cleanupCountryFiles,
                bind(&Storage::DeleteCountry, &storage, azerbaijanCountryId, MapFileType::Map));
    unique_ptr<CountryDownloaderChecker> checker =
        AbsentCountryDownloaderChecker(storage, azerbaijanCountryId, MapFileType::Map);
    checker->StartDownload();
    runner.Run();
  }
}

UNIT_TEST(StorageTest_DeleteTwoVersionsOfTheSameCountry)
{
  Storage storage;
  storage.SetCurrentDataVersionForTesting(version::FOR_TESTING_MWM_LATEST);
  int64_t const v1 = version::FOR_TESTING_MWM1;
  int64_t const v2 = version::FOR_TESTING_MWM2;

  storage.Init(&OnCountryDownloaded, [](CountryId const &, LocalFilePtr const) { return false; });
  storage.RegisterAllLocalMaps(false /* enableDiffs */);

  CountryId const countryId = storage.FindCountryIdByFile("Azerbaijan");
  TEST(IsCountryIdValid(countryId), ());
  CountryFile const countryFile = storage.GetCountryFile(countryId);

  storage.DeleteCountry(countryId, MapFileType::Map);
  LocalFilePtr latestLocalFile = storage.GetLatestLocalFile(countryId);
  TEST(!latestLocalFile.get(), ("Country wasn't deleted from disk."));
  TEST_EQUAL(Status::NotDownloaded, storage.CountryStatusEx(countryId), ());

  LocalFilePtr localFileV1 = CreateDummyMapFile(countryFile, v1, 1024 /* size */);
  storage.RegisterAllLocalMaps(false /* enableDiffs */);
  latestLocalFile = storage.GetLatestLocalFile(countryId);
  TEST(latestLocalFile.get(), ("Created map file wasn't found by storage."));
  TEST_EQUAL(latestLocalFile->GetVersion(), localFileV1->GetVersion(), ());
  TEST_EQUAL(Status::OnDiskOutOfDate, storage.CountryStatusEx(countryId), ());

  LocalFilePtr localFileV2 = CreateDummyMapFile(countryFile, v2, 2048 /* size */);
  storage.RegisterAllLocalMaps(false /* enableDiffs */);
  latestLocalFile = storage.GetLatestLocalFile(countryId);
  TEST(latestLocalFile.get(), ("Created map file wasn't found by storage."));
  TEST_EQUAL(latestLocalFile->GetVersion(), localFileV2->GetVersion(), ());
  TEST_EQUAL(Status::OnDiskOutOfDate, storage.CountryStatusEx(countryId), ());

  storage.DeleteCountry(countryId, MapFileType::Map);

  localFileV1->SyncWithDisk();
  TEST(!localFileV1->HasFiles(), ());

  localFileV2->SyncWithDisk();
  TEST(!localFileV2->HasFiles(), ());

  TEST_EQUAL(Status::NotDownloaded, storage.CountryStatusEx(countryId), ());
}

UNIT_CLASS_TEST(StorageTest, DeletePendingCountry)
{
  CountryId const countryId = storage.FindCountryIdByFile("Azerbaijan");
  TEST(IsCountryIdValid(countryId), ());
  storage.DeleteCountry(countryId, MapFileType::Map);

  {
    unique_ptr<CountryDownloaderChecker> checker =
        CancelledCountryDownloaderChecker(storage, countryId, MapFileType::Map);
    checker->StartDownload();
    storage.DeleteCountry(countryId, MapFileType::Map);
    runner.Run();
  }
}

UNIT_CLASS_TEST(StorageTest, CancelDownloadingWhenAlmostDone)
{
  CountryId const countryId = storage.FindCountryIdByFile("Uruguay");
  TEST(IsCountryIdValid(countryId), ());
  storage.DeleteCountry(countryId, MapFileType::Map);
  SCOPE_GUARD(cleanupFiles, bind(&Storage::DeleteCountry, &storage, countryId, MapFileType::Map));

  {
    CancelDownloadingWhenAlmostDoneChecker checker(storage, countryId, runner);
    checker.StartDownload();
    runner.Run();
  }
  LocalFilePtr file = storage.GetLatestLocalFile(countryId);
  TEST(!file, (*file));
}

UNIT_CLASS_TEST(StorageTest, DeleteCountry)
{
  tests_support::ScopedFile map("Wonderland.mwm", ScopedFile::Mode::Create);
  LocalCountryFile file = LocalCountryFile::MakeForTesting("Wonderland",
                                                           version::FOR_TESTING_MWM1);
  TEST(file.OnDisk(MapFileType::Map), ());

  CountryIndexes::PreparePlaceOnDisk(file);
  string const bitsPath = CountryIndexes::GetPath(file, CountryIndexes::Index::Bits);
  {
    FileWriter writer(bitsPath);
    string const data = "bits";
    writer.Write(data.data(), data.size());
  }

  storage.RegisterFakeCountryFiles(file);
  TEST(map.Exists(), ());
  TEST(Platform::IsFileExistsByFullPath(bitsPath), (bitsPath));

  storage.DeleteCustomCountryVersion(file);
  TEST(!map.Exists(), ());
  TEST(!Platform::IsFileExistsByFullPath(bitsPath), (bitsPath));

  map.Reset();
}

UNIT_TEST(StorageTest_FailedDownloading)
{
  Platform::ThreadRunner m_runner;
  Storage storage;
  storage.Init(&OnCountryDownloaded, [](CountryId const &, LocalFilePtr const) { return false; });
  storage.SetDownloaderForTesting(make_unique<TestMapFilesDownloader>());
  storage.SetCurrentDataVersionForTesting(1234);

  CountryId const countryId = storage.FindCountryIdByFile("Uruguay");
  CountryFile const countryFile = storage.GetCountryFile(countryId);

  // To prevent interference from other tests and on other tests it's
  // better to remove temprorary downloader files.
  DeleteDownloaderFilesForCountry(storage.GetCurrentDataVersion(), countryFile);
  SCOPE_GUARD(cleanup, [&]() {
    DeleteDownloaderFilesForCountry(storage.GetCurrentDataVersion(), countryFile);
    // Failed country is added into the "retry later" list, and fails next test.
    storage.Clear();
  });

  {
    FailedDownloadingWaiter waiter(storage, countryId);
    storage.DownloadCountry(countryId, MapFileType::Map);
    testing::RunEventLoop();
  }

  // File wasn't downloaded, but temprorary downloader files must exist.
  string const downloadPath =
      GetFileDownloadPath(storage.GetCurrentDataVersion(), countryFile, MapFileType::Map);
  TEST(!Platform::IsFileExistsByFullPath(downloadPath), ());
  TEST(Platform::IsFileExistsByFullPath(downloadPath + DOWNLOADING_FILE_EXTENSION), ());
  TEST(Platform::IsFileExistsByFullPath(downloadPath + RESUME_FILE_EXTENSION), ());
}

UNIT_TEST(StorageTest_ObsoleteMapsRemoval)
{
  Platform::ThreadRunner m_runner;
  Storage storage;
  CountryFile country("Azerbaijan");

  tests_support::ScopedDir dir1("1");
  tests_support::ScopedFile map1(dir1, country, MapFileType::Map);
  LocalCountryFile file1(dir1.GetFullPath(), country, 1 /* version */);

  tests_support::ScopedDir dir2("2");
  tests_support::ScopedFile map2(dir2, country, MapFileType::Map);
  LocalCountryFile file2(dir2.GetFullPath(), country, 2 /* version */);

  TEST(map1.Exists(), ());
  TEST(map2.Exists(), ());

  storage.RegisterAllLocalMaps(false /* enableDiffs */);

  TEST(!map1.Exists(), ());
  map1.Reset();

  TEST(map2.Exists(), ());
}

UNIT_TEST(StorageTest_GetRootId)
{
  Storage storage(string(R"({
                           "id": "Countries",
                           "v": )" + strings::to_string(version::FOR_TESTING_MWM1) + R"(,
                           "g": []
                         })"), make_unique<TestMapFilesDownloader>());

  // The name of the root is the same for courntries.txt version 1 and version 2.
  TEST_EQUAL(storage.GetRootId(), "Countries", ());
}

UNIT_TEST(StorageTest_GetChildren)
{
  Storage storage(kCountriesTxt, make_unique<TestMapFilesDownloader>());

  CountryId const world = storage.GetRootId();
  TEST_EQUAL(world, "Countries", ());

  CountriesVec countriesList;
  storage.GetChildren(world, countriesList);
  TEST_EQUAL(countriesList.size(), 7, ());
  TEST_EQUAL(countriesList.front(), "Abkhazia", ());
  TEST_EQUAL(countriesList.back(), "Country2", ());

  CountriesVec abkhaziaList;
  storage.GetChildren("Abkhazia", abkhaziaList);
  TEST(abkhaziaList.empty(), ());

  CountriesVec algeriaList;
  storage.GetChildren("Algeria", algeriaList);
  TEST_EQUAL(algeriaList.size(), 2, ());
  TEST_EQUAL(algeriaList.front(), "Algeria_Central", ());
}

UNIT_TEST(StorageTest_GetAffiliations)
{
  Storage storage(kCountriesTxt, make_unique<TestMapFilesDownloader>());

  vector<string> const abkhaziaId = {"Abkhazia"};
  for (auto const & s : {"Georgia", "Russia", "Europe"})
    TEST_EQUAL(storage.GetAffiliations()->at(s), abkhaziaId, ());

  // Affiliation inheritance.
  vector<string> const disputableId = {"Disputable Territory"};
  for (auto const & s : {"Stepchild Land1", "Stepchild Land2"})
    TEST_EQUAL(storage.GetAffiliations()->at(s), disputableId, ());

  vector<string> const indisputableId = {"Indisputable Territory Of Country1"};
  for (auto const & s : {"Child Land1"})
    TEST_EQUAL(storage.GetAffiliations()->at(s), indisputableId, ());
}

UNIT_TEST(StorageTest_HasCountryId)
{
  CountriesVec middleEarthCountryIdVec = {"Arnor",  "Mordor",  "Rhovanion", "Rhun",
                                          "Gondor", "Eriador", "Rohan"};
  sort(middleEarthCountryIdVec.begin(), middleEarthCountryIdVec.end());
}

UNIT_CLASS_TEST(StorageTest, DownloadedMap)
{
  CountryId const algeriaCentralCountryId = storage.FindCountryIdByFile("Algeria_Central");
  CountryId const algeriaCoastCountryId = storage.FindCountryIdByFile("Algeria_Coast");
  TEST(IsCountryIdValid(algeriaCentralCountryId), ());
  TEST(IsCountryIdValid(algeriaCoastCountryId), ());

  storage.DeleteCountry(algeriaCentralCountryId, MapFileType::Map);
  storage.DeleteCountry(algeriaCoastCountryId, MapFileType::Map);

  SCOPE_GUARD(cleanupAlgeriaCentral,
              bind(&Storage::DeleteCountry, &storage, algeriaCentralCountryId, MapFileType::Map));
  SCOPE_GUARD(cleanupAlgeriaCoast,
              bind(&Storage::DeleteCountry, &storage, algeriaCoastCountryId, MapFileType::Map));

  {
    auto algeriaCentralChecker = make_unique<CountryDownloaderChecker>(
        storage, algeriaCentralCountryId, MapFileType::Map,
        vector<Status>{Status::NotDownloaded, Status::InQueue, Status::Downloading,
                       Status::OnDisk});

    auto algeriaCoastChecker = make_unique<CountryDownloaderChecker>(
        storage, algeriaCoastCountryId, MapFileType::Map,
        vector<Status>{Status::NotDownloaded, Status::InQueue, Status::Downloading,
                       Status::OnDisk});

    algeriaCentralChecker->StartDownload();
    algeriaCoastChecker->StartDownload();
    runner.Run();
  }

  // Storage::GetLocalRealMaps() test.
  CountriesVec localRealMaps;
  storage.GetLocalRealMaps(localRealMaps);
  sort(localRealMaps.begin(), localRealMaps.end());

  TEST(storage.IsNodeDownloaded("Algeria_Central"), ());
  TEST(storage.IsNodeDownloaded("Algeria_Coast"), ());
  TEST(!storage.IsNodeDownloaded("Algeria_Coast.mwm"), ());
  TEST(!storage.IsNodeDownloaded("World"), ());
  TEST(!storage.IsNodeDownloaded("World"), ());

  // Storage::GetChildrenInGroups test when at least Algeria_Central and Algeria_Coast have been downloaded.
  CountryId const rootCountryId = storage.GetRootId();
  TEST_EQUAL(rootCountryId, "Countries", ());

  CountriesVec downloaded, available;
  CountriesVec downloadedWithKeep, availableWithKeep;

  storage.GetChildrenInGroups(rootCountryId, downloaded, available);
  TEST_EQUAL(downloaded.size(), 1, (downloaded));
  TEST_EQUAL(available.size(), 223, ());

  storage.GetChildrenInGroups(rootCountryId, downloadedWithKeep,
                              availableWithKeep, true /* keepAvailableChildren*/);
  TEST_EQUAL(downloadedWithKeep.size(), 1, (downloadedWithKeep));
  TEST_EQUAL(availableWithKeep.size(), 224, ());

  storage.GetChildrenInGroups("Algeria", downloaded, available);
  TEST_EQUAL(downloaded.size(), 2, (downloaded));

  storage.GetChildrenInGroups("Algeria", downloadedWithKeep,
                              availableWithKeep, true /* keepAvailableChildren*/);
  TEST_EQUAL(downloadedWithKeep.size(), 2, (downloadedWithKeep));
  TEST_EQUAL(availableWithKeep.size(), 2, (availableWithKeep));

  storage.GetChildrenInGroups("Algeria_Central", downloaded, available);
  TEST(downloaded.empty(), ());

  storage.GetChildrenInGroups("Algeria_Central", downloadedWithKeep,
                              availableWithKeep, true /* keepAvailableChildren*/);
  TEST_EQUAL(downloadedWithKeep.size(), 0, (downloadedWithKeep));
  TEST_EQUAL(availableWithKeep.size(), 0, (availableWithKeep));

  storage.DeleteCountry(algeriaCentralCountryId, MapFileType::Map);
  // Storage::GetChildrenInGroups test when Algeria_Coast has been downloaded and
  // Algeria_Central has been deleted.
  storage.GetChildrenInGroups(rootCountryId, downloaded, available);
  TEST_EQUAL(downloaded.size(), 1, (downloaded));

  storage.GetChildrenInGroups("Algeria", downloadedWithKeep,
                              availableWithKeep, true /* keepAvailableChildren*/);
  TEST_EQUAL(downloadedWithKeep.size(), 1, (downloadedWithKeep));
  TEST_EQUAL(availableWithKeep.size(), 2, (availableWithKeep));

  storage.GetChildrenInGroups("Algeria_Central", downloaded, available);
  TEST(downloaded.empty(), ());

  storage.GetChildrenInGroups("Algeria_Coast", downloaded, available);
  TEST(downloaded.empty(), ());

  TEST(!storage.IsNodeDownloaded("Algeria_Central"), ());
  TEST(storage.IsNodeDownloaded("Algeria_Coast"), ());

  storage.DeleteCountry(algeriaCoastCountryId, MapFileType::Map);
  // Storage::GetChildrenInGroups test when Algeria_Coast and Algeria_Central have been deleted.
  storage.GetChildrenInGroups(rootCountryId, downloaded, available);
  sort(downloaded.begin(), downloaded.end());

  TEST(!storage.IsNodeDownloaded("Algeria_Central"), ());
  TEST(!storage.IsNodeDownloaded("Algeria_Coast"), ());
}

UNIT_CLASS_TEST(StorageTest, IsPointCoveredByDownloadedMaps)
{
  auto const countryInfoGetter = CreateCountryInfoGetter();
  ASSERT(countryInfoGetter, ());
  string const uruguayId = string("Uruguay");
  m2::PointD const montevideoUruguay = mercator::FromLatLon(-34.8094, -56.1558);

  storage.DeleteCountry(uruguayId, MapFileType::Map);
  TEST(!storage::IsPointCoveredByDownloadedMaps(montevideoUruguay, storage, *countryInfoGetter), ());

  {
    SCOPE_GUARD(cleanupCountryFiles,
                bind(&Storage::DeleteCountry, &storage, uruguayId, MapFileType::Map));
    auto const checker = AbsentCountryDownloaderChecker(storage, uruguayId, MapFileType::Map);
    checker->StartDownload();
    runner.Run();
    TEST(storage::IsPointCoveredByDownloadedMaps(montevideoUruguay, storage, *countryInfoGetter), ());
  }
}

UNIT_TEST(StorageTest_ChildrenSizeSingleMwm)
{
  Storage storage(kCountriesTxt, make_unique<TestMapFilesDownloader>());

  Country const abkhaziaCountry = storage.CountryLeafByCountryId("Abkhazia");
  TEST_EQUAL(abkhaziaCountry.GetSubtreeMwmCounter(), 1, ());
  TEST_EQUAL(abkhaziaCountry.GetSubtreeMwmSizeBytes(), 4689718, ());

  Country const algeriaCountry = storage.CountryByCountryId("Algeria");
  TEST_EQUAL(algeriaCountry.GetSubtreeMwmCounter(), 2, ());
  TEST_EQUAL(algeriaCountry.GetSubtreeMwmSizeBytes(), 90878678, ());

  Country const southKoreaCountry = storage.CountryLeafByCountryId("South Korea_South");
  TEST_EQUAL(southKoreaCountry.GetSubtreeMwmCounter(), 1, ());
  TEST_EQUAL(southKoreaCountry.GetSubtreeMwmSizeBytes(), 48394664, ());
}

UNIT_TEST(StorageTest_ParentSingleMwm)
{
  Storage storage(kCountriesTxt, make_unique<TestMapFilesDownloader>());

  TEST(ParentOf(storage, "Countries", "Abkhazia"), ());
  TEST(ParentOf(storage, "Algeria", "Algeria_Central"), ());
  TEST(ParentOf(storage, "Countries", "South Korea_South"), ());
  TEST(ParentOf(storage, kInvalidCountryId, "Countries"), ());
}

UNIT_TEST(StorageTest_GetNodeStatusesSingleMwm)
{
  Storage storage(kCountriesTxt, make_unique<TestMapFilesDownloader>());

  NodeStatuses nodeStatuses;
  storage.GetNodeStatuses("Abkhazia", nodeStatuses);
  TEST_EQUAL(nodeStatuses.m_status, NodeStatus::NotDownloaded, ());
  TEST_EQUAL(nodeStatuses.m_error, NodeErrorCode::NoError, ());
  TEST(!nodeStatuses.m_groupNode, ());

  storage.GetNodeStatuses("Algeria", nodeStatuses);
  TEST_EQUAL(nodeStatuses.m_status, NodeStatus::NotDownloaded, ());
  TEST_EQUAL(nodeStatuses.m_error, NodeErrorCode::NoError, ());
  TEST(nodeStatuses.m_groupNode, ());
}

UNIT_TEST(StorageTest_GetNodeAttrsSingleMwm)
{
  Storage storage(kCountriesTxt, make_unique<TestMapFilesDownloader>());

  NodeAttrs nodeAttrs;
  storage.GetNodeAttrs("Abkhazia", nodeAttrs);
  TEST_EQUAL(nodeAttrs.m_mwmCounter, 1, ());
  TEST_EQUAL(nodeAttrs.m_mwmSize, 4689718, ());
  TEST_EQUAL(nodeAttrs.m_status, NodeStatus::NotDownloaded, ());
  TEST_EQUAL(nodeAttrs.m_error, NodeErrorCode::NoError, ());
  TEST_EQUAL(nodeAttrs.m_parentInfo.size(), 1, ());
  TEST_EQUAL(nodeAttrs.m_parentInfo[0].m_id, "Countries", ());
  TEST_EQUAL(nodeAttrs.m_downloadingProgress.m_bytesDownloaded, 0, ());
  TEST_EQUAL(nodeAttrs.m_downloadingProgress.m_bytesTotal, 0, ());
  TEST_EQUAL(nodeAttrs.m_localMwmCounter, 0, ());
  TEST_EQUAL(nodeAttrs.m_localMwmSize, 0, ());
  TEST_EQUAL(nodeAttrs.m_downloadingMwmCounter, 0, ());
  TEST_EQUAL(nodeAttrs.m_localMwmSize, 0, ());
  TEST(!nodeAttrs.m_present, ());

  storage.GetNodeAttrs("Algeria", nodeAttrs);
  TEST_EQUAL(nodeAttrs.m_mwmCounter, 2, ());
  TEST_EQUAL(nodeAttrs.m_mwmSize, 90878678, ());
  TEST_EQUAL(nodeAttrs.m_status, NodeStatus::NotDownloaded, ()); // It's a status of expandable node.
  TEST_EQUAL(nodeAttrs.m_error, NodeErrorCode::NoError, ());
  TEST_EQUAL(nodeAttrs.m_parentInfo.size(), 1, ());
  TEST_EQUAL(nodeAttrs.m_parentInfo[0].m_id, "Countries", ());
  TEST_EQUAL(nodeAttrs.m_downloadingProgress.m_bytesDownloaded, 0, ());
  TEST_EQUAL(nodeAttrs.m_downloadingProgress.m_bytesTotal, 0, ());
  TEST_EQUAL(nodeAttrs.m_localMwmCounter, 0, ());
  TEST_EQUAL(nodeAttrs.m_localMwmSize, 0, ());
  TEST_EQUAL(nodeAttrs.m_downloadingMwmCounter, 0, ());
  TEST_EQUAL(nodeAttrs.m_localMwmSize, 0, ());
  TEST(!nodeAttrs.m_present, ());

  storage.GetNodeAttrs("Algeria_Coast", nodeAttrs);
  TEST_EQUAL(nodeAttrs.m_mwmCounter, 1, ());
  TEST_EQUAL(nodeAttrs.m_mwmSize, 66701534, ());
  TEST_EQUAL(nodeAttrs.m_status, NodeStatus::NotDownloaded, ());
  TEST_EQUAL(nodeAttrs.m_error, NodeErrorCode::NoError, ());
  TEST_EQUAL(nodeAttrs.m_parentInfo.size(), 1, ());
  TEST_EQUAL(nodeAttrs.m_parentInfo[0].m_id, "Algeria", ());
  TEST_EQUAL(nodeAttrs.m_downloadingProgress.m_bytesDownloaded, 0, ());
  TEST_EQUAL(nodeAttrs.m_downloadingProgress.m_bytesTotal, 0, ());
  TEST_EQUAL(nodeAttrs.m_localMwmCounter, 0, ());
  TEST_EQUAL(nodeAttrs.m_localMwmSize, 0, ());
  TEST_EQUAL(nodeAttrs.m_downloadingMwmCounter, 0, ());
  TEST_EQUAL(nodeAttrs.m_localMwmSize, 0, ());
  TEST(!nodeAttrs.m_present, ());

  storage.GetNodeAttrs("South Korea_South", nodeAttrs);
  TEST_EQUAL(nodeAttrs.m_mwmCounter, 1, ());
  TEST_EQUAL(nodeAttrs.m_mwmSize, 48394664, ());
  TEST_EQUAL(nodeAttrs.m_status, NodeStatus::NotDownloaded, ());
  TEST_EQUAL(nodeAttrs.m_error, NodeErrorCode::NoError, ());
  TEST_EQUAL(nodeAttrs.m_parentInfo.size(), 1, ());
  TEST_EQUAL(nodeAttrs.m_parentInfo[0].m_id, "Countries", ());
  TEST_EQUAL(nodeAttrs.m_downloadingProgress.m_bytesDownloaded, 0, ());
  TEST_EQUAL(nodeAttrs.m_downloadingProgress.m_bytesTotal, 0, ());
  TEST_EQUAL(nodeAttrs.m_localMwmCounter, 0, ());
  TEST_EQUAL(nodeAttrs.m_localMwmSize, 0, ());
  TEST_EQUAL(nodeAttrs.m_downloadingMwmCounter, 0, ());
  TEST_EQUAL(nodeAttrs.m_localMwmSize, 0, ());
  TEST(!nodeAttrs.m_present, ());

  storage.GetNodeAttrs("Disputable Territory", nodeAttrs);
  TEST_EQUAL(nodeAttrs.m_mwmCounter, 1, ());
  TEST_EQUAL(nodeAttrs.m_mwmSize, 1234, ());
  TEST_EQUAL(nodeAttrs.m_status, NodeStatus::NotDownloaded, ());
  TEST_EQUAL(nodeAttrs.m_error, NodeErrorCode::NoError, ());
  vector<CountryId> const expectedParents = {"Country1", "Country2"};
  TEST_EQUAL(nodeAttrs.m_parentInfo.size(), 2, ());
  TEST_EQUAL(nodeAttrs.m_parentInfo[0].m_id, "Country1", ());
  TEST_EQUAL(nodeAttrs.m_parentInfo[1].m_id, "Country2", ());
  TEST_EQUAL(nodeAttrs.m_downloadingProgress.m_bytesDownloaded, 0, ());
  TEST_EQUAL(nodeAttrs.m_downloadingProgress.m_bytesTotal, 0, ());
  TEST_EQUAL(nodeAttrs.m_localMwmCounter, 0, ());
  TEST_EQUAL(nodeAttrs.m_localMwmSize, 0, ());
  TEST_EQUAL(nodeAttrs.m_downloadingMwmCounter, 0, ());
  TEST_EQUAL(nodeAttrs.m_localMwmSize, 0, ());
  TEST(!nodeAttrs.m_present, ());
}

#if defined(OMIM_OS_DESKTOP)
UNIT_TEST(StorageTest_GetUpdateInfoSingleMwm)
{
  classificator::Load();
  WritableDirChanger writableDirChanger(kMapTestDir);

  string const kVersion1Dir = base::JoinPath(GetPlatform().WritableDir(), "1");
  CHECK_EQUAL(Platform::MkDir(kVersion1Dir), Platform::ERR_OK, ());

  LocalCountryFile country1(kVersion1Dir, CountryFile("OutdatedCountry1"), 1);
  LocalCountryFile country2(kVersion1Dir, CountryFile("OutdatedCountry2"), 1);

  using namespace generator::tests_support;
  {
    TestMwmBuilder builder(country1, feature::DataHeader::MapType::Country);
  }
  {
    TestMwmBuilder builder(country2, feature::DataHeader::MapType::Country);
  }

  Storage storage(kCountriesTxt, make_unique<TestMapFilesDownloader>());
  storage.RegisterAllLocalMaps(false /* enableDiffs */);

  country1.SyncWithDisk();
  country2.SyncWithDisk();
  auto const country1Size = country1.GetSize(MapFileType::Map);
  auto const country2Size = country2.GetSize(MapFileType::Map);

  Storage::UpdateInfo updateInfo;

  storage.GetUpdateInfo("OutdatedCountry1", updateInfo);
  TEST_EQUAL(updateInfo.m_numberOfMwmFilesToUpdate, 1, ());
  TEST_EQUAL(updateInfo.m_totalUpdateSizeInBytes, 50, ());
  TEST_EQUAL(updateInfo.m_sizeDifference, 50 - static_cast<int64_t>(country1Size), ());

  storage.GetUpdateInfo("OutdatedCountry2", updateInfo);
  TEST_EQUAL(updateInfo.m_numberOfMwmFilesToUpdate, 1, ());
  TEST_EQUAL(updateInfo.m_totalUpdateSizeInBytes, 1000, ());
  TEST_EQUAL(updateInfo.m_sizeDifference, 1000 - static_cast<int64_t>(country2Size), ());

  storage.GetUpdateInfo("Abkhazia", updateInfo);
  TEST_EQUAL(updateInfo.m_numberOfMwmFilesToUpdate, 0, ());
  TEST_EQUAL(updateInfo.m_totalUpdateSizeInBytes, 0, ());

  storage.GetUpdateInfo("Country1", updateInfo);
  TEST_EQUAL(updateInfo.m_numberOfMwmFilesToUpdate, 0, ());
  TEST_EQUAL(updateInfo.m_totalUpdateSizeInBytes, 0, ());

  storage.GetUpdateInfo("Disputable Territory", updateInfo);
  TEST_EQUAL(updateInfo.m_numberOfMwmFilesToUpdate, 0, ());
  TEST_EQUAL(updateInfo.m_totalUpdateSizeInBytes, 0, ());

  storage.GetUpdateInfo(storage.GetRootId(), updateInfo);
  TEST_EQUAL(updateInfo.m_numberOfMwmFilesToUpdate, 2, ());
  TEST_EQUAL(updateInfo.m_totalUpdateSizeInBytes, 1050, ());
  TEST_EQUAL(updateInfo.m_sizeDifference,
             (1000 + 50) - static_cast<int64_t>((country1Size + country2Size)), ());
}
#endif  // defined(OMIM_OS_DESKTOP)

UNIT_TEST(StorageTest_ParseStatus)
{
  TEST_EQUAL(StatusAndError(NodeStatus::Undefined, NodeErrorCode::NoError),
             ParseStatus(Status::Undefined), ());
  TEST_EQUAL(StatusAndError(NodeStatus::Error, NodeErrorCode::NoInetConnection),
             ParseStatus(Status::DownloadFailed), ());
  TEST_EQUAL(StatusAndError(NodeStatus::Downloading, NodeErrorCode::NoError),
             ParseStatus(Status::Downloading), ());
}

UNIT_TEST(StorageTest_ForEachInSubtree)
{
  Storage storage(kCountriesTxt, make_unique<TestMapFilesDownloader>());

  CountriesVec leafVec;
  auto const forEach = [&leafVec](CountryId const & descendantId, bool groupNode) {
    if (!groupNode)
      leafVec.push_back(descendantId);
  };
  storage.ForEachInSubtree(storage.GetRootId(), forEach);

  CountriesVec const expectedLeafVec = {"Abkhazia",
                                        "OutdatedCountry1",
                                        "OutdatedCountry2",
                                        "Algeria_Central",
                                        "Algeria_Coast",
                                        "South Korea_South",
                                        "Disputable Territory",
                                        "Indisputable Territory Of Country1",
                                        "Indisputable Territory Of Country2",
                                        "Disputable Territory"};
  TEST_EQUAL(leafVec, expectedLeafVec, ());
}

UNIT_TEST(StorageTest_ForEachAncestorExceptForTheRoot)
{
  Storage storage(kCountriesTxt, make_unique<TestMapFilesDownloader>());

  // Two parent case.
  auto const forEachParentDisputableTerritory = [](CountryId const & parentId,
                                                   CountryTree::Node const & parentNode) {
    CountriesVec descendants;
    parentNode.ForEachDescendant([&descendants](CountryTree::Node const & container) {
      descendants.push_back(container.Value().Name());
    });

    if (parentId == "Country1")
    {
      CountriesVec const expectedDescendants = {"Disputable Territory",
                                                "Indisputable Territory Of Country1"};
      TEST_EQUAL(descendants, expectedDescendants, ());
      return;
    }
    if (parentId == "Country2")
    {
      CountriesVec const expectedDescendants = {"Indisputable Territory Of Country2",
                                                "Disputable Territory"};
      TEST_EQUAL(descendants, expectedDescendants, ());
      return;
    }
    TEST(false, ());
  };
  storage.ForEachAncestorExceptForTheRoot("Disputable Territory", forEachParentDisputableTerritory);

  // One parent case.
  auto const forEachParentIndisputableTerritory = [](CountryId const & parentId,
                                                     CountryTree::Node const & parentNode) {
    CountriesVec descendants;
    parentNode.ForEachDescendant([&descendants](CountryTree::Node const & container) {
      descendants.push_back(container.Value().Name());
    });

    if (parentId == "Country1")
    {
      CountriesVec const expectedDescendants = {"Disputable Territory",
                                                "Indisputable Territory Of Country1"};
      TEST_EQUAL(descendants, expectedDescendants, ());
      return;
    }
    TEST(false, ());
  };
  storage.ForEachAncestorExceptForTheRoot("Indisputable Territory Of Country1",
                                          forEachParentIndisputableTerritory);
}

UNIT_CLASS_TEST(StorageTest, CalcLimitRect)
{
  auto const countryInfoGetter = CreateCountryInfoGetter();
  ASSERT(countryInfoGetter, ());

  m2::RectD const boundingBox = storage::CalcLimitRect("Algeria", storage, *countryInfoGetter);
  m2::RectD const expectedBoundingBox = {-8.6689 /* minX */, 19.32443 /* minY */,
                                         11.99734 /* maxX */, 45.23 /* maxY */};

  TEST(AlmostEqualRectsAbs(boundingBox, expectedBoundingBox), ());
}

UNIT_TEST(StorageTest_CountriesNamesTest)
{
  string const kRuJson =
      string(R"json({
             "Countries":"Весь мир",
             "Abkhazia":"Абхазия",
             "Algeria":"Алжир",
             "Algeria_Central":"Алжир (центральная часть)",
             "Algeria_Coast":"Алжир (побережье)",
             "Country1":"Страна 1",
             "Disputable Territory":"Спорная территория",
             "Indisputable Territory Of Country1":"Бесспорная территория страны 1",
             "Country2":"Страна 2",
             "Indisputable Territory Of Country2":"Бесспорная территория страны 2"
             })json");

  string const kFrJson =
      string(R"json({
             "Countries":"Des pays",
             "Abkhazia":"Abkhazie",
             "Algeria":"Algérie",
             "Algeria_Central":"Algérie (partie centrale)",
             "Algeria_Coast":"Algérie (Côte)",
             "Country1":"Pays 1",
             "Disputable Territory":"Territoire contesté",
             "Indisputable Territory Of Country1":"Territoire incontestable. Pays 1.",
             "Country2":"Pays 2",
             "Indisputable Territory Of Country2":"Territoire incontestable. Pays 2"
             })json");

  Storage storage(kCountriesTxt, make_unique<TestMapFilesDownloader>());

  // set russian locale
  storage.SetLocaleForTesting(kRuJson, "ru");
  TEST_EQUAL(string("ru"), storage.GetLocale(), ());

  NodeAttrs nodeAttrs;
  storage.GetNodeAttrs("Abkhazia", nodeAttrs);
  TEST_EQUAL(nodeAttrs.m_nodeLocalName, "Абхазия", ());
  TEST_EQUAL(nodeAttrs.m_parentInfo.size(), 1, ());
  TEST_EQUAL(nodeAttrs.m_parentInfo[0].m_localName, "Весь мир", ());
  TEST(nodeAttrs.m_topmostParentInfo.empty(), ());

  nodeAttrs = NodeAttrs();
  storage.GetNodeAttrs("Algeria", nodeAttrs);
  TEST_EQUAL(nodeAttrs.m_nodeLocalName, "Алжир", ());
  TEST_EQUAL(nodeAttrs.m_parentInfo.size(), 1, ());
  TEST_EQUAL(nodeAttrs.m_parentInfo[0].m_localName, "Весь мир", ());
  TEST(nodeAttrs.m_topmostParentInfo.empty(), ());

  nodeAttrs = NodeAttrs();
  storage.GetNodeAttrs("Algeria_Coast", nodeAttrs);
  TEST_EQUAL(nodeAttrs.m_nodeLocalName, "Алжир (побережье)", ());
  TEST_EQUAL(nodeAttrs.m_parentInfo.size(), 1, ());
  TEST_EQUAL(nodeAttrs.m_parentInfo[0].m_localName, "Алжир", ());
  TEST_EQUAL(nodeAttrs.m_topmostParentInfo.size(), 1, ());
  TEST_EQUAL(nodeAttrs.m_topmostParentInfo[0].m_id, "Algeria", ());
  TEST_EQUAL(nodeAttrs.m_topmostParentInfo[0].m_localName, "Алжир", ());

  nodeAttrs = NodeAttrs();
  storage.GetNodeAttrs("Algeria_Central", nodeAttrs);
  TEST_EQUAL(nodeAttrs.m_nodeLocalName, "Алжир (центральная часть)", ());
  TEST_EQUAL(nodeAttrs.m_parentInfo.size(), 1, ());
  TEST_EQUAL(nodeAttrs.m_parentInfo[0].m_localName, "Алжир", ());

  nodeAttrs = NodeAttrs();
  storage.GetNodeAttrs("South Korea_South", nodeAttrs);
  TEST_EQUAL(nodeAttrs.m_nodeLocalName, "South Korea_South", ());
  TEST_EQUAL(nodeAttrs.m_parentInfo.size(), 1, ());
  TEST_EQUAL(nodeAttrs.m_parentInfo[0].m_localName, "Весь мир", ());
  TEST(nodeAttrs.m_topmostParentInfo.empty(), ());

  nodeAttrs = NodeAttrs();
  storage.GetNodeAttrs("Disputable Territory", nodeAttrs);
  TEST_EQUAL(nodeAttrs.m_nodeLocalName, "Спорная территория", ());
  TEST_EQUAL(nodeAttrs.m_parentInfo.size(), 2, ());
  TEST_EQUAL(nodeAttrs.m_parentInfo[0].m_localName, "Страна 1", ());
  TEST_EQUAL(nodeAttrs.m_parentInfo[1].m_localName, "Страна 2", ());
  vector<CountryIdAndName> const expectedTopmostParentsRu = {{"Country1", "Страна 1"},
                                                             {"Country2", "Страна 2"}};
  TEST(nodeAttrs.m_topmostParentInfo == expectedTopmostParentsRu, ());

  // set french locale
  storage.SetLocaleForTesting(kFrJson, "fr");
  TEST_EQUAL(string("fr"), storage.GetLocale(), ());

  nodeAttrs = NodeAttrs();
  storage.GetNodeAttrs("Abkhazia", nodeAttrs);
  TEST_EQUAL(nodeAttrs.m_nodeLocalName, "Abkhazie", ());
  TEST_EQUAL(nodeAttrs.m_parentInfo.size(), 1, ());
  TEST_EQUAL(nodeAttrs.m_parentInfo[0].m_localName, "Des pays", ());
  TEST(nodeAttrs.m_topmostParentInfo.empty(), ());

  nodeAttrs = NodeAttrs();
  storage.GetNodeAttrs("Algeria", nodeAttrs);
  TEST_EQUAL(nodeAttrs.m_nodeLocalName, "Algérie", ());
  TEST_EQUAL(nodeAttrs.m_parentInfo.size(), 1, ());
  TEST_EQUAL(nodeAttrs.m_parentInfo[0].m_localName, "Des pays", ());
  TEST(nodeAttrs.m_topmostParentInfo.empty(), ());

  nodeAttrs = NodeAttrs();
  storage.GetNodeAttrs("Algeria_Coast", nodeAttrs);
  TEST_EQUAL(nodeAttrs.m_nodeLocalName, "Algérie (Côte)", ());
  TEST_EQUAL(nodeAttrs.m_parentInfo.size(), 1, ());
  TEST_EQUAL(nodeAttrs.m_parentInfo[0].m_localName, "Algérie", ());
  TEST_EQUAL(nodeAttrs.m_topmostParentInfo.size(), 1, ());
  TEST_EQUAL(nodeAttrs.m_topmostParentInfo[0].m_id, "Algeria", ());
  TEST_EQUAL(nodeAttrs.m_topmostParentInfo[0].m_localName, "Algérie", ());

  nodeAttrs = NodeAttrs();
  storage.GetNodeAttrs("Algeria_Central", nodeAttrs);
  TEST_EQUAL(nodeAttrs.m_nodeLocalName, "Algérie (partie centrale)", ());
  TEST_EQUAL(nodeAttrs.m_parentInfo.size(), 1, ());
  TEST_EQUAL(nodeAttrs.m_parentInfo[0].m_localName, "Algérie", ());
  TEST_EQUAL(nodeAttrs.m_topmostParentInfo.size(), 1, ());
  TEST_EQUAL(nodeAttrs.m_topmostParentInfo[0].m_id, "Algeria", ());
  TEST_EQUAL(nodeAttrs.m_topmostParentInfo[0].m_localName, "Algérie", ());

  nodeAttrs = NodeAttrs();
  storage.GetNodeAttrs("South Korea_South", nodeAttrs);
  TEST_EQUAL(nodeAttrs.m_nodeLocalName, "South Korea_South", ());
  TEST_EQUAL(nodeAttrs.m_parentInfo.size(), 1, ());
  TEST_EQUAL(nodeAttrs.m_parentInfo[0].m_localName, "Des pays", ());
  TEST(nodeAttrs.m_topmostParentInfo.empty(), ());

  nodeAttrs = NodeAttrs();
  storage.GetNodeAttrs("Disputable Territory", nodeAttrs);
  TEST_EQUAL(nodeAttrs.m_nodeLocalName, "Territoire contesté", ());
  TEST_EQUAL(nodeAttrs.m_parentInfo.size(), 2, ());
  TEST_EQUAL(nodeAttrs.m_parentInfo[0].m_localName, "Pays 1", ());
  TEST_EQUAL(nodeAttrs.m_parentInfo[1].m_localName, "Pays 2", ());
  vector<CountryIdAndName> const expectedTopmostParentsFr = {{"Country1", "Pays 1"},
                                                             {"Country2", "Pays 2"}};
  TEST(nodeAttrs.m_topmostParentInfo == expectedTopmostParentsFr, ());
}

UNIT_TEST(StorageTest_DeleteNodeWithoutDownloading)
{
  Storage storage(kCountriesTxt, make_unique<TestMapFilesDownloader>());
  TaskRunner runner;
  InitStorage(storage, runner);

  storage.DeleteNode("Illegal_countryId");
  storage.DeleteNode("Algeria_Central");
  NodeAttrs nodeAttrs = NodeAttrs();
  storage.GetNodeAttrs("Algeria_Central", nodeAttrs);
  TEST_EQUAL(nodeAttrs.m_status, NodeStatus::NotDownloaded, ());
}

UNIT_TEST(StorageTest_GetOverallProgressSmokeTest)
{
  Storage storage(kCountriesTxt, make_unique<TestMapFilesDownloader>());
  TaskRunner runner;
  InitStorage(storage, runner);

  auto const currentProgress = storage.GetOverallProgress({"Abkhazia", "Algeria_Coast"});
  TEST_EQUAL(currentProgress.m_bytesDownloaded, 0, ());
  TEST_EQUAL(currentProgress.m_bytesTotal, 0, ());
}

UNIT_TEST(StorageTest_GetQueuedChildrenSmokeTest)
{
  Storage storage(kCountriesTxt, make_unique<TestMapFilesDownloader>());
  TaskRunner runner;
  InitStorage(storage, runner);

  CountriesVec queuedChildren;
  storage.GetQueuedChildren("Countries", queuedChildren);
  TEST(queuedChildren.empty(), ());

  storage.GetQueuedChildren("Abkhazia", queuedChildren);
  TEST(queuedChildren.empty(), ());

  storage.GetQueuedChildren("Country1", queuedChildren);
  TEST(queuedChildren.empty(), ());
}
  
UNIT_TEST(StorageTest_GetGroupNodePathToRootTest)
{
  Storage storage;

  CountriesVec path;

  storage.GetGroupNodePathToRoot("France_Auvergne_Allier", path);
  TEST(path.empty(), ());
  
  storage.GetGroupNodePathToRoot("France_Auvergne", path);
  TEST_EQUAL(path.size(), 2, (path));
  TEST_EQUAL(path[0], "France", ());
  TEST_EQUAL(path[1], "Countries", ());
  
  storage.GetGroupNodePathToRoot("France", path);
  TEST_EQUAL(path.size(), 1, (path));
  TEST_EQUAL(path[0], "Countries", ());
  
  storage.GetGroupNodePathToRoot("US_Florida_Miami", path);
  TEST(path.empty(), ());

  storage.GetGroupNodePathToRoot("Florida", path);
  TEST_EQUAL(path.size(), 2, (path));
  TEST_EQUAL(path[0], "United States of America", ());
  TEST_EQUAL(path[1], "Countries", ());

  storage.GetGroupNodePathToRoot("Country1", path);
  TEST(path.empty(), ());
}

UNIT_TEST(StorageTest_GetTopmostNodesFor)
{
  Storage storage;

  CountriesVec path;

  storage.GetTopmostNodesFor("France_Auvergne_Allier", path);
  TEST_EQUAL(path.size(), 1, (path));
  TEST_EQUAL(path[0], "France", ());

  storage.GetTopmostNodesFor("France_Auvergne", path);
  TEST_EQUAL(path.size(), 1, (path));
  TEST_EQUAL(path[0], "France", ());

  storage.GetTopmostNodesFor("Jerusalem", path);
  TEST_EQUAL(path.size(), 2, (path));
  TEST_EQUAL(path[0], "Israel Region", (path));
  TEST_EQUAL(path[1], "Palestine Region", (path));
}

UNIT_TEST(StorageTest_GetTopmostParentFor)
{
  Storage storage;

  TEST_EQUAL(storage.GetTopmostParentFor("France_Auvergne_Allier"), "France", ());
  TEST_EQUAL(storage.GetTopmostParentFor("France_Auvergne"), "France", ());
  TEST_EQUAL(storage.GetTopmostParentFor("Belgium"), "Belgium", ());
  TEST_EQUAL(storage.GetTopmostParentFor("Jerusalem"), "Jerusalem", ());
  TEST_EQUAL(storage.GetTopmostParentFor("US_California_LA"), "United States of America", ());
}

UNIT_TEST(StorageTest_GetTopmostNodesForWithLevel)
{
  Storage storage;

  CountriesVec path;

  storage.GetTopmostNodesFor("France_Burgundy_Saone-et-Loire", path, 0);
  TEST_EQUAL(path.size(), 1, (path));
  TEST_EQUAL(path[0], "France", ());

  storage.GetTopmostNodesFor("France_Burgundy_Saone-et-Loire", path, 1);
  TEST_EQUAL(path.size(), 1, (path));
  TEST_EQUAL(path[0], "France_Burgundy", ());

  storage.GetTopmostNodesFor("France_Burgundy_Saone-et-Loire", path, 2);
  TEST_EQUAL(path.size(), 1, (path));
  TEST_EQUAL(path[0], "France_Burgundy_Saone-et-Loire", ());

  // Below tests must return path with single element same as input.
  storage.GetTopmostNodesFor("France_Burgundy_Saone-et-Loire", path, 3);
  TEST_EQUAL(path.size(), 1, (path));
  TEST_EQUAL(path[0], "France_Burgundy_Saone-et-Loire", ());

  storage.GetTopmostNodesFor("France_Burgundy_Saone-et-Loire", path, -1);
  TEST_EQUAL(path.size(), 1, (path));
  TEST_EQUAL(path[0], "France_Burgundy_Saone-et-Loire", ());

  storage.GetTopmostNodesFor("France_Burgundy_Saone-et-Loire", path, 10000);
  TEST_EQUAL(path.size(), 1, (path));
  TEST_EQUAL(path[0], "France_Burgundy_Saone-et-Loire", ());
}

UNIT_TEST(StorageTest_FalsePolicy)
{
  DummyDownloadingPolicy policy;
  Storage storage;
  storage.SetDownloadingPolicy(&policy);

  storage.Init(&OnCountryDownloaded /* didDownload */,
               [](CountryId const &, LocalFilePtr const) { return false; } /* willDelete */);
  storage.SetDownloaderForTesting(make_unique<TestMapFilesDownloader>());
  storage.SetCurrentDataVersionForTesting(1234);

  auto const countryId = storage.FindCountryIdByFile("Uruguay");
  auto const countryFile = storage.GetCountryFile(countryId);

  // To prevent interference with other tests and on other tests it's
  // better to remove temporary downloader files.
  DeleteDownloaderFilesForCountry(storage.GetCurrentDataVersion(), countryFile);
  SCOPE_GUARD(cleanup, [&]() {
    DeleteDownloaderFilesForCountry(storage.GetCurrentDataVersion(),
                                    countryFile);
  });

  {
    FailedDownloadingWaiter waiter(storage, countryId);
    storage.DownloadCountry(countryId, MapFileType::Map);
  }
}

UNIT_CLASS_TEST(StorageTest, MultipleMaps)
{
  // This test tries do download all maps from Russian Federation, but
  // network policy sometimes changes, therefore some countries won't
  // be downloaded.

  vector<uint64_t> const failedRequests {{5, 10, 21}};
  TEST(is_sorted(failedRequests.begin(), failedRequests.end()), ());

  SometimesFailingDownloadingPolicy policy(failedRequests);
  Storage storage;
  storage.SetDownloadingPolicy(&policy);

  auto const nodeId = storage.FindCountryIdByFile("Russian Federation");
  CountriesVec children;
  storage.GetChildren(nodeId, children);
  vector<bool> downloaded(children.size());

  auto const onStatusChange = [&](CountryId const & id) {
    auto const status = storage.CountryStatusEx(id);
    if (status != Status::OnDisk)
      return;

    auto const it = find(children.cbegin(), children.cend(), id);
    if (it == children.end())
      return;

    downloaded[distance(children.cbegin(), it)] = true;
  };

  auto const onProgress = [&](CountryId const & /* countryId */,
                              downloader::Progress const & /* progress */) {};

  auto const slot = storage.Subscribe(onStatusChange, onProgress);
  SCOPE_GUARD(cleanup, [&]() { storage.Unsubscribe(slot); });

  storage.Init(&OnCountryDownloaded /* didDownload */,
               [](CountryId const &, LocalFilePtr const) { return false; } /* willDelete */);
  storage.SetDownloaderForTesting(make_unique<FakeMapFilesDownloader>(runner));
  // Disable because of FakeMapFilesDownloader.
  storage.SetEnabledIntegrityValidationForTesting(false);
  storage.DownloadNode(nodeId);
  runner.Run();

  for (size_t i = 0; i < downloaded.size(); ++i)
  {
    auto const expected = !binary_search(failedRequests.begin(), failedRequests.end(), i);
    TEST_EQUAL(downloaded[i], expected, ("Unexpected status for country:", children[i]));
  }

  // Unfortunately, whole country was not downloaded.
  TEST_EQUAL(storage.CountryStatusEx(nodeId), Status::NotDownloaded, ());
}
}  // namespace storage
