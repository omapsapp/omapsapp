#pragma once

#include "search/common.hpp"
#include "search/query_params.hpp"

#include "indexer/search_string_utils.hpp"

#include "base/string_utils.hpp"

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

class CategoriesHolder;

namespace feature
{
class TypesHolder;
}

namespace search
{
class QueryParams;
class TokenSlice;

class CategoriesInfo
{
public:
  CategoriesInfo(feature::TypesHolder const & holder, TokenSlice const & tokens,
                 Locales const & locales, CategoriesHolder const & categories);

  size_t GetMatchedLength() const { return m_matchedLength; }

  /// @return true when all tokens correspond to categories in |holder|.
  bool IsPureCategories() const { return m_pureCategories; }

  /// @return true when all tokens are categories tokens but none of them correspond to categories in |holder|.
  bool IsFalseCategories() const { return m_falseCategories; }

private:
  size_t m_matchedLength = 0;
  bool m_pureCategories = false;
  bool m_falseCategories = false;
};

struct ErrorsMade
{
  static uint16_t constexpr kInfiniteErrors = std::numeric_limits<uint16_t>::max();

  ErrorsMade() = default;
  explicit ErrorsMade(uint16_t errorsMade) : m_errorsMade(errorsMade) {}

  bool IsValid() const { return m_errorsMade != kInfiniteErrors; }
  bool IsZero() const { return m_errorsMade == 0; }
  bool IsBetterThan(ErrorsMade rhs) const { return m_errorsMade < rhs.m_errorsMade; }

  template <typename Fn>
  static ErrorsMade Combine(ErrorsMade const & lhs, ErrorsMade const & rhs, Fn && fn)
  {
    if (!lhs.IsValid())
      return rhs;
    if (!rhs.IsValid())
      return lhs;
    return ErrorsMade(fn(lhs.m_errorsMade, rhs.m_errorsMade));
  }

  static ErrorsMade Min(ErrorsMade const & lhs, ErrorsMade const & rhs)
  {
    return Combine(lhs, rhs, [](uint16_t u, uint16_t v) { return std::min(u, v); });
  }

  static ErrorsMade Max(ErrorsMade const & lhs, ErrorsMade const & rhs)
  {
    return Combine(lhs, rhs, [](uint16_t u, uint16_t v) { return std::max(u, v); });
  }

  friend ErrorsMade operator+(ErrorsMade const & lhs, ErrorsMade const & rhs)
  {
    return Combine(lhs, rhs, [](uint16_t u, uint16_t v) { return u + v; });
  }

  ErrorsMade & operator+=(ErrorsMade const & rhs)
  {
    *this = *this + rhs;
    return *this;
  }

  bool operator==(ErrorsMade const & rhs) const { return m_errorsMade == rhs.m_errorsMade; }

  uint16_t m_errorsMade = kInfiniteErrors;
};

std::string DebugPrint(ErrorsMade const & errorsMade);

namespace impl
{
// Returns the minimum number of errors needed to match |text| with |token|.
// If it's not possible in accordance with GetMaxErrorsForToken(|text|), returns kInfiniteErrors.
/// @param[in]  dfa DFA for |text|
ErrorsMade GetErrorsMade(QueryParams::Token const & token,
                         strings::UniString const & text, strings::LevenshteinDFA const & dfa);
ErrorsMade GetPrefixErrorsMade(QueryParams::Token const & token,
                               strings::UniString const & text, strings::LevenshteinDFA const & dfa);
}  // namespace impl

// The order and numeric values are important here. Please, check all use-cases before changing this enum.
enum class NameScore : uint8_t
{
  // example name = "Carrefour Mini"
                  // example query:
  ZERO = 0,       // Rewe
  SUBSTRING,      // Mini
  PREFIX,         // Carref
  FIRST_MATCH,    // Carrefour Maxi
  FULL_PREFIX,    // Carrefour
  FULL_MATCH,     // Carrefour Mini

  COUNT
};

struct NameScores
{
  NameScores() = default;
  NameScores(NameScore nameScore, ErrorsMade const & errorsMade, bool isAltOrOldName, size_t matchedLength)
    : m_nameScore(nameScore), m_errorsMade(errorsMade), m_isAltOrOldName(isAltOrOldName), m_matchedLength(matchedLength)
  {
  }

