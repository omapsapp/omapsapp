#include "testing/testing.hpp"

#include "platform/duration.hpp"

#include <chrono>

namespace platform
{
using std::chrono::duration_cast, std::chrono::seconds, std::chrono::minutes, std::chrono::hours, std::chrono::days;

struct TestData
{
  struct Duration
  {
    days m_days;
    hours m_hours;
    minutes m_minutes;
    seconds m_seconds;
    std::string result;

    Duration(long days, long hours, long minutes, long seconds, std::string const & result)
    : m_days(days), m_hours(hours), m_minutes(minutes), m_seconds(seconds), result(result)
    {}

    long Seconds() const
    {
      return (duration_cast<seconds>(m_days) +
              duration_cast<seconds>(m_hours) +
              duration_cast<seconds>(m_minutes) +
              m_seconds).count();
    }
  };

  Locale m_locale;
  std::vector<Duration> m_duration;

  constexpr TestData(Locale locale, std::vector<Duration> duration)
  : m_locale(locale), m_duration(duration)
  {}
};

Locale GetLocale(std::string const & language)
{
  Locale locale;
  locale.m_language = language;
  return locale;
}
/*
 Localized string cannot be retrieved from the app target bundle during the tests execution
 and the platform::GetLocalizedString will return the same string as the input ("minute", "hour" etc).
 This is why the expectation strings are not explicit.
 */

auto const m = Duration::GetUnitsString(Duration::Units::Minutes);
auto const h = Duration::GetUnitsString(Duration::Units::Hours);
auto const d = Duration::GetUnitsString(Duration::Units::Days);

UNIT_TEST(Duration_AllUnits)
{
  TestData const testData[] = {
    {GetLocale("en"),
      {
        {0, 0, 0, 0,     "0" + m},
        {0, 0, 0, 30,    "0" + m},
        {0, 0, 0, 59,    "0" + m},
        {0, 0, 1, 0,     "1" + m},
        {0, 0, 1, 59,    "1" + m},
        {0, 0, 60, 0,    "1" + h},
        {0, 0, 123, 0,   "2" + h + kNonBreakingSpace + "3" + m},
        {0, 3, 0, 0,     "3" + h},
        {0, 24, 0, 0,    "1" + d},
        {4, 0, 0, 0,     "4" + d},
        {1, 2, 3, 0,     "1" + d + kNonBreakingSpace + "2" + h + kNonBreakingSpace + "3" + m},
        {1, 0, 15, 0,    "1" + d + kNonBreakingSpace + "15" + m},
        {0, 15, 1, 0,    "15" + h + kNonBreakingSpace + "1" + m},
        {1, 15, 0, 0,    "1" + d + kNonBreakingSpace + "15" + h},
        {15, 0, 10, 0,   "15" + d + kNonBreakingSpace + "10" + m},
        {15, 15, 15, 0,  "15" + d + kNonBreakingSpace + "15" + h + kNonBreakingSpace + "15" + m}
      }
    },
  };

  for (auto const & data : testData)
  {
    for (auto const & dataDuration : data.m_duration)
    {
      auto const duration = Duration(dataDuration.Seconds());
      auto durationStr = duration.GetLocalizedString({Duration::Units::Days, Duration::Units::Hours, Duration::Units::Minutes}, data.m_locale);
      TEST_EQUAL(durationStr, dataDuration.result, ());
    }
  }
}

UNIT_TEST(Duration_Localization)
{
  TestData const testData[] = {
    // en
    {GetLocale("en"), {{1, 2, 3, 0,  "1" + d + kNonBreakingSpace + "2" + h + kNonBreakingSpace + "3" + m}}},
    // ru (narrow spacing between number and unit)
    {GetLocale("ru"), {{1, 2, 3, 0, "1" + kNarrowNonBreakingSpace + d + kNonBreakingSpace + "2" + kNarrowNonBreakingSpace + h + kNonBreakingSpace + "3" + kNarrowNonBreakingSpace + m}}},
    // zh (no spacings)
    {GetLocale("zh"), {{1, 2, 3, 0, "1" + d + "2" + h + "3" + m}}}
  };

  for (auto const & data : testData)
  {
    for (auto const & duration : data.m_duration)
    {
      auto const durationStr = Duration(duration.Seconds()).GetLocalizedString({Duration::Units::Days, Duration::Units::Hours, Duration::Units::Minutes}, data.m_locale);
      TEST_EQUAL(durationStr, duration.result, ());
    }
  }
}
}  // namespace platform
