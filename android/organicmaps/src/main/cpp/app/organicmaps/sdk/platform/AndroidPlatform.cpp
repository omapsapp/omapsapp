#include "app/organicmaps/platform/AndroidPlatform.hpp"
#include "app/organicmaps/platform/GuiThread.hpp"

#include "app/organicmaps/core/jni_helper.hpp"

#include "app/organicmaps/util/NetworkPolicy.hpp"

#include "platform/network_policy.hpp"
#include "platform/settings.hpp"

#include "base/logging.hpp"
#include "base/macros.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <memory>
#include <utility>

std::string Platform::GetMemoryInfo() const
{
  JNIEnv * env = jni::GetEnv();
  if (env == nullptr)
    return std::string();

  static std::shared_ptr<jobject> classLogsManager = jni::make_global_ref(env->FindClass("app/organicmaps/util/log/LogsManager"));
  ASSERT(classLogsManager, ());

  jobject context = android::Platform::Instance().GetContext();
  static jmethodID const getMemoryInfoId
    = jni::GetStaticMethodID(env,
                             static_cast<jclass>(*classLogsManager),
                             "getMemoryInfo",
                             "(Landroid/content/Context;)Ljava/lang/String;");
  jstring const memInfoString = static_cast<jstring>(env->CallStaticObjectMethod(
    static_cast<jclass>(*classLogsManager), getMemoryInfoId, context));
  ASSERT(memInfoString, ());

  return jni::ToNativeString(env, memInfoString);
}

std::string Platform::DeviceName() const
{
  JNIEnv * env = jni::GetEnv();
  static jmethodID const getDeviceNameId = jni::GetStaticMethodID(env, g_utilsClazz, "getDeviceName",
                                                                  "()Ljava/lang/String;");
  auto const deviceName = static_cast<jstring>(env->CallStaticObjectMethod(g_utilsClazz,
                                                                           getDeviceNameId));
  return jni::ToNativeString(env, deviceName);
}

std::string Platform::DeviceModel() const
{
  JNIEnv * env = jni::GetEnv();
  static jmethodID const getDeviceModelId = jni::GetStaticMethodID(env, g_utilsClazz, "getDeviceModel",
                                                                  "()Ljava/lang/String;");
  auto const deviceModel = static_cast<jstring>(env->CallStaticObjectMethod(g_utilsClazz,
                                                                            getDeviceModelId));
  return jni::ToNativeString(env, deviceModel);
}

std::string Platform::Version() const
{
  JNIEnv * env = jni::GetEnv();
  static jmethodID const getVersionId = jni::GetStaticMethodID(env, g_utilsClazz, "getVersion", "()Ljava/lang/String;");
  auto const version = static_cast<jstring>(env->CallStaticObjectMethod(g_utilsClazz, getVersionId));
  return jni::ToNativeString(env, version);
}

int32_t Platform::IntVersion() const
{
  JNIEnv * env = jni::GetEnv();
  static jmethodID const getIntVersionId = jni::GetStaticMethodID(env, g_utilsClazz, "getIntVersion", "()I");
  return env->CallStaticIntMethod(g_utilsClazz, getIntVersionId);
}

Platform::EConnectionType Platform::ConnectionStatus()
{
  JNIEnv * env = jni::GetEnv();
  if (env == nullptr)
    return EConnectionType::CONNECTION_NONE;

  static std::shared_ptr<jobject> clazzConnectionState = jni::make_global_ref(env->FindClass("app/organicmaps/util/ConnectionState"));
  ASSERT(clazzConnectionState, ());

  static jmethodID const getConnectionMethodId = jni::GetStaticMethodID(env, static_cast<jclass>(*clazzConnectionState), "getConnectionState", "()B");
  return static_cast<Platform::EConnectionType>(env->CallStaticByteMethod(static_cast<jclass>(*clazzConnectionState), getConnectionMethodId));
}

Platform::ChargingStatus Platform::GetChargingStatus()
{
  JNIEnv * env = jni::GetEnv();
  if (env == nullptr)
    return Platform::ChargingStatus::Unknown;

  static jclass const clazzBatteryState =
      jni::GetGlobalClassRef(env, "app/organicmaps/util/BatteryState");
  ASSERT(clazzBatteryState, ());

  static jmethodID const getChargingMethodId =
      jni::GetStaticMethodID(env, clazzBatteryState, "getChargingStatus", "(Landroid/content/Context;)I");
  jobject context = android::Platform::Instance().GetContext();
  return static_cast<Platform::ChargingStatus>(
      env->CallStaticIntMethod(clazzBatteryState, getChargingMethodId, context));
}

uint8_t Platform::GetBatteryLevel()
{
  JNIEnv * env = jni::GetEnv();
  if (env == nullptr)
    return 100;

  static auto const clazzBatteryState =
      jni::GetGlobalClassRef(env, "app/organicmaps/util/BatteryState");
  ASSERT(clazzBatteryState, ());

  static auto const getLevelMethodId =
      jni::GetStaticMethodID(env, clazzBatteryState, "getLevel", "(Landroid/content/Context;)I");
  jobject context = android::Platform::Instance().GetContext();
  return static_cast<uint8_t>(env->CallStaticIntMethod(clazzBatteryState, getLevelMethodId, context));
}

