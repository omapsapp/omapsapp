#include "platform/secure_storage.hpp"

#include "AndroidPlatform.hpp"

namespace platform
{
void SecureStorage::Save(std::string const & key, std::string const & value)
{
  android::Platform::Instance().GetSecureStorage().Save(key, value);
}

bool SecureStorage::Load(std::string const & key, std::string & value)
{
  return android::Platform::Instance().GetSecureStorage().Load(key, value);
}

void SecureStorage::Remove(std::string const & key)
{
  android::Platform::Instance().GetSecureStorage().Remove(key);
}
}  // namespace platform
