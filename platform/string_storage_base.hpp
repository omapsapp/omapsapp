#pragma once

#include <map>
#include <mutex>
#include <string>

namespace platform
{
class StringStorageBase
{
public:
  explicit StringStorageBase(std::string const & path);
  void Save() const;
  void Clear();
  bool GetValue(std::string_view key, std::string & outValue) const;
  void SetValue(std::string_view key, std::string && value);
  void Update(std::map<std::string, std::string> const & values);
  void DeleteKeyAndValue(std::string_view key);

private:
  using Container = std::map<std::string, std::string, std::less<>>;
  Container m_values;
  mutable std::mutex m_mutex;
  std::string const m_path;
};
}  // namespace platform
