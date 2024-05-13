#include "app/organicmaps/Framework.hpp"
#include "app/organicmaps/core/jni_helper.hpp"

#include "search/displayed_categories.hpp"

extern "C"
{
JNIEXPORT jobjectArray JNICALL
Java_app_organicmaps_search_DisplayedCategories_nativeGetKeys(JNIEnv * env, jclass clazz)
{
  ::Framework * fr = g_framework->NativeFramework();
  ASSERT(fr, ());
  search::DisplayedCategories categories = fr->GetDisplayedCategories();
  return jni::ToJavaStringArray(env, categories.GetKeys());
}
}  // extern "C"
