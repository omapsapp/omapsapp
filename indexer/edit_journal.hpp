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
    //Possible future values: ObjectDeleted, ObjectDisused, ObjectNotDisused
  };

  struct JournalEntry
  {
    JournalEntryType editingAction = JournalEntryType::TagModification;
    time_t timestamp;
    feature::Metadata::EType tag;
    std::string_view old_value;
    std::string_view new_value;
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

    void AddTagChange(feature::Metadata::EType type, std::string_view old_value, std::string_view new_value);

    void Clear();

    const std::list<JournalEntry> & GetJournal();

    void MarkAsCreated();

    osm::EditingLifecycle GetEditingLifecycle();
  };
}