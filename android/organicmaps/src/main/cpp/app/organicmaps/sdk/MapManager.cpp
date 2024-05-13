#include "Framework.hpp"

#include "app/organicmaps/core/jni_helper.hpp"
#include "app/organicmaps/core/jni_java_methods.hpp"

#include "coding/internal/file_data.hpp"

#include "storage/country_info_getter.hpp"
#include "storage/storage.hpp"
#include "storage/storage_helpers.hpp"

#include "base/thread_checker.hpp"

#include "platform/downloader_defines.hpp"
#include "platform/downloader_utils.hpp"
#include "platform/local_country_file_utils.hpp"
#include "platform/mwm_version.hpp"

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>


namespace
{
// The last 5% are left for applying diffs.
float const kMaxProgress = 95.0f;
float const kMaxProgressWithoutDiffs = 100.0f;

enum ItemCategory : uint32_t
{
  NEAR_ME,
  DOWNLOADED,
  AVAILABLE,
};

struct TBatchedData
{
  storage::CountryId const m_countryId;
  storage::NodeStatus const m_newStatus;
  storage::NodeErrorCode const m_errorCode;
  bool const m_isLeaf;

  TBatchedData(storage::CountryId const & countryId, storage::NodeStatus const newStatus,
               storage::NodeErrorCode const errorCode, bool isLeaf)
    : m_countryId(countryId), m_newStatus(newStatus), m_errorCode(errorCode), m_isLeaf(isLeaf)
  {}
};

jobject g_countryChangedListener = nullptr;

DECLARE_THREAD_CHECKER(g_batchingThreadChecker);
std::unordered_map<jobject, std::vector<TBatchedData>> g_batchedCallbackData;
bool g_isBatched;

storage::Storage & GetStorage()
{
  ASSERT(g_framework != nullptr, ());
  return g_framework->GetStorage();
}

struct CountryItemBuilder
{
  jclass m_class;
  jmethodID m_ctor;
  jfieldID m_Id, m_Name, m_DirectParentId, m_TopmostParentId, m_DirectParentName, m_TopmostParentName,
          m_Description, m_Size, m_EnqueuedSize, m_TotalSize, m_ChildCount, m_TotalChildCount,
          m_Present, m_Progress, m_DownloadedBytes, m_BytesToDownload, m_Category, m_Status, m_ErrorCode;

  CountryItemBuilder(JNIEnv *env)
  {
    m_class = jni::GetGlobalClassRef(env, "app/organicmaps/downloader/CountryItem");
    m_ctor = jni::GetConstructorID(env, m_class, "(Ljava/lang/String;)V");

    m_Id = env->GetFieldID(m_class, "id", "Ljava/lang/String;");
    m_Name = env->GetFieldID(m_class, "name", "Ljava/lang/String;");
    m_DirectParentId = env->GetFieldID(m_class, "directParentId", "Ljava/lang/String;");
    m_TopmostParentId = env->GetFieldID(m_class, "topmostParentId", "Ljava/lang/String;");
    m_DirectParentName = env->GetFieldID(m_class, "directParentName", "Ljava/lang/String;");
    m_TopmostParentName = env->GetFieldID(m_class, "topmostParentName", "Ljava/lang/String;");
    m_Description = env->GetFieldID(m_class, "description", "Ljava/lang/String;");
    m_Size = env->GetFieldID(m_class, "size", "J");
    m_EnqueuedSize = env->GetFieldID(m_class, "enqueuedSize", "J");
    m_TotalSize = env->GetFieldID(m_class, "totalSize", "J");
    m_ChildCount = env->GetFieldID(m_class, "childCount", "I");
    m_TotalChildCount = env->GetFieldID(m_class, "totalChildCount", "I");
    m_Present = env->GetFieldID(m_class, "present", "Z");
    m_Progress = env->GetFieldID(m_class, "progress", "F");
    m_DownloadedBytes = env->GetFieldID(m_class, "downloadedBytes", "J");
    m_BytesToDownload = env->GetFieldID(m_class, "bytesToDownload", "J");
    m_Category = env->GetFieldID(m_class, "category", "I");
    m_Status = env->GetFieldID(m_class, "status", "I");
    m_ErrorCode = env->GetFieldID(m_class, "errorCode", "I");
  }

