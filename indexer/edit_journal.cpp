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
    JournalEntry entry = {JournalEntryType::TagModification, time(nullptr), key, old_value, new_value};
    AddJournalEntry(entry);
    LOG(LDEBUG, ("Key ", key, "changed from \"", (std::string) old_value, "\" to \"", (std::string) new_value, "\""));
  }

  void EditJournal::Clear() {
    journal = {};
  }

  const std::list<JournalEntry> &EditJournal::GetJournal() {
    return journal;
  }

  void EditJournal::MarkAsCreated()
  {
    ASSERT(journal.empty(), ("Only empty journals can be marked as created"));
    JournalEntry entry = {JournalEntryType::ObjectCreated, time(nullptr), "", "", ""};
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

  string EditJournal::ToString(osm::JournalEntry journalEntry)
  {
    switch (journalEntry.journalEntryType) {
      case osm::JournalEntryType::TagModification:
        return ToString(journalEntry.journalEntryType)
          .append(": Key ").append(journalEntry.key)
          .append(" changed from \"").append(journalEntry.old_value)
          .append("\" to \"").append(journalEntry.new_value).append("\"");
      case osm::JournalEntryType::ObjectCreated:
        return ToString(journalEntry.journalEntryType) + ": -";
    }
  }

  string EditJournal::ToString(osm::JournalEntryType journalEntryType)
  {
    switch (journalEntryType) {
      case osm::JournalEntryType::TagModification:
        return "TAG MODIFICATION";
      case osm::JournalEntryType::ObjectCreated:
        return "OBJECT CREATED";
    }
  }
}