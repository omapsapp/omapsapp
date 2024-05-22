#pragma once

#include "base/macros.hpp"

#include <sstream>
#include <string>

class DebugPrintVisitor
{
public:
  DebugPrintVisitor(std::string const & name) : m_name(name) {}

  template <typename T>
  void operator()(T const & t, char const * name = nullptr)
  {
    if (!m_empty)
      m_os << ", ";
    m_empty = false;

    if (name)
      m_os << name << ": ";
    m_os << DebugPrint(t);
  }

  // This override is implemented to support deserialization with optional values
  // which could be declared by DECLARE_VISITOR_AND_DEBUG_PRINT macro.
  // Because of this class is used just for serialize structures into strings,
  // the second parameter is used for method signature compatibility only.
  template <typename T>
  void operator()(T const & t, T const &, char const * name = nullptr)
  {
    operator()(t, name);
  }

  std::string ToString() const { return m_name + " [" + m_os.str() + "]"; }

private:
  std::string m_name;
  bool m_empty = true;
  std::ostringstream m_os;
};

#define DECLARE_VISITOR(...)          \
  template <typename Visitor>         \
  void Visit(Visitor & visitor)       \
  {                                   \
    UNUSED_VALUE(visitor);            \
    __VA_ARGS__;                      \
  }                                   \
  template <typename Visitor>         \
  void Visit(Visitor & visitor) const \
  {                                   \
    UNUSED_VALUE(visitor);            \
    __VA_ARGS__;                      \
  }

#define DECLARE_DEBUG_PRINT(className)               \
  friend std::string DebugPrint(className const & c) \
  {                                                  \
    DebugPrintVisitor visitor(#className);           \
    c.Visit(visitor);                                \
    return visitor.ToString();                       \
  }

#define DECLARE_VISITOR_AND_DEBUG_PRINT(className, ...) \
  DECLARE_VISITOR(__VA_ARGS__)                          \
  DECLARE_DEBUG_PRINT(className)
