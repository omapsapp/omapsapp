#pragma once

#include "editor/config_loader.hpp"
#include "editor/editor_config.hpp"
#include "editor/editor_notes.hpp"
#include "editor/editor_storage.hpp"
#include "editor/new_feature_categories.hpp"
#include "editor/xml_feature.hpp"

#include "indexer/editable_map_object.hpp"
#include "indexer/edit_journal.hpp"
#include "indexer/feature.hpp"
#include "indexer/feature_source.hpp"
#include "indexer/mwm_set.hpp"

#include "geometry/rect2d.hpp"

#include "base/atomic_shared_ptr.hpp"
#include "base/thread_checker.hpp"
#include "base/timer.hpp"

#include <atomic>
#include <cstdint>
#include <ctime>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace editor::testing
{
class EditorTest;
}  // namespace editor::testing
namespace editor
{
class XMLFeature;
}  // namespace editor

namespace osm
{
// NOTE: this class is thead-safe for read operations,
// but write operations should be called on main thread only.
class Editor final : public MwmSet::Observer
{
  friend class editor::testing::EditorTest;

  Editor();

public:
  using FeatureTypeFn = std::function<void(FeatureType & ft)>;
  using InvalidateFn = std::function<void()>;
  using ForEachFeaturesNearByFn = std::function<void(FeatureTypeFn && fn, m2::PointD const & mercator)>;
  using MwmId = MwmSet::MwmId;

  struct Delegate
  {
    virtual ~Delegate() = default;

    virtual MwmId GetMwmIdByMapName(std::string const & name) const = 0;
    virtual std::unique_ptr<EditableMapObject> GetOriginalMapObject(FeatureID const & fid) const = 0;
    virtual std::string GetOriginalFeatureStreet(FeatureID const & fid) const = 0;
    virtual void ForEachFeatureAtPoint(FeatureTypeFn && fn, m2::PointD const & point) const = 0;
  };

  enum class UploadResult
  {
    Success,
    Error,
    NothingToUpload
  };

  using FinishUploadCallback = std::function<void(UploadResult)>;

  enum class SaveResult
  {
    NothingWasChanged,
    SavedSuccessfully,
    NoFreeSpaceError,
    NoUnderlyingMapError,
    SavingError
  };

  enum class NoteProblemType
  {
    General,
    PlaceDoesNotExist
  };

  struct Stats
  {
    /// <id, feature status string>
    std::vector<std::pair<FeatureID, std::string>> m_edits;
    size_t m_uploadedCount = 0;
    time_t m_lastUploadTimestamp = base::INVALID_TIME_STAMP;
  };

  static Editor & Instance();

  void SetDelegate(std::unique_ptr<Delegate> delegate) { m_delegate = std::move(delegate); }

  void SetStorageForTesting(std::unique_ptr<editor::StorageBase> storage) { m_storage = std::move(storage); }

  void ResetNotes() { m_notes = editor::Notes::MakeNotes(); }

  void SetDefaultStorage();

  void SetInvalidateFn(InvalidateFn const & fn) { m_invalidateFn = fn; }

  void LoadEdits();
  /// Resets editor to initial state: no any edits or created/deleted features.
  void ClearAllLocalEdits();

  // MwmSet::Observer overrides:
  void OnMapRegistered(platform::LocalCountryFile const & localFile) override;

  using FeatureIndexFunctor = std::function<void(uint32_t)>;
  void ForEachCreatedFeature(MwmId const & id, FeatureIndexFunctor const & f, m2::RectD const & rect, int scale) const;

  /// Easy way to check if a feature was deleted, modified, created or not changed at all.
  FeatureStatus GetFeatureStatus(MwmId const & mwmId, uint32_t index) const;
  FeatureStatus GetFeatureStatus(FeatureID const & fid) const;

  /// @returns true if a feature was uploaded to osm.
  bool IsFeatureUploaded(MwmId const & mwmId, uint32_t index) const;

  /// Marks feature as "deleted" from MwM file.
  void DeleteFeature(FeatureID const & fid);

  /// @returns empty object if feature wasn't edited.
  std::optional<osm::EditableMapObject> GetEditedFeature(FeatureID const & fid) const;

  /// @returns empty object if feature wasn't edited.
  std::optional<osm::EditJournal> GetEditedFeatureJournal(FeatureID const & fid) const;

  /// @returns false if feature wasn't edited.
  /// @param outFeatureStreet is valid only if true was returned.
  bool GetEditedFeatureStreet(FeatureID const & fid, std::string & outFeatureStreet) const;

  /// @returns sorted features indices with specified status.
  std::vector<uint32_t> GetFeaturesByStatus(MwmId const & mwmId, FeatureStatus status) const;

  /// Editor checks internally if any feature params were actually edited.
  SaveResult SaveEditedFeature(EditableMapObject const & emo);

  /// Removes changes from editor.
  /// @returns false if a feature was uploaded.
  bool RollBackChanges(FeatureID const & fid);

  EditableProperties GetEditableProperties(FeatureType & feature) const;

  bool HaveMapEditsOrNotesToUpload() const;
  bool HaveMapEditsToUpload(MwmId const & mwmId) const;

