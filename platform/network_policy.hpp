#pragma once

#include <functional>

struct _JNIEnv;
class _jobject;
typedef _JNIEnv JNIEnv;
typedef _jobject * jobject;

namespace platform
{
/// Class that is used to allow or disallow remote calls.
class NetworkPolicy
{
  // Maker for android.
  friend NetworkPolicy ToNativeNetworkPolicy(JNIEnv * env, jobject obj);

  friend NetworkPolicy GetCurrentNetworkPolicy();

public:
  bool CanUse() const { return m_canUse; }

private:
  explicit NetworkPolicy(bool canUse) : m_canUse(canUse) {}

  bool m_canUse = false;
};
  
extern NetworkPolicy GetCurrentNetworkPolicy();
}  // namespace platform
