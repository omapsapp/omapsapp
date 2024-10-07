#include "testing/testing.hpp"

#include "platform/mwm_version.hpp"
#include "platform/platform.hpp"
#include "platform/platform_tests_support/scoped_file.hpp"

#include "coding/file_writer.hpp"
#include "coding/internal/file_data.hpp"

#include "base/file_name_utils.hpp"
#include "base/logging.hpp"
#include "base/scope_guard.hpp"
#include "base/stl_helpers.hpp"

#include <functional>
#include <initializer_list>
#include <set>
#include <string>
#include <utility>

#include "defines.hpp"

namespace
{
char const * TEST_FILE_NAME = "some_temporary_unit_test_file.tmp";

void CheckFilesPresence(std::string const & baseDir, unsigned typeMask,
                        std::initializer_list<std::pair<std::string, size_t>> const & files)
{
  Platform::TFilesWithType fwts;
  Platform::GetFilesByType(baseDir, typeMask, fwts);

  std::multiset<std::string> filesSet;
  for (auto const & fwt : fwts)
    filesSet.insert(fwt.first);

  for (auto const & file : files)
    TEST_EQUAL(filesSet.count(file.first), file.second, (file.first, file.second));
}
}  // namespace

UNIT_TEST(WritableDir)
{
  std::string const path = GetPlatform().WritableDir() + TEST_FILE_NAME;

  try
  {
    base::FileData f(path, base::FileData::Op::WRITE_TRUNCATE);
  }
  catch (Writer::OpenException const &)
  {
    LOG(LCRITICAL, ("Can't create file", path));
    return;
  }

  base::DeleteFileX(path);
}

UNIT_TEST(WritablePathForFile)
{
  Platform & pl = GetPlatform();
  std::string const p1 = base::JoinPath(pl.WritableDir(), TEST_FILE_NAME);
  std::string const p2 = pl.WritablePathForFile(TEST_FILE_NAME);
  TEST_EQUAL(p1, p2, ());
}

UNIT_TEST(GetReader)
{
  char const * NON_EXISTING_FILE = "mgbwuerhsnmbui45efhdbn34.tmp";
  char const * arr[] = {
    "symbols/mdpi/light/symbols.sdf",
    "classificator.txt",
    "minsk-pass.mwm"
  };

  Platform & p = GetPlatform();
  for (size_t i = 0; i < ARRAY_SIZE(arr); ++i)
  {
    ReaderPtr<Reader> r = p.GetReader(arr[i]);
    TEST_GREATER(r.Size(), 0, ("File should exist!"));
  }

  bool wasException = false;
  try
  {
    ReaderPtr<Reader> r = p.GetReader(NON_EXISTING_FILE);
  }
  catch (FileAbsentException const &)
  {
    wasException = true;
  }
  TEST_EQUAL(wasException, true, ());
}

UNIT_TEST(GetFilesInDir_Smoke)
{
  Platform & pl = GetPlatform();
  Platform::FilesList files1, files2;

  std::string const dir = pl.ResourcesDir();

  pl.GetFilesByExt(dir, DATA_FILE_EXTENSION, files1);
  TEST_GREATER(files1.size(), 0, (dir, "folder should contain some data files"));

  TEST(base::IsExist(files1, "minsk-pass.mwm"), ());

  pl.GetFilesByRegExp(dir, ".*\\" DATA_FILE_EXTENSION "$", files2);
  TEST_EQUAL(files1, files2, ());

  files1.clear();
  pl.GetFilesByExt(dir, ".dsa", files1);
  TEST_EQUAL(files1.size(), 0, ());
}

UNIT_TEST(DirsRoutines)
{
  std::string const baseDir = GetPlatform().WritableDir();
  std::string const testDir = base::JoinPath(baseDir, "test-dir");
  std::string const testFile = base::JoinPath(testDir, "test-file");

  TEST(!Platform::IsFileExistsByFullPath(testDir), ());
  TEST_EQUAL(Platform::MkDir(testDir), Platform::ERR_OK, ());

  TEST(Platform::IsFileExistsByFullPath(testDir), ());
  TEST(Platform::IsDirectoryEmpty(testDir), ());

  {
    FileWriter writer(testFile);
  }
  TEST(!Platform::IsDirectoryEmpty(testDir), ());
  FileWriter::DeleteFileX(testFile);

  TEST_EQUAL(Platform::RmDir(testDir), Platform::ERR_OK, ());

  TEST(!Platform::IsFileExistsByFullPath(testDir), ());
}

