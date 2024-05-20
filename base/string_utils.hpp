#pragma once

#include "base/buffer_vector.hpp"
#include "base/checked_cast.hpp"
#include "base/stl_helpers.hpp"

#include <algorithm>
#include <charconv>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iterator>
#include <limits>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

#include <utf8/unchecked.h>

/// All methods work with strings in utf-8 format
namespace strings
{
using UniChar = char32_t;
// typedef buffer_vector<UniChar, 32> UniString;

/// Make new type, not typedef. Need to specialize DebugPrint.
class UniString : public buffer_vector<UniChar, 32>
{
  using BaseT = buffer_vector<UniChar, 32>;

public:
  using value_type = UniChar;

  UniString() = default;
  explicit UniString(size_t n) : BaseT(n) {}
  UniString(size_t n, UniChar c) { resize(n, c); }

  template <typename Iter>
  UniString(Iter b, Iter e) : BaseT(b, e)
  {
  }

  bool IsEqualAscii(char const * s) const;

  UniString & operator+=(UniString const & rhs)
  {
    append(rhs);
    return *this;
  }

  UniString operator+(UniString const & rhs) const
  {
    UniString result(*this);
    result += rhs;
    return result;
  }

  template <class Iter>
  void Replace(iterator first, iterator last, Iter first2, Iter last2)
  {
    auto it = first;
    auto it2 = first2;
    for (; it < last && it2 < last2; ++it, ++it2)
      *it = *it2;

    if (it == last && it2 == last2)
      return;

    if (it == last)
    {
      insert(it, it2, last2);
      return;
    }

    erase(it, last);
  }
};

/// Performs full case folding for string to make it search-compatible according
/// to rules in ftp://ftp.unicode.org/Public/UNIDATA/CaseFolding.txt
/// For implementation @see base/lower_case.cpp
void MakeLowerCaseInplace(UniString & s);
UniString MakeLowerCase(UniString s);

/// Performs NFKD - Compatibility decomposition for Unicode according
/// to rules in ftp://ftp.unicode.org/Public/UNIDATA/UnicodeData.txt
/// For implementation @see base/normalize_unicode.cpp
void NormalizeInplace(UniString & s);

UniString Normalize(UniString s);
std::string Normalize(std::string const & s);

/// Replaces "full width" unicode digits with ascii ones.
void NormalizeDigits(std::string & utf8);
void NormalizeDigits(UniString & us);

/// Counts number of start symbols in string s (that is not lower and not normalized) that maches
/// to lower and normalized string low_s. If s doen't starts with low_s then returns 0; otherwise
/// returns number of start symbols in s that equivalent to lowStr
/// For implementation @see base/lower_case.cpp
size_t CountNormLowerSymbols(UniString const & s, UniString const & lowStr);

void AsciiToLower(std::string & s);

// All triming functions return a reference on an input string.
// They do in-place trimming. In general, it does not work for any unicode whitespace except
// ASCII U+0020 one.
void Trim(std::string & s);
void Trim(std::string_view & sv);
/// Remove any characters that contain in "anyOf" on left and right side of string s
void Trim(std::string & s, std::string_view anyOf);

// Replace the first match of the search substring in the input with the format string.
// str - An input string
// from - A substring to be searched for
// to - A substitute string
bool ReplaceFirst(std::string & str, std::string const & from, std::string const & to);

bool ReplaceLast(std::string & str, std::string const & from, std::string const & to);

void MakeLowerCaseInplace(std::string & s);
std::string MakeLowerCase(std::string s);
bool EqualNoCase(std::string const & s1, std::string const & s2);

UniString MakeUniString(std::string_view utf8s);
std::string ToUtf8(UniString const & s);
std::u16string ToUtf16(std::string_view utf8);
bool IsASCIIString(std::string_view sv);
bool IsASCIIDigit(UniChar c);
template <class StringT> bool IsASCIINumeric(StringT const & str)
{
  return !std::empty(str) && std::all_of(std::begin(str), std::end(str), &IsASCIIDigit);
}
inline bool IsASCIINumeric(char const * s) { return IsASCIINumeric(std::string_view(s)); }
bool IsASCIISpace(UniChar c);
bool IsASCIILatin(UniChar c);

inline std::string DebugPrint(UniString const & s) { return ToUtf8(s); }

template <typename DelimFn, typename Iter> class TokenizeIteratorBase
{
public:
  using difference_type = std::ptrdiff_t;
  using iterator_category = std::input_iterator_tag;

  // Hack to get buffer pointer from any iterator.
  // Deliberately made non-static to simplify the call like this->ToCharPtr.
  char const * ToCharPtr(char const * p) const { return p; }
  template <class T> auto ToCharPtr(T const & i) const { return ToCharPtr(i.base()); }
};

template <typename DelimFn, typename Iter, bool KeepEmptyTokens = false>
class TokenizeIterator : public TokenizeIteratorBase<DelimFn, Iter>
{
public:
  template <class InIterT> TokenizeIterator(InIterT beg, InIterT end, DelimFn const & delimFn)
    : m_start(beg), m_end(beg), m_finish(end), m_delimFn(delimFn)
  {
    Move();
  }

