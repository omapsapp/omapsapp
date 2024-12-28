#include "search/highlighting.hpp"

namespace search
{
namespace
{
// Makes continuous range for tokens and prefix.
template <class Iter, class Value>
class CombinedIterator
{
  Iter m_cur;
  Iter m_end;
  Value const * m_val;

public:
  CombinedIterator(Iter cur, Iter end, Value const * val) : m_cur(cur), m_end(end), m_val(val) {}

  Value const & operator*() const
  {
    ASSERT(m_val != nullptr || m_cur != m_end, ("dereferencing of an empty iterator"));
    if (m_cur != m_end)
      return *m_cur;

    return *m_val;
  }

  CombinedIterator & operator++()
  {
    if (m_cur != m_end)
      ++m_cur;
    else
      m_val = nullptr;
    return *this;
  }

  bool operator==(CombinedIterator const & other) const
  {
    return m_val == other.m_val && m_cur == other.m_cur;
  }

  bool operator!=(CombinedIterator const & other) const { return !(*this == other); }
};
}  // namespace


void HighlightResult(QueryTokens const & tokens, strings::UniString const & prefix, Result & res)
{
  using Iter = QueryTokens::const_iterator;
  using CombinedIter = CombinedIterator<Iter, strings::UniString>;

  CombinedIter beg(tokens.begin(), tokens.end(), prefix.empty() ? nullptr : &prefix);
  CombinedIter end(tokens.end() /* cur */, tokens.end() /* end */, nullptr);

  // Highlight Title
  SearchStringTokensIntersectionRanges(res.GetString(), beg, end,
                                       [&](std::pair<uint16_t, uint16_t> const & range)
  {
    res.AddHighlightRange(range);
  });

  // Highlight description.
  SearchStringTokensIntersectionRanges(res.GetAddress(), beg, end,
                                       [&](std::pair<uint16_t, uint16_t> const & range)
  {
    res.AddDescHighlightRange(range);
  });
}
}  // namespace search
