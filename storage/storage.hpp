#pragma once

#include "storage/country.hpp"
#include "storage/country_name_getter.hpp"
#include "storage/country_tree.hpp"
#include "storage/diff_scheme/diffs_data_source.hpp"
#include "storage/downloading_policy.hpp"
#include "storage/map_files_downloader.hpp"
#include "storage/queued_country.hpp"
#include "storage/storage_defines.hpp"

#include "platform/downloader_defines.hpp"
#include "platform/local_country_file.hpp"

#include "base/cancellable.hpp"
#include "base/deferred_task.hpp"
#include "base/thread_checker.hpp"
#include "base/thread_pool_delayed.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace storage
{
struct CountryIdAndName
{
  CountryId m_id;
  std::string m_localName;

  bool operator==(CountryIdAndName const & other) const
  {
    return m_id == other.m_id && m_localName == other.m_localName;
  }
};

/// \brief Contains all properties for a node in the country tree.
/// It's applicable for expandable and not expandable node id.
struct NodeAttrs
{
  NodeAttrs() : m_mwmCounter(0), m_localMwmCounter(0), m_downloadingMwmCounter(0),
    m_mwmSize(0), m_localMwmSize(0), m_downloadingMwmSize(0),
    m_status(NodeStatus::Undefined), m_error(NodeErrorCode::NoError), m_present(false) {}

  /// If the node is expandable (a big country) |m_mwmCounter| is number of mwm files (leaves)
  /// belonging to the node. If the node isn't expandable |m_mwmCounter| == 1.
  /// Note. For every expandable node |m_mwmCounter| >= 2.
  MwmCounter m_mwmCounter;

  /// Number of mwms belonging to the node which have been downloaded.
  MwmCounter m_localMwmCounter;

  /// Number of leaves of the node which have been downloaded
  /// plus which is in progress of downloading (zero or one)
  /// plus which are staying in queue.
  MwmCounter m_downloadingMwmCounter;

  /// If it's not an expandable node, |m_mwmSize| is size of one mwm according to countries.txt.
  /// Otherwise |m_mwmSize| is the sum of all mwm file sizes which belong to the group
  /// according to countries.txt.
  MwmSize m_mwmSize;

  /// If it's not an expandable node, |m_localMwmSize| is size of one downloaded mwm.
  /// Otherwise |m_localNodeSize| is the sum of all mwm file sizes which belong to the group and
  /// have been downloaded.
  MwmSize m_localMwmSize;

  /// Size of leaves of the node which have been downloaded
  /// plus which is in progress of downloading (zero or one)
  /// plus which are staying in queue.
  /// \note The size of leaves is the size is written in countries.txt.
  MwmSize m_downloadingMwmSize;

  /// The name of the node in a local language. That means the language dependent on
  /// a device locale.
  std::string m_nodeLocalName;

  /// The description of the node in a local language. That means the language dependent on
  /// a device locale.
  std::string m_nodeLocalDescription;

  /// Node id and local name of the parents of the node.
  /// For the root |m_parentInfo| is empty.
  /// Locale language is a language set by Storage::SetLocale().
  /// \note Most number of nodes have only one parent. But in case of a disputed territories
  /// an mwm could have two or even more parents. See Country description for details.
  std::vector<CountryIdAndName> m_parentInfo;

  /// Node id and local name of the first level parents (root children nodes)
  /// if the node has first level parent(s). Otherwise |m_topmostParentInfo| is empty.
  /// That means for the root and for the root children |m_topmostParentInfo| is empty.
  /// Locale language is a language set by Storage::SetLocale().
  /// \note Most number of nodes have only first level parent. But in case of a disputed territories
  /// an mwm could have two or even more parents. See Country description for details.
  std::vector<CountryIdAndName> m_topmostParentInfo;

  /// Progress of downloading for the node expandable or not. It reflects downloading progress in case of
  /// downloading and updating mwm.
  /// m_downloadingProgress.m_bytesDownloaded is number of downloaded bytes.
  /// m_downloadingProgress.m_bytesTotal is size of file(s) in bytes to download.
  /// So m_downloadingProgress.m_bytesDownloaded <= m_downloadingProgress.m_bytesTotal.
  downloader::Progress m_downloadingProgress;

  /// Status of group and leaf node.
  /// For group nodes it's defined in the following way:
  /// If an mwm in a group has Downloading status the group has Downloading status
  /// Otherwise if an mwm in the group has InQueue status the group has InQueue status
  /// Otherwise if an mwm in the group has Error status the group has Error status
  /// Otherwise if an mwm in the group has OnDiskOutOfDate the group has OnDiskOutOfDate status
  /// Otherwise if all the mwms in the group have OnDisk status the group has OnDisk status
  /// Otherwise if all the mwms in the group have NotDownloaded status the group has NotDownloaded status
  /// Otherwise (that means a part of mwms in the group has OnDisk and the other part has NotDownloaded status)
  ///   the group has Mixed status
  NodeStatus m_status;
  /// Error code of leaf node. In case of group node |m_error| == NodeErrorCode::NoError.
  NodeErrorCode m_error;

  /// Indicates if leaf mwm is currently downloaded and connected to storage.
  /// Can be used to distinguish downloadable and updatable maps.
  /// m_present == false for group mwms.
  bool m_present;
};

/// \brief Statuses for a node in the country tree.
/// It's applicable for expandable and not expandable node id.
struct NodeStatuses
{
  NodeStatus m_status;
  NodeErrorCode m_error;
  bool m_groupNode;
};

// This class is used for downloading, updating and deleting maps.
// Storage manages a queue of mwms to be downloaded.
// Every operation with this queue must be executed
// on the storage thread. In the current implementation, the storage
// thread coincides with the main (UI) thread.
// Downloading of only one mwm at a time is supported, so while the
// mwm at the top of the queue is being downloaded (or updated by
// applying a diff file) all other mwms have to wait.
class Storage final : public QueuedCountry::Subscriber
{
public:
  using StartDownloadingCallback = std::function<void()>;
  using UpdateCallback = std::function<void(storage::CountryId const &, LocalFilePtr const)>;
  using DeleteCallback = std::function<bool(storage::CountryId const &, LocalFilePtr const)>;
  using ChangeCountryFunction = std::function<void(CountryId const &)>;
  using ProgressFunction = std::function<void(CountryId const &, downloader::Progress const &)>;
  using DownloadingCountries = std::unordered_map<CountryId, downloader::Progress>;

private:
  /// We support only one simultaneous request at the moment
  std::unique_ptr<MapFilesDownloader> m_downloader;

  /// Stores timestamp for update checks
  int64_t m_currentVersion = 0;

  CountryTree m_countries;

  /// Set of mwm files which have been downloaded recently.
  /// When a mwm file is downloaded it's added to |m_justDownloaded|.
  /// Note. This set is necessary for implementation of downloading progress of
  /// mwm group.
  CountriesSet m_justDownloaded;

  /// stores countries whose download has failed recently
  CountriesSet m_failedCountries;

  std::map<CountryId, std::list<LocalFilePtr>> m_localFiles;

  // Our World.mwm and WorldCoasts.mwm are fake countries, together with any custom mwm in data
  // folder.
  std::map<platform::CountryFile, LocalFilePtr> m_localFilesForFakeCountries;

  // Since the diffs applying runs on a different thread, the result
  // of diff application may return "Ok" when in fact the diff was
  // cancelled. However, the storage thread knows for sure whether
  // request was to apply or to cancel the diff, and this knowledge
  // is represented by |m_diffsBeingApplied|.
  std::unordered_map<CountryId, std::unique_ptr<base::Cancellable>> m_diffsBeingApplied;

  std::vector<platform::LocalCountryFile> m_notAppliedDiffs;

  diffs::DiffsSourcePtr m_diffsDataSource = std::make_shared<diffs::DiffsDataSource>();

  DownloadingPolicy m_defaultDownloadingPolicy;
  DownloadingPolicy * m_downloadingPolicy = &m_defaultDownloadingPolicy;

  /// @name Communicate with GUI
  //@{

  int m_currentSlotId = 0;

  struct CountryObservers
  {
    ChangeCountryFunction m_changeCountryFn;
    ProgressFunction m_progressFn;
    int m_slotId;
  };

  std::list<CountryObservers> m_observers;
  //@}

  // This function is called each time all files requested for a
  // country are successfully downloaded.
  UpdateCallback m_didDownload;

  // This function is called each time all files for a
  // country are deleted.
  DeleteCallback m_willDelete;

  // If |m_dataDir| is not empty Storage will create version directories and download maps in
  // platform::WritableDir/|m_dataDir|/. Not empty |m_dataDir| can be used only for
  // downloading maps to a special place but not for continue working with them from this place.
  std::string m_dataDir;

  bool m_integrityValidationEnabled = true;

  // |m_downloadMapOnTheMap| is called when an end user clicks on download map or retry button
  // on the map.
  DownloadFn m_downloadMapOnTheMap;

  CountryNameGetter m_countryNameGetter;

  /**
   * @brief Mapping from countryId to the list of names of
   * geographical objects (such as countries) that encompass this countryId.
   * @note Affiliations are inherited from ancestors of the countryId in country tree.
   * Initialized with data of countries.txt (field "affiliations").
   * Once filled, they are not changed.
   */
  Affiliations m_affiliations;
  CountryNameSynonyms m_countryNameSynonyms;

  /// @todo This containers are empty for now, but probably will be used in future.
  /// @{
  MwmTopCityGeoIds m_mwmTopCityGeoIds;
  MwmTopCountryGeoIds m_mwmTopCountryGeoIds;
  /// @}

  MwmSize m_maxMwmSizeBytes = 0;

  ThreadChecker m_threadChecker;

  bool m_needToStartDeferredDownloading = false;

  StartDownloadingCallback m_startDownloadingCallback;

  DownloadingCountries m_downloadingCountries;

  void LoadCountriesFile(std::string const & pathToCountriesFile);

  void ReportProgress(CountryId const & countryId, downloader::Progress const & p);
  void ReportProgressForHierarchy(CountryId const & countryId,
                                  downloader::Progress const & leafProgress);

  // QueuedCountry::Subscriber overrides:
  void OnCountryInQueue(QueuedCountry const & queuedCountry) override;
  void OnStartDownloading(QueuedCountry const & queuedCountry) override;
  /// Called on the main thread by MapFilesDownloader when
  /// downloading of a map file succeeds/fails.
  void OnDownloadFinished(QueuedCountry const & queuedCountry,
                          downloader::DownloadStatus status) override;

  /// Periodically called on the main thread by MapFilesDownloader
  /// during the downloading process.
  void OnDownloadProgress(QueuedCountry const & queuedCountry,
                          downloader::Progress const & progress) override;

  void RegisterDownloadedFiles(CountryId const & countryId, MapFileType type);

  void OnMapDownloadFinished(CountryId const & countryId, downloader::DownloadStatus status,
                             MapFileType type);

  /// Dummy ctor for private use only.
  explicit Storage(int);

public:
  ThreadChecker const & GetThreadChecker() const {return m_threadChecker;}

  /// \brief Storage will create its directories in Writable Directory
  /// (gotten with platform::WritableDir) by default.
  /// \param pathToCountriesFile is a name of countries.txt file.
  /// \param dataDir If |dataDir| is not empty Storage will create its directory in WritableDir/|dataDir|.
  /// \note if |dataDir| is not empty the instance of Storage can be used only for downloading map files
  /// but not for continue working with them.
  /// If |dataDir| is not empty the work flow is
  /// * create a instance of Storage with a special countries.txt and |dataDir|
  /// * download some maps to WritableDir/|dataDir|
  /// * destroy the instance of Storage and move the downloaded maps to proper place
  Storage(std::string const & pathToCountriesFile = COUNTRIES_FILE,
          std::string const & dataDir = std::string());

  /// \brief This constructor should be used for testing only.
  Storage(std::string const & referenceCountriesTxtJsonForTesting,
          std::unique_ptr<MapFilesDownloader> mapDownloaderForTesting);

  void Init(UpdateCallback didDownload, DeleteCallback willDelete);

  void SetDownloadingPolicy(DownloadingPolicy * policy);

  bool CheckFailedCountries(CountriesVec const & countries) const;

  /// @name Countries update functions. Public for unit tests.
  /// @{
  void RunCountriesCheckAsync();
  /// @return 0 If error.
  int64_t ParseIndexAndGetDataVersion(std::string const & index) const;
  void ApplyCountries(std::string const & countriesBuffer, Storage & storage);
  /// @}

  /// \brief Returns root country id of the country tree.
  CountryId const GetRootId() const;

  /// \param childIds is filled with children node ids by a parent. For example GetChildren(GetRootId())
  /// returns in param all countries ids. It's content of map downloader list by default.
  void GetChildren(CountryId const & parent, CountriesVec & childIds) const;

  /// \brief Fills |downloadedChildren| and |availChildren| with children of parent.
  /// If a direct child of |parent| contains at least one downloaded mwm
  /// the mwm id of the child will be added to |downloadedChildren|.
  /// If not, the mwm id the child will not be added to |availChildren|.
  /// \param parent is a parent acoording to countries.txt or cournties_migrate.txt.
  /// \param downloadedChildren children partly or fully downloaded.
  /// \param availChildren fully available children. None of its files have been downloaded.
  /// \param keepAvailableChildren keeps all children in |availChildren| otherwise downloaded
  ///        children will be removed from |availChildren|.
  /// \note. This method puts to |downloadedChildren| and |availChildren| only real maps (and its ancestors)
  /// which have been written in coutries.txt or cournties_migrate.txt.
  /// It means the method does not put to its params neither custom maps generated by user
  /// nor World.mwm and WorldCoasts.mwm.
  void GetChildrenInGroups(CountryId const & parent, CountriesVec & downloadedChildren,
                           CountriesVec & availChildren, bool keepAvailableChildren = false) const;
  /// \brief Fills |queuedChildren| with children of |parent| if they (or thier childen) are in |m_queue|.
  /// \note For group node children if one of child's ancestor has status
  /// NodeStatus::Downloading or NodeStatus::InQueue the child is considered as a queued child
  /// and will be added to |queuedChildren|.
  void GetQueuedChildren(CountryId const & parent, CountriesVec & queuedChildren) const;

  /// \brief Fills |path| with list of CountryId corresponding with path to the root of hierachy.
  /// \param groupNode is start of path, can't be a leaf node.
  /// \param path is resulting array of CountryId.
  void GetGroupNodePathToRoot(CountryId const & groupNode, CountriesVec & path) const;

  /// \brief Fills |nodes| with CountryIds of topmost nodes for this |countryId|.
  /// \param level is distance from top level except root.
  /// For disputed territories all possible owners will be added.
  /// Puts |countryId| to |nodes| when |level| is greater than the level of |countyId|.
  void GetTopmostNodesFor(CountryId const & countryId, CountriesVec & nodes,
                          size_t level = 0) const;

  /// \brief Returns topmost country id prior root id or |countryId| itself, if it's already
  /// a topmost node or disputed territory id if |countryId| is a disputed territory or belongs to
  /// disputed territory.
  CountryId const GetTopmostParentFor(CountryId const & countryId) const;
  /// \brief Returns parent id for node if node has single parent. Otherwise (if node is disputed
  /// territory and has multiple parents or does not exist) returns empty CountryId
  CountryId const GetParentIdFor(CountryId const & countryId) const;

  /// \brief Returns current version for mwms which are used by storage.
  inline int64_t GetCurrentDataVersion() const { return m_currentVersion; }

  /// \brief Returns true if the node with countryId has been downloaded and false othewise.
  /// If countryId is expandable returns true if all mwms which belongs to it have downloaded.
  /// Returns false if countryId is an unknown string.
  /// \note The method return false for custom maps generated by user
  /// and World.mwm and WorldCoasts.mwm.
  bool IsNodeDownloaded(CountryId const & countryId) const;

  /// \brief Returns true if the last version of countryId has been downloaded.
  bool HasLatestVersion(CountryId const & countryId) const;

  /// Returns version of downloaded mwm or zero.
  int64_t GetVersion(CountryId const & countryId) const;

  /// \brief Gets all the attributes for a node by its |countryId|.
  /// \param |nodeAttrs| is filled with attributes in this method.
  void GetNodeAttrs(CountryId const & countryId, NodeAttrs & nodeAttrs) const;

  /// \brief Gets a short list of node attributes by its |countriId|.
  /// \note This method works quicklier than GetNodeAttrs().
  void GetNodeStatuses(CountryId const & countryId, NodeStatuses & nodeStatuses) const;

  std::string GetNodeLocalName(CountryId const & countryId) const
  {
    return m_countryNameGetter(countryId);
  }

  /// \brief Downloads/update one node (expandable or not) by countryId.
  /// If node is expandable downloads/update all children (grandchildren) by the node
  /// until they haven't been downloaded before.
  void DownloadNode(CountryId const & countryId, bool isUpdate = false);

  /// \brief Delete node with all children (expandable or not).
  void DeleteNode(CountryId const & countryId);

  /// \brief Updates one node. It works for leaf and group mwms.
  /// \note If you want to update all the maps and this update is without changing
  /// borders or hierarchy just call UpdateNode(GetRootId()).
  void UpdateNode(CountryId const & countryId);

  /// \brief If the downloading a new node is in process cancels downloading the node and deletes
  /// the downloaded part of the map. If the map is in queue, remove the map from the queue.
  /// If the downloading a updating map is in process cancels the downloading,
  /// deletes the downloaded part of the map and leaves as is the old map (before the update)
  /// had been downloaded. It works for leaf and for group mwms.
  void CancelDownloadNode(CountryId const & countryId);

  /// \brief Downloading process could be interupted because of bad internet connection
  /// and some other reason.
  /// In that case user could want to recover it. This method is done for it.
  /// This method works with leaf and group mwm.
  /// In case of a group mwm this method retries downloading all mwm in m_failedCountries list
  /// which in the subtree with root |countryId|.
  /// It means the call RetryDownloadNode(GetRootId()) retries all the failed mwms.
  void RetryDownloadNode(CountryId const & countryId);

  struct UpdateInfo
  {
    UpdateInfo() : m_numberOfMwmFilesToUpdate(0), m_totalUpdateSizeInBytes(0), m_sizeDifference(0) {}

    MwmCounter m_numberOfMwmFilesToUpdate;
    MwmSize m_totalUpdateSizeInBytes;
    // Difference size in bytes between before update and after update.
    int64_t m_sizeDifference;
  };

  /// \brief Get information for mwm update button.
  /// \return true if updateInfo is filled correctly and false otherwise.
  bool GetUpdateInfo(CountryId const & countryId, UpdateInfo & updateInfo) const;

  /// @name This functions should be called from 'main' thread only to avoid races.
  /// @{
  /// @return Pointer that will be stored for later use.
  Affiliations const * GetAffiliations() const;
  CountryNameSynonyms const & GetCountryNameSynonyms() const;
  MwmTopCityGeoIds const & GetMwmTopCityGeoIds() const;
  std::vector<base::GeoObjectId> GetTopCountryGeoIds(CountryId const & countryId) const;
  /// @}

  /// \brief Calls |toDo| for each node for subtree with |root|.
  /// For example ForEachInSubtree(GetRootId()) calls |toDo| for every node including
  /// the result of GetRootId() call.
  template <class ToDo>
  void ForEachInSubtree(CountryId const & root, ToDo && toDo) const;
  template <class ToDo>
  void ForEachAncestorExceptForTheRoot(CountryId const & childId, ToDo && toDo) const;
  template <class ToDo>
  void ForEachCountryFile(ToDo && toDo) const;

  /// \brief Sets callback which will be called in case of a click on download map button on the map.
  void SetCallbackForClickOnDownloadMap(DownloadFn & downloadFn);

  /// \brief Calls |m_downloadMapOnTheMap| if one has been set.
  /// \param |countryId| is country id of a leaf. That means it's a file name.
  /// \note This method should be called for a click of download map button
  /// and for a click for retry downloading map button on the map.
  void DoClickOnDownloadMap(CountryId const & countryId);
  //@}

  /// \returns real (not fake) local maps contained in countries.txt.
  /// So this method does not return custom user local maps and World and WorldCoasts country id.
  void GetLocalRealMaps(CountriesVec & localMaps) const;

  /// Do we have downloaded countries
  bool HaveDownloadedCountries() const;

  /// Delete local maps and aggregate their Id if needed
  void DeleteAllLocalMaps(CountriesVec * existedCountries = nullptr);

  // Clears local files registry and downloader's queue.
  void Clear();

  // Finds and registers all map files in maps directory. In the case
  // of several versions of the same map keeps only the latest one, others
  // are deleted from disk.
  // *NOTE* storage will forget all already known local maps.
  void RegisterAllLocalMaps(bool enableDiffs);

  // Returns list of all local maps, including fake countries (World*.mwm).
  void GetLocalMaps(std::vector<LocalFilePtr> & maps) const;

  // Returns number of downloaded maps (files), excluding fake countries (World*.mwm).
  size_t GetDownloadedFilesCount() const;

  /// Guarantees that change and progress are called in the main thread context.
  /// @return unique identifier that should be used with Unsubscribe function
  int Subscribe(ChangeCountryFunction change, ProgressFunction progress);
  void Unsubscribe(int slotId);

  /// Returns information about selected counties downloading progress.
  /// |countries| - watched CountryId, ONLY leaf expected.
  downloader::Progress GetOverallProgress(CountriesVec const & countries) const;

  Country const & CountryLeafByCountryId(CountryId const & countryId) const;
  Country const & CountryByCountryId(CountryId const & countryId) const;

  CountryId FindCountryIdByFile(std::string const & name) const;

  // Returns true iff |countryId| exists as a node in the tree.
  bool IsNode(CountryId const & countryId) const;

  // Returns true iff |countryId| is a leaf of the tree.
  bool IsLeaf(CountryId const & countryId) const;

  // Returns true iff |countryId| is an inner node of the tree.
  bool IsInnerNode(CountryId const & countryId) const;

  LocalAndRemoteSize CountrySizeInBytes(CountryId const & countryId) const;
  MwmSize GetRemoteSize(platform::CountryFile const & file) const;
  platform::CountryFile const & GetCountryFile(CountryId const & countryId) const;
  LocalFilePtr GetLatestLocalFile(platform::CountryFile const & countryFile) const;
  LocalFilePtr GetLatestLocalFile(CountryId const & countryId) const;

  /// Slow version, but checks if country is out of date
  Status CountryStatusEx(CountryId const & countryId) const;

  /// Puts country denoted by countryId into the downloader's queue.
  /// During downloading process notifies observers about downloading
  /// progress and status changes.
  void DownloadCountry(CountryId const & countryId, MapFileType type);

  /// Removes country files (for all versions) from the device.
  /// Notifies observers about country status change.
  void DeleteCountry(CountryId const & countryId, MapFileType type);

  /// Removes country files of a particular version from the device.
  /// Notifies observers about country status change.
  void DeleteCustomCountryVersion(platform::LocalCountryFile const & localFile);

  bool IsDownloadInProgress() const;

  /// @param[out] res Populated with oudated countries.
  void GetOutdatedCountries(std::vector<Country const *> & countries) const;

  /// Sets and gets locale, which is used to get localized counries names
  void SetLocale(std::string const & locale) { m_countryNameGetter.SetLocale(locale); }
  std::string GetLocale() const { return m_countryNameGetter.GetLocale(); }

  MwmSize GetMaxMwmSizeBytes() const { return m_maxMwmSizeBytes; }

  // for testing:
  void SetEnabledIntegrityValidationForTesting(bool enabled);
  void SetDownloaderForTesting(std::unique_ptr<MapFilesDownloader> downloader);
  void SetCurrentDataVersionForTesting(int64_t currentVersion);
  void SetDownloadingServersForTesting(std::vector<std::string> const & downloadingUrls);
  void SetLocaleForTesting(std::string const & jsonBuffer, std::string const & locale);

  /// Returns true if the diff scheme is available and all local outdated maps can be updated via
  /// diffs.
  bool IsPossibleToAutoupdate() const;

  void SetStartDownloadingCallback(StartDownloadingCallback const & cb);

protected:
  void OnFinishDownloading();

private:
  friend struct UnitClass_StorageTest_DeleteCountry;

  void SaveDownloadQueue();
  void RestoreDownloadQueue();

  // Returns true when country is in the downloader's queue.
  bool IsCountryInQueue(CountryId const & countryId) const;

  // Returns true if we started the diff applying procedure for an mwm with countryId.
  bool IsDiffApplyingInProgressToCountry(CountryId const & countryId) const;

  // Returns local country files of a particular version, or wrapped
  // nullptr if there're no country files corresponding to the
  // version.
  LocalFilePtr GetLocalFile(CountryId const & countryId, int64_t version) const;

  // Tries to register disk files for a real (listed in countries.txt)
  // country. If map files of the same version were already
  // registered, does nothing.
  void RegisterCountryFiles(LocalFilePtr localFile);

  // Registers disk files for a country. This method must be used only
  // for real (listed in countries.txt) countries.
  void RegisterCountryFiles(CountryId const & countryId, std::string const & directory,
                            int64_t version);

  // Registers disk files for a country. This method must be used only
  // for custom (made by user) map files.
  void RegisterFakeCountryFiles(platform::LocalCountryFile const & localFile);

  // Removes disk files for all versions of a country.
  void DeleteCountryFiles(CountryId const & countryId, MapFileType type, bool deferredDelete);

  // Removes country files from downloader.
  bool DeleteCountryFilesFromDownloader(CountryId const & countryId);

  // Returns a path to a place on disk downloader can use for
  // downloaded files.
  std::string GetFileDownloadPath(CountryId const & countryId, MapFileType file) const;

  /// Fast version, doesn't check if country is out of date
  Status CountryStatus(CountryId const & countryId) const;

  /// Returns status for a node (group node or not).
  StatusAndError GetNodeStatus(CountryTree::Node const & node) const;
  /// Returns status for a node (group node or not).
  /// Fills |disputedTeritories| with all disputed teritories in subtree with the root == |node|.
  StatusAndError GetNodeStatusInfo(
      CountryTree::Node const & node,
      std::vector<std::pair<CountryId, NodeStatus>> & disputedTeritories,
      bool isDisputedTerritoriesCounted) const;

  void NotifyStatusChanged(CountryId const & countryId);
  void NotifyStatusChangedForHierarchy(CountryId const & countryId);

  /// @todo Temporary function to gel all associated indexes for the country file name.
  /// Will be removed in future after refactoring.
  CountriesVec FindAllIndexesByFile(CountryId const & name) const;

  /// Calculates progress of downloading for expandable nodes in country tree.
  /// |descendants| All descendants of the parent node.
  downloader::Progress CalculateProgress(CountriesVec const & descendants) const;

  template <class ToDo>
  void ForEachAncestorExceptForTheRoot(std::vector<CountryTree::Node const *> const & nodes,
                                       ToDo && toDo) const;

  /// Returns true if |node.Value().Name()| is a disputed territory and false otherwise.
  bool IsDisputed(CountryTree::Node const & node) const;

  void CalcMaxMwmSizeBytes();

  void OnMapDownloadFailed(CountryId const & countryId);

  void LoadDiffScheme();
  void ApplyDiff(CountryId const & countryId, std::function<void(bool isSuccess)> const & fn);

  using IsDiffAbsentForCountry = std::function<bool(CountryId const & id)>;
  void SetMapSchemeForCountriesWithAbsentDiffs(IsDiffAbsentForCountry const & isAbsent);
  void AbortDiffScheme();

  // Should be called once on startup, downloading process should be suspended until this method
  // was not called. Do not call this method manually.
  void OnDiffStatusReceived(diffs::NameDiffInfoMap && diffs);
};

CountriesSet GetQueuedCountries(QueueInterface const & queue);

template <class ToDo>
void Storage::ForEachInSubtree(CountryId const & root, ToDo && toDo) const
{
  CountryTree::Node const * const rootNode = m_countries.FindFirst(root);
  if (rootNode == nullptr)
  {
    ASSERT(false, ("CountryId =", root, "not found in m_countries."));
    return;
  }
  rootNode->ForEachInSubtree([&toDo](CountryTree::Node const & node) {
    Country const & value = node.Value();
    toDo(value.Name(), value.GetSubtreeMwmCounter() != 1 /* groupNode. */);
  });
}

/// Calls functor |toDo| with signature
/// void(const CountryId const & parentId, CountriesVec const & descendantCountryId)
/// for each ancestor except for the main root of the tree in order from the leaf to the root.
/// Note. In case of disputable territories several nodes with the same name may be
/// present in the country tree. In that case ForEachAncestorExceptForTheRoot calls
/// |toDo| for parents of each way to the root in the country tree. In case of diamond
/// trees toDo is called for common part of ways to the root only once.
template <class ToDo>
void Storage::ForEachAncestorExceptForTheRoot(CountryId const & countryId, ToDo && toDo) const
{
  std::vector<CountryTree::Node const *> nodes;
  m_countries.Find(countryId, nodes);
  if (nodes.empty())
  {
    ASSERT(false, ("CountryId =", countryId, "not found in m_countries."));
    return;
  }

  ForEachAncestorExceptForTheRoot(nodes, std::forward<ToDo>(toDo));
}

template <class ToDo>
void Storage::ForEachAncestorExceptForTheRoot(std::vector<CountryTree::Node const *> const & nodes,
                                              ToDo && toDo) const
{
  std::set<CountryTree::Node const *> visitedAncestors;
  // In most cases nodes.size() == 1. In case of disputable territories nodes.size()
  // may be more than one. It means |childId| is present in the country tree more than once.
  for (auto const & node : nodes)
  {
    node->ForEachAncestorExceptForTheRoot([&](CountryTree::Node const & node) {
      CountryId const ancestorId = node.Value().Name();
      if (visitedAncestors.find(&node) != visitedAncestors.end())
        return;  // The node was visited before because countryId is present in the tree more
                 // than once.
      visitedAncestors.insert(&node);
      toDo(ancestorId, node);
    });
  }
}

template <class ToDo>
void Storage::ForEachCountryFile(ToDo && toDo) const
{
  m_countries.GetRoot().ForEachInSubtree([&](CountryTree::Node const & node) {
    if (node.ChildrenCount() == 0)
      toDo(node.Value().GetFile());
  });
}
}  // namespace storage
