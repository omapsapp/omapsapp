#pragma once

#include "indexer/feature_decl.hpp"
#include "indexer/feature_meta.hpp"
#include "indexer/feature_utils.hpp"

#include <functional>
#include <string>
#include <vector>

namespace osm
{
  enum class JournalEntryType
  {
    TagModification,
    ObjectCreated,
    LegacyObject,   //object without full journal history, used for transition to new editor
    //Possible future values: ObjectDeleted, ObjectDisused, ObjectNotDisused, LocationChanged, FeatureTypeChanged
  };

  struct TagModData
  {
    std::string key;
    std::string old_value;
    std::string new_value;
  };

  struct ObjCreateData
  {
    uint32_t type;
    feature::GeomType geomType;
    m2::PointD mercator;
  };

  struct LegacyObjData
  {
    std::string version;
  };

  struct JournalEntry
  {
    JournalEntryType journalEntryType = JournalEntryType::TagModification;
    time_t timestamp;
    std::variant<TagModData, ObjCreateData, LegacyObjData> data;
  };

  /// Used to determine whether existing OSM object should be updated or new one created
  enum class EditingLifecycle
  {
    CREATED,      //newly created and not synced with OSM
    MODIFIED,     //modified and not synced with OSM
    IN_SYNC       //synced with OSM (including never edited)
  };

  class EditJournal
  {
    std::list<JournalEntry> m_journal{};
    std::list<JournalEntry> m_journalHistory{};

  public:
    std::list<JournalEntry> const & GetJournal() const;

    osm::EditingLifecycle GetEditingLifecycle() const;

    /// Log object edits in the journal
    void AddTagChange(std::string key, std::string old_value, std::string new_value);

    /// Log object creation in the journal
    void MarkAsCreated(uint32_t type, feature::GeomType geomType, m2::PointD mercator);

    void AddJournalEntry(JournalEntry entry);

    /// Clear Journal and move content to journalHistory, used after upload to OSM
    void Clear();

    std::list<JournalEntry> const & GetJournalHistory() const;

    void AddJournalHistoryEntry(JournalEntry entry);

    std::string JournalToString() const;

    static std::string ToString(osm::JournalEntry const & journalEntry);

    static std::string ToString(osm::JournalEntryType journalEntryType);

    static std::optional<JournalEntryType> TypeFromString(std::string const & entryType);
  };
}
