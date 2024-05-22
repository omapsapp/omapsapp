#include "testing/testing.hpp"

#include "storage/storage_integration_tests/test_defines.hpp"

#include "map/framework.hpp"

#include "platform/platform.hpp"
#include "platform/platform_tests_support/writable_dir_changer.hpp"

#include "base/file_name_utils.hpp"
#include "base/scope_guard.hpp"

#include <algorithm>
#include <string>

#include "defines.hpp"

using namespace platform;
using namespace storage;

namespace
{

int GetLevelCount(Storage & storage, CountryId const & countryId)
{
  CountriesVec children;
  storage.GetChildren(countryId, children);
  int level = 0;
  for (auto const & child : children)
    level = std::max(level, GetLevelCount(storage, child));
  return 1 + level;
}
} // namespace

UNIT_TEST(SmallMwms_3levels_Test)
{
  WritableDirChanger writableDirChanger(kMapTestDir);

  Platform & platform = GetPlatform();

  /// @todo So sick, but Framework.RoutingManager has so complicated logic with a bunch of
  /// RunOnGui callbacks, so delete Framework also in RunOnGui.
  auto * frm = new Framework(FrameworkParams(false /* m_enableDiffs */));

  SCOPE_GUARD(deleteFramework, [frm]()
  {
    GetPlatform().RunTask(Platform::Thread::Gui, [frm]()
    {
      delete frm;
    });
  });

  auto & storage = frm->GetStorage();
  std::string const version = strings::to_string(storage.GetCurrentDataVersion());

  CountryId country = "Germany";
  TEST_EQUAL(3, GetLevelCount(storage, country), ());

  std::string const mapDir = base::JoinPath(platform.WritableDir(), version);

  auto onProgressFn = [&](CountryId const & /* countryId */, downloader::Progress const & /* progress */) {};

  auto onChangeCountryFn = [&](CountryId const & /* countryId */) {
    if (!storage.IsDownloadInProgress())
      testing::StopEventLoop();
  };

  storage.Subscribe(onChangeCountryFn, onProgressFn);
  storage.SetDownloadingServersForTesting({kTestWebServer});

  /// @todo Download all Germany > 2GB takes hours here ..
  country = "Kiribati";

  NodeAttrs attrs;
  storage.GetNodeAttrs(country, attrs);
  TEST_EQUAL(attrs.m_status, NodeStatus::NotDownloaded, ());

  Platform::FilesList files;
  platform.GetFilesByExt(mapDir, DATA_FILE_EXTENSION, files);
  TEST_EQUAL(0, files.size(), ());

  storage.DownloadNode(country);
  testing::RunEventLoop();

  storage.GetNodeAttrs(country, attrs);
  TEST_EQUAL(attrs.m_status, NodeStatus::OnDisk, ());

  files.clear();
  platform.GetFilesByExt(mapDir, DATA_FILE_EXTENSION, files);
  TEST_GREATER(files.size(), 0, ());

  storage.DeleteNode(country);

  storage.GetNodeAttrs(country, attrs);
  TEST_EQUAL(attrs.m_status, NodeStatus::NotDownloaded, ());

  files.clear();
  platform.GetFilesByExt(mapDir, DATA_FILE_EXTENSION, files);
  TEST_EQUAL(0, files.size(), ());
}
