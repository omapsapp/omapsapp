#include "app/organicmaps/core/jni_helper.hpp"

namespace network_policy
{
bool GetNetworkPolicyStatus(JNIEnv * env, jobject obj)
{
  static jmethodID const networkPolicyCanUseMethod =
      jni::GetMethodID(env, obj, "canUseNetwork", "()Z");
  return env->CallBooleanMethod(obj, networkPolicyCanUseMethod);
}

bool GetCurrentNetworkUsageStatus(JNIEnv * env)
{
  static jmethodID const method =
    jni::GetStaticMethodID(env, g_networkPolicyClazz, "getCurrentNetworkUsageStatus", "()Z");
  return env->CallStaticBooleanMethod(g_networkPolicyClazz, method);
}
}  // namespace network_policy