  std::string_view operator*() const
  {
    ASSERT(m_start != m_finish, ("Dereferencing of empty iterator."));

    auto const baseI = this->ToCharPtr(m_start);
    return std::string_view(baseI, std::distance(baseI, this->ToCharPtr(m_end)));
  }

  UniString GetUniString() const
  {
    ASSERT(m_start != m_finish, ("Dereferencing of empty iterator."));
    return UniString(m_start, m_end);
  }

  explicit operator bool() const { return m_start != m_finish; }

  TokenizeIterator & operator++()
  {
    Move();
    return *this;
  }

  bool operator==(TokenizeIterator const & rhs) const
  {
    if (!*this && !rhs)
      return true;
    if (*this && rhs)
      return m_start == rhs.m_start && m_end == rhs.m_end && m_finish == rhs.m_finish;
    return false;
  }

  bool operator!=(TokenizeIterator const & rhs) const { return !(*this == rhs); }

private:
  void Move()
  {
    m_start = m_end;
    while (m_start != m_finish && m_delimFn(*m_start))
      ++m_start;

    m_end = m_start;
    while (m_end != m_finish && !m_delimFn(*m_end))
      ++m_end;
  }

  // Token is defined as a pair (|m_start|, |m_end|), where:
  //
  // * m_start < m_end
  // * m_start == begin or m_delimFn(m_start - 1)
  // * m_end == m_finish or m_delimFn(m_end)
  // * for all i from [m_start, m_end): !m_delimFn(i)
  //
  // This version of TokenizeIterator iterates over all tokens and
  // keeps the invariant above.
  Iter m_start;
  Iter m_end;

  // The end of the string the iterator iterates over.
  Iter m_finish;

  DelimFn m_delimFn;
};

/// Used in ParseCSVRow for the generator routine.
template <typename DelimFn, typename Iter>
class TokenizeIterator<DelimFn, Iter, true /* KeepEmptyTokens */> : public TokenizeIteratorBase<DelimFn, Iter>
{
public:
  template <class InIterT> TokenizeIterator(InIterT beg, InIterT end, DelimFn const & delimFn)
    : m_start(beg), m_end(beg), m_finish(end), m_delimFn(delimFn), m_finished(false)
  {
    while (m_end != m_finish && !m_delimFn(*m_end))
      ++m_end;
  }

  std::string_view operator*() const
  {
    ASSERT(!m_finished, ("Dereferencing of empty iterator."));

    auto const baseI = this->ToCharPtr(m_start);
    return std::string_view(baseI, std::distance(baseI, this->ToCharPtr(m_end)));
  }

  explicit operator bool() const { return !m_finished; }

  TokenizeIterator & operator++()
  {
    Move();
    return *this;
  }

  bool operator==(TokenizeIterator const & rhs) const
  {
    if (!*this && !rhs)
      return true;
    if (*this && rhs)
    {
      return m_start == rhs.m_start && m_end == rhs.m_end && m_finish == rhs.m_finish &&
             m_finished == rhs.m_finished;
    }
    return false;
  }

  bool operator!=(TokenizeIterator const & rhs) const { return !(*this == rhs); }

private:
  void Move()
  {
    if (m_end == m_finish)
    {
      ASSERT(!m_finished, ());
      m_start = m_end = m_finish;
      m_finished = true;
      return;
    }

    m_start = m_end;
    ++m_start;

    m_end = m_start;
    while (m_end != m_finish && !m_delimFn(*m_end))
      ++m_end;
  }

  // Token is defined as a pair (|m_start|, |m_end|), where:
  //
  // * m_start <= m_end
  // * m_start == begin or m_delimFn(m_start - 1)
  // * m_end == m_finish or m_delimFn(m_end)
  // * for all i from [m_start, m_end): !m_delimFn(i)
  //
  // This version of TokenizeIterator iterates over all tokens and
  // keeps the invariant above.
  Iter m_start;
  Iter m_end;