UNIT_TEST(GetFilesByType)
{
  std::string const kTestDirBaseName = "test-dir";
  std::string const kTestFileBaseName = "test-file";

  std::string const baseDir = GetPlatform().WritableDir();

  std::string const testDir = base::JoinPath(baseDir, kTestDirBaseName);
  TEST_EQUAL(Platform::MkDir(testDir), Platform::ERR_OK, ());
  SCOPE_GUARD(removeTestDir, bind(&Platform::RmDir, testDir));

  std::string const testFile = base::JoinPath(baseDir, kTestFileBaseName);
  TEST(!Platform::IsFileExistsByFullPath(testFile), ());
  {
    FileWriter writer(testFile);
  }
  TEST(Platform::IsFileExistsByFullPath(testFile), ());
  SCOPE_GUARD(removeTestFile, bind(FileWriter::DeleteFileX, testFile));

  CheckFilesPresence(baseDir, Platform::EFileType::Directory,
  {{
     kTestDirBaseName, 1 /* present */
   },
   {
     kTestFileBaseName, 0 /* not present */
   }});
  CheckFilesPresence(baseDir, Platform::EFileType::Regular,
  {{
     kTestDirBaseName, 0 /* not present */
   },
   {
     kTestFileBaseName, 1 /* present */
   }});
  CheckFilesPresence(baseDir, Platform::EFileType::Directory | Platform::EFileType::Regular,
  {{
     kTestDirBaseName, 1 /* present */
   },
   {
     kTestFileBaseName, 1 /* present */
   }});
}

UNIT_TEST(GetFileSize)
{
  Platform & pl = GetPlatform();
  uint64_t size = 123141;
  TEST(!pl.GetFileSizeByName("adsmngfuwrbfyfwe", size), ());
  TEST(!pl.IsFileExistsByFullPath("adsmngfuwrbfyfwe"), ());

  char const kContent[] = "HOHOHO";
  size_t const kSize = ARRAY_SIZE(kContent);
  std::string const fileName = pl.WritablePathForFile(TEST_FILE_NAME);
  {
    FileWriter testFile(fileName);
    testFile.Write(kContent, kSize);
  }
  size = 0;
  TEST(Platform::GetFileSizeByFullPath(fileName, size), ());
  TEST_EQUAL(size, kSize, ());

  FileWriter::DeleteFileX(fileName);
  TEST(!pl.IsFileExistsByFullPath(fileName), ());

  {
    FileWriter testFile(fileName);
    testFile.Write(kContent, kSize);
  }
  size = 0;
  TEST(pl.GetFileSizeByName(TEST_FILE_NAME, size), ());
  TEST_EQUAL(size, kSize, ());

  FileWriter::DeleteFileX(fileName);
}

UNIT_TEST(CpuCores)
{
  int const coresNum = GetPlatform().CpuCores();
  TEST_GREATER(coresNum, 0, ());
  TEST_LESS_OR_EQUAL(coresNum, 128, ());
}

UNIT_TEST(GetWritableStorageStatus)
{
  TEST_EQUAL(Platform::STORAGE_OK, GetPlatform().GetWritableStorageStatus(100000), ());
  TEST_EQUAL(Platform::NOT_ENOUGH_SPACE, GetPlatform().GetWritableStorageStatus(uint64_t(-1)), ());
}

