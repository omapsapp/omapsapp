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

  void EditJournal::AddTagChange(feature::Metadata::EType type, std::string_view old_value, std::string_view new_value)
  {
    JournalEntry entry = {JournalEntryType::TagModification, time(nullptr), type, old_value, new_value};
    AddJournalEntry(entry);
    LOG(LDEBUG, ("Tag ", feature::ToString(type), "changed from \"", (std::string) old_value, "\" to \"", (std::string) new_value, "\""));
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
    JournalEntry entry = {JournalEntryType::ObjectCreated, time(nullptr), feature::Metadata::FMD_TEST_ID, "", ""};
    AddJournalEntry(entry);
  }

  osm::EditingLifecycle EditJournal::GetEditingLifecycle()
  {
    if (journal.empty()) {
      return EditingLifecycle::IN_SYNC;
    }
    else if (journal.front().editingAction == JournalEntryType::ObjectCreated) {
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
    switch (journalEntry.editingAction) {
      case osm::JournalEntryType::TagModification:{
        string res = ToString(journalEntry.editingAction) + ": Tag "
            + feature::ToString(journalEntry.tag) + " changed from \"";
        res.append(journalEntry.old_value).append("\" to \"").append("[[new_value]]").append("\"");
        //std::string new_v = static_cast<string>(journalEntry.new_value);
        return res;
        //ToString(journalEntry.editingAction) + ": Tag " + feature::ToString(journalEntry.tag)
               //+ "changed from \"" + old_v
               //+ "\" to \""+ new_v + "\"";
      }
      case osm::JournalEntryType::ObjectCreated:
        return ToString(journalEntry.editingAction) + ": -";
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