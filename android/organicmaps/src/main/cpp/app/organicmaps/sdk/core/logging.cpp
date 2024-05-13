#include "platform/platform.hpp"

#include "base/exception.hpp"
#include "base/logging.hpp"

#include "app/organicmaps/core/jni_helper.hpp"
#include "app/organicmaps/core/logging.hpp"
#include "app/organicmaps/core/ScopedEnv.hpp"

#include <android/log.h>
#include <cassert>
#include <cstdlib>

namespace jni
{

using namespace base;

void AndroidMessage(LogLevel level, SrcPoint const & src, std::string const & s)
{
  android_LogPriority pr = ANDROID_LOG_SILENT;

  switch (level)
  {
    case LINFO: pr = ANDROID_LOG_INFO; break;
    case LDEBUG: pr = ANDROID_LOG_DEBUG; break;
    case LWARNING: pr = ANDROID_LOG_WARN; break;
    case LERROR: pr = ANDROID_LOG_ERROR; break;
    case LCRITICAL: pr = ANDROID_LOG_ERROR; break;
    case NUM_LOG_LEVELS: break;
  }

  ScopedEnv env(jni::GetJVM());
  static jmethodID const logMethod = jni::GetStaticMethodID(env.get(), g_loggerClazz,
     "log", "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)V");

  std::string const out = DebugPrint(src) + s;
  jni::TScopedLocalRef msg(env.get(), jni::ToJavaString(env.get(), out));
  env->CallStaticVoidMethod(g_loggerClazz, logMethod, pr, NULL, msg.get(), NULL);
}

void AndroidLogMessage(LogLevel level, SrcPoint const & src, std::string const & s)
{
  AndroidMessage(level, src, s);
  CHECK_LESS(level, g_LogAbortLevel, ("Abort. Log level is too serious", level));
}

bool AndroidAssertMessage(SrcPoint const & src, std::string const & s)
{
  AndroidMessage(LCRITICAL, src, s);
  return true;
}

void InitSystemLog()
{
  SetLogMessageFn(&AndroidLogMessage);
}

void InitAssertLog()
{
  SetAssertFunction(&AndroidAssertMessage);
}

void ToggleDebugLogs(bool enabled)
{
  if (enabled)
    g_LogLevel = LDEBUG;
  else
    g_LogLevel = LINFO;
}
}