UNIT_TEST(RmDirRecursively)
{
  std::string const testDir1 = base::JoinPath(GetPlatform().WritableDir(), "test_dir1");
  TEST_EQUAL(Platform::MkDir(testDir1), Platform::ERR_OK, ());
  SCOPE_GUARD(removeTestDir1, bind(&Platform::RmDir, testDir1));

  std::string const testDir2 = base::JoinPath(testDir1, "test_dir2");
  TEST_EQUAL(Platform::MkDir(testDir2), Platform::ERR_OK, ());
  SCOPE_GUARD(removeTestDir2, bind(&Platform::RmDir, testDir2));

  std::string const filePath = base::JoinPath(testDir2, "test_file");
  {
    FileWriter testFile(filePath);
    testFile.Write("HOHOHO", 6);
  }
  SCOPE_GUARD(removeTestFile, bind(&base::DeleteFileX, filePath));

  TEST(Platform::IsFileExistsByFullPath(filePath), ());
  TEST(Platform::IsFileExistsByFullPath(testDir1), ());
  TEST(Platform::IsFileExistsByFullPath(testDir2), ());

  TEST_EQUAL(Platform::ERR_DIRECTORY_NOT_EMPTY, Platform::RmDir(testDir1), ());

  TEST(Platform::IsFileExistsByFullPath(filePath), ());
  TEST(Platform::IsFileExistsByFullPath(testDir1), ());
  TEST(Platform::IsFileExistsByFullPath(testDir2), ());

  TEST(Platform::RmDirRecursively(testDir1), ());

  TEST(!Platform::IsFileExistsByFullPath(filePath), ());
  TEST(!Platform::IsFileExistsByFullPath(testDir1), ());
  TEST(!Platform::IsFileExistsByFullPath(testDir2), ());
}

UNIT_TEST(MkDirRecursively)
{
  using namespace platform::tests_support;
  auto const writablePath = GetPlatform().WritableDir();
  auto const workPath = base::JoinPath(writablePath, "MkDirRecursively");
  auto const resetDir =  [](std::string const & path) {
    if (Platform::IsFileExistsByFullPath(path) && !Platform::RmDirRecursively(path))
       return false;

    return Platform::MkDirChecked(path);
  };

  CHECK(resetDir(workPath), ());
  auto const path = base::JoinPath(workPath, "test1", "test2", "test3");
  TEST(Platform::MkDirRecursively(path), ());
  TEST(Platform::IsFileExistsByFullPath(path), ());
  TEST(Platform::IsDirectory(path), ());

  CHECK(resetDir(workPath), ());
  auto const filePath = base::JoinPath(workPath, "test1");
  SCOPE_GUARD(removeTestFile, bind(&base::DeleteFileX, filePath));
  {
    FileWriter testFile(filePath);
  }

  TEST(!Platform::MkDirRecursively(path), ());
  TEST(!Platform::IsFileExistsByFullPath(path), ());

  CHECK(Platform::RmDirRecursively(workPath), ());
}

UNIT_TEST(Platform_ThreadRunner)
{
  {
    Platform::ThreadRunner m_runner;

    bool called = false;
    GetPlatform().RunTask(Platform::Thread::File, [&called]
    {
      called = true;
      testing::Notify();
    });
    testing::Wait();

    TEST(called, ());
  }

  GetPlatform().RunTask(Platform::Thread::File, []
  {
    TEST(false, ("The task must not be posted when thread runner is dead. "
                 "But app must not be crashed. It is normal behaviour during destruction"));
  });
}

UNIT_TEST(GetFileCreationTime_GetFileModificationTime)
{
  auto const now = std::time(nullptr);

  std::string_view constexpr kContent{"HOHOHO"};
  std::string const fileName = GetPlatform().WritablePathForFile(TEST_FILE_NAME);
  {
    FileWriter testFile(fileName);
    testFile.Write(kContent.data(), kContent.size());
  }
  SCOPE_GUARD(removeTestFile, bind(&base::DeleteFileX, fileName));

  auto const creationTime = Platform::GetFileCreationTime(fileName);
  TEST_GREATER_OR_EQUAL(creationTime, now, ());

  {
    FileWriter testFile(fileName);
    testFile.Write(kContent.data(), kContent.size());
  }

  auto const modificationTime = Platform::GetFileModificationTime(fileName);
  TEST_GREATER_OR_EQUAL(modificationTime, creationTime, ());
}
