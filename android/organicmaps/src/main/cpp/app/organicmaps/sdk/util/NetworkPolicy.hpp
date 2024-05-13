#pragma once

namespace network_policy
{
bool GetNetworkPolicyStatus(JNIEnv * env, jobject obj);
bool GetCurrentNetworkUsageStatus(JNIEnv * env);
}
