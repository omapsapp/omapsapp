#pragma once

#include "base/buffer_vector.hpp"

#include <string>

namespace languages
{
/// @note These functions are heavy enough to call them often. Be careful.

/// @return List of original system languages in the form "en-US|ru-RU|es|zh-Hant".
std::string GetPreferred();

/// @return Original language code for the current user in the form "en-US", "zh-Hant".
std::string GetCurrentOrig();

/// @return Current language in out Twine translations compatible format, e.g. "en", "pt" or "zh-Hant".
std::string GetCurrentTwine();
std::string GetCurrentMapTwine();

/// @return Normalized language code for the current user in the form "en", "zh".
/// Returned languages are normalized to our supported languages in the core, see
/// string_utf8_multilang.cpp and should not be used with any sub-locales like zh-Hans/zh-Hant. Some
/// langs like Danish (da) are not supported in the core too, but used as a locale.
std::string Normalize(std::string_view lang);
std::string GetCurrentNorm();
std::string GetCurrentMapLanguage();

buffer_vector<std::string, 4> const & GetSystemPreferred();
}  // namespace languages
