#include "string_storage_base.hpp"

#include "coding/reader_streambuf.hpp"
#include "coding/file_reader.hpp"
#include "coding/file_writer.hpp"

#include "base/exception.hpp"
#include "base/logging.hpp"
#include "base/stl_helpers.hpp"

#include <istream>

namespace platform
{
namespace
{
constexpr char kKeyValueDelimChar = '=';
}  // namespace

StringStorageBase::StringStorageBase(std::string const & path) : m_path(path)
{
  try
  {
    LOG(LINFO, ("Settings path:", m_path));
    ReaderStreamBuf buffer(std::make_unique<FileReader>(m_path));
    std::istream stream(&buffer);

    std::string line;
    while (getline(stream, line))
    {
      if (line.empty())
        continue;

      size_t const delimPos = line.find(kKeyValueDelimChar);
      if (delimPos == std::string::npos)
        continue;

      std::string key = line.substr(0, delimPos);
      std::string value = line.substr(delimPos + 1);
      if (!key.empty() && !value.empty())
      {
        LOG(LINFO, (key, ":", value));
        VERIFY(m_values.emplace(std::move(key), std::move(value)).second, ());
      }
    }
  }
  catch (RootException const & ex)
  {
    LOG(LWARNING, ("Loading settings:", ex.Msg()));
  }
}

void StringStorageBase::Save() const
{
  try
  {
    FileWriter file(m_path);
    for (auto const & value : m_values)
    {
      std::string line(value.first);
      line += kKeyValueDelimChar;
      line += value.second;
      line += '\n';
      file.Write(line.data(), line.size());
    }
  }
  catch (RootException const & ex)
  {
    // Ignore all settings saving exceptions.
    LOG(LWARNING, ("Saving settings:", ex.Msg()));
  }
}

void StringStorageBase::Clear()
{
  std::lock_guard guard(m_mutex);
  m_values.clear();
  Save();
}

bool StringStorageBase::GetValue(std::string_view key, std::string & outValue) const
{
  std::lock_guard guard(m_mutex);

  auto const found = m_values.find(key);
  if (found == m_values.end())
    return false;

  outValue = found->second;
  return true;
}

void StringStorageBase::SetValue(std::string_view key, std::string && value)
{
  std::lock_guard guard(m_mutex);

  base::EmplaceOrAssign(m_values, key, std::move(value));

  Save();
}

void StringStorageBase::Update(std::map<std::string, std::string> const & values)
{
  std::lock_guard guard(m_mutex);

  for (const auto & pair : values)
  {
    if (pair.second.empty())
      m_values.erase(pair.first);
    else
      m_values[pair.first] = pair.second;
  }

  Save();
}

void StringStorageBase::DeleteKeyAndValue(std::string_view key)
{
  std::lock_guard guard(m_mutex);

  auto const found = m_values.find(key);
  if (found != m_values.end())
  {
    m_values.erase(found);
    Save();
  }
}
}  // namespace platform
