#pragma once

#include <jni.h>

#include "platform/platform.hpp"

#include <memory>
#include <string>

namespace base
{
class TaskLoop;
}

namespace android
{
class Platform : public ::Platform
{
public:
  void Initialize(JNIEnv * env, jobject functorProcessObject, jstring apkPath, jstring writablePath,
                  jstring privatePath, jstring tmpPath, jstring flavorName,
                  jstring buildType, bool isTablet);

  ~Platform() override;

  void OnExternalStorageStatusChanged(bool isAvailable);

  void SetWritableDir(std::string const & dir);
  void SetSettingsDir(std::string const & dir);

  bool HasAvailableSpaceForWriting(uint64_t size) const;

  void SetGuiThread(std::unique_ptr<base::TaskLoop> guiThread);

  class AndroidSecureStorage
  {
  public:
    void Save(std::string const & key, std::string const & value);
    bool Load(std::string const & key, std::string & value);
    void Remove(std::string const & key);

  private:
    void Init(JNIEnv * env);

    jclass m_secureStorageClass = nullptr;
  };

  AndroidSecureStorage & GetSecureStorage() { return m_secureStorage; }

  jobject GetContext() { return m_functorProcessObject; }

  static Platform & Instance();

private:
  jobject m_functorProcessObject = nullptr;
  AndroidSecureStorage m_secureStorage;
};
} // namespace android