  void UpdateIfBetter(NameScores const & rhs)
  {
    auto newNameScoreIsBetter = m_nameScore < rhs.m_nameScore;
    // FULL_PREFIX with 0 errors is better than FULL_MATCH with 2 errors.
    if (newNameScoreIsBetter && m_nameScore == NameScore::FULL_PREFIX && m_errorsMade.IsBetterThan(rhs.m_errorsMade))
      newNameScoreIsBetter = false;

    // FULL_PREFIX with !alt_old_name) is better than FULL_MATCH with alt_old_name.
    if (!m_isAltOrOldName && rhs.m_isAltOrOldName &&
        !rhs.m_errorsMade.IsBetterThan(m_errorsMade) &&
        (int)rhs.m_nameScore - (int)m_nameScore < 2)
    {
      newNameScoreIsBetter = false;
    }

    /// @todo What should we do with alt_old_name and errors==0 vs !alt_old_name and errors==1?

    auto const nameScoresAreEqual = m_nameScore == rhs.m_nameScore;
    auto const newLanguageIsBetter = m_isAltOrOldName && !rhs.m_isAltOrOldName;
    auto const languagesAreEqual = m_isAltOrOldName == rhs.m_isAltOrOldName;
    auto const newMatchedLengthIsBetter = m_matchedLength < rhs.m_matchedLength;
    // It's okay to pick a slightly worse matched length if other scores are better.
    auto const matchedLengthsAreSimilar = (m_matchedLength - m_matchedLength / 4) <= rhs.m_matchedLength;

    if (newMatchedLengthIsBetter ||
       (matchedLengthsAreSimilar && newNameScoreIsBetter) ||
       (matchedLengthsAreSimilar && nameScoresAreEqual && newLanguageIsBetter))
    {
      m_nameScore = rhs.m_nameScore;
      m_errorsMade = rhs.m_errorsMade;
      m_isAltOrOldName = rhs.m_isAltOrOldName;
      m_matchedLength = rhs.m_matchedLength;
      return;
    }
    if (matchedLengthsAreSimilar && nameScoresAreEqual && languagesAreEqual)
      m_errorsMade = ErrorsMade::Min(m_errorsMade, rhs.m_errorsMade);
  }

  bool operator==(NameScores const & rhs) const
  {
    return m_nameScore == rhs.m_nameScore && m_errorsMade == rhs.m_errorsMade &&
           m_isAltOrOldName == rhs.m_isAltOrOldName && m_matchedLength == rhs.m_matchedLength;
  }

  NameScore m_nameScore = NameScore::ZERO;
  ErrorsMade m_errorsMade;
  bool m_isAltOrOldName = false;
  size_t m_matchedLength = 0;
};

std::string DebugPrint(NameScore const & score);
std::string DebugPrint(NameScores const & scores);

// Returns true when |s| is a stop-word and may be removed from a query.
bool IsStopWord(strings::UniString const & s);


class TokensVector
{
  std::vector<strings::UniString> m_tokens;
  std::vector<strings::LevenshteinDFA> m_dfas;

private:
  void Init()
  {
    m_dfas.resize(m_tokens.size());
  }

public:
  TokensVector() = default;
  explicit TokensVector(std::string_view name);
  explicit TokensVector(std::vector<strings::UniString> && tokens) : m_tokens(std::move(tokens))
  {
    Init();
  }