  using ChangesetTags = std::map<std::string, std::string>;
  /// Tries to upload all local changes to OSM server in a separate thread.
  /// @param[in] tags should provide additional information about client to use in changeset.
  void UploadChanges(std::string const & oauthToken, ChangesetTags tags,
                     FinishUploadCallback callBack = FinishUploadCallback());
  // TODO(mgsergio): Test new types from new config but with old classificator (where these types are absent).
  // Editor should silently ignore all types in config which are unknown to him.
  NewFeatureCategories GetNewFeatureCategories() const;

  bool CreatePoint(uint32_t type, m2::PointD const & mercator, MwmId const & id, EditableMapObject & outFeature) const;

  void CreateNote(ms::LatLon const & latLon, FeatureID const & fid, feature::TypesHolder const & holder,
                  std::string_view defaultName, NoteProblemType type, std::string_view note);

  Stats GetStats() const;

  // Don't use this function to determine if a feature in editor was created.
  // Use GetFeatureStatus(fid) instead. This function is used when a feature is
  // not yet saved, and we have to know if it was modified or created.
  static bool IsCreatedFeature(FeatureID const & fid);

private:
  // TODO(a): Use this structure as part of FeatureTypeInfo.
  struct UploadInfo
  {
    time_t m_uploadAttemptTimestamp = base::INVALID_TIME_STAMP;
    /// Is empty if upload has never occurred or one of k* constants above otherwise.
    std::string m_uploadStatus;
    std::string m_uploadError;
  };

  struct FeatureTypeInfo
  {
    FeatureStatus m_status = FeatureStatus::Untouched;
    EditableMapObject m_object;
    /// If not empty contains Feature's addr:street, edited by user.
    std::string m_street;
    time_t m_modificationTimestamp = base::INVALID_TIME_STAMP;
    time_t m_uploadAttemptTimestamp = base::INVALID_TIME_STAMP;
    /// Is empty if upload has never occurred or one of k* constants above otherwise.
    std::string m_uploadStatus;
    std::string m_uploadError;
  };

  using FeaturesContainer = std::map<MwmId, std::map<uint32_t, FeatureTypeInfo>>;

  /// @returns false if fails.
  bool Save(FeaturesContainer const & features) const;
  bool SaveTransaction(std::shared_ptr<FeaturesContainer> const & features);
  bool RemoveFeatureIfExists(FeatureID const & fid);
  /// Notify framework that something has changed and should be redisplayed.
  void Invalidate();

  // Saves a feature in internal storage with FeatureStatus::Obsolete status.
  bool MarkFeatureAsObsolete(FeatureID const & fid);
  bool RemoveFeature(FeatureID const & fid);

  FeatureID GenerateNewFeatureId(FeaturesContainer const & features, MwmId const & id) const;
  EditableProperties GetEditablePropertiesForTypes(feature::TypesHolder const & types) const;

  bool FillFeatureInfo(FeatureStatus status, editor::XMLFeature const & xml, FeatureID const & fid,
                       FeatureTypeInfo & fti) const;
  /// @returns pointer to m_features[id][index] if exists, nullptr otherwise.
  static FeatureTypeInfo const * GetFeatureTypeInfo(FeaturesContainer const & features, MwmId const & mwmId,
                                                    uint32_t index);
  void SaveUploadedInformation(FeatureID const & fid, UploadInfo const & fromUploader);

  void MarkFeatureWithStatus(FeaturesContainer & editableFeatures, FeatureID const & fid, FeatureStatus status);

  // These methods are just checked wrappers around Delegate.
  MwmId GetMwmIdByMapName(std::string const & name);
  std::unique_ptr<EditableMapObject> GetOriginalMapObject(FeatureID const & fid) const;
  std::string GetOriginalFeatureStreet(FeatureID const & fid) const;
  void ForEachFeatureAtPoint(FeatureTypeFn && fn, m2::PointD const & point) const;
  FeatureID GetFeatureIdByXmlFeature(FeaturesContainer const & features, editor::XMLFeature const & xml,
                                     MwmId const & mwmId, FeatureStatus status, bool needMigrate) const;
  void LoadMwmEdits(FeaturesContainer & loadedFeatures, pugi::xml_node const & mwm,
                    MwmId const & mwmId, bool needMigrate);

  static bool HaveMapEditsToUpload(FeaturesContainer const & features);

  static FeatureStatus GetFeatureStatusImpl(FeaturesContainer const & features, MwmId const & mwmId, uint32_t index);

  static bool IsFeatureUploadedImpl(FeaturesContainer const & features, MwmId const & mwmId, uint32_t index);

  void UpdateXMLFeatureTags(editor::XMLFeature & feature, std::list<JournalEntry> const & journal);

  /// Deleted, edited and created features.
  base::AtomicSharedPtr<FeaturesContainer> m_features;

  std::unique_ptr<Delegate> m_delegate;

  /// Invalidate map viewport after edits.
  InvalidateFn m_invalidateFn;

  /// Contains information about what and how can be edited.
  base::AtomicSharedPtr<editor::EditorConfig> m_config;
  editor::ConfigLoader m_configLoader;

  /// Notes to be sent to osm.
  std::shared_ptr<editor::Notes> m_notes;

  std::unique_ptr<editor::StorageBase> m_storage;

  std::atomic<bool> m_isUploadingNow;

  DECLARE_THREAD_CHECKER(MainThreadChecker);
};  // class Editor

std::string DebugPrint(Editor::SaveResult saveResult);
}  // namespace osm
