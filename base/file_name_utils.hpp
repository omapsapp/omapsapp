#pragma once

#include <string>
#include <utility>

#include "base/assert.hpp"

namespace base
{
/// Remove extension from file name.
void GetNameWithoutExt(std::string & name);
std::string FilenameWithoutExt(std::string name);
/// @return File extension with the dot or empty std::string if no extension found.
std::string GetFileExtension(std::string const & name);

/// Get file name from full path.
void GetNameFromFullPath(std::string & name);

std::string FileNameFromFullPath(std::string path);

/// Get file name from full path without extension.
std::string GetNameFromFullPathWithoutExt(std::string path);

/// Returns all but last components of the path. After dropping the last
/// component, all trailing slashes are removed, unless the result is a
/// root directory. If the argument is a single component, returns ".".
std::string GetDirectory(std::string const & path);

/// Get native folder separator for the platform.
std::string::value_type GetNativeSeparator();

/// Add the terminating slash to the folder path std::string if it's not already there.
std::string AddSlashIfNeeded(std::string const & path);

namespace impl
{
inline std::string JoinPath(std::string const & file) { return file; }

template <typename... Args>
std::string JoinPath(std::string const & folder, Args &&... args)
{
  if (folder.empty())
  {
    if (sizeof...(args) == 0)
      return {};
    else
      return impl::JoinPath(std::forward<Args>(args)...);
  }

  return AddSlashIfNeeded(folder) + impl::JoinPath(std::forward<Args>(args)...);
}
}  // namespace impl

/// Create full path from some folder using native folders separator.
template <typename... Args>
std::string JoinPath(std::string const & dir, std::string const & fileOrDir, Args &&... args)
{
  return impl::JoinPath(dir, fileOrDir, std::forward<Args>(args)...);
}
}  // namespace base
