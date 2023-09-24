#include "routing/turns_sound_settings.hpp"
#include "routing/turns_tts_text.hpp"

#include "base/string_utils.hpp"

#include <algorithm>
#include <iterator>
#include <string>
#include <regex>

namespace routing
{
namespace turns
{
namespace sound
{

namespace
{
using namespace routing::turns::sound;

template <class TIter> std::string DistToTextId(TIter begin, TIter end, uint32_t dist)
{
  using TValue = typename std::iterator_traits<TIter>::value_type;

  TIter distToSound = lower_bound(begin, end, dist, [](TValue const & p1, uint32_t p2)
                      {
                        return p1.first < p2;
                      });
  if (distToSound == end)
  {
    ASSERT(false, ("notification.m_distanceUnits is not correct."));
    return std::string{};
  }
  return distToSound->second;
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

std::string GetTtsText::GetTurnNotification(Notification const & notification) const
{
  std::string dirKey = GetDirectionTextId(notification);
  std::string dirStr = GetTextById(dirKey);

  if (notification.m_distanceUnits == 0 && !notification.m_useThenInsteadOfDistance && notification.m_nextStreet.empty())
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
    thenStr = GetTextById("then") + " "; // add space only if needed

  std::string distStr;
  if (notification.m_distanceUnits > 0)
    distStr = GetTextById(GetDistanceTextId(notification));

  if (!notification.m_nextStreet.empty()) {
    // We're going to pronounce the street name.

    // First, let's get rid of unpronounceable symbols.
    std::string streetOut = notification.m_nextStreet;
    // Semicolons are between destinations
    // and pronounced more like commas.
    std::replace( streetOut.begin(), streetOut.end(), ';', ',');
    // Open brackets have no pronunciation analogue
    std::replace( streetOut.begin(), streetOut.end(), '[', ' ');
    // Closed brackets end a highway and introduce the rest
    std::replace( streetOut.begin(), streetOut.end(), ']', ':');
    // An angle bracket is currently used to represent "to" a place
    // Ideally we'd add a translation for "to" or create full format
    // strings for better i18n support, like "turn onto %s toward %s"
    std::replace( streetOut.begin(), streetOut.end(), '>', ',');

    // Replace any full-stop characters to make TTS flow better.
    // Full stops are: . (Period) or 。 (East Asian) or । (Hindi)
    strings::ReplaceLast(distStr, ".", "");
    strings::ReplaceLast(distStr, "。", "");
    strings::ReplaceLast(distStr, "।", "");

    // If the turn direction with the key +_street exists for this locale, use it (like make_a_right_turn_street)
    std::string dirStreetStr = GetTextById(dirKey+"_street");
    if (!dirStreetStr.empty())
      dirStr = std::move(dirStreetStr);

    // Normally use "onto" for "turn right onto Main St"
    std::string ontoStr = GetTextById("onto");

    // If the nextStreet begins with [123]: we'll announce it as an exit number
    std::regex re("^\\[.+\\]:");
    std::smatch m;
    if (std::regex_search(notification.m_nextStreet, m, re) && m.size() > 0) {
      // Try to get a specific "take exit #" phrase and its associated "onto" phrase (if any)
      std::string dirExitStr = GetTextById("take_exit_number");
      if (!dirExitStr.empty()) {
        dirStr = std::move(dirExitStr);
        ontoStr = ""; // take_exit_number overwrites "onto"

        /* // TODO: currently unused, consider removing
        // If 'onto_exit_number' exists for this locale, use it instead of "onto"
        // (for english, null is good: "take exit 543" instead of "take exit onto 543")
        std::string ontoExitStr = GetTextById("onto_exit_number");
        if (ontoExitStr == ".") {
          ontoStr = ""; // period means empty
        } else if (!ontoExitStr.empty()) {
          ontoStr = std::move(ontoExitStr);
        }
        */
      }
    }

    strings::ReplaceLast(dirStr, ".", "");
    strings::ReplaceLast(dirStr, "。", "");
    strings::ReplaceLast(dirStr, "।", "");

    std::string distDirOntoStreetStr = GetTextById("dist_direction_onto_street");
    // TODO: we may want to only load _street_verb if _street exists; may also need to handle
    //   a lack of a $5 position in the formatter string
    std::string dirVerb = GetTextById(dirKey+"_street_verb");

    if (GetLocale() == "hu") {
      uint8_t hungarianism = CategorizeHungarianLastWordVowels(streetOut);

      if (hungarianism == 1) {
        //strings::ReplaceLast(distDirOntoStreetStr, "-re", "-re"); // leave as-is
      } else if (hungarianism == 2) {
        strings::ReplaceLast(distDirOntoStreetStr, "-re", "-ra");
      } else {
        strings::ReplaceLast(distDirOntoStreetStr, "-re", ""); // clear it
      }

      // if the first pronounceable character of the street is a vowel, use "az" instead of "a"
      // 1, 5, and 1000 start with vowels but not 10 or 100
      std::regex reg("^[ \\[]*[5aeiouáéíóúöüőű]|1[^\\d]|1\\d\\d\\d[^\\d]", std::regex_constants::icase);
      std::smatch ma;
      if (std::regex_search(streetOut, ma, reg) && ma.size() > 0) {
        if (ontoStr == "a")
          ontoStr = "az";
        if (dirStr == "Kilépés a")
          dirStr = "Kilépés az";
      }
    }

    char ttsOut[1024];
    snprintf(ttsOut, 1024,
      distDirOntoStreetStr.c_str(),
      distStr.c_str(), // in 100 feet
      dirStr.c_str(), // turn right / take exit
      ontoStr.c_str(), // onto / null
      streetOut.c_str(), // Main Street / 543:: M4: Queens Parkway, London
      dirVerb.c_str() // (optional "turn right" verb)
    );

    // remove floating punctuation
    std::string cleanOut = std::regex_replace(ttsOut, std::regex(" [,\\.:]+ "), " ");
    // remove repetitious spaces or colons
    cleanOut = std::regex_replace(cleanOut, std::regex("[ :]{2,99}"), " ");
    // trim leading spaces
    cleanOut = std::regex_replace(cleanOut, std::regex("^ +"), "");

    LOG(LINFO, ("TTSn", thenStr + cleanOut));

    return thenStr + cleanOut;
  }

  LOG(LINFO, ("TTS", thenStr + distStr + " " + dirStr));
  return thenStr + distStr + " " + dirStr;
}

bool GetTtsText::FindInStrArray(std::vector<std::string>& haystack, std::string needle) const
{
  auto it = std::find(
    std::begin(haystack),
    std::end(haystack),
    needle
  );

  return it != std::end(haystack);
}

uint8_t GetTtsText::CategorizeHungarianAcronymsAndNumbers(std::string const & myString) const
{
  std::vector<std::string> backNames = {
    "A", // a
    "Á", // á
    "H", // há
    "I", // i
    "Í", // í
    "K", // ká
    "O", // o
    "Ó", // ó
    "U", // u
    "Ű", // ú
    "0", // nulla or zéró
    "3", // három
    "6", // hat
    "8", // nyolc
    "10", // tíz
  };

  std::vector<std::string> frontNames = {
    // all other letters besides H and K
    "B", "C", "D", "E", "É", "F", "G", "J", "L", "M", "N", "Ö", "Ő", "P", "Q", "R", "S", "T", "Ú", "Ü", "V", "W", "X", "Y", "Z",
    "1", // egy
    "2", // kettő
    "4", // négy
    "5", // öt
    "7", // hét
    "9", // kilenc
  };

  std::vector<std::string> specialCaseFront = {
    "10", // tíz special case front
    "40", // negyven front
    "50", // ötven front
    "70", // hetven front
    "90", // kilencven front
  };

  std::vector<std::string> specialCaseBack = {
    "20", // húsz back
    "30", // harminc back
    "60", // hatvan back
    "80", // nyolcvan back
  };

  //'100', // száz back

  for (std::string::size_type i = myString.size()-1; i > 0; i--) {

    // special is 2 char, so check last 2
    std::string twoBuf = "";
    twoBuf.append(1,myString[i-1]);
    twoBuf.append(1,myString[i]);
    if (FindInStrArray(specialCaseFront, twoBuf)) {
      return 1;
    }
    if (FindInStrArray(specialCaseBack, twoBuf)) {
      return 2;
    }
    std::string threeBuf = "";
    threeBuf.append(1,myString[i-2]);
    threeBuf.append(1,myString[i-1]);
    threeBuf.append(1,myString[i]);
    if (threeBuf == "100") {
      return 2;
    }

    std::string oneBuf = "";
    oneBuf.append(1,myString[i]);
    if (FindInStrArray(frontNames,oneBuf)) {
      return 1;
    }
    if (FindInStrArray(backNames,oneBuf)) {
      return 2;
    }
    if (myString[i] == ' ') {
      // if we've somehow hit a space, just say it's back
      LOG(LERROR, ("Unable to find front/back for",myString));
      return 2;
    }
  }

  LOG(LERROR, ("Unable to find front/back for",myString));
  return 2;
}

/*
 * @return 1 = front = -re, 2 = back = -ra
 */
uint8_t GetTtsText::CategorizeHungarianLastWordVowels(std::string const & myString) const
{
  std::vector<std::string> front = {"e","é","ö","ő","ü","ű"};
  std::vector<std::string> back = {"a","á","o","ó","u","ú"};
  std::vector<std::string> indeterminate = {"i","í"};

  bool allUppercaseNum = true;
  
  // scan for acronyms first
  for (std::string::size_type i = myString.size()-1; i > 0; i--) {
    if (myString[i] == ' ') {
      break;
    }
    if (myString[i] == std::tolower(myString[i])) {
      allUppercaseNum = false;
      break;
    }
  }
  
  // if the last word is an acronym/number like M5, check those instead
  if (allUppercaseNum) {
    return CategorizeHungarianAcronymsAndNumbers(myString);
  }

  bool foundIndeterminate = false;

  // find last vowel in last word, since it discriminates in all cases
  for (std::string::size_type i = myString.size()-1; i > 0; i--) {
    std::string lowerC = "";
    lowerC.append(1,std::tolower(myString[i]));
    if (FindInStrArray(front,lowerC)) {
      return 1;
    }
    if (FindInStrArray(back,lowerC)) {
      return 2;
    }
    if (FindInStrArray(indeterminate,lowerC)) {
      foundIndeterminate = true;
    }
    if (myString[i] == ' ' && foundIndeterminate == true) {
      // if we've hit a space with only indeterminates, it's back
      return 2;
    }
    if (myString[i] == ' ' && foundIndeterminate == false) {
      // if we've hit a space with no vowels at all, check for numbers
      // and acronyms
      return CategorizeHungarianAcronymsAndNumbers(myString);
    }
  }
  return 2; // default
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
    return std::string{};
  }
  if (!notification.m_useThenInsteadOfDistance)
    return "leave_the_roundabout"; // Notification just before leaving a roundabout.

  static const uint8_t kMaxSoundedExit = 11;
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
    return std::string{};
  }

  if (notification.IsPedestrianNotification() &&
      notification.m_turnDirPedestrian != PedestrianDirection::ReachedYourDestination)
  {
    ASSERT(false, ());
    return std::string{};
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
    case PedestrianDirection::Count: ASSERT(false, (notification)); return std::string{};
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
      return std::string{};
  }
  ASSERT(false, ());
  return std::string{};
}
}  // namespace sound
}  // namespace turns
}  // namespace routing
