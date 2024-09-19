#include "indexer/edit_journal.hpp"

#include "base/control_flow.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <cmath>
#include <regex>
#include <sstream>

namespace osm
{
  void EditJournal::AddJournalEntry(const JournalEntry& entry) {
    journal.push_back(entry);
  }

  void EditJournal::AddTagChange(std::string key, std::string old_value, std::string new_value)
  {
    TagModData tagModData = {key, old_value, new_value};
    JournalEntry entry = {JournalEntryType::TagModification, time(nullptr), tagModData};
    AddJournalEntry(entry);
    LOG(LDEBUG, ("Key ", key, "changed from \"", (std::string) old_value, "\" to \"", (std::string) new_value, "\""));
  }

  void EditJournal::Clear() {
    journal = {};
  }

  const std::list<JournalEntry> &EditJournal::GetJournal() {
    return journal;
  }

  void EditJournal::MarkAsCreated(uint32_t type, feature::GeomType geomType, m2::PointD mercator)
  {
    ASSERT(journal.empty(), ("Only empty journals can be marked as created"));
    ObjCreateData objCreateData = {type, geomType, mercator};
    JournalEntry entry = {JournalEntryType::ObjectCreated, time(nullptr), objCreateData};
    AddJournalEntry(entry);
  }

  osm::EditingLifecycle EditJournal::GetEditingLifecycle()
  {
    if (journal.empty()) {
      return EditingLifecycle::IN_SYNC;
    }
    else if (journal.front().journalEntryType == JournalEntryType::ObjectCreated) {
      return EditingLifecycle::CREATED;
    }
    return EditingLifecycle::MODIFIED;
  }

  std::string EditJournal::JournalToString()
  {
    std::string string;
    std::for_each(journal.begin(), journal.end(), [&](const auto &journalEntry) {
      string += ToString(journalEntry) + "\n";
    });
    return string;
  }

  std::string EditJournal::ToString(osm::JournalEntry journalEntry)
  {
    switch (journalEntry.journalEntryType) {
      case osm::JournalEntryType::TagModification: {
        TagModData tagModData = std::get<TagModData>(journalEntry.data);
        return ToString(journalEntry.journalEntryType)
            .append(": Key ").append(tagModData.key)
            .append(" changed from \"").append(tagModData.old_value)
            .append("\" to \"").append(tagModData.new_value).append("\"");
      }
      case osm::JournalEntryType::ObjectCreated: {
        ObjCreateData objCreatedData = std::get<ObjCreateData>(journalEntry.data);
        return ToString(journalEntry.journalEntryType)
            .append(": ").append(classif().GetFullObjectName(objCreatedData.type))
            .append(" (").append(to_string(objCreatedData.type)).append(")");
      }
    }
  }

  std::string EditJournal::ToString(osm::JournalEntryType journalEntryType)
  {
    switch (journalEntryType) {
      case osm::JournalEntryType::TagModification:
        return "TAG MODIFICATION";
      case osm::JournalEntryType::ObjectCreated:
        return "OBJECT CREATED";
    }
  }
}
