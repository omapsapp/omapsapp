#include "routing/turns_tts_text.hpp"

#include "routing/turns_sound_settings.hpp"
#include "routing/turns_tts_text_i18n.hpp"

#include "indexer/road_shields_parser.hpp"

#include "base/string_utils.hpp"

#include <regex>
#include <string>

namespace routing::turns::sound
{

namespace
{

template <class TIter> std::string DistToTextId(TIter begin, TIter end, uint32_t dist)
{
  TIter const it = std::lower_bound(begin, end, dist, [](auto const & p1, uint32_t p2) { return p1.first < p2; });
  if (it == end)
  {
    ASSERT(false, ("notification.m_distanceUnits is not correct."));
    return {};
  }

  if (it != begin)
  {
    // Rounding like 130 -> 100; 135 -> 200 is better than upper bound, IMO.
    auto iPrev = it;
    --iPrev;
    if ((dist - iPrev->first) * 2 < (it->first - dist))
      return iPrev->second;
  }

  return it->second;
}
}  //  namespace

void GetTtsText::SetLocale(std::string const & locale)
{
  m_getCurLang = platform::GetTextByIdFactory(platform::TextSource::TtsSound, locale);
}

void GetTtsText::ForTestingSetLocaleWithJson(std::string const & jsonBuffer, std::string const & locale)
{
  m_getCurLang = platform::ForTestingGetTextByIdFactory(jsonBuffer, locale);
}

/**
 * @brief Modified version of GetFullRoadName in routing_session.cpp.
 * For next street returns "ref; name" .
 * For highway exits (or main roads with exit info) returns "junction:ref; target:ref; target".
 * If no |target| - it will be replaced by |name| of next street.
 * If no |target:ref| - it will be replaced by |ref| of next road.
 * So if link has no info at all, "ref; name" of next will be returned (as for next street).
 *
 * @arg RouteSegment::RoadNameInfo & road - structured info input about the next street
 * @arg std::string & name - semicolon-delimited string output for the TTS engine
 */
void FormatFullRoadName(RouteSegment::RoadNameInfo & road, std::string & name)
{
  name.clear();

  if (auto const & sh = ftypes::GetRoadShields(road.m_ref); !sh.empty())
    road.m_ref = sh[0].m_name;
  if (auto const & sh = ftypes::GetRoadShields(road.m_destination_ref); !sh.empty())
    road.m_destination_ref = sh[0].m_name;

  std::vector<std::string> outArr;

  if (road.HasExitInfo())
  {
    if (!road.m_junction_ref.empty())
      outArr.emplace_back(road.m_junction_ref);

    if (!road.m_destination_ref.empty())
      outArr.emplace_back(road.m_destination_ref);

    if (!road.m_destination.empty())
      outArr.emplace_back(road.m_destination);
    else if (!road.m_name.empty())
      outArr.emplace_back(road.m_name);
  }
  else
  {
    if (!road.m_ref.empty())
      outArr.emplace_back(road.m_ref);
    if (!road.m_name.empty())
      outArr.emplace_back(road.m_name);
  }

  name = strings::JoinStrings(outArr.begin(), outArr.end(), "; ");

  strings::Trim(name);
}

std::string GetTtsText::GetTurnNotification(Notification const & notification) const
{
  const std::string localeKey = GetLocale();
  const std::string dirKey = GetDirectionTextId(notification);
  std::string dirStr = GetTextByIdTrimmed(dirKey);

  if (notification.m_distanceUnits == 0 && !notification.m_useThenInsteadOfDistance &&
      notification.m_nextStreetInfo.empty())
    return dirStr;

  if (notification.IsPedestrianNotification())
  {
    if (notification.m_useThenInsteadOfDistance &&
        notification.m_turnDirPedestrian == PedestrianDirection::None)
      return {};
  }

  if (notification.m_useThenInsteadOfDistance && notification.m_turnDir == CarDirection::None)
    return {};

  if (dirStr.empty())
    return {};

  std::string thenStr;
  if (notification.m_useThenInsteadOfDistance)
  {
    // add "then" and space only if needed, for appropriate languages
    thenStr = GetTextByIdTrimmed("then");
    if (localeKey != "ja")
      thenStr.push_back(' ');
  }

  std::string distStr;
  if (notification.m_distanceUnits > 0)
    distStr = GetTextByIdTrimmed(GetDistanceTextId(notification));

  // Get a string like 245; CA 123; Highway 99; San Francisco
  // In the future we could use the full RoadNameInfo struct to do some nice formatting.
  std::string streetOut;
  RouteSegment::RoadNameInfo nsi = notification.m_nextStreetInfo;  // extract non-const
  FormatFullRoadName(nsi, streetOut);

  if (!streetOut.empty())
  {
    // We're going to pronounce the street name.

    // Replace any full-stop characters (in between sub-instructions) to make TTS flow better.
    // Full stops are: . (Period) or 。 (East Asian) or । (Hindi)
    RemoveLastDot(distStr);

    // If the turn direction with the key +_street exists for this locale, and isn't "NULL",
    // use it (like make_a_right_turn_street)
    std::string dirStreetStr = GetTextByIdTrimmed(dirKey + "_street");
    if (!dirStreetStr.empty() && dirStreetStr != "NULL")
      dirStr = std::move(dirStreetStr);

    // Normally use "onto" for "turn right onto Main St" unless it's "NULL"
    std::string ontoStr = GetTextByIdTrimmed("onto");
    if (ontoStr == "NULL")
      ontoStr.clear();

    // If the nextStreetInfo has an exit number, we'll announce it
    if (!notification.m_nextStreetInfo.m_junction_ref.empty())
    {
      // Try to get a specific "take exit #" phrase and its associated "onto" phrase (if any)
      std::string dirExitStr = GetTextByIdTrimmed("take_exit_number");
      if (!dirExitStr.empty())
      {
        dirStr = std::move(dirExitStr);
        ontoStr.clear();  // take_exit_number overwrites "onto"
      }
    }

    // Same as above but for dirStr instead of distStr
    RemoveLastDot(dirStr);

    std::string distDirOntoStreetStr = GetTextByIdTrimmed("dist_direction_onto_street");
    // only load populate _street_verb if _street exists and isn't "NULL"
    // may also need to handle a lack of a $5 position in the formatter string
    std::string dirVerb;
    if (!dirStreetStr.empty() && dirStreetStr != "NULL")
    {
      dirVerb = GetTextByIdTrimmed(dirKey + "_street_verb");
      if (dirVerb == "NULL")
        dirVerb.clear();
    }

    if (localeKey == "hu")
    {
      HungarianBaseWordTransform(streetOut);  // modify streetOut's last letter if it's a vowel

      // adjust the -re suffix in the formatter string based on last-word vowels
      uint8_t hungarianism = CategorizeHungarianLastWordVowels(streetOut);

      if (hungarianism == 1)
        strings::ReplaceLast(distDirOntoStreetStr, "-re", "re");  // just remove hyphenation
      else if (hungarianism == 2)
        strings::ReplaceLast(distDirOntoStreetStr, "-re", "ra");  // change re to ra without hyphen
      else
        strings::ReplaceLast(distDirOntoStreetStr, "-re", "");  // clear it

      // if the first pronounceable character of the street is a vowel, use "az" instead of "a"
      // 1, 5, and 1000 start with vowels but not 10 or 100 (numbers are combined as in English: 5*, 5**, 1*, 1**, 1***, etc)
      static const std::regex rHun("^[5aeiouyáéíóúöüőű]|^1$|^1[^\\d]|^1\\d\\d\\d[^\\d]", std::regex_constants::icase);
      std::smatch ma;
      if (std::regex_search(streetOut, ma, rHun) && ma.size() > 0)
      {
        if (ontoStr == "a")
          ontoStr.push_back('z');
        if (dirStr == "Hajtson ki a")
          dirStr.push_back('z');
      }
    }

    char ttsOut[1024];
    std::snprintf(ttsOut, std::size(ttsOut), distDirOntoStreetStr.c_str(),
                  distStr.c_str(),    // in 100 feet
                  dirStr.c_str(),     // turn right / take exit
                  ontoStr.c_str(),    // onto / null
                  streetOut.c_str(),  // Main Street / 543:: M4: Queens Parkway, London
                  dirVerb.c_str()     // (optional "turn right" verb)
    );

    // remove floating punctuation
    static const std::regex rP(" [,\\.:;]+ ");
    std::string cleanOut = std::regex_replace(ttsOut, rP, " ");
    // remove repetitious spaces or colons
    static const std::regex rS("[ :]{2,99}");
    cleanOut = std::regex_replace(cleanOut, rS, " ");
    // trim leading spaces
    strings::Trim(cleanOut);

    LOG(LINFO, ("TTSn", thenStr + cleanOut));

    return thenStr + cleanOut;
  }

  std::string out;
  if (!distStr.empty())
  {
    // add distance and/or space only if needed, for appropriate languages
    if (localeKey != "ja")
      out = thenStr + distStr + " " + dirStr;
    else
      out = thenStr + distStr + dirStr;
  }
  else
  {
    out = thenStr + dirStr;
  }
  LOG(LINFO, ("TTS", out));
  return out;
}

std::string GetTtsText::GetRecalculatingNotification() const
{
  return GetTextById("route_recalculating");
}

std::string GetTtsText::GetSpeedCameraNotification() const
{
  return GetTextById("unknown_camera");
}

std::string GetTtsText::GetLocale() const
{
  if (m_getCurLang == nullptr)
  {
    ASSERT(false, ());
    return {};
  }
  return m_getCurLang->GetLocale();
}

std::string GetTtsText::GetTextByIdTrimmed(std::string const & textId) const
{
  std::string out = GetTextById(textId);
  strings::Trim(out);
  return out;
}

std::string GetTtsText::GetTextById(std::string const & textId) const
{
  ASSERT(!textId.empty(), ());

  if (m_getCurLang == nullptr)
  {
    ASSERT(false, ());
    return {};
  }
  return (*m_getCurLang)(textId);
}

std::string GetDistanceTextId(Notification const & notification)
{
//  if (notification.m_useThenInsteadOfDistance)
//    return "then";

  switch (notification.m_lengthUnits)
  {
  case measurement_utils::Units::Metric:
    return DistToTextId(GetAllSoundedDistMeters().cbegin(), GetAllSoundedDistMeters().cend(),
                        notification.m_distanceUnits);
  case measurement_utils::Units::Imperial:
    return DistToTextId(GetAllSoundedDistFeet().cbegin(), GetAllSoundedDistFeet().cend(),
                        notification.m_distanceUnits);
  }
  ASSERT(false, ());
  return {};
}

std::string GetRoundaboutTextId(Notification const & notification)
{
  if (notification.m_turnDir != CarDirection::LeaveRoundAbout)
  {
    ASSERT(false, ());
    return {};
  }
  if (!notification.m_useThenInsteadOfDistance)
    return "leave_the_roundabout"; // Notification just before leaving a roundabout.

  static constexpr uint8_t kMaxSoundedExit = 11;
  if (notification.m_exitNum == 0 || notification.m_exitNum > kMaxSoundedExit)
    return "leave_the_roundabout";

  return "take_the_" + strings::to_string(static_cast<int>(notification.m_exitNum)) + "_exit";
}

std::string GetYouArriveTextId(Notification const & notification)
{
  if (!notification.IsPedestrianNotification() &&
      notification.m_turnDir != CarDirection::ReachedYourDestination)
  {
    ASSERT(false, ());
    return {};
  }

  if (notification.IsPedestrianNotification() &&
      notification.m_turnDirPedestrian != PedestrianDirection::ReachedYourDestination)
  {
    ASSERT(false, ());
    return {};
  }

  if (notification.m_distanceUnits != 0 || notification.m_useThenInsteadOfDistance)
    return "destination";
  return "you_have_reached_the_destination";
}

std::string GetDirectionTextId(Notification const & notification)
{
  if (notification.IsPedestrianNotification())
  {
    switch (notification.m_turnDirPedestrian)
    {
    case PedestrianDirection::GoStraight: return "go_straight";
    case PedestrianDirection::TurnRight: return "make_a_right_turn";
    case PedestrianDirection::TurnLeft: return "make_a_left_turn";
    case PedestrianDirection::ReachedYourDestination: return GetYouArriveTextId(notification);
    case PedestrianDirection::None:
    case PedestrianDirection::Count: ASSERT(false, (notification)); return {};
    }
  }

  switch (notification.m_turnDir)
  {
    case CarDirection::GoStraight:
      return "go_straight";
    case CarDirection::TurnRight:
      return "make_a_right_turn";
    case CarDirection::TurnSharpRight:
      return "make_a_sharp_right_turn";
    case CarDirection::TurnSlightRight:
      return "make_a_slight_right_turn";
    case CarDirection::TurnLeft:
      return "make_a_left_turn";
    case CarDirection::TurnSharpLeft:
      return "make_a_sharp_left_turn";
    case CarDirection::TurnSlightLeft:
      return "make_a_slight_left_turn";
    case CarDirection::UTurnLeft:
    case CarDirection::UTurnRight:
      return "make_a_u_turn";
    case CarDirection::EnterRoundAbout:
      return "enter_the_roundabout";
    case CarDirection::LeaveRoundAbout:
      return GetRoundaboutTextId(notification);
    case CarDirection::ReachedYourDestination:
      return GetYouArriveTextId(notification);
    case CarDirection::ExitHighwayToLeft:
    case CarDirection::ExitHighwayToRight:
      return "exit";
    case CarDirection::StayOnRoundAbout:
    case CarDirection::StartAtEndOfStreet:
    case CarDirection::None:
    case CarDirection::Count:
      ASSERT(false, ());
      return {};
  }
  ASSERT(false, ());
  return {};
}
}  // namespace routing::turns::sound
