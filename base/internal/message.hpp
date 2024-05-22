#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <deque>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

/// @name Declarations.
//@{
template <typename T> inline std::string DebugPrint(T const & t);

inline std::string DebugPrint(std::string s) { return s; }
inline std::string DebugPrint(char const * t);
inline std::string DebugPrint(char * t) { return DebugPrint(static_cast<char const *>(t)); }
inline std::string DebugPrint(char t);
inline std::string DebugPrint(char32_t t);

/// @name We are going step-by-step to C++20. Use UTF8 string literals instead.
/// @{
std::string DebugPrint(char16_t const * t) = delete;
std::string DebugPrint(char16_t * t) = delete;
std::string DebugPrint(char32_t const * t) = delete;
std::string DebugPrint(char32_t * t) = delete;
/// @}

template <typename U, typename V> inline std::string DebugPrint(std::pair<U, V> const & p);
template <typename T> inline std::string DebugPrint(std::list<T> const & v);
template <typename T> inline std::string DebugPrint(std::vector<T> const & v);
template <typename T, typename C = std::less<T>> inline std::string DebugPrint(std::set<T, C> const & v);
template <typename T, typename C = std::less<T>> inline std::string DebugPrint(std::multiset<T, C> const & v);
template <typename U, typename V, typename C = std::less<U>> inline std::string DebugPrint(std::map<U, V, C> const & v);
template <typename T> inline std::string DebugPrint(std::initializer_list<T> const & v);
template <typename T> inline std::string DebugPrint(std::unique_ptr<T> const & v);

template <class Key, class Hash = std::hash<Key>, class Pred = std::equal_to<Key>>
inline std::string DebugPrint(std::unordered_set<Key, Hash, Pred> const & v);
template <class Key, class T, class Hash = std::hash<Key>, class Pred = std::equal_to<Key>>
inline std::string DebugPrint(std::unordered_map<Key, T, Hash, Pred> const & v);
//@}

template <typename T> inline std::string DebugPrint(T const & t)
{
  std::ostringstream out;
  out << t;
  return out.str();
}

inline std::string DebugPrint(char const * t)
{
  if (t)
    return {t};
  return {"NULL string pointer"};
}

inline std::string DebugPrint(char t)
{
  // return {1, t} wrongly constructs "\0x1t" string.
  return std::string(1, t);
}

namespace internal
{
std::string ToUtf8(std::u16string_view utf16);
std::string ToUtf8(std::u32string_view utf32);
}  // namespace internal

inline std::string DebugPrint(std::u16string const & utf16)
{
  return internal::ToUtf8(utf16);
}

inline std::string DebugPrint(std::u16string_view utf16)
{
  return internal::ToUtf8(utf16);
}

inline std::string DebugPrint(std::u32string const & utf32)
{
  return internal::ToUtf8(utf32);
}

inline std::string DebugPrint(std::u32string_view utf32)
{
  return internal::ToUtf8(utf32);
}

inline std::string DebugPrint(char32_t t)
{
  return internal::ToUtf8(std::u32string_view{&t, 1});
}

inline std::string DebugPrint(std::chrono::time_point<std::chrono::system_clock> const & ts)
{
  auto t = std::chrono::system_clock::to_time_t(ts);
  std::string str = std::ctime(&t);
  str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
  return str;
}

template <typename U, typename V>
std::string DebugPrint(std::pair<U, V> const & p)
{
  std::ostringstream out;
  out << "(" << DebugPrint(p.first) << ", " << DebugPrint(p.second) << ")";
  return out.str();
}

template <typename IterT>
std::string DebugPrintSequence(IterT beg, IterT end)
{
  std::ostringstream out;
  out << "[" << std::distance(beg, end) << ":";
  for (; beg != end; ++beg)
    out << " " << DebugPrint(*beg);
  out << " ]";
  return out.str();
}

template <typename T>
std::string DebugPrint(std::optional<T> const & p)
{
  if (p)
  {
    std::ostringstream out;
    out << "optional(" << DebugPrint(p.value()) << ")";
    return out.str();
  }
  else
    return "nullopt";
}

std::string inline DebugPrint(std::nullopt_t const &)
{
  return "nullopt";
}

// Avoid calling it for string literals.
template <typename T, size_t N,
         typename = std::enable_if_t<!std::is_same<typename std::remove_cv<T>::type, char>::value &&
                                     !std::is_same<typename std::remove_cv<T>::type, char16_t>::value &&
                                     !std::is_same<typename std::remove_cv<T>::type, char32_t>::value>>
inline std::string DebugPrint(T (&arr) [N])
{
  return DebugPrintSequence(arr, arr + N);
}

template <typename T, size_t N> inline std::string DebugPrint(std::array<T, N> const & v)
{
  return DebugPrintSequence(v.begin(), v.end());
}

template <typename T> inline std::string DebugPrint(std::vector<T> const & v)
{
  return DebugPrintSequence(v.begin(), v.end());
}

template <typename T> inline std::string DebugPrint(std::deque<T> const & d)
{
  return DebugPrintSequence(d.begin(), d.end());
}

template <typename T> inline std::string DebugPrint(std::list<T> const & v)
{
  return DebugPrintSequence(v.begin(), v.end());
}

template <typename T, typename C> inline std::string DebugPrint(std::set<T, C> const & v)
{
  return DebugPrintSequence(v.begin(), v.end());
}

template <typename T, typename C> inline std::string DebugPrint(std::multiset<T, C> const & v)
{
  return DebugPrintSequence(v.begin(), v.end());
}

template <typename U, typename V, typename C> inline std::string DebugPrint(std::map<U, V, C> const & v)
{
  return DebugPrintSequence(v.begin(), v.end());
}

template <typename T> inline std::string DebugPrint(std::initializer_list<T> const & v)
{
  return DebugPrintSequence(v.begin(), v.end());
}

template <class Key, class Hash, class Pred>
inline std::string DebugPrint(std::unordered_set<Key, Hash, Pred> const & v)
{
  return DebugPrintSequence(v.begin(), v.end());
}

template <class Key, class T, class Hash, class Pred>
inline std::string DebugPrint(std::unordered_map<Key, T, Hash, Pred> const & v)
{
  return DebugPrintSequence(v.begin(), v.end());
}

template <typename T> inline std::string DebugPrint(std::unique_ptr<T> const & v)
{
  std::ostringstream out;
  if (v.get() != nullptr)
    out << DebugPrint(*v);
  else
    out << DebugPrint("null");
  return out.str();
}

namespace base
{
inline std::string Message() { return {}; }

template <typename T>
std::string Message(T const & t)
{
  using ::DebugPrint;
  return DebugPrint(t);
}

template <typename T, typename... Args>
std::string Message(T const & t, Args const &... others)
{
  using ::DebugPrint;
  return DebugPrint(t) + " " + Message(others...);
}
}  // namespace base
