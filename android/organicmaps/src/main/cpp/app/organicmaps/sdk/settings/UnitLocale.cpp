#include "app/organicmaps/Framework.hpp"

#include "platform/measurement_utils.hpp"
#include "platform/settings.hpp"

extern "C"
{
  JNIEXPORT void JNICALL
  Java_app_organicmaps_settings_UnitLocale_setCurrentUnits(JNIEnv * env, jobject thiz, jint units)
  {
    measurement_utils::Units const u = static_cast<measurement_utils::Units>(units);
    settings::Set(settings::kMeasurementUnits, u);
    g_framework->SetupMeasurementSystem();
  }

  JNIEXPORT jint JNICALL
  Java_app_organicmaps_settings_UnitLocale_getCurrentUnits(JNIEnv * env, jobject thiz)
  {
    measurement_utils::Units u;
    return static_cast<jint>(
        settings::Get(settings::kMeasurementUnits, u) ? u : measurement_utils::Units::Metric);
  }
}
