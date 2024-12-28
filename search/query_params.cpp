#include "search/query_params.hpp"

#include "search/ranking_utils.hpp"
#include "search/token_range.hpp"

#include <map>
#include <sstream>

namespace search
{
using namespace std;

namespace
{
// All synonyms should be lowercase.

/// @todo These should check the map language and use only the corresponding translation.
map<string, vector<string>> const kSynonyms = {
    {"n",    {"north"}},
    {"w",    {"west"}},
    {"s",    {"south"}},
    {"e",    {"east"}},
    {"nw",   {"northwest"}},
    {"ne",   {"northeast"}},
    {"sw",   {"southwest"}},
    {"se",   {"southeast"}},

    /// @todo Should not duplicate Street synonyms defined in StreetsSynonymsHolder (avoid useless double queries).
    /// Remove "street" and "avenue" here, but should update GetNameScore.
    {"st",   {"saint", "street"}},
    {"dr",   {"doctor"}},

    // widely used in LATAM, like "Ntra Sra Asuncion Zelaya"
    {"ntra",  {"nuestra"}},
    {"sra",   {"senora"}},
    {"sta",   {"santa"}},

    {"al",    {"allee", "alle"}},
    {"ave",   {"avenue"}},
    /// @todo Should process synonyms with errors like "blvrd" -> "blvd".
    /// @see HouseOnStreetSynonymsWithMisprints test.
    {"blvd",  {"boulevard"}},
    {"blvrd", {"boulevard"}},
    {"cir",   {"circle"}},
    {"ct",    {"court"}},
    {"hwy",   {"highway"}},
    {"pl",    {"place", "platz"}},
    {"rt",    {"route"}},
    {"sq",    {"square"}},

    {"ал",    {"аллея", "алея"}},
    {"бул",   {"бульвар"}},
    {"зав",   {"завулак"}},
    {"кв",    {"квартал"}},
    {"наб",   {"набережная", "набярэжная", "набережна"}},
    {"пер",   {"переулок"}},
    {"пл",    {"площадь", "площа"}},
    {"пр",    {"проспект", "праспект", "провулок", "проезд", "праезд", "проїзд"}},
    {"туп",   {"тупик", "тупік"}},
    {"ш",     {"шоссе", "шаша", "шосе"}},

    {"cd",    {"caddesi"}},

    {"св",   {"святой", "святого", "святая", "святые", "святых", "свято"}},
    {"б",    {"большая", "большой"}},
    {"бол",  {"большая", "большой"}},
    {"м",    {"малая", "малый"}},
    {"мал",  {"малая", "малый"}},
    {"нов",  {"новая", "новый"}},
    {"стар", {"старая", "старый"}},
};
}  // namespace

// QueryParams::Token ------------------------------------------------------------------------------
void QueryParams::Token::AddSynonym(string const & s)
{
  AddSynonym(strings::MakeUniString(s));
}

void QueryParams::Token::AddSynonym(String const & s)
{
  if (!IsStopWord(s))
    m_synonyms.push_back(s);
}

string DebugPrint(QueryParams::Token const & token)
{
  ostringstream os;
  os << "Token [ m_original=" << DebugPrint(token.GetOriginal())
     << ", m_synonyms=" << DebugPrint(token.m_synonyms) << " ]";
  return os.str();
}

// QueryParams -------------------------------------------------------------------------------------
void QueryParams::ClearStreetIndices()
{
  class AdditionalCommonTokens
  {
    set<String> m_strings;
  public:
    AdditionalCommonTokens()
    {
      char const * arr[] = {
        "the",                      // English
        "der", "zum", "und", "auf", // German
        "del", "les",               // Spanish
        "в", "на"                   // Cyrillic
      };
      for (char const * s : arr)
        m_strings.insert(NormalizeAndSimplifyString(s));
    }
    bool Has(String const & s) const { return m_strings.count(s) > 0; }
  };
  static AdditionalCommonTokens const s_addCommonTokens;

  size_t const count = GetNumTokens();
  m_isCommonToken.resize(count, false);

  for (size_t i = 0; i < count; ++i)
  {
    auto const & token = GetToken(i).GetOriginal();
    if (IsStreetSynonym(token))
    {
      m_typeIndices[i].clear();
      m_isCommonToken[i] = true;
    }
    else if (s_addCommonTokens.Has(token))
      m_isCommonToken[i] = true;
  }
}

void QueryParams::Clear()
{
  m_tokens.clear();
  m_prefixToken.Clear();
  m_hasPrefix = false;
  m_isCommonToken.clear();
  m_typeIndices.clear();
  m_langs.Clear();
}

bool QueryParams::IsCategorySynonym(size_t i) const { return !GetTypeIndices(i).empty(); }

QueryParams::TypeIndices & QueryParams::GetTypeIndices(size_t i)
{
  ASSERT_LESS(i, GetNumTokens(), ());
  return m_typeIndices[i];
}

QueryParams::TypeIndices const & QueryParams::GetTypeIndices(size_t i) const
{
  ASSERT_LESS(i, GetNumTokens(), ());
  return m_typeIndices[i];
}

bool QueryParams::IsPrefixToken(size_t i) const
{
  ASSERT_LESS(i, GetNumTokens(), ());
  return i == m_tokens.size();
}

QueryParams::Token const & QueryParams::GetToken(size_t i) const
{
  ASSERT_LESS(i, GetNumTokens(), ());
  return i < m_tokens.size() ? m_tokens[i] : m_prefixToken;
}

QueryParams::Token & QueryParams::GetToken(size_t i)
{
  ASSERT_LESS(i, GetNumTokens(), ());
  return i < m_tokens.size() ? m_tokens[i] : m_prefixToken;
}

bool QueryParams::IsCommonToken(size_t i) const
{
  return i < m_isCommonToken.size() && m_isCommonToken[i];
}

bool QueryParams::IsNumberTokens(TokenRange const & range) const
{
  ASSERT(range.IsValid(), (range));
  ASSERT_LESS_OR_EQUAL(range.End(), GetNumTokens(), ());

  for (size_t i : range)
  {
    if (!GetToken(i).AnyOfOriginalOrSynonyms([](String const & s) { return strings::IsASCIINumeric(s); }))
      return false;
  }

  return true;
}

void QueryParams::RemoveToken(size_t i)
{
  ASSERT_LESS(i, GetNumTokens(), ());
  if (i == m_tokens.size())
  {
    m_prefixToken.Clear();
    m_hasPrefix = false;
  }
  else
  {
    m_tokens.erase(m_tokens.begin() + i);
  }
  m_typeIndices.erase(m_typeIndices.begin() + i);
}

void QueryParams::AddSynonyms()
{
  for (auto & token : m_tokens)
  {
    string const ss = ToUtf8(MakeLowerCase(token.GetOriginal()));
    auto const it = kSynonyms.find(ss);
    if (it != kSynonyms.end())
    {
      for (auto const & synonym : it->second)
        token.AddSynonym(synonym);
    }
  }
  if (m_hasPrefix)
  {
    string const ss = ToUtf8(MakeLowerCase(m_prefixToken.GetOriginal()));
    auto const it = kSynonyms.find(ss);
    if (it != kSynonyms.end())
    {
      for (auto const & synonym : it->second)
        m_prefixToken.AddSynonym(synonym);
    }
  }
}

string DebugPrint(QueryParams const & params)
{
  ostringstream os;
  os << boolalpha << "QueryParams "
     << "{ m_tokens: " << ::DebugPrint(params.m_tokens)
     << ", m_prefixToken: " << DebugPrint(params.m_prefixToken)
     << ", m_typeIndices: " << ::DebugPrint(params.m_typeIndices)
     << ", m_langs: " << DebugPrint(params.m_langs)
     << ", m_isCommonToken: " << ::DebugPrint(params.m_isCommonToken)
     << " }";
  return os.str();
}
}  // namespace search
