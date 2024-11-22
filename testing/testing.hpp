#pragma once

#include "testing/testregister.hpp"

#include "base/exception.hpp"
#include "base/logging.hpp"
#include "base/math.hpp"
#include "base/src_point.hpp"

#include <string>

// TestRegister is static to avoid this warning:
// No previous extern declaration for non-static variable 'g_testRegister_TESTNAME'
// note: declare 'static' if the variable is not intended to be used outside of this translation unit
#define UNIT_TEST(name)                                                  \
  void UnitTest_##name();                                                \
  static TestRegister g_testRegister_##name(#name, __FILE__, &UnitTest_##name); \
  void UnitTest_##name()

#define UNIT_CLASS_TEST(CLASS, NAME)               \
  struct UnitClass_##CLASS##_##NAME : public CLASS \
  {                                                \
  public:                                          \
    void NAME();                                   \
  };                                               \
  UNIT_TEST(CLASS##_##NAME)                        \
  {                                                \
    UnitClass_##CLASS##_##NAME instance;           \
    instance.NAME();                               \
  }                                                \
  void UnitClass_##CLASS##_##NAME::NAME()

DECLARE_EXCEPTION(TestFailureException, RootException);

namespace base
{
[[noreturn]] inline void OnTestFailed(SrcPoint const & srcPoint, std::string const & msg)
{
  LOG(LINFO, ("FAILED"));
  LOG(LINFO, (::DebugPrint(srcPoint.FileName()) + ":" + ::DebugPrint(srcPoint.Line()), msg));
  MYTHROW(TestFailureException, (srcPoint.FileName(), srcPoint.Line(), msg));
}
}  // namespace base

namespace testing
{
void RunEventLoop();
void StopEventLoop();

void Wait();
void Notify();
}  // namespace testing

// This struct contains parsed command line options. It may contain pointers to argc contents.
struct CommandLineOptions
{
  CommandLineOptions() = default;

  char const * m_filterRegExp = nullptr;
  char const * m_suppressRegExp = nullptr;
  char const * m_dataPath = nullptr;
  char const * m_resourcePath = nullptr;

  bool m_help = false;
  bool m_listTests = false;
};
CommandLineOptions const & GetTestingOptions();

#define TEST(X, msg)                                                                     \
  do                                                                                     \
  {                                                                                      \
    if (X)                                                                               \
    {                                                                                    \
    }                                                                                    \
    else                                                                                 \
    {                                                                                    \
      ::base::OnTestFailed(SRC(), ::base::Message("TEST(" #X ")", ::base::Message msg)); \
    }                                                                                    \
  } while (0)

#define TEST_EQUAL(X, Y, msg)                                                                      \
  do                                                                                               \
  {                                                                                                \
    if ((X) == (Y))                                                                                \
    {                                                                                              \
    }                                                                                              \
    else                                                                                           \
    {                                                                                              \
      ::base::OnTestFailed(SRC(), ::base::Message("TEST(" #X " == " #Y ")", ::base::Message(X, Y), \
                                                  ::base::Message msg));                           \
    }                                                                                              \
  } while (0)

#define TEST_NOT_EQUAL(X, Y, msg)                                                                  \
  do                                                                                               \
  {                                                                                                \
    if ((X) != (Y))                                                                                \
    {                                                                                              \
    }                                                                                              \
    else                                                                                           \
    {                                                                                              \
      ::base::OnTestFailed(SRC(), ::base::Message("TEST(" #X " != " #Y ")", ::base::Message(X, Y), \
                                                  ::base::Message msg));                           \
    }                                                                                              \
  } while (0)

#define TEST_LESS(X, Y, msg)                                                                      \
  do                                                                                              \
  {                                                                                               \
    if ((X) < (Y))                                                                                \
    {                                                                                             \
    }                                                                                             \
    else                                                                                          \
    {                                                                                             \
      ::base::OnTestFailed(SRC(), ::base::Message("TEST(" #X " < " #Y ")", ::base::Message(X, Y), \
                                                  ::base::Message msg));                          \
    }                                                                                             \
  } while (0)

#define TEST_LESS_OR_EQUAL(X, Y, msg)                                                              \
  do                                                                                               \
  {                                                                                                \
    if ((X) <= (Y))                                                                                \
    {                                                                                              \
    }                                                                                              \
    else                                                                                           \
    {                                                                                              \
      ::base::OnTestFailed(SRC(), ::base::Message("TEST(" #X " <= " #Y ")", ::base::Message(X, Y), \
                                                  ::base::Message msg));                           \
    }                                                                                              \
  } while (0)

#define TEST_GREATER(X, Y, msg)                                                                   \
  do                                                                                              \
  {                                                                                               \
    if ((X) > (Y))                                                                                \
    {                                                                                             \
    }                                                                                             \
    else                                                                                          \
    {                                                                                             \
      ::base::OnTestFailed(SRC(), ::base::Message("TEST(" #X " > " #Y ")", ::base::Message(X, Y), \
                                                  ::base::Message msg));                          \
    }                                                                                             \
  } while (0)

#define TEST_GREATER_OR_EQUAL(X, Y, msg)                                                           \
  do                                                                                               \
  {                                                                                                \
    if ((X) >= (Y))                                                                                \
    {                                                                                              \
    }                                                                                              \
    else                                                                                           \
    {                                                                                              \
      ::base::OnTestFailed(SRC(), ::base::Message("TEST(" #X " >= " #Y ")", ::base::Message(X, Y), \
                                                  ::base::Message msg));                           \
    }                                                                                              \
  } while (0)

#define TEST_ALMOST_EQUAL_ULPS(X, Y, msg)                                                       \
  do                                                                                            \
  {                                                                                             \
    if (AlmostEqualULPs(X, Y))                                                          \
    {                                                                                           \
    }                                                                                           \
    else                                                                                        \
    {                                                                                           \
      ::base::OnTestFailed(SRC(), ::base::Message("TEST(AlmostEqualULPs(" #X ", " #Y ")", \
                                                  ::base::Message(X, Y), ::base::Message msg)); \
    }                                                                                           \
  } while (0)

#define TEST_NOT_ALMOST_EQUAL_ULPS(X, Y, msg)                                                    \
  do                                                                                             \
  {                                                                                              \
    if (!AlmostEqualULPs(X, Y))                                                          \
    {                                                                                            \
    }                                                                                            \
    else                                                                                         \
    {                                                                                            \
      ::base::OnTestFailed(SRC(), ::base::Message("TEST(!AlmostEqualULPs(" #X ", " #Y ")", \
                                                  ::base::Message(X, Y), ::base::Message msg));  \
    }                                                                                            \
  } while (0)

#define TEST_ALMOST_EQUAL_ABS(X, Y, eps, msg)                                                      \
  do                                                                                               \
  {                                                                                                \
    if (AlmostEqualAbs(X, Y, eps))                                                         \
    {                                                                                              \
    }                                                                                              \
    else                                                                                           \
    {                                                                                              \
      ::base::OnTestFailed(SRC(),                                                                  \
                           ::base::Message("TEST(!AlmostEqualAbs(" #X ", " #Y ", " #eps ")", \
                           ::base::Message(X, Y, eps), ::base::Message msg));                      \
    }                                                                                              \
  } while (0)

// TODO(AlexZ): Add more cool macroses (or switch all unit tests to gtest).
#define TEST_THROW(X, exception, msg)                                                           \
  do                                                                                            \
  {                                                                                             \
    bool expected_exception = false;                                                            \
    try                                                                                         \
    {                                                                                           \
      X;                                                                                        \
    }                                                                                           \
    catch (std::exception const &)                                                              \
    {                                                                                           \
      expected_exception = true;                                                                \
    }                                                                                           \
    catch (...)                                                                                 \
    {                                                                                           \
      ::base::OnTestFailed(                                                                     \
          SRC(), ::base::Message("Unexpected exception at TEST(" #X ")", ::base::Message msg)); \
    }                                                                                           \
    if (!expected_exception)                                                                    \
      ::base::OnTestFailed(SRC(), ::base::Message("Expected exception " #exception              \
                                                  " was not thrown in TEST(" #X ")",            \
                                                  ::base::Message msg));                        \
  } while (0)

#define TEST_NO_THROW(X, msg)                                                                   \
  do                                                                                            \
  {                                                                                             \
    try                                                                                         \
    {                                                                                           \
      X;                                                                                        \
    }                                                                                           \
    catch (RootException const & ex)                                                            \
    {                                                                                           \
      ::base::OnTestFailed(SRC(),                                                               \
                           ::base::Message("Unexpected exception at TEST(" #X ")",              \
                           ::base::Message(ex.Msg()), ::base::Message msg));                    \
    }                                                                                           \
    catch (...)                                                                                 \
    {                                                                                           \
      ::base::OnTestFailed(                                                                     \
          SRC(), ::base::Message("Unexpected exception at TEST(" #X ")", ::base::Message msg)); \
    }                                                                                           \
  } while (0)

#define TEST_ANY_THROW(X, msg)                                                                 \
  do                                                                                           \
  {                                                                                            \
    bool was_exception = false;                                                                \
    try                                                                                        \
    {                                                                                          \
      X;                                                                                       \
    }                                                                                          \
    catch (...)                                                                                \
    {                                                                                          \
      was_exception = true;                                                                    \
    }                                                                                          \
    if (!was_exception)                                                                        \
      ::base::OnTestFailed(SRC(), ::base::Message("No exceptions were thrown in TEST(" #X ")", \
                                                  ::base::Message msg));                       \
  } while (0)
