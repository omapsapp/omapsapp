#pragma once

#include <ctime>

namespace base
{
/// A cross-platform replacenemt of gmtime_r
std::tm GmTime(time_t const time);
}  // namespace base
