#pragma once

#include <map>
#include <string>

#include "localization/localization.hpp"

namespace om::localization
{
class StringsBundle
{
  static const std::string kEmpty;

  using TStringMap = std::map<std::string, std::string>;
  TStringMap m_values;
  TStringMap m_defValues;

public:
  StringsBundle();

  void SetString(std::string const & name, std::string const & value);
  std::string const & GetString(std::string const & name) const;
};
}  // namespace om::localization
