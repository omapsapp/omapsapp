#pragma once

#include "platform/get_text_by_id.hpp"

#include "base/string_utils.hpp"

#include <memory>
#include <string>

namespace routing
{
namespace turns
{
namespace sound
{
struct Notification;

/// GetTtsText is responsible for generating text message for TTS in a specified locale
/// by notification. To get this message use operator().
/// If the message is not available for specified locale GetTtsText tries to find it in
/// English locale.
class GetTtsText
{
public:

  std::string GetTurnNotification(Notification const & notification) const;

  std::string GetRecalculatingNotification() const;

  std::string GetSpeedCameraNotification() const;

  /// \brief Sets a locale.
  /// @param locale is a string representation of locale. For example "en", "ru", "zh-Hant" and so on.
  /// \note See sound/tts/languages.txt for the full list of available locales.
  void SetLocale(std::string const & locale);

  /// @return current TTS locale. For example "en", "ru", "zh-Hant" and so on.
  /// \note The method returns correct locale after SetLocale has been called.
  /// If not, it returns an empty string.
  std::string GetLocale() const;

  void ForTestingSetLocaleWithJson(std::string const & jsonBuffer, std::string const & locale);

private:
  std::string GetTextById(std::string const & textId) const;
  std::string GetTextByIdTrimmed(std::string const & textId) const;

  std::unique_ptr<platform::GetTextById> m_getCurLang;

  /// \brief Removes a terminal period (or CJK or Hindi equivalent) from a string
  /// @param s - String to be modified
  static void RemoveLastDot(std::string & s)
  {
    strings::EatSuffix(s, ".");
    strings::EatSuffix(s, "。");
    strings::EatSuffix(s, "।");
  }
};
/// Generates text message id about the distance of the notification. For example: In 300 meters.
std::string GetDistanceTextId(Notification const & notification);
/// Generates text message id for roundabouts.
/// For example: leave_the_roundabout or take_the_3_exit
std::string GetRoundaboutTextId(Notification const & notification);
/// Generates text message id for the finish of the route.
/// For example: destination or you_have_reached_the_destination
std::string GetYouArriveTextId(Notification const & notification);
/// Generates text message id about the direction of the notification. For example: Make a right
/// turn.
std::string GetDirectionTextId(Notification const & notification);
}  // namespace sound
}  // namespace turns
}  // namespace routing
