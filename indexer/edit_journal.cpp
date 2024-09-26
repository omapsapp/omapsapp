#include "indexer/edit_journal.hpp"

#include "base/control_flow.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <cmath>
#include <regex>
#include <sstream>

namespace osm
{
  void EditJournal::AddJournalEntry(JournalEntry const & entry)
  {
    journal.push_back(entry);
  }

  void EditJournal::AddTagChange(std::string key, std::string old_value, std::string new_value)
  {
    TagModData tagModData = {key, old_value, new_value};
    JournalEntry entry = {JournalEntryType::TagModification, time(nullptr), std::move(tagModData)};
    AddJournalEntry(entry);
    LOG(LDEBUG, ("Key ", key, "changed from \"", (std::string) old_value, "\" to \"", (std::string) new_value, "\""));
  }

  void EditJournal::Clear()
  {
    journal = {};
  }

  std::list<JournalEntry> const & EditJournal::GetJournal() const
  {
    return journal;
  }

  void EditJournal::MarkAsCreated(uint32_t type, feature::GeomType geomType, m2::PointD mercator)
  {
    ASSERT(journal.empty(), ("Only empty journals can be marked as created"));
    ObjCreateData objCreateData = {type, geomType, std::move(mercator)};
    JournalEntry entry = {JournalEntryType::ObjectCreated, time(nullptr), std::move(objCreateData)};
    AddJournalEntry(entry);
    LOG(LDEBUG, ("Object of type ", classif().GetFullObjectName(type), " created"));
  }

  osm::EditingLifecycle EditJournal::GetEditingLifecycle() const
  {
    if (journal.empty()) {
      return EditingLifecycle::IN_SYNC;
    }
    else if (journal.front().journalEntryType == JournalEntryType::ObjectCreated) {
      return EditingLifecycle::CREATED;
    }
    return EditingLifecycle::MODIFIED;
  }

  std::string EditJournal::JournalToString() const
  {
    std::string string;
    std::for_each(journal.begin(), journal.end(), [&](auto const & journalEntry) {
      string += ToString(journalEntry) + "\n";
    });
    return string;
  }

  std::string EditJournal::ToString(osm::JournalEntry const & journalEntry) const
  {
    switch (journalEntry.journalEntryType) {
      case osm::JournalEntryType::TagModification: {
        TagModData const & tagModData = std::get<TagModData>(journalEntry.data);
        return ToString(journalEntry.journalEntryType)
            .append(": Key ").append(tagModData.key)
            .append(" changed from \"").append(tagModData.old_value)
            .append("\" to \"").append(tagModData.new_value).append("\"");
      }
      case osm::JournalEntryType::ObjectCreated: {
        ObjCreateData const & objCreatedData = std::get<ObjCreateData>(journalEntry.data);
        return ToString(journalEntry.journalEntryType)
            .append(": ").append(classif().GetFullObjectName(objCreatedData.type))
            .append(" (").append(std::to_string(objCreatedData.type)).append(")");
      }
    }
  }

  std::string EditJournal::ToString(osm::JournalEntryType journalEntryType) const
  {
    switch (journalEntryType) {
      case osm::JournalEntryType::TagModification:
        return "TAG MODIFICATION";
      case osm::JournalEntryType::ObjectCreated:
        return "OBJECT CREATED";
    }
  }
}
