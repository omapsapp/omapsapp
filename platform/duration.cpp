#include "duration.hpp"

#include "base/stl_helpers.hpp"

/// @todo(KK): move the formatting code from the platform namespace
namespace platform
{
namespace
{
using namespace std::chrono;

static constexpr std::string_view kNoSpace = "";

unsigned long SecondsToUnits(seconds duration, Duration::Units unit)
{
  switch (unit)
  {
    case Duration::Units::Days: return duration_cast<days>(duration).count();
    case Duration::Units::Hours: return duration_cast<hours>(duration).count();
    case Duration::Units::Minutes: return duration_cast<minutes>(duration).count();
    default: UNREACHABLE();
  }
}

seconds UnitsToSeconds(long value, Duration::Units unit)
{
  switch (unit)
  {
    case Duration::Units::Days: return days(value);
    case Duration::Units::Hours: return hours(value);
    case Duration::Units::Minutes: return minutes(value);
    default: UNREACHABLE();
  }
}

std::string_view GetUnitSeparator(Locale const & locale)
{
  static constexpr auto kEmptyNumberUnitSeparatorLocales = std::array
  {
    "en", "de", "fr", "he", "fa", "ja", "ko", "mr", "th", "tr", "vi", "zh"
  };
  bool const isEmptySeparator = base::IsExist(kEmptyNumberUnitSeparatorLocales, locale.m_language);
  return isEmptySeparator ? kNoSpace : kNarrowNonBreakingSpace;
}

std::string_view GetUnitsGroupingSeparator(Locale const & locale)
{
  static constexpr auto kEmptyGroupingSeparatorLocales = std::array
  {
    "ja", "zh"
  };
  bool const isEmptySeparator = base::IsExist(kEmptyGroupingSeparatorLocales, locale.m_language);
  return isEmptySeparator ? kNoSpace : kNonBreakingSpace;
}

bool IsUnitsOrderValid(std::initializer_list<Duration::Units> units)
{
  return base::IsSortedAndUnique(units.begin(), units.end());
}
} // namespace

Duration::Duration(unsigned long seconds) : m_seconds(seconds)
{}

std::string Duration::GetLocalizedString(std::initializer_list<Units> units, Locale const & locale) const
{
  return GetString(std::move(units), GetUnitSeparator(locale), GetUnitsGroupingSeparator(locale));
}

std::string Duration::GetPlatformLocalizedString() const
{
  struct InitSeparators
  {
    std::string_view m_unitSep, m_groupingSep;
    InitSeparators()
    {
      auto const loc = GetCurrentLocale();
      m_unitSep = GetUnitSeparator(loc);
      m_groupingSep = GetUnitsGroupingSeparator(loc);
    }
  };
  static InitSeparators seps;

  return GetString({Units::Days, Units::Hours, Units::Minutes}, seps.m_unitSep, seps.m_groupingSep);
}

std::string Duration::GetString(std::initializer_list<Units> units, std::string_view unitSeparator,
                                std::string_view groupingSeparator) const
{
  ASSERT(units.size(), ());
  ASSERT(IsUnitsOrderValid(units), ());

  if (SecondsToUnits(m_seconds, Units::Minutes) == 0)
    return std::to_string(0U).append(unitSeparator).append(GetUnitsString(Units::Minutes));

  std::string formattedTime;
  seconds remainingSeconds = m_seconds;

  for (auto const unit : units)
  {
    const unsigned long unitsCount = SecondsToUnits(remainingSeconds, unit);
    if (unitsCount > 0)
    {
      if (!formattedTime.empty())
        formattedTime.append(groupingSeparator);
      formattedTime.append(std::to_string(unitsCount).append(unitSeparator).append(GetUnitsString(unit)));
      remainingSeconds -= UnitsToSeconds(unitsCount, unit);
    }
  }
  return formattedTime;
}

std::string Duration::GetUnitsString(Units unit)
{
  switch (unit)
  {
    case Units::Minutes: return platform::GetLocalizedString("minute");
    case Units::Hours: return platform::GetLocalizedString("hour");
    case Units::Days: return platform::GetLocalizedString("day");
    default: UNREACHABLE();
  }
}

std::string DebugPrint(Duration::Units units)
{
  switch (units)
  {
    case Duration::Units::Days: return "d";
    case Duration::Units::Hours: return "h";
    case Duration::Units::Minutes: return "m";
    default: UNREACHABLE();
  }
}
}  // namespace platform
