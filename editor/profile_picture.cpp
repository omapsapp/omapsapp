#include "editor/profile_picture.hpp"

#include "platform/remote_file.hpp"
#include "platform/platform.hpp"
#include "platform/settings.hpp"

#include "filesystem"

// Example pic_url: https://www.openstreetmap.org/rails/active_storage/representations/redirect/eyJfcmFpbHMiOnsiZGF0YSI6MzM1MjcxMDUsInB1ciI6ImJsb2JfaWQifX0=--957a2a99111c97d4b9f4181003c3e69713a221c6/eyJfcmFpbHMiOnsiZGF0YSI6eyJmb3JtYXQiOiJwbmciLCJyZXNpemVfdG9fbGltaXQiOlsxMDAsMTAwXX0sInB1ciI6InZhcmlhdGlvbiJ9fQ==--473f0190a9e741bb16565db85fe650d7b0a9ee69/cat.png
// Example hash: 473f0190a9e741bb16565db85fe650d7b0a9ee69
namespace editor
{
  std::string ProfilePicture::download(const std::string& pic_url)
  {
    std::string image_path = GetPlatform().WritablePathForFile(PROFILE_PICTURE_FILENAME);
    bool image_exists = std::filesystem::exists(image_path);

    if(!pic_url.empty()) // If internet available
    {
      if(pic_url != "none")
      {
        // Get new hash (hash start always includes '=--' but is usually '==--')
        const int hash_start = static_cast<int>(pic_url.rfind("=--") + 3);
        const int length = static_cast<int>(pic_url.rfind('/') - hash_start);
        std::string new_hash = pic_url.substr(hash_start, length);

        // Get cached hash
        std::string current_hash;
        settings::StringStorage::Instance().GetValue("ProfilePictureHash", current_hash);

        // Download new image
        if (new_hash != current_hash || !image_exists)
        {
          settings::StringStorage::Instance().SetValue("ProfilePictureHash", std::move(new_hash));
          platform::RemoteFile remoteFile(pic_url);
          remoteFile.Download(image_path);
          image_exists = true;
        }
      }
      else
      {
        // User has removed profile picture online
        settings::StringStorage::Instance().DeleteKeyAndValue("ProfilePictureHash");
        std::filesystem::remove(image_path);
        image_exists = false;
      }
    }

    if(image_exists)
      return image_path;
    else
      return {};
  }
}