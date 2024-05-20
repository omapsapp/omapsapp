#pragma once

#include <string>

namespace generator
{
class TurnLanesMetadataProcessor
{
public:
  static std::string ValidateAndFormat(std::string value);
};
}  // namespace generator