  std::vector<strings::UniString> const & GetTokens() const { return m_tokens; }
  size_t Size() const { return m_tokens.size(); }
  strings::UniString const & Token(size_t i) const { return m_tokens[i]; }
  strings::LevenshteinDFA const & DFA(size_t i)
  {
    if (m_dfas[i].IsEmpty())
      m_dfas[i] = BuildLevenshteinDFA(m_tokens[i]);
    return m_dfas[i];
  }
};


/// @param[in]  tokens  Feature's name (splitted on tokens, without delimiters) to match.
/// @param[in]  slice   Input query.
/// @todo Should make the honest recurrent score calculation like:
/// F(iToken, iSlice) = {
///   score = max(F(iToken + 1, iSlice), F(iToken, iSlice + 1));
///   if (Match(tokens[iToken], slice[iSlice]))
///     score = max(score, Score(tokens[iToken], slice[iSlice]) + F(iToken + 1, iSlice + 1));
///   return score;
/// }
template <typename Slice>
NameScores GetNameScores(TokensVector & tokens, uint8_t lang, Slice const & slice)
{
  if (slice.Empty())
    return {};

  NameScores scores;
  size_t const tokenCount = tokens.Size();
  size_t const sliceCount = slice.Size();

  // Try matching words between token and slice, iterating over offsets.
  // We want to try all possible offsets of the slice and token lists
  // When offset = 0, the last token in tokens is compared to the first in slice.
  // When offset = sliceCount + tokenCount, the last token
  // in slice is compared to the first in tokens.
  // Feature names and queries aren't necessarily index-aligned, so it's important
  // to "slide" the feature name along the query to look for matches.
  // For instance,
  // "Pennsylvania Ave NW, Washington, DC"
  // "1600 Pennsylvania Ave"
  // doesn't match at all, but
  //      "Pennsylvania Ave NW, Washington, DC"
  // "1600 Pennsylvania Ave"
  // is a partial match. Fuzzy matching helps match buildings
  // missing addresses in OSM, and it helps be more flexible in general.
  for (size_t offset = 0; offset < sliceCount + tokenCount; ++offset)
  {
    // Reset error and match-length count for each offset attempt.
    ErrorsMade totalErrorsMade(0);
    int matchedLength = 0;
    // Highest quality namescore possible for this offset
    NameScore nameScore = NameScore::SUBSTRING;
    // Prefix & full matches must test starting at the same index. (tokenIndex == i)
    if (offset + 1 == tokenCount)
    {
      if (sliceCount == tokenCount)
        nameScore = NameScore::FULL_MATCH;
      else
        nameScore = NameScore::FULL_PREFIX;
    }
    bool isAltOrOldName = false;

    auto const isFullScore = [&nameScore]()
    {
      return (nameScore == NameScore::FULL_MATCH || nameScore == NameScore::FULL_PREFIX);
    };

    // Iterate through the entire slice. Incomplete matches can still be good.
    // Using this slice & token as an example:
    //                              0   1   2   3   4   5   6
    // slice count=7:              foo bar baz bot bop bip bla
    // token count=3:      bar baz bot
    //
    // When offset = 0, tokenIndex should start at +2:
    //                 0   1   2   3   4   5   6
    // slice =         foo bar baz bot bop bip bla
    // token = baz bot bop
    //          0   1   2
    //
    // Offset must run to 8 to test all potential matches. (slice + token - 1)
    // Making tokenIndex start at -6 (-sliceSize)
    //                  0   1   2   3   4   5   6
    // slice =         foo bar baz bot bop bip bla
    // token =                                 baz bot bop
    //                 -6  -5  -4  -3  -2  -1   0   1   2

    size_t iToken, iSlice;
    for (size_t i = std::max(0, int(offset) + 1 - int(tokenCount));
                i < std::min(sliceCount, offset + 1); ++i)
    {
      size_t const tIdx = i + tokenCount - 1 - offset;

      // Count the errors. If GetErrorsMade finds a match, count it towards
      // the matched length and check against the prior best.
      auto errorsMade = impl::GetErrorsMade(slice.Get(i), tokens.Token(tIdx), tokens.DFA(tIdx));

      // Also, check like prefix, if we've got token-like matching errors.
      if (!errorsMade.IsZero() && slice.IsPrefix(i))
      {
        auto const prefixErrors = impl::GetPrefixErrorsMade(slice.Get(i), tokens.Token(tIdx), tokens.DFA(tIdx));
        if (prefixErrors.IsBetterThan(errorsMade))
        {
          // NameScore::PREFIX with less errors is better than NameScore::FULL_MATCH.
          /// @see RankingInfo_PrefixVsFull test.
          errorsMade = prefixErrors;
          if (isFullScore())
            nameScore = NameScore::PREFIX;
        }
      }

      if (!errorsMade.IsValid())
      {
        // This block is responsibe for: name = "X Y {Z}", query = "X Z" => score is FIRST_MATCHED.
        if (matchedLength > 0 && isFullScore())
        {
          nameScore = NameScore::FIRST_MATCH;
          isAltOrOldName = StringUtf8Multilang::IsAltOrOldName(lang);
        }
        else
        {
          // If any token mismatches, this is at best a substring match.
          nameScore = NameScore::SUBSTRING;
        }
      }
      else
      {
        // Update the match quality
        totalErrorsMade += errorsMade;
        matchedLength += slice.Get(i).GetOriginal().size();
        isAltOrOldName = StringUtf8Multilang::IsAltOrOldName(lang);

        iToken = tIdx;
        iSlice = i;
      }
    }

    if (matchedLength == 0)
    {
      nameScore = NameScore::ZERO;
      totalErrorsMade = ErrorsMade();
    }
    else
    {
      // Update |matchedLength| and |totalErrorsMade| for slice -> tokens tail (after last valid match).
      while (++iSlice < sliceCount)
      {
        while (++iToken < tokenCount)
        {
          auto const errorsMade = impl::GetErrorsMade(slice.Get(iSlice), tokens.Token(iToken), tokens.DFA(iToken));
          if (errorsMade.IsValid())
          {
            totalErrorsMade += errorsMade;
            matchedLength += slice.Get(iSlice).GetOriginal().size();
            break;
          }
        }
      }
    }

    scores.UpdateIfBetter(NameScores(nameScore, totalErrorsMade, isAltOrOldName, matchedLength));
  }

  // Uncomment for verbose search logging.
  // LOG(LDEBUG, ("Match quality", scores, "from", tokens, "into", slice));
  return scores;
}

template <typename Slice>
NameScores GetNameScores(std::string_view name, uint8_t lang, Slice const & slice)
{
  TokensVector tokens(name);
  return GetNameScores(tokens, lang, slice);
}
}  // namespace search
