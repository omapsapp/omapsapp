#pragma once

#include "platform/localization.hpp"

#include <string>
#include <set>
#include <chrono>

namespace platform
{

class Duration
{
public:
  enum class Units
  {
    Days = 0,
    Hours = 1,
    Minutes = 2,
  };

  explicit Duration(unsigned long seconds);

  static std::string GetUnitsString(Units unit);

  std::string GetLocalizedString(std::initializer_list<Units> units, Locale const & locale) const;
  std::string GetPlatformLocalizedString() const;

private:
  const std::chrono::seconds m_seconds;

  std::string GetString(std::initializer_list<Units> units, std::string_view unitSeparator, std::string_view groupingSeparator) const;
};

std::string DebugPrint(Duration::Units units);

}  // namespace platform
