#include "app/organicmaps/Framework.hpp"
#include "app/organicmaps/UserMarkHelper.hpp"
#include "app/organicmaps/platform/AndroidPlatform.hpp"
#include "app/organicmaps/util/Distance.hpp"

#include "map/bookmarks_search_params.hpp"
#include "map/everywhere_search_params.hpp"
#include "map/place_page_info.hpp"
#include "map/viewport_search_params.hpp"

#include "search/mode.hpp"
#include "search/result.hpp"

#include "platform/network_policy.hpp"

#include "geometry/distance_on_sphere.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"

#include "defines.hpp"

#include <chrono>
#include <memory>
#include <vector>

using namespace std;
using namespace std::placeholders;
using search::Result;
using search::Results;

namespace
{
FeatureID const kEmptyFeatureId;

// This cache is needed only for showing a specific result on the map after click on the list item.
// Don't use it with another intentions!
Results g_results;

// Timestamp of last search query. Results with older stamps are ignored.
jlong g_queryTimestamp;
// Implements 'NativeSearchListener' java interface.
jobject g_javaListener;
jmethodID g_updateResultsId;
jmethodID g_endResultsId;
// Cached classes and methods to return results.
jclass g_resultClass;
jmethodID g_resultConstructor;
jmethodID g_suggestConstructor;
jclass g_descriptionClass;
jmethodID g_descriptionConstructor;
jclass g_popularityClass;
jmethodID g_popularityConstructor;

// Implements 'NativeMapSearchListener' java interface.
jmethodID g_mapResultsMethod;
jclass g_mapResultClass;
jmethodID g_mapResultCtor;

jmethodID g_updateBookmarksResultsId;
jmethodID g_endBookmarksResultsId;

bool PopularityHasHigherPriority(bool hasPosition, double distanceInMeters)
{
  return !hasPosition || distanceInMeters > search::Result::kPopularityHighPriorityMinDistance;
}

jobject ToJavaResult(Result const & result, search::ProductInfo const & productInfo,
                     bool hasPosition, double lat, double lon)
{
  JNIEnv * env = jni::GetEnv();

  jni::TScopedLocalIntArrayRef ranges(
      env, env->NewIntArray(static_cast<jsize>(result.GetHighlightRangesCount() * 2)));
  jint * rawArr = env->GetIntArrayElements(ranges, nullptr);
  for (size_t i = 0; i < result.GetHighlightRangesCount(); i++)
  {
    auto const & range = result.GetHighlightRange(i);
    rawArr[2 * i] = range.first;
    rawArr[2 * i + 1] = range.second;
  }
  env->ReleaseIntArrayElements(ranges.get(), rawArr, 0);

  jni::TScopedLocalIntArrayRef descRanges(env, env->NewIntArray(
          static_cast<jsize>(result.GetDescHighlightRangesCount() * 2)));
  jint * rawArr2 = env->GetIntArrayElements(descRanges, nullptr);
  for (size_t i = 0; i < result.GetDescHighlightRangesCount(); i++)
  {
    auto const & range = result.GetDescHighlightRange(i);
    rawArr2[2 * i] = range.first;
    rawArr2[2 * i + 1] = range.second;
  }
  env->ReleaseIntArrayElements(descRanges.get(), rawArr2, 0);

  ms::LatLon ll = ms::LatLon::Zero();
  if (result.HasPoint())
    ll = mercator::ToLatLon(result.GetFeatureCenter());

  if (result.IsSuggest())
  {
    jni::TScopedLocalRef name(env, jni::ToJavaString(env, result.GetString()));
    jni::TScopedLocalRef suggest(env, jni::ToJavaString(env, result.GetSuggestionString()));
    return env->NewObject(g_resultClass, g_suggestConstructor, name.get(), suggest.get(), ll.m_lat, ll.m_lon, ranges.get(),descRanges.get());
  }

  platform::Distance distance;
  double distanceInMeters = 0.0;
  if (result.HasPoint() && hasPosition)
  {
    distanceInMeters = ms::DistanceOnEarth(lat, lon, ll.m_lat, ll.m_lon);
    distance = platform::Distance::CreateFormatted(distanceInMeters);
  }

  bool const popularityHasHigherPriority = PopularityHasHigherPriority(hasPosition, distanceInMeters);
  bool const isFeature = result.GetResultType() == Result::Type::Feature;
  jni::TScopedLocalRef featureId(env, usermark_helper::CreateFeatureId(env, isFeature ?
                                                                            result.GetFeatureID() :
                                                                            kEmptyFeatureId));

  jni::TScopedLocalRef featureType(env, jni::ToJavaString(env, result.GetLocalizedFeatureType()));
  jni::TScopedLocalRef address(env, jni::ToJavaString(env, result.GetAddress()));
  jni::TScopedLocalRef dist(env, ToJavaDistance(env, distance));
  jni::TScopedLocalRef description(env, jni::ToJavaString(env, result.GetFeatureDescription()));

  jni::TScopedLocalRef desc(env, env->NewObject(g_descriptionClass, g_descriptionConstructor,
                                                featureId.get(), featureType.get(), address.get(),
                                                dist.get(), description.get(),
                                                static_cast<jint>(result.IsOpenNow()),
                                                result.GetMinutesUntilOpen(),result.GetMinutesUntilClosed(),
                                                static_cast<jboolean>(popularityHasHigherPriority)));

  jni::TScopedLocalRef name(env, jni::ToJavaString(env, result.GetString()));
  jni::TScopedLocalRef popularity(env, env->NewObject(g_popularityClass,
                                                      g_popularityConstructor,
                                                      /// @todo Restore when popularity will be available
                                                      0/*static_cast<jint>(result.GetRankingInfo().m_popularity)*/));

  return env->NewObject(g_resultClass, g_resultConstructor, name.get(), desc.get(), ll.m_lat, ll.m_lon,
                        ranges.get(), descRanges.get(), popularity.get());
}

jobjectArray BuildSearchResults(vector<search::ProductInfo> const & productInfo,
                                bool hasPosition, double lat, double lon)
{
  JNIEnv * env = jni::GetEnv();

  auto const count = static_cast<jsize>(g_results.GetCount());
  jobjectArray const jResults = env->NewObjectArray(count, g_resultClass, nullptr);
  for (jsize i = 0; i < count; i++)
  {
    jni::TScopedLocalRef jRes(env, ToJavaResult(g_results[i], productInfo[i], hasPosition, lat, lon));
    env->SetObjectArrayElement(jResults, i, jRes.get());
  }
  return jResults;
}

void OnResults(Results results, vector<search::ProductInfo> const & productInfo,
               jlong timestamp, bool isMapAndTable, bool hasPosition, double lat, double lon)
{
  // Ignore results from obsolete searches.
  if (g_queryTimestamp > timestamp)
    return;

  JNIEnv * env = jni::GetEnv();

  if (!results.IsEndMarker() || results.IsEndedNormal())
  {
    g_results = std::move(results);
    jni::TScopedLocalObjectArrayRef jResults(env, BuildSearchResults(productInfo, hasPosition, lat, lon));
    env->CallVoidMethod(g_javaListener, g_updateResultsId, jResults.get(), timestamp);
  }

  if (results.IsEndMarker())
  {
    env->CallVoidMethod(g_javaListener, g_endResultsId, timestamp);
    if (isMapAndTable && results.IsEndedNormal())
      g_framework->NativeFramework()->GetSearchAPI().PokeSearchInViewport();
  }
}

jobjectArray BuildJavaMapResults(vector<storage::DownloaderSearchResult> const & results)
{
  JNIEnv * env = jni::GetEnv();

  auto const count = static_cast<jsize>(results.size());
  jobjectArray const res = env->NewObjectArray(count, g_mapResultClass, nullptr);
  for (jsize i = 0; i < count; i++)
  {
    jni::TScopedLocalRef country(env, jni::ToJavaString(env, results[i].m_countryId));
    jni::TScopedLocalRef matched(env, jni::ToJavaString(env, results[i].m_matchedName));
    jni::TScopedLocalRef item(env, env->NewObject(g_mapResultClass, g_mapResultCtor, country.get(), matched.get()));
    env->SetObjectArrayElement(res, i, item.get());
  }

  return res;
}

void OnMapSearchResults(storage::DownloaderSearchResults const & results, long long timestamp)
{
  // Ignore results from obsolete searches.
  if (g_queryTimestamp > timestamp)
    return;

  JNIEnv * env = jni::GetEnv();
  jni::TScopedLocalObjectArrayRef jResults(env, BuildJavaMapResults(results.m_results));
  env->CallVoidMethod(g_javaListener, g_mapResultsMethod, jResults.get(),
                      static_cast<jlong>(timestamp), results.m_endMarker);
}

void OnBookmarksSearchResults(search::BookmarksSearchParams::Results results,
                              search::BookmarksSearchParams::Status status, long long timestamp)
{
  // Ignore results from obsolete searches.
  if (g_queryTimestamp > timestamp)
    return;

  JNIEnv * env = jni::GetEnv();

  g_framework->NativeFramework()->GetBookmarkManager().FilterInvalidBookmarks(results);
  jni::ScopedLocalRef<jlongArray> jResults(env, env->NewLongArray(static_cast<jsize>(results.size())));
  vector<jlong> const tmp(results.cbegin(), results.cend());
  env->SetLongArrayRegion(jResults.get(), 0, static_cast<jsize>(tmp.size()), tmp.data());

  auto const method = (status == search::BookmarksSearchParams::Status::InProgress) ?
                      g_updateBookmarksResultsId : g_endBookmarksResultsId;

  env->CallVoidMethod(g_javaListener, method, jResults.get(), static_cast<jlong>(timestamp));
}

}  // namespace

