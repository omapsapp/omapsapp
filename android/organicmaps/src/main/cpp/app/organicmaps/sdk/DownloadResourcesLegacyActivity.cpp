#include "Framework.hpp"

#include "defines.hpp"

#include "storage/downloader.hpp"
#include "storage/storage.hpp"

#include "platform/downloader_defines.hpp"
#include "platform/http_request.hpp"
#include "platform/platform.hpp"

#include "coding/internal/file_data.hpp"
#include "coding/reader_streambuf.hpp"

#include "base/file_name_utils.hpp"
#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include "app/organicmaps/core/jni_helper.hpp"

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace downloader;
using namespace storage;

using namespace std::placeholders;

/// Special error codes to notify GUI about free space
//@{
#define ERR_DOWNLOAD_SUCCESS         0
#define ERR_DISK_ERROR              -1
#define ERR_NOT_ENOUGH_FREE_SPACE   -2
#define ERR_STORAGE_DISCONNECTED    -3
#define ERR_DOWNLOAD_ERROR          -4
#define ERR_NO_MORE_FILES           -5
#define ERR_FILE_IN_PROGRESS        -6
//@}

namespace
{

static std::vector<platform::CountryFile> g_filesToDownload;
static int g_totalDownloadedBytes;
static int g_totalBytesToDownload;
static std::shared_ptr<HttpRequest> g_currentRequest;

}  // namespace

extern "C"
{
  using Callback = HttpRequest::Callback;

  static int HasSpaceForFiles(Platform & pl, std::string const & sdcardPath, size_t fileSize)
  {
    switch (pl.GetWritableStorageStatus(fileSize))
    {
      case Platform::STORAGE_DISCONNECTED:
        return ERR_STORAGE_DISCONNECTED;

      case Platform::NOT_ENOUGH_SPACE:
        return ERR_NOT_ENOUGH_FREE_SPACE;

      default:
        return static_cast<int>(fileSize);
    }
  }

  JNIEXPORT jint JNICALL
  Java_app_organicmaps_DownloadResourcesLegacyActivity_nativeGetBytesToDownload(JNIEnv * env, jclass clazz)
  {
    // clear all
    g_filesToDownload.clear();
    g_totalBytesToDownload = 0;
    g_totalDownloadedBytes = 0;

    using namespace storage;
    Storage const & storage = g_framework->GetStorage();
    auto const status = storage.GetForceDownloadWorlds(g_filesToDownload);

    for (auto const & cf : g_filesToDownload)
      g_totalBytesToDownload += cf.GetRemoteSize();

    int res;
    if (status == Storage::WorldStatus::ERROR_CREATE_FOLDER ||
        status == Storage::WorldStatus::ERROR_MOVE_FILE)
    {
      res = ERR_DISK_ERROR;
    }
    else
    {
      Platform & pl = GetPlatform();
      res = HasSpaceForFiles(pl, pl.WritableDir(), g_totalBytesToDownload);
    }

    if (res == ERR_STORAGE_DISCONNECTED)
      LOG(LWARNING, ("External file system is not available"));
    else if (res == ERR_NOT_ENOUGH_FREE_SPACE)
      LOG(LWARNING, ("Not enough space to extract files"));

    g_currentRequest.reset();

    if (status == Storage::WorldStatus::WAS_MOVED)
    {
      g_framework->ReloadWorldMaps();
      res = ERR_DOWNLOAD_SUCCESS;  // reset possible storage error if we moved files
    }

    return res;
  }

  static void DownloadFileFinished(std::shared_ptr<jobject> obj, HttpRequest const & req)
  {
    auto const status = req.GetStatus();
    ASSERT_NOT_EQUAL(status, DownloadStatus::InProgress, ());

    int errorCode = ERR_DOWNLOAD_ERROR;
    if (status == DownloadStatus::Completed)
      errorCode = ERR_DOWNLOAD_SUCCESS;

    g_currentRequest.reset();

    if (errorCode == ERR_DOWNLOAD_SUCCESS)
    {
      auto const & curFile = g_filesToDownload.back();
      size_t const sz = curFile.GetRemoteSize();
      LOG(LDEBUG, ("finished downloading", curFile.GetName(), "size", sz, "bytes"));

      g_totalDownloadedBytes += sz;
      LOG(LDEBUG, ("totalDownloadedBytes:", g_totalDownloadedBytes));

      g_filesToDownload.pop_back();
    }

    JNIEnv * env = jni::GetEnv();

    jmethodID methodID = jni::GetMethodID(env, *obj, "onFinish", "(I)V");
    env->CallVoidMethod(*obj, methodID, errorCode);
  }

  static void DownloadFileProgress(std::shared_ptr<jobject> listener, HttpRequest const & req)
  {
    JNIEnv * env = jni::GetEnv();
    static jmethodID methodID = jni::GetMethodID(env, *listener, "onProgress", "(I)V");
    env->CallVoidMethod(*listener, methodID, static_cast<jint>(g_totalDownloadedBytes + req.GetProgress().m_bytesDownloaded));
  }

  JNIEXPORT jint JNICALL
  Java_app_organicmaps_DownloadResourcesLegacyActivity_nativeStartNextFileDownload(JNIEnv * env, jclass clazz, jobject listener)
  {
    if (g_filesToDownload.empty())
      return ERR_NO_MORE_FILES;

    /// @todo One downloader instance with cached servers. All this routine will be refactored some time.
    static auto downloader = storage::GetDownloader();
    storage::Storage const & storage = g_framework->GetStorage();
    downloader->SetDataVersion(storage.GetCurrentDataVersion());

    downloader->EnsureMetaConfigReady([&storage, ptr = jni::make_global_ref(listener)]()
    {
      auto const & curFile = g_filesToDownload.back();
      auto const fileName = curFile.GetFileName(MapFileType::Map);
      LOG(LINFO, ("Downloading file", fileName));

      g_currentRequest.reset(HttpRequest::GetFile(
          downloader->MakeUrlListLegacy(fileName),
          storage.GetFilePath(curFile.GetName(), MapFileType::Map),
          curFile.GetRemoteSize(),
          std::bind(&DownloadFileFinished, ptr, _1),
          std::bind(&DownloadFileProgress, ptr, _1),
          512 * 1024, false));
    });

    return ERR_FILE_IN_PROGRESS;
  }

  JNIEXPORT void JNICALL
  Java_app_organicmaps_DownloadResourcesLegacyActivity_nativeCancelCurrentFile(JNIEnv * env, jclass clazz)
  {
    LOG(LDEBUG, ("cancelCurrentFile, currentRequest=", g_currentRequest));
    g_currentRequest.reset();
  }
}
