#include "app/organicmaps/core/jni_helper.hpp"

#include "app/organicmaps/Framework.hpp"

#include "indexer/map_style.hpp"

extern "C"
{
JNIEXPORT void JNICALL Java_app_organicmaps_sdk_Router_nativeSet(JNIEnv *, jclass, jint routerType)
{
  using Type = routing::RouterType;
  Type type;
  switch (routerType)
  {
  case 0: type = Type::Vehicle; break;
  case 1: type = Type::Pedestrian; break;
  case 2: type = Type::Bicycle; break;
  case 3: type = Type::Transit; break;
  case 4: type = Type::Ruler; break;
  default: assert(false); break;
  }
  frm()->GetRoutingManager().SetRouter(type);
}

JNIEXPORT jint JNICALL Java_app_organicmaps_sdk_Router_nativeGet(JNIEnv *, jclass)
{
  return static_cast<jint>(frm()->GetRoutingManager().GetRouter());
}

JNIEXPORT jint JNICALL Java_app_organicmaps_sdk_Router_nativeGetLastUsed(JNIEnv *, jclass)
{
  return static_cast<jint>(frm()->GetRoutingManager().GetLastUsedRouter());
}

JNIEXPORT jint JNICALL Java_app_organicmaps_sdk_Router_nativeGetBest(JNIEnv *, jclass, jdouble srcLat, jdouble srcLon,
                                                                     jdouble dstLat, jdouble dstLon)
{
  return static_cast<jint>(frm()->GetRoutingManager().GetBestRouter(mercator::FromLatLon(srcLat, srcLon),
                                                                    mercator::FromLatLon(dstLat, dstLon)));
}
}