extern "C"
{
  JNIEXPORT void JNICALL
  Java_app_organicmaps_search_SearchEngine_nativeInit(JNIEnv * env, jobject thiz)
  {
    g_javaListener = env->NewGlobalRef(thiz);
    // public void onResultsUpdate(@NonNull SearchResult[] results, long timestamp)
    g_updateResultsId = jni::GetMethodID(env, g_javaListener, "onResultsUpdate",
                                         "([Lapp/organicmaps/search/SearchResult;J)V");
    // public void onResultsEnd(long timestamp)
    g_endResultsId = jni::GetMethodID(env, g_javaListener, "onResultsEnd", "(J)V");
    g_resultClass = jni::GetGlobalClassRef(env, "app/organicmaps/search/SearchResult");
    g_resultConstructor = jni::GetConstructorID(
        env, g_resultClass,
        "(Ljava/lang/String;Lapp/organicmaps/search/SearchResult$Description;DD[I[I"
          "Lapp/organicmaps/search/Popularity;)V");
    g_suggestConstructor = jni::GetConstructorID(env, g_resultClass, "(Ljava/lang/String;Ljava/lang/String;DD[I[I)V");
    g_descriptionClass = jni::GetGlobalClassRef(env, "app/organicmaps/search/SearchResult$Description");
    /*
        Description(FeatureId featureId, String featureType, String region, Distance distance,
                    String description, int openNow, int minutesUntilOpen, int minutesUntilClosed,
                    boolean hasPopularityHigherPriority)
    */
    g_descriptionConstructor = jni::GetConstructorID(env, g_descriptionClass,
                                                     "(Lapp/organicmaps/bookmarks/data/FeatureId;"
                                                     "Ljava/lang/String;Ljava/lang/String;Lapp/organicmaps/util/Distance;"
                                                     "Ljava/lang/String;IIIZ)V");

    g_popularityClass = jni::GetGlobalClassRef(env, "app/organicmaps/search/Popularity");
    g_popularityConstructor = jni::GetConstructorID(env, g_popularityClass, "(I)V");

    g_mapResultsMethod = jni::GetMethodID(env, g_javaListener, "onMapSearchResults",
                                          "([Lapp/organicmaps/search/NativeMapSearchListener$Result;JZ)V");
    g_mapResultClass = jni::GetGlobalClassRef(env, "app/organicmaps/search/NativeMapSearchListener$Result");
    g_mapResultCtor = jni::GetConstructorID(env, g_mapResultClass, "(Ljava/lang/String;Ljava/lang/String;)V");

    g_updateBookmarksResultsId =
      jni::GetMethodID(env, g_javaListener, "onBookmarkSearchResultsUpdate", "([JJ)V");
    g_endBookmarksResultsId =
      jni::GetMethodID(env, g_javaListener, "onBookmarkSearchResultsEnd", "([JJ)V");
  }

  JNIEXPORT jboolean JNICALL Java_app_organicmaps_search_SearchEngine_nativeRunSearch(
      JNIEnv * env, jclass clazz, jbyteArray bytes, jboolean isCategory,
      jstring lang, jlong timestamp, jboolean hasPosition, jdouble lat, jdouble lon)
  {
    search::EverywhereSearchParams params{
        jni::ToNativeString(env, bytes),
        jni::ToNativeString(env, lang),
        {},   // default timeout
        static_cast<bool>(isCategory),
        bind(&OnResults, _1, _2, timestamp, false, hasPosition, lat, lon)
    };
    bool const searchStarted = g_framework->NativeFramework()->GetSearchAPI().SearchEverywhere(std::move(params));
    if (searchStarted)
      g_queryTimestamp = timestamp;
    return searchStarted;
  }

  JNIEXPORT void JNICALL Java_app_organicmaps_search_SearchEngine_nativeRunInteractiveSearch(
      JNIEnv * env, jclass clazz, jbyteArray bytes, jboolean isCategory,
      jstring lang, jlong timestamp, jboolean isMapAndTable, jboolean hasPosition, jdouble lat, jdouble lon)
  {
    search::ViewportSearchParams vparams{
        jni::ToNativeString(env, bytes),
        jni::ToNativeString(env, lang),
        {},  // Default timeout
        static_cast<bool>(isCategory),
        {},  // Empty m_onStarted callback
        {},  // Empty m_onCompleted callback
    };

    // TODO (@alexzatsepin): set up vparams.m_onCompleted here and use
    // HotelsClassifier for hotel queries detection.
    // Don't move vparams here, because it's used below.
    g_framework->NativeFramework()->GetSearchAPI().SearchInViewport(vparams);

    if (isMapAndTable)
    {
      search::EverywhereSearchParams eparams{
          std::move(vparams.m_query),
          std::move(vparams.m_inputLocale),
          {},   // default timeout
          static_cast<bool>(isCategory),
          bind(&OnResults, _1, _2, timestamp, isMapAndTable, hasPosition, lat, lon)
      };

      if (g_framework->NativeFramework()->GetSearchAPI().SearchEverywhere(std::move(eparams)))
        g_queryTimestamp = timestamp;
    }
  }

  JNIEXPORT void JNICALL Java_app_organicmaps_search_SearchEngine_nativeRunSearchMaps(
      JNIEnv * env, jclass clazz, jbyteArray bytes, jstring lang, jlong timestamp)
  {
    storage::DownloaderSearchParams params{
        jni::ToNativeString(env, bytes),
        jni::ToNativeString(env, lang),
        bind(&OnMapSearchResults, _1, timestamp)
    };

    if (g_framework->NativeFramework()->GetSearchAPI().SearchInDownloader(std::move(params)))
      g_queryTimestamp = timestamp;
  }

  JNIEXPORT jboolean JNICALL Java_app_organicmaps_search_SearchEngine_nativeRunSearchInBookmarks(
      JNIEnv * env, jclass clazz, jbyteArray query, jlong catId, jlong timestamp)
  {
    search::BookmarksSearchParams params{
        jni::ToNativeString(env, query),
        static_cast<kml::MarkGroupId>(catId),
        bind(&OnBookmarksSearchResults, _1, _2, timestamp)
    };

    bool const searchStarted = g_framework->NativeFramework()->GetSearchAPI().SearchInBookmarks(std::move(params));
    if (searchStarted)
      g_queryTimestamp = timestamp;
    return searchStarted;
  }

  JNIEXPORT void JNICALL
  Java_app_organicmaps_search_SearchEngine_nativeShowResult(JNIEnv * env, jclass clazz, jint index)
  {
    g_framework->NativeFramework()->ShowSearchResult(g_results[index]);
  }

  JNIEXPORT void JNICALL
  Java_app_organicmaps_search_SearchEngine_nativeCancelInteractiveSearch(JNIEnv * env, jclass clazz)
  {
    g_framework->NativeFramework()->GetSearchAPI().CancelSearch(search::Mode::Viewport);
  }

  JNIEXPORT void JNICALL
  Java_app_organicmaps_search_SearchEngine_nativeCancelEverywhereSearch(JNIEnv * env, jclass clazz)
  {
    g_framework->NativeFramework()->GetSearchAPI().CancelSearch(search::Mode::Everywhere);
  }

  JNIEXPORT void JNICALL
  Java_app_organicmaps_search_SearchEngine_nativeCancelAllSearches(JNIEnv * env, jclass clazz)
  {
    g_framework->NativeFramework()->GetSearchAPI().CancelAllSearches();
  }
} // extern "C"
