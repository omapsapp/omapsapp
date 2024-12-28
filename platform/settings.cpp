#include "platform/settings.hpp"

#include "platform/location.hpp"
#include "platform/measurement_utils.hpp"
#include "platform/platform.hpp"

#include "defines.hpp"

#include "coding/transliteration.hpp"

#include "geometry/any_rect2d.hpp"
#include "geometry/rect2d.hpp"

#include "base/string_utils.hpp"

#include <iostream>
#include <sstream>

namespace settings
{
using namespace std;

std::string_view kMeasurementUnits = "Units";
std::string_view kMapLanguageCode = "MapLanguageCode";
std::string_view kDeveloperMode = "DeveloperMode";
std::string_view kDonateUrl = "DonateUrl";
std::string_view kNightMode = "NightMode";

StringStorage::StringStorage() : StringStorageBase(GetPlatform().SettingsPathForFile(SETTINGS_FILE_NAME)) {}

StringStorage & StringStorage::Instance()
{
  static StringStorage inst;
  return inst;
}

////////////////////////////////////////////////////////////////////////////////////////////

template <>
string ToString<string>(string const & str)
{
  return str;
}

template <>
bool FromString<string>(string const & strIn, string & strOut)
{
  strOut = strIn;
  return true;
}

namespace impl
{
template <class T, size_t N>
bool FromStringArray(string const & s, T(&arr)[N])
{
  istringstream in(s);
  size_t count = 0;
  while (count < N && in >> arr[count])
  {
    if (!strings::is_finite(arr[count]))
      return false;
    ++count;
  }

  return (!in.fail() && count == N);
}
}  // namespace impl

template <>
string ToString<m2::AnyRectD>(m2::AnyRectD const & rect)
{
  ostringstream out;
  out.precision(12);
  m2::PointD glbZero(rect.GlobalZero());
  out << glbZero.x << " " << glbZero.y << " ";
  out << rect.Angle().val() << " ";
  m2::RectD const & r = rect.GetLocalRect();
  out << r.minX() << " " << r.minY() << " " << r.maxX() << " " << r.maxY();
  return out.str();
}

template <>
bool FromString<m2::AnyRectD>(string const & str, m2::AnyRectD & rect)
{
  double val[7];
  if (!impl::FromStringArray(str, val))
    return false;

  // Will get an assertion in DEBUG and false return in RELEASE.
  m2::RectD const r(val[3], val[4], val[5], val[6]);
  if (!r.IsValid())
    return false;

  rect = m2::AnyRectD(m2::PointD(val[0], val[1]), ang::AngleD(val[2]), r);
  return true;
}

template <>
string ToString<m2::RectD>(m2::RectD const & rect)
{
  ostringstream stream;
  stream.precision(12);
  stream << rect.minX() << " " << rect.minY() << " " << rect.maxX() << " " << rect.maxY();
  return stream.str();
}
template <>
bool FromString<m2::RectD>(string const & str, m2::RectD & rect)
{
  double val[4];
  if (!impl::FromStringArray(str, val))
    return false;

  // Will get an assertion in DEBUG and false return in RELEASE.
  rect = m2::RectD(val[0], val[1], val[2], val[3]);
  return rect.IsValid();
}

template <>
string ToString<bool>(bool const & v)
{
  return v ? "true" : "false";
}

template <>
bool FromString<bool>(string const & str, bool & v)
{
  if (str == "true")
    v = true;
  else if (str == "false")
    v = false;
  else
    return false;
  return true;
}

namespace impl
{
template <typename T>
string ToStringScalar(T const & v)
{
  ostringstream stream;
  stream.precision(12);
  stream << v;
  return stream.str();
}

template <typename T>
bool FromStringScalar(string const & str, T & v)
{
  istringstream stream(str);
  if (stream)
  {
    stream >> v;
    return !stream.fail();
  }
  else
    return false;
}
}  // namespace impl

template <>
string ToString<double>(double const & v)
{
  return impl::ToStringScalar<double>(v);
}

template <>
bool FromString<double>(string const & str, double & v)
{
  return impl::FromStringScalar<double>(str, v);
}

template <>
string ToString<int32_t>(int32_t const & v)
{
  return impl::ToStringScalar<int32_t>(v);
}

template <>
bool FromString<int32_t>(string const & str, int32_t & v)
{
  return impl::FromStringScalar<int32_t>(str, v);
}

template <>
string ToString<int64_t>(int64_t const & v)
{
  return impl::ToStringScalar<int64_t>(v);
}

template <>
bool FromString<int64_t>(string const & str, int64_t & v)
{
  return impl::FromStringScalar<int64_t>(str, v);
}

template <>
string ToString<uint32_t>(uint32_t const & v)
{
  return impl::ToStringScalar<uint32_t>(v);
}

template <>
string ToString<uint64_t>(uint64_t const & v)
{
  return impl::ToStringScalar<uint64_t>(v);
}

template <>
bool FromString<uint32_t>(string const & str, uint32_t & v)
{
  return impl::FromStringScalar<uint32_t>(str, v);
}

template <>
bool FromString<uint64_t>(string const & str, uint64_t & v)
{
  return impl::FromStringScalar<uint64_t>(str, v);
}

namespace impl
{
template <class TPair>
string ToStringPair(TPair const & value)
{
  ostringstream stream;
  stream.precision(12);
  stream << value.first << " " << value.second;
  return stream.str();
}

template <class TPair>
bool FromStringPair(string const & str, TPair & value)
{
  istringstream stream(str);
  if (stream)
  {
    stream >> value.first;
    if (stream)
    {
      stream >> value.second;
      return !stream.fail();
    }
  }
  return false;
}
}  // namespace impl

typedef pair<int, int> IPairT;
typedef pair<double, double> DPairT;

template <>
string ToString<IPairT>(IPairT const & v)
{
  return impl::ToStringPair(v);
}

template <>
bool FromString<IPairT>(string const & s, IPairT & v)
{
  return impl::FromStringPair(s, v);
}

template <>
string ToString<DPairT>(DPairT const & v)
{
  return impl::ToStringPair(v);
}

template <>
bool FromString<DPairT>(string const & s, DPairT & v)
{
  return impl::FromStringPair(s, v);
}

template <>
string ToString<measurement_utils::Units>(measurement_utils::Units const & v)
{
  switch (v)
  {
  // The value "Foot" is left here for compatibility with old settings.ini files.
  case measurement_utils::Units::Imperial: return "Foot";
  case measurement_utils::Units::Metric: return "Metric";
  }
  UNREACHABLE();
}

template <>
bool FromString<measurement_utils::Units>(string const & s, measurement_utils::Units & v)
{
  if (s == "Metric")
    v = measurement_utils::Units::Metric;
  else if (s == "Foot")
    v = measurement_utils::Units::Imperial;
  else
    return false;

  return true;
}

template <>
string ToString<location::EMyPositionMode>(location::EMyPositionMode const & v)
{
  switch (v)
  {
  case location::PendingPosition: return "PendingPosition";
  case location::NotFollow: return "NotFollow";
  case location::NotFollowNoPosition: return "NotFollowNoPosition";
  case location::Follow: return "Follow";
  case location::FollowAndRotate: return "FollowAndRotate";
  default: return "Pending";
  }
}

template <>
bool FromString<location::EMyPositionMode>(string const & s, location::EMyPositionMode & v)
{
  if (s == "PendingPosition")
    v = location::PendingPosition;
  else if (s == "NotFollow")
    v = location::NotFollow;
  else if (s == "NotFollowNoPosition")
    v = location::NotFollowNoPosition;
  else if (s == "Follow")
    v = location::Follow;
  else if (s == "FollowAndRotate")
    v = location::FollowAndRotate;
  else
    return false;

  return true;
}

template <>
string ToString<Transliteration::Mode>(Transliteration::Mode const & mode)
{
  switch (mode)
  {
  case Transliteration::Mode::Enabled: return "Enabled";
  case Transliteration::Mode::Disabled: return "Disabled";
  }
  UNREACHABLE();
}

template <>
bool FromString<Transliteration::Mode>(string const & s, Transliteration::Mode & mode)
{
  if (s == "Enabled")
    mode = Transliteration::Mode::Enabled;
  else if (s == "Disabled")
    mode = Transliteration::Mode::Disabled;
  else
    return false;

  return true;
}

UsageStats::UsageStats()
  : m_firstLaunch("US_FirstLaunch")
  , m_lastBackground("US_LastBackground")
  , m_totalForeground("US_TotalForeground")
  , m_sessions("US_SessionsCount")
  , m_ss(StringStorage::Instance())
{
  std::string str;
  uint64_t val;
  if (m_ss.GetValue(m_totalForeground, str) && FromString(str, val))
    m_totalForegroundTime = val;

  if (m_ss.GetValue(m_sessions, str) && FromString(str, val))
    m_sessionsCount = val;

  if (!m_ss.GetValue(m_firstLaunch, str))
  {
    auto const fileTime = Platform::GetFileCreationTime(GetPlatform().SettingsPathForFile(SETTINGS_FILE_NAME));
    if (fileTime > 0)
    {
      // Check that file wasn't created on this first launch (1 hour threshold).
      uint64_t const first = base::TimeTToSecondsSinceEpoch(fileTime);
      uint64_t const curr = TimeSinceEpoch();
      if (curr >= first + 3600 /* 1 hour */)
        m_ss.SetValue(m_firstLaunch, ToString(first));
    }
  }
}

uint64_t UsageStats::TimeSinceEpoch()
{
  using namespace std::chrono;
  return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

void UsageStats::EnterForeground()
{
  m_enterForegroundTime = TimeSinceEpoch();
}

void UsageStats::EnterBackground()
{
  uint64_t const currTime = TimeSinceEpoch();

  // Safe check if something wrong with device's time.
  ASSERT_GREATER_OR_EQUAL(currTime, m_enterForegroundTime, ());
  if (currTime < m_enterForegroundTime)
    return;

  // Safe check if uninitialized. Possible when entering background on DownloadResourcesLegacyActivity.
  //ASSERT(m_enterForegroundTime > 0, ());
  if (m_enterForegroundTime == 0)
    return;

  // Save first launch.
  std::string dummy;
  if (!m_ss.GetValue(m_firstLaunch, dummy))
    m_ss.SetValue(m_firstLaunch, ToString(m_enterForegroundTime));

  // Save last background.
  m_ss.SetValue(m_lastBackground, ToString(currTime));

  // Aggregate foreground duration.
  m_totalForegroundTime += (currTime - m_enterForegroundTime);
  m_ss.SetValue(m_totalForeground, ToString(m_totalForegroundTime));

  // Aggregate sessions count.
  ++m_sessionsCount;
  m_ss.SetValue(m_sessions, ToString(m_sessionsCount));
}

bool UsageStats::IsLoyalUser() const
{
  #ifdef DEBUG
  uint32_t constexpr kMinTotalForegroundTimeout = 30;
  uint32_t constexpr kMinSessionsCount = 3;
  #else
  uint32_t constexpr kMinTotalForegroundTimeout = 60 * 30; // 30 min
  uint32_t constexpr kMinSessionsCount = 5;
  #endif
  return m_sessionsCount >= kMinSessionsCount && m_totalForegroundTime >= kMinTotalForegroundTimeout;
}

}  // namespace settings