namespace platform
{
platform::NetworkPolicy GetCurrentNetworkPolicy()
{
  JNIEnv *env = jni::GetEnv();
  return platform::NetworkPolicy(network_policy::GetCurrentNetworkUsageStatus(env));
}
}

namespace android
{
void Platform::Initialize(JNIEnv * env, jobject functorProcessObject, jstring apkPath,
                          jstring writablePath, jstring privatePath, jstring tmpPath,
                          jstring flavorName, jstring buildType, bool isTablet)
{
  m_functorProcessObject = env->NewGlobalRef(functorProcessObject);

  m_guiThread = std::make_unique<GuiThread>(m_functorProcessObject);

  std::string const flavor = jni::ToNativeString(env, flavorName);
  std::string const build = jni::ToNativeString(env, buildType);
  LOG(LINFO, ("Flavor name:", flavor));
  LOG(LINFO, ("Build type name:", build));

  m_isTablet = isTablet;
  m_resourcesDir = jni::ToNativeString(env, apkPath);
  m_tmpDir = jni::ToNativeString(env, tmpPath);
  SetWritableDir(jni::ToNativeString(env, writablePath));
  LOG(LINFO, ("Apk path = ", m_resourcesDir));
  LOG(LINFO, ("Temporary path = ", m_tmpDir));

  // IMPORTANT: This method SHOULD be called from UI thread to cache static jni ID-s inside.
  (void) ConnectionStatus();
}

Platform::~Platform()
{
  JNIEnv * env = jni::GetEnv();

  if (m_functorProcessObject)
    env->DeleteGlobalRef(m_functorProcessObject);
}

void Platform::OnExternalStorageStatusChanged(bool isAvailable)
{
}

void Platform::SetWritableDir(std::string const & dir)
{
  m_writableDir = dir;
  LOG(LINFO, ("Writable path = ", m_writableDir));
}

void Platform::SetSettingsDir(std::string const & dir)
{
  m_settingsDir = dir;
  // Logger is not fully initialized here.
  //LOG(LINFO, ("Settings path = ", m_settingsDir));
}

bool Platform::HasAvailableSpaceForWriting(uint64_t size) const
{
  return (GetWritableStorageStatus(size) == ::Platform::STORAGE_OK);
}

Platform & Platform::Instance()
{
  static Platform platform;
  return platform;
}

void Platform::AndroidSecureStorage::Init(JNIEnv * env)
{
  if (m_secureStorageClass != nullptr)
    return;

  m_secureStorageClass = jni::GetGlobalClassRef(env, "app/organicmaps/util/SecureStorage");
  ASSERT(m_secureStorageClass, ());
}

void Platform::AndroidSecureStorage::Save(std::string const & key, std::string const & value)
{
  JNIEnv * env = jni::GetEnv();
  if (env == nullptr)
    return;

  Init(env);

  static jmethodID const saveMethodId =
    jni::GetStaticMethodID(env, m_secureStorageClass, "save",
                           "(Landroid/content/Context;Ljava/lang/String;"
                           "Ljava/lang/String;)V");
  jobject context = android::Platform::Instance().GetContext();
  env->CallStaticVoidMethod(m_secureStorageClass, saveMethodId,
                            context,
                            jni::TScopedLocalRef(env, jni::ToJavaString(env, key)).get(),
                            jni::TScopedLocalRef(env, jni::ToJavaString(env, value)).get());
}

bool Platform::AndroidSecureStorage::Load(std::string const & key, std::string & value)
{
  JNIEnv * env = jni::GetEnv();
  if (env == nullptr)
    return false;

  Init(env);

  static jmethodID const loadMethodId =
    jni::GetStaticMethodID(env, m_secureStorageClass, "load",
                           "(Landroid/content/Context;Ljava/lang/String;)"
                           "Ljava/lang/String;");
  jobject context = android::Platform::Instance().GetContext();
  auto const resultString = static_cast<jstring>(env->CallStaticObjectMethod(m_secureStorageClass,
    loadMethodId, context,
    jni::TScopedLocalRef(env, jni::ToJavaString(env, key)).get()));
  if (resultString == nullptr)
    return false;

  value = jni::ToNativeString(env, resultString);
  return true;
}

void Platform::AndroidSecureStorage::Remove(std::string const & key)
{
  JNIEnv * env = jni::GetEnv();
  if (env == nullptr)
    return;

  Init(env);

  static jmethodID const removeMethodId =
    jni::GetStaticMethodID(env, m_secureStorageClass, "remove",
                           "(Landroid/content/Context;Ljava/lang/String;)V");
  jobject context = android::Platform::Instance().GetContext();
  env->CallStaticVoidMethod(m_secureStorageClass, removeMethodId, context,
                            jni::TScopedLocalRef(env, jni::ToJavaString(env, key)).get());
}
}  // namespace android

Platform & GetPlatform()
{
  return android::Platform::Instance();
}
