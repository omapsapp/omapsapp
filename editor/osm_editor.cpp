#include "editor/osm_editor.hpp"

#include "editor/changeset_wrapper.hpp"
#include "editor/edits_migration.hpp"
#include "editor/osm_auth.hpp"
#include "editor/xml_feature.hpp"

#include "indexer/fake_feature_ids.hpp"
#include "indexer/feature_decl.hpp"
#include "indexer/feature_meta.hpp"
#include "indexer/feature_source.hpp"
#include "indexer/mwm_set.hpp"

#include "platform/local_country_file_utils.hpp"
#include "platform/platform.hpp"

#include "base/exception.hpp"
#include "base/logging.hpp"
#include "base/stl_helpers.hpp"
#include "base/thread_checker.hpp"
#include "base/timer.hpp"

#include <algorithm>
#include <array>
#include <sstream>

#include "3party/opening_hours/opening_hours.hpp"
#include <pugixml.hpp>
#include <utility>

namespace osm
{
// TODO(AlexZ): Normalize osm multivalue strings for correct merging
// (e.g. insert/remove spaces after ';' delimiter);
using namespace pugi;
using editor::XMLFeature;
using std::make_shared, std::string;

namespace
{
constexpr char const * kXmlRootNode = "omaps";
constexpr char const * kXmlMwmNode = "mwm";
constexpr char const * kDeleteSection = "delete";
constexpr char const * kModifySection = "modify";
constexpr char const * kCreateSection = "create";
constexpr char const * kObsoleteSection = "obsolete";
/// We store edited streets in OSM-compatible way.
constexpr char const * kAddrStreetTag = "addr:street";

constexpr char const * kUploaded = "Uploaded";
constexpr char const * kDeletedFromOSMServer = "Deleted from OSM by someone";
constexpr char const * kNeedsRetry = "Needs Retry";
constexpr char const * kMatchedFeatureIsEmpty = "Matched feature has no tags";

struct XmlSection
{
  constexpr XmlSection(FeatureStatus status, char const * sectionName)
    : m_status(status), m_sectionName(sectionName)
  {
  }

  FeatureStatus m_status = FeatureStatus::Untouched;
  char const * m_sectionName;
};

std::array<XmlSection, 4> constexpr kXmlSections = {
    {{FeatureStatus::Deleted, kDeleteSection},
    {FeatureStatus::Modified, kModifySection},
    {FeatureStatus::Obsolete, kObsoleteSection},
    {FeatureStatus::Created, kCreateSection}}
};

struct LogHelper
{
  explicit LogHelper(MwmSet::MwmId const & mwmId) : m_mwmId(mwmId) {}

  ~LogHelper()
  {
    LOG(LINFO, ("For", m_mwmId, ". Was loaded", m_modified, "modified,", m_created, "created,",
                m_deleted, "deleted and", m_obsolete, "obsolete features."));
  }

  void OnStatus(FeatureStatus status)
  {
    switch (status)
    {
    case FeatureStatus::Deleted: ++m_deleted; break;
    case FeatureStatus::Modified: ++m_modified; break;
    case FeatureStatus::Obsolete: ++m_obsolete; break;
    case FeatureStatus::Created: ++m_created; break;
    case FeatureStatus::Untouched: ASSERT(false, ());
    }
  }

