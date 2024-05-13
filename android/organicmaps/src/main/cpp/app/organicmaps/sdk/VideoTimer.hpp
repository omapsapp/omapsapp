#pragma once

#include <jni.h>

#include "platform/video_timer.hpp"

namespace android
{
  class VideoTimer : public ::VideoTimer
  {
  private:
    jobject m_videoTimer;

  public:

    explicit VideoTimer(TFrameFn frameFn);
    ~VideoTimer();

    void SetParentObject(jobject videoTimer);

    void start();

    void stop();

    void resume();

    void pause();

    void perform();
  };
}

extern android::VideoTimer * g_timer;
