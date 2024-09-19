#pragma once

#include "indexer/feature_decl.hpp"
#include "indexer/feature_meta.hpp"
#include "indexer/feature_utils.hpp"

#include <functional>
#include <string>
#include <vector>
//TODO: clean up imports

namespace osm
{
  enum class JournalEntryType {
    TagModification,
    ObjectCreated,
    //LegacyObject,   //object without full history journal, only used for transition
    //Possible future values: ObjectDeleted, ObjectDisused, ObjectNotDisused, LocationChanged, FeatureTypeChanged
  };

  struct TagModData {
    std::string key;
    std::string old_value;
    std::string new_value;
  };

  struct ObjCreateData {
    uint32_t type;
    feature::GeomType geomType;
    m2::PointD mercator;
  };

  struct JournalEntry
  {
    JournalEntryType journalEntryType = JournalEntryType::TagModification;
    time_t timestamp;
    std::variant<TagModData, ObjCreateData> data;
  };

  enum class EditingLifecycle
  {
    CREATED,      //newly created and not synced with OSM
    MODIFIED,     //modified and not synced with OSM
    IN_SYNC       //synced with OSM (including never edited)
  };

  //using EditJournal = std::list<JournalEntry>;

  class EditJournal
  {
    std::list<JournalEntry> journal{};

  public:
    void AddJournalEntry(const JournalEntry& entry);

    void AddTagChange(std::string key, std::string old_value, std::string new_value);

    void Clear();

    const std::list<JournalEntry> & GetJournal();

    void MarkAsCreated(uint32_t type, feature::GeomType geomType, m2::PointD mercator);

    osm::EditingLifecycle GetEditingLifecycle();

    std::string JournalToString();

    std::string ToString(osm::JournalEntry journalEntry);

    std::string ToString(osm::JournalEntryType journalEntryType);
  };
}
