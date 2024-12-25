#pragma once

#include <functional>
#include <string>
#include <vector>

namespace pl
{
void EnumerateFiles(std::string const & directory, std::function<void(char const *)> const & fn);

void EnumerateFilesByRegExp(std::string const & directory, std::string const & regexp,
                            std::vector<std::string> & res);

void SetMaxOpenFileLimit();
}  // namespace pl
