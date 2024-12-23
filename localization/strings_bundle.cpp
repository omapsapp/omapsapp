#include "strings_bundle.hpp"

namespace om::localization
{
const std::string StringsBundle::kEmpty = "";

StringsBundle::StringsBundle() { init::SetDefaultStrings(m_defValues); }

void StringsBundle::SetString(std::string const & name, std::string const & value) { m_values[name] = value; }

std::string const & StringsBundle::GetString(std::string const & name) const
{
  auto it = m_values.find(name);
  if (it != m_values.end())
    return it->second;
  else
  {
    it = m_defValues.find(name);
    if (it != m_defValues.end())
      return it->second;
  }
  return kEmpty;
}
}  // namespace om::localization
