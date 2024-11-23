#include "testing/testing.hpp"

#include "platform/platform.hpp"

#include "coding/zip_reader.hpp"
#include "coding/internal/file_data.hpp"

#include "base/file_name_utils.hpp"
#include "base/thread.hpp"
#include "base/thread_pool.hpp"
#include "base/logging.hpp"

#include <cstdint>
#include <memory>
#include <numeric>
#include <string>

namespace apk_test
{
using std::string, std::vector;

char const * arrFiles[] = {
  "copyright.html",
  "resources-mdpi_light/symbols.sdf",
  "resources-mdpi_light/symbols.png",
  "resources-hdpi_light/symbols.sdf",
  "resources-hdpi_light/symbols.png",
  "resources-xhdpi_light/symbols.sdf",
  "resources-xhdpi_light/symbols.png",
  "categories.txt",
  "categories_cuisines.txt",
  "classificator.txt",
  "types.txt",
  "fonts/blacklist.txt",
  "fonts/whitelist.txt",
  "fonts/unicode_blocks.txt",
  "languages.txt",
  "drules_proto_default_light.bin",
  "packed_polygons.bin",
  "countries.txt"
};

class ApkTester : public threads::IRoutine
{
  static const int COUNT = ARRAY_SIZE(arrFiles);
  string const & m_cont;

public:
  explicit ApkTester(string const & cont) : m_cont(cont), m_hashes(COUNT)
  {
  }

  virtual void Do()
  {
    string const prefix("assets/");

    while (true)
    {
      size_t ind = rand() % COUNT;
      if (m_hashes[ind] != 0)
      {
        ind = COUNT;
        for (size_t i = 0; i < COUNT; ++i)
          if (m_hashes[i] == 0)
          {
            ind = i;
            break;
          }
      }

      if (ind == COUNT)
        break;

      try
      {
        ZipFileReader reader(m_cont, prefix + arrFiles[ind]);

        size_t const size = reader.Size();
        vector<char> buffer(size);
        reader.Read(0, &buffer[0], size);

        m_hashes[ind] = accumulate(buffer.begin(), buffer.end(), static_cast<uint64_t>(0));
      }
      catch (Reader::Exception const & ex)
      {
        LOG(LERROR, (ex.Msg()));
        break;
      }
    }
  }

  vector<uint64_t> m_hashes;
};

/*
UNIT_TEST(ApkReader_Multithreaded)
{
  /// @todo Update test with current apk path.
  string const path = base::JoinPath(GetPlatform().WritableDir(), "../android/MapsWithMePro/bin/MapsWithMePro-production.apk");

  uint64_t size;
  if (!base::GetFileSize(path, size))
  {
    LOG(LINFO, ("Apk not found"));
    return;
  }

  srand(static_cast<unsigned>(size));

  size_t const count = 20;
  base::thread_pool::routine_simple::ThreadPool pool(count);

  for (size_t i = 0; i < count; ++i)
    pool.Add(make_unique<ApkTester>(path));

  pool.Join();

  typedef ApkTester const * PtrT;
  PtrT etalon = dynamic_cast<PtrT>(pool.GetRoutine(0));
  for (size_t i = 1; i < count; ++i)
  {
    PtrT p = dynamic_cast<PtrT>(pool.GetRoutine(i));
    TEST_EQUAL(etalon->m_hashes, p->m_hashes, ());
  }
*/
}  // namespace apk_test
