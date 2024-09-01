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
    std::string tag;
    std::string old_value;
    std::string new_value;
  };

  //using EditJournal = std::list<JournalEntry>;

  class EditJournal
  {
    std::list<JournalEntry> journal{};

    void addJournalEntry(const JournalEntry& entry) {
      journal.push_back(entry);
    }

    void clearJournal() {
      journal = {};
    }

    const std::list<JournalEntry> & getJournal() {
      return journal;
    }
  };
}