  DECLARE_BUILDER_INSTANCE(CountryItemBuilder);
  jobject Create(JNIEnv * env, jobject id) const
  {
    return env->NewObject(m_class, m_ctor, id);
  }
};

static storage::CountryId const GetRootId(JNIEnv * env, jstring root)
{
  return (root ? jni::ToNativeString(env, root) : GetStorage().GetRootId());
}
}  // namespace

extern "C"
{

// static String nativeGetRoot();
JNIEXPORT jstring JNICALL
Java_app_organicmaps_downloader_MapManager_nativeGetRoot(JNIEnv * env, jclass clazz)
{
  return jni::ToJavaString(env, GetStorage().GetRootId());
}

// static boolean nativeMoveFile(String oldFile, String newFile);
JNIEXPORT jboolean JNICALL
Java_app_organicmaps_downloader_MapManager_nativeMoveFile(JNIEnv * env, jclass clazz, jstring oldFile, jstring newFile)
{
  return base::MoveFileX(jni::ToNativeString(env, oldFile), jni::ToNativeString(env, newFile));
}

// static boolean nativeHasSpaceToDownloadAmount(long bytes);
JNIEXPORT jboolean JNICALL
Java_app_organicmaps_downloader_MapManager_nativeHasSpaceToDownloadAmount(JNIEnv * env, jclass clazz, jlong bytes)
{
  return storage::IsEnoughSpaceForDownload(bytes);
}

// static boolean nativeHasSpaceToDownloadCountry(String root);
JNIEXPORT jboolean JNICALL
Java_app_organicmaps_downloader_MapManager_nativeHasSpaceToDownloadCountry(JNIEnv * env, jclass clazz, jstring root)
{
  return storage::IsEnoughSpaceForDownload(jni::ToNativeString(env, root), GetStorage());
}

// static boolean nativeHasSpaceToUpdate(String root);
JNIEXPORT jboolean JNICALL
Java_app_organicmaps_downloader_MapManager_nativeHasSpaceToUpdate(JNIEnv * env, jclass clazz, jstring root)
{
  return IsEnoughSpaceForUpdate(jni::ToNativeString(env, root), GetStorage());
}

// static int nativeGetDownloadedCount();
JNIEXPORT jint JNICALL
Java_app_organicmaps_downloader_MapManager_nativeGetDownloadedCount(JNIEnv * env, jclass clazz)
{
  return static_cast<jint>(GetStorage().GetDownloadedFilesCount());
}

// static @Nullable UpdateInfo nativeGetUpdateInfo(@Nullable String root);
JNIEXPORT jobject JNICALL
Java_app_organicmaps_downloader_MapManager_nativeGetUpdateInfo(JNIEnv * env, jclass clazz, jstring root)
{
  storage::Storage::UpdateInfo info;
  if (!GetStorage().GetUpdateInfo(GetRootId(env, root), info))
    return nullptr;

  static jclass const infoClass = jni::GetGlobalClassRef(env, "app/organicmaps/downloader/UpdateInfo");
  ASSERT(infoClass, (jni::DescribeException()));
  static jmethodID const ctor = jni::GetConstructorID(env, infoClass, "(IJ)V");
  ASSERT(ctor, (jni::DescribeException()));

  return env->NewObject(infoClass, ctor, info.m_numberOfMwmFilesToUpdate, info.m_totalDownloadSizeInBytes);
}

static void UpdateItemShort(JNIEnv * env, jobject item, storage::NodeStatus const status, storage::NodeErrorCode const error)
{
  auto const & ciBuilder = CountryItemBuilder::Instance(env);

  env->SetIntField(item, ciBuilder.m_Status, static_cast<jint>(status));
  env->SetIntField(item, ciBuilder.m_ErrorCode, static_cast<jint>(error));
}

static void UpdateItem(JNIEnv * env, jobject item, storage::NodeAttrs const & attrs)
{
  auto const & ciBuilder = CountryItemBuilder::Instance(env);
  using SLR = jni::TScopedLocalRef;

  // Localized name
  env->SetObjectField(item, ciBuilder.m_Name, SLR(env, jni::ToJavaString(env, attrs.m_nodeLocalName)).get());

  // Direct parent[s]. Do not specify if there are multiple or none.
  if (attrs.m_parentInfo.size() == 1)
  {
    storage::CountryIdAndName const & info = attrs.m_parentInfo[0];
    env->SetObjectField(item, ciBuilder.m_DirectParentId, SLR(env, jni::ToJavaString(env, info.m_id)).get());
    env->SetObjectField(item, ciBuilder.m_DirectParentName, SLR(env, jni::ToJavaString(env, info.m_localName)).get());
  }
  else
  {
    env->SetObjectField(item, ciBuilder.m_DirectParentId, nullptr);
    env->SetObjectField(item, ciBuilder.m_DirectParentName, nullptr);
  }

  // Topmost parent[s]. Do not specify if there are multiple or none.
  if (attrs.m_topmostParentInfo.size() == 1)
  {
    storage::CountryIdAndName const & info = attrs.m_topmostParentInfo[0];
    env->SetObjectField(item, ciBuilder.m_TopmostParentId, SLR(env, jni::ToJavaString(env, info.m_id)).get());
    env->SetObjectField(item, ciBuilder.m_TopmostParentName, SLR(env, jni::ToJavaString(env, info.m_localName)).get());
  }
  else
  {
    env->SetObjectField(item, ciBuilder.m_TopmostParentId, nullptr);
    env->SetObjectField(item, ciBuilder.m_TopmostParentName, nullptr);
  }

  // Description
  env->SetObjectField(item, ciBuilder.m_Description, SLR(env, jni::ToJavaString(env, attrs.m_nodeLocalDescription)).get());

  // Sizes
  env->SetLongField(item, ciBuilder.m_Size, attrs.m_localMwmSize);
  env->SetLongField(item, ciBuilder.m_EnqueuedSize, attrs.m_downloadingMwmSize);
  env->SetLongField(item, ciBuilder.m_TotalSize, attrs.m_mwmSize);

  // Child counts
  env->SetIntField(item, ciBuilder.m_ChildCount, attrs.m_downloadingMwmCounter);
  env->SetIntField(item, ciBuilder.m_TotalChildCount, attrs.m_mwmCounter);

  // Status and error code
  UpdateItemShort(env, item, attrs.m_status, attrs.m_error);

  // Presence flag
  env->SetBooleanField(item, ciBuilder.m_Present, attrs.m_present);

  // Progress
  float percentage = 0;
  if (attrs.m_downloadingProgress.m_bytesTotal != 0)
  {
    auto const & progress = attrs.m_downloadingProgress;
    percentage = progress.m_bytesDownloaded * kMaxProgress / progress.m_bytesTotal;
  }

  env->SetFloatField(item, ciBuilder.m_Progress, percentage);
  env->SetLongField(item, ciBuilder.m_DownloadedBytes, attrs.m_downloadingProgress.m_bytesDownloaded);
  env->SetLongField(item, ciBuilder.m_BytesToDownload, attrs.m_downloadingProgress.m_bytesTotal);
}

static void PutItemsToList(
    JNIEnv * env, jobject const list, storage::CountriesVec const & children, int category,
    std::function<bool(storage::CountryId const & countryId, storage::NodeAttrs const & attrs)> const & predicate)
{
  auto const & ciBuilder = CountryItemBuilder::Instance(env);
  auto const listAddMethod = jni::ListBuilder::Instance(env).m_add;

  storage::NodeAttrs attrs;
  for (storage::CountryId const & child : children)
  {
    GetStorage().GetNodeAttrs(child, attrs);

    if (predicate && !predicate(child, attrs))
      continue;

    using SLR = jni::TScopedLocalRef;
    SLR const item(env, ciBuilder.Create(env, SLR(env, jni::ToJavaString(env, child))));
    env->SetIntField(item.get(), ciBuilder.m_Category, category);

    UpdateItem(env, item.get(), attrs);

    // Put to resulting list
    env->CallBooleanMethod(list, listAddMethod, item.get());
  }
}

// static void nativeListItems(@Nullable String root, double lat, double lon, boolean hasLocation, boolean myMapsMode, List<CountryItem> result);
JNIEXPORT void JNICALL
Java_app_organicmaps_downloader_MapManager_nativeListItems(JNIEnv * env, jclass clazz, jstring parent, jdouble lat, jdouble lon, jboolean hasLocation, jboolean myMapsMode, jobject result)
{
  if (hasLocation && !myMapsMode)
  {
    storage::CountriesVec near;
    g_framework->NativeFramework()->GetCountryInfoGetter().GetRegionsCountryId(mercator::FromLatLon(lat, lon), near);
    PutItemsToList(env, result, near, ItemCategory::NEAR_ME,
                   [](storage::CountryId const & countryId, storage::NodeAttrs const & attrs) -> bool {
                     return !attrs.m_present;
                   });
  }

  storage::CountriesVec downloaded, available;
  GetStorage().GetChildrenInGroups(GetRootId(env, parent), downloaded, available, true);

  if (myMapsMode)
    PutItemsToList(env, result, downloaded, ItemCategory::DOWNLOADED, nullptr);
  else
    PutItemsToList(env, result, available, ItemCategory::AVAILABLE, nullptr);
}

// static void nativeUpdateItem(CountryItem item);
JNIEXPORT void JNICALL
Java_app_organicmaps_downloader_MapManager_nativeGetAttributes(JNIEnv * env, jclass, jobject item)
{
  auto const & ciBuilder = CountryItemBuilder::Instance(env);
  jstring id = static_cast<jstring>(env->GetObjectField(item, ciBuilder.m_Id));

  storage::NodeAttrs attrs;
  GetStorage().GetNodeAttrs(jni::ToNativeString(env, id), attrs);

  UpdateItem(env, item, attrs);
}

// static void nativeGetStatus(String root);
JNIEXPORT jint JNICALL
Java_app_organicmaps_downloader_MapManager_nativeGetStatus(JNIEnv * env, jclass clazz, jstring root)
{
  storage::NodeStatuses ns;
  GetStorage().GetNodeStatuses(jni::ToNativeString(env, root), ns);
  return static_cast<jint>(ns.m_status);
}

// static void nativeGetError(String root);
JNIEXPORT jint JNICALL
Java_app_organicmaps_downloader_MapManager_nativeGetError(JNIEnv * env, jclass clazz, jstring root)
{
  storage::NodeStatuses ns;
  GetStorage().GetNodeStatuses(jni::ToNativeString(env, root), ns);
  return static_cast<jint>(ns.m_error);
}

// static String nativeGetName(String root);
JNIEXPORT jstring JNICALL
Java_app_organicmaps_downloader_MapManager_nativeGetName(JNIEnv * env, jclass clazz, jstring root)
{
  return jni::ToJavaString(env, GetStorage().GetNodeLocalName(jni::ToNativeString(env, root)));
}

// static @Nullable String nativeFindCountry(double lat, double lon);
JNIEXPORT jstring JNICALL
Java_app_organicmaps_downloader_MapManager_nativeFindCountry(JNIEnv * env, jclass clazz, jdouble lat, jdouble lon)
{
  return jni::ToJavaString(env, g_framework->NativeFramework()->GetCountryInfoGetter().GetRegionCountryId(mercator::FromLatLon(lat, lon)));
}

// static boolean nativeIsDownloading();
JNIEXPORT jboolean JNICALL
Java_app_organicmaps_downloader_MapManager_nativeIsDownloading(JNIEnv * env, jclass clazz)
{
  return static_cast<jboolean>(GetStorage().IsDownloadInProgress());
}

static void StartBatchingCallbacks()
{
  CHECK_THREAD_CHECKER(g_batchingThreadChecker, ("StartBatchingCallbacks"));
  ASSERT(!g_isBatched, ());
  ASSERT(g_batchedCallbackData.empty(), ());

  g_isBatched = true;
}

static void EndBatchingCallbacks(JNIEnv * env)
{
  CHECK_THREAD_CHECKER(g_batchingThreadChecker, ("EndBatchingCallbacks"));

  auto const & listBuilder = jni::ListBuilder::Instance(env);

  for (auto & key : g_batchedCallbackData)
  {
    // Allocate resulting ArrayList
    jni::TScopedLocalRef const list(env, listBuilder.CreateArray(env, key.second.size()));

    for (TBatchedData const & dataItem : key.second)
    {
      // Create StorageCallbackData instance…
      static jclass batchDataClass = jni::GetGlobalClassRef(env, "app/organicmaps/downloader/MapManager$StorageCallbackData");
      static jmethodID batchDataCtor = jni::GetConstructorID(env, batchDataClass, "(Ljava/lang/String;IIZ)V");

      jni::TScopedLocalRef const id(env, jni::ToJavaString(env, dataItem.m_countryId));
      jni::TScopedLocalRef const item(env, env->NewObject(batchDataClass, batchDataCtor, id.get(),
                                                                                         static_cast<jint>(dataItem.m_newStatus),
                                                                                         static_cast<jint>(dataItem.m_errorCode),
                                                                                         dataItem.m_isLeaf));
      // …and put it into the resulting list
      env->CallBooleanMethod(list.get(), listBuilder.m_add, item.get());
    }

    // Invoke Java callback
    jmethodID const method = jni::GetMethodID(env, key.first, "onStatusChanged", "(Ljava/util/List;)V");
    env->CallVoidMethod(key.first, method, list.get());
  }

  g_batchedCallbackData.clear();
  g_isBatched = false;
}

// static void nativeDownload(String root);
JNIEXPORT void JNICALL
Java_app_organicmaps_downloader_MapManager_nativeDownload(JNIEnv * env, jclass clazz, jstring root)
{
  StartBatchingCallbacks();
  GetStorage().DownloadNode(jni::ToNativeString(env, root));
  EndBatchingCallbacks(env);
}

// static boolean nativeRetry(String root);
JNIEXPORT void JNICALL
Java_app_organicmaps_downloader_MapManager_nativeRetry(JNIEnv * env, jclass clazz, jstring root)
{
  StartBatchingCallbacks();
  GetStorage().RetryDownloadNode(jni::ToNativeString(env, root));
  EndBatchingCallbacks(env);
}

// static void nativeUpdate(String root);
JNIEXPORT void JNICALL
Java_app_organicmaps_downloader_MapManager_nativeUpdate(JNIEnv * env, jclass clazz, jstring root)
{
  StartBatchingCallbacks();
  GetStorage().UpdateNode(GetRootId(env, root));
  EndBatchingCallbacks(env);
}

// static void nativeCancel(String root);
JNIEXPORT void JNICALL
Java_app_organicmaps_downloader_MapManager_nativeCancel(JNIEnv * env, jclass clazz, jstring root)
{
  StartBatchingCallbacks();
  GetStorage().CancelDownloadNode(GetRootId(env, root));
  EndBatchingCallbacks(env);
}

// static void nativeDelete(String root);
JNIEXPORT void JNICALL
Java_app_organicmaps_downloader_MapManager_nativeDelete(JNIEnv * env, jclass clazz, jstring root)
{
  StartBatchingCallbacks();
  auto const countryId = jni::ToNativeString(env, root);
  GetStorage().DeleteNode(countryId);
  EndBatchingCallbacks(env);
}

static void StatusChangedCallback(std::shared_ptr<jobject> const & listenerRef,
                                  storage::CountryId const & countryId)
{
  storage::NodeStatuses ns;
  GetStorage().GetNodeStatuses(countryId, ns);

  TBatchedData const data(countryId, ns.m_status, ns.m_error, !ns.m_groupNode);
  g_batchedCallbackData[*listenerRef].push_back(std::move(data));

  if (!g_isBatched)
    EndBatchingCallbacks(jni::GetEnv());
}

static void ProgressChangedCallback(std::shared_ptr<jobject> const & listenerRef,
                                    storage::CountryId const & countryId, downloader::Progress const & progress)
{
  JNIEnv * env = jni::GetEnv();

  jmethodID const methodID = jni::GetMethodID(env, *listenerRef, "onProgress", "(Ljava/lang/String;JJ)V");
  env->CallVoidMethod(*listenerRef, methodID, jni::ToJavaString(env, countryId),
                      progress.m_bytesDownloaded, progress.m_bytesTotal);
}

// static int nativeSubscribe(StorageCallback listener);
JNIEXPORT jint JNICALL
Java_app_organicmaps_downloader_MapManager_nativeSubscribe(JNIEnv * env, jclass clazz, jobject listener)
{
  return GetStorage().Subscribe(std::bind(&StatusChangedCallback, jni::make_global_ref(listener), std::placeholders::_1),
                                std::bind(&ProgressChangedCallback, jni::make_global_ref(listener), std::placeholders::_1, std::placeholders::_2));
}

// static void nativeUnsubscribe(int slot);
JNIEXPORT void JNICALL
Java_app_organicmaps_downloader_MapManager_nativeUnsubscribe(JNIEnv * env, jclass clazz, jint slot)
{
  GetStorage().Unsubscribe(slot);
}

// static void nativeSubscribeOnCountryChanged(CurrentCountryChangedListener listener);
JNIEXPORT void JNICALL
Java_app_organicmaps_downloader_MapManager_nativeSubscribeOnCountryChanged(JNIEnv * env, jclass clazz, jobject listener)
{
  ASSERT(!g_countryChangedListener, ());
  g_countryChangedListener = env->NewGlobalRef(listener);

  auto const callback = [](storage::CountryId const & countryId) {
    JNIEnv * env = jni::GetEnv();
    jmethodID methodID = jni::GetMethodID(env, g_countryChangedListener, "onCurrentCountryChanged", "(Ljava/lang/String;)V");
    env->CallVoidMethod(g_countryChangedListener, methodID, jni::TScopedLocalRef(env, jni::ToJavaString(env, countryId)).get());
  };

  storage::CountryId const & prev = g_framework->NativeFramework()->GetLastReportedCountry();
  g_framework->NativeFramework()->SetCurrentCountryChangedListener(callback);

  // Report previous value
  callback(prev);
}

// static void nativeUnsubscribeOnCountryChanged();
JNIEXPORT void JNICALL
Java_app_organicmaps_downloader_MapManager_nativeUnsubscribeOnCountryChanged(JNIEnv * env, jclass clazz)
{
  g_framework->NativeFramework()->SetCurrentCountryChangedListener(nullptr);

  env->DeleteGlobalRef(g_countryChangedListener);
  g_countryChangedListener = nullptr;
}

// static boolean nativeHasUnsavedEditorChanges(String root);
JNIEXPORT jboolean JNICALL
Java_app_organicmaps_downloader_MapManager_nativeHasUnsavedEditorChanges(JNIEnv * env, jclass clazz, jstring root)
{
  return g_framework->NativeFramework()->HasUnsavedEdits(jni::ToNativeString(env, root));
}

// static void nativeGetPathTo(String root, List<String> result);
JNIEXPORT void JNICALL
Java_app_organicmaps_downloader_MapManager_nativeGetPathTo(JNIEnv * env, jclass clazz, jstring root, jobject result)
{
  auto const listAddMethod = jni::ListBuilder::Instance(env).m_add;

  storage::CountriesVec path;
  GetStorage().GetGroupNodePathToRoot(jni::ToNativeString(env, root), path);
  for (storage::CountryId const & node : path)
    env->CallBooleanMethod(result, listAddMethod, jni::TScopedLocalRef(env, jni::ToJavaString(env, node)).get());
}

// static int nativeGetOverallProgress(String[] countries);
JNIEXPORT jint JNICALL
Java_app_organicmaps_downloader_MapManager_nativeGetOverallProgress(JNIEnv * env, jclass clazz, jobjectArray jcountries)
{
  int const size = env->GetArrayLength(jcountries);
  storage::CountriesVec countries;
  countries.reserve(size);

  for (int i = 0; i < size; i++)
  {
    jni::TScopedLocalRef const item(env, env->GetObjectArrayElement(jcountries, i));
    countries.push_back(jni::ToNativeString(env, static_cast<jstring>(item.get())));
  }

  downloader::Progress const progress = GetStorage().GetOverallProgress(countries);

  jint res = 0;
  if (progress.m_bytesTotal)
    res = static_cast<jint>(progress.m_bytesDownloaded * kMaxProgressWithoutDiffs / progress.m_bytesTotal);

  return res;
}

// static boolean nativeIsAutoretryFailed();
JNIEXPORT jboolean JNICALL
Java_app_organicmaps_downloader_MapManager_nativeIsAutoretryFailed(JNIEnv * env, jclass clazz)
{
  return g_framework->IsAutoRetryDownloadFailed();
}

// static boolean nativeIsDownloadOn3gEnabled();
JNIEXPORT jboolean JNICALL
Java_app_organicmaps_downloader_MapManager_nativeIsDownloadOn3gEnabled(JNIEnv * env, jclass clazz)
{
  return g_framework->IsDownloadOn3gEnabled();
}

// static void nativeEnableDownloadOn3g();
JNIEXPORT void JNICALL
Java_app_organicmaps_downloader_MapManager_nativeEnableDownloadOn3g(JNIEnv * env, jclass clazz)
{
  g_framework->EnableDownloadOn3g();
}

// static @Nullable String nativeGetSelectedCountry();
JNIEXPORT jstring JNICALL
Java_app_organicmaps_downloader_MapManager_nativeGetSelectedCountry(JNIEnv * env, jclass clazz)
{
  if (!g_framework->NativeFramework()->HasPlacePageInfo())
    return nullptr;

  storage::CountryId const & res = g_framework->GetPlacePageInfo().GetCountryId();
  return (res == storage::kInvalidCountryId ? nullptr : jni::ToJavaString(env, res));
}
} // extern "C"
