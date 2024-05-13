#include "VideoTimer.hpp"

#include "app/organicmaps/core/jni_helper.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"


android::VideoTimer * g_timer = 0;

namespace android
{
  VideoTimer::VideoTimer(TFrameFn frameFn)
    : ::VideoTimer(frameFn)
  {
    ASSERT(g_timer == 0, ());
    g_timer = this;
  }

  VideoTimer::~VideoTimer()
  {
    stop();
    g_timer = 0;
  }

  void VideoTimer::SetParentObject(jobject videoTimer)
  {
    m_videoTimer = videoTimer;
  }

  void VideoTimer::start()
  {
    /*JNIEnv * env;
    m_javaVM->AttachCurrentThread(&env, NULL);
    env->CallVoidMethod(m_videoTimer, jni::GetJavaMethodID(env, m_videoTimer, "start", "()V"));*/
    m_state = ERunning;
  }

  void VideoTimer::resume()
  {
    /*JNIEnv * env;
    m_javaVM->AttachCurrentThread(&env, NULL);
    env->CallVoidMethod(m_videoTimer, jni::GetJavaMethodID(env, m_videoTimer, "resume", "()V"));*/
    m_state = ERunning;
  }

  void VideoTimer::pause()
  {
    /*JNIEnv * env;
    m_javaVM->AttachCurrentThread(&env, NULL);
    env->CallVoidMethod(m_videoTimer, jni::GetJavaMethodID(env, m_videoTimer, "pause", "()V"));*/
    m_state = EPaused;
  }

  void VideoTimer::stop()
  {
    /*JNIEnv * env;
    m_javaVM->AttachCurrentThread(&env, NULL);
    env->CallVoidMethod(m_videoTimer, jni::GetJavaMethodID(env, m_videoTimer, "stop", "()V"));*/
    m_state = EStopped;
  }

  void VideoTimer::perform()
  {
    //m_frameFn();
  }
}

extern "C"
{
  JNIEXPORT void JNICALL
  Java_app_organicmaps_VideoTimer_nativeRun(JNIEnv * env, jobject thiz)
  {
    ASSERT ( g_timer, ());
    g_timer->perform();
  }

  JNIEXPORT void JNICALL
  Java_app_organicmaps_VideoTimer_nativeInit(JNIEnv * env, jobject thiz)
  {
    ASSERT ( g_timer, ());
    g_timer->SetParentObject(thiz);
  }
}