  // The end of the string the iterator iterates over.
  Iter m_finish;

  DelimFn m_delimFn;

  // When true, iterator is at the end position and is not valid anymore.
  bool m_finished;
};

class SimpleDelimiter
{
  UniString m_delims;

public:
  SimpleDelimiter(char const * delims);
  SimpleDelimiter(char delim);

  // Returns true iff |c| is a delimiter.
  bool operator()(UniChar c) const;
};

template <class StringT, bool KeepEmptyTokens = false>
class SimpleTokenizer : public
    TokenizeIterator<SimpleDelimiter, ::utf8::unchecked::iterator<typename StringT::const_iterator>, KeepEmptyTokens>
{
  using BaseT = TokenizeIterator<SimpleDelimiter, ::utf8::unchecked::iterator<typename StringT::const_iterator>, KeepEmptyTokens>;
public:
  SimpleTokenizer(StringT const & str, SimpleDelimiter const & delims)
    : BaseT(str.begin(), str.end(), delims)
  {
  }
};

template <typename TFunctor, bool KeepEmptyTokens = false>
void Tokenize(std::string_view str, char const * delims, TFunctor && f)
{
  SimpleTokenizer<std::string_view, KeepEmptyTokens> iter(str, delims);
  while (iter)
  {
    f(*iter);
    ++iter;
  }
}

/// @note Lifetime of return container is the same as \a str lifetime. Avoid temporary input.
template <class ResultT = std::string_view, bool KeepEmtyTokens = false>
std::vector<ResultT> Tokenize(std::string_view str, char const * delims)
{
  std::vector<ResultT> c;
  auto f = [&c](std::string_view v) { c.push_back(ResultT(v)); };
  Tokenize<decltype(f), KeepEmtyTokens>(str, delims, std::move(f));
  return c;
}

/// Splits a string by the delimiter, keeps empty parts, on an empty string returns an empty vector.
/// Does not support quoted columns, newlines in columns and escaped quotes.
void ParseCSVRow(std::string const & s, char const delimiter, std::vector<std::string> & target);

/// @return code of last symbol in string or 0 if s is empty
UniChar LastUniChar(std::string const & s);

/// @name From string to numeric.
//@{
namespace internal
{
template <typename T, typename = std::enable_if_t<std::is_signed<T>::value &&
                                                  sizeof(T) < sizeof(long long)>>
long IntConverter(char const * start, char ** stop, int base)
{
  return std::strtol(start, stop, base);
}

template <typename T, typename = std::enable_if_t<std::is_unsigned<T>::value &&
                                                  sizeof(T) < sizeof(unsigned long long)>>
unsigned long IntConverter(char const * start, char ** stop, int base)
{
  return std::strtoul(start, stop, base);
}

template <typename T, typename = std::enable_if_t<std::is_signed<T>::value &&
                                                  sizeof(T) == sizeof(long long)>>
long long IntConverter(char const * start, char ** stop, int base)
{
#ifdef OMIM_OS_WINDOWS_NATIVE
  return _strtoi64(start, &stop, base);
#else
  return std::strtoll(start, stop, base);
#endif
}

template <typename T, typename = std::enable_if_t<std::is_unsigned<T>::value &&
                                                  sizeof(T) == sizeof(unsigned long long)>>
unsigned long long IntConverter(char const * start, char ** stop, int base)
{
#ifdef OMIM_OS_WINDOWS_NATIVE
  return _strtoui64(start, &stop, base);
#else
  return std::strtoull(start, stop, base);
#endif
}

template <typename T,
          typename = std::enable_if_t<std::is_integral<T>::value>>
bool ToInteger(char const * start, T & result, int base = 10)
{
  char * stop;
  errno = 0;  // Library functions do not reset it.

  auto const v = IntConverter<T>(start, &stop, base);

  if (errno == EINVAL || errno == ERANGE || *stop != 0 || start == stop ||
      !base::IsCastValid<T>(v))
  {
    errno = 0;
    return false;
  }

  result = static_cast<T>(v);
  return true;
}
}  // namespace internal

[[nodiscard]] inline bool to_int(char const * s, int & i, int base = 10)
{
  return internal::ToInteger(s, i, base);
}

[[nodiscard]] inline bool to_uint(char const * s, unsigned int & i, int base = 10)
{
  return internal::ToInteger(s, i, base);
}

[[nodiscard]] inline bool to_int16(char const * s, short & i, int base = 10)
{
  return internal::ToInteger(s, i, base);
}

[[nodiscard]] inline bool to_uint16(char const * s, unsigned short & i, int base = 10)
{
  return internal::ToInteger(s, i, base);
}

// Note: negative values will be converted too.
// For ex.:
//  "-1" converts to std::numeric_limits<uint64_t>::max();
//  "-2" converts to std::numeric_limits<uint64_t>::max() - 1;
//  "-3" converts to std::numeric_limits<uint64_t>::max() - 2;
//  ...
// negative std::numeric_limits<uint64_t>::max() converts to 1.
// Values lower than negative std::numeric_limits<uint64_t>::max()
// are not convertible (method returns false).
[[nodiscard]] inline bool to_uint64(char const * s, uint64_t & i, int base = 10)
{
  return internal::ToInteger(s, i, base);
}

[[nodiscard]] inline bool to_int64(char const * s, int64_t & i)
{
  return internal::ToInteger(s, i);
}

// Unlike the 64-bit version, to_uint32 is not guaranteed to convert negative values.
// Current implementation conflates fixed-width types (uint32, uint64) with types that have no
// guarantees on their exact sizes (unsigned long, unsigned long long) so results of internal
// conversions may differ between platforms.
// Converting strings representing negative numbers to unsigned integers looks like a bad
// idea anyway and it's not worth changing the implementation solely for this reason.
[[nodiscard]] inline bool to_uint32(char const * s, uint32_t & i, int base = 10)
{
  return internal::ToInteger(s, i, base);
}

[[nodiscard]] inline bool to_int32(char const * s, int32_t & i)
{
  return internal::ToInteger(s, i);
}

[[nodiscard]] bool to_size_t(char const * s, size_t & i, int base = 10);
// Both functions return false for INF, NAN, numbers like "1." and "0.4 ".
[[nodiscard]] bool to_float(char const * s, float & f);
[[nodiscard]] bool to_double(char const * s, double & d);
[[nodiscard]] bool is_finite(double d);

[[nodiscard]] inline bool to_int(std::string const & s, int & i, int base = 10)
{
  return to_int(s.c_str(), i, base);
}
[[nodiscard]] inline bool to_uint(std::string const & s, unsigned int & i, int base = 10)
{
  return to_uint(s.c_str(), i, base);
}

// Note: negative values will be converted too.
// For ex. "-1" converts to uint64_t max value.
[[nodiscard]] inline bool to_uint64(std::string const & s, uint64_t & i, int base = 10)
{
  return to_uint64(s.c_str(), i, base);
}
[[nodiscard]] inline bool to_int64(std::string const & s, int64_t & i)
{
  return to_int64(s.c_str(), i);
}
[[nodiscard]] inline bool to_uint32(std::string const & s, uint32_t & i, int base = 10)
{
  return to_uint32(s.c_str(), i, base);
}
[[nodiscard]] inline bool to_int32(std::string const & s, int32_t & i)
{
  return to_int32(s.c_str(), i);
}
[[nodiscard]] inline bool to_size_t(std::string const & s, size_t & i)
{
  return to_size_t(s.c_str(), i);
}
[[nodiscard]] inline bool to_float(std::string const & s, float & f)
{
  return to_float(s.c_str(), f);
}
[[nodiscard]] inline bool to_double(std::string const & s, double & d)
{
  return to_double(s.c_str(), d);
}


namespace impl
{
template <typename T> bool from_sv(std::string_view sv, T & t)
{
  auto const end = sv.data() + sv.size();
  auto const res = std::from_chars(sv.data(), end, t);
  return (res.ec != std::errc::invalid_argument && res.ec != std::errc::result_out_of_range &&
          res.ptr == end);
}
} // namespace impl

template <class T> inline bool to_uint(std::string_view sv, T & i)
{
  static_assert(std::is_unsigned<T>::value, "");
  return impl::from_sv(sv, i);
}

inline bool to_double(std::string_view sv, double & d)
{
  /// @todo std::from_chars still not implemented?
  return to_double(std::string(sv), d);
}
//@}


/// @name From numeric to string.
//@{
inline std::string to_string(std::string const & s) { return s; }
inline std::string to_string(char const * s) { return s; }
template <typename T>
std::string to_string(T t)
{
  std::ostringstream ss;
  ss << t;
  return ss.str();
}

template <typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
[[nodiscard]] inline bool to_any(std::string const & s, T & i)
{
  return internal::ToInteger(s.c_str(), i);
}

[[nodiscard]] inline bool to_any(std::string const & s, float & f) { return to_float(s, f); }
[[nodiscard]] inline bool to_any(std::string const & s, double & d) { return to_double(s, d); }
[[nodiscard]] inline bool to_any(std::string const & s, std::string & result)
{
  result = s;
  return true;
}

inline std::string to_string(int32_t i) { return std::to_string(i); }
inline std::string to_string(int64_t i) { return std::to_string(i); }
inline std::string to_string(uint64_t i) { return std::to_string(i); }
/// Use this function to get string with fixed count of
/// "Digits after comma".
std::string to_string_dac(double d, int dac);
//@}

// Get string with fixed width. Extra '0' are added at the begining to fit size.
template <typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
std::string to_string_width(T l, int width)
{
  std::ostringstream ss;
  if (l < 0)
    ss << '-';
  ss << std::setfill('0') << std::setw(width) << std::abs(l);
  return ss.str();
}

template <typename IterT1, typename IterT2>
bool StartsWith(IterT1 beg, IterT1 end, IterT2 begPrefix, IterT2 endPrefix)
{
  while (beg != end && begPrefix != endPrefix && *beg == *begPrefix)
  {
    ++beg;
    ++begPrefix;
  }
  return begPrefix == endPrefix;
}

bool StartsWith(UniString const & s, UniString const & p);
bool StartsWith(std::string_view s1, std::string_view s2);
bool StartsWith(std::string const & s, std::string::value_type c);

bool EndsWith(UniString const & s1, UniString const & s2);
bool EndsWith(std::string_view s1, std::string_view s2);
bool EndsWith(std::string const & s, std::string::value_type c);

// If |s| starts with |prefix|, deletes it from |s| and returns true.
// Otherwise, leaves |s| unmodified and returns false.
bool EatPrefix(std::string & s, std::string const & prefix);
// If |s| ends with |suffix|, deletes it from |s| and returns true.
// Otherwise, leaves |s| unmodified and returns false.
bool EatSuffix(std::string & s, std::string const & suffix);

/// Try to guess if it's HTML or not. No guarantee.
bool IsHTML(std::string const & utf8);

/// Compare str1 and str2 and return if they are equal except for mismatchedSymbolsNum symbols
bool AlmostEqual(std::string const & str1, std::string const & str2, size_t mismatchedCount);

template <typename Iterator, typename Delimiter>
typename std::iterator_traits<Iterator>::value_type
JoinStrings(Iterator begin, Iterator end, Delimiter const & delimiter)
{
  if (begin == end)
    return {};

  auto result = *begin++;
  for (Iterator it = begin; it != end; ++it)
  {
    result += delimiter;
    result += *it;
  }

  return result;
}

template <typename Container, typename Delimiter>
typename Container::value_type JoinStrings(Container const & container, Delimiter const & delimiter)
{
  return JoinStrings(begin(container), end(container), delimiter);
}

template <typename Fn>
void ForEachMatched(std::string const & s, std::regex const & regex, Fn && fn)
{
  for (std::sregex_token_iterator cur(s.begin(), s.end(), regex), end; cur != end; ++cur)
    fn(*cur);
}

// Computes the minimum number of insertions, deletions and
// alterations of characters needed to transform one string into
// another.  The function works in O(length1 * length2) time and
// O(min(length1, length2)) memory where length1 and length2 are the
// lengths of the argument strings.  See
// https://en.wikipedia.org/wiki/Levenshtein_distance.  One string is
// [b1, e1) and the other is [b2, e2). The iterator form is chosen to
// fit both std::string and strings::UniString.  This function does
// not normalize either of the strings.
template <typename Iter>
size_t EditDistance(Iter const & b1, Iter const & e1, Iter const & b2, Iter const & e2)
{
  size_t const n = std::distance(b1, e1);
  size_t const m = std::distance(b2, e2);

  if (m > n)
    return EditDistance(b2, e2, b1, e1);

  // |curr| and |prev| are current and previous rows of the
  // dynamic programming table.
  std::vector<size_t> prev(m + 1);
  std::vector<size_t> curr(m + 1);
  for (size_t j = 0; j <= m; ++j)
    prev[j] = j;
  auto it1 = b1;
  // 1-based to avoid corner cases.
  for (size_t i = 1; i <= n; ++i, ++it1)
  {
    curr[0] = i;
    auto const & c1 = *it1;
    auto it2 = b2;
    for (size_t j = 1; j <= m; ++j, ++it2)
    {
      auto const & c2 = *it2;

      curr[j] = std::min(curr[j - 1], prev[j]) + 1;
      curr[j] = std::min(curr[j], prev[j - 1] + (c1 == c2 ? 0 : 1));
    }
    prev.swap(curr);
  }
  return prev[m];
}
}  // namespace strings
