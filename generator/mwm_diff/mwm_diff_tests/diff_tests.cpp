#include "testing/testing.hpp"

#include "generator/mwm_diff/diff.hpp"

#include "platform/platform.hpp"

#include "coding/file_writer.hpp"
#include "coding/internal/file_data.hpp"

#include "base/file_name_utils.hpp"
#include "base/logging.hpp"
#include "base/scope_guard.hpp"

#include <vector>

namespace generator::diff_tests
{
using namespace mwm_diff;
using std::string, std::vector;

UNIT_TEST(IncrementalUpdates_Smoke)
{
  base::ScopedLogAbortLevelChanger ignoreLogError(base::LogLevel::LCRITICAL);

  string const oldMwmPath = base::JoinPath(GetPlatform().WritableDir(), "minsk-pass.mwm");
  string const newMwmPath1 = base::JoinPath(GetPlatform().WritableDir(), "minsk-pass-new1.mwm");
  string const newMwmPath2 = base::JoinPath(GetPlatform().WritableDir(), "minsk-pass-new2.mwm");
  string const diffPath = base::JoinPath(GetPlatform().WritableDir(), "minsk-pass.mwmdiff");

  SCOPE_GUARD(cleanup, [&] {
    FileWriter::DeleteFileX(newMwmPath1);
    FileWriter::DeleteFileX(newMwmPath2);
    FileWriter::DeleteFileX(diffPath);
  });

  {
    // Create an empty file.
    FileWriter writer(newMwmPath1);
  }

  base::Cancellable cancellable;

  // Reset cancellable before test.
  cancellable.Reset();
  LOG(LINFO, ("Starting first diff application"));
  TEST(MakeDiff(oldMwmPath, newMwmPath1, diffPath, cancellable), ());
  TEST_EQUAL(ApplyDiff(oldMwmPath, newMwmPath2, diffPath, cancellable), DiffApplicationResult::Ok, ());

  {
    // Alter the old mwm slightly.
    vector<uint8_t> oldMwmContents = base::ReadFile(oldMwmPath);
    size_t const sz = oldMwmContents.size();
    for (size_t i = 3 * sz / 10; i < 4 * sz / 10; i++)
      oldMwmContents[i] += static_cast<uint8_t>(i);

    FileWriter writer(newMwmPath1);
    writer.Write(oldMwmContents.data(), oldMwmContents.size());
  }

  // Reset cancellable before test.
  cancellable.Reset();
  LOG(LINFO, ("Starting second diff application"));
  TEST(MakeDiff(oldMwmPath, newMwmPath1, diffPath, cancellable), ());
  TEST_EQUAL(ApplyDiff(oldMwmPath, newMwmPath2, diffPath, cancellable), DiffApplicationResult::Ok, ());

  TEST(base::IsEqualFiles(newMwmPath1, newMwmPath2), ());

  cancellable.Cancel();
  LOG(LINFO, ("Cancelling ApplyDiff"));
  TEST_EQUAL(ApplyDiff(oldMwmPath, newMwmPath2, diffPath, cancellable), DiffApplicationResult::Cancelled, ());
  cancellable.Reset();

  {
    // Corrupt the diff file contents.
    vector<uint8_t> diffContents = base::ReadFile(diffPath);

    // Leave the version bits intact.
    for (size_t i = 4; i < diffContents.size(); i += 2)
      diffContents[i] ^= 255;

    FileWriter writer(diffPath);
    writer.Write(diffContents.data(), diffContents.size());
  }

  // Reset cancellable before test.
  cancellable.Reset();
  LOG(LINFO, ("Starting corrupted diff application"));
  TEST_EQUAL(ApplyDiff(oldMwmPath, newMwmPath2, diffPath, cancellable), DiffApplicationResult::Failed, ());

  {
    // Reset the diff file contents.
    FileWriter writer(diffPath);
  }

  // Reset cancellable before test.
  cancellable.Reset();
  LOG(LINFO, ("Starting empty diff application"));
  TEST_EQUAL(ApplyDiff(oldMwmPath, newMwmPath2, diffPath, cancellable), DiffApplicationResult::Failed, ());
}
}  // namespace generator::diff_tests