  uint32_t m_deleted = 0;
  uint32_t m_obsolete = 0;
  uint32_t m_modified = 0;
  uint32_t m_created = 0;
  MwmSet::MwmId const & m_mwmId;
};

bool NeedsUpload(string const & uploadStatus)
{
  return uploadStatus != kUploaded && uploadStatus != kDeletedFromOSMServer &&
         uploadStatus != kMatchedFeatureIsEmpty;
}

XMLFeature GetMatchingFeatureFromOSM(osm::ChangesetWrapper & cw, const osm::EditableMapObject & o)
{
  ASSERT_NOT_EQUAL(o.GetGeomType(), feature::GeomType::Line,
                   ("Line features are not supported yet."));
  if (o.GetGeomType() == feature::GeomType::Point)
    return cw.GetMatchingNodeFeatureFromOSM(o.GetMercator());

  auto const & geometry = o.GetTriangesAsPoints();

  ASSERT_GREATER_OR_EQUAL(geometry.size(), 3, ("Is it an area feature?"));

  return cw.GetMatchingAreaFeatureFromOSM(geometry);
}

uint64_t GetMwmCreationTimeByMwmId(MwmSet::MwmId const & mwmId)
{
  return mwmId.GetInfo()->m_version.GetSecondsSinceEpoch();
}

bool IsObsolete(editor::XMLFeature const & xml, FeatureID const & fid)
{
  // TODO(mgsergio): If xml and feature are identical return true
  auto const uploadTime = xml.GetUploadTime();
  return uploadTime != base::INVALID_TIME_STAMP &&
         base::TimeTToSecondsSinceEpoch(uploadTime) < GetMwmCreationTimeByMwmId(fid.m_mwmId);
}
}  // namespace

Editor::Editor()
  : m_configLoader(m_config)
  , m_notes(editor::Notes::MakeNotes())
  , m_isUploadingNow(false)
{
  SetDefaultStorage();
}

Editor & Editor::Instance()
{
  static Editor instance;
  return instance;
}

void Editor::SetDefaultStorage() { m_storage = std::make_unique<editor::LocalStorage>(); }

void Editor::LoadEdits()
{
  CHECK_THREAD_CHECKER(MainThreadChecker, ());
  if (!m_delegate)
  {
    LOG(LERROR, ("Can't load any map edits, delegate has not been set."));
    return;
  }

  xml_document doc;
  bool needRewriteEdits = false;

  if (!m_storage->Load(doc))
    return;

  m_features.Set(make_shared<FeaturesContainer>());
  auto loadedFeatures = make_shared<FeaturesContainer>();

  auto rootNode = doc.child(kXmlRootNode);
  // Migrate clients with an old root node.
  if (!rootNode)
    rootNode = doc.child("mapsme");
  // TODO: Empty rootNode is an OK case for the current logic and unit tests. Check if there is a better way to do it.
  for (auto const & mwm : rootNode.children(kXmlMwmNode))
  {
    string const mapName = mwm.attribute("name").as_string("");
    int64_t const mapVersion = mwm.attribute("version").as_llong(0);
    auto const mwmId = GetMwmIdByMapName(mapName);

    // TODO(mgsergio, milchakov): |mapName| may change between launches.
    // The right thing to do here is to try to migrate all changes anyway.
    if (!mwmId.IsAlive())
    {
      LOG(LINFO, ("Mwm", mapName, "was deleted"));
      needRewriteEdits = true;
      continue;
    }

    auto const needMigrateEdits = mapVersion != mwmId.GetInfo()->GetVersion();
    needRewriteEdits = needRewriteEdits || needMigrateEdits;

    LoadMwmEdits(*loadedFeatures, mwm, mwmId, needMigrateEdits);
  }
  // Save edits with new indexes and mwm version to avoid another migration on next startup.
  if (needRewriteEdits)
    SaveTransaction(loadedFeatures);
  else
    m_features.Set(loadedFeatures);
}

bool Editor::Save(FeaturesContainer const & features) const
{
  if (features.empty())
    return m_storage->Reset();

  xml_document doc;
  xml_node root = doc.append_child(kXmlRootNode);
  // Use format_version for possible future format changes.
  root.append_attribute("format_version") = 1;
  for (auto const & mwm : features)
  {
    if (!mwm.first.IsAlive())
      continue;

    xml_node mwmNode = root.append_child(kXmlMwmNode);
    mwmNode.append_attribute("name") = mwm.first.GetInfo()->GetCountryName().c_str();
    mwmNode.append_attribute("version") = static_cast<long long>(mwm.first.GetInfo()->GetVersion());
    xml_node const deleted = mwmNode.append_child(kDeleteSection);
    xml_node const modified = mwmNode.append_child(kModifySection);
    xml_node const created = mwmNode.append_child(kCreateSection);
    xml_node const obsolete = mwmNode.append_child(kObsoleteSection);
    for (auto & index : mwm.second)
    {
      FeatureTypeInfo const & fti = index.second;
      // TODO: Do we really need to serialize deleted features in full details? Looks like mwm ID
      // and meta fields are enough.
      XMLFeature xf =
          editor::ToXML(fti.m_object, true /* type serializing helps during migration */);
      xf.SetEditJournal(fti.m_object.GetJournal());
      xf.SetMWMFeatureIndex(index.first);
      if (!fti.m_street.empty())
        xf.SetTagValue(kAddrStreetTag, fti.m_street);
      ASSERT_NOT_EQUAL(0, fti.m_modificationTimestamp, ());
      xf.SetModificationTime(fti.m_modificationTimestamp);
      if (fti.m_uploadAttemptTimestamp != base::INVALID_TIME_STAMP)
      {
        xf.SetUploadTime(fti.m_uploadAttemptTimestamp);
        ASSERT(!fti.m_uploadStatus.empty(), ("Upload status updates with upload timestamp."));
        xf.SetUploadStatus(fti.m_uploadStatus);
        if (!fti.m_uploadError.empty())
          xf.SetUploadError(fti.m_uploadError);
      }
      switch (fti.m_status)
      {
      case FeatureStatus::Deleted: VERIFY(xf.AttachToParentNode(deleted), ()); break;
      case FeatureStatus::Modified: VERIFY(xf.AttachToParentNode(modified), ()); break;
      case FeatureStatus::Created: VERIFY(xf.AttachToParentNode(created), ()); break;
      case FeatureStatus::Obsolete: VERIFY(xf.AttachToParentNode(obsolete), ()); break;
      case FeatureStatus::Untouched: CHECK(false, ("Not edited features shouldn't be here."));
      }
    }
  }

  return m_storage->Save(doc);
}

bool Editor::SaveTransaction(std::shared_ptr<FeaturesContainer> const & features)
{
  if (!Save(*features))
    return false;

  m_features.Set(features);
  return true;
}

void Editor::ClearAllLocalEdits()
{
  CHECK_THREAD_CHECKER(MainThreadChecker, (""));

  SaveTransaction(make_shared<FeaturesContainer>());
  Invalidate();
}

void Editor::OnMapRegistered(platform::LocalCountryFile const &)
{
  // todo(@a, @m) Reloading edits only for |localFile| should be enough.
  LoadEdits();
}

FeatureStatus Editor::GetFeatureStatus(MwmId const & mwmId, uint32_t index) const
{
  auto const features = m_features.Get();
  return GetFeatureStatusImpl(*features, mwmId, index);
}

FeatureStatus Editor::GetFeatureStatus(FeatureID const & fid) const
{
  auto const features = m_features.Get();
  return GetFeatureStatusImpl(*features, fid.m_mwmId, fid.m_index);
}

bool Editor::IsFeatureUploaded(MwmId const & mwmId, uint32_t index) const
{
  auto const features = m_features.Get();
  return IsFeatureUploadedImpl(*features, mwmId, index);
}

void Editor::DeleteFeature(FeatureID const & fid)
{
  CHECK_THREAD_CHECKER(MainThreadChecker, (""));

  auto const features = m_features.Get();
  auto editableFeatures = make_shared<FeaturesContainer>(*features);

  auto const mwm = editableFeatures->find(fid.m_mwmId);

  if (mwm != editableFeatures->end())
  {
    auto const f = mwm->second.find(fid.m_index);
    // Created feature is deleted by removing all traces of it.
    if (f != mwm->second.end() && f->second.m_status == FeatureStatus::Created)
    {
      mwm->second.erase(f);
      SaveTransaction(editableFeatures);
      return;
    }
  }

  MarkFeatureWithStatus(*editableFeatures, fid, FeatureStatus::Deleted);
  SaveTransaction(editableFeatures);
  Invalidate();
}

bool Editor::IsCreatedFeature(FeatureID const & fid)
{
  return feature::FakeFeatureIds::IsEditorCreatedFeature(fid.m_index);
}

/// Several cases should be handled while saving changes:
/// 1) a feature is not in editor's cache
///   I. a feature was created
///      save it and mark as `Created'
///   II. a feature was modified
///      save it and mark as `Modified'
/// 2) a feature is in editor's cache
///   I. a feature was created
///      save it and mark as `Created'
///   II. a feature was modified and equals to the one in cache
///      ignore it
///   III. a feature was modified and equals to the one in mwm
///      either delete it or save and mark as `Modified' depending on upload status
Editor::SaveResult Editor::SaveEditedFeature(EditableMapObject const & emo)
{
  CHECK_THREAD_CHECKER(MainThreadChecker, (""));

  FeatureID const & fid = emo.GetID();
  FeatureTypeInfo fti;

  auto const features = m_features.Get();

  auto const featureStatus = GetFeatureStatusImpl(*features, fid.m_mwmId, fid.m_index);
  ASSERT_NOT_EQUAL(featureStatus, FeatureStatus::Obsolete, ("Obsolete feature cannot be modified."));
  ASSERT_NOT_EQUAL(featureStatus, FeatureStatus::Deleted, ("Unexpected feature status."));

  bool const wasCreatedByUser = IsCreatedFeature(fid);
  if (wasCreatedByUser)
  {
    fti.m_status = FeatureStatus::Created;
    fti.m_object = emo;

    if (featureStatus == FeatureStatus::Created)
    {
      auto const & editedFeatureInfo = features->at(fid.m_mwmId).at(fid.m_index);
      if (AreObjectsEqualIgnoringStreet(fti.m_object, editedFeatureInfo.m_object) &&
          emo.GetStreet().m_defaultName == editedFeatureInfo.m_street)
      {
        return SaveResult::NothingWasChanged;
      }
    }
  }
  else
  {
    auto const originalObjectPtr = GetOriginalMapObject(fid);
    if (!originalObjectPtr)
    {
      LOG(LERROR, ("A feature with id", fid, "cannot be loaded."));
      return SaveResult::SavingError;
    }
    fti.m_object = emo;
    bool const sameAsInMWM =
        AreObjectsEqualIgnoringStreet(fti.m_object, *originalObjectPtr) &&
        emo.GetStreet().m_defaultName == GetOriginalFeatureStreet(fti.m_object.GetID());

    if (featureStatus != FeatureStatus::Untouched)
    {
      // A feature was modified and equals to the one in editor.
      auto const & editedFeatureInfo = features->at(fid.m_mwmId).at(fid.m_index);
      if (AreObjectsEqualIgnoringStreet(fti.m_object, editedFeatureInfo.m_object) &&
          emo.GetStreet().m_defaultName == editedFeatureInfo.m_street)
      {
        return SaveResult::NothingWasChanged;
      }

      // A feature was modified and equals to the one in mwm (changes are rolled back).
      if (sameAsInMWM)
      {
        // Feature was not yet uploaded. Since it's equal to one mwm we can remove changes.
        if (editedFeatureInfo.m_uploadStatus != kUploaded)
        {
          if (!RemoveFeature(fid))
            return SaveResult::SavingError;

          return SaveResult::SavedSuccessfully;
        }
      }

      // If a feature is not the same as in mwm, or if it was uploaded,
      // we must save it and mark for upload.
    }
    // A feature was NOT edited before and current changes are useless.
    else if (sameAsInMWM)
    {
      return SaveResult::NothingWasChanged;
    }

    fti.m_status = FeatureStatus::Modified;
  }

  // TODO: What if local client time is absolutely wrong?
  fti.m_modificationTimestamp = time(nullptr);
  fti.m_street = emo.GetStreet().m_defaultName;

  // Reset upload status so already uploaded features can be uploaded again after modification.
  fti.m_uploadStatus = {};

  auto editableFeatures = make_shared<FeaturesContainer>(*features);
  (*editableFeatures)[fid.m_mwmId][fid.m_index] = std::move(fti);

  bool const savedSuccessfully = SaveTransaction(editableFeatures);

  Invalidate();
  return savedSuccessfully ? SaveResult::SavedSuccessfully : SaveResult::NoFreeSpaceError;
}

bool Editor::RollBackChanges(FeatureID const & fid)
{
  CHECK_THREAD_CHECKER(MainThreadChecker, (""));

  if (IsFeatureUploaded(fid.m_mwmId, fid.m_index))
    return false;

  return RemoveFeature(fid);
}

void Editor::ForEachCreatedFeature(MwmId const & id, FeatureIndexFunctor const & f,
                                   m2::RectD const & rect, int /*scale*/) const
{
  auto const features = m_features.Get();

  auto const mwmFound = features->find(id);
  if (mwmFound == features->cend())
    return;

  // Process only new (created) features.
  for (auto const & index : mwmFound->second)
  {
    FeatureTypeInfo const & ftInfo = index.second;
    if (ftInfo.m_status == FeatureStatus::Created)
    {
      if (rect.IsPointInside(ftInfo.m_object.GetMercator()))
        f(index.first);
    }
  }
}

std::optional<osm::EditableMapObject> Editor::GetEditedFeature(FeatureID const & fid) const
{
  auto const features = m_features.Get();
  auto const * featureInfo = GetFeatureTypeInfo(*features, fid.m_mwmId, fid.m_index);
  if (featureInfo == nullptr)
    return {};

  return featureInfo->m_object;
}

std::optional<osm::EditJournal> Editor::GetEditedFeatureJournal(FeatureID const & fid) const
{
  auto const features = m_features.Get();
  auto const * featureInfo = GetFeatureTypeInfo(*features, fid.m_mwmId, fid.m_index);
  if (featureInfo == nullptr)
    return {};

  return featureInfo->m_object.GetJournal();
}

bool Editor::GetEditedFeatureStreet(FeatureID const & fid, string & outFeatureStreet) const
{
  auto const features = m_features.Get();
  auto const * featureInfo = GetFeatureTypeInfo(*features, fid.m_mwmId, fid.m_index);
  if (featureInfo == nullptr)
    return false;

  outFeatureStreet = featureInfo->m_street;
  return true;
}

std::vector<uint32_t> Editor::GetFeaturesByStatus(MwmId const & mwmId,
                                                  FeatureStatus status) const
{
  auto const features = m_features.Get();

  std::vector<uint32_t> result;
  auto const matchedMwm = features->find(mwmId);
  if (matchedMwm == features->cend())
    return result;

  for (auto const & index : matchedMwm->second)
  {
    if (index.second.m_status == status)
      result.push_back(index.first);
  }
  std::sort(result.begin(), result.end());
  return result;
}

EditableProperties Editor::GetEditableProperties(FeatureType & feature) const
{
  auto const features = m_features.Get();

  auto const & fid = feature.GetID();
  auto const featureStatus = GetFeatureStatusImpl(*features, fid.m_mwmId, fid.m_index);

  ASSERT(featureStatus != FeatureStatus::Obsolete,
         ("Edit mode should not be available on obsolete features"));

  // TODO(mgsergio): Check if feature is in the area where editing is disabled in the config.
  auto editableProperties = GetEditablePropertiesForTypes(feature::TypesHolder(feature));

  // Disable opening hours editing if opening hours cannot be parsed.
  if (featureStatus != FeatureStatus::Created)
  {
    auto const originalObjectPtr = GetOriginalMapObject(fid);
    if (!originalObjectPtr)
    {
      LOG(LERROR, ("A feature with id", fid, "cannot be loaded."));
      return {};
    }

    /// @todo Avoid temporary string when OpeningHours (boost::spirit) will allow string_view.
    string const featureOpeningHours(originalObjectPtr->GetOpeningHours());
    /// @note Empty string is parsed as a valid opening hours rule.
    if (!osmoh::OpeningHours(featureOpeningHours).IsValid())
    {
      auto & meta = editableProperties.m_metadata;
      meta.erase(remove(begin(meta), end(meta), feature::Metadata::FMD_OPEN_HOURS), end(meta));
    }
  }

  return editableProperties;
}

EditableProperties Editor::GetEditablePropertiesForTypes(feature::TypesHolder const & types) const
{
  editor::TypeAggregatedDescription desc;
  if (m_config.Get()->GetTypeDescription(types.ToObjectNames(), desc))
  {
    return { std::move(desc.m_editableFields), desc.IsNameEditable(),
             desc.IsAddressEditable(), desc.IsCuisineEditable() };
  }
  return {};
}

bool Editor::HaveMapEditsOrNotesToUpload() const
{
  if (m_notes->NotUploadedNotesCount() != 0)
    return true;

  auto const features = m_features.Get();
  return HaveMapEditsToUpload(*features);
}

bool Editor::HaveMapEditsToUpload(MwmId const & mwmId) const
{
  if (!mwmId.IsAlive())
    return false;

  auto const features = m_features.Get();

  auto const found = features->find(mwmId);
  if (found != features->cend())
  {
    for (auto const & index : found->second)
    {
      if (NeedsUpload(index.second.m_uploadStatus))
        return true;
    }
  }
  return false;
}

void Editor::UploadChanges(string const & oauthToken, ChangesetTags tags,
                           FinishUploadCallback callback)
{
  if (m_notes->NotUploadedNotesCount())
    m_notes->Upload(OsmOAuth::ServerAuth(oauthToken));

  auto const features = m_features.Get();

  if (!HaveMapEditsToUpload(*features))
  {
    LOG(LDEBUG, ("There are no local edits to upload."));
    return;
  }

  auto upload = [this](string secret, ChangesetTags tags, FinishUploadCallback callback)
  {
    int uploadedFeaturesCount = 0, errorsCount = 0;
    ChangesetWrapper changeset(secret, std::move(tags));
    auto const features = m_features.Get();

    for (auto const & id : *features)
    {
      if (!id.first.IsAlive())
        continue;

      for (auto const & index : id.second)
      {
        FeatureTypeInfo const & fti = index.second;
        // Do not process already uploaded features or those failed permanently.
        if (!NeedsUpload(fti.m_uploadStatus))
          continue;

        // TODO(a): Use UploadInfo as part of FeatureTypeInfo.
        UploadInfo uploadInfo = {fti.m_uploadAttemptTimestamp, fti.m_uploadStatus, fti.m_uploadError};

        LOG(LDEBUG, ("Content of editJournal:\n", fti.m_object.GetJournal().JournalToString()));

        // Don't use new editor for Legacy Objects
        auto const & journalHistory = fti.m_object.GetJournal().GetJournalHistory();
        bool useNewEditor = journalHistory.empty() || journalHistory.front().journalEntryType != JournalEntryType::LegacyObject;

        try
        {
          if (useNewEditor)
          {
            LOG(LDEBUG, ("New Editor used\n"));

            switch (fti.m_status)
            {
              case FeatureStatus::Untouched:
                CHECK(false, ("It's impossible."));
                continue;
              case FeatureStatus::Obsolete:
                continue;  // Obsolete features will be deleted by OSMers.
              case FeatureStatus::Created: // fallthrough
              case FeatureStatus::Modified:
              {
                std::list<JournalEntry> const & journal = fti.m_object.GetJournal().GetJournal();

                switch (fti.m_object.GetEditingLifecycle())
                {
                  case EditingLifecycle::CREATED:
                  {
                    // Generate XMLFeature for new object
                    JournalEntry const & createEntry = journal.front();
                    ASSERT(createEntry.journalEntryType == JournalEntryType::ObjectCreated,("First item should have type ObjectCreated"));
                    ObjCreateData const & objCreateData = std::get<ObjCreateData>(createEntry.data);
                    XMLFeature feature = editor::TypeToXML(objCreateData.type, objCreateData.geomType, objCreateData.mercator);

                    // Check if place already exists
                    bool mergeSameLocation = false;
                    try
                    {
                      XMLFeature osmFeature = changeset.GetMatchingNodeFeatureFromOSM(objCreateData.mercator);
                      if (objCreateData.mercator == osmFeature.GetMercatorCenter()) {
                        changeset.AddChangesetTag("info:merged_same_location", "yes");
                        feature = osmFeature;
                        mergeSameLocation = true;
                      }
                      else
                      {
                        changeset.AddChangesetTag("info:feature_close_by", "yes");
                      }
                    }
                    catch (ChangesetWrapper::OsmObjectWasDeletedException const &) {}
                    catch (ChangesetWrapper::EmptyFeatureException const &) {}

                    // Add tags to XMLFeature
                    UpdateXMLFeatureTags(feature, journal);

                    // Upload XMLFeature to OSM
                    LOG(LDEBUG, ("CREATE Feature (newEditor)", feature));
                    changeset.AddChangesetTag("info:new_editor", "yes");
                    if (!mergeSameLocation)
                      changeset.Create(feature);
                    else
                      changeset.Modify(feature);
                    break;
                  }

                  case EditingLifecycle::MODIFIED:
                  {
                    // Load existing OSM object (Throws, see catch below)
                    XMLFeature feature = GetMatchingFeatureFromOSM(changeset, fti.m_object);

                    // Update tags of XMLFeature
                    UpdateXMLFeatureTags(feature, journal);

                    // Upload XMLFeature to OSM
                    LOG(LDEBUG, ("MODIFIED Feature (newEditor)", feature));
                    changeset.AddChangesetTag("info:new_editor", "yes");
                    changeset.Modify(feature);
                    break;
                  }

                  case EditingLifecycle::IN_SYNC:
                  {
                    CHECK(false, ("Object already IN_SYNC should not be here"));
                    continue;
                  }
                }
                break;
              }
              case FeatureStatus::Deleted:
                auto const originalObjectPtr = GetOriginalMapObject(fti.m_object.GetID());
                if (!originalObjectPtr)
                {
                  LOG(LERROR, ("A feature with id", fti.m_object.GetID(), "cannot be loaded."));
                  GetPlatform().RunTask(Platform::Thread::Gui,[this, fid = fti.m_object.GetID()]() {
                    RemoveFeatureIfExists(fid);
                  });
                  continue;
                }
                changeset.Delete(GetMatchingFeatureFromOSM(changeset, *originalObjectPtr));
                break;
            }
          }
          else // Use old editor
          {
            // Todo: Remove old editor after transition period
            switch (fti.m_status)
            {
              case FeatureStatus::Untouched: CHECK(false, ("It's impossible.")); continue;
              case FeatureStatus::Obsolete: continue;  // Obsolete features will be deleted by OSMers.
              case FeatureStatus::Created:
              {
                XMLFeature feature = editor::ToXML(fti.m_object, true);
                if (!fti.m_street.empty())
                  feature.SetTagValue(kAddrStreetTag, fti.m_street);

                ASSERT_EQUAL(feature.GetType(), XMLFeature::Type::Node,
                             ("Linear and area features creation is not supported yet."));
                try
                {
                  auto const center = fti.m_object.GetMercator();
                  // Throws, see catch below.
                  XMLFeature osmFeature = changeset.GetMatchingNodeFeatureFromOSM(center);

                  // If we are here, it means that object already exists at the given point.
                  // To avoid nodes duplication, merge and apply changes to it instead of creating a new one.
                  XMLFeature const osmFeatureCopy = osmFeature;
                  osmFeature.ApplyPatch(feature);
                  // Check to avoid uploading duplicates into OSM.
                  if (osmFeature == osmFeatureCopy)
                  {
                    LOG(LWARNING,("Local changes are equal to OSM, feature has not been uploaded.", osmFeatureCopy));
                    // Don't delete this local change right now for user to see it in profile.
                    // It will be automatically deleted by migration code on the next maps update.
                  }
                  else
                  {
                    LOG(LDEBUG, ("Create case: uploading patched feature", osmFeature));
                    changeset.AddChangesetTag("info:old_editor", "yes");
                    changeset.AddChangesetTag("info:features_merged", "yes");
                    changeset.Modify(osmFeature);
                  }
                }
                catch (ChangesetWrapper::OsmObjectWasDeletedException const &)
                {
                  // Object was never created by anyone else - it's safe to create it.
                  changeset.AddChangesetTag("info:old_editor", "yes");
                  changeset.Create(feature);
                }
                catch (ChangesetWrapper::EmptyFeatureException const &)
                {
                  // There is another node nearby, but it should be safe to create a new one.
                  changeset.AddChangesetTag("info:old_editor", "yes");
                  changeset.Create(feature);
                }
                catch (...)
                {
                  // Pass network or other errors to outside exception handler.
                  throw;
                }
              }
                break;

              case FeatureStatus::Modified:
              {
                // Do not serialize feature's type to avoid breaking OSM data.
                // TODO: Implement correct types matching when we support modifying existing feature types.
                XMLFeature feature = editor::ToXML(fti.m_object, false);
                if (!fti.m_street.empty())
                  feature.SetTagValue(kAddrStreetTag, fti.m_street);

                auto const originalObjectPtr = GetOriginalMapObject(fti.m_object.GetID());
                if (!originalObjectPtr)
                {
                  LOG(LERROR, ("A feature with id", fti.m_object.GetID(), "cannot be loaded."));
                  GetPlatform().RunTask(Platform::Thread::Gui,[this, fid = fti.m_object.GetID()]() {
                                          RemoveFeatureIfExists(fid);
                                        });
                  continue;
                }

                XMLFeature osmFeature = GetMatchingFeatureFromOSM(changeset, *originalObjectPtr);
                XMLFeature const osmFeatureCopy = osmFeature;
                osmFeature.ApplyPatch(feature);
                // Check to avoid uploading duplicates into OSM.
                if (osmFeature == osmFeatureCopy)
                {
                  LOG(LWARNING,("Local changes are equal to OSM, feature has not been uploaded.", osmFeatureCopy));
                  // Don't delete this local change right now for user to see it in profile.
                  // It will be automatically deleted by migration code on the next maps update.
                }
                else
                {
                  LOG(LDEBUG, ("Uploading patched feature", osmFeature));
                  changeset.AddChangesetTag("info:old_editor", "yes");
                  changeset.Modify(osmFeature);
                }
              }
                break;

              case FeatureStatus::Deleted:
                auto const originalObjectPtr = GetOriginalMapObject(fti.m_object.GetID());
                if (!originalObjectPtr)
                {
                  LOG(LERROR, ("A feature with id", fti.m_object.GetID(), "cannot be loaded."));
                  GetPlatform().RunTask(Platform::Thread::Gui,[this, fid = fti.m_object.GetID()]() {
                                          RemoveFeatureIfExists(fid);
                                        });
                  continue;
                }
                changeset.AddChangesetTag("info:old_editor", "yes");
                changeset.Delete(GetMatchingFeatureFromOSM(changeset, *originalObjectPtr));
                break;
            }
          }
          uploadInfo.m_uploadStatus = kUploaded;
          uploadInfo.m_uploadError.clear();
          ++uploadedFeaturesCount;
        }
        catch (ChangesetWrapper::OsmObjectWasDeletedException const & ex)
        {
          uploadInfo.m_uploadStatus = kDeletedFromOSMServer;
          uploadInfo.m_uploadError = ex.Msg();
          ++errorsCount;
          LOG(LWARNING, (ex.what()));
          changeset.SetErrorDescription(ex.Msg());
        }
        catch (ChangesetWrapper::EmptyFeatureException const & ex)
        {
          uploadInfo.m_uploadStatus = kMatchedFeatureIsEmpty;
          uploadInfo.m_uploadError = ex.Msg();
          ++errorsCount;
          LOG(LWARNING, (ex.what()));
          changeset.SetErrorDescription(ex.Msg());
        }
        catch (RootException const & ex)
        {
          uploadInfo.m_uploadStatus = kNeedsRetry;
          uploadInfo.m_uploadError = ex.Msg();
          ++errorsCount;
          LOG(LWARNING, (ex.what()));
          changeset.SetErrorDescription(ex.Msg());
        }
        // TODO(AlexZ): Use timestamp from the server.
        uploadInfo.m_uploadAttemptTimestamp = time(nullptr);

        GetPlatform().RunTask(Platform::Thread::Gui,
                              [this, id = fti.m_object.GetID(), uploadInfo]() {
                                // Call Save every time we modify each feature's information.
                                SaveUploadedInformation(id, uploadInfo);
                              });
      }
    }

    if (callback)
    {
      UploadResult result = UploadResult::NothingToUpload;
      if (uploadedFeaturesCount)
        result = UploadResult::Success;
      else if (errorsCount)
        result = UploadResult::Error;
      callback(result);
    }

    m_isUploadingNow = false;
  };

  // Do not run more than one uploading task at a time.
  if (!m_isUploadingNow)
  {
    m_isUploadingNow = true;
    GetPlatform().RunTask(Platform::Thread::Network, [upload = std::move(upload), oauthToken,
                                                      tags = std::move(tags), callback = std::move(callback)]()
    {
      upload(std::move(oauthToken), std::move(tags), std::move(callback));
    });
  }
}

void Editor::SaveUploadedInformation(FeatureID const & fid, UploadInfo const & uploadInfo)
{
  CHECK_THREAD_CHECKER(MainThreadChecker, (""));

  auto const features = m_features.Get();
  auto editableFeatures = make_shared<FeaturesContainer>(*features);

  auto id = editableFeatures->find(fid.m_mwmId);
  // Rare case: feature was deleted at the time of changes uploading.
  if (id == editableFeatures->end())
    return;

  auto index = id->second.find(fid.m_index);
  // Rare case: feature was deleted at the time of changes uploading.
  if (index == id->second.end())
    return;

  auto & fti = index->second;
  fti.m_uploadAttemptTimestamp = uploadInfo.m_uploadAttemptTimestamp;
  fti.m_uploadStatus = uploadInfo.m_uploadStatus;
  fti.m_uploadError = uploadInfo.m_uploadError;

  if (!NeedsUpload(uploadInfo.m_uploadStatus))
    fti.m_object.ClearJournal();

  SaveTransaction(editableFeatures);
}

bool Editor::FillFeatureInfo(FeatureStatus status, XMLFeature const & xml, FeatureID const & fid,
                             FeatureTypeInfo & fti) const
{
  EditJournal journal = xml.GetEditJournal();

  // Do not load Legacy Objects form Journal
  auto const & journalHistory = journal.GetJournalHistory();
  bool loadFromJournal = journalHistory.empty() || journalHistory.front().journalEntryType != JournalEntryType::LegacyObject;

  LOG(LDEBUG, ("loadFromJournal: ", loadFromJournal));

  if (status == FeatureStatus::Created)
  {
    if (loadFromJournal)
      fti.m_object.ApplyEditsFromJournal(journal);
    else
      editor::FromXML(xml, fti.m_object);
  }
  else
  {
    auto const originalObjectPtr = GetOriginalMapObject(fid);
    if (!originalObjectPtr)
    {
      LOG(LERROR, ("A feature with id", fid, "cannot be loaded."));
      return false;
    }

    fti.m_object = *originalObjectPtr;

    if (loadFromJournal)
      fti.m_object.ApplyEditsFromJournal(journal);
    else
      editor::ApplyPatch(xml, fti.m_object);
  }

  fti.m_object.SetJournal(std::move(journal));
  fti.m_object.SetID(fid);
  fti.m_street = xml.GetTagValue(kAddrStreetTag);

  fti.m_modificationTimestamp = xml.GetModificationTime();
  ASSERT_NOT_EQUAL(base::INVALID_TIME_STAMP, fti.m_modificationTimestamp, ());
  fti.m_uploadAttemptTimestamp = xml.GetUploadTime();
  fti.m_uploadStatus = xml.GetUploadStatus();
  fti.m_uploadError = xml.GetUploadError();
  fti.m_status = status;

  return true;
}

Editor::FeatureTypeInfo const * Editor::GetFeatureTypeInfo(FeaturesContainer const & features,
                                                           MwmId const & mwmId,
                                                           uint32_t index)
{
  auto const matchedMwm = features.find(mwmId);
  if (matchedMwm == features.cend())
    return nullptr;

  auto const matchedIndex = matchedMwm->second.find(index);
  if (matchedIndex == matchedMwm->second.cend())
    return nullptr;

  /* TODO(AlexZ): Should we process deleted/created features as well?*/
  return &matchedIndex->second;
}

bool Editor::RemoveFeatureIfExists(FeatureID const & fid)
{
  CHECK_THREAD_CHECKER(MainThreadChecker, (""));

  auto const features = m_features.Get();
  auto editableFeatures = make_shared<FeaturesContainer>(*features);

  auto matchedMwm = editableFeatures->find(fid.m_mwmId);
  if (matchedMwm == editableFeatures->end())
    return true;

  auto matchedIndex = matchedMwm->second.find(fid.m_index);
  if (matchedIndex != matchedMwm->second.end())
    matchedMwm->second.erase(matchedIndex);

  if (matchedMwm->second.empty())
    editableFeatures->erase(matchedMwm);

  return SaveTransaction(editableFeatures);
}

void Editor::Invalidate()
{
  if (m_invalidateFn)
    m_invalidateFn();
}

bool Editor::MarkFeatureAsObsolete(FeatureID const & fid)
{
  CHECK_THREAD_CHECKER(MainThreadChecker, (""));

  auto const features = m_features.Get();
  auto editableFeatures = make_shared<FeaturesContainer>(*features);

  auto const featureStatus = GetFeatureStatusImpl(*editableFeatures, fid.m_mwmId, fid.m_index);
  if (featureStatus != FeatureStatus::Untouched && featureStatus != FeatureStatus::Modified)
  {
    ASSERT(false, ("Only untouched and modified features can be made obsolete"));
    return false;
  }

  MarkFeatureWithStatus(*editableFeatures, fid, FeatureStatus::Obsolete);
  auto const result = SaveTransaction(editableFeatures);
  Invalidate();

  return result;
}

bool Editor::RemoveFeature(FeatureID const & fid)
{
  CHECK_THREAD_CHECKER(MainThreadChecker, (""));

  auto const result = RemoveFeatureIfExists(fid);

  if (result)
    Invalidate();

  return result;
}

Editor::Stats Editor::GetStats() const
{
  Stats stats;
  LOG(LDEBUG, ("Edited features status:"));

  auto const features = m_features.Get();
  for (auto const & id : *features)
  {
    for (auto & index : id.second)
    {
      auto const & fti = index.second;
      stats.m_edits.emplace_back(FeatureID(id.first, index.first),
                                 fti.m_uploadStatus + " " + fti.m_uploadError);
      LOG(LDEBUG, (fti.m_uploadAttemptTimestamp == base::INVALID_TIME_STAMP
                       ? "NOT_UPLOADED_YET"
                       : base::TimestampToString(fti.m_uploadAttemptTimestamp),
                   fti.m_uploadStatus, fti.m_uploadError, fti.m_object.GetGeomType(),
                   fti.m_object.GetMercator()));
      if (fti.m_uploadStatus == kUploaded)
      {
        ++stats.m_uploadedCount;
        if (stats.m_lastUploadTimestamp < fti.m_uploadAttemptTimestamp)
          stats.m_lastUploadTimestamp = fti.m_uploadAttemptTimestamp;
      }
    }
  }
  return stats;
}

NewFeatureCategories Editor::GetNewFeatureCategories() const
{
  return NewFeatureCategories(*(m_config.Get()));
}

FeatureID Editor::GenerateNewFeatureId(FeaturesContainer const & features,
                                       MwmId const & id) const
{
  CHECK_THREAD_CHECKER(MainThreadChecker, (""));

  uint32_t featureIndex = feature::FakeFeatureIds::kEditorCreatedFeaturesStart;

  auto const found = features.find(id);
  if (found != features.cend())
  {
    // Scan all already created features and choose next available ID.
    for (auto const & feature : found->second)
    {
      if (feature.second.m_status == FeatureStatus::Created && featureIndex <= feature.first)
        featureIndex = feature.first + 1;
    }
  }
  CHECK(feature::FakeFeatureIds::IsEditorCreatedFeature(featureIndex), ());
  return {id, featureIndex};
}

bool Editor::CreatePoint(uint32_t type, m2::PointD const & mercator, MwmId const & id,
                         EditableMapObject & outFeature) const
{
  ASSERT(id.IsAlive(),
         ("Please check that feature is created in valid MWM file before calling this method."));
  if (!id.GetInfo()->m_bordersRect.IsPointInside(mercator))
  {
    LOG(LERROR, ("Attempt to create a feature outside of the MWM's bounding box."));
    return false;
  }

  outFeature.SetMercator(mercator);
  outFeature.SetID(GenerateNewFeatureId(*(m_features.Get()), id));
  outFeature.SetType(type);
  outFeature.SetEditableProperties(GetEditablePropertiesForTypes(outFeature.GetTypes()));
  // Only point type features can be created at the moment.
  outFeature.SetPointType();
  outFeature.MarkAsCreated(type, feature::GeomType::Point, mercator);
  return true;
}

void Editor::CreateNote(ms::LatLon const & latLon, FeatureID const & fid,
                        feature::TypesHolder const & holder, std::string_view defaultName,
                        NoteProblemType type, std::string_view note)
{
  CHECK_THREAD_CHECKER(MainThreadChecker, (""));

  std::ostringstream sstr;
  auto canCreate = true;

  if (!note.empty())
    sstr << '"' << note << "\"\n";

  switch (type)
  {
  case NoteProblemType::PlaceDoesNotExist:
  {
    sstr << "The place has gone or never existed. A user of Organic Maps application has reported "
            "that the POI was visible on the map (see snapshot date below), but was not found "
            "on the ground.\n";
    auto const features = m_features.Get();
    auto const isCreated =
        GetFeatureStatusImpl(*features, fid.m_mwmId, fid.m_index) == FeatureStatus::Created;
    auto const createdAndUploaded =
        (isCreated && IsFeatureUploadedImpl(*features, fid.m_mwmId, fid.m_index));
    CHECK(!isCreated || createdAndUploaded, ());

    if (createdAndUploaded)
      canCreate = RemoveFeature(fid);
    else
      canCreate = MarkFeatureAsObsolete(fid);

    break;
  }
  case NoteProblemType::General: break;
  }

  if (!canCreate)
    return;

  auto const version = GetMwmCreationTimeByMwmId(fid.m_mwmId);
  auto const strVersion = base::TimestampToString(base::SecondsSinceEpochToTimeT(version));
  sstr << "OSM snapshot date: " << strVersion << "\n";

  if (defaultName.empty())
    sstr << "POI has no name\n";
  else
    sstr << "POI name: " << defaultName << "\n";

  sstr << "POI types:";
  for (auto const & poiType : holder.ToObjectNames())
    sstr << ' ' << poiType;
  sstr << "\n";
  std::cout << "***TEXT*** " << sstr.str();
  m_notes->CreateNote(latLon, sstr.str());
}

void Editor::MarkFeatureWithStatus(FeaturesContainer & editableFeatures, FeatureID const & fid,
                                   FeatureStatus status)
{
  CHECK_THREAD_CHECKER(MainThreadChecker, (""));

  auto & fti = editableFeatures[fid.m_mwmId][fid.m_index];

  auto const originalObjectPtr = GetOriginalMapObject(fid);

  if (!originalObjectPtr)
  {
    LOG(LERROR, ("A feature with id", fid, "cannot be loaded."));
    return;
  }

  fti.m_object = *originalObjectPtr;
  fti.m_status = status;
  fti.m_modificationTimestamp = time(nullptr);
}

MwmSet::MwmId Editor::GetMwmIdByMapName(string const & name)
{
  if (!m_delegate)
  {
    LOG(LERROR, ("Can't get mwm id by map name:", name, ", delegate is not set."));
    return {};
  }
  return m_delegate->GetMwmIdByMapName(name);
}

std::unique_ptr<EditableMapObject> Editor::GetOriginalMapObject(FeatureID const & fid) const
{
  if (!m_delegate)
  {
    LOG(LERROR, ("Can't get original feature by id:", fid, ", delegate is not set."));
    return {};
  }
  return m_delegate->GetOriginalMapObject(fid);
}

string Editor::GetOriginalFeatureStreet(FeatureID const & fid) const
{
  if (!m_delegate)
  {
    LOG(LERROR, ("Can't get feature street, delegate is not set."));
    return {};
  }
  return m_delegate->GetOriginalFeatureStreet(fid);
}

void Editor::ForEachFeatureAtPoint(FeatureTypeFn && fn, m2::PointD const & point) const
{
  if (!m_delegate)
  {
    LOG(LERROR, ("Can't load features in point's vicinity, delegate is not set."));
    return;
  }
  m_delegate->ForEachFeatureAtPoint(std::move(fn), point);
}

FeatureID Editor::GetFeatureIdByXmlFeature(FeaturesContainer const & features,
                                           XMLFeature const & xml, MwmId const & mwmId,
                                           FeatureStatus status, bool needMigrate) const
{
  ForEachFeaturesNearByFn forEach = [this](FeatureTypeFn && fn, m2::PointD const & point) {
    return ForEachFeatureAtPoint(std::move(fn), point);
  };

  // TODO(mgsergio): Deleted features are not properly handled yet.
  if (needMigrate)
  {
    return editor::MigrateFeatureIndex(forEach, xml, status, [this, &mwmId, &features] {
      return GenerateNewFeatureId(features, mwmId);
    });
  }

  return {mwmId, xml.GetMWMFeatureIndex()};
}

void Editor::LoadMwmEdits(FeaturesContainer & loadedFeatures, xml_node const & mwm,
                          MwmId const & mwmId, bool needMigrate)
{
  LogHelper logHelper(mwmId);

  for (auto const & section : kXmlSections)
  {
    for (auto const & nodeOrWay : mwm.child(section.m_sectionName).select_nodes("node|way"))
    {
      try
      {
        XMLFeature const xml(nodeOrWay.node());

        auto const fid =
            GetFeatureIdByXmlFeature(loadedFeatures, xml, mwmId, section.m_status, needMigrate);

        // Remove obsolete changes during migration.
        if (needMigrate && IsObsolete(xml, fid))
          continue;

        FeatureTypeInfo fti;
        if (!FillFeatureInfo(section.m_status, xml, fid, fti))
          continue;

        logHelper.OnStatus(section.m_status);

        loadedFeatures[fid.m_mwmId].emplace(fid.m_index, std::move(fti));
      }
      catch (editor::XMLFeatureError const & ex)
      {
        std::ostringstream s;
        nodeOrWay.node().print(s, "  ");
        LOG(LERROR, (ex.what(), "mwmId =", mwmId, "in section", section.m_sectionName, s.str()));
      }
      catch (editor::MigrationError const & ex)
      {
        LOG(LWARNING, (ex.what(), "mwmId =", mwmId, XMLFeature(nodeOrWay.node())));
      }
    }
  }
}

bool Editor::HaveMapEditsToUpload(FeaturesContainer const & features)
{
  for (auto const & id : features)
  {
    // Do not upload changes for a deleted map.
    // todo(@a, @m) This should be unnecessary but unfortunately it's not.
    // The mwm's feature is read right before the xml feature is uploaded to fill
    // in missing fields such as precise feature geometry. This is an artefact
    // of an old design decision; we have all the information needed for uploading
    // already at the time when the xml feature is created and should not
    // need to re-read it when uploading and when the corresponding mwm may have
    // been deleted from disk.
    if (!id.first.IsAlive())
      continue;

    for (auto const & index : id.second)
    {
      if (NeedsUpload(index.second.m_uploadStatus))
        return true;
    }
  }
  return false;
}

FeatureStatus Editor::GetFeatureStatusImpl(FeaturesContainer const & features,
                                           MwmId const & mwmId, uint32_t index)
{
  // Most popular case optimization.
  if (features.empty())
    return FeatureStatus::Untouched;

  auto const * featureInfo = GetFeatureTypeInfo(features, mwmId, index);
  if (featureInfo == nullptr)
    return FeatureStatus::Untouched;

  return featureInfo->m_status;
}

bool Editor::IsFeatureUploadedImpl(FeaturesContainer const & features, MwmId const & mwmId, uint32_t index)
{
  auto const * info = GetFeatureTypeInfo(features, mwmId, index);
  return info && info->m_uploadStatus == kUploaded;
}

void Editor::UpdateXMLFeatureTags(editor::XMLFeature & feature, std::list<JournalEntry> const & journal)
{
  for (JournalEntry const & entry: journal)
  {
    switch (entry.journalEntryType)
    {
      case JournalEntryType::TagModification:
      {
        TagModData const & tagModData = std::get<TagModData>(entry.data);
        feature.UpdateOSMTag(tagModData.key, tagModData.new_value);
        break;
      }
      case JournalEntryType::ObjectCreated:
        break;
      case JournalEntryType::LegacyObject:
        ASSERT_FAIL(("Legacy Objects can not be edited with the new editor"));
        break;
    }
  }
}

string DebugPrint(Editor::SaveResult saveResult)
{
  switch (saveResult)
  {
    case Editor::SaveResult::NothingWasChanged: return "NothingWasChanged";
    case Editor::SaveResult::SavedSuccessfully: return "SavedSuccessfully";
    case Editor::SaveResult::NoFreeSpaceError: return "NoFreeSpaceError";
    case Editor::SaveResult::NoUnderlyingMapError: return "NoUnderlyingMapError";
    case Editor::SaveResult::SavingError: return "SavingError";
  }
  UNREACHABLE();
}
}  // namespace osm
