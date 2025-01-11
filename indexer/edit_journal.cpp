#include "indexer/edit_journal.hpp"

#include "base/control_flow.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <cmath>
#include <regex>
#include <sstream>

namespace osm
{
  std::list<JournalEntry> const & EditJournal::GetJournal() const
  {
    return m_journal;
  }

  osm::EditingLifecycle EditJournal::GetEditingLifecycle() const
  {
    if (m_journal.empty())
      return EditingLifecycle::IN_SYNC;

    else if (m_journal.front().journalEntryType == JournalEntryType::ObjectCreated)
      return EditingLifecycle::CREATED;

    return EditingLifecycle::MODIFIED;
  }

  void EditJournal::AddTagChange(std::string key, std::string old_value, std::string new_value)
  {
    LOG(LDEBUG, ("Key ", key, "changed from \"", old_value, "\" to \"", new_value, "\""));
    AddJournalEntry({JournalEntryType::TagModification, time(nullptr), TagModData{std::move(key), std::move(old_value), std::move(new_value)}});
  }

  void EditJournal::MarkAsCreated(uint32_t type, feature::GeomType geomType, m2::PointD mercator)
  {
    ASSERT(m_journal.empty(), ("Only empty journals can be marked as created"));
    LOG(LDEBUG, ("Object of type ", classif().GetReadableObjectName(type), " created"));
    AddJournalEntry({JournalEntryType::ObjectCreated, time(nullptr), osm::ObjCreateData{type, geomType, mercator}});
  }

  void EditJournal::AddJournalEntry(JournalEntry entry)
  {
    m_journal.push_back(std::move(entry));
  }

  void EditJournal::Clear()
  {
    for (JournalEntry & entry : m_journal)
      m_journalHistory.push_back(std::move(entry));

    m_journal = {};
  }

  std::list<JournalEntry> const & EditJournal::GetJournalHistory() const
  {
    return m_journalHistory;
  }

  void EditJournal::AddJournalHistoryEntry(JournalEntry entry)
  {
    m_journalHistory.push_back(std::move(entry));
  }

  std::string EditJournal::JournalToString() const
  {
    std::string string;
    std::for_each(m_journal.begin(), m_journal.end(), [&](auto const & journalEntry) {
      string += ToString(journalEntry) + "\n";
    });
    return string;
  }

  std::string EditJournal::ToString(osm::JournalEntry const & journalEntry)
  {
    switch (journalEntry.journalEntryType)
    {
      case osm::JournalEntryType::TagModification:
      {
        TagModData const & tagModData = std::get<TagModData>(journalEntry.data);
        return ToString(journalEntry.journalEntryType)
            .append(": Key ").append(tagModData.key)
            .append(" changed from \"").append(tagModData.old_value)
            .append("\" to \"").append(tagModData.new_value).append("\"");
      }
      case osm::JournalEntryType::ObjectCreated:
      {
        ObjCreateData const & objCreatedData = std::get<ObjCreateData>(journalEntry.data);
        return ToString(journalEntry.journalEntryType)
            .append(": ").append(classif().GetReadableObjectName(objCreatedData.type))
            .append(" (").append(std::to_string(objCreatedData.type)).append(")");
      }
      case osm::JournalEntryType::LegacyObject:
      {
        LegacyObjData const & legacyObjData = std::get<LegacyObjData>(journalEntry.data);
        return ToString(journalEntry.journalEntryType)
            .append(": version=\"").append(legacyObjData.version).append("\"");
      }
    }
  }

  std::string EditJournal::ToString(osm::JournalEntryType journalEntryType)
  {
    switch (journalEntryType)
    {
      case osm::JournalEntryType::TagModification:
        return "TagModification";
      case osm::JournalEntryType::ObjectCreated:
        return "ObjectCreated";
      case osm::JournalEntryType::LegacyObject:
        return "LegacyObject";
    }
  }

  std::optional<JournalEntryType> EditJournal::TypeFromString(std::string const & entryType) {
    if (entryType == "TagModification")
      return JournalEntryType::TagModification;
    else if (entryType == "ObjectCreated")
      return JournalEntryType::ObjectCreated;
    else if (entryType == "LegacyObject")
      return JournalEntryType::LegacyObject;
    else
      return {};
  }
}
