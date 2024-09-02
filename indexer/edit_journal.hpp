#pragma once

#include "indexer/feature_decl.hpp"
#include "indexer/feature_meta.hpp"
#include "indexer/feature_utils.hpp"
#include "indexer/map_object.hpp"

#include "coding/string_utf8_multilang.hpp"

#include <functional>
#include <string>
#include <vector>
//TODO: clean up imports

namespace osm
{
  enum class JournalEntryType {
    TagModification,
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

  //using EditJournal = std::list<JournalEntry>;

  class EditJournal
  {
    std::list<JournalEntry> journal{};

    void addJournalEntry(const JournalEntry& entry) {
      journal.push_back(entry);
    }

    public: void addTagChange(feature::Metadata::EType type, std::string_view old_value, std::string_view new_value)
    {
      JournalEntry entry = {JournalEntryType::TagModification, time(nullptr), type, old_value, new_value};
      addJournalEntry(entry);
      //std::string old =
      LOG(LDEBUG, ("Tag ", ToString(type), "changed from \"", (std::string) old_value, "\" to \"", (std::string) new_value, "\""));
    }

    void clearJournal() {
      journal = {};
    }

    const std::list<JournalEntry> & getJournal() {
      return journal;
    }
  };
}