#pragma once

#include <string>

namespace editor
{
class ProfilePicture
{
public:
    // Returns downloaded image or cached image if one exists.
    // if not, an empty string is returned.
    static std::string download(const std::string& pic_url);
};
}
